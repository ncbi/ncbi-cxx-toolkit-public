#ifndef DBAPI_DRIVER_CTLIB___INTERFACES__HPP
#define DBAPI_DRIVER_CTLIB___INTERFACES__HPP

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
 * File Description:  Driver for CTLib server
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>

#include <ctpublic.h>
#include <bkpublic.h>


BEGIN_NCBI_SCOPE

class CTLibContext;
class CTL_Connection;
class CTL_LangCmd;
class CTL_RPCCmd;
class CTL_CursorCmd;
class CTL_BCPInCmd;
class CTL_SendDataCmd;
class CTL_RowResult;
class CTL_ParamResult;
class CTL_ComputeResult;
class CTL_StatusResult;
class CTL_CursorResult;



/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTLibContext : public I_DriverContext
{
    friend class CDB_Connection;

public:
    CTLibContext(bool reuse_context = true, CS_INT version = CS_VERSION_110);

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

    virtual ~CTLibContext();


    //
    // CTLIB specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    virtual void CTLIB_SetApplicationName(const string& a_name);
    virtual void CTLIB_SetHostName(const string& host_name);
    virtual void CTLIB_SetPacketSize(CS_INT packet_size);
    virtual void CTLIB_SetLoginRetryCount(CS_INT n);
    virtual void CTLIB_SetLoginLoopDelay(CS_INT nof_sec);

    virtual bool IsAbleTo(ECapability cpb) const;

    virtual CS_CONTEXT* CTLIB_GetContext() const;

    static bool CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg);
    static bool CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                    CS_CLIENTMSG* msg);
    static bool CTLIB_srverr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                     CS_SERVERMSG* msg);

private:
    CS_CONTEXT* m_Context;
    string      m_AppName;
    string      m_HostName;
    CS_INT      m_PacketSize;
    CS_INT      m_LoginRetryCount;
    CS_INT      m_LoginLoopDelay;

    CS_CONNECTION* x_ConnectToServer(const string&   srv_name,
                                     const string&   usr_name,
                                     const string&   passwd,
                                     TConnectionMode mode);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_Connection : public I_Connection
{
    friend class CTLibContext;
    friend class CDB_Connection;
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_CursorCmd;
    friend class CTL_BCPInCmd;
    friend class CTL_SendDataCmd;

protected:
    CTL_Connection(CTLibContext* cntx, CS_CONNECTION* con,
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

    virtual ~CTL_Connection();

    void DropCmd(CDB_BaseEnt& cmd);

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

private:
    bool x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it = true);
    I_ITDescriptor* x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in);

    CS_CONNECTION*  m_Link;
    CTLibContext*   m_Context;
    CPointerPot     m_CMDs;
    CDBHandlerStack m_MsgHandlers;
    string          m_Server;
    string          m_User;
    string          m_Passwd;
    string          m_Pool;
    bool            m_Reusable;
    bool            m_BCPable;
    bool            m_SecureLogin;
    CDB_ResultProcessor* m_ResProc;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_LangCmd : public I_LangCmd
{
    friend class CTL_Connection;
protected:
    CTL_LangCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                const string& lang_query, unsigned int nof_params);

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

    virtual ~CTL_LangCmd();

private:
    bool x_AssignParams();

    CTL_Connection* m_Connect;
    CS_COMMAND*     m_Cmd;
    string          m_Query;
    CDB_Params      m_Params;
    bool            m_WasSent;
    bool            m_HasFailed;
    I_Result*       m_Res;
    int             m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_RPCCmd : public I_RPCCmd
{
    friend class CTL_Connection;
protected:
    CTL_RPCCmd(CTL_Connection* con, CS_COMMAND* cmd,
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

    virtual ~CTL_RPCCmd();

private:
    bool x_AssignParams();

    CTL_Connection* m_Connect;
    CS_COMMAND*     m_Cmd;
    string          m_Query;
    CDB_Params      m_Params;
    bool            m_WasSent;
    bool            m_HasFailed;
    bool            m_Recompile;
    I_Result*       m_Res;
    int             m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorCmd : public I_CursorCmd
{
    friend class CTL_Connection;
protected:
    CTL_CursorCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                  const string& cursor_name, const string& query,
                  unsigned int nof_params, unsigned int fetch_size);

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

    virtual ~CTL_CursorCmd();

private:
    bool x_AssignParams(bool just_declare = false);
    I_ITDescriptor*   x_GetITDescriptor(unsigned int item_num);
    CTL_Connection*   m_Connect;
    CS_COMMAND*       m_Cmd;
    string            m_Name;
    string            m_Query;
    CDB_Params        m_Params;
    unsigned int      m_FetchSize;
    bool              m_IsOpen;
    bool              m_HasFailed;
    bool              m_Used;
    CTL_CursorResult* m_Res;
    int               m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BCPInCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_BCPInCmd : public I_BCPInCmd
{
    friend class CTL_Connection;
protected:
    CTL_BCPInCmd(CTL_Connection* con, CS_BLKDESC* cmd,
                 const string& table_name, unsigned int nof_columns);

    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow();
    virtual bool CompleteBatch();
    virtual bool Cancel();
    virtual bool CompleteBCP();
    virtual void Release();

    virtual ~CTL_BCPInCmd();

private:
    bool x_AssignParams();

    CTL_Connection* m_Connect;
    CS_BLKDESC*     m_Cmd;
    string          m_Query;
    CDB_Params      m_Params;
    bool            m_WasSent;
    bool            m_HasFailed;

    struct SBcpBind {
        CS_INT      datalen;
        CS_SMALLINT indicator;
        char        buffer[sizeof(CS_NUMERIC)];
    };
    SBcpBind*       m_Bind;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_SendDataCmd : public I_SendDataCmd
{
    friend class CTL_Connection;
protected:
    CTL_SendDataCmd(CTL_Connection* con, CS_COMMAND* cmd, size_t nof_bytes);

    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release();

    virtual ~CTL_SendDataCmd();

private:
    CTL_Connection* m_Connect;
    CS_COMMAND*     m_Cmd;
    size_t          m_Bytes2go;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RowResult::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_RowResult : public I_Result
{
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_CursorCmd;
    friend class CTL_Connection;
    friend class CTL_SendDataCmd;
protected:
    CTL_RowResult(CS_COMMAND* cmd);

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
    virtual bool            SkipItem();

    CS_RETCODE my_ct_get_data(CS_COMMAND* cmd, CS_INT item, 
							  CS_VOID* buffer, 
							  CS_INT buflen, CS_INT *outlen);
    CDB_Object* s_GetItem(CS_COMMAND* cmd, CS_INT item_no, CS_DATAFMT& fmt,
						  CDB_Object* item_buf);
    virtual ~CTL_RowResult();

    // data
    CS_COMMAND*  m_Cmd;
    int          m_CurrItem;
    bool         m_EOR;
    unsigned int m_NofCols;
    unsigned int m_CmdNum;
    CS_DATAFMT*  m_ColFmt;
    int          m_BindedCols;
    CS_VOID**    m_BindItem;
    CS_INT*      m_Copied;
    CS_SMALLINT* m_Indicator;
    unsigned char m_BindBuff[2048];
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_ComputeResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_ParamResult : public CTL_RowResult
{
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_Connection;
    friend class CTL_SendDataCmd;
    friend class CTL_CursorCmd;
protected:
    CTL_ParamResult(CS_COMMAND* pCmd) : CTL_RowResult(pCmd) {}
    virtual EDB_ResType ResultType() const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_ComputeResult : public CTL_RowResult
{
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_Connection;
    friend class CTL_SendDataCmd;
    friend class CTL_CursorCmd;
protected:
    CTL_ComputeResult(CS_COMMAND* pCmd) : CTL_RowResult(pCmd) {}
    virtual EDB_ResType ResultType() const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_StatusResult :  public CTL_RowResult
{
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_Connection;
    friend class CTL_SendDataCmd;
    friend class CTL_CursorCmd;
protected:
    CTL_StatusResult(CS_COMMAND* pCmd) : CTL_RowResult(pCmd) {}
    virtual EDB_ResType ResultType() const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorResult :  public CTL_RowResult
{
    friend class CTL_CursorCmd;
protected:
    CTL_CursorResult(CS_COMMAND* pCmd) : CTL_RowResult(pCmd) {}
    virtual EDB_ResType ResultType() const;
    virtual bool        SkipItem();
    virtual ~CTL_CursorResult();
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ITDescriptor::
//

#define CTL_ITDESCRIPTOR_TYPE_MAGNUM 0xc00

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_ITDescriptor : public I_ITDescriptor
{
    friend class CTL_RowResult;
    friend class CTL_Connection;
    friend class CTL_CursorCmd;

public:
    virtual int DescriptorType() const;
    virtual ~CTL_ITDescriptor();

protected:
    CTL_ITDescriptor();

    CS_IODESC m_Desc;
};



/////////////////////////////////////////////////////////////////////////////
//
//  Miscellaneous
//

extern void g_CTLIB_GetRowCount
(CS_COMMAND* cmd,
 int*        cnt
);


extern bool g_CTLIB_AssignCmdParam
(CS_COMMAND*   cmd,
 CDB_Object&   param,
 const string& param_name,
 CS_DATAFMT&   param_fmt,
 CS_SMALLINT   indicator,
 bool          declare_only = false
 );


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_CTLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2005/02/23 21:33:49  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.16  2003/07/17 20:41:48  soussov
 * connections pool improvements
 *
 * Revision 1.15  2003/06/05 15:55:26  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.14  2003/04/21 20:17:06  soussov
 * Buffering fetched result to avoid ct_get_data performance issue
 *
 * Revision 1.13  2003/04/01 20:26:24  vakatov
 * Temporarily rollback to R1.11 -- until more backward-incompatible
 * changes (in CException) are ready to commit (to avoid breaking the
 * compatibility twice).
 *
 * Revision 1.11  2003/02/13 16:06:36  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_CTLIB_EXPORT for class definitions
 *
 * Revision 1.10  2002/03/28 00:37:01  vakatov
 * CTL_CursorCmd::  use CTL_CursorResult rather than I_Result (fix access)
 *
 * Revision 1.9  2002/03/26 15:26:02  soussov
 * new image/text operations added
 *
 * Revision 1.8  2001/11/06 17:58:56  lavr
 * Get rid of inline methods - all moved into source files
 *
 * Revision 1.7  2001/10/22 15:14:50  lavr
 * Beautifications...
 *
 * Revision 1.6  2001/09/27 23:01:06  vakatov
 * CTL_***Result::  virtual methods' implementation moved away from the header
 *
 * Revision 1.5  2001/09/27 20:08:30  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.4  2001/09/27 15:41:28  soussov
 * CTL_Connection::Release() added
 *
 * Revision 1.3  2001/09/26 23:23:28  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
 *
 * Revision 1.2  2001/09/24 20:52:19  vakatov
 * Fixed args like "string& s = 0" to "string& s = kEmptyStr"
 *
 * Revision 1.1  2001/09/21 23:39:53  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
