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
#include <corelib/metareg.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/error_codes.hpp>
#include "ncbisys.hpp"

#include <algorithm>
#include <set>


#define NCBI_USE_ERRCODE_X   Corelib_Reg


BEGIN_NCBI_SCOPE

typedef CRegistryReadGuard  TReadGuard;
typedef CRegistryWriteGuard TWriteGuard;


// Valid symbols for a section/entry name
inline bool s_IsNameSectionSymbol(char ch, IRegistry::TFlags flags)
{
    return (isalnum((unsigned char) ch)
            ||  ch == '_'  ||  ch == '-' ||  ch == '.'  ||  ch == '/'
            ||  ((flags & IRegistry::fInternalSpaces)  &&  ch == ' '));
}


// Check if "str" consists of alphanumeric and '_' only
// Treat the case of set fSectionlessEntries separately
static bool s_IsNameSection(const string& str, IRegistry::TFlags flags)
{
    // Allow empty section name in case of fSectionlessEntries set
    if (str.empty() && !(flags & IRegistry::fSectionlessEntries) ) {
        return false;
    }

    ITERATE (string, it, str) {
        if (!s_IsNameSectionSymbol(*it, flags)) {
            return false;
        }
    }
    return true;
}

// Check if "str" consists of alphanumeric and '_' only
static bool s_IsNameEntry(const string& str, IRegistry::TFlags flags)
{
    // Do not allow empty entry name regardless of fSectionlessEntries
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

// Convert "comment" from plain text to comment
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


// Dump the comment to stream "os"
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
    x_CheckFlags("IRegistry::Write", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fCountCleared
                 | fSectionlessEntries);

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
        if(!(section->empty()))
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
               << Printable(Get(*section, *entry, flags)) << "\""
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
    x_CheckFlags("IRegistry::Get", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fSectionlessEntries);
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
    if ( !s_IsNameEntry(clean_name, flags) ) {
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
    x_CheckFlags("IRegistry::HasEntry", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fCountCleared
                 | fSections | fSectionlessEntries);

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
    if ( !clean_name.empty()  &&  !s_IsNameEntry(clean_name, flags) ) {
        _TRACE("IRegistry::HasEntry: bad entry name \""
               << NStr::PrintableString(name) << '\"');
        return false;
    }
    TReadGuard LOCK(*this);
    return x_HasEntry(clean_section, clean_name, flags);
}


string IRegistry::GetString(const string& section, const string& name,
                            const string& default_value, TFlags flags) const
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
            ERR_POST_X(1, ex.what() << msg);
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
            ERR_POST_X(2, ex.what() << msg);
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
        return NStr::StringToDouble(value, NStr::fDecimalPosixOrLocal);
    } catch (CStringException& ex) {
        if (err_action == eReturn) {
            return default_value;
        }

        string msg = "IRegistry::GetDouble()";
        msg += " Reg entry:" + section + ":" + name;

        if (err_action == eThrow) {
            NCBI_RETHROW_SAME(ex, msg);
        } else if (err_action == eErrPost) {
            ERR_POST_X(3, ex.what() << msg);
        }

        return default_value;
    }
}


const string& IRegistry::GetComment(const string& section, const string& name,
                                    TFlags flags) const
{
    x_CheckFlags("IRegistry::GetComment", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fSectionlessEntries);
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
    // Should clear fSectionlessEntries if set
    x_CheckFlags("IRegistry::EnumerateSections", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fCountCleared
                 | fSectionlessEntries);

    if ( !(flags & fTPFlags) ) {
        flags |= fTPFlags;
    }
    _ASSERT(sections);
    sections->clear();
    TReadGuard LOCK(*this);
    x_Enumerate(kEmptyStr, *sections, flags | fSections);
}


void IRegistry::EnumerateEntries(const string& section, list<string>* entries,
                                 TFlags flags) const
{
    x_CheckFlags("IRegistry::EnumerateEntries", flags,
                 (TFlags)fLayerFlags | fInternalSpaces | fCountCleared
                 | fSectionlessEntries | fSections);

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


void IRegistry::x_CheckFlags(const string& _DEBUG_ARG(func),
                             TFlags& flags, TFlags allowed)
{
    if (flags & ~allowed)
        _TRACE(func << "(): extra flags passed: "
               << resetiosflags(IOS_BASE::basefield)
               << setiosflags(IOS_BASE::hex | IOS_BASE::showbase)
               << flags);
    flags &= allowed;
}


//////////////////////////////////////////////////////////////////////
//
// IRWRegistry

IRegistry::TFlags IRWRegistry::AssessImpact(TFlags flags, EOperation op)
{
    // mask out irrelevant flags
    flags &= fLayerFlags | fTPFlags;
    switch (op) {
    case eClear:
        return flags;
    case eRead:
    case eSet:
        return ((flags & fTransient) ? fTransient : fPersistent) | fJustCore;
    default:
        _TROUBLE;
        return flags;
    }
}

void IRWRegistry::Clear(TFlags flags)
{
    x_CheckFlags("IRWRegistry::Clear", flags,
                 (TFlags)fLayerFlags | fInternalSpaces);
    TWriteGuard LOCK(*this);
    if ( (flags & fPersistent)  &&  !x_Empty(fPersistent) ) {
        x_SetModifiedFlag(true, flags & ~fTransient);
    }
    if ( (flags & fTransient)  &&  !x_Empty(fTransient) ) {
        x_SetModifiedFlag(true, flags & ~fPersistent);
    }
    x_Clear(flags);
}


IRWRegistry* IRWRegistry::Read(CNcbiIstream& is, TFlags flags,
                               const string& path)
{
    x_CheckFlags("IRWRegistry::Read", flags,
                 fTransient | fNoOverride | fIgnoreErrors | fInternalSpaces
                 | fWithNcbirc | fJustCore | fCountCleared
                 | fSectionlessEntries);

    if ( !is ) {
        return NULL;
    }

    // Ensure that x_Read gets a stream it can handle.
    EEncodingForm ef = GetTextEncodingForm(is, eBOM_Discard);
    if (ef == eEncodingForm_Utf16Native  ||  ef == eEncodingForm_Utf16Foreign) {
        CStringUTF8 s;
        ReadIntoUtf8(is, &s, ef);
        CNcbiIstrstream iss(s.c_str());
        return x_Read(iss, flags, path);
    } else {
        return x_Read(is, flags, path);
    }
}


IRWRegistry* IRWRegistry::x_Read(CNcbiIstream& is, TFlags flags,
                                 const string& path)
{
    // Whether to consider this read to be (unconditionally) non-modifying
    TFlags layer         = (flags & fTransient) ? fTransient : fPersistent;
    TFlags impact        = layer | fJustCore;
    bool   was_empty     = Empty(impact);
    bool   non_modifying = was_empty  &&  !Modified(impact);
    bool   ignore_errors = (flags & fIgnoreErrors) > 0;

    // Adjust flags for Set() fSectionlessEntries must survive this change
    flags = (flags & ~fTPFlags & ~fIgnoreErrors) | layer;

    string    str;          // the line being parsed
    SIZE_TYPE line;         // # of the line being parsed
    string    section(kEmptyStr);      // current section name
    string    comment;      // current comment
    string    in_path = path.empty() ? kEmptyStr : (" in " + path);

    for (line = 1;  NcbiGetlineEOL(is, str);  ++line) {
        try {
            SIZE_TYPE len = str.length();
            SIZE_TYPE beg = 0;

            while (beg < len  &&  isspace((unsigned char) str[beg])) {
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
                                "Invalid registry section" + in_path
                                + " (']' is missing): `" + str + "'", line);
                }
                section = NStr::TruncateSpaces(str.substr(beg, end - beg));
                if (section.empty()) {
                    NCBI_THROW2(CRegistryException, eSection,
                                "Unnamed registry section" + in_path + ": `"
                                + str + "'", line);
                } else if ( !s_IsNameSection(section, flags) ) {
                    NCBI_THROW2(CRegistryException, eSection,
                                "Invalid registry section name" + in_path
                                + ": `" + str + "'", line);
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
                                "Invalid registry entry format" + in_path
                                + ": '" + str + "'", line);
                }
                NStr::TruncateSpacesInPlace(name);
                if ( !s_IsNameSection(name, flags) ) {
                    NCBI_THROW2(CRegistryException, eEntry,
                                "Invalid registry entry name" + in_path + ": '"
                                + str + "'", line);
                }
            
                NStr::TruncateSpacesInPlace(value);
#if 0 // historic behavior; could inappropriately expose entries in lower layers
                if (value.empty()) {
                    if ( !(flags & fNoOverride) ) {
                        Set(section, name, kEmptyStr, flags, comment);
                        comment.erase();
                    }
                    break;
                }
#endif
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
                                    "of registry value" + in_path + ": '"
                                    + str + "'", line);
                    }
                }

                try {
                    value = NStr::ParseEscapes(value.substr(beg, end - beg));
                } catch (CStringException&) {
                    NCBI_THROW2(CRegistryException, eValue,
                                "Badly placed '\\' in the registry value"
                                + in_path + ": '" + str + "'", line);

                }
                TFlags set_flags = flags & ~fJustCore;
                if (NStr::EqualNocase(section, "NCBI")
                    &&  NStr::EqualNocase(name, ".Inherits")
                    &&  HasEntry(section, name, flags)) {
                    const string& old_value = Get(section, name, flags);
                    if (flags & fNoOverride) {
                        value = old_value + ' ' + value;
                        set_flags &= ~fNoOverride;
                    } else {
                        value += ' ';
                        value += old_value;
                    }
                } else if (was_empty  &&  HasEntry(section, name, flags)) {
                    ERR_POST_X(8, Warning
                               << "Found multiple [" << section << "] "
                               << name << " settings" << in_path
                               << "; using the one from line " << line);
                }
                Set(section, name, value, set_flags, comment);
                comment.erase();
            }
            }
        } catch (exception& e) {
            if (ignore_errors) {
                ERR_POST_X(4, e.what());
            } else {
                throw;
            }
        }
    }

    if ( !is.eof() ) {
        ERR_POST_X(4, "Error reading the registry after line " << line
                   << in_path << ": " << str);
    }

    if ( non_modifying ) {
        SetModifiedFlag(false, impact);
    }

    return NULL;
}


bool IRWRegistry::Set(const string& section, const string& name,
                      const string& value, TFlags flags,
                      const string& comment)
{
    x_CheckFlags("IRWRegistry::Set", flags,
                 fPersistent | fNoOverride | fTruncate | fInternalSpaces
                 | fCountCleared | fSectionlessEntries);
    string clean_section = NStr::TruncateSpaces(section);
    if ( !s_IsNameSection(clean_section, flags) ) {
        _TRACE("IRWRegistry::Set: bad section name \""
               << NStr::PrintableString(section) << '\"');
        return false;
    }
    string clean_name = NStr::TruncateSpaces(name);
    if ( !s_IsNameEntry(clean_name, flags) ) {
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
    if ( !clean_name.empty()  &&  !s_IsNameEntry(clean_name, flags) ) {
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
                                 TFlags flags) const
{
    TSections::const_iterator sit = m_Sections.find(section);
    if (sit == m_Sections.end()) {
        return false;
    } else if (name.empty()) {
        return ((flags & fCountCleared) != 0) || !sit->second.cleared;
    }
    const TEntries& entries = sit->second.entries;
    TEntries::const_iterator eit = entries.find(name);
    if (eit == entries.end()) {
        return false;
    } else if ((flags & fCountCleared) != 0) {
        return true;
    } else {
        return !eit->second.value.empty();
    }
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
    if (section.empty() &&
        ( (flags & IRegistry::fSections)
          || !(flags & IRegistry::fSectionlessEntries))) {
        // Enumerate sections
        if(!(flags & (IRegistry::fSections | IRegistry::fSectionlessEntries)))
            _TRACE("Deprecated call to x_Enumerate with empty section name, "
                   " but with no fSections flag set");

        ITERATE (TSections, it, m_Sections) {
            if (s_IsNameSection(it->first, flags)
                &&  HasEntry(it->first, kEmptyStr, flags)) {
                entries.push_back(it->first);
            }
        }
    } else {
        TSections::const_iterator sit = m_Sections.find(section);
        if (sit != m_Sections.end()) {
            ITERATE (TEntries, it, sit->second.entries) {
                if (s_IsNameSection(it->first, flags)
                    &&  ((flags & fCountCleared) != 0
                         ||  !it->second.value.empty() )) {
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
    _TRACE(this << ": [" << section << ']' << name << " = " << value);
#if 0 // historic behavior; could inappropriately expose entries in lower layers
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
    } else
#endif
    {
        TSections::iterator sit = m_Sections.find(section);
        if (sit == m_Sections.end()) {
            sit = m_Sections.insert(make_pair(section, SSection(m_Flags)))
                .first;
            sit->second.cleared = false;
        }
        SEntry& entry = sit->second.entries[name];
#if 0
        if (entry.value == value) {
            if (entry.comment != comment) {
                return MaybeSet(entry.comment, comment, flags);
            }
            return false; // not actually modified
        }
#endif
        if ( !value.empty() ) {
            sit->second.cleared = false;
        } else if ( !entry.value.empty() ) {
            _ASSERT( !sit->second.cleared );
            bool cleared = true;
            ITERATE (TEntries, eit, sit->second.entries) {
                if (&eit->second != &entry  &&  !eit->second.value.empty() ) {
                    cleared = false;
                    break;
                }
            }
            sit->second.cleared = cleared;
        }
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
            sit = m_Sections.insert(make_pair(section, SSection(m_Flags)))
                  .first;
            sit->second.cleared = false;
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


CConstRef<IRegistry> CCompoundRegistry::FindByContents(const string& section,
                                                       const string& entry,
                                                       TFlags flags) const
{
    TFlags has_entry_flags = (flags | fCountCleared) & ~fJustCore;
    REVERSE_ITERATE(TPriorityMap, it, m_PriorityMap) {
        if (it->second->HasEntry(section, entry, has_entry_flags)) {
            return it->second;
        }
    }
    return null;
}


bool CCompoundRegistry::x_Empty(TFlags flags) const
{
    REVERSE_ITERATE (TPriorityMap, it, m_PriorityMap) {
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
    REVERSE_ITERATE (TPriorityMap, it, m_PriorityMap) {
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
    REVERSE_ITERATE (TPriorityMap, it, m_PriorityMap) {
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

CTwoLayerRegistry::CTwoLayerRegistry(IRWRegistry* persistent, TFlags flags)
    : m_Transient(CRegRef(new CMemoryRegistry(flags))),
      m_Persistent(CRegRef(persistent ? persistent
                           : new CMemoryRegistry(flags)))
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
// CNcbiRegistry -- compound R/W registry with extra policy and
// compatibility features.  (See below for CCompoundRWRegistry,
// which has been factored out.)

const char* CNcbiRegistry::sm_EnvRegName      = ".env";
const char* CNcbiRegistry::sm_FileRegName     = ".file";
const char* CNcbiRegistry::sm_OverrideRegName = ".overrides";
const char* CNcbiRegistry::sm_SysRegName      = ".ncbirc";

inline
void CNcbiRegistry::x_Init(void)
{
    CNcbiApplication* app = CNcbiApplication::Instance();
    TFlags            cf  = m_Flags & fCaseFlags;
    if (app) {
        m_EnvRegistry.Reset(new CEnvironmentRegistry(app->SetEnvironment(),
                                                     eNoOwnership, cf));
    } else {
        m_EnvRegistry.Reset(new CEnvironmentRegistry(cf));
    }
    x_Add(*m_EnvRegistry, ePriority_Environment, sm_EnvRegName);

    m_FileRegistry.Reset(new CTwoLayerRegistry(NULL, cf));
    x_Add(*m_FileRegistry, ePriority_File, sm_FileRegName);

    m_SysRegistry.Reset(new CTwoLayerRegistry(NULL, cf));
    x_Add(*m_SysRegistry, ePriority_Default - 1, sm_SysRegName);

    const TXChar* xoverride_path = NcbiSys_getenv(_TX("NCBI_CONFIG_OVERRIDES"));
    if (xoverride_path  &&  *xoverride_path) {
        string override_path = _T_STDSTRING(xoverride_path);
        m_OverrideRegistry.Reset(new CCompoundRWRegistry(cf));
        CMetaRegistry::SEntry entry
            = CMetaRegistry::Load(override_path, CMetaRegistry::eName_AsIs,
                                  0, cf, m_OverrideRegistry.GetPointer());
        if (entry.registry) {
            if (entry.registry != m_OverrideRegistry) {
                ERR_POST_X(5, Warning << "Resetting m_OverrideRegistry");
                m_OverrideRegistry.Reset(entry.registry);
            }
            x_Add(*m_OverrideRegistry, ePriority_Overrides, sm_OverrideRegName);
        } else {
            ERR_POST_ONCE(Warning
                          << "NCBI_CONFIG_OVERRIDES names nonexistent file "
                          << override_path);
            m_OverrideRegistry.Reset();
        }
    }
}


CNcbiRegistry::CNcbiRegistry(TFlags flags)
    : m_RuntimeOverrideCount(0), m_Flags(flags)
{
    x_Init();
}


CNcbiRegistry::CNcbiRegistry(CNcbiIstream& is, TFlags flags,
                             const string& path)
    : m_RuntimeOverrideCount(0), m_Flags(flags)
{
    x_CheckFlags("CNcbiRegistry::CNcbiRegistry", flags,
                 fTransient | fInternalSpaces | fWithNcbirc | fCaseFlags
                 | fSectionlessEntries);
    x_Init();
    m_FileRegistry->Read(is, flags & ~(fWithNcbirc | fCaseFlags));
    LoadBaseRegistries(flags, 0, path);
    IncludeNcbircIfAllowed(flags & ~fCaseFlags);
}


CNcbiRegistry::~CNcbiRegistry()
{
}


bool CNcbiRegistry::IncludeNcbircIfAllowed(TFlags flags)
{
    if (flags & fWithNcbirc) {
        flags &= ~fWithNcbirc;
    } else {
        return false;
    }

    if (getenv("NCBI_DONT_USE_NCBIRC")) {
        return false;
    }

    if (HasEntry("NCBI", "DONT_USE_NCBIRC")) {
        return false;
    }

    try {
        CMetaRegistry::SEntry entry
            = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni,
                                  0, flags, m_SysRegistry.GetPointer());
        if (entry.registry  &&  entry.registry != m_SysRegistry) {
            ERR_POST_X(5, Warning << "Resetting m_SysRegistry");
            m_SysRegistry.Reset(entry.registry);
        }
    } catch (CRegistryException& e) {
        ERR_POST_X(6, Critical << "CNcbiRegistry: "
                      "Syntax error in system-wide configuration file: "
                      << e.what());
        return false;
    }

    if ( !m_SysRegistry->Empty() ) {
        return true;
    }

    return false;
}


void CNcbiRegistry::x_Clear(TFlags flags) // XXX - should this do more?
{
    CCompoundRWRegistry::x_Clear(flags);
    m_FileRegistry->Clear(flags);
}


IRWRegistry* CNcbiRegistry::x_Read(CNcbiIstream& is, TFlags flags,
                                   const string& path)
{
    // Normally, all settings should go to the main portion.  However,
    // loading an initial configuration file should instead go to the
    // file portion so that environment settings can take priority.
    CConstRef<IRegistry> main_reg(FindByName(sm_MainRegName));
    if (main_reg->Empty()  &&  m_FileRegistry->Empty()) {
        m_FileRegistry->Read(is, flags & ~fWithNcbirc);
        LoadBaseRegistries(flags, 0, path);
        IncludeNcbircIfAllowed(flags);
        return NULL;
    } else if ((flags & fNoOverride) == 0) { // ensure proper layering
        CRef<CCompoundRWRegistry> crwreg
            (new CCompoundRWRegistry(m_Flags & fCaseFlags));
        crwreg->Read(is, flags);
        // Allow contents to override anything previously Set() directly.
        IRWRegistry& nc_main_reg
            = dynamic_cast<IRWRegistry&>(const_cast<IRegistry&>(*main_reg));
        if ((flags & fTransient) == 0) {
            flags |= fPersistent;
        }
        list<string> sections;
        crwreg->EnumerateSections(&sections, flags | fCountCleared);
        ITERATE (list<string>, sit, sections) {
            list<string> entries;
            crwreg->EnumerateEntries(*sit, &entries, flags | fCountCleared);
            ITERATE (list<string>, eit, entries) {
                // In principle, it should be possible to clear the setting
                // in nc_main_reg rather than duplicating it; however,
                // letting the entry in crwreg be visible would require
                // having CCompoundRegistry::FindByContents no longer force
                // fCountCleared, which breaks other corner cases (as shown
                // by test_sub_reg). :-/
                if (nc_main_reg.HasEntry(*sit, *eit, flags | fCountCleared)) {
                    nc_main_reg.Set(*sit, *eit, crwreg->Get(*sit, *eit), flags);
                }
            }
        }
        ++m_RuntimeOverrideCount;
        x_Add(*crwreg, ePriority_RuntimeOverrides + m_RuntimeOverrideCount,
              sm_OverrideRegName + NStr::UIntToString(m_RuntimeOverrideCount));
        return crwreg.GetPointer();
    } else {
        // This will only affect the main registry, but still needs to
        // go through CCompoundRWRegistry::x_Set.
        return CCompoundRWRegistry::x_Read(is, flags, path);
    }
}


//////////////////////////////////////////////////////////////////////
//
// CCompoundRWRegistry -- general-purpose setup

const char* CCompoundRWRegistry::sm_MainRegName       = ".main";
const char* CCompoundRWRegistry::sm_BaseRegNamePrefix = ".base:";


CCompoundRWRegistry::CCompoundRWRegistry(TFlags flags)
    : m_MainRegistry(new CTwoLayerRegistry),
      m_AllRegistries(new CCompoundRegistry),
      m_Flags(flags)
{
    x_Add(*m_MainRegistry, CCompoundRegistry::ePriority_Max - 1,
          sm_MainRegName);
}


CCompoundRWRegistry::~CCompoundRWRegistry()
{
}


CCompoundRWRegistry::TPriority CCompoundRWRegistry::GetCoreCutoff(void) const
{
    return m_AllRegistries->GetCoreCutoff();
}


void CCompoundRWRegistry::SetCoreCutoff(TPriority prio)
{
    m_AllRegistries->SetCoreCutoff(prio);
}


void CCompoundRWRegistry::Add(const IRegistry& reg, TPriority prio,
                              const string& name)
{
    if (name.size() > 1  &&  name[0] == '.') {
        NCBI_THROW2(CRegistryException, eErr,
                    "The sub-registry name " + name + " is reserved.", 0);
    }
    if (prio > ePriority_MaxUser) {
        ERR_POST_X(7, Warning
                      << "Reserved priority value automatically downgraded.");
        prio = ePriority_MaxUser;
    }
    x_Add(reg, prio, name);
}


void CCompoundRWRegistry::Remove(const IRegistry& reg)
{
    if (&reg == m_MainRegistry.GetPointer()) {
        NCBI_THROW2(CRegistryException, eErr,
                    "The primary portion of the registry may not be removed.",
                    0);
    } else {
        m_AllRegistries->Remove(reg);
    }
}


CConstRef<IRegistry> CCompoundRWRegistry::FindByName(const string& name) const
{
    return m_AllRegistries->FindByName(name);
}


CConstRef<IRegistry> CCompoundRWRegistry::FindByContents(const string& section,
                                                         const string& entry,
                                                         TFlags flags) const
{
    return m_AllRegistries->FindByContents(section, entry, flags);
}


bool CCompoundRWRegistry::LoadBaseRegistries(TFlags flags, int metareg_flags,
                                             const string& path)
{
    if (flags & fJustCore) {
        return false;
    }

    list<string> names;
    {{
        string s = m_MainRegistry->Get("NCBI", ".Inherits");
        if (s.empty()) {
            if (dynamic_cast<CNcbiRegistry*>(this) != NULL) {
                _TRACE("LoadBaseRegistries(" << this
                       << "): trying file registry");
                s = FindByName(CNcbiRegistry::sm_FileRegName)
                    ->Get("NCBI", ".Inherits");
            }
            if (s.empty()) {
                return false;
            }
        }
        _TRACE("LoadBaseRegistries(" << this << "): using " << s);
        NStr::Split(s, ", ", names, NStr::fSplit_CanSingleQuote);
    }}

    typedef pair<string, CRef<IRWRegistry> > TNewBase;
    typedef vector<TNewBase> TNewBases;
    TNewBases bases;
    SIZE_TYPE initial_num_bases = m_BaseRegNames.size();

    ITERATE (list<string>, it, names) {
        if (m_BaseRegNames.find(*it) != m_BaseRegNames.end()) {
            continue;
        }
        CRef<CCompoundRWRegistry> reg2
            (new CCompoundRWRegistry(m_Flags & fCaseFlags));
        // First try adding .ini unless it's already present; when a
        // file with the unsuffixed name also exists, it is likely an
        // executable that would be inappropriate to try to parse.
        CMetaRegistry::SEntry entry2;
        if (NStr::EndsWith(*it, ".ini")) {
            entry2.registry = NULL;
        } else {
            entry2 = CMetaRegistry::Load(*it, CMetaRegistry::eName_Ini,
                                         metareg_flags, flags,
                                         reg2.GetPointer(), path);
        }
        if (entry2.registry == NULL) {
            entry2 = CMetaRegistry::Load(*it, CMetaRegistry::eName_AsIs,
                                         metareg_flags, flags,
                                         reg2.GetPointer(), path);
        }
        if (entry2.registry) {
            m_BaseRegNames.insert(*it);
            bases.push_back(TNewBase(*it, entry2.registry));
        } else {
            ERR_POST(Critical << "Base registry " << *it
                     << " absent or unreadable");
        }
    }

    for (SIZE_TYPE i = 0;  i < bases.size();  ++i) {
        x_Add(*bases[i].second,
              TPriority(ePriority_MaxUser - initial_num_bases - i),
              sm_BaseRegNamePrefix + bases[i].first);
    }

    return !bases.empty();
}


bool CCompoundRWRegistry::x_Empty(TFlags flags) const
{
    return m_AllRegistries->Empty(flags);
}


bool CCompoundRWRegistry::x_Modified(TFlags flags) const
{
    return m_AllRegistries->Modified(flags);
}


void CCompoundRWRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    if (modified) {
        m_MainRegistry->SetModifiedFlag(modified, flags);
    } else {
        // CCompoundRegistry only permits clearing...
        m_AllRegistries->SetModifiedFlag(modified, flags);
    }
}


const string& CCompoundRWRegistry::x_Get(const string& section,
                                         const string& name,
                                         TFlags flags) const
{
    TClearedEntries::const_iterator it
        = m_ClearedEntries.find(s_FlatKey(section, name));
    if (it != m_ClearedEntries.end()) {
        flags &= ~it->second;
        if ( !(flags & ~fJustCore) ) {
            return kEmptyStr;
        }
    }
    return m_AllRegistries->Get(section, name, flags);
}


bool CCompoundRWRegistry::x_HasEntry(const string& section, const string& name,
                                     TFlags flags) const
{
    TClearedEntries::const_iterator it
        = m_ClearedEntries.find(s_FlatKey(section, name));
    if (it != m_ClearedEntries.end()) {
        if ((flags & fCountCleared)  &&  (flags & it->second)) {
            return true;
        }
        flags &= ~it->second;
        if ( !(flags & ~fJustCore) ) {
            return false;
        }
    }
    return m_AllRegistries->HasEntry(section, name, flags);
}


const string& CCompoundRWRegistry::x_GetComment(const string& section,
                                                const string& name,
                                                TFlags flags) const
{
    return m_AllRegistries->GetComment(section, name, flags);
}


void CCompoundRWRegistry::x_Enumerate(const string& section,
                                      list<string>& entries,
                                      TFlags flags) const
{
    set<string> accum;
    REVERSE_ITERATE (CCompoundRegistry::TPriorityMap, it,
                     m_AllRegistries->m_PriorityMap) {
        if ((flags & fJustCore)  &&  (it->first < GetCoreCutoff())) {
            break;
        }

        list<string> tmp;
        it->second->EnumerateEntries(section, &tmp, flags & ~fJustCore);

        ITERATE (list<string>, it2, tmp) {
            // avoid reporting cleared entries
            TClearedEntries::const_iterator ceci
                = (flags & fCountCleared) ? m_ClearedEntries.end() 
                : m_ClearedEntries.find(s_FlatKey(section, *it2));
            if (ceci == m_ClearedEntries.end()
                ||  (flags & ~fJustCore & ~ceci->second)) {
                accum.insert(*it2);
            }
        }
    }
    ITERATE (set<string>, it, accum) {
        entries.push_back(*it);
    }
}


void CCompoundRWRegistry::x_ChildLockAction(FLockAction action)
{
    ((*m_AllRegistries).*action)();
}


void CCompoundRWRegistry::x_Clear(TFlags flags) // XXX - should this do more?
{
    m_MainRegistry->Clear(flags);

    ITERATE (set<string>, it, m_BaseRegNames) {
        Remove(*FindByName(sm_BaseRegNamePrefix + *it));
    }
    m_BaseRegNames.clear();
}


bool CCompoundRWRegistry::x_Set(const string& section, const string& name,
                                const string& value, TFlags flags,
                                const string& comment)
{
    TFlags flags2 = (flags & fPersistent) ? flags : (flags | fTransient);
    flags2 &= fLayerFlags;
    _TRACE('[' << section << ']' << name << " = " << value);
    if ((flags & fNoOverride)  &&  HasEntry(section, name, flags)) {
        return false;
    }
    if (value.empty()) {
        bool was_empty = Get(section, name, flags).empty();
        m_MainRegistry->Set(section, name, value, flags, comment);
        m_ClearedEntries[s_FlatKey(section, name)] |= flags2;
        return !was_empty;
    } else {
        TClearedEntries::iterator it
            = m_ClearedEntries.find(s_FlatKey(section, name));
        if (it != m_ClearedEntries.end()) {
            if ((it->second &= ~flags2) == 0) {
                m_ClearedEntries.erase(it);
            }
        }
    }
    return m_MainRegistry->Set(section, name, value, flags, comment);
}


bool CCompoundRWRegistry::x_SetComment(const string& comment,
                                       const string& section,
                                       const string& name, TFlags flags)
{
    return m_MainRegistry->SetComment(comment, section, name, flags);
}


IRWRegistry* CCompoundRWRegistry::x_Read(CNcbiIstream& in, TFlags flags,
                                         const string& path)
{
    TFlags lbr_flags = flags;
    if ((flags & fNoOverride) == 0  &&  !Empty(fPersistent) ) {
        lbr_flags |= fOverride;
    } else {
        lbr_flags &= ~fOverride;
    }
    IRWRegistry::x_Read(in, flags, path);
    LoadBaseRegistries(lbr_flags, 0, path);
    return NULL;
}


void CCompoundRWRegistry::x_Add(const IRegistry& reg, TPriority prio,
                                const string& name)
{
    m_AllRegistries->Add(reg, prio, name);
}


//////////////////////////////////////////////////////////////////////
//
// CRegistryException -- error reporting

const char* CRegistryException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eSection: return "eSection";
    case eEntry:   return "eEntry";
    case eValue:   return "eValue";
    case eErr:     return "eErr";
    default:       return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
