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
#include <string>
#include <vector>
#include <map>


/** @addtogroup DbDrvMgr
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_DBAPIDRIVER_EXPORT C_DriverMgr : public I_DriverMgr
{
public:
    C_DriverMgr(unsigned int nof_drivers = 16);

    FDBAPI_CreateContext GetDriver(const string& driver_name,
                                   string*       err_msg = 0);

    virtual void RegisterDriver(const string&        driver_name,
                                FDBAPI_CreateContext driver_ctx_func);

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
                    const map<string, string>* attr);

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2006/04/06 21:27:27  ssikorsk
 * + #include <string>
 * + #include <vector>
 * + #include <map>
 *
 * Revision 1.20  2006/03/09 16:52:04  ssikorsk
 * Added methods ResetDllSearchPath,  SetDllStdSearchPath,
 * GetDllStdSearchPath to the C_DriverMgr class.
 *
 * Revision 1.19  2005/06/07 16:19:23  ssikorsk
 * Moved a definition of CDllResolver_Getter<I_DriverContext>::operator()(void) into cpp (again).
 *
 * Revision 1.18  2005/06/06 22:23:45  vakatov
 * Rollback previous revision as it caused linking errors in static MSVC++
 *
 * Revision 1.16  2005/03/08 17:13:10  ssikorsk
 * Allow to add a driver search path for the driver manager
 *
 * Revision 1.15  2005/03/03 19:06:49  ssikorsk
 * Do not unload database drivers with the PluginManager
 *
 * Revision 1.14  2005/03/01 16:24:44  ssikorsk
 * Restored the "GetDriver" method
 *
 * Revision 1.13  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.12  2004/12/20 16:20:47  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.11  2003/04/11 17:46:01  siyan
 * Added doxygen support
 *
 * Revision 1.10  2002/12/26 19:29:12  dicuccio
 * Added Win32 export specifier for base DBAPI library
 *
 * Revision 1.9  2002/04/23 16:45:28  soussov
 * GetDriverContext added
 *
 * Revision 1.8  2002/04/12 18:44:34  soussov
 * makes driver_mgr working properly in mt environment
 *
 * Revision 1.7  2002/04/09 22:18:13  vakatov
 * Moved code from the header
 *
 * Revision 1.6  2002/04/09 20:05:57  vakatov
 * Identation
 *
 * Revision 1.5  2002/04/04 23:48:44  soussov
 * return of error message from dlopen added
 *
 * Revision 1.4  2002/01/23 21:14:06  soussov
 * replaces map with array
 *
 * Revision 1.3  2002/01/20 07:26:57  vakatov
 * Fool-proofed to compile func_ptr/void_ptr type casts on all compilers.
 * Added virtual destructor.
 * Temporarily get rid of the MT-safety features -- they need to be
 * implemented here more carefully (in the nearest future).
 *
 * Revision 1.2  2002/01/17 23:18:58  soussov
 * makes gcc happy
 *
 * Revision 1.1  2002/01/17 22:05:56  soussov
 * adds driver manager
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___DRIVER_MGR__HPP */
