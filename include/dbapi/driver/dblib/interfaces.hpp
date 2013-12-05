#ifndef DBAPI_DRIVER_DBLIB___INTERFACES__HPP
#define DBAPI_DRIVER_DBLIB___INTERFACES__HPP

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
 * File Description:  Driver for Sybase DBLib server
 *
 */

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/util/parameters.hpp>
// #include <dbapi/driver/util/handle_stack.hpp>

#include <corelib/ncbi_safe_static.hpp>

#include <sybfront.h>
#include <sybdb.h>
#include <syberror.h>


BEGIN_NCBI_SCOPE


#define DEFAULT_TDS_VERSION DBVERSION_46

class CDBLibContext;
class CDBL_Connection;
class CDBL_Cmd;
class CDBL_LangCmd;
class CDBL_RPCCmd;
class CDBL_CursorCmd;
class CDBL_BCPInCmd;
class CDBL_SendDataCmd;
class CDBL_RowResult;
class CDBL_ParamResult;
class CDBL_ComputeResult;
class CDBL_StatusResult;
class CDBL_CursorResult;
class CDBL_BlobResult;


const unsigned int kDBLibMaxNameLen = 128 + 4;

/////////////////////////////////////////////////////////////////////////////
///
///  CDBLibContext::
///

class CDblibContextRegistry;

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBLibContext :
    public impl::CDriverContext,
    public impl::CWinSock
{
    friend class CDB_Connection;

public:
    CDBLibContext(DBINT version = DEFAULT_TDS_VERSION);
    virtual ~CDBLibContext(void);

public:
    ///
    /// GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    ///

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual bool IsAbleTo(ECapability cpb) const;

    virtual string GetDriverName(void) const;

    ///
    /// DBLIB specific functionality
    ///

    /// the following methods are optional (driver will use the default
    /// values if not called)
    /// the values will affect the new connections only

    /// Deprecated. Use SetApplicationName instead.
    NCBI_DEPRECATED
    virtual void DBLIB_SetApplicationName(const string& a_name);
    /// Deprecated. Use SetHostName instead.
    NCBI_DEPRECATED
    virtual void DBLIB_SetHostName(const string& host_name);
    virtual void DBLIB_SetPacketSize(int p_size);
    virtual bool DBLIB_SetMaxNofConns(int n);

public:
    static  int  DBLIB_dberr_handler(DBPROCESS*    dblink,   int     severity,
                                     int           dberr,    int     oserr,
                                     const string& dberrstr,
                                     const string& oserrstr);
    static  void DBLIB_dbmsg_handler(DBPROCESS*    dblink,   DBINT   msgno,
                                     int           msgstate, int     severity,
                                     const string& msgtxt,
                                     const string& srvname,
                                     const string& procname,
                                     int           line);

    CDBLibContext* GetContext(void) const;

public:
    int GetTDSVersion(void) const;

    //
    unsigned int GetBufferSize(void) const
    {
        return m_BufferSize;
    }
    void SetBufferSize(unsigned int size)
    {
        m_BufferSize = size;
    }

    short GetPacketSize(void) const
    {
        return m_PacketSize;
    }

    template <typename T>
    T Check(T rc)
    {
        CheckFunctCall();
        return rc;
    }

    void CheckFunctCall(void);

protected:
    virtual impl::CConnection* MakeIConnection(const CDBConnParams& params);

private:
    short                  m_PacketSize;
    int                    m_TDSVersion;
    CDblibContextRegistry* m_Registry;
    unsigned int           m_BufferSize;


    void x_AddToRegistry(void);
    void x_RemoveFromRegistry(void);
    void x_SetRegistry(CDblibContextRegistry* registry);
    void x_Close(bool delete_conn = true);
    bool x_SafeToFinalize(void) const;

    friend class CDblibContextRegistry;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_Connection::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_Connection : public impl::CConnection
{
    friend class CDBLibContext;
    friend class CDB_Connection;
    friend class CDBL_Cmd;
    friend class CDBL_Result;
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_CursorCmd;
    friend class CDBL_BCPInCmd;
    friend class CDBL_SendDataCmd;

protected:
    CDBL_Connection( CDBLibContext& cntx, const CDBConnParams& params);
    virtual ~CDBL_Connection(void);

protected:
    virtual bool IsAlive(void);

    virtual CDB_LangCmd*     LangCmd(const string&       lang_query);
    virtual CDB_RPCCmd*      RPC(const string&           rpc_name);
    virtual CDB_BCPInCmd*    BCPIn(const string&         table_name);
    virtual CDB_CursorCmd*   Cursor(const string&        cursor_name,
                                    const string&        query,
                                    unsigned int         batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true,
                                         bool            dump_results = true);

    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    virtual bool Refresh(void);
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;

    /// abort the connection
    /// Attention: it is not recommended to use this method unless you absolutely have to.
    /// The expected implementation is - close underlying file descriptor[s] without
    /// destroing any objects associated with a connection.
    /// Returns: true - if succeed
    ///          false - if not
    virtual bool Abort(void);

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void);

    virtual void SetTimeout(size_t nof_secs);
    virtual void SetCancelTimeout(size_t nof_secs);

private:
    bool x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it = true);
    I_ITDescriptor* x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in);
    RETCODE x_Results(DBPROCESS* pLink);

    CDBLibContext& GetDBLibCtx(void) const
    {
        _ASSERT(m_DBLibCtx);
        return *m_DBLibCtx;
    }
    DBPROCESS* GetDBLibConnection(void) const { return m_Link; }

    template <typename T>
    T Check(T rc)
    {
        CheckFunctCall();
        return rc;
    }
    template <typename T>
    T Check(T rc, const string& extra_msg)
    {
        CheckFunctCall(extra_msg);
        return rc;
    }
    RETCODE CheckDead(RETCODE rc);

    void CheckFunctCall(void);
    void CheckFunctCall(const string& extra_msg);

    string GetDbgInfo(void) const;

    string GetBaseDbgInfo(void) const
    {
        return " " + GetExecCntxInfo();
    }

private:
    const CDBParams* GetBindParams(void) const;

    CDBLibContext* m_DBLibCtx;
    LOGINREC*      m_Login;
    DBPROCESS*     m_Link;
    CDBL_Cmd*      m_ActiveCmd;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_Cmd::
///

class CDBL_Cmd : public impl::CBaseCmd
{
    friend class CDBL_Connection;
public:
    CDBL_Cmd(CDBL_Connection& conn,
             DBPROCESS*       cmd,
             const string&    query);
    CDBL_Cmd(CDBL_Connection& conn,
             DBPROCESS*       cmd,
             const string&    cursor_name,
             const string&    query);
    ~CDBL_Cmd(void);

    CDBL_Connection& GetConnection(void)
    {
        return static_cast<CDBL_Connection&>(GetConnImpl());
    }
    const CDBL_Connection& GetConnection(void) const
    {
        return static_cast<const CDBL_Connection&>(GetConnImpl());
    }

    DBPROCESS* GetCmd(void) const
    {
        _ASSERT(m_Cmd);
        return m_Cmd;
    }

    RETCODE Check(RETCODE rc)
    {
        return GetConnection().Check(rc, GetDbgInfo(false));
    }
    void CheckFunctCall(void)
    {
        GetConnection().CheckFunctCall(GetDbgInfo(false));
    }

    void SetExecCntxInfo(const string& info)
    {
        m_ExecCntxInfo = info;
    }
    const string& GetExecCntxInfo(void) const
    {
        return m_ExecCntxInfo;
    }

    string GetDbgInfo(bool want_space = true) const
    {
        return (want_space ? string(" ") : kEmptyStr) + GetExecCntxInfo() + " "
            + GetConnection().GetExecCntxInfo();
    }

protected:
//     bool             m_HasFailed;
    int              m_RowCount;
    string           m_ExecCntxInfo;

private:
    DBPROCESS*       m_Cmd;
    bool             m_IsActive;
};

/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_LangCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_LangCmd : CDBL_Cmd
{
    friend class CDBL_Connection;

protected:
    CDBL_LangCmd(CDBL_Connection& conn,
                 DBPROCESS* cmd,
                 const string& lang_query);
    virtual ~CDBL_LangCmd(void);

protected:
    virtual bool Send(void);
    virtual bool Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    impl::CResult* GetResultSet(void) const;
    void SetResultSet(impl::CResult* res);
    void ClearResultSet(void);

private:
    bool x_AssignParams(void);

    impl::CResult*   m_Res;
    unsigned int     m_Status;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CTL_RPCCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_RPCCmd : CDBL_Cmd
{
    friend class CDBL_Connection;

protected:
    CDBL_RPCCmd(CDBL_Connection& con, DBPROCESS* cmd, const string& proc_name);
    ~CDBL_RPCCmd(void);

protected:
    virtual CDBParams& GetBindParams(void);

    virtual bool Send(void);
    virtual bool Cancel(void);
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual int  RowCount(void) const;

private:
    impl::CResult* GetResultSet(void) const;
    void SetResultSet(impl::CResult* res);
    void ClearResultSet(void);

private:
    bool x_AssignParams(char* param_buff);

    impl::CResult*   m_Res;
    unsigned int     m_Status;

    auto_ptr<CDBParams> m_InParams;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_CursorCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_CursorCmd : CDBL_Cmd
{
    friend class CDBL_Connection;

protected:
    CDBL_CursorCmd(CDBL_Connection& con, DBPROCESS* cmd,
                   const string& cursor_name, const string& query);
    virtual ~CDBL_CursorCmd(void);

protected:
    virtual CDB_Result* OpenCursor(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool dump_results = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool CloseCursor(void);

private:
    CDBL_CursorResult* GetResultSet(void) const;
    void SetResultSet(CDBL_CursorResult* res);
    void ClearResultSet(void);
    const string GetCombinedQuery(void) const
    {
        return m_CombinedQuery;
    }

private:
    bool x_AssignParams(void);
    I_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CDB_LangCmd*       m_LCmd;
    CDBL_CursorResult* m_Res;
    string             m_CombinedQuery;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_BCPInCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_BCPInCmd : CDBL_Cmd
{
    friend class CDBL_Connection;

protected:
    CDBL_BCPInCmd(CDBL_Connection& con, DBPROCESS* cmd, const string& table_name);
    ~CDBL_BCPInCmd(void);

protected:
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool CommitBCPTrans(void);
    virtual bool Cancel(void);
    virtual bool EndBCP(void);
    virtual int RowCount(void) const;

private:
    bool x_AssignParams(void* pb);

    bool m_HasTextImage;
    bool m_WasBound;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_SendDataCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_SendDataCmd :
    CDBL_Cmd,
    public impl::CSendDataCmd
{
    friend class CDBL_Connection;

protected:
    CDBL_SendDataCmd(CDBL_Connection& con, DBPROCESS* cmd, size_t nof_bytes);
    ~CDBL_SendDataCmd(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual bool   Cancel(void);
    virtual int    RowCount(void) const
        { return -1; }
};



/////////////////////////////////////////////////////////////////////////////
///
///  SDBL_ColDescr::
///

struct SDBL_ColDescr
{
    DBINT      max_length;
    EDB_Type   data_type;
    string     col_name;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_Result
///

class CDBL_Result
{
public:
    CDBL_Result(CDBL_Connection& conn,
                DBPROCESS*       cmd
                ) :
    m_Connect(&conn),
    m_Cmd(cmd)
    {
    }
    ~CDBL_Result(void)
    {
    }

protected:
    CDBL_Connection& GetConnection(void)
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }
    CDBL_Connection const& GetConnection(void) const
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }
    DBPROCESS* GetCmd(void) const
    {
        _ASSERT(m_Cmd);
        return m_Cmd;
    }

    template <typename T>
    T Check(T rc)
    {
        return GetConnection().Check(rc);
    }
    void CheckFunctCall(void)
    {
        GetConnection().CheckFunctCall();
    }
    EDB_Type GetDataType(int n);
    CDB_Object* GetItemInternal(
        I_Result::EGetItem policy,
        int item_no,
        SDBL_ColDescr* fmt,
        CDB_Object* item_buff
        );
    EDB_Type RetGetDataType(int n);
    CDB_Object* RetGetItem(int item_no,
                           SDBL_ColDescr* fmt,
                           CDB_Object* item_buff);
    EDB_Type AltGetDataType(int id, int n);
    CDB_Object* AltGetItem(int id,
                           int item_no,
                           SDBL_ColDescr* fmt,
                           CDB_Object* item_buff);

    string GetDbgInfo(void) const
    {
        return " " + GetConnection().GetExecCntxInfo();
    }

    const CDBParams* GetBindParams(void) const 
    {
        return m_Connect ? m_Connect->GetBindParams() : NULL;
    }

private:
    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
};

/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ResultBase::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ResultBase :
    public CDBL_Result,
    public impl::CResult
{
protected:
    CDBL_ResultBase(
            CDBL_Connection& conn,
            DBPROCESS* cmd,
            unsigned int* res_status = NULL
            );
    virtual ~CDBL_ResultBase(void);

protected:
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual bool            SkipItem(void);

protected:
    int            m_CurrItem;
    size_t         m_Offset;
    int            m_CmdNum;
    SDBL_ColDescr* m_ColFmt;
    unsigned int*  m_ResStatus;
    bool           m_EOR;
};

/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_RowResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_RowResult : CDBL_ResultBase
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;

protected:
    CDBL_RowResult(CDBL_Connection& conn,
                   DBPROCESS* cmd,
                   unsigned int* res_status,
                   bool need_init = true);
    virtual ~CDBL_RowResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_BlobResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_BlobResult : CDBL_Result, public impl::CResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;

protected:
    CDBL_BlobResult(CDBL_Connection& conn, DBPROCESS* cmd);
    virtual ~CDBL_BlobResult(void);

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
    virtual bool            SkipItem(void);

    // data
    int           m_CurrItem;
    bool          m_EOR;
    int           m_CmdNum;
    char          m_Buff[2048];
    SDBL_ColDescr m_ColFmt;
    int           m_BytesInBuffer;
    int           m_ReadedBytes;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ParamResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ParamResult : public CDBL_ResultBase
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;

protected:
    CDBL_ParamResult(CDBL_Connection& conn, DBPROCESS* cmd, int nof_params);
    virtual ~CDBL_ParamResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);

    // data
    bool m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ComputeResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ComputeResult : public CDBL_ResultBase
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;

protected:
    CDBL_ComputeResult(CDBL_Connection& conn,
                       DBPROCESS* cmd,
                       unsigned int* res_stat);
    virtual ~CDBL_ComputeResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);

    // data
    int  m_ComputeId;
    bool m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_StatusResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_StatusResult : CDBL_Result, public impl::CResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;

protected:
    CDBL_StatusResult(CDBL_Connection& conn, DBPROCESS* cmd);
    virtual ~CDBL_StatusResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const ;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0,
                            I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    virtual bool            SkipItem(void);

    // data
    int    m_Val;
    size_t m_Offset;
    bool   m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_CursorResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_CursorResult : CDBL_Result, public impl::CResult
{
    friend class CDBL_CursorCmd;

protected:
    CDBL_CursorResult(CDBL_Connection& conn, CDB_LangCmd* cmd);
    virtual ~CDBL_CursorResult(void);

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

private:
    CDB_Result* GetResultSet(void) const;
    void SetResultSet(CDB_Result* res);
    void ClearResultSet(void);
    void DumpResultSet(void);
    void FetchAllResultSet(void);

    CDB_LangCmd& GetCmd(void)
    {
        _ASSERT(m_Cmd);
        return *m_Cmd;
    }
    CDB_LangCmd const& GetCmd(void) const
    {
        _ASSERT(m_Cmd);
        return *m_Cmd;
    }

private:
    // data
    CDB_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ITDescriptor::
///

#define CDBL_ITDESCRIPTOR_TYPE_MAGNUM 0xd00

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ITDescriptor :
    CDBL_Result,
    public I_ITDescriptor
{
    friend class CDBL_RowResult;
    friend class CDBL_BlobResult;
    friend class CDBL_Connection;
    friend class CDBL_CursorCmd;

protected:
    CDBL_ITDescriptor(CDBL_Connection& conn,
                      DBPROCESS* m_link,
                      int col_num);
    CDBL_ITDescriptor(CDBL_Connection& conn,
                      DBPROCESS* m_link,
                      const CDB_ITDescriptor& inp_d);

public:
    virtual ~CDBL_ITDescriptor(void);

public:
    virtual int DescriptorType(void) const;

private:
    bool x_MakeObjName(DBCOLINFO* col_info);

protected:
    // data
    string              m_ObjName;
    DBBINARY            m_TxtPtr[DBTXPLEN];
    DBBINARY            m_TimeStamp[DBTXTSLEN];
    bool                m_TxtPtr_is_NULL;
    bool                m_TimeStamp_is_NULL;
};


/////////////////////////////////////////////////////////////////////////////
inline
string CDBL_Connection::GetDbgInfo(void) const {
    return m_ActiveCmd ? m_ActiveCmd->GetDbgInfo() : GetBaseDbgInfo();
}

inline
const CDBParams* CDBL_Connection::GetBindParams(void) const {
    // Calling CDBL_RPCCmd::GetBindParams within an error handler can
    // deadlock, but specifically testing for it fails because
    // dynamic_cast only reliably handles types known to the main
    // executable, as opposed to plugins.  No other derived classes
    // override GetBindParams anyway.
    return m_ActiveCmd ? &m_ActiveCmd->impl::CBaseCmd::GetBindParams() : NULL;
}

inline
impl::CResult*
CDBL_LangCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void
CDBL_LangCmd::SetResultSet(impl::CResult* res)
{
    m_Res = res;
}

inline
void
CDBL_LangCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
CDBL_CursorResult*
CDBL_CursorCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void
CDBL_CursorCmd::SetResultSet(CDBL_CursorResult* res)
{
    m_Res = res;
}

inline
void
CDBL_CursorCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
CDB_Result*
CDBL_CursorResult::GetResultSet(void) const
{
    return m_Res;
}

inline
void
CDBL_CursorResult::SetResultSet(CDB_Result* res)
{
    m_Res = res;
}

inline
void
CDBL_CursorResult::ClearResultSet(void)
{
   delete m_Res;
    m_Res = NULL;
}

inline
void
CDBL_CursorResult::DumpResultSet(void)
{
    SetResultSet( m_Cmd->Result() );
    FetchAllResultSet();
}

inline
void
CDBL_CursorResult::FetchAllResultSet(void)
{
    if (GetResultSet()) {
        while (GetResultSet()->Fetch())
            continue;
        ClearResultSet();
    }
}

inline
impl::CResult*
CDBL_RPCCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void
CDBL_RPCCmd::SetResultSet(impl::CResult* res)
{
    m_Res = res;
}

inline
void
CDBL_RPCCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES__HPP */

