/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Denis Vakatov
*
* File Description:
*   Handle info in the NCBI configuration file(s):
*      read and parse config. file
*      search, edit, etc. in the retrieved configuration info
*      dump info back to config. file
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.20  2001/09/11 00:52:51  vakatov
* Fixes to R1.17 - R1.19:
*   Renamed HasChanged() to Modified(), get rid of bugs, refined and
*   extended its functionality.
*   Made Write() be "const" again.
*   Prevent section comment from inadvertently concatenating with the
*   section name (without a new-line between them).
*   Other fixes and refinements.
*
* Revision 1.19  2001/09/10 16:34:35  ivanov
* Added method HasChanged()
*
* Revision 1.18  2001/06/26 15:20:22  ivanov
* Fixed small bug in s_ConvertComment().
* Changed method of check and create new section in Read().
*
* Revision 1.17  2001/06/22 21:49:47  ivanov
* Added (with Denis Vakatov) ability for read/write the registry file
* with comments. Also added functions GetComment() and SetComment().
*
* Revision 1.16  2001/05/17 15:04:59  lavr
* Typos corrected
*
* Revision 1.15  2001/04/09 17:39:45  grichenk
* CNcbiRegistry::Get() return type reverted to "const string&"
*
* Revision 1.14  2001/04/06 15:46:30  grichenk
* Added thread-safety to CNcbiRegistry:: methods
*
* Revision 1.13  2001/01/30 22:13:28  vakatov
* Write() -- use "s_Endl" instead of "NcbiEndl"
*
* Revision 1.12  2001/01/30 00:41:58  vakatov
* Read/Write -- serialize '\r' as "\\r"
*
* Revision 1.11  2000/03/30 21:06:35  kans
* bad end of line detected on Mac
*
* Revision 1.10  1999/11/18 20:14:15  vakatov
* Get rid of some CodeWarrior(MAC) C++ compilation warnings
*
* Revision 1.9  1999/09/02 21:52:14  vakatov
* CNcbiRegistry::Read() -- fixed to accept if underscore is the 1st
* symbol in the section/entry name;
* Allow '-' and '.' in the section/entry name
*
* Revision 1.8  1999/08/30 16:00:41  vakatov
* CNcbiRegistry:: Get()/Set() -- force the "name" and "section" to
* consist of alphanumeric and '_' only;  ignore leading and trailing
* spaces
*
* Revision 1.7  1999/07/06 15:26:35  vakatov
* CNcbiRegistry::
*   - allow multi-line values
*   - allow values starting and ending with space symbols
*   - introduced EFlags/TFlags for optional parameters in the class
*     member functions -- rather than former numerous boolean parameters
*
* Revision 1.6  1998/12/28 17:56:37  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/12/10 22:59:47  vakatov
* CNcbiRegistry:: API is ready(and by-and-large tested)
* ===========================================================================
*/

#include <corelib/ncbireg.hpp>
#include <corelib/ncbimtx.hpp>

// Platform-specific EndOfLine
#if   defined(NCBI_OS_MAC)
const char s_Endl[] = "\r";
#elif defined(NCBI_OS_MSWIN)
const char s_Endl[] = "\r\n";
#else /* assume UNIX-like EOLs */
const char s_Endl[] = "\n";
#endif


BEGIN_NCBI_SCOPE


#define CHECK_FLAGS(func_name, flags, allowed_flags)  do { \
    if (flags & ~((TFlags)(allowed_flags))) \
        _TRACE("CNcbiRegistry::" func_name "(): extra flags passed: " \
               << setiosflags(IOS_BASE::hex) << flags); \
    flags = flags & (TFlags)(allowed_flags); \
} while (0)



/* Get the next line taking into account platform specifics of End-of-Line
 */
static CNcbiIstream& s_NcbiGetline(CNcbiIstream& is, string& str)
{
#if   defined(NCBI_OS_MAC)
    NcbiGetline(is, str, '\r');
#elif defined(NCBI_OS_MSWIN)
    NcbiGetline(is, str, '\n');
    if (!str.empty()  &&  str[str.length()-1] == '\r')
        str.resize(str.length() - 1);
#else /* assume UNIX-like EOLs */
    NcbiGetline(is, str, '\n');
#endif

    // special case -- an empty line
    if (is.fail()  &&  !is.eof()  &&  !is.gcount()  &&  str.empty())
        is.clear(is.rdstate() & ~IOS_BASE::failbit);

    return is;
}


/* Valid symbols for a section/entry name
 */
inline bool s_IsNameSectionSymbol(char ch)
{
    return (isalnum(ch)  ||  ch == '_'  ||  ch == '-'  ||  ch == '.');
}


/* Check if "str" consists of alphanumeric and '_' only
 */
static bool s_IsNameSection(const string& str)
{
    if ( str.empty() )
        return false;

    iterate (string, it, str) {
        if ( !s_IsNameSectionSymbol(*it) )
            return false;
    }
    return true;
}


/* Convert "comment" from plain text to comment
 */
static const string s_ConvertComment(const string& comment,
                                     bool is_file_comment = false)
{
    if ( !comment.length() )
        return kEmptyStr;

    string x_comment;
    const char c_comment = is_file_comment ? '#' : ';';

    SIZE_TYPE endl_pos = 0;
    for (SIZE_TYPE beg = 0;  endl_pos != comment.length();
         beg = endl_pos + 1) {
        SIZE_TYPE pos = comment.find_first_not_of(" \t", beg);
        endl_pos = comment.find_first_of("\n", beg);
        if (endl_pos == NPOS) {
            endl_pos = comment.length();
        }
        if (((pos != NPOS  &&  comment[pos] != c_comment) ||
            (pos == NPOS  &&  endl_pos == comment.length())) &&
            (is_file_comment  ||  beg != endl_pos) ) {
            x_comment += c_comment;
        }
        x_comment.append(comment, beg, endl_pos - beg);
        if (is_file_comment  ||  endl_pos != comment.length()) {
            x_comment += '\n';
        }
    }
    return x_comment;
}


/* Dump the comment to stream "os"
 */
static bool s_WriteComment(CNcbiOstream& os, const string& comment)
{
    if (!comment.length())
        return true;

    if (strcmp(s_Endl, "\n") == 0) {
        os << comment;
    } else {
        iterate(string, i, comment) {
            if (*i == '\n') {
                os << s_Endl;
            } else {
                os << *i;
            }
        }
    }

    os << s_Endl;
    return os.good();
}


// Protective mutex for registry Get() and Set() functions
static CFastMutex s_RegMutex;


CNcbiRegistry::CNcbiRegistry(void)
    : m_Modified(false), m_Written(false)
{
    return;
}


CNcbiRegistry::~CNcbiRegistry(void)
{
    return;
}


CNcbiRegistry::CNcbiRegistry(CNcbiIstream& is, TFlags flags)
    : m_Modified(false), m_Written(false)
{
    CHECK_FLAGS("CNcbiRegistry", flags, eTransient);
    Read(is, flags);
}


bool CNcbiRegistry::Empty(void) const {
    return m_Registry.empty();
}


bool CNcbiRegistry::Modified(void) const
{
    return m_Modified;
}


/* Read data to reqistry from stream
 */
void CNcbiRegistry::Read(CNcbiIstream& is, TFlags flags)
{
    CHECK_FLAGS("Read", flags, eTransient | eNoOverride);

    // If to consider this read to be (unconditionally) non-modifying
    bool non_modifying = !m_Modified  &&  !m_Written  &&  x_IsAllTransient();

    // Adjust flags for Set()
    if (flags & eTransient) {
        flags &= ~((TFlags) eTransient);
    } else {
        flags |= ePersistent;
    }

    string    str;          // the line being parsed
    SIZE_TYPE line;         // # of the line being parsed
    string    section;      // current section name
    string    comment;      // current comment

    for (line = 1;  s_NcbiGetline(is, str);  line++) {
        SIZE_TYPE len = str.length();
        SIZE_TYPE beg;

        for (beg = 0;  beg < len  &&  isspace(str[beg]);  beg++)
            continue;
        if (beg == len) {
            comment += str;
            comment += '\n';
            continue;
        }

        switch ( str[beg] ) {

        case '#':  { // file comment
            m_Comment += str;
            m_Comment += '\n';
            break;
        }

        case ';':  { // comment
            comment += str;
            comment += '\n';
            break;
        }

        case '[':  { // section name
            beg++;
            SIZE_TYPE end = str.find_first_of(']');
            if (end == NPOS)
                throw CParseException("\
Invalid registry section(']' is missing): `" + str + "'", line);
            while ( isspace(str[beg]) )
                beg++;
            if (str[beg] == ']') {
                throw CParseException("\
Unnamed registry section: `" + str + "'", line);
            }

            for (end = beg;  s_IsNameSectionSymbol(str[end]);  end++)
                continue;
            section = str.substr(beg, end - beg);

            // an extra validity check
            while ( isspace(str[end]) )
                end++;
            _ASSERT( end <= str.find_first_of(']', 0) );
            if (str[end] != ']')
                throw CParseException("\
Invalid registry section name: `" + str + "'", line);
            // add section comment
            if ( !comment.empty() ) {
                _ASSERT( s_IsNameSection(section) );
                // create section if it not exist
                m_Registry.insert(TRegistry::value_type(section,
                                                        TRegSection()));
                SetComment(GetComment(section) + comment, section);
                comment.erase();
            }
            break;
        }

        default:  { // regular entry
            if (!s_IsNameSectionSymbol(str[beg])  ||
                str.find_first_of('=') == NPOS)
                throw CParseException("\
Invalid registry entry format: '" + str + "'", line);
            // name
            SIZE_TYPE mid;
            for (mid = beg;  s_IsNameSectionSymbol(str[mid]);  mid++)
                continue;
            string name = str.substr(beg, mid);

            // '=' and surrounding spaces
            while ( isspace(str[mid]) )
                mid++;
            if (str[mid] != '=')
                throw CParseException("\
Invalid registry entry name: '" + str + "'", line);
            for (mid++;  mid < len  &&  isspace(str[mid]);  mid++)
                continue;
            _ASSERT( mid <= len );

            // ? empty value
            if (mid == len) {
                if ( !(flags & eNoOverride) ) {
                    Set(section, name, kEmptyStr, flags, comment);
                    comment.erase();
                }
                break;
            }

            // value
            string value;
            beg = mid;
            if (str[beg] == '"')
                beg++;

            bool read_next_line;
            do {
                read_next_line = false;

                // strip trailing spaces, check for an empty string
                if ( str.empty() )
                    break;
                SIZE_TYPE end;
                for (end = str.length() - 1;
                     end > beg  &&  isspace(str[end]);  end--)
                    continue;
                if (end < beg  ||  isspace(str[end]) )
                    break;

                // un-escape the value
                for (SIZE_TYPE i = beg;  i <= end;  i++) {
                    if (str[i] == '"') {
                        if (i != end) {
                            throw CParseException("\
Single(unescaped) '\"' in the middle of registry value: '" + str + "'", line);
                        }
                        break;
                    }

                    if (str[i] != '\\') {
                        value += str[i];
                        continue;
                    }
                    // process back-slash
                    if (i == end) {
                        value += '\n';
                        beg = 0;
                        read_next_line = true;
                        line++;
                    } else if (str[i+1] == 'r') {
                        value += '\r';
                        i++;
                    } else if (str[i+1] == '\\') {
                        value += '\\';
                        i++;
                    } else if (str[i+1] == '"') {
                        value += '"';
                        i++;
                    } else {
                        throw CParseException("\
Badly placed '\\' in the registry value: '" + str + "'", line);
                    }
                }
            } while (read_next_line  &&  s_NcbiGetline(is, str));

            Set(section, name, value, flags, comment);
            comment.erase();
        }
        }
    }

    if ( !is.eof() ) {
        throw CParseException("\
Error in reading the registry: '" + str + "'", line);
    }

    if ( non_modifying ) {
        m_Modified = false;
    }
}


/* Write data from reqistry to stream
 */
bool CNcbiRegistry::Write(CNcbiOstream& os)
const
{
    CFastMutexGuard LOCK(s_RegMutex);

    // write file comment
    if ( !s_WriteComment(os, m_Comment) )
        return false;

    // write data
    iterate (TRegistry, section, m_Registry) {
        //
        const TRegSection& reg_section = section->second;
        _ASSERT( !reg_section.empty() );

        // beauty line
        os << s_Endl;

        // write section comment, if any
        TRegSection::const_iterator comm_entry = reg_section.find(kEmptyStr);
        if (comm_entry != reg_section.end()  &&
            !s_WriteComment(os, comm_entry->second.comment) ) {
            return false;
        }

        // write section header
        os << '[' << section->first << ']' << s_Endl;
        if ( !os )
            return false;

        // write section entries
        iterate (TRegSection, entry, reg_section) {
            // if this entry is actually a section comment, then skip it
            if (entry == comm_entry)
                continue;

            // dump only persistent entries
            if ( entry->second.persistent.empty() )
                continue;

            // write entry comment
            if ( !s_WriteComment(os, entry->second.comment) )
                return false;

            // write next entry;  escape all back-slash and new-line symbols;
            // add "\\i" to the beginning/end of the string if it has
            // spaces there
            os << entry->first << " = ";
            const char* cs = entry->second.persistent.c_str();
            if (isspace(*cs)  &&  *cs != '\n')
                os << '"';
            for ( ;  *cs;  cs++) {
                switch ( *cs ) {
                case '\n':
                    os << '\\' << s_Endl;  break;
                case '\r':
                    os << "\\r";  break;
                case '\\':
                    os << "\\\\";  break;
                case '"':
                    os << "\\\"";  break;
                default:
                    os << *cs;
                }
            }
            cs--;
            if (isspace(*cs)  &&  *cs != '\n')
                os << '"';

            os << s_Endl;
            if ( !os )
                return false;
        }
    }

    m_Modified = false;
    m_Written  = true;
    return true;
}



void CNcbiRegistry::Clear(void)
{
    m_Modified = (m_Modified  ||  !x_IsAllTransient());
    m_Comment.erase();
    m_Registry.clear();
}



const string& CNcbiRegistry::Get(const string& section, const string& name,
                                 TFlags flags)
    const
{
    CHECK_FLAGS("Get", flags, ePersistent);

    // Truncate marginal spaces of "section" and "name"
    // Make sure they aren't empty and consist of alpanum and '_' only
    string x_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(x_section) ) {
        _TRACE("CNcbiRegistry::Get():  bad or empty section name: " + section);
        return kEmptyStr;
    }
    string x_name = NStr::TruncateSpaces(name);
    if ( !s_IsNameSection(x_name) ) {
        _TRACE("CNcbiRegistry::Get():  bad or empty entry name: " + name);
        return kEmptyStr;
    }

    CFastMutexGuard LOCK(s_RegMutex);

    // find section
    TRegistry::const_iterator find_section = m_Registry.find(x_section);
    if (find_section == m_Registry.end())
        return kEmptyStr;

    // find entry in the section
    const TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );
    TRegSection::const_iterator find_entry = reg_section.find(x_name);
    if (find_entry == reg_section.end())
        return kEmptyStr;

    // ok -- found the requested entry
    const TRegEntry& entry = find_entry->second;
    _ASSERT( !entry.persistent.empty()  ||  !entry.transient.empty() );
    return ((flags & ePersistent) == 0  &&  !entry.transient.empty()) ?
        entry.transient : entry.persistent;
}



bool CNcbiRegistry::Set(const string& section, const string& name,
                        const string& value, TFlags flags,
                        const string& comment)
{
    CHECK_FLAGS("Set", flags, ePersistent | eNoOverride | eTruncate);

    // Truncate marginal spaces of "section" and "name"
    // Make sure they aren't empty and consist of alpanum and '_' only
    string x_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(x_section) ) {
        _TRACE("CNcbiRegistry::Set():  bad or empty section name: " + section);
        return false;
    }
    string x_name = NStr::TruncateSpaces(name);

    // is the entry name valid ?
    if ( !s_IsNameSection(x_name) ) {
        _TRACE("CNcbiRegistry::Set():  bad or empty entry name: " + name);
        return false;
    }

    // MT-protect everything down the function code
    CFastMutexGuard LOCK(s_RegMutex);

    // find section
    TRegistry::iterator find_section = m_Registry.find(x_section);
    if (find_section == m_Registry.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        // new section, new entry
        x_SetValue(m_Registry[x_section][x_name], value, flags, comment);
        return true;
    }

    // find entry within the found section
    TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );
    TRegSection::iterator find_entry = reg_section.find(x_name);
    if (find_entry == reg_section.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        // new entry
        x_SetValue(reg_section[x_name], value, flags, comment);
        return true;
    }

    // modifying an existing entry...
    if (flags & eNoOverride)
        return false;  // cannot override

    TRegEntry& entry = find_entry->second;

    // check if it tries to unset an already unset value
    bool transient = (flags & ePersistent) == 0;
    if (value.empty()  &&
        (( transient  &&  entry.transient.empty())  ||
         (!transient  &&  entry.persistent.empty())))
        return false;

    // modify an existing entry
    x_SetValue(entry, value, flags, comment);

    // unset(remove) the entry, if empty
    if (entry.persistent.empty()  &&  entry.transient.empty()) {
        reg_section.erase(find_entry);
        // remove the section, if empty
        if (reg_section.empty() ) {
            m_Registry.erase(find_section);
        }
    }

    return true;
}



bool CNcbiRegistry::SetComment(const string& comment, const string& section,
                               const string& name)
{
    CFastMutexGuard LOCK(s_RegMutex);

    // If "section" is empty string, then set as the registry comment
    string x_section = NStr::TruncateSpaces(section);
    if (x_section == kEmptyStr) {
        m_Comment = s_ConvertComment(comment, true);
        m_Modified = true;
        return true;
    }

    // Find section
    TRegistry::iterator find_section = m_Registry.find(x_section);
    if (find_section == m_Registry.end()) {
        return false;
    }
    TRegSection& reg_section = find_section->second;

    string x_name    = NStr::TruncateSpaces(name);
    string x_comment = s_ConvertComment(comment);

    // If "name" is empty string, then set as the "section" comment
    if (name == kEmptyStr) {
        TRegSection::iterator comm_entry = reg_section.find(kEmptyStr);
        if (comm_entry != reg_section.end()) {
            // replace old comment
            comm_entry->second.comment = x_comment;
            m_Modified = true;
        } else {
            // new comment
            x_SetValue(m_Registry[x_section][kEmptyStr],
                       kEmptyStr, ePersistent, x_comment);
        }
        return true;
    }

    // This is an entry comment
    _ASSERT( !reg_section.empty() );
    TRegSection::iterator find_entry = reg_section.find(x_name);
    if (find_entry == reg_section.end())
        return false;

    // (Re)set entry comment
    find_entry->second.comment = x_comment;

    m_Modified = true;
    return true;
}



const string& CNcbiRegistry::GetComment(const string& section,
                                        const string& name)
{
    CFastMutexGuard LOCK(s_RegMutex);

    // If "section" is empty string, then get the registry's comment.
    string x_section = NStr::TruncateSpaces(section);
    if (x_section == kEmptyStr) {
        return m_Comment;
    }

    // Find section
    TRegistry::iterator find_section = m_Registry.find(x_section);
    if (find_section == m_Registry.end()) {
        return kEmptyStr;
    }
    TRegSection& reg_section = find_section->second;

    // If "name" is empty string, then get "section"'s comment.
    string x_name = NStr::TruncateSpaces(name);
    if (x_name == kEmptyStr) {
        TRegSection::const_iterator comm_entry = reg_section.find(kEmptyStr);
        if (comm_entry == reg_section.end()) {
            return kEmptyStr;
        }
        return comm_entry->second.comment;
    }

    // Get "section:entry"'s comment
    _ASSERT( !reg_section.empty() );
    TRegSection::iterator find_entry = reg_section.find(x_name);
    if (find_entry == reg_section.end()) {
        return kEmptyStr;
    }
    return find_entry->second.comment;
}



void CNcbiRegistry::EnumerateSections(list<string>* sections)
    const
{
    CFastMutexGuard LOCK(s_RegMutex);

    sections->clear();
    iterate (TRegistry, section, m_Registry) {
        sections->push_back(section->first);
    }
}


void CNcbiRegistry::EnumerateEntries(const string& section,
                                     list<string>* entries)
    const
{
    CFastMutexGuard LOCK(s_RegMutex);

    entries->clear();

    // find section
    TRegistry::const_iterator find_section = m_Registry.find(section);
    if (find_section == m_Registry.end())
        return;

    const TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );

    // enumerate through the entries in the found section
    iterate (TRegSection, entry, reg_section) {
        if ( entry->first.empty() )
            continue;  // skip section comment

        entries->push_back(entry->first);
    }
}


bool CNcbiRegistry::x_IsAllTransient(void)
    const
{
    if ( !m_Comment.empty() ) {
        return false;
    }

    iterate (TRegistry, section, m_Registry) {
        iterate (TRegSection, entry, section->second) {
            if (section->first.empty()  || !entry->second.persistent.empty()) {
                return false;  // section comment or non-transient entry
            }
        }
    }

    return true;
}


void CNcbiRegistry::x_SetValue(TRegEntry& entry, const string& value,
                               TFlags flags, const string& comment)
{
    bool persistent = (flags & ePersistent) != 0;

    if ( persistent ) {
        m_Modified = true;
    }

    if ( !comment.empty() ) {
        entry.comment = comment;
    }

    string* to = persistent ? &entry.persistent : &entry.transient;

    if ((flags & eTruncate) == 0  ||  value.empty()) {
        *to = value;
        return;
    }

    SIZE_TYPE beg;
    for (beg = 0;
         beg < value.length()  &&  isspace(value[beg])  &&  value[beg] != '\n';
         beg++)
        continue;

    if (beg == value.length()) {
        to->erase();
        return;
    }

    SIZE_TYPE end;
    for (end = value.length() - 1;
         isspace(value[end])  &&  value[end] != '\n';
         end--)
        continue;

    _ASSERT(beg <= end);
    *to = value.substr(beg, end - beg + 1);
}


END_NCBI_SCOPE
