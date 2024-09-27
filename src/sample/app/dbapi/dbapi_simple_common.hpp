#ifndef SAMPLE__DBAPI_SIMPLE_COMMON__HPP
#define SAMPLE__DBAPI_SIMPLE_COMMON__HPP

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
 * Authors: David McElhany, Pavel Ivanov, Michael Kholodov
 *
 */

 /// @dbapi_simple_common.hpp
 /// CNcbiSample_Dbapi_Simple class definition, to be used by
 /// 'dbapi_simple' test and the NEW_PROJECT-class utilities.


#include <common/ncbi_sample_api.hpp>

 // "new" DBAPI
#include <dbapi/dbapi.hpp>

// "old" DBAPI
#include <dbapi/driver/dbapi_conn_factory.hpp>  // CTrivialConnValidator
#include <dbapi/driver/dbapi_svc_mapper.hpp>    // DBLB_INSTALL_DEFAULT
#include <dbapi/driver/drivers.hpp>             // DBAPI_RegisterDriver_FTDS
#include <dbapi/driver/exception.hpp>           // CDB_UserHandler


/** @addtogroup Sample
 *
 * @{
 */

USING_NCBI_SCOPE;


/// CNcbiSample_Dbapi_Simple -- class describing 'dbapi_simple' test business logic.
/// 
class CNcbiSample_Dbapi_Simple : public CNcbiSample
{
public:
    CNcbiSample_Dbapi_Simple() {};

    /// Sample description
    virtual string Description(void) { return "DBAPI Simple"; }

    /// Initialize app parameters with hard-coded values,
    /// they will be redefined in the main application.
    /// 
    virtual void Init(void)
    {
        m_Service     = "My_Service";
        m_DbName      = "My_DbName";
        m_User        = "My_User";
        m_Password    = "My_Password";
        m_UserString1 = "My_UserString1";
        m_UserString2 = "My_UserString2";
        m_UserString3 = "My_UserString3";

        m_LogFileName = "sample.log";
    }

    /// Sample's business logic
    virtual int Run(void)
    {
        // Connect to database
        SetupDb();

        // Demo several simple operations
        DemoStoredProc();
        DemoStaticSql();
        DemoParamerizedSql();
        DemoDynamicSql();

        // Show how SQL injection can happen
        DemoSqlInjection();

        return 0;
    }

    /// Cleanup
    void Exit(void)
    {
        CDriverManager& dm(CDriverManager::GetInstance());
        dm.DestroyDs(m_Ds);
        m_Logstream.flush();
    }

public:
    void SetupDb(void);
    void DemoStoredProc(void);
    void DemoParamerizedSql(void);
    void DemoStaticSql(void);
    void DemoDynamicSql(void);
    void DemoSqlInjection(void);
    void RetrieveData(IStatement* stmt);

public:
    // Run parameters
    string m_UserString1;
    string m_UserString2;
    string m_UserString3;
    string m_Service;
    string m_DbName;
    string m_User;
    string m_Password;
    string m_LogFileName;

private:
    // Other data
    IDataSource* m_Ds;
    IConnection* m_Conn;
    CNcbiOfstream  m_Logstream;
    CDB_UserHandler_Stream  m_ExHandler;
};


/* @} */

#endif // SAMPLE__DBAPI_SIMPLE_COMMON__HPP
