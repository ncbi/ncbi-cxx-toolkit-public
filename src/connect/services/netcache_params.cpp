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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   NetCache API parameters (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include "netcache_params.hpp"

#define MAX_NETCACHE_PASSWORD_LENGTH 64

BEGIN_NCBI_SCOPE

CNetCacheAPIParameters::CNetCacheAPIParameters(
        const CNetCacheAPIParameters* defaults) :
    m_DefinedParameters(0),
    m_Defaults(defaults)
{
}

void CNetCacheAPIParameters::LoadNamedParameters(
        const CNamedParameterList* optional)
{
    for (; optional; optional = optional->m_MoreParams)
        if (optional->Is(eNetCacheNPT_TTL))
            SetTTL(Get<unsigned>(optional));
        else if (optional->Is(eNetCacheNPT_Password))
            SetPassword(Get<std::string>(optional));
        else if (optional->Is(eNetCacheNPT_CachingMode))
            SetCachingMode(Get<CNetCacheAPI::ECachingMode>(optional));
        else if (optional->Is(eNetCacheNPT_MirroringMode))
            SetMirroringMode(Get<CNetCacheAPI::EMirroringMode>(optional));
        else if (optional->Is(eNetCacheNPT_ServerCheck))
            SetServerCheck(Get<ESwitch>(optional));
        else if (optional->Is(eNetCacheNPT_ServerCheckHint))
            SetServerCheckHint(Get<bool>(optional));
        else if (optional->Is(eNetCacheNPT_ServerToUse))
            SetServerToUse(Get<CNetServer::TInstance>(optional));
        else if (optional->Is(eNetCacheNPT_MaxBlobAge))
            SetMaxBlobAge(Get<unsigned>(optional));
        else if (optional->Is(eNetCacheNPT_ActualBlobAgePtr))
            SetActualBlobAgePtr(Get<unsigned*>(optional));
        else if (optional->Is(eNetCacheNPT_UseCompoundID))
            SetUseCompoundID(Get<bool>(optional));
}

void CNetCacheAPIParameters::SetTTL(unsigned blob_ttl)
{
    if ((m_TTL = blob_ttl) == 0)
        m_DefinedParameters &= ~(TDefinedParameters) eDP_TTL;
    else
        m_DefinedParameters |= eDP_TTL;
}

void CNetCacheAPIParameters::SetPassword(const string& password)
{
    if (password.empty()) {
        m_DefinedParameters &= ~(TDefinedParameters) eDP_Password;
        m_Password = kEmptyStr;
    } else {
        m_DefinedParameters |= eDP_Password;

        string encoded_password(NStr::PrintableString(password));

        if (encoded_password.length() > MAX_NETCACHE_PASSWORD_LENGTH) {
            NCBI_THROW(CNetCacheException,
                eAuthenticationError, "Password is too long");
        }

        m_Password.assign(" pass=\"");
        m_Password.append(encoded_password);
        m_Password.append("\"");
    }
}


#define NETCACHE_API_GET_PARAM_IMPL(param_name) \
    return m_Defaults == NULL || (m_DefinedParameters & eDP_##param_name) ? \
            m_##param_name : m_Defaults->Get##param_name()

unsigned CNetCacheAPIParameters::GetTTL() const
{
    NETCACHE_API_GET_PARAM_IMPL(TTL);
}

CNetCacheAPI::ECachingMode CNetCacheAPIParameters::GetCachingMode() const
{
    NETCACHE_API_GET_PARAM_IMPL(CachingMode);
}

CNetCacheAPI::EMirroringMode CNetCacheAPIParameters::GetMirroringMode() const
{
    NETCACHE_API_GET_PARAM_IMPL(MirroringMode);
}

#define NETCACHE_API_GET_PARAM_REVERSE_IMPL(param_name, target_var) \
    if (m_Defaults != NULL && m_Defaults->Get##param_name(target_var)) \
        return true; \
    if ((m_DefinedParameters & eDP_##param_name) == 0) \
        return false; \
    *target_var = m_##param_name; \
    return true

bool CNetCacheAPIParameters::GetServerCheck(ESwitch* server_check) const
{
    NETCACHE_API_GET_PARAM_REVERSE_IMPL(ServerCheck, server_check);
}

bool CNetCacheAPIParameters::GetServerCheckHint(bool* server_check_hint) const
{
    NETCACHE_API_GET_PARAM_REVERSE_IMPL(ServerCheckHint, server_check_hint);
}

std::string CNetCacheAPIParameters::GetPassword() const
{
    NETCACHE_API_GET_PARAM_IMPL(Password);
}

CNetServer CNetCacheAPIParameters::GetServerToUse() const
{
    NETCACHE_API_GET_PARAM_IMPL(ServerToUse);
}

unsigned CNetCacheAPIParameters::GetMaxBlobAge() const
{
    NETCACHE_API_GET_PARAM_IMPL(MaxBlobAge);
}

unsigned* CNetCacheAPIParameters::GetActualBlobAgePtr() const
{
    NETCACHE_API_GET_PARAM_IMPL(ActualBlobAgePtr);
}

bool CNetCacheAPIParameters::GetUseCompoundID() const
{
    NETCACHE_API_GET_PARAM_IMPL(UseCompoundID);
}

END_NCBI_SCOPE
