#ifndef DBAPI_DRIVER_ODBC___INTERFACES__HPP
#define DBAPI_DRIVER_ODBC___INTERFACES__HPP

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
 * File Description:  Driver for MS-SQL server (odbc version)
 *
 */

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>
#include <dbapi/driver/util/parameters.hpp>

#ifdef NCBI_OS_MSWIN
#include <windows.h>
#endif

#if !defined(HAVE_LONG_LONG)  &&  defined(SIZEOF_LONG_LONG)
#  if SIZEOF_LONG_LONG != 0
#    define HAVE_LONG_LONG 1  // needed by UnixODBC
#  endif
#endif

#if defined(NCBI_OS_MSWIN)
#  define HAVE_SQLGETPRIVATEPROFILESTRING 1
#endif

#if defined(UCS2) && defined(HAVE_WSTRING)
#  define UNICODE 1
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#define HAS_DEFERRED_PREPARE 1

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(odbc)

#if defined(UNICODE)
    typedef SQLWCHAR TSqlChar;
    typedef wstring::size_type TStrSize;
    typedef wchar_t TChar;
#else
    typedef SQLCHAR TSqlChar;
    typedef string::size_type TStrSize;
    typedef char TChar;
#endif

END_SCOPE(odbc)

class CODBCContext;
class CODBC_Connection;
class CODBC_LangCmd;
class CODBC_RPCCmd;
class CODBC_CursorCmd;
class CODBC_BCPInCmd;
class CODBC_SendDataCmd;
class CODBC_RowResult;
class CODBC_ParamResult;
class CODBC_StatusResult;
class CODBCContextRegistry;
class CStatementBase;

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Reporter
{
public:
    CODBC_Reporter(impl::CDBHandlerStack* hs,
                   SQLSMALLINT ht,
                   SQLHANDLE h,
                   const CODBC_Connection* connection = NULL,
                   const CODBC_Reporter* parent_reporter = NULL);
    ~CODBC_Reporter(void);

public:
    void ReportErrors(void) const;

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const;

    void SetHandlerStack(impl::CDBHandlerStack& hs) {
        m_HStack = &hs;
    }
    void SetHandle(SQLHANDLE h) {
        m_Handle = h;
    }
    void SetHandleType(SQLSMALLINT ht) {
        m_HType = ht;
    }
    void SetExtraMsg(const string& em) {
        m_ExtraMsg = em;
    }
    string GetExtraMsg(void) const;

    // Needed for correct reporting of errors establishing new connections.
    class CTempConnectionGuard
    {
    public:
        CTempConnectionGuard(CODBC_Reporter& rptr, CODBC_Connection& conn)
            : m_Reporter(rptr)
        {
            _ASSERT(rptr.m_Connection == NULL);
            rptr.m_Connection = &conn;
        }

        ~CTempConnectionGuard()
        {
            m_Reporter.m_Connection = NULL;
        }

    private:
        CODBC_Reporter& m_Reporter;
    };
    friend class CTempConnectionGuard;

private:
    CODBC_Reporter(void);

    void x_PostMsg(CDB_Exception& ex, bool always_throw = false) const;

private:
    impl::CDBHandlerStack*  m_HStack;
    SQLHANDLE               m_Handle;
    SQLSMALLINT             m_HType;
    const CODBC_Reporter*   m_ParentReporter;
    string                  m_ExtraMsg;
    const CODBC_Connection* m_Connection;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBCContext :
    public impl::CDriverContext,
    public impl::CWinSock
{
    friend class CDB_Connection;

public:
    CODBCContext(SQLLEN version = SQL_OV_ODBC3,
                 int tds_version = 80,
                 bool use_dsn = false);
    virtual ~CODBCContext(void);

public:
    //
    // GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    //

    virtual bool IsAbleTo(ECapability cpb) const {return false;}

    virtual string GetDriverName(void) const;

    //
    // ODBC specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    void SetPacketSize(SQLUINTEGER packet_size);
    SQLUINTEGER GetPacketSize(void) const
    {
        return m_PacketSize;
    }

    SQLHENV GetODBCContext(void) const
    {
        return m_Context;
    }
    const CODBC_Reporter& GetReporter(void) const
    {
        return m_Reporter;
    }
    int GetTDSVersion(void) const
    {
        return m_TDSVersion;
    }

    bool GetUseDSN(void) const
    {
        return m_UseDSN;
    }

    bool CheckSIE(int rc, SQLHDBC con);
    void SetupErrorReporter(const CDBConnParams& params);

protected:
    virtual impl::CConnection* MakeIConnection(const CDBConnParams& params);

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const
    {
        m_Reporter.ReportError(ex);
    }

private:
    SQLHENV         m_Context;
    SQLUINTEGER     m_PacketSize;
    CODBC_Reporter  m_Reporter;
    bool            m_UseDSN;
    CODBCContextRegistry* m_Registry;
    int             m_TDSVersion;

    void x_ReportConError(SQLHDBC con);

    void x_AddToRegistry(void);
    void x_RemoveFromRegistry(void);
    void x_SetRegistry(CODBCContextRegistry* registry);
    void x_Close(bool delete_conn = true);


    friend class CODBCContextRegistry;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Connection::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Connection : public impl::CConnection
{
    friend class CStatementBase;
    friend class CODBCContext;
    friend class CDB_Connection;
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_BCPInCmd;
    friend class CODBC_SendDataCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_CursorCmdExpl;
    friend class CODBC_Reporter;

protected:
    CODBC_Connection(CODBCContext& cntx,
                     const CDBConnParams& params);
    virtual ~CODBC_Connection(void);

protected:
    virtual bool IsAlive(void);

    virtual CDB_LangCmd*     LangCmd     (const string&   lang_query);
    virtual CDB_RPCCmd*      RPC         (const string&   rpc_name);
    virtual CDB_BCPInCmd*    BCPIn       (const string&   table_name);
    virtual CDB_CursorCmd*   Cursor      (const string&   cursor_name,
                                          const string&   query,
                                          unsigned int    batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd (I_ITDescriptor& desc,
                                          size_t          data_size,
                                          bool            log_it = true,
                                          bool            dump_results = true);

    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    virtual bool Refresh(void);
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;

    CODBC_LangCmd* xLangCmd(const string&   lang_query);

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort(void);

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void);

    virtual void SetTimeout(size_t nof_secs);
    size_t GetTimeout() const
    {
        return m_query_timeout;
    }
    virtual void SetCancelTimeout(size_t nof_secs);
    size_t GetCancelTimeout() const
    {
        return m_cancel_timeout;
    }

    virtual string GetDriverName(void) const;

protected:
    // for NCBI_DATABASE_THROW_ANNOTATED
    const CDBParams* GetBindParams(void) const;

    string GetDbgInfo(void) const
    {
        return m_Reporter.GetExtraMsg();
    }
    void ReportErrors(void)
    {
        m_Reporter.ReportErrors();
    }

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const
    {
        m_Reporter.ReportError(ex);
    }

private:
    bool x_SendData(CDB_ITDescriptor::ETDescriptorType descr_type,
                    CStatementBase& stmt,
                    CDB_Stream& stream);
    static string x_MakeFreeTDSVersion(int version);
    static string x_GetDriverName(const IRegistry& registry);
    void x_SetConnAttributesBefore(const CODBCContext& cntx,
                                   const CDBConnParams& params);
    void x_Connect(CODBCContext& cntx,
                   const CDBConnParams& params) const;
    void x_SetupErrorReporter(const CDBConnParams& params);

    const SQLHDBC   m_Link;
    CStatementBase* m_ActiveStmt;
    CODBC_Reporter  m_Reporter;
    size_t          m_query_timeout;
    size_t          m_cancel_timeout;
};


/////////////////////////////////////////////////////////////////////////////
class CStatementBase : public impl::CBaseCmd
{
    friend class CODBC_Connection;
public:
    CStatementBase(CODBC_Connection& conn, const string& query);
    CStatementBase(CODBC_Connection& conn, const string& cursor_name, const string& query);
    ~CStatementBase(void);

public:
    SQLHSTMT GetHandle(void) const
    {
        return m_Cmd;
    }
    CODBC_Connection& GetConnection(void)
    {
        return static_cast<CODBC_Connection&>(GetConnImpl());
    }
    const CODBC_Connection& GetConnection(void) const
    {
        return static_cast<const CODBC_Connection&>(GetConnImpl());
    }

public:
    void SetDbgInfo(const string& msg)
    {
        m_Reporter.SetExtraMsg( msg );
    }
    string GetDbgInfo(void) const
    {
        return m_Reporter.GetExtraMsg();
    }
    void ReportErrors(void) const
    {
        m_Reporter.ReportErrors();
    }

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const
    {
        m_Reporter.ReportError(ex);
    }

    CDBParams& GetBindParams(void)
    {
        return CBaseCmd::GetBindParams();
    }
    const CDBParams& GetBindParams(void) const
    {
        return const_cast<CStatementBase*>(this)->CBaseCmd::GetBindParams();
    }

public:
    // Do not throw exceptions in case of database errors.
    // Exception will be thrown in case of a logical error only.
    // Return false in case of a database error.
    bool CheckRC(int rc) const;
    int CheckSIE(int rc, const char* msg, unsigned int msg_num) const;
    int CheckSIENd(int rc, const char* msg, unsigned int msg_num) const;

    bool Close(void) const;
    bool Unbind(void) const
    {
        return CheckRC( SQLFreeStmt(m_Cmd, SQL_UNBIND) );
    }
    bool ResetParams(void) const
    {
        return CheckRC( SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS) );
    }

    bool IsMultibyteClientEncoding(void) const
    {
        return GetConnection().IsMultibyteClientEncoding();
    }
    EEncoding GetClientEncoding(void) const
    {
        return GetConnection().GetClientEncoding();
    }

protected:
    virtual int    RowCount(void) const;

    string Type2String(const CDB_Object& param) const;
    bool x_BindParam_ODBC(const CDB_Object& param,
                          CMemPot& bind_guard,
                          SQLLEN* indicator_base,
                          unsigned int pos) const;
    SQLSMALLINT x_GetCType(const CDB_Object& param) const;
    SQLSMALLINT x_GetSQLType(const CDB_Object& param) const;
    SQLULEN x_GetMaxDataSize(const CDB_Object& param) const;
    SQLLEN x_GetCurDataSize(const CDB_Object& param) const;
    SQLLEN x_GetIndicator(const CDB_Object& param) const;
    SQLPOINTER x_GetData(const CDB_Object& param, CMemPot& bind_guard) const;

protected:
    SQLLEN  m_RowCount;
//     bool    m_HasFailed;

private:
    void x_Init(void);

    SQLHSTMT            m_Cmd;
    CODBC_Reporter      m_Reporter;
    bool                m_IsActive;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_LangCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_LangCmd :
    public CStatementBase
{
    friend class CODBC_Connection;
    friend class CODBC_CursorCmdBase;
    friend class CODBC_CursorResult;
    friend class CODBC_CursorResultExpl;
    friend class CODBC_CursorCmd;
    friend class CODBC_CursorCmdExpl;

protected:
    CODBC_LangCmd(
        CODBC_Connection& conn,
        const string& lang_query
        );

public:
    virtual ~CODBC_LangCmd(void);

protected:
    virtual bool Send(void);
    virtual bool Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;
    virtual bool CloseCursor(void);

protected:
    void SetCursorName(const string& name) const;

private:
    bool x_AssignParams(string& cmd, CMemPot& bind_guard, SQLLEN* indicator);
    bool xCheck4MoreResults(void);

    CODBC_RowResult*  m_Res;
    bool              m_HasMoreResults;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RPCCmd :
    public CStatementBase
{
    friend class CODBC_Connection;

protected:
    CODBC_RPCCmd(CODBC_Connection& conn,
                 const string& proc_name);
    virtual ~CODBC_RPCCmd(void);

protected:
    virtual CDBParams& GetBindParams(void);

    virtual bool Send(void);
    virtual bool Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    bool x_AssignParams(string& cmd, string& q_exec, string& q_select,
        CMemPot& bind_guard, SQLLEN* indicator);
    bool xCheck4MoreResults(void);

    bool              m_HasStatus;
    bool              m_HasMoreResults;
    impl::CResult*    m_Res;

    auto_ptr<CDBParams> m_InParams;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmdBase :
    public CStatementBase
{
protected:
    CODBC_CursorCmdBase(CODBC_Connection& conn,
                        const string& cursor_name,
                        const string& query);
    virtual ~CODBC_CursorCmdBase(void);

protected:
    virtual CDBParams& GetBindParams(void);
    virtual CDBParams& GetDefineParams(void);
    virtual int  RowCount(void) const;

protected:
    CODBC_LangCmd           m_CursCmd;

    unsigned int            m_FetchSize;
    auto_ptr<impl::CResult> m_Res;
};


// Implicit cursor based on ODBC API.
class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmd :
    public CODBC_CursorCmdBase
{
    friend class CODBC_Connection;

protected:
    CODBC_CursorCmd(CODBC_Connection& conn,
                    const string& cursor_name,
                    const string& query);
    virtual ~CODBC_CursorCmd(void);

protected:
    virtual CDB_Result* OpenCursor(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool dump_results = true);
    virtual bool Delete(const string& table_name);
    virtual bool CloseCursor(void);

protected:
    CDB_ITDescriptor* x_GetITDescriptor(unsigned int item_num);
};


// Explicit cursor based on Transact-SQL cursors.
class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmdExpl :
    public CODBC_CursorCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_CursorCmdExpl(CODBC_Connection& conn,
                        const string& cursor_name,
                        const string& query);
    virtual ~CODBC_CursorCmdExpl(void);

protected:
    virtual CDB_Result* OpenCursor(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool dump_results = true);
    virtual bool Delete(const string& table_name);
    virtual bool CloseCursor(void);

protected:
    CDB_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

protected:
    auto_ptr<CODBC_LangCmd> m_LCmd;
};


// Future development.
// // Explicit cursor based on stored procedures.
// class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmdExplSP :
//     public CODBC_CursorCmd
// {
// };

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//
// This class is not implemented yet ...

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_BCPInCmd :
    public CStatementBase
{
    friend class CODBC_Connection;

protected:
    CODBC_BCPInCmd(CODBC_Connection& conn,
                   SQLHDBC cmd,
                   const string& table_name);
    virtual ~CODBC_BCPInCmd(void);

protected:
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool CommitBCPTrans(void);
    virtual bool Cancel(void);
    virtual bool EndBCP(void);
    virtual int  RowCount(void) const;

private:
    SQLHDBC GetHandle(void) const
    {
        return m_Cmd;
    }
    bool x_AssignParams(void* p);
    static int x_GetBCPDataType(EDB_Type type);
    static size_t x_GetBCPDataSize(EDB_Type type);
    static void* x_GetDataTerminator(EDB_Type type);
    static size_t x_GetDataTermSize(EDB_Type type);
    static const void* x_GetDataPtr(EDB_Type type, void* pb);

    SQLHDBC m_Cmd;
    bool    m_WasBound;
    bool    m_HasTextImage;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_SendDataCmd :
    public CStatementBase,
    public impl::CSendDataCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_SendDataCmd(CODBC_Connection& conn,
                      CDB_ITDescriptor& descr,
                      size_t nof_bytes,
                      bool logit,
                      bool dump_results);
    virtual ~CODBC_SendDataCmd(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual bool   Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;

private:
    void xCancel(void);
    bool xCheck4MoreResults(void);

    SQLLEN  m_ParamPH;
    const CDB_ITDescriptor::ETDescriptorType m_DescrType;
    CODBC_RowResult*  m_Res;
    bool m_HasMoreResults;
    bool m_DumpResults;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RowResult : public impl::CResult
{
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_Connection;
    friend class CODBC_CursorCmdExpl;
    friend class CODBC_SendDataCmd;

public:
    CStatementBase& GetStatementBase(void)
    {
        return m_Stmt;
    }
    const CStatementBase& GetStatementBase(void) const
    {
        return m_Stmt;
    }

    EEncoding GetClientEncoding(void) const
    {
        return GetStatementBase().GetClientEncoding();
    }

protected:
    CODBC_RowResult(
        CStatementBase& stmt,
        SQLSMALLINT nof_cols,
        SQLLEN* row_count
        );
    virtual ~CODBC_RowResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    CDB_ITDescriptor* GetImageOrTextDescriptor(int item_no,
                                               const string& cond);
    virtual bool            SkipItem(void);

    int xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
        SQLINTEGER buffer_size);
    CDB_Object* x_LoadItem(I_Result::EGetItem policy, CDB_Object* item_buf);
    CDB_Object* x_MakeItem(void);

protected:
    SQLHSTMT GetHandle(void) const
    {
        return GetStatementBase().GetHandle();
    }
    void ReportErrors(void)
    {
        GetStatementBase().ReportErrors();
    }

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const
    {
        GetStatementBase().ReportError(ex);
    }

    string GetDbgInfo(void) const
    {
        return GetStatementBase().GetDbgInfo();
    }
    const CDBParams* GetBindParams(void) const 
    {
        return &m_Stmt.GetBindParams();
    }
    const CODBC_Connection& GetConnection(void) const
    {
        return GetStatementBase().GetConnection();
    }
    void Close(void)
    {
        GetStatementBase().Close();
    }
    bool CheckSIENoD_Text(CDB_Stream* val);
#ifdef HAVE_WSTRING
    bool CheckSIENoD_WText(CDB_Stream* val);
#endif
    bool CheckSIENoD_Binary(CDB_Stream* val);

private:
    CStatementBase&   m_Stmt;
    int               m_CurrItem;
    bool              m_EOR;
    unsigned int      m_CmdNum;
    enum {eODBC_Column_Name_Size = 80};

    typedef struct t_SODBC_ColDescr {
        string          ColumnName;
        SQLULEN         ColumnSize;
        SQLSMALLINT     DataType;
        SQLSMALLINT     DecimalDigits;
    } SODBC_ColDescr;

    SODBC_ColDescr* m_ColFmt;
    SQLLEN* const   m_RowCountPtr;

    string m_LastReadData;
    bool m_HasMoreData;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_ParamResult::
//  CODBC_StatusResult::
//  CODBC_CursorResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_StatusResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;

protected:
    CODBC_StatusResult(CStatementBase& stmt);
    virtual ~CODBC_StatusResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_ParamResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;

protected:
    CODBC_ParamResult(
        CStatementBase& stmt,
        SQLSMALLINT nof_cols
        );
    virtual ~CODBC_ParamResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorResult : public impl::CResult
{
    friend class CODBC_CursorCmd;
    friend class CODBC_CursorCmdExpl;

protected:
    CODBC_CursorResult(CODBC_LangCmd* cmd);
    virtual ~CODBC_CursorResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual const CDBParams& GetDefineParams(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    virtual bool            SkipItem(void);

protected:
    string GetDbgInfo(void) const
    {
        return m_Cmd->GetDbgInfo();
    }
    CDB_Result* GetCDBResultPtr(void) const
    {
        return m_Res;
    }
    CDB_Result& GetCDBResult(void)
    {
        _ASSERT(m_Res);
        return *m_Res;
    }
    void SetCDBResultPtr(CDB_Result* res)
    {
        _ASSERT(res);
        m_Res = res;
    }

    NCBI_NORETURN
    void ReportError(CDB_Exception& ex) const
    {
        m_Cmd->ReportError(ex);
    }


protected:
    // data
    CODBC_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
    bool m_EOR;
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorResultExpl : public CODBC_CursorResult
{
    friend class CODBC_CursorCmdExpl;

protected:
    CODBC_CursorResultExpl(CODBC_LangCmd* cmd);
    virtual ~CODBC_CursorResultExpl(void);

protected:
    virtual bool Fetch(void);
};

#define NCBI_ODBC_THROW(ex_class, message, err_code, severity) \
    do { \
        ex_class ex(DIAG_COMPILE_INFO, NULL, (message), severity, err_code); \
        ReportError(ex); \
    } while (0)
#define NCBI_ODBC_RETHROW(prev_ex, ex_class, message, err_code, severity) \
    do { \
        ex_class ex(DIAG_COMPILE_INFO, &(prev_ex), (message), severity, \
                    err_code); \
        ReportError(ex); \
    } while (0)

///////////////////////////////////////////////////////////////////////////
inline
const CDBParams* CODBC_Connection::GetBindParams(void) const {
    return m_ActiveStmt ? &m_ActiveStmt->GetBindParams() : NULL;
}

END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_ODBC___INTERFACES__HPP */

