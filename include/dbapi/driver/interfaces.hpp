#ifndef DBAPI_DRIVER___INTERFACES__HPP
#define DBAPI_DRIVER___INTERFACES__HPP

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
 * File Description:  Data Server interfaces
 *
 */

#include <dbapi/driver/types.hpp>
#include <dbapi/driver/exception.hpp>

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif

#include <map>


/** @addtogroup DbInterfaces
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class I_BaseCmd;

class I_DriverContext;

class I_Connection;
class I_Result;
class I_LangCmd;
class I_RPCCmd;
class I_BCPInCmd;
class I_CursorCmd;
class I_SendDataCmd;

class CDB_Connection;
class CDB_Result;
class CDB_LangCmd;
class CDB_RPCCmd;
class CDB_BCPInCmd;
class CDB_CursorCmd;
class CDB_SendDataCmd;
class CDB_ResultProcessor;

class IConnValidator;

namespace impl
{
    class CResult;
    class CBaseCmd;
    class CSendDataCmd;
}


/////////////////////////////////////////////////////////////////////////////
///
///  I_ITDescriptor::
///
/// Image or Text descriptor.
///

class NCBI_DBAPIDRIVER_EXPORT I_ITDescriptor
{
public:
    virtual int DescriptorType(void) const = 0;
    virtual ~I_ITDescriptor(void);
};


/// auto_ptr<I_ITDescriptor> does the same job ...
class NCBI_DBAPIDRIVER_EXPORT C_ITDescriptorGuard
{
public:
    NCBI_DEPRECATED_CTOR(C_ITDescriptorGuard(I_ITDescriptor* d));
    NCBI_DEPRECATED ~C_ITDescriptorGuard(void);

private:
    I_ITDescriptor* m_D;
};



/////////////////////////////////////////////////////////////////////////////
///
///  EDB_ResType::
///
/// Type of result set
///

enum EDB_ResType {
    eDB_RowResult,
    eDB_ParamResult,
    eDB_ComputeResult,
    eDB_StatusResult,
    eDB_CursorResult
};


/////////////////////////////////////////////////////////////////////////////
///
///  I_BaseCmd::
///
/// Abstract base class for most "command" interface classes.
///

class NCBI_DBAPIDRIVER_EXPORT I_BaseCmd
{
public:
    I_BaseCmd(void);
    // Destructor
    virtual ~I_BaseCmd(void);

public:
    /// Send command to the server
    virtual bool Send(void) = 0;
    /// Implementation-specific.
    virtual bool WasSent(void) const = 0;

    /// Cancel the command execution
    virtual bool Cancel(void) = 0;
    /// Implementation-specific.
    virtual bool WasCanceled(void) const = 0;

    /// Get result set
    virtual CDB_Result* Result(void) = 0;
    virtual bool HasMoreResults(void) const = 0;

    // Check if command has failed
    virtual bool HasFailed(void) const = 0;

    /// Get the number of rows affected by the command
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount(void) const = 0;

    /// Dump the results of the command
    /// if result processor is installed for this connection, it will be called for
    /// each result set
    virtual void DumpResults(void)= 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  I_LangCmd::
///  I_RPCCmd::
///  I_BCPInCmd::
///  I_CursorCmd::
///  I_SendDataCmd::
///
/// "Command" interface classes.
///


class NCBI_DBAPIDRIVER_EXPORT I_LangCmd : public I_BaseCmd
{
public:
    I_LangCmd(void);
    virtual ~I_LangCmd(void);

protected:
    /// Add more text to the language command
    virtual bool More(const string& query_text) = 0;

    /// Bind cmd parameter with name "name" to the object pointed by "value"
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

    /// Set cmd parameter with name "name" to the object pointed by "value"
    virtual bool SetParam(const string& name, CDB_Object* param_ptr) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_RPCCmd : public I_BaseCmd
{
public:
    I_RPCCmd(void);
    virtual ~I_RPCCmd(void);

protected:
    /// Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr,
                           bool out_param = false) = 0;

    /// Setting
    virtual bool SetParam(const string& name, CDB_Object* param_ptr,
                          bool out_param = false) = 0;

    /// Set the "recompile before execute" flag for the stored proc
    /// Implementation-specific.
    virtual void SetRecompile(bool recompile = true) = 0;

    /// Get a name of the procedure.
    virtual const string& GetProcName(void) const = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_BCPInCmd
{
public:
    I_BCPInCmd(void);
    virtual ~I_BCPInCmd(void);

protected:
    /// Binding
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr) = 0;

    /// Send row to the server
    virtual bool SendRow(void) = 0;

    /// Complete batch -- to store all rows transferred by far in this batch
    /// into the table
    virtual bool CompleteBatch(void) = 0;

    /// Cancel the BCP command
    virtual bool Cancel(void) = 0;

    /// Complete the BCP and store all rows transferred in last batch into
    /// the table
    virtual bool CompleteBCP(void) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_CursorCmd
{
public:
    I_CursorCmd(void);
    virtual ~I_CursorCmd(void);

protected:
    /// Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

    /// Open the cursor.
    /// Return NULL if cursor resulted in no data.
    /// Throw exception on error.
    virtual CDB_Result* Open(void) = 0;

    /// Update the last fetched row.
    /// NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query) = 0;

    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                 bool log_it = true) = 0;

    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true) = 0;
    /// Delete the last fetched row.
    /// NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
    virtual bool Delete(const string& table_name) = 0;

    /// Get the number of fetched rows
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount(void) const = 0;

    /// Close the cursor.
    /// Return FALSE if the cursor is closed already (or not opened yet)
    virtual bool Close(void) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_SendDataCmd
{
public:
    I_SendDataCmd(void);
    virtual ~I_SendDataCmd(void);

protected:
    /// Send chunk of data to the server.
    /// Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* pChunk, size_t nofBytes) = 0;
    virtual bool Cancel(void) = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  I_Result::
///

class NCBI_DBAPIDRIVER_EXPORT I_Result
{
public:
    I_Result(void);
    virtual ~I_Result(void);

public:
    /// Get type of the result
    virtual EDB_ResType ResultType(void) const = 0;

    /// Get # of items (columns) in the result
    virtual unsigned int NofItems(void) const = 0;

    /// Get name of a result item.
    /// Return NULL if "item_num" >= NofItems().
    virtual const char* ItemName(unsigned int item_num) const = 0;

    /// Get size (in bytes) of a result item.
    /// Return zero if "item_num" >= NofItems().
    virtual size_t ItemMaxSize(unsigned int item_num) const = 0;

    /// Get datatype of a result item.
    /// Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const = 0;

    /// Fetch next row
    virtual bool Fetch(void) = 0;

    /// Return current item number we can retrieve (0,1,...)
    /// Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo(void) const = 0;

    /// Return number of columns in the recordset.
    virtual int GetColumnNum(void) const = 0;

    /// Get a result item (you can use either GetItem or ReadItem).
    /// If "item_buf" is not NULL, then use "*item_buf" (its type should be
    /// compatible with the type of retrieved item!) to retrieve the item to;
    /// otherwise allocate new "CDB_Object".
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0) = 0;

    /// Read a result item body (for text/image mostly).
    /// Return number of successfully read bytes.
    /// Set "*is_null" to TRUE if the item is <NULL>.
    /// Throw an exception on any error.
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0) = 0;

    /// Get a descriptor for text/image column (for SendData).
    /// Return NULL if this result doesn't (or can't) have img/text descriptor.
    /// NOTE: you need to call ReadItem (maybe even with buffer_size == 0)
    ///       before calling this method!
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void) = 0;

    /// Skip result item
    virtual bool SkipItem(void) = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  I_DriverContext::
///

class NCBI_DBAPIDRIVER_EXPORT I_DriverContext
{
protected:
    I_DriverContext(void);

public:
    virtual ~I_DriverContext(void);


    /// Connection mode
    enum EConnectionMode {
        fBcpIn             = 0x1,
        fPasswordEncrypted = 0x2,
        fDoNotConnect      = 0x4   //< Use just connections from NotInUse pool
        // all driver-specific mode flags > 0x100
    };
    typedef int TConnectionMode;  //< holds a binary OR of "EConnectionMode"

    struct NCBI_DBAPIDRIVER_EXPORT SConnAttr {
        SConnAttr(void);

        string          srv_name;
        string          user_name;
        string          passwd;
        TConnectionMode mode;
        bool            reusable;
        string          pool_name;
    };

    /// Set login and connection timeouts.
    /// NOTE:  if "nof_secs" is zero or is "too big" (depends on the underlying
    ///        DB API), then set the timeout to infinite.
    /// Return FALSE on error.
    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);

    /// Methods below may be overrided in order to return a value taken directly
    /// from the API.
    virtual unsigned int GetLoginTimeout(void) const { return m_LoginTimeout; }
    virtual unsigned int GetTimeout     (void) const { return m_Timeout;      }

    /// Set maximal size for Text and Image objects. Text and Image objects
    /// exceeding this size will be truncated.
    /// Return FALSE on error (e.g. if "nof_bytes" is too big).
    virtual bool SetMaxTextImageSize(size_t nof_bytes) = 0;

    /// Create new connection to specified server (within this context).
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    ///
    /// pool_name - name of a pool to which this connection is going to belong.
    ///
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    virtual CDB_Connection* Connect
    (const string&   srv_name,
     const string&   user_name,
     const string&   passwd,
     TConnectionMode mode,
     bool            reusable  = false,
     const string&   pool_name = kEmptyStr) = 0;

    /// Create new connection to specified server (within this context).
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    ///
    /// pool_name - name of a pool to which this connection is going to belong.
    ///
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    virtual CDB_Connection* ConnectValidated
    (const string&   srv_name,
     const string&   user_name,
     const string&   passwd,
     IConnValidator& validator,
     TConnectionMode mode      = 0,
     bool            reusable  = false,
     const string&   pool_name = kEmptyStr) = 0;

    /// Return number of currently open connections in this context.
    /// If "srv_name" is not NULL, then return # of conn. open to that server.
    virtual unsigned int NofConnections(const string& srv_name  = kEmptyStr,
                                        const string& pool_name = kEmptyStr)
        const = 0;

    /// Add message handler "h" to process 'context-wide' (not bound
    /// to any particular connection) error messages.
    virtual void PushCntxMsgHandler(CDB_UserHandler* h,
                            EOwnership ownership = eNoOwnership) = 0;

    /// Remove message handler "h" and all handlers above it in the stack
    virtual void PopCntxMsgHandler(CDB_UserHandler* h) = 0;

    /// Add `per-connection' err.message handler "h" to the stack of default
    /// handlers which are inherited by all newly created connections.
    virtual void PushDefConnMsgHandler(CDB_UserHandler* h,
                               EOwnership ownership = eNoOwnership) = 0;

    /// Remove `per-connection' mess. handler "h" and all above it in the stack.
    virtual void PopDefConnMsgHandler(CDB_UserHandler* h) = 0;

    /// Report if the driver supports this functionality
    enum ECapability {
        eBcp,
        eReturnITDescriptors,
        eReturnComputeResults
    };
    virtual bool IsAbleTo(ECapability cpb) const = 0;

    /// close reusable deleted connections for specified server and/or pool
    virtual void CloseUnusedConnections(const string& srv_name  = kEmptyStr,
                                const string& pool_name = kEmptyStr) = 0;

    /// app_name defines the application name that a connection will use when
    /// connecting to a server.
    void SetApplicationName(const string& app_name);
    const string& GetApplicationName(void) const;

    void SetHostName(const string& host_name);
    const string& GetHostName(void) const;

protected:
    virtual CDB_Connection* MakePooledConnection(const SConnAttr& conn_attr) = 0;

private:
    unsigned int    m_LoginTimeout;
    unsigned int    m_Timeout;

    string          m_AppName;
    string          m_HostName;

    friend class IDBConnectionFactory;
};

/////////////////////////////////////////////////////////////////////////////
///
///  I_Connection::
///

class NCBI_DBAPIDRIVER_EXPORT I_Connection
{
public:
    I_Connection(void);
    virtual ~I_Connection(void);

protected:
    /// Check out if connection is alive (this function doesn't ping the server,
    /// it just checks the status of connection which was set by the last
    /// i/o operation)
    virtual bool IsAlive(void) = 0;

    /// These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    /// create and return a "command" object, register it for later use with
    /// this (and only this!) connection.
    /// On error, an exception will be thrown (they never return NULL!).
    /// It is the user's responsibility to delete the returned "command" object.

    /// Language command
    virtual CDB_LangCmd* LangCmd(const string& lang_query,
                                 unsigned int  nof_params = 0) = 0;
    /// Remote procedure call
    virtual CDB_RPCCmd* RPC(const string& rpc_name,
                            unsigned int  nof_args) = 0;
    /// "Bulk copy in" command
    virtual CDB_BCPInCmd* BCPIn(const string& table_name,
                                unsigned int  nof_columns) = 0;
    /// Cursor
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int  nof_params,
                                  unsigned int  batch_size = 1) = 0;
    /// "Send-data" command
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true) = 0;

    /// Shortcut to send text and image to the server without using the
    /// "Send-data" command (SendDataCmd)
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text& txt,
                          bool log_it = true) = 0;
    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true) = 0;

    /// Reset the connection to the "ready" state (cancel all active commands)
    virtual bool Refresh(void) = 0;

    /// Get the server name, user login name, and password
    virtual const string& ServerName(void) const = 0;
    virtual const string& UserName(void) const = 0;
    virtual const string& Password(void) const = 0;

    /// Get the bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const = 0;

    /// Check if this connection is a reusable one
    virtual bool IsReusable(void) const = 0;

    /// Find out which connection pool this connection belongs to
    virtual const string& PoolName(void) const = 0;

    /// Get pointer to the driver context
    virtual I_DriverContext* Context(void) const = 0;

    /// Put the message handler into message handler stack
    virtual void PushMsgHandler(CDB_UserHandler* h,
                                EOwnership ownership = eNoOwnership) = 0;

    /// Remove the message handler (and all above it) from the stack
    virtual void PopMsgHandler(CDB_UserHandler* h) = 0;

    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp) = 0;

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

    /// Set connection timeout.
    /// NOTE:  if "nof_secs" is zero or is "too big" (depends on the underlying
    ///        DB API), then set the timeout to infinite.
    virtual void SetTimeout(size_t nof_secs) = 0;
};


END_NCBI_SCOPE



/* @} */


#endif  /* DBAPI_DRIVER___INTERFACES__HPP */
