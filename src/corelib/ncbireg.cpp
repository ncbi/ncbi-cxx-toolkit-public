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


// Platform-specific EndOfLine
#if   defined(NCBI_OS_MAC)
const char s_Endl[] = "\r";
#elif defined(NCBI_OS_MSWIN)
const char s_Endl[] = "\r\n";
#else /* assume UNIX-like EOLs */
const char s_Endl[] = "\n";
#endif


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


#define CHECK_FLAGS(func_name, flags, allowed_flags)  do { \
    if (flags & ~((TFlags)(allowed_flags))) \
        _TRACE("CNcbiRegistry::" func_name "(): extra flags passed: " \
               << setiosflags(IOS_BASE::hex) << flags); \
    flags = flags & (TFlags)(allowed_flags); \
} while (0)


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


CNcbiRegistry::CNcbiRegistry(void) {}

CNcbiRegistry::~CNcbiRegistry(void) {}

CNcbiRegistry::CNcbiRegistry(CNcbiIstream& is, TFlags flags)
{
    CHECK_FLAGS("CNcbiRegistry", flags, eTransient);
    Read(is, flags);
}


bool CNcbiRegistry::Empty(void) const {
    return m_Registry.empty();
}


void CNcbiRegistry::Read(CNcbiIstream& is, TFlags flags)
{
    CHECK_FLAGS("Read", flags, eTransient | eNoOverride);

    // Adjust flags for Set()
    if (flags & eTransient) {
        flags &= ~((TFlags)eTransient);
    } else {
        flags |= ePersistent;
    }

    string    str;     // the line being parsed
    SIZE_TYPE line;    // # of the line being parsed
    string    section; // current section name

    for (line = 1;  s_NcbiGetline(is, str);  line++) {
        SIZE_TYPE len = str.length();
        SIZE_TYPE beg;
        for (beg = 0;  beg < len  &&  isspace(str[beg]);  beg++);
        if (beg == len)
            continue;

        switch ( str[beg] ) {
        case ';':  { // comment
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
            
            for (end = beg;  isalnum(str[end])  ||  str[end] == '_';  end++);
            section = str.substr(beg, end - beg);

            // an extra validity check
            while ( isspace(str[end]) )
                end++;
            _ASSERT( end <= str.find_first_of(']', 0) );
            if (str[end] != ']')
                throw CParseException("\
Invalid registry section name: `" + str + "'", line);
            break;
        }

        default:  { // regular entry
            if (!isalnum(str[beg])  ||  str.find_first_of('=') == NPOS)
                throw CParseException("\
Invalid registry entry format: '" + str + "'", line);
            // name
            SIZE_TYPE mid;
            for (mid = beg;  isalnum(str[mid])  ||  str[mid] == '_';  mid++);
            string name = str.substr(beg, mid);

            // '=' and surrounding spaces
            while ( isspace(str[mid]) )
                mid++;
            if (str[mid] != '=')
                 throw CParseException("\
Invalid registry entry name: '" + str + "'", line);
            for (mid++;  mid < len  &&  isspace(str[mid]);  mid++);
            _ASSERT( mid <= len );

            // ? empty value
            if (mid == len) {
                if ( !(flags & eNoOverride) )
                    Set(section, name, NcbiEmptyString, flags);
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
                for (end = str.length() - 1;  end > beg  &&  isspace(str[end]);
                     end--);
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

            Set(section, name, value, flags);
        }
        }
    }

    if ( !is.eof() )
        throw CParseException("\
Error in reading the registry: '" + str + "'", line);
}


bool CNcbiRegistry::Write(CNcbiOstream& os)
const
{
    for (TRegistry::const_iterator section = m_Registry.begin();
         section != m_Registry.end();  section++) {
        // write section header
        os << '[' << section->first << ']' << NcbiEndl;
        if ( !os )
            return false;
        // write section entries
        const TRegSection& reg_section = section->second;
        _ASSERT( !reg_section.empty() );
        for (TRegSection::const_iterator entry = reg_section.begin();
             entry != reg_section.end();  entry++) {
            // dump only persistent values
            if ( entry->second.persistent.empty() )
                continue;

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
    return true;
}


void CNcbiRegistry::Clear(void)
{
    m_Registry.clear();
}


const string& CNcbiRegistry::Get(const string& section, const string& name,
                                 TFlags flags)
const
{
    CHECK_FLAGS("Get", flags, ePersistent);

    if ( name.empty() )  // no empty names allowed
        return NcbiEmptyString;

    // find section
    TRegistry::const_iterator find_section = m_Registry.find(section);
    if (find_section == m_Registry.end())
        return NcbiEmptyString;

    // find entry in the section
    const TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );
    TRegSection::const_iterator find_entry = reg_section.find(name);
    if (find_entry == reg_section.end())
        return NcbiEmptyString;

    // ok -- found the requested entry
    const TRegEntry& entry = find_entry->second;
    _ASSERT( !entry.persistent.empty()  ||  !entry.transient.empty() );
    return ((flags & ePersistent) == 0  &&  !entry.transient.empty()) ?
        entry.transient : entry.persistent;
}


bool CNcbiRegistry::Set(const string& section, const string& name,
                        const string& value, TFlags flags)
{
    CHECK_FLAGS("Set", flags, ePersistent | eNoOverride | eTruncate);

    // find section
    TRegistry::iterator find_section = m_Registry.find(section);
    if (find_section == m_Registry.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        // new section, new entry
        x_SetValue(m_Registry[section][name], value, flags);
        return true;
    }

    // find entry within the found section
    TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );
    TRegSection::iterator find_entry = reg_section.find(name);
    if (find_entry == reg_section.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        // new entry
        x_SetValue(reg_section[name], value, flags);
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
    x_SetValue(entry, value, flags);

    // unset(remove) the entry, if empty
    if (entry.persistent.empty()  &&  entry.transient.empty()) {
        reg_section.erase(find_entry);

        // remove the section, if empty
        if ( reg_section.empty() ) {
            m_Registry.erase(find_section);
        }
    }

    return true;
}


void CNcbiRegistry::EnumerateSections(list<string>* sections)
const
{
    sections->clear();
    for (TRegistry::const_iterator section = m_Registry.begin();
         section != m_Registry.end();  section++) {
        sections->push_back(section->first);
    }
}


void CNcbiRegistry::EnumerateEntries(const string& section,
                                     list<string>* entries)
const
{
    entries->clear();

    // find section
    TRegistry::const_iterator find_section = m_Registry.find(section);
    if (find_section == m_Registry.end())
        return;

    const TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );

    // enumerate through the entries in the found section
    for (TRegSection::const_iterator entry = reg_section.begin();
         entry != reg_section.end();  entry++) {
        entries->push_back(entry->first);
    }
}


void CNcbiRegistry::x_SetValue(TRegEntry& entry, const string& value,
                               TFlags flags)
{
    string* to = (flags & ePersistent) ? &entry.persistent : &entry.transient;

    if ((flags & eTruncate) == 0  ||  value.empty()) {
        *to = value;
        return;
    }

    SIZE_TYPE beg;
    for (beg = 0;
         beg < value.length()  &&  isspace(value[beg])  &&  value[beg] != '\n';
         beg++);

    if (beg == value.length()) {
        to->erase();
        return;
    }
    
    SIZE_TYPE end;
    for (end = value.length() - 1;
         isspace(value[end])  &&  value[end] != '\n';
         end--);

    _ASSERT(beg <= end);
    *to = value.substr(beg, end - beg + 1);
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

