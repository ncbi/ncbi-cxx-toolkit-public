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
 * Authors:  Denis Vakatov, Aaron Ucko
 *
 * File Description:
 *   Handle info in the NCBI configuration file(s):
 *      read and parse config. file
 *      search, edit, etc. in the retrieved configuration info
 *      dump info back to config. file
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/env_reg.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimtx.hpp>

#include <algorithm>
#include <set>


BEGIN_NCBI_SCOPE

typedef CRegistryReadGuard  TReadGuard;
typedef CRegistryWriteGuard TWriteGuard;


/* Valid symbols for a section/entry name
 */
inline bool s_IsNameSectionSymbol(char ch, IRegistry::TFlags flags)
{
    return (isalnum(ch)
            ||  ch == '_'  ||  ch == '-' ||  ch == '.'  ||  ch == '/'
            ||  ((flags & IRegistry::fInternalSpaces)  &&  ch == ' '));
}


/* Check if "str" consists of alphanumeric and '_' only
 */
static bool s_IsNameSection(const string& str, IRegistry::TFlags flags)
{
    if (str.empty()) {
        return false;
    }

    ITERATE (string, it, str) {
        if (!s_IsNameSectionSymbol(*it, flags)) {
            return false;
        }
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
    for (SIZE_TYPE beg = 0;  beg < comment.length();
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
        x_comment += '\n';
    }
    return x_comment;
}


/* Dump the comment to stream "os"
 */
static bool s_WriteComment(CNcbiOstream& os, const string& comment)
{
    if (!comment.length())
        return true;

    if (strcmp(Endl(), "\n") == 0) {
        os << comment;
    } else {
        ITERATE(string, i, comment) {
            if (*i == '\n') {
                os << Endl();
            } else {
                os << *i;
            }
        }
    }
    return os.good();
}

// Does pos follow an odd number of backslashes?
inline bool s_Backslashed(const string& s, SIZE_TYPE pos)
{
    if (pos == 0) {
        return false;
    }
    SIZE_TYPE last_non_bs = s.find_last_not_of("\\", pos - 1);
    return (pos - last_non_bs) % 2 == 0;
}

inline string s_FlatKey(const string& section, const string& name)
{
    return section + '#' + name;
}


//////////////////////////////////////////////////////////////////////
//
// IRegistry

bool IRegistry::Empty(TFlags flags) const
{
    x_CheckFlags("IRegistry::Empty", flags, fLayerFlags);
    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    TReadGuard LOCK(*this);
    return x_Empty(flags);
}

bool IRegistry::Modified(TFlags flags) const
{
    x_CheckFlags("IRegistry::Modified", flags, fLayerFlags);
    if ( !(flags & fTransient) ) {
        flags |= fPersistent;
    }
    TReadGuard LOCK(*this);
    return x_Modified(flags);
}

void IRegistry::SetModifiedFlag(bool modified, TFlags flags)
{
    x_CheckFlags("IRegistry::SetModifiedFlag", flags, fLayerFlags);
    if ( !(flags & fTransient) ) {
        flags |= fPersistent;
    }
    TReadGuard LOCK(*this); // Treat the flag as semi-mutable
    x_SetModifiedFlag(modified, flags);
}

// Somewhat inefficient, but that can't really be helped....
bool IRegistry::Write(CNcbiOstream& os, TFlags flags) const
{
    x_CheckFlags("IRegistry::Write", flags, fLayerFlags | fInternalSpaces);
    if ( !(flags & fTransient) ) {
        flags |= fPersistent;
    }
    if ( !(flags & fNotJustCore) ) {
        flags |= fJustCore;
    }
    TReadGuard LOCK(*this);

    // Write file comment
    if ( !s_WriteComment(os, GetComment(kEmptyStr, kEmptyStr, flags)) )
        return false;

    list<string> sections;
    EnumerateSections(&sections, flags);

    ITERATE (list<string>, section, sections) {
        if ( !s_WriteComment(os, GetComment(*section, kEmptyStr, flags)) ) {
            return false;
        }
        os << '[' << *section << ']' << Endl();
        if ( !os ) {
            return false;
        }
        list<string> entries;
        EnumerateEntries(*section, &entries, flags);
        ITERATE (list<string>, entry, entries) {
            s_WriteComment(os, GetComment(*section, *entry, flags));
            // XXX - produces output that older versions can't handle
            // when the value contains control characters other than
            // CR (\r) or LF (\n).
            os << *entry << " = \""
               << NStr::PrintableString(Get(*section, *entry, flags)) << "\""
               << Endl();
            if ( !os ) {
                return false;
            }
        }
    }

    // Clear the modified bit (checking it first so as to perform the
    // const_cast<> only if absolutely necessary).
    if (Modified(flags & fLayerFlags)) {
        const_cast<IRegistry*>(this)->SetModifiedFlag
            (false, flags & fLayerFlags);
    }

    return true;
}

const string& IRegistry::Get(const string& section, const string& name,
                             TFlags flags) const
{
    x_CheckFlags("IRegistry::Get", flags, fLayerFlags | fInternalSpaces);
    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    string clean_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRegistry::Get: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return kEmptyStr;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !s_IsNameSection(clean_name, flags) ) {
        _TRACE("IRegistry::Get: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return kEmptyStr;
    }
    TReadGuard LOCK(*this);
    return x_Get(clean_section, clean_name, flags);
}

bool IRegistry::HasEntry(const string& section, const string& name,
                         TFlags flags) const
{
    x_CheckFlags("IRegistry::HasEntry", flags, fLayerFlags | fInternalSpaces);
    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    string clean_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRegistry::HasEntry: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return false;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !clean_name.empty()  &&  !s_IsNameSection(clean_name, flags) ) {
        _TRACE("IRegistry::HasEntry: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return false;
    }
    TReadGuard LOCK(*this);
    return x_HasEntry(clean_section, clean_name, flags);
}

const string& IRegistry::GetString(const string& section, const string& name,
                                   const string& default_value, TFlags flags)
    const
{
    const string& value = Get(section, name, flags);
    return value.empty() ? default_value : value;
}

int IRegistry::GetInt(const string& section, const string& name,
                      int default_value, TFlags flags, EErrAction err_action)
    const
{
    const string& value = Get(section, name, flags);
    if (value.empty()) {
        return default_value;
    }

    try {
        return NStr::StringToInt(value);
    } catch (CStringException& ex) {
        if (err_action == eReturn) {
            return default_value;
        }

        string msg = "IRegistry::GetInt(): [" + section + ']' + name;

        if (err_action == eThrow) {
            NCBI_RETHROW_SAME(ex, msg);
        } else if (err_action == eErrPost) {
            ERR_POST(ex.what() << msg);
        }

        return default_value;
    }
}

bool IRegistry::GetBool(const string& section, const string& name,
                        bool default_value, TFlags flags,
                        EErrAction err_action) const
{
    const string& value = Get(section, name, flags);
    if (value.empty()) {
        return default_value;
    }

    try {
        return NStr::StringToBool(value);
    } catch (CStringException& ex) {
        if (err_action == eReturn) {
            return default_value;
        }

        string msg = "IRegistry::GetBool(): [" + section + ']' + name;

        if (err_action == eThrow) {
            NCBI_RETHROW_SAME(ex, msg);
        } else if (err_action == eErrPost) {
            ERR_POST(ex.what() << msg);
        }

        return default_value;
    }
}

double IRegistry::GetDouble(const string& section, const string& name,
                            double default_value, TFlags flags,
                            EErrAction err_action) const
{
    const string& value = Get(section, name, flags);
    if (value.empty()) {
        return default_value;
    }

    try {
        return NStr::StringToDouble(value);
    } catch (CStringException& ex) {
        if (err_action == eReturn) {
            return default_value;
        }

        string msg = "IRegistry::GetDouble()";
        msg += " Reg entry:" + section + ":" + name;

        if (err_action == eThrow) {
            NCBI_RETHROW_SAME(ex, msg);
        } else if (err_action == eErrPost) {
            ERR_POST(ex.what() << msg);
        }

        return default_value;
    }
}

const string& IRegistry::GetComment(const string& section, const string& name,
                                    TFlags flags) const
{
    x_CheckFlags("IRegistry::GetComment", flags,
                 fLayerFlags | fInternalSpaces);
    string clean_section = NStr::TruncateSpaces(section);
    if ( !clean_section.empty()  &&  !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRegistry::GetComment: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return kEmptyStr;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !clean_name.empty()  &&  !s_IsNameSection(clean_name, flags) ) {
        _TRACE("IRegistry::GetComment: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return kEmptyStr;
    }
    TReadGuard LOCK(*this);
    return x_GetComment(clean_section, clean_name, flags);
}

void IRegistry::EnumerateSections(list<string>* sections, TFlags flags) const
{
    x_CheckFlags("IRegistry::EnumerateSections", flags,
                 fLayerFlags | fInternalSpaces);
    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    _ASSERT(sections);
    sections->clear();
    TReadGuard LOCK(*this);
    x_Enumerate(kEmptyStr, *sections, flags);
}

void IRegistry::EnumerateEntries(const string& section, list<string>* entries,
                                 TFlags flags) const
{
    x_CheckFlags("IRegistry::EnumerateEntries", flags,
                 fLayerFlags | fInternalSpaces);
    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    _ASSERT(entries);
    entries->clear();
    string clean_section = NStr::TruncateSpaces(section);
    if ( !clean_section.empty()  &&  !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRegistry::EnumerateEntries: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return;
    }
    TReadGuard LOCK(*this);
    x_Enumerate(clean_section, *entries, flags);
}

void IRegistry::ReadLock (void)
{
    x_ChildLockAction(&IRegistry::ReadLock);
    m_Lock.ReadLock();
}

void IRegistry::WriteLock(void)
{
    x_ChildLockAction(&IRegistry::WriteLock);
    m_Lock.WriteLock();
}

void IRegistry::Unlock(void)
{
    m_Lock.Unlock();
    x_ChildLockAction(&IRegistry::Unlock);
}

void IRegistry::x_CheckFlags(const string& func, TFlags& flags, TFlags allowed)
{
    if (flags & ~allowed)
        _TRACE(func << "(): extra flags passed: "
               << setiosflags(IOS_BASE::hex) << flags);
    flags &= allowed;
}

//////////////////////////////////////////////////////////////////////
//
// IRWRegistry

void IRWRegistry::Clear(TFlags flags)
{
    x_CheckFlags("IRWRegistry::Clear", flags, fLayerFlags | fInternalSpaces);
    TWriteGuard LOCK(*this);
    if ( (flags & fPersistent)  &&  !x_Empty(fPersistent) ) {
        x_SetModifiedFlag(true, flags & ~fTransient);
    }
    if ( (flags & fTransient)  &&  !x_Empty(fTransient) ) {
        x_SetModifiedFlag(true, flags & ~fPersistent);
    }
    x_Clear(flags);
}

void IRWRegistry::Read(CNcbiIstream& is, TFlags flags)
{
    x_CheckFlags("IRWRegistry::Read", flags,
                 fTransient | fNoOverride | fIgnoreErrors | fInternalSpaces);
    x_Read(is, flags);
}

void IRWRegistry::x_Read(CNcbiIstream& is, TFlags flags)
{
    // Whether to consider this read to be (unconditionally) non-modifying
    EFlags layer         = (flags & fTransient) ? fTransient : fPersistent;
    bool   non_modifying = Empty(layer)  &&  !Modified(layer);
    bool   ignore_errors = flags & fIgnoreErrors;

    // Adjust flags for Set()
    flags = (flags & ~fTPFlags & ~fIgnoreErrors) | layer;

    string    str;          // the line being parsed
    SIZE_TYPE line;         // # of the line being parsed
    string    section;      // current section name
    string    comment;      // current comment

    for (line = 1;  NcbiGetlineEOL(is, str);  ++line) {
        try {
            SIZE_TYPE len = str.length();
            SIZE_TYPE beg = 0;

            while (beg < len  &&  isspace(str[beg])) {
                ++beg;
            }
            if (beg == len) {
                comment += str;
                comment += '\n';
                continue;
            }

            switch (str[beg]) {

            case '#':  { // file comment
                SetComment(GetComment() + str + '\n');
                break;
            }

            case ';':  { // section or entry comment
                comment += str;
                comment += '\n';
                break;
            }

            case '[':  { // section name
                ++beg;
                SIZE_TYPE end = str.find_first_of(']', beg + 1);
                if (end == NPOS) {
                    NCBI_THROW2(CRegistryException, eSection,
                                "Invalid registry section(']' is missing): `"
                                + str + "'", line);
                }
                section = NStr::TruncateSpaces(str.substr(beg, end - beg));
                if (section.empty()) {
                    NCBI_THROW2(CRegistryException, eSection,
                                "Unnamed registry section: `" + str + "'",
                                line);
                } else if ( !s_IsNameSection(section, flags) ) {
                    NCBI_THROW2(CRegistryException, eSection,
                                "Invalid registry section name: `"
                                + str + "'", line);
                }
                // add section comment
                if ( !comment.empty() ) {
                    SetComment(GetComment(section) + comment, section);
                    comment.erase();
                }
                break;
            }

            default:  { // regular entry
                string name, value;
                if ( !NStr::SplitInTwo(str, "=", name, value) ) {
                    NCBI_THROW2(CRegistryException, eEntry,
                                "Invalid registry entry format: '" + str + "'",
                                line);
                }
                NStr::TruncateSpacesInPlace(name);
                if ( !s_IsNameSection(name, flags) ) {
                    NCBI_THROW2(CRegistryException, eEntry,
                                "Invalid registry entry name: '" + str + "'",
                                line);
                }
            
                NStr::TruncateSpacesInPlace(value);
                if (value.empty()) {
                    if ( !(flags & fNoOverride) ) {
                        Set(section, name, kEmptyStr, flags, comment);
                        comment.erase();
                    }
                    break;
                }
                // read continuation lines, if any
                string cont;
                while (s_Backslashed(value, value.size())
                       &&  NcbiGetlineEOL(is, cont)) {
                    ++line;
                    value[value.size() - 1] = '\n';
                    value += NStr::TruncateSpaces(cont);
                    str   += 'n' + cont; // for presentation in exceptions
                }

                // Historically, " may appear unescaped at the beginning,
                // end, both, or neither.
                beg = 0;
                SIZE_TYPE end = value.size();
                for (SIZE_TYPE pos = value.find('\"');
                     pos < end  &&  pos != NPOS;
                     pos = value.find('\"', pos + 1)) {
                    if (s_Backslashed(value, pos)) {
                        continue;
                    } else if (pos == beg) {
                        ++beg;
                    } else if (pos == end - 1) {
                        --end;
                    } else {
                        NCBI_THROW2(CRegistryException, eValue,
                                    "Single(unescaped) '\"' in the middle "
                                    "of registry value: '" + str + "'",
                                    line);
                    }
                }

                try {
                    value = NStr::ParseEscapes(value.substr(beg, end - beg));
                } catch (CStringException&) {
                    NCBI_THROW2(CRegistryException, eValue,
                                "Badly placed '\\' in the registry value: '"
                                + str + "'", line);

                }
                Set(section, name, value, flags, comment);
                comment.erase();
            }
            }
        } catch (exception& e) {
            if (ignore_errors) {
                ERR_POST(e.what());
            } else {
                throw;
            }
        }
    }

    if ( !is.eof() ) {
        NCBI_THROW2(CRegistryException, eErr,
                    "Error in reading the registry: '" + str + "'", line);
    }

    if ( non_modifying ) {
        SetModifiedFlag(false, layer);
    }
}

bool IRWRegistry::Set(const string& section, const string& name,
                      const string& value, TFlags flags,
                      const string& comment)
{
    x_CheckFlags("IRWRegistry::Set", flags,
                 fPersistent | fNoOverride | fTruncate | fInternalSpaces);
    string clean_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRWRegistry::Set: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return false;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !s_IsNameSection(clean_name, flags) ) {
        _TRACE("IRWRegistry::Set: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return false;
    }
    SIZE_TYPE beg = 0, end = value.size();
    if (flags & fTruncate) {
        // don't use TruncateSpaces, since newlines should stay
        beg = value.find_first_not_of(" \r\t\v");
        end = value.find_last_not_of (" \r\t\v");
        if (beg == NPOS) {
            _ASSERT(end == NPOS);
            beg = 1;
            end = 0;
        }
    }
    TWriteGuard LOCK(*this);
    if (x_Set(clean_section, clean_name, value.substr(beg, end - beg + 1),
              flags, s_ConvertComment(comment, section.empty()))) {
        x_SetModifiedFlag(true, flags);
        return true;
    } else {
        return false;
    }
}

bool IRWRegistry::SetComment(const string& comment, const string& section,
                             const string& name, TFlags flags)
{
    x_CheckFlags("IRWRegistry::SetComment", flags,
                 fTransient | fNoOverride | fInternalSpaces);
    string clean_section = NStr::TruncateSpaces(section);
    if ( !clean_section.empty()  &&  !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRWRegistry::SetComment: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return false;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !clean_name.empty()  &&  !s_IsNameSection(clean_name, flags) ) {
        _TRACE("IRWRegistry::SetComment: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return false;
    }
    TWriteGuard LOCK(*this);
    if (x_SetComment(s_ConvertComment(comment, section.empty()),
                     clean_section, clean_name, flags)) {
        x_SetModifiedFlag(true, fPersistent);
        return true;
    } else {
        return false;
    }
}

bool IRWRegistry::MaybeSet(string& target, const string& value, TFlags flags)
{
    if (target.empty()) {
        target = value;
        return !value.empty();
    } else if ( !(flags & fNoOverride) ) {
        target = value;
        return true;
    } else {
        return false;
    }
}


//////////////////////////////////////////////////////////////////////
//
// CMemoryRegistry

bool CMemoryRegistry::x_Empty(TFlags) const
{
    TReadGuard LOCK(*this);
    return m_Sections.empty()  &&  m_RegistryComment.empty();
}

const string& CMemoryRegistry::x_Get(const string& section, const string& name,
                                     TFlags) const
{
    TSections::const_iterator sit = m_Sections.find(section);
    if (sit == m_Sections.end()) {
        return kEmptyStr;
    }
    const TEntries& entries = sit->second.entries;
    TEntries::const_iterator eit = entries.find(name);
    return (eit == entries.end()) ? kEmptyStr : eit->second.value;
}

bool CMemoryRegistry::x_HasEntry(const string& section, const string& name,
                                 TFlags) const
{
    TSections::const_iterator sit = m_Sections.find(section);
    if (sit == m_Sections.end()) {
        return false;
    } else if (name.empty()) {
        return true;
    }
    const TEntries& entries = sit->second.entries;
    TEntries::const_iterator eit = entries.find(name);
    return eit != entries.end();
}

const string& CMemoryRegistry::x_GetComment(const string& section,
                                            const string& name,
                                            TFlags) const
{
    if (section.empty()) {
        return m_RegistryComment;
    }
    TSections::const_iterator sit = m_Sections.find(section);
    if (sit == m_Sections.end()) {
        return kEmptyStr;
    } else if (name.empty()) {
        return sit->second.comment;
    }
    const TEntries& entries = sit->second.entries;
    TEntries::const_iterator eit = entries.find(name);
    return (eit == entries.end()) ? kEmptyStr : eit->second.comment;
}

void CMemoryRegistry::x_Enumerate(const string& section, list<string>& entries,
                                  TFlags flags) const
{
    if (section.empty()) {
        ITERATE (TSections, it, m_Sections) {
            if (s_IsNameSection(it->first, flags)) {
                entries.push_back(it->first);
            }
        }
    } else {
        TSections::const_iterator sit = m_Sections.find(section);
        if (sit != m_Sections.end()) {
            ITERATE (TEntries, it, sit->second.entries) {
                if (s_IsNameSection(it->first, flags)) {
                    entries.push_back(it->first);
                }
            }
        }
    }
}

void CMemoryRegistry::x_Clear(TFlags)
{
    m_RegistryComment.erase();
    m_Sections.clear();
}

bool CMemoryRegistry::x_Set(const string& section, const string& name,
                            const string& value, TFlags flags,
                            const string& comment)
{
    if (value.empty()) {
        if (flags & fNoOverride) {
            return false;
        }
        // remove
        TSections::iterator sit = m_Sections.find(section);
        if (sit == m_Sections.end()) {
            return false;
        }
        TEntries& entries = sit->second.entries;
        TEntries::iterator eit = entries.find(name);
        if (eit == entries.end()) {
            return false;
        } else {
            entries.erase(eit);
            if (entries.empty()  &&  sit->second.comment.empty()) {
                m_Sections.erase(sit);
            }
            return true;
        }
    } else {
        SEntry& entry = m_Sections[section].entries[name];
#if 0
        if (entry.value == value) {
            if (entry.comment != comment) {
                return MaybeSet(entry.comment, comment, flags);
            }
            return false; // not actually modified
        }
#endif
        if (MaybeSet(entry.value, value, flags)) {
            MaybeSet(entry.comment, comment, flags);
            return true;
        }
        return false;
    }
}

bool CMemoryRegistry::x_SetComment(const string& comment,
                                   const string& section, const string& name,
                                   TFlags flags)
{
    if (comment.empty()  &&  (flags & fNoOverride)) {
        return false;
    }
    if (section.empty()) {
        return MaybeSet(m_RegistryComment, comment, flags);
    }
    TSections::iterator sit = m_Sections.find(section);
    if (sit == m_Sections.end()) {
        if (comment.empty()) {
            return false;
        } else {
            sit = m_Sections.insert(make_pair(section, SSection())).first;
        }
    }
    TEntries& entries = sit->second.entries;
    if (name.empty()) {
        if (comment.empty()  &&  entries.empty()) {
            m_Sections.erase(sit);
            return true;
        } else {
            return MaybeSet(sit->second.comment, comment, flags);
        }
    }
    TEntries::iterator eit = entries.find(name);
    if (eit == entries.end()) {
        return false;
    } else {
        return MaybeSet(eit->second.comment, comment, flags);
    }
}


//////////////////////////////////////////////////////////////////////
//
// CCompoundRegistry

void CCompoundRegistry::Add(const IRegistry& reg, TPriority prio,
                            const string& name)
{
    // Needed for some operations that touch (only) metadata...
    IRegistry& nc_reg = const_cast<IRegistry&>(reg);
    // XXX - Check whether reg is a duplicate, at least in debug mode?
    m_PriorityMap.insert(TPriorityMap::value_type
                         (prio, CRef<IRegistry>(&nc_reg)));
    if (name.size()) {
        CRef<IRegistry>& preg = m_NameMap[name];
        if (preg) {
            NCBI_THROW2(CRegistryException, eErr,
                        "CCompoundRegistry::Add: name " + name
                        + " already in use", 0);
        } else {
            preg.Reset(&nc_reg);
        }
    }
}

void CCompoundRegistry::Remove(const IRegistry& reg)
{
    NON_CONST_ITERATE (TNameMap, it, m_NameMap) {
        if (it->second == &reg) {
            m_NameMap.erase(it);
            break; // subregistries should be unique
        }
    }
    NON_CONST_ITERATE (TPriorityMap, it, m_PriorityMap) {
        if (it->second == &reg) {
            m_PriorityMap.erase(it);
            return; // subregistries should be unique
        }
    }
    // already returned if found...
    NCBI_THROW2(CRegistryException, eErr,
                "CCompoundRegistry::Remove:"
                " reg is not a (direct) subregistry of this.", 0);
}

CConstRef<IRegistry> CCompoundRegistry::FindByName(const string& name) const
{
    TNameMap::const_iterator it = m_NameMap.find(name);
    return it == m_NameMap.end() ? CConstRef<IRegistry>() : it->second;
}

#define REV_ITERATE(Type, Var, Cont) \
    for ( Type::const_reverse_iterator Var = (Cont).rbegin(), NCBI_NAME2(Var,_end) = (Cont).rend();  Var != NCBI_NAME2(Var,_end);  ++Var )

CConstRef<IRegistry> CCompoundRegistry::FindByContents(const string& section,
                                                       const string& entry,
                                                       TFlags flags) const
{
    REV_ITERATE(TPriorityMap, it, m_PriorityMap) {
        if (it->second->HasEntry(section, entry, flags & ~fJustCore)) {
            return it->second;
        }
    }
    return null;
}

bool CCompoundRegistry::x_Empty(TFlags flags) const
{
    REV_ITERATE (TPriorityMap, it, m_PriorityMap) {
        if ((flags & fJustCore)  &&  (it->first < m_CoreCutoff)) {
            break;
        }
        if ( !it->second->Empty(flags & ~fJustCore) ) {
            return false;
        }
    }
    return true;
}

bool CCompoundRegistry::x_Modified(TFlags flags) const
{
    REV_ITERATE (TPriorityMap, it, m_PriorityMap) {
        if ((flags & fJustCore)  &&  (it->first < m_CoreCutoff)) {
            break;
        }
        if ( it->second->Modified(flags & ~fJustCore) ) {
            return true;
        }
    }
    return false;
}

void CCompoundRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    _ASSERT( !modified );
    for (TPriorityMap::reverse_iterator it = m_PriorityMap.rbegin();
         it != m_PriorityMap.rend();  ++it) {
        if ((flags & fJustCore)  &&  (it->first < m_CoreCutoff)) {
            break;
        }
        it->second->SetModifiedFlag(modified, flags & ~fJustCore);
    }
}

const string& CCompoundRegistry::x_Get(const string& section,
                                       const string& name,
                                       TFlags flags) const
{
    CConstRef<IRegistry> reg = FindByContents(section, name,
                                              flags & ~fJustCore);
    return reg ? reg->Get(section, name, flags & ~fJustCore) : kEmptyStr;
}

bool CCompoundRegistry::x_HasEntry(const string& section, const string& name,
                                   TFlags flags) const
{
    return FindByContents(section, name, flags).NotEmpty();
}

const string& CCompoundRegistry::x_GetComment(const string& section,
                                              const string& name, TFlags flags)
    const
{
    if ( m_PriorityMap.empty() ) {
        return kEmptyStr;
    }

    CConstRef<IRegistry> reg;
    if (section.empty()) {
        reg = m_PriorityMap.rbegin()->second;
    } else {
        reg = FindByContents(section, name, flags);
    }
    return reg ? reg->GetComment(section, name, flags & ~fJustCore)
        : kEmptyStr;
}

void CCompoundRegistry::x_Enumerate(const string& section,
                                    list<string>& entries, TFlags flags) const
{
    set<string> accum;
    REV_ITERATE (TPriorityMap, it, m_PriorityMap) {
        if ((flags & fJustCore)  &&  (it->first < m_CoreCutoff)) {
            break;
        }
        list<string> tmp;
        it->second->EnumerateEntries(section, &tmp, flags & ~fJustCore);
        ITERATE (list<string>, it2, tmp) {
            accum.insert(*it2);
        }
    }
    ITERATE (set<string>, it, accum) {
        entries.push_back(*it);
    }
}

void CCompoundRegistry::x_ChildLockAction(FLockAction action)
{
    NON_CONST_ITERATE (TPriorityMap, it, m_PriorityMap) {
        ((*it->second).*action)();
    }
}


//////////////////////////////////////////////////////////////////////
//
// CTwoLayerRegistry

CTwoLayerRegistry::CTwoLayerRegistry(IRWRegistry* persistent)
    : m_Transient(CRegRef(new CMemoryRegistry)),
      m_Persistent(CRegRef(persistent ? persistent : new CMemoryRegistry))
{
}

bool CTwoLayerRegistry::x_Empty(TFlags flags) const
{
    // mask out fTPFlags whe 
    if (flags & fTransient  &&  !m_Transient->Empty(flags | fTPFlags) ) {
        return false;
    } else if (flags & fPersistent
               &&  !m_Persistent->Empty(flags | fTPFlags) ) {
        return false;
    } else {
        return true;
    }
}

bool CTwoLayerRegistry::x_Modified(TFlags flags) const
{
    if (flags & fTransient  &&  m_Transient->Modified(flags | fTPFlags)) {
        return true;
    } else if (flags & fPersistent
               &&  m_Persistent->Modified(flags | fTPFlags)) {
        return true;
    } else {
        return false;
    }
}

void CTwoLayerRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    if (flags & fTransient) {
        m_Transient->SetModifiedFlag(modified, flags | fTPFlags);
    }
    if (flags & fPersistent) {
        m_Persistent->SetModifiedFlag(modified, flags | fTPFlags);
    }
}

const string& CTwoLayerRegistry::x_Get(const string& section,
                                       const string& name, TFlags flags) const
{
    if (flags & fTransient) {
        const string& result = m_Transient->Get(section, name,
                                                flags & ~fTPFlags);
        if ( !result.empty()  ||  !(flags & fPersistent) ) {
            return result;
        }
    }
    return m_Persistent->Get(section, name, flags & ~fTPFlags);
}

bool CTwoLayerRegistry::x_HasEntry(const string& section, const string& name,
                                   TFlags flags) const
{
    return (((flags & fTransient)
             &&  m_Transient->HasEntry(section, name, flags & ~fTPFlags))  ||
            ((flags & fPersistent)
             &&  m_Persistent->HasEntry(section, name, flags & ~fTPFlags)));
}

const string& CTwoLayerRegistry::x_GetComment(const string& section,
                                              const string& name,
                                              TFlags flags) const
{
    if (flags & fTransient) {
        const string& result = m_Transient->GetComment(section, name,
                                                       flags & ~fTPFlags);
        if ( !result.empty()  ||  !(flags & fPersistent) ) {
            return result;
        }
    }
    return m_Persistent->GetComment(section, name, flags & ~fTPFlags);
}

void CTwoLayerRegistry::x_Enumerate(const string& section,
                                    list<string>& entries, TFlags flags) const
{
    switch (flags & fTPFlags) {
    case fTransient:
        m_Transient->EnumerateEntries(section, &entries, flags | fTPFlags);
        break;
    case fPersistent:
        m_Persistent->EnumerateEntries(section, &entries, flags | fTPFlags);
        break;
    case fTPFlags:
    {
        list<string> tl, pl;
        m_Transient ->EnumerateEntries(section, &tl, flags | fTPFlags);
        m_Persistent->EnumerateEntries(section, &pl, flags | fTPFlags);
        set_union(pl.begin(), pl.end(), tl.begin(), tl.end(),
                  back_inserter(entries), PNocase());
        break;
    }
    default:
        _TROUBLE;
    }
}

void CTwoLayerRegistry::x_ChildLockAction(FLockAction action)
{
    ((*m_Transient).*action)();
    ((*m_Persistent).*action)();
}

void CTwoLayerRegistry::x_Clear(TFlags flags)
{
    if (flags & fTransient) {
        m_Transient->Clear(flags | fTPFlags);
    }
    if (flags & fPersistent) {
        m_Persistent->Clear(flags | fTPFlags);
    }
}

bool CTwoLayerRegistry::x_Set(const string& section, const string& name,
                              const string& value, TFlags flags,
                              const string& comment)
{
    if (flags & fPersistent) {
        return m_Persistent->Set(section, name, value, flags & ~fTPFlags,
                                 comment);
    } else {
        return m_Transient->Set(section, name, value, flags & ~fTPFlags,
                                comment);
    }
}

bool CTwoLayerRegistry::x_SetComment(const string& comment,
                                     const string& section, const string& name,
                                     TFlags flags)
{
    if (flags & fTransient) {
        return m_Transient->SetComment(comment, section, name,
                                       flags & ~fTPFlags);
    } else {
        return m_Persistent->SetComment(comment, section, name,
                                        flags & ~fTPFlags);
    }
}



//////////////////////////////////////////////////////////////////////
//
// CNcbiRegistry --  elaborate general-purpose setup

const char* CNcbiRegistry::sm_MainRegName = ".main";
const char* CNcbiRegistry::sm_EnvRegName  = ".env";
const char* CNcbiRegistry::sm_FileRegName = ".file";

inline
void CNcbiRegistry::x_Init(void)
{
    m_AllRegistries.Reset(new CCompoundRegistry);
    m_AllRegistries->SetCoreCutoff(ePriority_Reserved);

    m_MainRegistry.Reset(new CTwoLayerRegistry);
    m_AllRegistries->Add(*m_MainRegistry, ePriority_Main);

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        m_EnvRegistry.Reset(new CEnvironmentRegistry(app->SetEnvironment()));
    } else {
        m_EnvRegistry.Reset(new CEnvironmentRegistry);
    }
    m_AllRegistries->Add(*m_EnvRegistry, ePriority_Environment);

    m_FileRegistry.Reset(new CTwoLayerRegistry);
    m_AllRegistries->Add(*m_FileRegistry, ePriority_File);
}

CNcbiRegistry::CNcbiRegistry(void)
{
    x_Init();
}

CNcbiRegistry::CNcbiRegistry(CNcbiIstream& is, TFlags flags)
{
    x_CheckFlags("CNcbiRegistry::CNcbiRegistry", flags, fTransient);
    x_Init();
    m_FileRegistry->Read(is, flags);
}

CNcbiRegistry::~CNcbiRegistry()
{
}

CNcbiRegistry::TPriority CNcbiRegistry::GetCoreCutoff(void)
{
    return m_AllRegistries->GetCoreCutoff();
}

void CNcbiRegistry::SetCoreCutoff(TPriority prio)
{
    m_AllRegistries->SetCoreCutoff(prio);
}

void CNcbiRegistry::Add(const IRegistry& reg, TPriority prio,
                        const string& name)
{
    if (name.size() > 1  &&  name[0] == '.') {
        NCBI_THROW2(CRegistryException, eErr,
                    "The sub-registry name " + name + " is reserved.", 0);
    }
    if (prio > ePriority_MaxUser) {
        ERR_POST(Warning
                 << "Reserved priority value automatically downgraded.");
        prio = ePriority_MaxUser;
    }
    m_AllRegistries->Add(reg, prio, name);
}

void CNcbiRegistry::Remove(const IRegistry& reg)
{
    if (&reg == m_MainRegistry.GetPointer()) {
        NCBI_THROW2(CRegistryException, eErr,
                    "The primary portion of the registry may not be removed.",
                    0);
    } else {
        m_AllRegistries->Remove(reg);
    }
}

CConstRef<IRegistry> CNcbiRegistry::FindByName(const string& name) const
{
    return m_AllRegistries->FindByName(name);
}

CConstRef<IRegistry> CNcbiRegistry::FindByContents(const string& section,
                                                   const string& entry,
                                                   TFlags        flags) const
{
    return m_AllRegistries->FindByContents(section, entry, flags);
}

bool CNcbiRegistry::x_Empty(TFlags flags) const
{
    return m_AllRegistries->Empty(flags);
}

bool CNcbiRegistry::x_Modified(TFlags flags) const
{
    return m_AllRegistries->Modified(flags);
}

void CNcbiRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    if (modified) {
        m_MainRegistry->SetModifiedFlag(modified, flags);
    } else {
        // CCompoundRegistry only permits clearing...
        m_AllRegistries->SetModifiedFlag(modified, flags);
    }
}

const string& CNcbiRegistry::x_Get(const string& section, const string& name,
                                   TFlags flags) const
{
    TClearedEntries::const_iterator it
        = m_ClearedEntries.find(s_FlatKey(section, name));
    if (it != m_ClearedEntries.end()) {
        flags &= ~it->second;
        if ( !flags ) {
            return kEmptyStr;
        }
    }
    return m_AllRegistries->Get(section, name, flags);
}

bool CNcbiRegistry::x_HasEntry(const string& section, const string& name,
                               TFlags flags) const
{
    TClearedEntries::const_iterator it
        = m_ClearedEntries.find(s_FlatKey(section, name));
    if (it != m_ClearedEntries.end()) {
        flags &= ~it->second;
        if ( !flags ) {
            return false;
        }
    }
    return m_AllRegistries->HasEntry(section, name, flags);
}

const string& CNcbiRegistry::x_GetComment(const string& section,
                                          const string& name,
                                          TFlags flags) const
{
    return m_AllRegistries->GetComment(section, name, flags);
}

void CNcbiRegistry::x_Enumerate(const string& section, list<string>& entries,
                                TFlags flags) const
{
    // XXX - may reveal "ghosts"
    m_AllRegistries->EnumerateEntries(section, &entries, flags);
}

void CNcbiRegistry::x_ChildLockAction(FLockAction action)
{
    m_AllRegistries->x_ChildLockAction(action);
}

void CNcbiRegistry::x_Clear(TFlags flags) // XXX - should this do more?
{
    m_MainRegistry->Clear(flags);
    m_FileRegistry->Clear(flags);
}

bool CNcbiRegistry::x_Set(const string& section, const string& name,
                          const string& value, TFlags flags,
                          const string& comment)
{
    if ( !(flags & fPersistent) ) {
        flags |= fTransient;
    }
    bool was_empty = Get(section, name, flags).empty();
    _TRACE('[' << section << ']' << name << " = " << value << ':' << flags);
    if ((flags & fNoOverride)  &&  !was_empty) {
        return false;
    }
    if (value.empty()) {
        m_MainRegistry->Set(section, name, value, flags, comment);
        m_ClearedEntries[s_FlatKey(section, name)] |= flags;
        return !was_empty;
    } else {
        TClearedEntries::iterator it
            = m_ClearedEntries.find(s_FlatKey(section, name));
        if (it != m_ClearedEntries.end()) {
            if ((it->second &= ~flags) == 0) {
                m_ClearedEntries.erase(it);
            }
        }
    }
    return m_MainRegistry->Set(section, name, value, flags, comment);
}

bool CNcbiRegistry::x_SetComment(const string& comment, const string& section,
                                 const string& name, TFlags flags)
{
    return m_MainRegistry->SetComment(comment, section, name, flags);
}

void CNcbiRegistry::x_Read(CNcbiIstream& is, TFlags flags)
{
    // Normally, all settings should go to the main portion.  However,
    // loading an initial configuration file should instead go to the
    // file portion so that environment settings can take priority.
    if (m_MainRegistry->Empty()  &&  m_FileRegistry->Empty()) {
        m_FileRegistry->Read(is, flags);
    } else {
        // This will only affect the main registry, but still needs to
        // go through CNcbiRegistry::x_Set.
        IRWRegistry::x_Read(is, flags);
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.49  2005/03/21 19:46:30  ucko
 * Add support for two new flags: fIgnoreErrors, fInternalSpaces.
 *
 * Revision 1.48  2005/03/17 00:01:52  ucko
 * CNcbiRegistry::x_Read: don't overoptimize the standard case.
 *
 * Revision 1.47  2005/03/16 22:54:22  ucko
 * CNcbiRegistry::x_Set: fix behavior in fNoOverride case, sometimes
 * mishandled in previous revision.
 *
 * Revision 1.46  2005/03/14 15:52:09  ucko
 * Support taking settings from the environment.
 *
 * Revision 1.45  2005/01/18 15:18:51  ucko
 * CTwoLayerRegistry::x_Enumerate: remember to call set_union with PNocase().
 *
 * Revision 1.44  2005/01/12 16:55:15  vasilche
 * Avoid performance warning on MSVC.
 *
 * Revision 1.43  2004/12/20 17:34:13  ucko
 * Fix silly portability bugs.
 *
 * Revision 1.42  2004/12/20 15:28:34  ucko
 * Extensively refactor, and add support for subregistries.
 *
 * Revision 1.41  2004/07/28 18:59:53  kuznets
 * Fixed CNcbiRegistry::EnumerateEntries (added section name trim)
 *
 * Revision 1.40  2004/07/19 20:35:16  gouriano
 * Allow forward slash in section and entry names
 *
 * Revision 1.39  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.38  2003/10/20 21:55:12  vakatov
 * CNcbiRegistry::GetComment() -- make it "const"
 *
 * Revision 1.37  2003/06/26 18:54:48  rsmith
 * fix bug where comments multiplied blank lines after them.
 *
 * Revision 1.36  2003/06/25 20:05:57  rsmith
 * Fix small bugs setting comments
 *
 * Revision 1.35  2003/05/08 13:47:16  ivanov
 * Fixed Read() for right handle entry names with leading spaces
 *
 * Revision 1.34  2003/04/07 19:40:36  ivanov
 * Rollback to R1.32
 *
 * Revision 1.33  2003/04/07 16:08:29  ivanov
 * Added more thread-safety to CNcbiRegistry:: methods -- mutex protection.
 * Get() and GetComment() returns "string", not "string&".
 *
 * Revision 1.32  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.31  2003/02/28 19:24:51  vakatov
 * Get rid of redundant "const" in the return type of GetInt/Bool/Double()
 *
 * Revision 1.30  2003/02/24 19:56:05  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.29  2003/01/17 20:45:39  kuznets
 * Minor code cleanup~
 *
 * Revision 1.28  2003/01/17 20:27:30  kuznets
 * CNcbiRegistry added ErrPost error action
 *
 * Revision 1.27  2003/01/17 17:31:07  vakatov
 * CNcbiRegistry::GetString() to return "string", not "string&" -- for safety
 *
 * Revision 1.26  2002/12/30 23:23:07  vakatov
 * + GetString(), GetInt(), GetBool(), GetDouble() -- with defaults,
 * conversions and error handling control (to extend Get()'s functionality).
 *
 * Revision 1.25  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.24  2002/08/01 18:43:57  ivanov
 * Using NcbiGetlineEOL() instead s_NcbiGetline()
 *
 * Revision 1.23  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.22  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.21  2001/10/29 18:55:08  ivanov
 * Fixed adding blank lines at write registry file
 *
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
