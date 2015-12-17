#ifndef DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP
#define DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP

/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * File Description:  Small utility classes common to the ctlib driver.
 *
 */

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include "../dbapi_driver_exception_storage.hpp"

#include <corelib/plugin_manager_impl.hpp>

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
BEGIN_SCOPE(NCBI_NS_FTDS_CTLIB)
#endif

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CDbapiCtlibCFBase
    : public CSimpleClassFactoryImpl<I_DriverContext, CTLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTLibContext> TParent;

public:
    CDbapiCtlibCFBase(const string& driver_name);
    ~CDbapiCtlibCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

#ifdef FTDS_IN_USE
END_SCOPE(NCBI_NS_FTDS_CTLIB)
#endif

/////////////////////////////////////////////////////////////////////////////
extern "C"
{

#if defined(FTDS_IN_USE)

#  define NCBI_EntryPoint_xdbapi_ftdsVER \
    NCBI_FTDS_VERSION_NAME(NCBI_EntryPoint_xdbapi_ftds)

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftdsVER(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

#else // defined(FTDS_IN_USE)

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

#endif // defined(FTDS_IN_USE)

} // extern C


/////////////////////////////////////////////////////////////////////////////
// Singleton

impl::CDBExceptionStorage& GetCTLExceptionStorage(void);


END_NCBI_SCOPE


#endif // DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP

