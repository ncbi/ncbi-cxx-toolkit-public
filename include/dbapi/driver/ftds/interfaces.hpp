#ifndef DBAPI_DRIVER_FTDS___INTERFACES__HPP
#define DBAPI_DRIVER_FTDS___INTERFACES__HPP

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
 * File Description:  Driver for TDS server
 *
 */

#include <corelib/ncbistl.hpp>

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>

#include <dbapi/driver/ftds/ncbi_ftds_rename_sybdb.h>

#include <cspublic.h>
#include <sybfront.h>
#include <sybdb.h>
#include <syberror.h>

BEGIN_NCBI_SCOPE


// some defines
#ifndef SYBEFCON
#  define SYBEFCON        20002   /* SQL Server connection failed. */
#endif
#ifndef SYBETIME
#  define SYBETIME        20003   /* SQL Server connection timed out. */
#endif
#ifndef SYBECONN
#  define SYBECONN        20009   /* Unable to connect socket -- SQL Server is
                                   * unavailable or does not exist.
                                   */
#endif
#ifndef EXINFO
#  define EXINFO          1       /* informational, non-error */
#endif
#ifndef EXUSER
#  define EXUSER          2       /* user error */
#endif

#ifndef EXNONFATAL
#  define EXNONFATAL      3       /* non-fatal error */
#endif
#ifndef EXCONVERSION
#  define EXCONVERSION    4       /* Error in DB-LIBRARY data conversion. */
#endif
#ifndef EXSERVER
#  define EXSERVER        5       /* The Server has returned an error flag. */
#endif
#ifndef EXTIME
#  define EXTIME          6       /* We have exceeded our timeout period while
                                   * waiting for a response from the Server -
                                   * the DBPROCESS is still alive.
                                   */
#endif
#ifndef EXPROGRAM
#  define EXPROGRAM       7       /* coding error in user program */
#endif
#ifndef INT_EXIT
#  define INT_EXIT        0
#endif
#ifndef INT_CONTINUE
#  define INT_CONTINUE    1
#endif
#ifndef INT_CANCEL
#  define INT_CANCEL      2
#endif
#ifndef INT_TIMEOUT
#  define INT_TIMEOUT     3
#endif


typedef unsigned char CS_BIT;
#ifndef DBBIT
#  define DBBIT CS_BIT
#endif

#if defined(NCBI_FTDS)
#  if NCBI_FTDS == 7
extern "C" {
    BYTE *dbgetuserdata(DBPROCESS *dbproc);
    RETCODE dbsetuserdata(DBPROCESS *dbproc, BYTE *ptr);
    RETCODE dbmoretext(DBPROCESS *dbproc, DBINT size, BYTE *text);
    DBTYPEINFO *dbcoltypeinfo(DBPROCESS *dbproc, int column);
    int DBCURCMD(DBPROCESS *dbproc);
    STATUS dbreadtext(DBPROCESS *dbproc, void *buf, DBINT bufsize);
    int dbrettype(DBPROCESS *dbproc,int retnum);
}
#  endif
#endif

class CTDSContext;
class CTDS_Connection;
class CTDS_LangCmd;
class CTDS_RPCCmd;
class CTDS_CursorCmd;
class CTDS_BCPInCmd;
class CTDS_SendDataCmd;
class CTDS_RowResult;
class CTDS_ParamResult;
class CTDS_ComputeResult;
class CTDS_StatusResult;
class CTDS_CursorResult;
class CTDS_BlobResult;


const unsigned int kTDSMaxNameLen = 128 + 4;


/////////////////////////////////////////////////////////////////////////////
//
//  CTDSContext::
//

I_DriverContext* FTDS_CreateContext(const map<string,string>* attr = 0);

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDSContext : public I_DriverContext
{
    friend class CDB_Connection;
    friend I_DriverContext* FTDS_CreateContext(const map<string,string>* attr);
    friend class CDbapiFtdsCF2;

public:
    CTDSContext(DBINT version = DBVERSION_UNKNOWN);

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

    virtual bool IsAbleTo(ECapability cpb) const;

    virtual ~CTDSContext();


    //
    // TDS specific functionality
    //

    // the following methods are optional (driver will use the
    // default values if not called)
    // the values will affect the new connections only

    virtual void TDS_SetApplicationName(const string& a_name);
    virtual void TDS_SetHostName(const string& host_name);
    virtual void TDS_SetPacketSize(int p_size);
    virtual bool TDS_SetMaxNofConns(int n);

    unsigned int TDS_GetTimeout(void) {
        return m_Timeout;
    }

    static  int  TDS_dberr_handler(DBPROCESS*    dblink,   int     severity,
                                   int           dberr,    int     oserr,
                                   const string& dberrstr,
                                   const string& oserrstr);
    static  void TDS_dbmsg_handler(DBPROCESS*    dblink,   DBINT   msgno,
                                   int           msgstate, int     severity,
                                   const string& msgtxt,
                                   const string& srvname,
                                   const string& procname,
                                   int           line);

private:
    static CTDSContext* m_pTDSContext;
    string              m_AppName;
    string              m_HostName;
    short               m_PacketSize;
    LOGINREC*           m_Login;
    unsigned int        m_LoginTimeout;
    unsigned int        m_Timeout;
    DBINT               m_TDSVersion;

    DBPROCESS* x_ConnectToServer(const string&   srv_name,
                                 const string&   user_name,
                                 const string&   passwd,
                                 TConnectionMode mode);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_Connection : public I_Connection
{
    friend class CTDSContext;
    friend class CDB_Connection;
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_CursorCmd;
    friend class CTDS_BCPInCmd;
    friend class CTDS_SendDataCmd;

protected:
    CTDS_Connection(CTDSContext* cntx, DBPROCESS* con,
                    bool reusable, const string& pool_name);

    virtual bool IsAlive();

    virtual CDB_LangCmd*     LangCmd(const string&       lang_query,
                                     unsigned int        nof_params = 0);
    virtual CDB_RPCCmd*      RPC(const string&           rpc_name,
                                 unsigned int            nof_args);
    virtual CDB_BCPInCmd*    BCPIn(const string&         table_name,
                                   unsigned int          nof_columns);
    virtual CDB_CursorCmd*   Cursor(const string&        cursor_name,
                                    const string&        query,
                                    unsigned int         nof_params,
                                    unsigned int         batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
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

    virtual ~CTDS_Connection();

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

    void TDS_SetTimeout(void) {
        m_Link->tds_socket->timeout= (TDS_INT)(m_Context->TDS_GetTimeout());
    }

    RETCODE x_Results(DBPROCESS* pLink);

    DBPROCESS*      m_Link;
    CTDSContext*    m_Context;
    CPointerPot     m_CMDs;
    CDBHandlerStack m_MsgHandlers;
    string          m_Server;
    string          m_User;
    string          m_Passwd;
    string          m_Pool;
    bool            m_Reusable;
    bool            m_BCPAble;
    bool            m_SecureLogin;
    CDB_ResultProcessor* m_ResProc;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_LangCmd::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_LangCmd : public I_LangCmd
{
    friend class CTDS_Connection;
protected:
    CTDS_LangCmd(CTDS_Connection* conn, DBPROCESS* cmd,
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

    virtual ~CTDS_LangCmd();

private:
    bool x_AssignParams();

    CTDS_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    string           m_Query;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    I_Result*        m_Res;
    int              m_RowCount;
    unsigned int     m_Status;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_RPCCmd : public I_RPCCmd
{
    friend class CTDS_Connection;
protected:
    CTDS_RPCCmd(CTDS_Connection* con, DBPROCESS* cmd,
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
    virtual bool HasFailed() const ;
    virtual int  RowCount() const;
    virtual void SetRecompile(bool recompile = true);
    virtual void DumpResults();
    virtual void Release();

    ~CTDS_RPCCmd();

private:
    bool x_AddParamValue(string& cmd, const CDB_Object& param);
    bool x_AssignOutputParams();
    bool x_AssignParams();

    CTDS_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    string           m_Query;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    bool             m_Recompile;
    I_Result*        m_Res;
    int              m_RowCount;
    unsigned int     m_Status;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_CursorCmd::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_CursorCmd : public I_CursorCmd
{
    friend class CTDS_Connection;
protected:
    CTDS_CursorCmd(CTDS_Connection* con, DBPROCESS* cmd,
                   const string& cursor_name, const string& query,
                   unsigned int nof_params);

    virtual bool BindParam(const string& param_name, CDB_Object* pVal);
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

    virtual ~CTDS_CursorCmd();

private:
    bool x_AssignParams();
    I_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CTDS_Connection*   m_Connect;
    DBPROCESS*         m_Cmd;
    string             m_Name;
    CDB_LangCmd*       m_LCmd;
    string             m_Query;
    CDB_Params         m_Params;
    bool               m_IsOpen;
    bool               m_HasFailed;
    bool               m_IsDeclared;
    CTDS_CursorResult* m_Res;
    int                m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_BCPInCmd::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_BCPInCmd : public I_BCPInCmd
{
    friend class CTDS_Connection;
protected:
    CTDS_BCPInCmd(CTDS_Connection* con, DBPROCESS* cmd,
                  const string& table_name, unsigned int nof_columns);

    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow();
    virtual bool CompleteBatch();
    virtual bool Cancel();
    virtual bool CompleteBCP();
    virtual void Release();

    ~CTDS_BCPInCmd();

private:
    bool x_AssignParams(void* pb);

    CTDS_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    bool             m_HasTextImage;
    bool             m_WasBound;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_SendDataCmd::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_SendDataCmd : public I_SendDataCmd
{
    friend class CTDS_Connection;
protected:
    CTDS_SendDataCmd(CTDS_Connection* con, DBPROCESS* cmd, size_t nof_bytes);

    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release();

    ~CTDS_SendDataCmd();

private:
    CTDS_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    size_t           m_Bytes2go;
};



/////////////////////////////////////////////////////////////////////////////
//
//  STDS_ColDescr::
//

struct STDS_ColDescr
{
    DBINT      max_length;
    EDB_Type   data_type;
    string     col_name;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_RowResult::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_RowResult : public I_Result
{
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_Connection;
protected:
    CTDS_RowResult(DBPROCESS* cmd, unsigned int* res_status,
                   bool need_init = true);

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

    virtual ~CTDS_RowResult();

    // data
    DBPROCESS*     m_Cmd;
    int            m_CurrItem;
    bool           m_EOR;
    unsigned int   m_NofCols;
    int            m_CmdNum;
    unsigned int*  m_ResStatus;
    DBINT          m_Offset;
    STDS_ColDescr* m_ColFmt;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_BlobResult::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_BlobResult : public I_Result
{
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_Connection;
protected:
    CTDS_BlobResult(DBPROCESS* cmd);

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

    virtual ~CTDS_BlobResult();

    // data
    DBPROCESS*    m_Cmd;
    int           m_CurrItem;
    bool          m_EOR;
    int           m_CmdNum;
    char          m_Buff[2048];
    STDS_ColDescr m_ColFmt;
    int           m_BytesInBuffer;
    int           m_ReadedBytes;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_ParamResult::
//  CTDS_ComputeResult::
//  CTDS_StatusResult::
//  CTDS_CursorResult::
//

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_ParamResult : public CTDS_RowResult
{
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_Connection;
protected:
    CTDS_ParamResult(DBPROCESS* cmd, int nof_params);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CTDS_ParamResult();

    // data
    bool m_1stFetch;
};


class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_ComputeResult : public CTDS_RowResult
{
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_Connection;
protected:
    CTDS_ComputeResult(DBPROCESS* cmd, unsigned int* res_stat);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CTDS_ComputeResult();

    // data
    int  m_ComputeId;
    bool m_1stFetch;
};


class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_StatusResult : public I_Result
{
    friend class CTDS_LangCmd;
    friend class CTDS_RPCCmd;
    friend class CTDS_Connection;
protected:
    CTDS_StatusResult(DBPROCESS* cmd);
    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const ;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CTDS_StatusResult();

    // data
    int  m_Val;
    int  m_Offset;
    bool m_1stFetch;
};


class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_CursorResult : public I_Result
{
    friend class CTDS_CursorCmd;
protected:
    CTDS_CursorResult(CDB_LangCmd* cmd);
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

    virtual ~CTDS_CursorResult();

    // data
    CDB_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_ITDescriptor::
//
#define CTDS_ITDESCRIPTOR_TYPE_MAGNUM 0xf00

class NCBI_DBAPIDRIVER_FTDS_EXPORT CTDS_ITDescriptor : public I_ITDescriptor
{
    friend class CTDS_RowResult;
    friend class CTDS_BlobResult;
    friend class CTDS_Connection;
    friend class CTDS_CursorCmd;
public:
    virtual int DescriptorType() const;
    virtual ~CTDS_ITDescriptor();
private:
    // bool x_MakeObjName(DBCOLINFO* col_info);
protected:
    CTDS_ITDescriptor(DBPROCESS* m_link, int col_num);
    CTDS_ITDescriptor(DBPROCESS* m_link, const CDB_ITDescriptor& inp_d);

    // data
    string   m_ObjName;
    DBBINARY m_TxtPtr[DBTXPLEN];
    DBBINARY m_TimeStamp[DBTXTSLEN];
    bool     m_TxtPtr_is_NULL;
    bool     m_TimeStamp_is_NULL;
};

/////////////////////////////////////////////////////////////////////////////
extern NCBI_DBAPIDRIVER_FTDS_EXPORT const string kDBAPI_FTDS_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_FTDS_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_FTDS___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.19  2005/02/23 21:33:11  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.18  2004/12/20 16:20:48  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.17  2004/12/10 15:26:41  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.16  2004/10/16 20:48:42  vakatov
 * +  #include <dbapi/driver/ftds/ncbi_ftds_rename_sybdb.h>
 * to allow the renaming (and the use of the renamed) DBLIB symbols in
 * the built-in FreeTDS.
 *
 * Revision 1.15  2003/12/18 19:00:55  soussov
 * makes FTDS_CreateContext return an existing context if reuse_context option is set
 *
 * Revision 1.14  2003/07/17 20:43:33  soussov
 * connections pool improvements
 *
 * Revision 1.13  2003/06/05 15:55:47  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.12  2003/04/01 20:28:20  vakatov
 * Temporarily rollback to R1.10 -- until more backward-incompatible
 * changes (in CException) are ready to commit (to avoid breaking the
 * compatibility twice).
 *
 * Revision 1.10  2003/01/06 20:09:01  vakatov
 * cosmetics (identation)
 *
 * Revision 1.9  2002/12/13 21:56:41  vakatov
 * Use the newly defined #NCBI_FTDS value "7"
 *
 * Revision 1.8  2002/12/05 22:39:39  soussov
 * Adapted for TDS8
 *
 * Revision 1.7  2002/05/29 22:03:50  soussov
 * Makes BlobResult read ahead
 *
 * Revision 1.6  2002/03/28 00:45:18  vakatov
 * CTDS_CursorCmd::  use CTDS_CursorResult rather than I_Result (to fix access)
 *
 * Revision 1.5  2002/03/26 15:29:30  soussov
 * new image/text operations added
 *
 * Revision 1.4  2002/01/28 19:57:13  soussov
 * removing the long query timout
 *
 * Revision 1.3  2002/01/14 20:38:01  soussov
 * timeout support for tds added
 *
 * Revision 1.2  2001/11/06 17:58:04  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:20  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
