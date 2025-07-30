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
 * Authors:  Aaron Ucko
 *
 * File Description:
 *   Classes to support using environment variables as a backend for
 *   the registry framework.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/env_reg.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_mask.hpp>
#include <corelib/error_codes.hpp>
#include <set>


#define NCBI_USE_ERRCODE_X   Corelib_Env


BEGIN_NCBI_SCOPE


//#define UPPER_CASE_ONLY


CEnvironmentRegistry::CEnvironmentRegistry(TFlags flags)
    : m_Env(new CNcbiEnvironment, eTakeOwnership),
      m_Modified(false), m_Flags(flags)
{
    AddMapper(*new CNcbiEnvRegMapper);
}


CEnvironmentRegistry::CEnvironmentRegistry(CNcbiEnvironment& env,
                                           EOwnership own, TFlags flags)
    : m_Env(&env, own), m_Modified(false), m_Flags(flags)
{
    AddMapper(*new CNcbiEnvRegMapper);
}


CEnvironmentRegistry::~CEnvironmentRegistry()
{
}


void CEnvironmentRegistry::AddMapper(const IEnvRegMapper& mapper,
                                     TPriority prio)
{
    CRegistryWriteGuard LOCK(*this);
    m_PriorityMap.insert(TPriorityMap::value_type
                     (prio, CConstRef<IEnvRegMapper>(&mapper)));
}


void CEnvironmentRegistry::RemoveMapper(const IEnvRegMapper& mapper)
{
    CRegistryWriteGuard LOCK(*this);
    NON_CONST_ITERATE (TPriorityMap, it, m_PriorityMap) {
        if (it->second == &mapper) {
            m_PriorityMap.erase(it);
            return; // mappers should be unique
        }
    }
    // already returned if found...
    NCBI_THROW2(CRegistryException, eErr,
                "CEnvironmentRegistry::RemoveMapper:"
                " unknown mapper (already removed?)", 0);
}

bool CEnvironmentRegistry::x_Empty(TFlags /*flags*/) const
{
    // return (flags & fTransient) ? m_PriorityMap.empty() : true;
    list<string> l;
    string       section, name;
    ITERATE (TPriorityMap, mapper, m_PriorityMap) {
        m_Env->Enumerate(l, mapper->second->GetPrefix());
        ITERATE (list<string>, it, l) {
            if (mapper->second->EnvToReg(*it, section, name)) {
                return false;
            }
        }
    }
    return true;
}


bool CEnvironmentRegistry::x_Modified(TFlags flags) const
{
    return (flags & fTransient) ? m_Modified : false;
}


void CEnvironmentRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    if (flags & fTransient) {
        m_Modified = modified;
    }
}


const string& CEnvironmentRegistry::x_Get(const string& section,
                                          const string& name,
                                          TFlags flags) const
{
    bool found;
    return x_Get(section, name, flags, found);
}


const string& CEnvironmentRegistry::x_Get(const string& section,
                                          const string& name,
                                          TFlags flags,
                                          bool& found) const
{
    if ((flags & fTPFlags) == fPersistent) {
        return kEmptyStr;
    }
    REVERSE_ITERATE (TPriorityMap, it, m_PriorityMap) {
        string        var_name = it->second->RegToEnv(section, name);
        const string* resultp  = &m_Env->Get(var_name, &found);
        if ((m_Flags & fCaseFlags) == 0  &&  !found) {
            // try capitalizing the name
            resultp = &m_Env->Get(NStr::ToUpper(var_name), &found);
        }
        if (found) {
            return *resultp;
        }
    }
    return kEmptyStr;
}


bool CEnvironmentRegistry::x_HasEntry(const string& section,
                                      const string& name,
                                      TFlags flags) const
{
    if (name.empty()) {
        return x_HasSection(section, flags);
    }
    bool found = false;
    x_Get(section, name, flags, found);
    return found;
}


bool CEnvironmentRegistry::x_HasSection(const string& section, TFlags flags)
    const
{
    static const char kLowercaseASCII[] = "abcdefghijklmnopqrstuvwxyz";
    static const char kPattern[] = ".*";
    NStr::ECase use_case
        = (flags & fSectionCase) == 0 ? NStr::eNocase : NStr::eCase;
    string section2, name;
    for (const auto& mapper : m_PriorityMap) {
        CMaskFileName masks;
        for (int i = sizeof(kPattern) - 1;  i >= 0;  --i) {
            string mask = mapper.second->RegToEnv(section, kPattern + i);
            masks.Add(mask);
            if (mask.find_first_of(kLowercaseASCII) != NPOS) {
                NStr::ToUpper(mask);
                masks.Add(mask);
            }
        }
        list<string> l;
        m_Env->Enumerate(l, mapper.second->GetPrefix());
        for (const auto& env : l) {
            if (masks.Match(env)
                &&  mapper.second->EnvToReg(env, section2, name)
                &&  NStr::Equal(section, section2, use_case)) {
                return true;
            }
        }
    }
    return false;
}


const string& CEnvironmentRegistry::x_GetComment(const string&, const string&,
                                                 TFlags) const
{
    return kEmptyStr;
}


void CEnvironmentRegistry::x_Enumerate(const string& section,
                                       list<string>& entries,
                                       TFlags flags) const
{
    // Environment does not provide comments, so if we came for in-section 
    // comments, we can just return doing nothing
    if (flags & fInSectionComments) {
        return;
    }
    if ( !(flags & fTransient) ) {
        return;
    }

    NStr::ECase use_case = (flags & fSectionCase) == 0 ? NStr::eNocase : NStr::eCase;
    typedef set<string, PNocase_Conditional> TEntrySet;

    list<string> l;
    TEntrySet    entry_set(use_case);
    string       parsed_section, parsed_name;

    ITERATE (TPriorityMap, mapper, m_PriorityMap) {
        m_Env->Enumerate(l, mapper->second->GetPrefix());
        ITERATE (list<string>, it, l) {
            if (mapper->second->EnvToReg(*it, parsed_section, parsed_name)) {
                if (section.empty()) {
                    entry_set.insert(parsed_section);
                } else if (NStr::Equal(section, parsed_section, use_case)) {
                    entry_set.insert(parsed_name);
                }
            }
        }
    }
    ITERATE (TEntrySet, it, entry_set) {
        entries.push_back(*it);
    }
}


void CEnvironmentRegistry::x_ChildLockAction(FLockAction)
{
}


void CEnvironmentRegistry::x_Clear(TFlags flags)
{
    if (flags & fTransient) {
        // m_Mappers.clear();
    }
}


bool CEnvironmentRegistry::x_Set(const string& section, const string& name,
                                 const string& value, TFlags flags,
                                 const string& /*comment*/)
{
    REVERSE_ITERATE (TPriorityMap, it,
                 const_cast<const TPriorityMap&>(m_PriorityMap)) {
        string var_name = it->second->RegToEnv(section, name);
        if ( !var_name.empty() ) {
            string cap_name = var_name;
            NStr::ToUpper(cap_name);
            string old_value = m_Env->Get(var_name);
            if ((m_Flags & fCaseFlags) == 0  &&  old_value.empty()) {
                old_value = m_Env->Get(cap_name);
            }
            if (MaybeSet(old_value, value, flags)) {
                m_Env->Set(var_name, value);
                // m_Env->Set(cap_name, value);
                return true;
            }
            return false;
        }
    }

    ERR_POST_X(1, Warning << "CEnvironmentRegistry::x_Set: "
                  "no mapping defined for [" << section << ']' << name);
    return false;
}


bool CEnvironmentRegistry::x_Unset(const string& section, const string& name,
                                   TFlags /*flags*/)
{
    bool result = false;
    ITERATE (TPriorityMap, it, m_PriorityMap) {
        string var_name = it->second->RegToEnv(section, name);
        bool   found;
        if (var_name.empty()) {
            continue;
        }
        m_Env->Get(var_name, &found);
        if (found) {
            result = true;
            m_Env->Unset(var_name);
        }
        if ((m_Flags & fCaseFlags) == 0) {
            string cap_name = var_name;
            NStr::ToUpper(cap_name);
            m_Env->Get(cap_name, &found);
            if (found) {
                result = true;
                m_Env->Unset(cap_name);
            }
        }
    }
    return result;
}

bool CEnvironmentRegistry::x_SetComment(const string&, const string&,
                                        const string&, TFlags)
{
    ERR_POST_X(2, Warning
                  << "CEnvironmentRegistry::x_SetComment: unsupported operation");
    return false;
}


CSimpleEnvRegMapper::CSimpleEnvRegMapper(const string& section,
                                         const string& prefix,
                                         const string& suffix)
    : m_Section(section), m_Prefix(prefix), m_Suffix(suffix)
{
}


string CSimpleEnvRegMapper::RegToEnv(const string& section, const string& name)
    const
{
    return (section == m_Section ? (m_Prefix + name + m_Suffix) : kEmptyStr);
}


bool CSimpleEnvRegMapper::EnvToReg(const string& env, string& section,
                                   string& name) const
{
    SIZE_TYPE plen = m_Prefix.length();
    SIZE_TYPE tlen = plen + m_Suffix.length();
    if (env.size() > tlen  &&  NStr::StartsWith(env, m_Prefix, NStr::eNocase)
        &&  NStr::EndsWith(name, m_Suffix, NStr::eNocase)) {
        section = m_Section;
        name    = env.substr(plen, env.length() - tlen);
        return true;
    }
    return false;
}


string CSimpleEnvRegMapper::GetPrefix(void) const
{
    return m_Prefix;
}


static const char*  kPrefix = "NCBI_CONFIG_";
static const size_t kPrefixLen = 12;


string CNcbiEnvRegMapper::RegToEnv(const string& section, const string& name)
    const
{
    string result;
    result.assign(kPrefix, kPrefixLen);
    if (NStr::StartsWith(name, ".")) {
        result += name.substr(1) + "__" + section;
    } else {
        result += "_" + section + "__" + name;
    }
#ifdef UPPER_CASE_ONLY
    NStr::ToUpper(result);
#endif
    if (result.find_first_of(".-/ ") != NPOS) {
        NStr::ReplaceInPlace(result, ".", "_DOT_");
        NStr::ReplaceInPlace(result, "-", "_HYPHEN_");
        NStr::ReplaceInPlace(result, "/", "_SLASH_");
        NStr::ReplaceInPlace(result, " ", "_SPACE_");
    }
    return result;
}


inline
static char s_IdentifySubstitution(const CTempString& s) {
    switch (s[0]) {
    case 'D':
        if (s.size() == 3  &&  s == "DOT") {
            return '.';
        }
        break;
    case 'H':
        if (s.size() == 6  &&  s == "HYPHEN") {
            return '-';
        }
        break;
    case 'S':
        if (s.size() == 5) {
            if (s == "SLASH") {
                return '/';
            } else if (s == "SPACE") {
                return ' ';
            }
        }
        break;
    default:
        break;
    }
    return '\0';
}


bool CNcbiEnvRegMapper::EnvToReg(const string& env_in, string& section,
                                 string& name) const
{
    if (env_in.size() <= kPrefixLen  ||  !NStr::StartsWith(env_in, kPrefix) ) {
        return false;
    }
#ifdef UPPER_CASE_ONLY
    for ( auto c : env_in ) {
        if ( islower(c) ) {
            return false;
        }
    }
#endif
    vector<CTempString> v;
    NStr::Split(env_in, "_", v);
    string env;
    env.reserve(env_in.size());
    for (const auto& it : v) {
        SIZE_TYPE l = env.size();
        char c = '\0';
        if (&it != &v.back()  &&  !env.empty()  &&  env[l - 1] == '_'
            &&  !it.empty()) {
            c = s_IdentifySubstitution(it);
        }
        if (c == '\0') {
            env += it;
            if (&it != &v.back()) {
                env += '_';
            }
        } else {
            env[l - 1] = c;
        }
    }
    // Make an offset until the first symbol that could be section name
    // (alphanumeric character)
    SIZE_TYPE section_start_pos = kPrefixLen;
    for (  ; section_start_pos < env.size(); section_start_pos++) {
        if (isalnum(env[section_start_pos])) break;
    }
    SIZE_TYPE uu_pos = env.find("__", section_start_pos + 1);
    if (uu_pos == NPOS  ||  uu_pos == env.size() - 2) {
        return false;
    }
    /* Parse section and entry names from the variable */
    if (env[kPrefixLen] == '_') { // regular entry
        section = env.substr(kPrefixLen + 1, uu_pos - kPrefixLen - 1);
        name    = env.substr(uu_pos + 2);
    } else {
        name    = env.substr(kPrefixLen - 1, uu_pos - kPrefixLen + 1);
        _ASSERT(name[0] == '_');
        name[0] = '.';
        section = env.substr(uu_pos + 2);
    }
    if (!IRegistry::IsNameSection(section, IRegistry::fInternalSpaces)) {
        ERR_POST(Info << "Invalid registry section name in environment "
                            "variable " << env);
    }
    if (!IRegistry::IsNameEntry(name, IRegistry::fInternalSpaces)) {
        ERR_POST(Info << "Invalid registry entry name in environment "
                            "variable " << env);
    }
    return true;
}


string CNcbiEnvRegMapper::GetPrefix(void) const
{
    return kPrefix;
}


END_NCBI_SCOPE
