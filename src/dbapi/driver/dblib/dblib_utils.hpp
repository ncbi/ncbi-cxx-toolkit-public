#ifndef DBAPI_DRIVER_DBLIB___DBAPI_DRIVER_DBLIB_UTILS__HPP
#define DBAPI_DRIVER_DBLIB___DBAPI_DRIVER_DBLIB_UTILS__HPP

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

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>

#ifdef MS_DBLIB_IN_USE
#    define GetDBLExceptionStorage  GetMSDBLExceptionStorage
#endif // MS_DBLIB_IN_USE

#ifdef FTDS_IN_USE
#    define GetDBLExceptionStorage  GetTDSExceptionStorage
#endif // FTDS_IN_USE

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
#if defined(MS_DBLIB_IN_USE)

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_MSDBLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

#elif defined(FTDS_IN_USE)

// Uncomment a line below if you want to simulate a previous ftds driver logic.
// #define FTDS_LOGIC

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_FTDS_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds63(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

#else

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_DBLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

#endif


/////////////////////////////////////////////////////////////////////////////
// Singleton

CDBExceptionStorage& GetDBLExceptionStorage(void);


END_NCBI_SCOPE


#endif // DBAPI_DRIVER_DBLIB___DBAPI_DRIVER_DBLIB_UTILS__HPP


