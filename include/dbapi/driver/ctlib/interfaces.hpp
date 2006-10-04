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

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>

#include <corelib/ncbi_safe_static.hpp>

#include <ctpublic.h>
#include <bkpublic.h>


BEGIN_NCBI_SCOPE

class CDB_ITDescriptor;

class CTLibContext;
class CTL_Connection;
class CTL_Cmd;
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
class CTLibContextRegistry;


CS_INT NCBI_DBAPIDRIVER_CTLIB_EXPORT GetCtlibTdsVersion(int version = 0);


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTLibContext :
    public impl::CDriverContext,
    public impl::CWinSock
{
    friend class CDB_Connection;

public:
    CTLibContext(bool   reuse_context = true,
                 CS_INT version       = GetCtlibTdsVersion());
    virtual ~CTLibContext(void);

public:
    //
    // GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    //

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual unsigned int GetLoginTimeout(void) const;
    virtual unsigned int GetTimeout     (void) const;

    //
    // CTLIB specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    // Deprecated. Use SetApplicationName instead.
    virtual void CTLIB_SetApplicationName(const string& a_name);
    // Deprecated. Use SetHostName instead.
    virtual void CTLIB_SetHostName(const string& host_name);
    virtual void CTLIB_SetPacketSize(CS_INT packet_size);
    virtual void CTLIB_SetLoginRetryCount(CS_INT n);
    virtual void CTLIB_SetLoginLoopDelay(CS_INT nof_sec);

    virtual bool IsAbleTo(ECapability cpb) const;

    virtual CS_CONTEXT* CTLIB_GetContext(void) const;
    CS_LOCALE* GetLocale(void) const
    {
        return m_Locale;
    }

    static bool CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg);
    static bool CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                    CS_CLIENTMSG* msg);
    static bool CTLIB_srverr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                     CS_SERVERMSG* msg);
    CS_INT GetTDSVersion(void) const
    {
        return m_TDSVersion;
    }

    virtual void SetClientCharset(const string& charset);

protected:
    virtual impl::CConnection* MakeIConnection(const SConnAttr& conn_attr);

private:
    CS_CONTEXT* m_Context;
    CS_LOCALE*  m_Locale;
    CS_INT      m_PacketSize;
    CS_INT      m_LoginRetryCount;
    CS_INT      m_LoginLoopDelay;
    CS_INT      m_TDSVersion;
    CTLibContextRegistry* m_Registry;

    CS_CONNECTION* x_ConnectToServer(const string&   srv_name,
                                     const string&   usr_name,
                                     const string&   passwd,
                                     TConnectionMode mode);
    void x_AddToRegistry(void);
    void x_RemoveFromRegistry(void);
    void x_SetRegistry(CTLibContextRegistry* registry);
    // Deinitialize all internal structures.
    void x_Close(bool delete_conn = true);
    bool x_SafeToFinalize(void) const;
    CS_RETCODE Check(CS_RETCODE rc) const;

    friend class CTLibContextRegistry;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_Connection : public impl::CConnection
{
    friend class CTLibContext;
    friend class CDB_Connection;
    friend class CTL_Cmd;

protected:
    CTL_Connection(CTLibContext& cntx, CS_CONNECTION* con,
                   bool reusable, const string& pool_name);
    virtual ~CTL_Connection(void);

public:
    CS_RETCODE Check(CS_RETCODE rc);
    CS_INT GetBLKVersion(void) const;
    const CTLibContext& GetCTLibContext(void) const
    {
        _ASSERT(m_Cntx);
        return *m_Cntx;
    }

protected:
    virtual bool IsAlive(void);

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
    virtual bool Refresh(void);
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;

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
    void x_CmdAlloc(CS_COMMAND** cmd);

    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);

private:
    bool x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it = true);
    I_ITDescriptor* x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in);
    CS_CONNECTION* x_GetSybaseConn(void) const { return m_Link; }
    bool x_ProcessResultInternal(CS_COMMAND* cmd, CS_INT res_type);

    CTLibContext*   m_Cntx;
    CS_CONNECTION*  m_Link;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Cmd::
//

class CTL_Cmd
{
public:
    CTL_Cmd(CTL_Connection* conn, CS_COMMAND* cmd);
    virtual ~CTL_Cmd(void);

protected:
    inline CS_RETCODE Check(CS_RETCODE rc);
    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);
    CS_RETCODE CheckSentSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);
    CS_RETCODE CheckSFBCP(CS_RETCODE rc, const char* msg, unsigned int msg_num);

protected:
    inline CTL_Connection& GetConnection(void);
    inline const CTL_Connection& GetConnection(void) const;
    inline CS_COMMAND* x_GetSybaseCmd(void) const;
    inline void SetSybaseCmd(CS_COMMAND* cmd);
    inline void DropCmd(impl::CCommand& cmd);
    inline bool x_SendData(I_ITDescriptor& desc,
                           CDB_Stream& img,
                           bool log_it = true);
    inline CDB_SendDataCmd* ConnSendDataCmd (I_ITDescriptor& desc,
                                             size_t          data_size,
                                             bool            log_it = true);

    void DropSybaseCmd(void);
    bool Cancel(void);
    bool AssignCmdParam(CDB_Object&   param,
                        const string& param_name,
                        CS_DATAFMT&   param_fmt,
                        CS_SMALLINT   indicator,
                        bool          declare_only = false
                        );
    void GetRowCount(int* cnt);

protected:
    // Result-related ...
    inline bool HaveResult(void) const;
    inline CTL_RowResult& GetResult(void);
    inline void DeleteResult(void);
    virtual CDB_Result* CreateResult(impl::CResult& result) = 0;
    inline CDB_Result* CreateResult(void);
    CDB_Result* MakeResult(void);

    void SetResult(CTL_RowResult* result)
    {
        m_Res = result;
    }
    inline CTL_RowResult* MakeCursorResult(void);
    inline CTL_RowResult* MakeRowResult(void);
    inline CTL_RowResult* MakeParamResult(void);
    inline CTL_RowResult* MakeComputeResult(void);
    inline CTL_RowResult* MakeStatusResult(void);

    inline bool ProcessResultInternal(CS_INT res_type);
    bool ProcessResultInternal(CDB_Result& res);
    bool ProcessResults(void);

protected:
    bool            m_HasFailed;
    bool            m_WasSent;
    int             m_RowCount;

private:
    CTL_Connection* m_Connect;
    CS_COMMAND*     m_Cmd;
    CTL_RowResult*  m_Res;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_LangCmd : CTL_Cmd, public impl::CBaseCmd
{
    friend class CTL_Connection;

protected:
    CTL_LangCmd(CTL_Connection* conn,
                CS_COMMAND* cmd,
                const string& lang_query,
                unsigned int nof_params);
    virtual ~CTL_LangCmd(void);

    void Close(void);

protected:
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);
    virtual CDB_Result* CreateResult(impl::CResult& result);

private:
    bool x_AssignParams(void);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_RPCCmd : CTL_Cmd, public impl::CBaseCmd
{
    friend class CTL_Connection;

protected:
    CTL_RPCCmd(CTL_Connection* con, CS_COMMAND* cmd,
               const string& proc_name, unsigned int nof_params);
    virtual ~CTL_RPCCmd(void);

    void Close(void);

protected:
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);
    virtual CDB_Result* CreateResult(impl::CResult& result);

private:
    bool x_AssignParams(void);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorCmd :
    CTL_Cmd,
    public impl::CCursorCmd
{
    friend class CTL_Connection;

protected:
    CTL_CursorCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                  const string& cursor_name, const string& query,
                  unsigned int nof_params, unsigned int fetch_size);
    virtual ~CTL_CursorCmd(void);

    void CloseForever(void);

protected:
    virtual CDB_Result* Open(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                     bool log_it = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool Close(void);
    virtual CDB_Result* CreateResult(impl::CResult& result);

private:
    bool x_AssignParams(bool just_declare = false);
    I_ITDescriptor*   x_GetITDescriptor(unsigned int item_num);

private:
    unsigned int      m_FetchSize;
    bool              m_Used;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BCPInCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_BCPInCmd : CTL_Cmd, public impl::CBaseCmd
{
    friend class CTL_Connection;

protected:
    CTL_BCPInCmd(CTL_Connection* con, CS_BLKDESC* cmd,
                 const string& table_name, unsigned int nof_columns);
    virtual ~CTL_BCPInCmd(void);

    void Close(void);

protected:
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool CommitBCPTrans(void);
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual bool EndBCP(void);
    virtual CDB_Result* CreateResult(impl::CResult& result);
    virtual bool HasFailed(void) const;
    virtual int RowCount(void) const;

private:
    bool x_AssignParams(void);
    CS_BLKDESC* x_GetSybaseCmd(void) const { return m_Cmd; }
    bool x_IsUnicodeClientAPI(void) const;
    CS_VOID* x_GetValue(const CDB_Char& value) const;
    CS_VOID* x_GetValue(const CDB_VarChar& value) const;

private:
    struct SBcpBind {
        CS_INT      datalen;
        CS_SMALLINT indicator;
        char        buffer[sizeof(CS_NUMERIC)];
    };

    CS_BLKDESC*         m_Cmd;
    AutoArray<SBcpBind> m_Bind;
    int                 m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_SendDataCmd : CTL_Cmd, public impl::CSendDataCmd
{
    friend class CTL_Connection;

protected:
    CTL_SendDataCmd(CTL_Connection* conn, CS_COMMAND* cmd, size_t nof_bytes);
    virtual ~CTL_SendDataCmd(void);

    void Close(void);
    virtual bool Cancel(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual CDB_Result* CreateResult(impl::CResult& result);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RowResult::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_RowResult : public impl::CResult
{
    friend class CTL_Connection;
    friend class CTL_Cmd;
    friend class CTL_CursorCmd;

protected:
    CTL_RowResult(CS_COMMAND* cmd, CTL_Connection& conn);
    virtual ~CTL_RowResult(void);

    void Close(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual unsigned int    NofItems(void) const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    virtual bool            SkipItem(void);

    CS_RETCODE my_ct_get_data(CS_COMMAND* cmd, CS_INT item,
                              CS_VOID* buffer,
                              CS_INT buflen, CS_INT *outlen);
    CDB_Object* s_GetItem(CS_COMMAND* cmd, CS_INT item_no, CS_DATAFMT& fmt,
                          CDB_Object* item_buf);
    CS_COMMAND* x_GetSybaseCmd(void) const { return m_Cmd; }
    CS_RETCODE Check(CS_RETCODE rc)
    {
        _ASSERT(m_Connect);
        return m_Connect->Check(rc);
    }

protected:
    // data
    CTL_Connection*         m_Connect;
    CS_COMMAND*             m_Cmd;
    int                     m_CurrItem;
    bool                    m_EOR;
    unsigned int            m_NofCols;
    AutoArray<CS_DATAFMT>   m_ColFmt;
    int                     m_BindedCols;
    AutoArray<CS_VOID*>     m_BindItem;
    AutoArray<CS_INT>       m_Copied;
    AutoArray<CS_SMALLINT>  m_Indicator;
    unsigned char           m_BindBuff[2048];

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
    friend class CTL_Connection;
    friend class CTL_Cmd;

protected:
    CTL_ParamResult(CS_COMMAND* pCmd, CTL_Connection& conn) :
    CTL_RowResult(pCmd, conn)
    {
    }

protected:
    virtual EDB_ResType ResultType(void) const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_ComputeResult : public CTL_RowResult
{
    friend class CTL_Connection;
    friend class CTL_Cmd;

protected:
    CTL_ComputeResult(CS_COMMAND* pCmd, CTL_Connection& conn) :
    CTL_RowResult(pCmd, conn)
    {
    }

protected:
    virtual EDB_ResType ResultType(void) const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_StatusResult :  public CTL_RowResult
{
    friend class CTL_Connection;
    friend class CTL_Cmd;

protected:
    CTL_StatusResult(CS_COMMAND* pCmd, CTL_Connection& conn) :
    CTL_RowResult(pCmd, conn)
    {
    }

protected:
    virtual EDB_ResType ResultType(void) const;
};


class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorResult :  public CTL_RowResult
{
    friend class CTL_Cmd;

protected:
    CTL_CursorResult(CS_COMMAND* pCmd, CTL_Connection& conn) :
    CTL_RowResult(pCmd, conn)
    {
    }
    virtual ~CTL_CursorResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
    virtual bool        SkipItem(void);
};


/////////////////////////////////////////////////////////////////////////////
inline
CTL_Connection& CTL_Cmd::GetConnection(void)
{
    _ASSERT(m_Connect);
    return *m_Connect;
}

inline
const CTL_Connection& CTL_Cmd::GetConnection(void) const
{
    _ASSERT(m_Connect);
    return *m_Connect;
}

inline
CS_COMMAND* CTL_Cmd::x_GetSybaseCmd(void) const
{
    return m_Cmd;
}

inline
void CTL_Cmd::SetSybaseCmd(CS_COMMAND* cmd)
{
    m_Cmd = cmd;
}

inline
CS_RETCODE CTL_Cmd::Check(CS_RETCODE rc)
{
    _ASSERT(m_Connect);
    return m_Connect->Check(rc);
}

inline
void CTL_Cmd::DropCmd(impl::CCommand& cmd)
{
    GetConnection().DropCmd(cmd);
}

inline
bool CTL_Cmd::x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it)
{
    return GetConnection().x_SendData(desc, img, log_it);
}

inline
CDB_SendDataCmd* CTL_Cmd::ConnSendDataCmd (I_ITDescriptor& desc,
                                           size_t          data_size,
                                           bool            log_it)
{
    return GetConnection().SendDataCmd(desc, data_size, log_it);
}

inline
bool CTL_Cmd::HaveResult(void) const
{
    return (m_Res != NULL);
}

inline
CTL_RowResult& CTL_Cmd::GetResult(void)
{
    _ASSERT(HaveResult());
    return *m_Res;
}

inline
CDB_Result* CTL_Cmd::CreateResult(void)
{
    return CreateResult(static_cast<impl::CResult&>(*m_Res));
}

inline
void CTL_Cmd::DeleteResult(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
bool CTL_Cmd::ProcessResultInternal(CS_INT res_type)
{
    return GetConnection().x_ProcessResultInternal(m_Cmd, res_type);
}

inline
CTL_RowResult* CTL_Cmd::MakeCursorResult(void)
{
    return new CTL_CursorResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult* CTL_Cmd::MakeRowResult(void)
{
    return new CTL_RowResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult* CTL_Cmd::MakeParamResult(void)
{
    return new CTL_ParamResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult* CTL_Cmd::MakeComputeResult(void)
{
    return new CTL_ComputeResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult* CTL_Cmd::MakeStatusResult(void)
{
    return new CTL_StatusResult(x_GetSybaseCmd(), GetConnection());
}


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
    virtual int DescriptorType(void) const;
    virtual ~CTL_ITDescriptor(void);

protected:
    CTL_ITDescriptor(void);

    CS_IODESC m_Desc;
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_CTLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.49  2006/10/04 19:26:10  ssikorsk
 * Revamp code to use AutoArray where it is possible.
 *
 * Revision 1.48  2006/10/03 17:49:14  ssikorsk
 * + CTL_Connection::GetCTLibContext;
 * + CTL_BCPInCmd::x_GetValue;
 *
 * Revision 1.47  2006/09/18 15:21:28  ssikorsk
 * Added methods GetLoginTimeout and GetTimeout to CTLibContext.
 *
 * Revision 1.46  2006/09/13 19:36:02  ssikorsk
 * - CTLibContext::m_ClientCharset.
 *
 * Revision 1.45  2006/08/31 14:58:41  ssikorsk
 * Added ClientCharset and locale to the DriverContext.
 *
 * Revision 1.44  2006/08/22 20:11:42  ssikorsk
 * - CTL_RowResult::m_CmdNum.
 *
 * Revision 1.43  2006/08/17 06:33:06  vakatov
 * Switch default version of TDS protocol to 12.5 (from 11.0)
 *
 * Revision 1.42  2006/08/10 15:17:54  ssikorsk
 * Added method CTL_Connection::CheckSFB;
 * Added method CTL_Cmd:: CheckSentSFB;
 *
 * Revision 1.41  2006/08/02 15:12:46  ssikorsk
 * Added methods CheckSFB and CheckSFBCP to CTL_Cmd.
 *
 * Revision 1.40  2006/07/20 19:50:02  ssikorsk
 * Added CTLibContext.m_TDSVersion, CTLibContext ::GetTDSVersion();
 * Added CTL_Connection:: GetBLKVersion();
 *
 * Revision 1.39  2006/07/19 14:09:55  ssikorsk
 * Refactoring of CursorCmd.
 *
 * Revision 1.38  2006/07/18 15:46:00  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.37  2006/07/12 16:28:48  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.36  2006/07/05 18:04:10  ssikorsk
 * Added NCBI_DBAPIDRIVER_CTLIB_EXPORT to the GetCtlibTdsVersion declaration.
 *
 * Revision 1.35  2006/07/05 16:06:24  ssikorsk
 * Added function GetCtlibTdsVersion().
 *
 * Revision 1.34  2006/06/05 20:59:40  ssikorsk
 * Moved method Release from CTL_Connection to I_Connection.
 *
 * Revision 1.33  2006/06/05 19:06:28  ssikorsk
 * Declared C...Cmd::Release deprecated.
 *
 * Revision 1.32  2006/06/05 18:02:21  ssikorsk
 * Added method Cancel to CTL_SendDataCmd.
 *
 * Revision 1.31  2006/06/05 14:34:49  ssikorsk
 * Removed members m_MsgHandlers and m_CMDs from CTL_Connection.
 *
 * Revision 1.30  2006/05/15 19:35:17  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.29  2006/05/11 17:53:25  ssikorsk
 * Added CDBExceptionStorage class
 *
 * Revision 1.28  2006/05/10 14:38:13  ssikorsk
 *      Added class CGuard to CCTLExceptions.
 *
 * Revision 1.27  2006/05/04 15:23:26  ucko
 * Modify CCTLExceptions to store pointers, as our exception classes
 * don't support assignment.
 *
 * Revision 1.26  2006/05/03 15:09:54  ssikorsk
 * Added method Check to CTLibContext, CTL_Connection and CTL_RowResult classes;
 * Replaced type of  CTL_Connection::m_CMDs from CPointerPot to deque<CDB_BaseEnt*>;
 * Added new class CTL_Cmd, which is a base class for all "command" classes now;
 * Added new class CCTLExceptions to collect database messages;
 * Moved global functions g_CTLIB_GetRowCount and g_CTLIB_AssignCmdParam into CTL_Cmd class;
 *
 * Revision 1.25  2006/04/05 14:21:29  ssikorsk
 * Added CTL_Connection::Close
 *
 * Revision 1.24  2006/03/06 19:50:02  ssikorsk
 * Added method Close/CloseForever to all context/command-aware classes.
 * Use getters to access Sybase's context and command handles.
 *
 * Revision 1.23  2006/01/23 13:12:50  ssikorsk
 * Renamed CTLibContext::MakeConnection to MakeIConnection.
 *
 * Revision 1.22  2006/01/03 18:55:00  ssikorsk
 * Replace method Connect with MakeConnection.
 *
 * Revision 1.21  2005/12/06 19:20:02  ssikorsk
 * Added private method GetConnection to all *command* classes
 *
 * Revision 1.20  2005/10/20 13:01:04  ssikorsk
 * - CTLibContext::m_AppName
 * - CTLibContext::m_HostName
 *
 * Revision 1.19  2005/09/07 11:00:07  ssikorsk
 * Added GetColumnNum method
 *
 * Revision 1.18  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
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
