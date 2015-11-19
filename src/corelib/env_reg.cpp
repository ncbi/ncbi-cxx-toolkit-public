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
#include <corelib/error_codes.hpp>
#include <set>


#define NCBI_USE_ERRCODE_X   Corelib_Env


BEGIN_NCBI_SCOPE


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
    m_PriorityMap.insert(TPriorityMap::value_type
                     (prio, CConstRef<IEnvRegMapper>(&mapper)));
}


void CEnvironmentRegistry::RemoveMapper(const IEnvRegMapper& mapper)
{
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
            resultp = &m_Env->Get(NStr::ToUpper(var_name));
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
    bool found = false;
    x_Get(section, name, flags, found);
    return found;
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
    if ( !(flags & fTransient) ) {
        return;
    }

    typedef set<string, PNocase> TEntrySet;

    list<string> l;
    TEntrySet    entry_set;
    string       parsed_section, parsed_name;

    ITERATE (TPriorityMap, mapper, m_PriorityMap) {
        m_Env->Enumerate(l, mapper->second->GetPrefix());
        ITERATE (list<string>, it, l) {
            if (mapper->second->EnvToReg(*it, parsed_section, parsed_name)) {
                if (section.empty()) {
                    entry_set.insert(parsed_section);
                } else if (section == parsed_section) {
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
                                   TFlags flags)
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


const char* CNcbiEnvRegMapper::sm_Prefix = "NCBI_CONFIG_";


string CNcbiEnvRegMapper::RegToEnv(const string& section, const string& name)
    const
{
    string result(sm_Prefix);
    if (NStr::StartsWith(name, ".")) {
        result += name.substr(1) + "__" + section;
    } else {
        result += "_" + section + "__" + name;
    }
    return NStr::Replace(result, ".", "_DOT_");
}


bool CNcbiEnvRegMapper::EnvToReg(const string& env, string& section,
                                 string& name) const
{
    static const SIZE_TYPE kPfxLen = strlen(sm_Prefix);
    if (env.size() <= kPfxLen  ||  !NStr::StartsWith(env, sm_Prefix) ) {
        return false;
    }
    SIZE_TYPE uu_pos = env.find("__", kPfxLen + 1);
    if (uu_pos == NPOS  ||  uu_pos == env.size() - 2) {
        return false;
    }
    if (env[kPfxLen] == '_') { // regular entry
        section = env.substr(kPfxLen + 1, uu_pos - kPfxLen - 1);
        name    = env.substr(uu_pos + 2);
    } else {
        name    = env.substr(kPfxLen - 1, uu_pos - kPfxLen + 1);
        _ASSERT(name[0] == '_');
        name[0] = '.';
        section = env.substr(uu_pos + 2);
    }
    NStr::ReplaceInPlace(section, "_DOT_", ".");
    NStr::ReplaceInPlace(name,    "_DOT_", ".");
    return true;
}


string CNcbiEnvRegMapper::GetPrefix(void) const
{
    return sm_Prefix;
}


END_NCBI_SCOPE
