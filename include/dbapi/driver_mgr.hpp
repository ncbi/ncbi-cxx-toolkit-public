#ifndef DBAPI___DRIVER_MGR__HPP
#define DBAPI___DRIVER_MGR__HPP

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
 * Author:  Michael Kholodov, Denis Vakatov
 *
 * File Description:  Driver Manager definition
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/plugin_manager.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <map>


/** @addtogroup DbDrvMgr
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDriverManager::
//
//  Static class for registering drivers and getting the datasource
//

// Forward declaration
class IDataSource;

class NCBI_DBAPI_EXPORT CDriverManager : public C_DriverMgr
{
    template<class T> friend class CSafeStaticPtr;

public:
    // Get a single instance of CDriverManager
    static CDriverManager& GetInstance();

    // Remove instance of CDriverManager
    // DEPRECAETD. Instance will be removed automatically.
    static void RemoveInstance();

    // Create datasource object
    IDataSource* CreateDs(const string& driver_name,
                const map<string, string> *attr = 0);

    IDataSource* CreateDsFrom(const string& drivers,
                    const IRegistry* reg = 0);

    // Destroy datasource object
    void DestroyDs(const string& driver_name);

protected:
    // Prohibit explicit construction and destruction
    CDriverManager();
    virtual ~CDriverManager();

    // Put the new data source into the internal list with
    // corresponding driver name, return previous, if already exists
    class IDataSource* RegisterDs(const string& driver_name,
                  class I_DriverContext* ctx);

    map<string, class IDataSource*> m_ds_list;

private:
    static IDataSource* CreateDs(I_DriverContext* ctx);
    // This function will just call the delete operator.
    static void DeleteDs(const IDataSource* const ds);

// DEPRECATED. Will be removed in next version.
private:
    static CDriverManager* sm_Instance;
    DECLARE_CLASS_STATIC_MUTEX(sm_Mutex);
};

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/04 13:03:02  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.10  2005/03/08 17:13:10  ssikorsk
 * Allow to add a driver search path for the driver manager
 *
 * Revision 1.9  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.8  2004/12/20 16:45:27  ucko
 * Accept any IRegistry rather than specifically requiring a CNcbiRegistry.
 *
 * Revision 1.7  2004/04/22 11:28:41  ivanov
 * Use pointer to driver manager instance instead auto_ptr
 *
 * Revision 1.6  2003/12/17 14:57:00  ivanov
 * Changed type of the Driver Manager instance to auto_ptr.
 * Added RemoveInstance() method.
 *
 * Revision 1.5  2003/12/15 20:05:41  ivanov
 * Added export specifier for building DLLs in MS Windows.
 *
 * Revision 1.4  2003/04/11 17:45:57  siyan
 * Added doxygen support
 *
 * Revision 1.3  2003/02/10 17:21:09  kholodov
 * Added: DestroyDs() method
 *
 * Revision 1.2  2002/04/29 19:13:14  kholodov
 * Modified: using C_DriverMgr as parent class of CDriverManager
 *
 * Revision 1.1  2002/01/30 14:51:24  kholodov
 * User DBAPI implementation, first commit
 *
 * ===========================================================================
 */

#endif  /* DBAPI___DBAPI__HPP */
