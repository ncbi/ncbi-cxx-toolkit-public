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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Wrapper for the default FreeTDS DBAPI driver
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "../ctlib/ctlib_utils.hpp"

#include <corelib/plugin_manager_store.hpp>
// Need declaration for CDllResolver_Getter<I_DriverContext>
#include <dbapi/driver/driver_mgr.hpp>
// #include <dbapi/driver/drivers.hpp>

#define DBAPI_RegisterDriver_FTDSVER \
    NCBI_FTDS_VERSION_NAME(DBAPI_RegisterDriver_FTDS)

BEGIN_NCBI_SCOPE

extern void NCBI_DBAPIDRIVER_CTLIB_EXPORT DBAPI_RegisterDriver_FTDSVER(void);

BEGIN_SCOPE(NCBI_NS_FTDS_CTLIB)

class CDbapiCtlibCF_ftds : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds(void)
    : CDbapiCtlibCFBase("ftds")
    {
    }
};

END_SCOPE(NCBI_NS_FTDS_CTLIB)

extern "C"
NCBI_DLL_EXPORT void NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<NCBI_NS_FTDS_CTLIB::CDbapiCtlibCF_ftds>
        ::NCBI_EntryPointImpl(info_list, method);
}

NCBI_DLL_EXPORT void DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>(NCBI_EntryPoint_xdbapi_ftds);
    DBAPI_RegisterDriver_FTDSVER();
}

END_NCBI_SCOPE
