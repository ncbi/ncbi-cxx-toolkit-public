#ifndef DBAPI_DRIVER_MSDBLIB___INTERFACES__HPP
#define DBAPI_DRIVER_MSDBLIB___INTERFACES__HPP

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
 * File Description:  Driver for Microsoft DBLib server
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>


#ifdef NCBI_OS_MSWIN

#include <windows.h>
#define DBNTWIN32                   /* must be defined before sqlfront.h */
#include <sqlfront.h>               /* must be after windows.h */
# if defined(_MSC_VER)  &&  (_MSC_VER > 1200)
typedef const BYTE *LPCBYTE;    /* MSVC7 headers lucks typedef for LPCBYTE */
# endif
#include <sqldb.h>

#define DBVERSION_UNKNOWN DBUNKNOWN
#define DBVERSION_46 DBVER42
#define DBVERSION_100 DBVER60
#define DBCOLINFO    DBCOL
// Other constants and types remapped in interfaces_p.hpp


BEGIN_NCBI_SCOPE

class CMSDBLibContext;
class CMSDBL_Connection;
class CMSDBL_LangCmd;
class CMSDBL_RPCCmd;
class CMSDBL_CursorCmd;
class CMSDBL_BCPInCmd;
class CMSDBL_SendDataCmd;
class CMSDBL_RowResult;
class CMSDBL_ParamResult;
class CMSDBL_ComputeResult;
class CMSDBL_StatusResult;
class CMSDBL_CursorResult;
class CMSDBL_BlobResult;


const unsigned int kDBLibMaxNameLen = 128 + 4;


/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBLibContext::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBLibContext : public I_DriverContext
{
    friend class CDB_Connection;

public:
    CMSDBLibContext(DBINT version = DBVERSION_46);

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

    virtual ~CMSDBLibContext();


    //
    // DBLIB specific functionality
    //

    // the following methods are optional (driver will use the default
    // values if not called)
    // the values will affect the new connections only

    virtual void DBLIB_SetApplicationName(const string& a_name);
    virtual void DBLIB_SetHostName(const string& host_name);
    virtual void DBLIB_SetPacketSize(int p_size);
    virtual bool DBLIB_SetMaxNofConns(int n);
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

private:
    static CMSDBLibContext* m_pDBLibContext;
    string                m_AppName;
    string                m_HostName;
    short                 m_PacketSize;
    LOGINREC*             m_Login;

    DBPROCESS* x_ConnectToServer(const string&   srv_name,
                                 const string&   user_name,
                                 const string&   passwd,
                                 TConnectionMode mode);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_Connection : public I_Connection
{
    friend class CMSDBLibContext;
    friend class CDB_Connection;
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_CursorCmd;
    friend class CMSDBL_BCPInCmd;
    friend class CMSDBL_SendDataCmd;

protected:
    CMSDBL_Connection(CMSDBLibContext* cntx, DBPROCESS* con,
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

    virtual ~CMSDBL_Connection();

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
    RETCODE x_Results(DBPROCESS* pLink);


    DBPROCESS*      m_Link;
    CMSDBLibContext*  m_Context;
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
//  CMSDBL_LangCmd::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_LangCmd : public I_LangCmd
{
    friend class CMSDBL_Connection;
protected:
    CMSDBL_LangCmd(CMSDBL_Connection* conn, DBPROCESS* cmd,
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

    virtual ~CMSDBL_LangCmd();

private:
    bool x_AssignParams();

    CMSDBL_Connection* m_Connect;
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

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_RPCCmd : public I_RPCCmd
{
    friend class CMSDBL_Connection;
protected:
    CMSDBL_RPCCmd(CMSDBL_Connection* con, DBPROCESS* cmd,
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
    virtual void DumpResults();
    virtual void SetRecompile(bool recompile = true);
    virtual void Release();

    ~CMSDBL_RPCCmd();

private:
    bool x_AssignParams(char* param_buff);

    CMSDBL_Connection* m_Connect;
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
//  CMSDBL_CursorCmd::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_CursorCmd : public I_CursorCmd
{
    friend class CMSDBL_Connection;
protected:
    CMSDBL_CursorCmd(CMSDBL_Connection* con, DBPROCESS* cmd,
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

    virtual ~CMSDBL_CursorCmd();

private:
    bool x_AssignParams();
    I_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CMSDBL_Connection*   m_Connect;
    DBPROCESS*         m_Cmd;
    string             m_Name;
    CDB_LangCmd*       m_LCmd;
    string             m_Query;
    CDB_Params         m_Params;
    bool               m_IsOpen;
    bool               m_HasFailed;
    bool               m_IsDeclared;
    CMSDBL_CursorResult* m_Res;
    int                m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_BCPInCmd::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_BCPInCmd : public I_BCPInCmd
{
    friend class CMSDBL_Connection;
protected:
    CMSDBL_BCPInCmd(CMSDBL_Connection* con, DBPROCESS* cmd,
                  const string& table_name, unsigned int nof_columns);

    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow();
    virtual bool CompleteBatch();
    virtual bool Cancel();
    virtual bool CompleteBCP();
    virtual void Release();

    ~CMSDBL_BCPInCmd();

private:
    bool x_AssignParams(void* pb);

    CMSDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    bool             m_HasTextImage;
    bool             m_WasBound;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_SendDataCmd::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_SendDataCmd : public I_SendDataCmd {
    friend class CMSDBL_Connection;
protected:
    CMSDBL_SendDataCmd(CMSDBL_Connection* con, DBPROCESS* cmd, size_t nof_bytes);

    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release();

    ~CMSDBL_SendDataCmd();

private:
    CMSDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    size_t           m_Bytes2go;
};



/////////////////////////////////////////////////////////////////////////////
//
//  SDBL_ColDescr::
//

struct SDBL_ColDescr
{
    DBINT      max_length;
    EDB_Type   data_type;
    string     col_name;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_RowResult::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_RowResult : public I_Result
{
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_Connection;
protected:
    CMSDBL_RowResult(DBPROCESS* cmd, unsigned int* res_status,
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

    virtual ~CMSDBL_RowResult();

    // data
    DBPROCESS*     m_Cmd;
    int            m_CurrItem;
    bool           m_EOR;
    unsigned int   m_NofCols;
    int            m_CmdNum;
    unsigned int*  m_ResStatus;
    size_t         m_Offset;
    SDBL_ColDescr* m_ColFmt;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_BlobResult::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_BlobResult : public I_Result
{
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_Connection;
protected:
    CMSDBL_BlobResult(DBPROCESS* cmd);

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

    virtual ~CMSDBL_BlobResult();

    // data
    DBPROCESS*    m_Cmd;
    int           m_CurrItem;
    bool          m_EOR;
    int           m_CmdNum;
    char          m_Buff[2048];
    SDBL_ColDescr m_ColFmt;
    int           m_BytesInBuffer;
    int           m_ReadedBytes;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_ParamResult::
//  CMSDBL_ComputeResult::
//  CMSDBL_StatusResult::
//  CMSDBL_CursorResult::
//

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_ParamResult : public CMSDBL_RowResult
{
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_Connection;
protected:
    CMSDBL_ParamResult(DBPROCESS* cmd, int nof_params);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CMSDBL_ParamResult();

    // data
    bool m_1stFetch;
};


class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_ComputeResult : public CMSDBL_RowResult
{
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_Connection;
protected:
    CMSDBL_ComputeResult(DBPROCESS* cmd, unsigned int* res_stat);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CMSDBL_ComputeResult();

    // data
    int  m_ComputeId;
    bool m_1stFetch;
};


class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_StatusResult : public I_Result
{
    friend class CMSDBL_LangCmd;
    friend class CMSDBL_RPCCmd;
    friend class CMSDBL_Connection;
protected:
    CMSDBL_StatusResult(DBPROCESS* cmd);
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

    virtual ~CMSDBL_StatusResult();

    // data
    int    m_Val;
    size_t m_Offset;
    bool   m_1stFetch;
};


class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_CursorResult : public I_Result
{
    friend class CMSDBL_CursorCmd;
protected:
    CMSDBL_CursorResult(CDB_LangCmd* cmd);
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

    virtual ~CMSDBL_CursorResult();

    // data
    CDB_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSDBL_ITDescriptor::
//

#define CMSDBL_ITDESCRIPTOR_TYPE_MAGNUM 0xd01

class NCBI_DBAPIDRIVER_MSDBLIB_EXPORT CMSDBL_ITDescriptor : public I_ITDescriptor
{
    friend class CMSDBL_RowResult;
    friend class CMSDBL_BlobResult;
    friend class CMSDBL_Connection;
    friend class CMSDBL_CursorCmd;
public:
    virtual int DescriptorType() const;
    virtual ~CMSDBL_ITDescriptor();

protected:
    CMSDBL_ITDescriptor(DBPROCESS* m_link, int col_num);
    CMSDBL_ITDescriptor(DBPROCESS* m_link, const CDB_ITDescriptor& inp_d);

    // data
    string   m_ObjName;
    DBBINARY m_TxtPtr[DBTXPLEN];
    DBBINARY m_TimeStamp[DBTXTSLEN];
    bool     m_TxtPtr_is_NULL;
    bool     m_TimeStamp_is_NULL;
};

/////////////////////////////////////////////////////////////////////////////
extern NCBI_DBAPIDRIVER_MSDBLIB_EXPORT const string kDBAPI_MSDBLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_MSDBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

END_NCBI_SCOPE

#endif  /* NCBI_OS_MSWIN */
#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.8  2005/02/23 21:33:00  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.7  2004/07/06 14:26:48  gorelenk
 * Changed typedef for LPCBYTE to be consistent with Platform SDK headers
 *
 * Revision 1.6  2004/05/18 19:22:08  gorelenk
 * Conditionaly added typedef for LPCBYTE missed in MSVC7 headers .
 *
 * Revision 1.5  2003/07/17 20:42:47  soussov
 * connections pool improvements
 *
 * Revision 1.4  2003/06/06 18:43:16  soussov
 * Removes SetPacketSize()
 *
 * Revision 1.3  2003/06/05 15:56:19  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.2  2003/02/13 15:43:18  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_MSDBLIB_EXPORT for class definitions
 *
 * Revision 1.1  2002/07/02 16:02:25  soussov
 * initial commit
 *
 * ===========================================================================
 */
