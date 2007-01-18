#ifndef DBAPI_SAMPLE_BASE_HPP
#define DBAPI_SAMPLE_BASE_HPP

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
* Author:  Denis Vakatov
*
* File Description:
*    CDbapiSampleApp_Base
*      A base class for various DBAPI tests and sample applications.
*
*/


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <dbapi/driver/interfaces.hpp>



BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// Forward declaration
//

extern const char* file_name[];
class CDB_Connection;



/////////////////////////////////////////////////////////////////////////////
//  CDbapiSampleApp::
//

class CDbapiSampleApp : public CNcbiApplication
{
public:
    enum EUseSampleDatabase {eUseSampleDatabase, eDoNotUseSampleDatabase};

public:
    CDbapiSampleApp(EUseSampleDatabase sd = eUseSampleDatabase);
    virtual ~CDbapiSampleApp(void);

protected:
    /// Override these 3 to suit your test's needs
    virtual void InitSample(CArgDescriptions& arg_desc);
    virtual int  RunSample(void) = 0;
    virtual void ExitSample(void);

protected:
    // Hi-level functions

    /// Get connection created using server, username and password
    /// specified in the application command line
    CDB_Connection& GetConnection(void);
    /// Delete table if it exists
    ///ShowResults is printing resuts on screen
    void ShowResults (const string& query);

    void DeleteTable(const string& table_name);
    /// function CreateTable is creating table in the database
    void CreateTable (const string& table_name);

protected:
    // Less than Hi-level functions

    /// Get the driver context (for the driver specified in the command line)
    I_DriverContext& GetDriverContext(void);

    /// Create new connection using server, username and password
    /// specified in the application command line
    CDB_Connection*
    CreateConnection(IConnValidator*                  validator = NULL,
                     I_DriverContext::TConnectionMode mode = I_DriverContext::fBcpIn,
                     bool                             reusable  = false,
                     const string&                    pool_name = kEmptyStr);

    /// Delete tables which are lost after previous tests.
    void DeleteLostTables(void);

    /// Return an unique identificatot which contains host name, process id
    /// and current date.
    const string& GetTableUID(void) const
    {
        return m_TableUID;
    }

protected:
    enum EServerType {
        eUnknown,   //< Server type is not known
        eSybase,    //< Sybase server
        eMsSql      //< Microsoft SQL server
    };
    typedef map<string, string> TDatabaseParameters;

protected:
    /// Return current driver name
    const string& GetDriverName(void) const
    {
        _ASSERT(!m_DriverName.empty());
        return m_DriverName;
    }
    /// Set driver name
    void SetDriverName(const string& name)
    {
        _ASSERT(!name.empty());
        m_DriverName = name;
    }
    /// Return current server name
    const string& GetServerName(void) const
    {
        _ASSERT(!m_ServerName.empty());
        return m_ServerName;
    }
    /// Return current server type
    EServerType GetServerType(void) const;
    /// Return current user name
    const string& GetUserName(void) const
    {
        _ASSERT(!m_UserName.empty());
        return m_UserName;
    }
    /// Return current password
    const string& GetPassword(void) const
    {
        _ASSERT(!m_Password.empty());
        return m_Password;
    }

    /// Set database connection parameter.
    /// NOTE: Database parameters will affect a connection only is you set them
    /// before actual connect.
    void SetDatabaseParameter(const string& name, const string& value)
    {
        _ASSERT(!name.empty());
        m_DatabaseParameters[name] = value;
    }
    /// Return database conection parameters
    const TDatabaseParameters& GetDatabaseParameters(void) const
    {
        return m_DatabaseParameters;
    }

    bool UseSvcMapper(void) const
    {
        return m_UseSvcMapper;
    }

private:
    virtual void Init();
    virtual int  Run();
    virtual void Exit();

private:
    auto_ptr<I_DriverContext> m_DriverContext;
    auto_ptr<CDB_Connection> m_Connection;
    string m_TableUID;
    EUseSampleDatabase m_UseSampleDatabase;

    string m_DriverName;
    string m_ServerName;
    string m_UserName;
    string m_Password;
    string m_TDSVersion;
    bool   m_UseSvcMapper;

    TDatabaseParameters  m_DatabaseParameters;
};


class CDbapiSampleErrHandler : public CDB_UserHandler
{
public:
    // c-tor
    CDbapiSampleErrHandler(void);
    // d-tor
    virtual ~CDbapiSampleErrHandler(void);

public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);
};


END_NCBI_SCOPE


#endif // DBAPI_SAMPLE_BASE_HPP
