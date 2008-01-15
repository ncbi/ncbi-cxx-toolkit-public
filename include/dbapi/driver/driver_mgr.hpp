#ifndef DBAPI_DRIVER___DRIVER_MGR__HPP
#define DBAPI_DRIVER___DRIVER_MGR__HPP

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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver manager
 *
 */

#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/public.hpp>
#include <vector>
#include <map>


/** @addtogroup DbDrvMgr
 *
 * @{
 */

BEGIN_NCBI_SCOPE


class NCBI_DBAPIDRIVER_EXPORT C_DriverMgr
{
public:
    C_DriverMgr(unsigned int nof_drivers = 16);

    I_DriverContext* GetDriverContext(const string&       driver_name,
                                      string*             err_msg = 0,
                                      const map<string, string>* attr = 0);

    virtual ~C_DriverMgr();

public:
    /// Add path for the DLL lookup
    void AddDllSearchPath(const string& path);
    /// Delete all user-installed paths for the DLL lookup (for all resolvers)
    /// @param previous_paths
    ///  If non-NULL, store the prevously set search paths in this container
    void ResetDllSearchPath(vector<string>* previous_paths = NULL);

    /// Specify which standard locations should be used for the DLL lookup
    /// (for all resolvers). If standard locations are not set explicitelly
    /// using this method CDllResolver::fDefaultDllPath will be used by default.
    CDllResolver::TExtraDllPath
    SetDllStdSearchPath(CDllResolver::TExtraDllPath standard_paths);

    /// Get standard locations which should be used for the DLL lookup.
    /// @sa SetDllStdSearchPath
    CDllResolver::TExtraDllPath GetDllStdSearchPath(void) const;

    I_DriverContext* GetDriverContextFromTree(
        const string& driver_name,
        const TPluginManagerParamTree* const attr = NULL);

    I_DriverContext* GetDriverContextFromMap(
        const string& driver_name,
        const map<string, string>* attr = NULL);
};

/////////////////////////////////////////////////////////////////////////////
template<>
class NCBI_DBAPIDRIVER_EXPORT CDllResolver_Getter<I_DriverContext>
{
public:
    CPluginManager_DllResolver* operator()(void);
};

NCBI_DBAPIDRIVER_EXPORT
I_DriverContext*
Get_I_DriverContext(const string&              driver_name,
                    const map<string, string>* attr = NULL);

END_NCBI_SCOPE


/* @} */


#endif  /* DBAPI_DRIVER___DRIVER_MGR__HPP */
