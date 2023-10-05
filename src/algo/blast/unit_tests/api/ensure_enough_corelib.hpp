#ifndef ENSURE_ENOUGH_CORELIB__HPP
#define ENSURE_ENOUGH_CORELIB__HPP

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
 */

/// @file ensure_enough_corelib.hpp
/// Ensure direct dependencies on enough of the core xncbi library to
/// satisfy shared libraries that depend on it.  (In particular, affected
/// tests otherwise fail to start when built with Apple Developer Tools 15
/// using mostly static libraries.)

#if defined(NCBI_APP_BUILT_AS)  &&  !defined(BLAST_SECONDARY_SOURCE)

#include <corelib/ncbi_cookies.hpp>
#include <corelib/ncbi_message.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <corelib/impl/rwstreambuf.hpp>

#include <objmgr/data_loader_factory.hpp>

BEGIN_NCBI_SCOPE

void BLAST_Test_EnsureEnoughCorelib(void)
{
    class CFakeDataLoaderFactory : public objects::CDataLoaderFactory
    {
    public:
        CFakeDataLoaderFactory() : CDataLoaderFactory("foo") { }
    protected:
        objects::CDataLoader* CreateAndRegister(objects::CObjectManager&,
                                                const TPluginManagerParamTree*)
            const
            { return nullptr; }
    };

    CConfig empty_config((CMemoryRegistry()));
    CEndpointKey fake_endpoint("1.2.3.4:5", 0);
    CFakeDataLoaderFactory fake_dl_factory;
    CHttpCookie empty_cookie;
    CMessage_Basic empty_message(kEmptyStr, eDiag_Trace);
    CPluginManager_DllResolver dummy_resolver;
    CPluginManagerGetterImpl::GetMutex();
    CRWStreambuf detached_buf;
}

END_NCBI_SCOPE

#endif /* NCBI_APP_BUILT_AS && !BLAST_SECONDARY_SOURCE */

#endif  /* ENSURE_ENOUGH_CORELIB__HPP */
