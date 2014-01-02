#ifndef DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP


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
 *
 */

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>


BEGIN_NCBI_SCOPE

class CDB_Connection;

namespace impl
{

////////////////////////////////////////////////////////////////////////////
class CDriverContext;
class CLangCmd;
class CRPCCmd;
class CBCPInCmd;
class CCursorCmd;
class CSendDataCmd;
class CCommand;

/////////////////////////////////////////////////////////////////////////////
///
///  CConnection::
///

class NCBI_DBAPIDRIVER_EXPORT CConnection : public I_ConnectionExtra
{
    friend class impl::CDriverContext;
    friend class ncbi::CDB_Connection; // Because of AttachTo

public:
    CConnection(CDriverContext& dc,
                const CDBConnParams& params,
                bool isBCPable  = false
                );
    virtual ~CConnection(void);

    CDB_ResultProcessor* GetResultProcessor(void) const
    {
        return m_ResProc;
    }

    CDriverContext& GetCDriverContext(void)
    {
        _ASSERT(m_DriverContext);
        return *m_DriverContext;
    }
    const CDriverContext& GetCDriverContext(void) const
    {
        _ASSERT(m_DriverContext);
        return *m_DriverContext;
    }

    bool IsMultibyteClientEncoding(void) const;
    EEncoding GetClientEncoding(void) const;

    void SetExtraMsg(const string& msg) const
    {
        m_ExtraMsg = msg;
    }
    const string& GetExtraMsg(void) const
    {
        return m_ExtraMsg;
    }


    const string& GetServerName(void) const
    {
        return ServerName();
    }
    const string& GetUserName(void) const
    {
        return UserName();
    }
    const string& GetPassword(void) const
    {
        return Password();
    }
    const string& GetDatabaseName(void) const;

    const string& GetExecCntxInfo(void) const
    {
        return m_ExecCntxInfo;
    }

public:
    /// Check out if connection is alive (this function doesn't ping the server,
    /// it just checks the status of connection which was set by the last
    /// i/o operation)
    virtual bool IsAlive(void) = 0;
    bool IsOpeningFinished(void) const
    {
        return m_OpenFinished;
    }
    void FinishOpening(void)
    {
        m_OpenFinished = true;
    }
    bool IsValid(void) const
    {
        return m_Valid;
    }

    /// These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    /// create and return a "command" object, register it for later use with
    /// this (and only this!) connection.
    /// On error, an exception will be thrown (they never return NULL!).
    /// It is the user's responsibility to delete the returned "command" object.

    /// Language command
    virtual CDB_LangCmd* LangCmd(const string& lang_query) = 0;
    /// Remote procedure call
    virtual CDB_RPCCmd* RPC(const string& rpc_name) = 0;
    /// "Bulk copy in" command
    virtual CDB_BCPInCmd* BCPIn(const string& table_name) = 0; /// Cursor
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int  batch_size = 1) = 0;
    /// "Send-data" command
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true,
                                         bool            dump_results = true) = 0;

    /// Shortcut to send text and image to the server without using the
    /// "Send-data" command (SendDataCmd)
    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true) = 0;

    virtual void SetDatabaseName(const string& name);

    /// Reset the connection to the "ready" state (cancel all active commands)
    virtual bool Refresh(void) = 0;
    void Invalidate(void)
    {
        m_Valid = false;
    }

    /// Get the server name, user login name, and password
    const string& ServerName(void) const;
    Uint4         Host(void) const;
    Uint2         Port(void) const;
    const string& UserName(void) const;
    const string& Password(void) const;

    /// Get the bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const = 0;

    /// Check if this connection is a reusable one
    bool IsReusable(void) const;

    /// Find out which connection pool this connection belongs to
    const string& PoolName(void) const;

    /// Get pointer to the driver context
    I_DriverContext* Context(void) const;

    /// Put the message handler into message handler stack
    void PushMsgHandler(CDB_UserHandler* h,
                                EOwnership ownership = eNoOwnership);

    /// Remove the message handler (and all above it) from the stack
    void PopMsgHandler(CDB_UserHandler* h);

    CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);

    /// abort the connection
    /// Attention: it is not recommended to use this method unless you absolutely have to.
    /// The expected implementation is - close underlying file descriptor[s] without
    /// destroing any objects associated with a connection.
    /// Returns: true - if succeed
    ///          false - if not
    virtual bool Abort(void) = 0;

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void) = 0;

    virtual void SetTimeout(size_t nof_secs) = 0;
    virtual void SetCancelTimeout(size_t nof_secs) = 0;
    virtual void SetTextImageSize(size_t nof_bytes);

    virtual TSockHandle GetLowLevelHandle(void) const;

    virtual string GetDriverName(void) const;

    CDBConnParams::EServerType GetServerType(void);

    //
    CDBConnParams::EServerType CalculateServerType(CDBConnParams::EServerType server_type);
    
protected:
    /// These methods to allow the children of CConnection to create
    /// various command-objects
    CDB_LangCmd*     Create_LangCmd     (CBaseCmd&     lang_cmd    );
    CDB_RPCCmd*      Create_RPCCmd      (CBaseCmd&     rpc_cmd     );
    CDB_BCPInCmd*    Create_BCPInCmd    (CBaseCmd&     bcpin_cmd   );
//     CDB_CursorCmd*   Create_CursorCmd   (CCursorCmd&   cursor_cmd  );
    CDB_CursorCmd*   Create_CursorCmd   (CBaseCmd&     cursor_cmd  );
    CDB_SendDataCmd* Create_SendDataCmd (CSendDataCmd& senddata_cmd);

protected:
    void Release(void);
    static CDB_Result* Create_Result(impl::CResult& result);

    const CDBHandlerStack& GetMsgHandlers(void) const
    {
        _ASSERT(m_MsgHandlers.GetSize() > 0);
        return m_MsgHandlers;
    }
    CDBHandlerStack& GetMsgHandlers(void)
    {
        _ASSERT(m_MsgHandlers.GetSize() > 0);
        return m_MsgHandlers;
    }

    void DropCmd(impl::CCommand& cmd);
    void DeleteAllCommands(void);


    //
    void AttachTo(CDB_Connection* interface);
    void ReleaseInterface(void);

    //
    void DetachResultProcessor(void);

    void CheckCanOpen(void);
    void MarkClosed(void);

    //
    bool IsBCPable(void) const
    {
        return m_BCPable;
    }
    bool HasSecureLogin(void) const
    {
        return m_SecureLogin;
    }

    void SetExecCntxInfo(const string& info)
    {
        m_ExecCntxInfo = info;
    }

    void SetServerType(CDBConnParams::EServerType type)
    {
        m_ServerType = type;
    }

    const CDBHandlerStack& GetOpeningMsgHandlers(void) const
    {
        _ASSERT( !m_OpenFinished );
        return m_OpeningMsgHandlers;
    }

private:
    typedef deque<impl::CCommand*>  TCommandList;

    CDriverContext*                 m_DriverContext;
    CDBHandlerStack                 m_MsgHandlers;
    CDBHandlerStack                 m_OpeningMsgHandlers;
    TCommandList                    m_CMDs;
    CInterfaceHook<CDB_Connection>  m_Interface;
    CDB_ResultProcessor*            m_ResProc;

    CDBConnParams::EServerType      m_ServerType;
    bool                            m_ServerTypeIsKnown;

    const string   m_Server;
    const Uint4    m_Host;
    const Uint2    m_Port;
    const string   m_User;
    const string   m_Passwd;
    string         m_Database;
    const string   m_Pool;
    const bool     m_Reusable;
    bool           m_OpenFinished;
    bool           m_Valid;
    const bool     m_BCPable; //< Does this connection support BCP (It is related to Context, actually)
    const bool     m_SecureLogin;
    bool           m_Opened;
    string         m_ExecCntxInfo;
    mutable string m_ExtraMsg;
};

} // namespace impl

END_NCBI_SCOPE



#endif  /* DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP */
