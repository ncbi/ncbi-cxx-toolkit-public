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

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#ifdef NCBI_OS_MSWIN
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>


BEGIN_NCBI_SCOPE

class CODBCContext;
class CODBC_Connection;
class CODBC_LangCmd;
class CODBC_RPCCmd;
class CODBC_CursorCmd;
class CODBC_BCPInCmd;
class CODBC_SendDataCmd;
class CODBC_RowResult;
class CODBC_ParamResult;
class CODBC_ComputeResult;
class CODBC_StatusResult;


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Reporter
{
public:
    CODBC_Reporter(CDBHandlerStack* hs, SQLSMALLINT ht, SQLHANDLE h) :
        m_HStack(hs), m_HType(ht), m_Handle(h) {}
    void ReportErrors();
    void SetHandlerStack(CDBHandlerStack* hs) {
        m_HStack= hs;
    }
    void SetHandle(SQLHANDLE h) {
        m_Handle= h;
    }
	void SetHandleType(SQLSMALLINT ht) {
		m_HType= ht;
	}
private:
    CODBC_Reporter();
    CDBHandlerStack* m_HStack;
    SQLHANDLE m_Handle;
    SQLSMALLINT m_HType;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBCContext : public I_DriverContext
{
    friend class CDB_Connection;

public:
    CODBCContext(SQLINTEGER version = SQL_OV_ODBC3, bool use_dsn= false);

    //
    // GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    //

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual CDB_Connection* Connect(const string&   srv_name,
                                    const string&   user_name,
                                    const string&   passwd,
                                    TConnectionMode mode,
                                    bool            reusable  = false,
                                    const string&   pool_name = kEmptyStr);

    virtual bool IsAbleTo(ECapability cpb) const {return false;}

    virtual ~CODBCContext();


    //
    // ODBC specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    virtual void ODBC_SetPacketSize(SQLUINTEGER packet_size);

    virtual SQLHENV ODBC_GetContext() const;


private:
    SQLHENV     m_Context;
    SQLUINTEGER m_PacketSize;
    SQLUINTEGER m_LoginTimeout;
    SQLUINTEGER m_Timeout;
    SQLUINTEGER m_TextImageSize;
    CODBC_Reporter m_Reporter;
    bool m_UseDSN;

    SQLHDBC x_ConnectToServer(const string&   srv_name,
			       const string&   usr_name,
			       const string&   passwd,
			       TConnectionMode mode);
    void xReportConError(SQLHDBC con);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Connection::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Connection : public I_Connection
{
    friend class CODBCContext;
    friend class CDB_Connection;
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_BCPInCmd;
    friend class CODBC_SendDataCmd;

protected:
    CODBC_Connection(CODBCContext* cntx, SQLHDBC con,
		     bool reusable, const string& pool_name);

    virtual bool IsAlive();

    virtual CDB_LangCmd*     LangCmd     (const string&   lang_query,
                                          unsigned int    nof_params = 0);
    virtual CDB_RPCCmd*      RPC         (const string&   rpc_name,
                                          unsigned int    nof_args);
    virtual CDB_BCPInCmd*    BCPIn       (const string&   table_name,
                                          unsigned int    nof_columns);
    virtual CDB_CursorCmd*   Cursor      (const string&   cursor_name,
                                          const string&   query,
                                          unsigned int    nof_params,
                                          unsigned int    batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd (I_ITDescriptor& desc,
                                          size_t          data_size,
                                          bool            log_it = true);

    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true);
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text&  txt,
                          bool log_it = true);
    virtual bool Refresh();
    virtual const string& ServerName() const;
    virtual const string& UserName()   const;
    virtual const string& Password()   const;
    virtual I_DriverContext::TConnectionMode ConnectMode() const;
    virtual bool IsReusable() const;
    virtual const string& PoolName() const;
    virtual I_DriverContext* Context() const;
    virtual void PushMsgHandler(CDB_UserHandler* h);
    virtual void PopMsgHandler (CDB_UserHandler* h);
    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);
    virtual void Release();

    virtual ~CODBC_Connection();

    void ODBC_SetTimeout(SQLUINTEGER nof_secs);
    void ODBC_SetTextImageSize(SQLUINTEGER nof_bytes);

    void DropCmd(CDB_BaseEnt& cmd);

    CODBC_LangCmd* xLangCmd(const string&   lang_query,
                            unsigned int    nof_params = 0);

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

private:
    bool x_SendData(SQLHSTMT cmd, CDB_Stream& stream, CODBC_Reporter& rep);

    SQLHDBC         m_Link;

    CODBCContext*   m_Context;
    CPointerPot     m_CMDs;
    CDBHandlerStack m_MsgHandlers;
    string          m_Server;
    string          m_User;
    string          m_Passwd;
    string          m_Pool;
    CODBC_Reporter  m_Reporter;
    bool            m_Reusable;
    bool            m_BCPable;
    bool            m_SecureLogin;
    CDB_ResultProcessor* m_ResProc;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_LangCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_LangCmd : public I_LangCmd
{
    friend class CODBC_Connection;
    friend class CODBC_CursorCmd;
	friend class CODBC_CursorResult;

protected:
    CODBC_LangCmd(
        CODBC_Connection* conn,
        SQLHSTMT cmd,
        const string& lang_query,
        unsigned int nof_params
        );

    virtual bool More(const string& query_text);
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool Send();
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;
    virtual bool HasFailed() const;
    virtual int  RowCount() const;
    virtual void DumpResults();
    virtual void Release();

    virtual ~CODBC_LangCmd();

private:
    bool x_AssignParams(string& cmd, CMemPot& bind_guard, SQLINTEGER* indicator);
    bool xCheck4MoreResults();

    CODBC_Connection* m_Connect;
    SQLHSTMT          m_Cmd;
    string            m_Query;
    CDB_Params        m_Params;
    CODBC_RowResult*  m_Res;
    CODBC_Reporter    m_Reporter;
    int               m_RowCount;
    bool              m_hasResults;
    bool              m_WasSent;
    bool              m_HasFailed;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RPCCmd : public I_RPCCmd
{
    friend class CODBC_Connection;
protected:
    CODBC_RPCCmd(CODBC_Connection* con, SQLHSTMT cmd,
               const string& proc_name, unsigned int nof_params);

    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param = false);
    virtual bool Send();
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;
    virtual bool HasFailed() const;
    virtual int  RowCount() const;
    virtual void DumpResults();
    virtual void SetRecompile(bool recompile = true);
    virtual void Release();

    virtual ~CODBC_RPCCmd();

private:
    bool x_AssignParams(string& cmd, string& q_exec, string& q_select,
		CMemPot& bind_guard, SQLINTEGER* indicator);
    bool xCheck4MoreResults();

    CODBC_Connection* m_Connect;
    SQLHSTMT          m_Cmd;
    string            m_Query;
    CDB_Params        m_Params;
    CODBC_Reporter    m_Reporter;
    bool              m_WasSent;
    bool              m_HasFailed;
    bool              m_Recompile;
    bool              m_HasStatus;
	bool              m_hasResults;
    I_Result*         m_Res;
    int               m_RowCount;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmd : public I_CursorCmd
{
    friend class CODBC_Connection;
protected:
    CODBC_CursorCmd(CODBC_Connection* conn, SQLHSTMT cmd,
                  const string& cursor_name, const string& query,
                  unsigned int nof_params);

    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
    virtual CDB_Result* Open();
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
				 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
					 bool log_it = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount() const;
    virtual bool Close();
    virtual void Release();

    virtual ~CODBC_CursorCmd();

private:
    bool x_AssignParams(bool just_declare = false);
    CDB_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CODBC_LangCmd m_CursCmd;
    CODBC_LangCmd* m_LCmd;

    CODBC_Connection* m_Connect;
    string          m_Name;
    unsigned int    m_FetchSize;
    bool            m_IsOpen;
    bool            m_IsDeclared;
    bool            m_HasFailed;
    I_Result*       m_Res;
    int             m_RowCount;
};




/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_BCPInCmd : public I_BCPInCmd
{
    friend class CODBC_Connection;
protected:
    CODBC_BCPInCmd(CODBC_Connection* con, SQLHDBC cmd,
                 const string& table_name, unsigned int nof_columns);

    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow();
    virtual bool CompleteBatch();
    virtual bool Cancel();
    virtual bool CompleteBCP();
    virtual void Release();

    virtual ~CODBC_BCPInCmd();

private:
    bool x_AssignParams(void* p);

    CODBC_Connection* m_Connect;
    SQLHDBC     m_Cmd;
    string          m_Query;
    CDB_Params      m_Params;
    bool            m_WasSent;
    bool            m_HasFailed;
	bool            m_WasBound;
	bool            m_HasTextImage;

    CODBC_Reporter  m_Reporter;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_SendDataCmd : public I_SendDataCmd
{
    friend class CODBC_Connection;
protected:
    CODBC_SendDataCmd(CODBC_Connection* con, SQLHSTMT cmd, CDB_ITDescriptor& descr,
                      size_t nof_bytes, bool logit);

    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release();

    virtual ~CODBC_SendDataCmd();

private:
    void xCancel();
    CODBC_Connection* m_Connect;
    SQLHSTMT          m_Cmd;
    CODBC_Reporter    m_Reporter;
    size_t            m_Bytes2go;
	SQLINTEGER        m_ParamPH;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RowResult : public I_Result
{
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_Connection;
protected:
    CODBC_RowResult(
        SQLSMALLINT nof_cols,
        SQLHSTMT cmd,
        CODBC_Reporter& r,
        int* row_count
        );

    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    CDB_ITDescriptor* GetImageOrTextDescriptor(int item_no,
                                               const string& cond);
    virtual bool            SkipItem();

    virtual ~CODBC_RowResult();

	int xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
		SQLINTEGER buffer_size);
    CDB_Object* xLoadItem(CDB_Object* item_buf);
    CDB_Object* xMakeItem();

private:
    // data
    SQLHSTMT          m_Cmd;
    // CODBC_Connection* m_Connect;
    int               m_CurrItem;
    bool              m_EOR;
    unsigned int      m_NofCols;
    unsigned int      m_CmdNum;
#define ODBC_COLUMN_NAME_SIZE 80
    typedef struct t_SODBC_ColDescr {
        SQLCHAR     ColumnName[ODBC_COLUMN_NAME_SIZE];
        SQLUINTEGER ColumnSize;
        SQLSMALLINT DataType;
        SQLSMALLINT DecimalDigits;
    } SODBC_ColDescr;

    SODBC_ColDescr* m_ColFmt;
    CODBC_Reporter& m_Reporter;
    int* const      m_RowCountPtr;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_ParamResult::
//  CODBC_ComputeResult::
//  CODBC_StatusResult::
//  CODBC_CursorResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_StatusResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;
protected:
    CODBC_StatusResult(
        SQLHSTMT cmd,
        CODBC_Reporter& r
        )
        : CODBC_RowResult(1, cmd, r, NULL)
    {
    }
    virtual EDB_ResType ResultType() const;
    virtual ~CODBC_StatusResult();
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_ParamResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;
protected:
    CODBC_ParamResult(
        SQLSMALLINT nof_cols,
        SQLHSTMT cmd,
        CODBC_Reporter& r
        )
        : CODBC_RowResult(nof_cols, cmd, r, NULL)
    {
    }
    virtual EDB_ResType ResultType() const;
    virtual ~CODBC_ParamResult();
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorResult : public I_Result
{
    friend class CODBC_CursorCmd;
protected:
    CODBC_CursorResult(CODBC_LangCmd* cmd);
    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CODBC_CursorResult();

    // data
    CODBC_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
	bool m_EOR;
};

/////////////////////////////////////////////////////////////////////////////
extern NCBI_DBAPIDRIVER_ODBC_EXPORT const string kDBAPI_ODBC_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_ODBC_EXPORT
void
NCBI_EntryPoint_xdbapi_odbc(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_ODBC___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.10  2005/02/23 21:36:24  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.9  2005/02/15 17:57:19  ssikorsk
 * Fixed RowCountNum() with a SELECT statement
 *
 * Revision 1.8  2004/12/21 22:16:04  soussov
 * fixes bug in SendDataCmd
 *
 * Revision 1.7  2003/07/17 20:43:17  soussov
 * connections pool improvements
 *
 * Revision 1.6  2003/06/06 18:43:15  soussov
 * Removes SetPacketSize()
 *
 * Revision 1.5  2003/06/05 15:57:02  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.4  2003/05/05 20:45:51  ucko
 * Lowercase header names for compatibility with Unix.
 *
 * Revision 1.3  2003/02/13 15:43:44  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_ODBC_EXPORT for class definitions
 *
 * Revision 1.2  2002/07/03 21:48:08  soussov
 * adds DSN support if needed
 *
 * Revision 1.1  2002/06/18 22:00:53  soussov
 * initial commit
 *
 * ===========================================================================
 */
