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

#ifdef FTDS_IN_USE

#  if defined(NCBI_FTDS_VERSION_NAME)  &&  defined(NCBI_IS_FTDS_DEFAULT)
#    undef NCBI_FTDS_VERSION_NAME
#    undef NCBI_FTDS_VERSION_NAME2
#    define NCBI_FTDS_VERSION_NAME(X)    X
#    define NCBI_FTDS_VERSION_NAME2(X,Y) X ## Y
#  elif defined(NCBI_DBAPI_RENAME_CTLIB)
#    include <../impl/ncbi_ftds_ver.h>
#  else
#    define NCBI_FTDS_VERSION            0
#    define NCBI_FTDS_VERSION_NAME(X)    X
#    define NCBI_FTDS_VERSION_NAME2(X,Y) X ## Y
#  endif

#  define NCBI_NS_FTDS_CTLIB NCBI_FTDS_VERSION_NAME2(ftds,_ctlib)

// Make it look like the ftds driver ...
#    define CTLibContext            CTDSContext
#    define CTL_Connection          CTDS_Connection
#    define CTL_Cmd                 CTDS_Cmd
#    define CTL_CmdBase             CTDS_CmdBase
#    define CTL_LangCmd             CTDS_LangCmd
#    define CTL_RPCCmd              CTDS_RPCCmd
#    define CTL_CursorCmd           CTDS_CursorCmd
#    define CTL_BCPInCmd            CTDS_BCPInCmd
#    define CTL_SendDataCmd         CTDS_SendDataCmd
#    define CTL_Result              CTDS_Result
#    define CTL_RowResult           CTDS_RowResult
#    define CTL_ParamResult         CTDS_ParamResult
#    define CTL_ComputeResult       CTDS_ComputeResult
#    define CTL_StatusResult        CTDS_StatusResult
#    define CTL_CursorResult        CTDS_CursorResult
#    define CTL_CursorResultExpl    CTDS_CursorResultExpl
#    define CTL_BlobResult          CTDS_BlobResult
#    define CTL_BlobDescriptor      CTDS_BlobDescriptor
#    define CTL_CursorBlobDescriptor CTDS_CursorBlobDescriptor
#    define CTLibContextRegistry    CTDSContextRegistry

// historical names
#    define CTL_ITDescriptor        CTDS_ITDescriptor
#    define CTL_CursorITDescriptor  CTDS_CursorITDescriptor

#    define CTLIB_SetApplicationName    TDS_SetApplicationName
#    define CTLIB_SetHostName           TDS_SetHostName
#    define CTLIB_SetPacketSize         TDS_SetPacketSize
#    define CTLIB_SetMaxNofConns        TDS_SetMaxNofConns

#  define NCBI_CS_STRING_TYPE CS_VARCHAR_TYPE
#  if NCBI_FTDS_VERSION >= 91
#    define USE_STRUCT_CS_VARCHAR 1
#  endif

#else

// There is a problem with CS_VARCHAR_TYPE on x86_64 ...
#  define NCBI_CS_STRING_TYPE CS_CHAR_TYPE

#endif // FTDS_IN_USE

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace NCBI_NS_FTDS_CTLIB
{
#endif

class CTLibContext;
class CTL_Connection;
class CTL_Cmd;
class CTL_CmdBase;
class CTL_LangCmd;
class CTL_RPCCmd;
class CTL_CursorCmd;
class CTL_CursorCmdExpl;
class CTL_BCPInCmd;
class CTL_SendDataCmd;
class CTL_RowResult;
class CTL_ParamResult;
class CTL_ComputeResult;
class CTL_StatusResult;
class CTL_CursorResult;
class CTL_CursorResultExpl;
class CTL_CursorBlobDescriptor;
class CTLibContextRegistry;


CS_INT NCBI_DBAPIDRIVER_CTLIB_EXPORT GetCtlibTdsVersion(int version = 0);

/////////////////////////////////////////////////////////////////////////////
namespace ctlib
{

class Connection
{
public:
    Connection(CTLibContext& context, CTL_Connection& ctl_conn);
    ~Connection(void) noexcept;
    /// Drop allocated connection.
    bool Drop(void);

public:
    CS_CONNECTION* GetNativeHandle(void) const
    {
        return m_Handle;
    }

    bool IsOpen(void) const
    {
        return m_IsOpen;
    }
    bool Open(const CDBConnParams& params);
    bool Close(void);
    // cancel all pending commands
    bool Cancel(void);
    bool IsAlive(void);
    // Ask ctlib directly whether a connection is open or not ...
    bool IsOpen_native(void);

    bool IsDead(void) const
    {
        return m_IsDead;
    }
    void SetDead(bool flag = true)
    {
        // When connection is dead it doen't mean that it was automatically
        // closed from ctlib's point of view.
        m_IsDead = flag;
    }

protected:
    const CTL_Connection& GetCTLConn(void) const;
    CTL_Connection& GetCTLConn(void);

    const CTLibContext& GetCTLContext(void) const
    {
        _ASSERT(m_CTL_Context);
        return *m_CTL_Context;
    }
    CTLibContext& GetCTLContext(void)
    {
        _ASSERT(m_CTL_Context);
        return *m_CTL_Context;
    }

    CS_RETCODE CheckWhileOpening(CS_RETCODE rc);

private:
    CTLibContext*   m_CTL_Context;
    CTL_Connection* m_CTL_Conn;
    CS_CONNECTION*  m_Handle;
    bool            m_IsAllocated;
    bool            m_IsOpen;
    bool            m_IsDead;
};

////////////////////////////////////////////////////////////////////////////////
class Command
{
public:
    Command(CTL_Connection& ctl_conn);
    ~Command(void);

public:
    CS_COMMAND* GetNativeHandle(void) const
    {
        return m_Handle;
    }

    bool Open(CS_INT type, CS_INT option, const string& arg = kEmptyCStr);
    bool GetDataInfo(CS_IODESC& desc);
    bool SendData(CS_VOID* buff, CS_INT buff_len);
    bool Send(void);
    CS_RETCODE GetResults(CS_INT& res_type);
    CS_RETCODE Fetch(void);

protected:
    const CTL_Connection& GetCTLConn(void) const
    {
        _ASSERT(m_CTL_Conn);
        return *m_CTL_Conn;
    }
    CTL_Connection& GetCTLConn(void)
    {
        _ASSERT(m_CTL_Conn);
        return *m_CTL_Conn;
    }

    void Drop(void);
    void Close(void);

private:
    CTL_Connection* m_CTL_Conn;
    CS_COMMAND*     m_Handle;
    bool            m_IsAllocated;
    bool            m_IsOpen;
};

} // namespace ctlib

/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTLibContext :
    public impl::CDriverContext,
    public impl::CWinSock
{
    friend class ncbi::CDB_Connection;

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
    virtual bool SetMaxBlobSize  (size_t nof_bytes);

    virtual void InitApplicationName(void);

    virtual unsigned int GetLoginTimeout(void) const;
    virtual unsigned int GetTimeout     (void) const;

    virtual string GetDriverName(void) const;

    //
    // CTLIB specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    // Deprecated. Use SetApplicationName instead.
    NCBI_DEPRECATED
    virtual void CTLIB_SetApplicationName(const string& a_name);
    // Deprecated. Use SetHostName instead.
    NCBI_DEPRECATED
    virtual void CTLIB_SetHostName(const string& host_name);
    virtual void CTLIB_SetPacketSize(CS_INT packet_size);
    virtual void CTLIB_SetLoginRetryCount(CS_INT n);
    virtual void CTLIB_SetLoginLoopDelay(CS_INT nof_sec);

    virtual bool IsAbleTo(ECapability cpb) const;

    /// Set maximal number of open connections.
    /// Default value is 30.
    bool SetMaxConnect(unsigned int num);
    unsigned int GetMaxConnect(void);

    virtual CS_CONTEXT* CTLIB_GetContext(void) const;
    CS_LOCALE* GetLocale(void) const
    {
        return m_Locale;
    }

    static CS_RETCODE CS_PUBLIC CTLIB_cserr_handler(CS_CONTEXT* context,
                                                    CS_CLIENTMSG* msg);
    static CS_RETCODE CS_PUBLIC CTLIB_cterr_handler(CS_CONTEXT* context,
                                                    CS_CONNECTION* con,
                                                    CS_CLIENTMSG* msg);
    static CS_RETCODE CS_PUBLIC CTLIB_srverr_handler(CS_CONTEXT* context,
                                                     CS_CONNECTION* con,
                                                     CS_SERVERMSG* msg);

    CS_INT GetTDSVersion(void) const
    {
        return m_TDSVersion;
    }
    CS_INT GetPacketSize(void) const
    {
        return m_PacketSize;
    }
    CS_INT GetLoginRetryCount(void) const
    {
        return m_LoginRetryCount;
    }
    CS_INT GetLoginLoopDelay(void) const
    {
        return m_LoginLoopDelay;
    }

    virtual void SetClientCharset(const string& charset);
    CS_RETCODE Check(CS_RETCODE rc) const;

protected:
    virtual impl::CConnection* MakeIConnection(const CDBConnParams& params);
    CRWLock& x_GetCtxLock(void) const;

private:
    CS_CONTEXT* m_Context;
    CS_LOCALE*  m_Locale;
    CS_INT      m_PacketSize;
    CS_INT      m_LoginRetryCount;
    CS_INT      m_LoginLoopDelay;
    CS_INT      m_TDSVersion;
    CTLibContextRegistry* m_Registry;
    bool        m_ReusingContext;

    void x_AddToRegistry(void);
    void x_RemoveFromRegistry(void);
    void x_SetRegistry(CTLibContextRegistry* registry);
    // Deinitialize all internal structures.
    void x_Close(bool delete_conn = true);
    bool x_SafeToFinalize(void) const;

#if defined(FTDS_IN_USE)  &&  NCBI_FTDS_VERSION >= 95
    typedef int (*FIntHandler)(void*);
    FIntHandler m_OrigIntHandler;
#endif

    friend class CTLibContextRegistry;
    friend class ctlib::Connection;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_Connection : public impl::CConnection
{
    friend class CTLibContext;
    friend class ctlib::Connection;
    friend class ncbi::CDB_Connection;
    friend class CTL_Cmd;
    friend class CTL_CmdBase;
    friend class CTL_LRCmd;
    friend class CTL_LangCmd;
    friend class CTL_RPCCmd;
    friend class CTL_SendDataCmd;
    friend class CTL_BCPInCmd;
    friend class CTL_CursorCmd;
    friend class CTL_CursorCmdExpl;
    friend class CTL_CursorResultExpl;
    friend class CTL_RowResult;

protected:
    CTL_Connection(CTLibContext& cntx,
                   const CDBConnParams& params);
    virtual ~CTL_Connection(void);

public:
    CS_RETCODE Check(CS_RETCODE rc);
    CS_RETCODE Check(CS_RETCODE rc, const TDbgInfo& dbg_info);
    CS_INT GetBLKVersion(void) const;

    const CTLibContext& GetCTLibContext(void) const
    {
        _ASSERT(m_Cntx);
        return *m_Cntx;
    }
    CTLibContext& GetCTLibContext(void)
    {
        _ASSERT(m_Cntx);
        return *m_Cntx;
    }

    ctlib::Connection& GetNativeConnection(void)
    {
        return m_Handle;
    }
    const ctlib::Connection& GetNativeConnection(void) const
    {
        return m_Handle;
    }

    virtual bool IsAlive(void);
    bool IsOpen(void)
    {
        return m_Handle.IsOpen();
    }

    void DeferTimeout(void);

#if defined(FTDS_IN_USE)  &&  NCBI_FTDS_VERSION >= 95
    void SetCancelTimedOut(bool val)
    {
        m_CancelTimedOut = val;
    }

    bool GetCancelTimedOut(void) const
    {
        return m_CancelTimedOut;
    }
#endif


protected:
    virtual CDB_LangCmd*     LangCmd     (const string&   lang_query);
    virtual CDB_RPCCmd*      RPC         (const string&   rpc_name);
    virtual CDB_BCPInCmd*    BCPIn       (const string&   table_name);
    virtual CDB_CursorCmd*   Cursor      (const string&   cursor_name,
                                          const string&   query,
                                          unsigned int    batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd (I_BlobDescriptor& desc,
                                          size_t          data_size,
                                          bool            log_it = true,
                                          bool            dump_results = true);

    virtual bool SendData(I_BlobDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    virtual bool Refresh(void);
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;

    // This method is required for CTL_CursorCmdExpl only ...
    CTL_LangCmd* xLangCmd(const string&   lang_query);

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

    CS_RETCODE CheckWhileOpening(CS_RETCODE rc);

    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);

    bool IsDead(void) const
    {
        return !IsValid()  ||  GetNativeConnection().IsDead();
    }
    void SetDead(bool flag = true)
    {
        GetNativeConnection().SetDead(flag);
    }

    const TDbgInfo& GetDbgInfo(void) const;

    virtual void SetTimeout(size_t nof_secs);
    virtual void SetCancelTimeout(size_t nof_secs);

    virtual size_t GetTimeout(void) const;
    virtual size_t GetCancelTimeout(void) const;

    virtual unsigned int GetRowsInCurrentBatch(void) const;

    size_t PrepareToCancel(void);
    void CancelFinished(size_t was_timeout);
    bool IsCancelInProgress(void) const { return m_CancelInProgress; }

    virtual TSockHandle GetLowLevelHandle(void) const;
    virtual string GetVersionString(void) const;

    void CompleteBlobDescriptor(I_BlobDescriptor& desc,
                                const string& cursor_name,
                                int item_num);

    void CompleteBlobDescriptors(vector<I_BlobDescriptor*>& descs,
                                 const string& cursor_name);

private:
    void x_LoadTextPtrProcs(void);
    void x_CmdAlloc(CS_COMMAND** cmd);
    void x_SetExtraMsg(const I_BlobDescriptor& descr, size_t data_size);
    bool x_SendData(I_BlobDescriptor& desc, CDB_Stream& img,
                    bool log_it = true);
    bool x_SendUpdateWrite(CDB_BlobDescriptor& desc, CDB_Stream& img,
                           size_t size);

    I_BlobDescriptor* x_GetNativeBlobDescriptor(const CDB_BlobDescriptor& d);
    bool x_IsLegacyBlobColumnType(const string& table_name,
                                  const string& column_name);
    CS_CONNECTION* x_GetSybaseConn(void) const { return m_Handle.GetNativeHandle(); }
    bool x_ProcessResultInternal(CS_COMMAND* cmd, CS_INT res_type);

    const CDBParams* GetLastParams(void) const;

    CTLibContext*       m_Cntx;
    CTL_CmdBase*        m_ActiveCmd;
    ctlib::Connection   m_Handle;
    int                 m_TDSVersion; // as CS_TDS_nn
    bool                m_TextPtrProcsLoaded;
    bool                m_CancelInProgress;
    bool                m_CancelRequested;
    unsigned int        m_ActivityLevel;
    CMutex              m_CancelLogisticsMutex;

    class CCancelModeGuard
    {
    public:
        enum EContext {
            eAsyncCancel,
            eSyncCancel,
            eOther
        };
        
        CCancelModeGuard(CTL_Connection& conn, EContext ctx = eOther);
        ~CCancelModeGuard();

        bool IsForCancelInProgress(void) { return m_ForCancelInProgress; }

    private:
        // For the sake of DATABASE_DRIVER_ERROR
        const TDbgInfo&  GetDbgInfo()    { return m_Conn.GetDbgInfo();    }
        CTL_Connection&  GetConnection() { return m_Conn;                 }
        const CDBParams* GetLastParams() { return m_Conn.GetLastParams(); }
        
        CTL_Connection& m_Conn;
        bool            m_ForCancelInProgress;
    };

    friend class CCancelModeGuard;

#ifdef FTDS_IN_USE
    class CAsyncCancelGuard
    {
    public:
        CAsyncCancelGuard(CTL_Connection& conn);
        ~CAsyncCancelGuard(void);
    private:
        CTL_Connection &m_Conn;
    };
    friend class CAsyncCancelGuard;

    bool AsyncCancel(CTL_CmdBase& cmd);

#  if NCBI_FTDS_VERSION >= 95
    static int x_IntHandler(void* param);
    typedef int (*FIntHandler)(void*);
    FIntHandler m_OrigIntHandler;

    // The variable is used to handle the following scenarios:
    // an SQL server request has timed out; this leads to sending a cancel
    // request to the SQL server; the SQL server is supposed to answer to this
    // within a short time. If the SQL server does not answer on the cancel
    // request within one iteration over the poll() call i.e. 1 sec then this
    // variable is set to true. It happens in CTL_Connection::x_IntHandler()
    // and the checked in the CTLibContext::CTLIB_cterr_handler().
    bool        m_CancelTimedOut;
#  else
    static int x_TimeoutFunc(void* param, unsigned int total_timeout);
    int (*m_OrigTimeoutFunc)(void*, unsigned int);
    void *m_OrigTimeoutParam;
#  endif

    CFastMutex   m_AsyncCancelMutex;
    size_t       m_OrigTimeout;
    unsigned int m_BaseTimeout;
    unsigned int m_TotalTimeout;
    bool         m_AsyncCancelAllowed;
    bool         m_AsyncCancelRequested;
#endif
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CmdBase::
//

class CTL_CmdBase : public impl::CBaseCmd
{
    friend class CTL_Connection;
public:
    CTL_CmdBase(CTL_Connection& conn, const string& query);
    CTL_CmdBase(CTL_Connection& conn, const string& cursor_name,
                const string& query);
    virtual ~CTL_CmdBase(void);

protected:
    class CTempVarChar
    {
    public:
        void SetValue(const CTempString& s);
        CTempString GetValue(void) const;
    private:
#ifdef USE_STRUCT_CS_VARCHAR
        AutoPtr<CS_VARCHAR, CDeleter<CS_VARCHAR> > m_Data;
#else
        CTempString m_Data;
#endif
    };

    CS_RETCODE Check(CS_RETCODE rc);

protected:
    inline CTL_Connection& GetConnection(void);
    inline const CTL_Connection& GetConnection(void) const;

    inline void DropCmd(impl::CCommand& cmd);
    inline bool x_SendData(I_BlobDescriptor& desc,
                           CDB_Stream& img,
                           bool log_it = true);
    inline CDB_SendDataCmd* ConnSendDataCmd (I_BlobDescriptor& desc,
                                             size_t          data_size,
                                             bool            log_it = true,
                                             bool            dump_results = true);

    bool IsMultibyteClientEncoding(void) const
    {
        return GetConnection().IsMultibyteClientEncoding();
    }

    EEncoding GetClientEncoding(void) const
    {
        return GetConnection().GetClientEncoding();
    }

    enum ECancelType
    {
        eAsyncCancel = CS_CANCEL_ATTN,
        eSyncCancel  = CS_CANCEL_ALL
    };

    virtual bool x_Cancel(ECancelType)
    {
        return Cancel();
    }


protected:
    // Result-related ...
    void SetExecCntxInfo(const string& info)
    {
        m_DbgInfo->extra_msg = info;
    }
    const string& GetExecCntxInfo(void) const
    {
        return m_DbgInfo->extra_msg;
    }

    bool IsDead(void) const
    {
        return GetConnection().IsDead();
    }
    void SetDead(bool flag = true)
    {
        GetConnection().SetDead(flag);
    }
    void CheckIsDead(void) // const
    {
        if (IsDead()) {
            NCBI_DATABASE_THROW_ANNOTATED(CDB_ClientEx, "Connection has died.",
                                          122010, eDiag_Error, GetDbgInfo(),
                                          GetConnection(), GetLastParams());
        }
    }
    virtual void SetHasFailed(bool flag = true)
    {
        CBaseCmd::SetHasFailed(flag);
        if (flag  &&  !GetConnection().IsAlive()) {
            NCBI_DATABASE_THROW_ANNOTATED(CDB_ClientEx, "Connection has died.",
                                          122010, eDiag_Error, GetDbgInfo(),
                                          GetConnection(), GetLastParams());
        }
    }

    typedef CTL_Connection::TDbgInfo TDbgInfo;
    const TDbgInfo& GetDbgInfo(void) const
    {
        return *m_DbgInfo;
    }

protected:
    bool GetTimedOut(void) const
    {
        return m_TimedOut;
    }

    void SetTimedOut(bool  val)
    {
        m_TimedOut = val;
    }

    ERetriable GetRetriable(void) const
    {
        return m_Retriable;
    }

    void SetRetriable(ERetriable val)
    {
        m_Retriable = val;
    }

    void EnsureActiveStatus(void);
    
protected:
    int             m_RowCount;
    CRef<TDbgInfo>  m_DbgInfo;

private:
    bool            m_IsActive;

    // Support for a better exception in case of a suppressed timeout
    // exception.
    bool            m_TimedOut;
    ERetriable      m_Retriable;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Cmd::
//

class CTL_Cmd : public CTL_CmdBase
{
    friend class CTL_CursorCmdExpl;
    friend class CTL_CursorResultExpl;

public:
    CTL_Cmd(CTL_Connection& conn, const string& query);
    CTL_Cmd(CTL_Connection& conn, const string& cursor_name,
            const string& query);
    virtual ~CTL_Cmd(void);

protected:
    inline CS_COMMAND* x_GetSybaseCmd(void) const;
    inline void SetSybaseCmd(CS_COMMAND* cmd);

    bool AssignCmdParam(CDB_Object&   param,
                        const string& param_name,
                        CS_DATAFMT&   param_fmt,
                        bool          declare_only = false
                        );
    void GetRowCount(int* cnt);

protected:
    // Result-related ...

    inline CTL_RowResult& GetResult(void);
    inline void DeleteResult(void);
    inline void DeleteResultInternal(void);
    inline void MarkEndOfReply(void);

    inline bool HaveResult(void) const;
    void SetResult(CTL_RowResult* result)
    {
        m_Res = result;
    }

    inline CTL_RowResult* MakeCursorResult(void);
    inline CTL_RowResult* MakeRowResult(void);
    inline CTL_RowResult* MakeParamResult(void);
    inline CTL_RowResult* MakeComputeResult(void);
    inline CTL_RowResult* MakeStatusResult(void);

    bool ProcessResultInternal(CDB_Result& res);
    inline bool ProcessResultInternal(CS_INT res_type);

    CS_RETCODE CheckSFB_Internal(CS_RETCODE rc,
                                 const char* msg,
                                 unsigned int msg_num);

protected:
    void DropSybaseCmd(void);

private:
    void x_Init(void);

    CS_COMMAND*     m_Cmd;
    CTL_RowResult*  m_Res;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//
class CTL_LRCmd : public CTL_Cmd
{
public:
    CTL_LRCmd(CTL_Connection& conn,
              const string& query);
    virtual ~CTL_LRCmd(void);

public:
    CTL_RowResult* MakeResultInternal(void);
    CDB_Result* MakeResult(void);
    bool Cancel(void) override;

protected:
    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);

    bool SendInternal(void);
    bool x_Cancel(ECancelType cancel_type) override;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_LangCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_LangCmd : public CTL_LRCmd
{
    friend class CTL_Connection;
    friend class CTL_CursorCmdExpl;
    friend class CTL_CursorResultExpl;
    friend struct default_delete<CTL_LangCmd>;

protected:
    CTL_LangCmd(CTL_Connection& conn,
                const string& lang_query);
    virtual ~CTL_LangCmd(void);

    void Close(void);

protected:
    virtual bool Send(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    bool x_AssignParams(void);
    CTempString x_GetDynamicID(void);

    string m_DynamicID;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_RPCCmd : CTL_LRCmd
{
    friend class CTL_Connection;

protected:
    CTL_RPCCmd(CTL_Connection& con,
               const string& proc_name
               );
    virtual ~CTL_RPCCmd(void);

protected:
    virtual CDBParams& GetBindParams(void);

    virtual bool Send(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    bool x_AssignParams(void);
    void x_Close(void);

    unique_ptr<CDBParams> m_InParams;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorCmd : CTL_Cmd
{
    friend class CTL_Connection;

protected:
    CTL_CursorCmd(CTL_Connection& conn,
                  const string& cursor_name,
                  const string& query,
                  unsigned int fetch_size
                  );
    virtual ~CTL_CursorCmd(void);

    void CloseForever(void);

protected:
    virtual CDB_Result* OpenCursor(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateBlob(unsigned int item_num, CDB_Stream& data,
                            bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool dump_results = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool CloseCursor(void);

    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);
    CS_RETCODE CheckSFBCP(CS_RETCODE rc, const char* msg, unsigned int msg_num);

    bool ProcessResults(void);

private:
    bool x_AssignParams(bool just_declare = false);
    I_BlobDescriptor* x_GetBlobDescriptor(unsigned int item_num);

private:
    unsigned int      m_FetchSize;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmdExpl::
//  Explicit cursor (based on T-SQL)

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorCmdExpl : CTL_Cmd
{
    friend class CTL_Connection;

protected:
    CTL_CursorCmdExpl(CTL_Connection& con,
                      const string& cursor_name,
                      const string& query,
                      unsigned int fetch_size);
    virtual ~CTL_CursorCmdExpl(void);

protected:
    virtual CDB_Result* OpenCursor(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateBlob(unsigned int item_num, CDB_Stream& data,
                            bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool dump_results = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool CloseCursor(void);

private:
    CTL_CursorResultExpl* GetResultSet(void) const;
    void SetResultSet(CTL_CursorResultExpl* res);
    void ClearResultSet(void);
    const string GetCombinedQuery(void) const
    {
        return m_CombinedQuery;
    }

private:
    bool x_AssignParams(void);
    I_BlobDescriptor* x_GetBlobDescriptor(unsigned int item_num);

    unique_ptr<CTL_LangCmd>          m_LCmd;
    unique_ptr<CTL_CursorResultExpl> m_Res;
    string                         m_CombinedQuery;
    unsigned int                   m_FetchSize;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BCPInCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_BCPInCmd : CTL_CmdBase
{
    friend class CTL_Connection;

protected:
    CTL_BCPInCmd(CTL_Connection& con,
                 const string& table_name);
    virtual ~CTL_BCPInCmd(void);

    void Close(void);

protected:
    virtual void SetHints(CTempString hints);
    virtual void AddHint(CDB_BCPInCmd::EBCP_Hints hint, unsigned int value);
    virtual void AddOrderHint(CTempString columns);
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool CommitBCPTrans(void);
    virtual bool Cancel(void);
    virtual bool EndBCP(void);
    virtual int RowCount(void) const;

    CS_RETCODE CheckSF(CS_RETCODE rc, const char* msg, unsigned int msg_num);
    CS_RETCODE CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);
    CS_RETCODE CheckSentSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num);

private:
    bool x_AssignParams(void);
    bool x_IsUnicodeClientAPI(void) const;
    CTempString x_GetStringValue(unsigned int i);
    CS_BLKDESC* x_GetSybaseCmd(void) const
    {
        return m_Cmd;
    }
    void x_BlkSetHints(void);

private:
    struct SBcpBind {
        CTempVarChar varchar;
        CS_INT      datalen;
        CS_SMALLINT indicator;
        char        buffer[80];
    };

    typedef map<CDB_BCPInCmd::EBCP_Hints, string>  THintsMap;

    CS_BLKDESC*         m_Cmd;
    AutoArray<SBcpBind> m_BindArray;
    int                 m_RowCount;
    THintsMap           m_Hints;

    AutoArray<SBcpBind>& GetBind(void)
    {
        unsigned int param_num = GetBindParamsImpl().NofParams();
        _ASSERT(param_num);

        if (!m_BindArray) {
            m_BindArray = AutoArray<SBcpBind>(param_num);
        }

        return m_BindArray;
    }

};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_SendDataCmd : CTL_LRCmd, public impl::CSendDataCmd
{
    friend class CTL_Connection;

protected:
    CTL_SendDataCmd(CTL_Connection& conn,
                    I_BlobDescriptor& descr_in,
                    size_t nof_bytes,
                    bool log_it,
                    bool dump_results);
    virtual ~CTL_SendDataCmd(void);

    void Close(void);
    virtual bool Cancel(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    CDB_BlobDescriptor::ETDescriptorType m_DescrType;
#ifdef FTDS_IN_USE
    string m_SQL;
    string m_UTF8Fragment;
#endif
    bool m_DumpResults;
    bool m_UseUpdateWrite;
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
    friend class CTL_CursorCmdExpl;
    friend class CTL_CursorResultExpl;

protected:
    CTL_RowResult(CS_COMMAND* cmd, CTL_Connection& conn);
    virtual ~CTL_RowResult(void);

    void Close(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0,
							I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_BlobDescriptor* GetBlobDescriptor(void);
    virtual bool            SkipItem(void);

    I_BlobDescriptor*       GetBlobDescriptor(int item_num);

    CS_RETCODE my_ct_get_data(CS_COMMAND* cmd,
                              CS_INT item,
                              CS_VOID* buffer,
                              CS_INT buflen,
                              CS_INT *outlen,
                              bool& is_null);
    CDB_Object* GetItemInternal(
		I_Result::EGetItem policy, 
		CS_COMMAND* cmd, 
		CS_INT item_no, 
		CS_DATAFMT& fmt,
		CDB_Object* item_buf
		);
    CS_COMMAND* x_GetSybaseCmd(void) const { return m_Cmd; }
    CS_RETCODE Check(CS_RETCODE rc)
    {
        _ASSERT(m_Connect);
        return m_Connect->Check(rc);
    }

    //
    CTL_Connection& GetConnection(void)
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }
    const CTL_Connection& GetConnection(void) const
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }

    //
    const CTL_Connection::TDbgInfo& GetDbgInfo(void) const
    {
        return GetConnection().GetDbgInfo();
    }

    static EDB_Type ConvDataType_Ctlib2DBAPI(const CS_DATAFMT& fmt);

    void SetCurrentItemNum(int num)
    {
        m_CurrItem = num;
    }
    int GetCurrentItemNum(void) const
    {
        return m_CurrItem;
    }
    void IncCurrentItemNum(void)
    {
        ++m_CurrItem;
    }

protected:
    enum ENullValue {eNullUnknown, eIsNull, eIsNotNull};

    bool IsDead(void) const
    {
        return GetConnection().IsDead();
    }

    const CDBParams* GetLastParams(void) const 
    {
        return m_Connect ? m_Connect->GetLastParams() : NULL;
    }

    void CheckIsDead(void) const
    {
        if (IsDead()) {
            NCBI_DATABASE_THROW_ANNOTATED(CDB_ClientEx, "Connection has died.",
                                          122011, eDiag_Error, GetDbgInfo(),
                                          GetConnection(), GetLastParams());
        } else {
            m_Connect->DeferTimeout();
        }
    }

    // data
    CTL_Connection*         m_Connect;
    CS_COMMAND*             m_Cmd;
    int                     m_CurrItem;
    bool                    m_EOR;
    AutoArray<CS_DATAFMT>   m_ColFmt;
    int                     m_BindedCols;
    AutoArray<CS_VOID*>     m_BindItem;
    AutoArray<CS_INT>       m_Copied;
    AutoArray<CS_SMALLINT>  m_Indicator;
    AutoArray<ENullValue>   m_NullValue;
    unsigned char           m_BindBuff[2048];
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_ComputeResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

////////////////////////////////////////////////////////////////////////////////
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


////////////////////////////////////////////////////////////////////////////////
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


////////////////////////////////////////////////////////////////////////////////
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


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorResult :  public CTL_RowResult
{
    friend class CTL_Cmd;

public:
    const string& GetCursorName(void) const { return m_CursorName; }
    void RegisterDescriptor(CTL_CursorBlobDescriptor& desc)
    { m_Descriptors.insert(&desc); }
    void UnregisterDescriptor(CTL_CursorBlobDescriptor& desc)
    { m_Descriptors.erase(&desc); }

protected:
    CTL_CursorResult(CS_COMMAND* pCmd, CTL_Connection& conn,
                     const string& cursor_name) :
    CTL_RowResult(pCmd, conn), m_CursorName(cursor_name)
    {
    }
    virtual ~CTL_CursorResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
    virtual bool        SkipItem(void);
    virtual bool        Fetch(void);

    void x_InvalidateDescriptors(void);

private:
    set<CTL_CursorBlobDescriptor*> m_Descriptors;
    string                         m_CursorName;
};

////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorResultExpl : public CTL_CursorResult
{
    friend class CTL_CursorCmdExpl;
    friend struct default_delete<CTL_CursorResultExpl>;

protected:
    CTL_CursorResultExpl(CTL_LangCmd* cmd, const string& cursor_name);
    virtual ~CTL_CursorResultExpl(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0,
							I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_BlobDescriptor* GetBlobDescriptor(void)
    {
        return GetBlobDescriptor(m_CurItemNo);
    }
    I_BlobDescriptor*       GetBlobDescriptor(int item_num);
    virtual bool            SkipItem(void);

private:
    CDB_Result* GetResultSet(void) const;
    void SetResultSet(CDB_Result* res);
    void ClearResultSet(void);
    void DumpResultSet(void);
    void FetchAllResultSet(void);

    CTL_LangCmd& GetCmd(void)
    {
        _ASSERT(m_Cmd);
        return *m_Cmd;
    }
    CTL_LangCmd const& GetCmd(void) const
    {
        _ASSERT(m_Cmd);
        return *m_Cmd;
    }

    void ClearFields(void);

private:
    // data
    CTL_LangCmd*           m_Cmd;
    // CTL_RowResult* m_Res;
    CDB_Result*             m_Res;
    vector<CDB_Object*>     m_Fields;
    vector<I_BlobDescriptor*> m_BlobDescrs;
    int                     m_CurItemNo;
    size_t                  m_ReadBytes;
    void*                   m_ReadBuffer;
    string                  m_CursorName;
};


/////////////////////////////////////////////////////////////////////////////
namespace ctlib {
    inline
    CS_RETCODE Connection::CheckWhileOpening(CS_RETCODE rc)
    {
        return GetCTLConn().CheckWhileOpening(rc);
    }
}

inline
const CTL_Connection::TDbgInfo& CTL_Connection::GetDbgInfo(void) const {
    return m_ActiveCmd ? m_ActiveCmd->GetDbgInfo() : CConnection::GetDbgInfo();
}

inline
const CDBParams* CTL_Connection::GetLastParams(void) const {
    return m_ActiveCmd ? m_ActiveCmd->GetLastParams() : NULL;
}

inline
unsigned int CTL_Connection::GetRowsInCurrentBatch(void) const
{
    return m_ActiveCmd ? m_ActiveCmd->GetRowsInCurrentBatch() : 0U;
}

#ifdef FTDS_IN_USE
inline
CTL_Connection::CAsyncCancelGuard::CAsyncCancelGuard(CTL_Connection& conn)
    : m_Conn(conn)
{
    CFastMutexGuard LOCK(conn.m_AsyncCancelMutex);
    conn.m_OrigTimeout          = conn.GetTimeout();
    conn.m_BaseTimeout          = 0;
#  if NCBI_FTDS_VERSION >= 95
    conn.m_TotalTimeout         = 0;
#  endif
    conn.m_AsyncCancelAllowed   = true;
    conn.m_AsyncCancelRequested = false;
#  if NCBI_FTDS_VERSION < 95
    if (conn.m_OrigTimeout == 0) {
        conn.SetTimeout(kMax_Int / 1000);
    }
#  endif
}

inline
CTL_Connection::CAsyncCancelGuard::~CAsyncCancelGuard(void)
{
    CFastMutexGuard LOCK(m_Conn.m_AsyncCancelMutex);
    m_Conn.SetTimeout(m_Conn.m_OrigTimeout);
    m_Conn.m_AsyncCancelAllowed = false;
}
#endif

#ifdef USE_STRUCT_CS_VARCHAR
inline
void CTL_CmdBase::CTempVarChar::SetValue(const CTempString& s)
{
    m_Data.reset
        (static_cast<CS_VARCHAR*>
         (malloc(sizeof(CS_VARCHAR) - sizeof(m_Data->str) + s.size())));
    m_Data->len = static_cast<CS_SMALLINT>(s.size());
    memcpy(m_Data->str, s.data(), s.size());
}

inline
CTempString CTL_CmdBase::CTempVarChar::GetValue(void) const
{
    return CTempString((char*)m_Data.get(),
                       sizeof(CS_VARCHAR) - sizeof(m_Data->str) + m_Data->len);
}
#else
inline
void CTL_CmdBase::CTempVarChar::SetValue(const CTempString& s)
{
    m_Data = s;
}

inline
CTempString CTL_CmdBase::CTempVarChar::GetValue(void) const
{
    return m_Data;
}
#endif


inline
CTL_Connection&
CTL_CmdBase::GetConnection(void)
{
    return static_cast<CTL_Connection&>(GetConnImpl());
}

inline
const CTL_Connection&
CTL_CmdBase::GetConnection(void) const
{
    return static_cast<CTL_Connection&>(GetConnImpl());
}

inline
void
CTL_CmdBase::DropCmd(impl::CCommand& cmd)
{
    GetConnection().DropCmd(cmd);
}

inline
bool
CTL_CmdBase::x_SendData(I_BlobDescriptor& desc, CDB_Stream& img, bool log_it)
{
    return GetConnection().x_SendData(desc, img, log_it);
}

inline
CDB_SendDataCmd*
CTL_CmdBase::ConnSendDataCmd (I_BlobDescriptor& desc,
                              size_t          data_size,
                              bool            log_it,
                              bool            dump_results)
{
    return GetConnection().SendDataCmd(desc, data_size, log_it, dump_results);
}


/////////////////////////////////////////////////////////////////////////////
inline
CS_COMMAND*
CTL_Cmd::x_GetSybaseCmd(void) const
{
    return m_Cmd;
}

inline
void
CTL_Cmd::SetSybaseCmd(CS_COMMAND* cmd)
{
    m_Cmd = cmd;
}

inline
bool
CTL_Cmd::ProcessResultInternal(CS_INT res_type)
{
    return GetConnection().x_ProcessResultInternal(x_GetSybaseCmd(), res_type);
}

inline
bool
CTL_Cmd::HaveResult(void) const
{
    return (m_Res != NULL);
}

inline
CTL_RowResult*
CTL_Cmd::MakeCursorResult(void)
{
    return new CTL_CursorResult(x_GetSybaseCmd(), GetConnection(),
                                GetCmdName());
}

inline
CTL_RowResult*
CTL_Cmd::MakeRowResult(void)
{
    return new CTL_RowResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult*
CTL_Cmd::MakeParamResult(void)
{
    return new CTL_ParamResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult*
CTL_Cmd::MakeComputeResult(void)
{
    return new CTL_ComputeResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult*
CTL_Cmd::MakeStatusResult(void)
{
    return new CTL_StatusResult(x_GetSybaseCmd(), GetConnection());
}

inline
CTL_RowResult&
CTL_Cmd::GetResult(void)
{
    _ASSERT(HaveResult());
    return *m_Res;
}

inline
void
CTL_Cmd::DeleteResult(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
void
CTL_Cmd::DeleteResultInternal(void)
{
    MarkEndOfReply();
    if ( HaveResult() ) {
        DeleteResult();
    }
}

inline
void CTL_Cmd::MarkEndOfReply(void)
{
    // to prevent ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_CURRENT) call:
    if (HaveResult()) {
        m_Res->m_EOR = true;
    }
    GetConnection().m_CancelRequested = false;
}

/////////////////////////////////////////////////////////////////////////////
inline
CTL_CursorResultExpl*
CTL_CursorCmdExpl::GetResultSet(void) const
{
    return m_Res.get();
}

inline
void
CTL_CursorCmdExpl::SetResultSet(CTL_CursorResultExpl* res)
{
    m_Res.reset(res);
}

inline
void
CTL_CursorCmdExpl::ClearResultSet(void)
{
    m_Res.reset(NULL);
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_BlobDescriptor::
//

#define CTL_BLOB_DESCRIPTOR_TYPE_MAGNUM 0xc00
#define CTL_BLOB_DESCRIPTOR_TYPE_CURSOR 0xc01

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_BlobDescriptor
    : public I_BlobDescriptor
{
    friend class CTL_RowResult;
    friend class CTL_CursorResultExpl;
    friend class CTL_Connection;
    friend class CTL_CursorCmd;
    friend class CTL_CursorCmdExpl;
    friend class CTL_SendDataCmd;

public:
    virtual int DescriptorType(void) const;
    virtual ~CTL_BlobDescriptor(void);

protected:
    CTL_BlobDescriptor(void);
    CTL_BlobDescriptor& operator=(const CTL_BlobDescriptor& desc);

    CS_IODESC               m_Desc;
    /// Set only when m_Desc lacks a valid textptr
    unique_ptr<CDB_Exception> m_Context;
};

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTL_CursorBlobDescriptor
    : public CDB_BlobDescriptor
{
public:
    CTL_CursorBlobDescriptor(CTL_CursorResult& cursor_result,
                             const string& table_name,
                             const string& column_name,
                             CS_INT datatype);
    ~CTL_CursorBlobDescriptor();

    int DescriptorType(void) const;

    void Invalidate(void) { m_CursorResult = NULL; }

    bool IsValid(void) const { return m_CursorResult != NULL; }

private:
    CTL_CursorResult* m_CursorResult;
};

// historical names
#define CTL_ITDESCRIPTOR_TYPE_MAGNUM CTL_BLOB_DESCRIPTOR_TYPE_MAGNUM
#define CTL_ITDESCRIPTOR_TYPE_CURSOR CTL_BLOB_DESCRIPTOR_TYPE_CURSOR
typedef CTL_BlobDescriptor       CTL_ITDescriptor;
typedef CTL_CursorBlobDescriptor CTL_CursorITDescriptor;

#ifdef FTDS_IN_USE
} // namespace NCBI_NS_FTDS_CTLIB
#endif

END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_CTLIB___INTERFACES__HPP */

