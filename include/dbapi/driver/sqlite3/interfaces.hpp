#ifndef DBAPI_DRIVER_SQLITE3___INTERFACES__HPP
#define DBAPI_DRIVER_SQLITE3___INTERFACES__HPP

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
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/util/parameters.hpp>

#include <sqlite3.h>

BEGIN_NCBI_SCOPE


class CSL3Context;
class CSL3_Connection;
class CSL3_LangCmd;
class CSL3_RowResult;


/////////////////////////////////////////////////////////////////////////////
//
//  CSL3Context::
//

class NCBI_DBAPIDRIVER_SQLITE3_EXPORT CSL3Context : public impl::CDriverContext
{
    friend class CSL3_Connection;

public:
    CSL3Context(void);
    virtual ~CSL3Context(void);

public:
    virtual bool IsAbleTo(ECapability cpb) const;

protected:
    virtual impl::CConnection* MakeIConnection(const CDBConnParams& params);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSL3_Connection::
//

class NCBI_DBAPIDRIVER_SQLITE3_EXPORT CSL3_Connection : public impl::CConnection
{
public:
    sqlite3* GetSQLite3(void) const
    {
        _ASSERT(m_SQLite3);
        return m_SQLite3;
    }
    int Check(int rc);

protected:
    CSL3_Connection(CSL3Context&  cntx,
                    const CDBConnParams& params);
    virtual ~CSL3_Connection();

protected:
    virtual bool IsAlive();

    virtual CDB_LangCmd*     LangCmd(const string& lang_query);
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true);
    virtual CDB_RPCCmd*      RPC(const string& rpc_name);
    virtual CDB_BCPInCmd*    BCPIn(const string& table_name);
    virtual CDB_CursorCmd*   Cursor(const string& cursor_name,
                                    const string& query,
                                    unsigned int  batch_size = 1);


    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    virtual bool Refresh();
    virtual I_DriverContext::TConnectionMode ConnectMode() const;

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void);

    virtual void SetTimeout(size_t nof_secs);

private:
    sqlite3*    m_SQLite3;

    bool        m_IsOpen;

    friend class CSL3_LangCmd;
    friend class CSL3Context;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSL3_LangCmd::
//

class NCBI_DBAPIDRIVER_SQLITE3_EXPORT CSL3_LangCmd : public impl::CBaseCmd
{
    friend class CSL3_Connection;

public:
    sqlite3_stmt* x_GetSQLite3stmt(void) const
    {
        _ASSERT(m_SQLite3stmt);
        return m_SQLite3stmt;
    }
    CSL3_Connection& GetConnection(void)
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }
    const CSL3_Connection& GetConnection(void) const
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }
    int Check(int rc)
    {
        return GetConnection().Check(rc);
    }


protected:
    CSL3_LangCmd(CSL3_Connection& conn,
                 const string&    lang_query);
    virtual ~CSL3_LangCmd(void);

protected:
    /// Get meta-information about binded parameters. 
    virtual CDBParams& GetBindParams(void);

    virtual bool        Send(void);
    virtual bool        Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool        HasMoreResults(void) const;
    virtual bool        HasFailed(void) const;
    virtual int         RowCount(void) const;
    long long int       LastInsertId(void) const;
    static sqlite3_stmt* MakeSQLiteStmt(CSL3_Connection& conn, 
                                        const string& sql);

    string GetDbgInfo(void) const
    {
        return " " + GetConnection().GetExecCntxInfo();
    }

    /* Shouldn't be used till design changes.
    class CParamInfo : public impl::CDBBindedParams
    {
    public:
        CParamInfo(sqlite3_stmt* stmt, impl::CDB_Params& bindings);
        virtual ~CParamInfo(void);

    public:
        virtual unsigned int GetNum(void) const;

        virtual string GetName(
            const CDBParamVariant& param, 
            CDBParamVariant::ENameFormat format = 
                CDBParamVariant::eSQLServerName) const;
        virtual unsigned int GetIndex(const CDBParamVariant& param) const;

        virtual EDirection GetDirection(const CDBParamVariant& param) const;

    private:
        sqlite3_stmt* x_GetSQLite3stmt(void) const
        {
            _ASSERT(m_SQLite3stmt);
            return m_SQLite3stmt;
        }

    private:
        sqlite3_stmt*   m_SQLite3stmt;
    };
    */

private:
    bool x_AssignParams(void);
    bool x_AssignCmdParam(CDB_Object& param,
                          int         param_num);

    CSL3_Connection*     m_Connect;
    bool                 m_HasMoreResults;
    int                  m_RowCount;

    sqlite3_stmt*        m_SQLite3stmt;
    CSL3_RowResult*      m_Res;
    int                  m_RC;

    impl::CCachedRowInfo m_InParams;
};



class NCBI_DBAPIDRIVER_SQLITE3_EXPORT CSL3_BCPInCmd : public CSL3_LangCmd
{
    friend class CSL3_Connection;

protected:
    CSL3_BCPInCmd(CSL3_Connection& conn,
                  const string&    table_name);
    virtual ~CSL3_BCPInCmd(void);

protected:
    // BCP stuff ...
    virtual bool CommitBCPTrans(void);
    virtual bool EndBCP(void);

private:
    void ExecuteSQL(const string& sql);
    static string MakePlaceholders(size_t num);
    static size_t GetTableColumnNum(
            CSL3_Connection& conn, 
            const string& table_name);
    static string MakeInsertStmt(
            CSL3_Connection& conn, 
            const string& table_name);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CSL3_RowResult::
//

class NCBI_DBAPIDRIVER_SQLITE3_EXPORT CSL3_RowResult : public impl::CResult
{
    friend class CSL3_LangCmd;

protected:
    CSL3_RowResult(CSL3_LangCmd* cmd, bool fetch_done);
    virtual ~CSL3_RowResult();

protected:
    virtual EDB_ResType     ResultType() const;

    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0,
							I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

protected:
    /* Shouldn't be used till design changes ...
    class CRowInfo : public CDBParams
    {
    public:
        CRowInfo(sqlite3_stmt* stmt);
        virtual ~CRowInfo(void);

    public:
        virtual unsigned int GetNum(void) const;

        virtual string GetName(
            const CDBParamVariant& param, 
            CDBParamVariant::ENameFormat format = 
                CDBParamVariant::eSQLServerName) const;
        virtual unsigned int GetIndex(const CDBParamVariant& param) const;

        virtual size_t GetMaxSize(const CDBParamVariant& param) const;
        virtual EDB_Type GetDataType(const CDBParamVariant& param) const;
        virtual EDirection GetDirection(const CDBParamVariant& param) const;

    private:
        sqlite3_stmt* x_GetSQLite3stmt(void) const
        {
            _ASSERT(m_SQLite3stmt);
            return m_SQLite3stmt;
        }

    private:
        sqlite3_stmt*   m_SQLite3stmt;
        unsigned int    m_NofCols;
    };
    */

private:
    sqlite3_stmt* x_GetSQLite3stmt(void) const
    {
        _ASSERT(m_SQLite3stmt);
        return m_SQLite3stmt;
    }

    int Check(int rc)
    {
        _ASSERT(m_Cmd);
        return m_Cmd->Check(rc);
    }

    CSL3_LangCmd*       m_Cmd;
    int                 m_CurrItem;
    sqlite3_stmt*       m_SQLite3stmt;
    int                 m_RC;
    bool                m_FetchDone;
};


END_NCBI_SCOPE


#endif


