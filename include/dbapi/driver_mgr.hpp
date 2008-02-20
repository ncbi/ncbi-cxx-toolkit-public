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
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
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

template <typename T> class CSafeStaticPtr;

class NCBI_DBAPI_EXPORT CDriverManager : public C_DriverMgr
{
    friend class CSafeStaticPtr<CDriverManager>;

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

    IDataSource* MakeDs(const CDBConnParams& params);

    // Destroy datasource object
    void DestroyDs(const string& driver_name);
    void DestroyDs(const IDataSource* ds);

    // Set maximum number of connections in application
    void SetMaxConnect(int max_connect) {
        CDbapiConnMgr::Instance().SetMaxConnect(max_connect);
    }

    // Get maximum number of connections in application
    int GetMaxConnect(void) {
        return CDbapiConnMgr::Instance().GetMaxConnect();
    }

protected:
    typedef multimap<string, class IDataSource*> TDsContainer;

    // Prohibit explicit construction and destruction
    CDriverManager();
    virtual ~CDriverManager();

    // Put the new data source into the internal list with
    // corresponding driver name, return previous, if already exists
    class IDataSource* RegisterDs(const string& driver_name,
                  class I_DriverContext* ctx);

    mutable CMutex  m_Mutex;
    TDsContainer    m_ds_list;
};

END_NCBI_SCOPE


/* @} */

#endif  /* DBAPI___DBAPI__HPP */
