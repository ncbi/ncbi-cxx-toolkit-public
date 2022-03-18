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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Resolve WGS accessions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/readers/sra/impl/wgsresolver_impl.hpp>
#include <sra/error_codes.hpp>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSResolver
NCBI_DEFINE_ERR_SUBCODE_X(32);

BEGIN_NAMESPACE(objects);


NCBI_PARAM_DECL(int, WGS, DEBUG_RESOLVE);
NCBI_PARAM_DEF_EX(int, WGS, DEBUG_RESOLVE, CWGSResolver::eDebug_error,
                  eParam_NoThread, WGS_DEBUG_RESOLVE);

static inline int s_DebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, DEBUG_RESOLVE)> s_Value;
    return s_Value->Get();
}


bool CWGSResolver::s_DebugEnabled(EDebugLevel level)
{
    return s_DebugLevel() >= level;
}


CWGSResolver::CWGSResolver(void)
{
}


CWGSResolver::~CWGSResolver(void)
{
}


CRef<CWGSResolver>
CWGSResolver::CreateResolver(const CVDBMgr& mgr)
{
    CRef<CWGSResolver> ret;
    if ( !ret ) {
        // resolver from local VDB index file
        ret = CWGSResolver_VDB::CreateResolver(mgr);
    }
    //if ( !ret ) {
    //    // resolver from GenBank loader
    //    ret = CWGSResolver_DL::CreateResolver();
    //}
#ifdef WGS_RESOLVER_USE_ID2_CLIENT
    if ( !ret ) {
        // resolver from ID2
        ret = CWGSResolver_ID2::CreateResolver();
    }
#endif
    return ret;
}


void CWGSResolver::SetWGSPrefix(TGi /*gi*/,
                                const TWGSPrefixes& /*prefixes*/,
                                const string& /*prefix*/)
{
}


void CWGSResolver::SetWGSPrefix(const string& /*acc*/,
                                const TWGSPrefixes& /*prefixes*/,
                                const string& /*prefix*/)
{
}


void CWGSResolver::SetNonWGS(TGi /*gi*/,
                             const TWGSPrefixes& /*prefixes*/)
{
}


void CWGSResolver::SetNonWGS(const string& /*acc*/,
                             const TWGSPrefixes& /*prefixes*/)
{
}


bool CWGSResolver::Update(void)
{
    return false;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
