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
 * ===========================================================================
 */

#include <dbapi/driver/types.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>


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



/////////////////////////////////////////////////////////////////////////////
//
//  ITDescriptor::
//
// Image or Text descriptor.
//

class ITDescriptor
{
public:
    virtual ~ITDescriptor();
};



/////////////////////////////////////////////////////////////////////////////
//
//  EDBResType::
//
// Type of result set
//

enum EDBResType {
    eRowResult,
    eParamResult,
    eComputeResult,
    eStatusResult,
    eCursorResult
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDB_BaseEnt::
//
// Base class for most interface classes.
// It keeps a double-reference to itself, and resets this reference
// (used by others to access this object) on the object destruction.
//

class CDB_BaseEnt
{
public:
    CDB_BaseEnt() {
        m_BR = 0;
    }
    void Acquire(CDB_BaseEnt** br) {
        m_BR = br;
    }
    virtual void Release();
    virtual ~CDB_BaseEnt();

protected:
    CDB_BaseEnt** m_BR;  // double-reference to itself

    // To allow "I_***Cmd" to create CDB_Result
    static CDB_Result* Create_Result(I_Result& result);
};



/////////////////////////////////////////////////////////////////////////////
//
//  I_BaseCmd::
//
// Abstract base class for most "command" interface classes.
//

class I_BaseCmd : public CDB_BaseEnt
{
public:
    // Send command to the server
    virtual bool Send() = 0;
    virtual bool WasSent() const = 0;

    // Cancel the command execution
    virtual bool Cancel() = 0;
    virtual bool WasCanceled() const = 0;

    // Get result set
    virtual CDB_Result* Result() = 0;
    virtual bool HasMoreResults() const = 0;

    // Check if command has failed
    virtual bool HasFailed() const = 0;

    // Get the number of rows affected by the command
    // Special case:  negative on error or if there is no way that this
    //                command could ever affect any rows (like PRINT).
    virtual int RowCount() const = 0;

    // Destructor
    virtual ~I_BaseCmd();
};



/////////////////////////////////////////////////////////////////////////////
//
//  I_LangCmd::
//  I_RPCCmd::
//  I_BCPInCmd::
//  I_CursorCmd::
//  I_SendDataCmd::
//
// "Command" interface classes.
//


class I_LangCmd : public I_BaseCmd
{
protected:
    // Add more text to the language command
    virtual bool More(const string& query_text) = 0;

    // Bind cmd parameter with name "name" to the object pointed by "value"
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

    // Set cmd parameter with name "name" to the object pointed by "value"
    virtual bool SetParam(const string& name, CDB_Object* param_ptr) = 0;

public:
    virtual ~I_LangCmd();

    friend class CDB_LangCmd;
};



class I_RPCCmd : public I_BaseCmd
{
protected:
    // Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr,
                           bool out_param = false) = 0;

    // Setting
    virtual bool SetParam(const string& name, CDB_Object* param_ptr,
                          bool out_param = false) = 0;

    // Set the "recompile before execute" flag for the stored proc
    virtual void SetRecompile(bool recompile = true) = 0;

public:
    virtual ~I_RPCCmd();

    friend class CDB_RPCCmd;
};



class I_BCPInCmd : public CDB_BaseEnt
{
protected:
    // Binding
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr) = 0;

    // Send row to the server
    virtual bool SendRow() = 0;

    // Complete batch -- to store all rows transferred by far in this batch
    // into the table
    virtual bool CompleteBatch() = 0;

    // Cancel the BCP command 
    virtual bool Cancel() = 0;

    // Complete the BCP and store all rows transferred in last batch into
    // the table
    virtual bool CompleteBCP() = 0;

public:
    virtual ~I_BCPInCmd();

    friend class CDB_BCPInCmd;
};



class I_CursorCmd : public CDB_BaseEnt
{
protected:
    // Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

    // Open the cursor.
    // Return NULL if cursor resulted in no data.
    // Throw exception on error.
    virtual CDB_Result* Open() = 0;

    // Update the last fetched row.
    // NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query) = 0;

    // Delete the last fetched row.
    // NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
    virtual bool Delete(const string& table_name) = 0;

    // Get the number of fetched rows
    // Special case:  negative on error or if there is no way that this
    //                command could ever affect any rows (like PRINT).
    virtual int RowCount() const = 0;

    // Close the cursor.
    // Return FALSE if the cursor is closed already (or not opened yet)
    virtual bool Close() = 0;

public:
    virtual ~I_CursorCmd();

    friend class CDB_CursorCmd;
};



class I_SendDataCmd : public CDB_BaseEnt
{
protected:
    // Send chunk of data to the server.
    // Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* pChunk, size_t nofBytes) = 0;

public:
    virtual ~I_SendDataCmd();

    friend class CDB_SendDataCmd;
};

    

/////////////////////////////////////////////////////////////////////////////
//
//  I_Result::
//

class I_Result : public CDB_BaseEnt
{
protected:
    // Get type of the result
    virtual EDBResType ResultType() const = 0;

    // Get # of items (columns) in the result
    virtual unsigned int NofItems() const = 0;

    // Get name of a result item.
    // Return NULL if "item_num" >= NofItems().
    virtual const char* ItemName(unsigned int item_num) const = 0;

    // Get size (in bytes) of a result item.
    // Return zero if "item_num" >= NofItems().
    virtual size_t ItemMaxSize(unsigned int item_num) const = 0;

    // Get datatype of a result item.
    // Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const = 0;

    // Fetch next row
    virtual bool Fetch() = 0;

    // Return current item number we can retrieve (0,1,...)
    // Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo() const = 0;

    // Get a result item (you can use either GetItem or ReadItem).
    // If "item_buf" is not NULL, then use "*item_buf" (its type should be
    // compatible with the type of retrieved item!) to retrieve the item to;
    // otherwise allocate new "CDB_Object".
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0) = 0;

    // Read a result item body (for text/image mostly).
    // Return number of successfully read bytes.
    // Set "*is_null" to TRUE if the item is <NULL>.
    // Throw an exception on any error.
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0) = 0;

    // Get a descriptor for text/image column (for SendData).
    // Return NULL if this result does not (or cannot) have img/text descriptor.
    // NOTE: you need to call ReadItem (maybe even with buffer_size == 0)
    //       before calling this method!
    virtual ITDescriptor* GetImageOrTextDescriptor() = 0;

    // Skip result item
    virtual bool SkipItem() = 0;

public:
    virtual ~I_Result();

    friend class CDB_Result;
};


    
/////////////////////////////////////////////////////////////////////////////
//
//  I_DriverContext::
//

class I_DriverContext
{
public:
    // Connection mode
    enum EConnectionMode {
        fBcpIn             = 0x1,
        fPasswordEncrypted = 0x2
        // all driver-specific mode flags > 0x100
    };
    typedef int TConnectionMode;

    // Set login and connection timeouts.
    // NOTE:  if "nof_secs" is zero or is "too big" (depends on the underlying
    //        DB API), then set the timeout to infinite.
    // Return FALSE on error.
    virtual bool SetLoginTimeout (unsigned int nof_secs = 0) = 0;
    virtual bool SetTimeout      (unsigned int nof_secs = 0) = 0;

    // Set maximal size for Text and Image objects. Text and Image objects
    // exceeding this size will be truncated.
    // Return FALSE on error (e.g. if "nof_bytes" is too big).
    virtual bool SetMaxTextImageSize(size_t nof_bytes) = 0;

    // Create new connection to specified server (within this context).
    // It is your responsibility to delete the returned connection object.
    virtual CDB_Connection* Connect(const string&      srv_name,
                                    const string&      user_name,
                                    const string&      passwd, 
                                    TConnectionMode    mode,
                                    bool               reusable  = false,
                                    const string&      pool_name = kEmptyStr)=0;

    // Return number of currently open connections in this context.
    // If "srv_name" is not NULL, then return # of conn. open to that server.
    virtual unsigned int NofConnections(const string& srv_name = kEmptyStr)
        const = 0;

    // Add message handler "h" to process 'context-wide' (not bound 
    // to any particular connection) error messages.
    virtual void PushCntxMsgHandler(CDBUserHandler* h) = 0;

    // Remove message handler "h" and all handlers above it in the stack
    virtual void PopCntxMsgHandler(CDBUserHandler* h) = 0;

    // Add `per-connection' err.message handler "h" to the stack of default
    // handlers which are inherited by all newly created connections.
    virtual void PushDefConnMsgHandler(CDBUserHandler* h) = 0;

    // Remove `per-connection' mess. handler "h" and all above it in the stack.
    virtual void PopDefConnMsgHandler(CDBUserHandler* h) = 0;

    virtual ~I_DriverContext();

protected:
    // To allow the children of I_DriverContext to create CDB_Connection
    static CDB_Connection* Create_Connection(I_Connection& connection);

    // Used and unused(reserve) connections
    CPointerPot m_NotInUse;
    CPointerPot m_InUse;

private:
    // Return unused connection "conn" to the driver context for future
    // reuse (if "conn_reusable" is TRUE) or utilization
    void x_Recycle(I_Connection* conn, bool conn_reusable);
    friend class CDB_Connection;
};



/////////////////////////////////////////////////////////////////////////////
//
//  I_Connection::
//

class I_Connection : public CDB_BaseEnt
{
protected:
    // Check out if connection is alive (this function doesn't ping the server,
    // it just checks the status of connection which was set by the last
    // i/o operation)
    virtual bool IsAlive() = 0;

    // These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    // create and return a "command" object, register it for later use with
    // this (and only this!) connection.
    // On error, an exception will be thrown (they never return NULL!).
    // It is the user's responsibility to delete the returned "command" object.

    // Language command
    virtual CDB_LangCmd* LangCmd(const string& lang_query,
                                 unsigned int  nof_params = 0) = 0;
    // Remote procedure call
    virtual CDB_RPCCmd* RPC(const string& rpc_name,
                            unsigned int  nof_args) = 0;
    // "Bulk copy in" command
    virtual CDB_BCPInCmd* BCPIn(const string& table_name,
                                unsigned int  nof_columns) = 0;
    // Cursor
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int  nof_params,
                                  unsigned int  batch_size = 1) = 0;
    // "Send-data" command
    virtual CDB_SendDataCmd* SendDataCmd(ITDescriptor& desc, size_t data_size,
                                         bool log_it = true) = 0;

    // Shortcut to send text and image to the server without using the
    // "Send-data" command (SendDataCmd)
    virtual bool SendData(ITDescriptor& desc, CDB_Text& txt,
                          bool log_it = true) = 0;
    virtual bool SendData(ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true) = 0;

    // Reset the connection to the "ready" state (cancel all active commands)
    virtual bool Refresh() = 0;

    // Get the server name, user login name, and password
    virtual const string& ServerName() const = 0;
    virtual const string& UserName() const = 0;
    virtual const string& Password() const = 0;

    // Get the bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode() const = 0;

    // Check if this connection is a reusable one
    virtual bool IsReusable() const = 0;

    // Find out which connection pool this connection belongs to
    virtual const string& PoolName() const = 0;

    // Get pointer to the driver context
    virtual I_DriverContext* Context() const = 0;

    // Put the message handler into message handler stack
    virtual void PushMsgHandler(CDBUserHandler* h) = 0;

    // Remove the message handler (and all above it) from the stack
    virtual void PopMsgHandler(CDBUserHandler* h) = 0;

    // These methods to allow the children of I_Connection to create
    // various command-objects
    static CDB_LangCmd*     Create_LangCmd     (I_LangCmd&     lang_cmd    );
    static CDB_RPCCmd*      Create_RPCCmd      (I_RPCCmd&      rpc_cmd     );
    static CDB_BCPInCmd*    Create_BCPInCmd    (I_BCPInCmd&    bcpin_cmd   );
    static CDB_CursorCmd*   Create_CursorCmd   (I_CursorCmd&   cursor_cmd  );
    static CDB_SendDataCmd* Create_SendDataCmd (I_SendDataCmd& senddata_cmd);

public:
    virtual ~I_Connection();

    friend class CDB_Connection;
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/09/24 20:52:18  vakatov
 * Fixed args like "string& s = 0" to "string& s = kEmptyStr"
 *
 * Revision 1.1  2001/09/21 23:39:52  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___INTERFACES__HPP */
