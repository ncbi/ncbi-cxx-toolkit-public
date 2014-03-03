#ifndef DBAPI_DRIVER___PUBLIC__HPP
#define DBAPI_DRIVER___PUBLIC__HPP

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
 * File Description:  Data Server public interfaces
 *
 */

#include <corelib/plugin_manager.hpp>
#include <dbapi/driver/interfaces.hpp>

/** @addtogroup DbPubInterfaces
 *
 * @{
 */


BEGIN_NCBI_SCOPE

NCBI_DECLARE_INTERFACE_VERSION(I_DriverContext,  "xdbapi", 14, 0, 0);


namespace impl
{
    class CDriverContext;
    class CConnection;
    class CCommand;
}


template <class I> class CInterfaceHook;


class NCBI_DBAPIDRIVER_EXPORT CDB_Connection : public I_Connection
{
public:
    /// @brief 
    /// Check out if connection is alive. 
    ///
    /// This function doesn't ping the server,
    /// it just checks the status of connection which was set by the last
    /// i/o operation.
    /// 
    /// @return 
    ///   - true if connection is alive
    ///   - false in other case.
    virtual bool IsAlive();

    // These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    // create and return a "command" object, register it for later use with
    // this (and only this!) connection.
    // On error, an exception will be thrown (they never return NULL!).
    // It is the user's responsibility to delete the returned "command" object.

    /// Make language command
    virtual CDB_LangCmd*     LangCmd(const string& lang_query);
    /// Make remote procedure call command
    virtual CDB_RPCCmd*      RPC(const string& rpc_name);
    /// Make "bulk copy in" command
    virtual CDB_BCPInCmd*    BCPIn(const string& table_name);
    /// Make cursor command
    virtual CDB_CursorCmd*   Cursor(const string& cursor_name,
                                    const string& query,
                                    unsigned int  batch_size);
    CDB_CursorCmd* Cursor(const string& cursor_name,
                          const string& query)
    {
        return Cursor(cursor_name, query, 1);
    }

    /// Make "send-data" command
    /// @brief 
    ///   Create send-data command.
    /// 
    /// @param desc 
    ///   Lob descriptor.
    /// @param data_size 
    ///   Maximal data size.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// @param discard_results
    ///   Discard all resultsets that might be returned from server
    ///   if this value is set to true.
    /// 
    /// @return 
    ///   Newly created send-data object.
    ///
    /// @sa SendData
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true,
                                         bool            discard_results = true);

    /// @brief 
    ///   Shortcut to send text and image to the server without using the
    ///   "Send-data" command (SendDataCmd)
    /// 
    /// @param desc 
    ///   Lob descriptor.
    /// @param lob 
    ///   Text or Image object.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// 
    /// @return 
    ///   - true on success.
    ///
    /// @sa
    ///   SendDataCmd
    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    /// @brief 
    ///   Set database name
    /// 
    /// @param name 
    ///   Database name
    void SetDatabaseName(const string& name);
    
    /// @brief 
    /// Reset the connection to the "ready" state (cancel all active commands)
    /// 
    /// @return 
    ///   - true on success.
    virtual bool Refresh();

    /// @brief 
    ///   Get the server name.
    /// 
    /// @return
    ///   Server/Service name.
    virtual const string& ServerName() const;
    
    /// @brief 
    ///   Get the host.
    /// 
    /// @return
    ///   host ip.
    virtual Uint4 Host() const;

    /// @brief 
    ///   Get the port.
    /// 
    /// @return
    ///   port.
    virtual Uint2 Port() const;

    /// @brief 
    ///   Get the user user.
    /// 
    /// @return
    ///   User name.
    virtual const string& UserName() const;

    /// @brief 
    ///   Get the  password.
    /// 
    /// @return 
    ///   Password value.
    virtual const string& Password() const;

    /// @brief 
    ///   Get the database name.
    /// 
    /// @return 
    ///   Database name.
    virtual const string& DatabaseName(void) const;

    /// @brief 
    /// Get the bitmask for the connection mode (BCP, secure login, ...)
    /// 
    /// @return 
    ///  bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode() const;

    /// @brief 
    ///   Check if this connection is a reusable one
    /// 
    /// @return 
    ///   - true if this connection is a reusable one.
    virtual bool IsReusable() const;

    /// @brief 
    ///   Find out which connection pool this connection belongs to
    /// 
    /// @return 
    ///   connection pool
    virtual const string& PoolName() const;

    /// @brief 
    ///   Get pointer to the driver context
    /// 
    /// @return 
    ///   pointer to the driver context
    virtual I_DriverContext* Context() const;

    /// @brief 
    ///   Put the message handler into message handler stack
    /// 
    /// @param h 
    ///   Error message handler.
    /// @param ownership 
    ///   If set to eNoOwnership, it is user's responsibility to unregister
    ///   and delete the error message handler.
    ///   If set to eTakeOwnership, then DBAPI will take ownership of the
    ///   error message handler and delete it itself.
    ///
    /// @sa
    ///   PopMsgHandler
    virtual void PushMsgHandler(CDB_UserHandler* h,
                                EOwnership ownership = eNoOwnership);

    /// @brief 
    ///   Remove the message handler (and all above it) from the stack
    /// 
    /// @param h 
    ///   Error message handler.
    /// @sa
    ///   PushMsgHandler
    virtual void PopMsgHandler(CDB_UserHandler* h);

    /// @brief 
    ///   Set new result-processor.
    /// 
    /// @param rp 
    ///   New result-processor.
    /// 
    /// @return 
    ///   Old result-processor
    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);

    /// Destructor
    virtual ~CDB_Connection();

    /// Abort the connection
    /// 
    /// @return 
    ///  TRUE - if succeeded, FALSE if not
    ///
    /// @note
    ///   Attention: it is not recommended to use this method unless you 
    ///   absolutely have to.  The expected implementation is - close 
    ///   underlying file descriptor[s] without destroing any objects 
    ///   associated with a connection.
    ///
    /// @sa
    ///   Close
    virtual bool Abort();

    ///  Close an open connection.
    ///  This method will return connection (if it is created as reusable) to 
    ///  its connection pool
    ///   
    /// @return 
    ///  TRUE - if succeeded, FALSE if not
    ///
    /// @sa
    ///   Abort, I_DriverContext::Connect
    virtual bool Close(void);

    /// @brief 
    ///   Set connection timeout.    
    /// 
    /// @param nof_secs 
    ///   Number of seconds.  If "nof_secs" is zero or is "too big" 
    ///   (depends on the underlying DB API), then set the timeout to infinite.
    virtual void SetTimeout(size_t nof_secs);

    /// Set timeout for command cancellation and connection closing
    virtual void SetCancelTimeout(size_t nof_secs);

    /// Get interface for extra features that could be implemented in the driver.
    virtual I_ConnectionExtra& GetExtraFeatures(void);

    virtual string GetDriverName(void) const;

public:
    // Deprecated legacy methods.
    // CXX-601

    /// @deprecated
#ifndef NCBI_UNDEPRECATE__DBAPI_OLD_CONNECTION
    NCBI_DEPRECATED
#endif
    CDB_LangCmd* LangCmd(const string& lang_query, unsigned int /*unused*/)
    {
        return LangCmd(lang_query);
    }
    /// @deprecated
#ifndef NCBI_UNDEPRECATE__DBAPI_OLD_CONNECTION
    NCBI_DEPRECATED
#endif
    CDB_RPCCmd* RPC(const string& rpc_name, unsigned int /*unused*/)
    {
        return RPC(rpc_name);
    }
    /// @deprecated
#ifndef NCBI_UNDEPRECATE__DBAPI_OLD_CONNECTION
    NCBI_DEPRECATED
#endif
    CDB_BCPInCmd* BCPIn(const string& table_name, unsigned int /*unused*/)
    {
        return BCPIn(table_name);
    }
    /// @deprecated
#ifndef NCBI_UNDEPRECATE__DBAPI_OLD_CONNECTION
    NCBI_DEPRECATED
#endif
    CDB_CursorCmd* Cursor(const string& cursor_name,
                          const string& query,
                          unsigned int /*unused*/,
                          unsigned int  batch_size)
    {
        return Cursor(cursor_name, query, batch_size);
    }

    void FinishOpening(void);

private:
    impl::CConnection* m_ConnImpl;

    CDB_Connection(impl::CConnection* c);

    // Prohibit default- and copy- constructors, and assignment
    CDB_Connection();
    CDB_Connection& operator= (const CDB_Connection&);
    CDB_Connection(const CDB_Connection&);

    void ReleaseImpl(void)
    {
        m_ConnImpl = NULL;;
    }

    // The constructor should be called by "I_DriverContext" only!
    friend class impl::CDriverContext;
    friend class CInterfaceHook<CDB_Connection>;
};


class NCBI_DBAPIDRIVER_EXPORT CDB_Result : public I_Result
{
public:
    /// @brief 
    ///   Get type of the result
    /// 
    /// @return 
    ///   Result type
    virtual EDB_ResType ResultType() const;

    /// Get meta-information about rows in resultset. 
    virtual const CDBParams& GetDefineParams(void) const;

    /// Get # of items (columns) in the result
    /// @brief 
    ///   Get # of items (columns) in the result.
    /// 
    /// @return 
    ///   Number of items (columns) in the result.
    virtual unsigned int NofItems() const;

    /// @brief 
    ///   Get name of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    ///
    /// @return 
    ///   NULL if "item_num" >= NofItems(), otherwise item name.
    virtual const char* ItemName(unsigned int item_num) const;

    /// @brief 
    ///   Get size (in bytes) of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    /// 
    /// @return 
    ///   Return zero if "item_num" >= NofItems().
    virtual size_t ItemMaxSize(unsigned int item_num) const;

    /// @brief 
    ///   Get datatype of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    /// 
    /// @return 
    ///   Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const;

    /// @brief 
    ///   Fetch next row
    /// 
    /// @return
    ///   - true if a record was fetched.
    ///   - false if no more record can be fetched.
    virtual bool Fetch();

    /// @brief 
    ///   Return current item number we can retrieve (0,1,...)
    /// 
    /// @return 
    ///   Return current item number we can retrieve (0,1,...)
    ///   Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo() const;

    /// @brief 
    ///   Return number of columns in the recordset.
    /// 
    /// @return 
    ///   number of columns in the recordset.
    virtual int GetColumnNum(void) const;

    /// @brief 
    ///   Get a result item (you can use either GetItem or ReadItem).
    /// 
    /// @param item_buf 
    ///   If "item_buf" is not NULL, then use "*item_buf" (its type should be
    ///   compatible with the type of retrieved item!) to retrieve the item to;
    ///   otherwise allocate new "CDB_Object".
	///   In case of "CDB_Image" and "CDB_Text" data types value will be *appended*
	///   to the "item_buf" by default (policy == eAppendLOB).
    /// 
	/// @param policy
	///   Data retrieval policy. If policy == eAppendLOB and "item_buf" is an
	///   object of CDB_Image or CDB_Text type, then data will be *appended* to
	///   the end of previously assigned data. If policy == eAssignLOB and "item_buf" is an
	///   object of CDB_Image or CDB_Text type, then new value will be *assigned*
	///   to the "item_buf" object.
	///
    /// @return 
    ///   a result item
    ///
    /// @sa
    ///   ReadItem, SkipItem
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0, EGetItem policy = eAppendLOB);

    /// @brief 
    ///   Read a result item body (for text/image mostly).
    ///   Throw an exception on any error.
    /// 
    /// @param buffer 
    ///   Buffer to fill with data.
    /// @param buffer_size 
    ///   Buffere size.
    /// @param is_null 
    ///   Set "*is_null" to TRUE if the item is <NULL>.
    /// 
    /// @return 
    ///   number of successfully read bytes.
    ///
    /// @sa
    ///   GetItem, SkipItem
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0);

    /// @brief 
    ///   Get a descriptor for text/image column (for SendData).
    /// 
    /// @return 
    ///   Return NULL if this result doesn't (or can't) have img/text descriptor.
    ///
    /// @note
    ///   You need to call ReadItem (maybe even with buffer_size == 0)
    ///   before calling this method!
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    /// @brief 
    /// Skip result item
    /// 
    /// @return 
    ///   TRUE on success.
    /// 
    /// @sa
    ///   GetItem, ReadItem
    virtual bool SkipItem();

    /// Destructor
    virtual ~CDB_Result();

private:
    impl::CResult* GetIResultPtr(void) const
    {
        return m_ResImpl;
    }
    impl::CResult& GetIResult(void) const
    {
        _ASSERT(m_ResImpl);
        return *m_ResImpl;
    }

    void ReleaseImpl(void)
    {
        m_ResImpl = NULL;;
    }

private:
    impl::CResult* m_ResImpl;

    CDB_Result(impl::CResult* r);

    // Prohibit default- and copy- constructors, and assignment
    CDB_Result& operator= (const CDB_Result&);
    CDB_Result(const CDB_Result&);
    CDB_Result(void);

    // The constructor should be called by "I_***Cmd" only!
    friend class impl::CConnection;
    friend class impl::CCommand;
    friend class CInterfaceHook<CDB_Result>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_LangCmd : public I_LangCmd
{
public:
    /// Add more text to the language command
    /// @deprecated
    // CXX-601
#ifndef NCBI_UNDEPRECATE__DBAPI_OLD_LANGCMD
    NCBI_DEPRECATED
#endif
    virtual bool More(const string& query_text);

    /// Get meta-information about parameters. 
    virtual CDBParams& GetBindParams(void);
    virtual CDBParams& GetDefineParams(void);

    // Bind cmd parameter with name "name" to the object pointed by "value"
    bool BindParam(const string& name, CDB_Object* value)
    {
        GetBindParams().Bind(name, value);
        return true;
    }

    // Set cmd parameter with name "name" to the object pointed by "value"
    bool SetParam(const string& name, CDB_Object* value)
    {
        GetBindParams().Set(name, value);
        return true;
    }

    /// Send command to the server
    virtual bool Send();
    /// Implementation-specific.
    /// @deprecated
    NCBI_DEPRECATED virtual bool WasSent() const;

    /// Cancel the command execution
    virtual bool Cancel();
    /// Implementation-specific.
    /// @deprecated
    NCBI_DEPRECATED virtual bool WasCanceled() const;

    /// Get result set
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;

    /// Check if command has failed
    virtual bool HasFailed() const;

    /// Get the number of rows affected by the command.
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    /// Dump the results of the command
    /// If result processor is installed for this connection, then it will be
    /// called for each result set
    virtual void DumpResults();

    // Destructor
    virtual ~CDB_LangCmd();

private:
    impl::CBaseCmd* m_CmdImpl;

    CDB_LangCmd(impl::CBaseCmd* cmd);

    // Prohibit default- and copy- constructors, and assignment
    CDB_LangCmd& operator= (const CDB_LangCmd&);
    CDB_LangCmd(const CDB_LangCmd&);
    CDB_LangCmd();

    void ReleaseImpl(void)
    {
        m_CmdImpl = NULL;;
    }

    // The constructor should be called by "I_Connection" only!
    friend class impl::CConnection;
    friend class CInterfaceHook<CDB_LangCmd>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_RPCCmd : public I_RPCCmd
{
public:
    /// Get meta-information about parameters. 
    virtual CDBParams& GetBindParams(void);
    virtual CDBParams& GetDefineParams(void);

    // Binding
    bool BindParam(
            const string& name, 
            CDB_Object* value, 
            bool out_param = false
            )
    {
        GetBindParams().Bind(name, value, out_param);
        return true;
    }

    // Setting
    bool SetParam(
            const string& name, 
            CDB_Object* value, 
            bool out_param = false
            )
    {
        GetBindParams().Set(name, value, out_param);
        return true;
    }

    /// Send command to the server
    virtual bool Send();
    /// Implementation-specific.
    /// @deprecated
    NCBI_DEPRECATED virtual bool WasSent() const;

    /// Cancel the command execution
    virtual bool Cancel();
    /// Implementation-specific.
    /// @deprecated
    NCBI_DEPRECATED virtual bool WasCanceled() const;

    /// Get result set.
    /// Return NULL if no more results left to read.
    /// Throw exception on error or if attempted to read after NULL was returned
    virtual CDB_Result* Result();

    /// Return TRUE if it makes sense (at all) to call Result()
    virtual bool HasMoreResults() const;

    /// Check if command has failed
    virtual bool HasFailed() const;

    /// Get the number of rows affected by the command
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    /// Dump the results of the command
    /// If result processor is installed for this connection, then it will be
    /// called for each result set
    virtual void DumpResults();

    /// Set the "recompile before execute" flag for the stored proc
    /// Implementation-specific.
    /// @deprecated
    NCBI_DEPRECATED virtual void SetRecompile(bool recompile = true);

    /// Get a name of the procedure.
    virtual const string& GetProcName(void) const;

    // Destructor
    virtual ~CDB_RPCCmd();

private:
    impl::CBaseCmd* m_CmdImpl;

    CDB_RPCCmd(impl::CBaseCmd* rpc);

    // Prohibit default- and copy- constructors, and assignment
    CDB_RPCCmd& operator= (const CDB_RPCCmd&);
    CDB_RPCCmd(const CDB_RPCCmd&);
    CDB_RPCCmd();

    void ReleaseImpl(void)
    {
        m_CmdImpl = NULL;;
    }

    // Constructor should be called by "I_Connection" only!
    friend class impl::CConnection;
    friend class CInterfaceHook<CDB_RPCCmd>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_BCPInCmd : public I_BCPInCmd
{
public:
    /// Set hints by one call. Resets everything that was set by Add*Hint().
    void SetHints(CTempString hints);

    /// Type of hint that can be set.
    enum EBCP_Hints {
        eOrder,
        eRowsPerBatch,
        eKilobytesPerBatch,
        eTabLock,
        eCheckConstraints,
        eFireTriggers
    };

    /// Add hint with value.
    /// Can be used with any hint type except eOrder and with e..PerBatch
    /// value should be non-zero.
    /// Resets everything that was set by SetHints().
    void AddHint(EBCP_Hints hint, unsigned int value = 0);

    /// Add "ORDER" hint.
    /// Resets everything that was set by SetHints().
    void AddOrderHint(CTempString columns);

    /// Get meta-information about parameters.
    virtual CDBParams& GetBindParams(void);

    // Binding
    bool Bind(unsigned int column_num, CDB_Object* value);

    /// Send row to the server
    virtual bool SendRow();

    /// Complete batch -- to store all rows transferred by far in this batch
    /// into the table
    virtual bool CompleteBatch();

    /// Cancel the BCP command
    virtual bool Cancel();

    /// Complete the BCP and store all rows transferred in last batch into
    /// the table
    virtual bool CompleteBCP();

    // Destructor
    virtual ~CDB_BCPInCmd();

private:
    impl::CBaseCmd* m_CmdImpl;

    CDB_BCPInCmd(impl::CBaseCmd* bcp);

    // Prohibit default- and copy- constructors, and assignment
    CDB_BCPInCmd& operator= (const CDB_BCPInCmd&);
    CDB_BCPInCmd(const CDB_BCPInCmd&);
    CDB_BCPInCmd();

    void ReleaseImpl(void)
    {
        m_CmdImpl = NULL;;
    }

    // The constructor should be called by "I_Connection" only!
    friend class impl::CConnection;
    friend class CInterfaceHook<CDB_BCPInCmd>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_CursorCmd : public I_CursorCmd
{
public:
    /// Get meta-information about parameters. 
    virtual CDBParams& GetBindParams(void);
    virtual CDBParams& GetDefineParams(void);

    // Binding
    bool BindParam(const string& name, CDB_Object* value)
    {
        GetBindParams().Bind(name, value);
        return true;
    }
        
    /// Open the cursor.
    /// Return NULL if cursor resulted in no data.
    /// Throw exception on error.
    virtual CDB_Result* Open();

    /// Update the last fetched row.
    /// NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                 bool log_it = true);
    /// Make "send-data" command
    /// @brief 
    ///   Create send-data command.
    /// 
    /// @param item_num 
    ///   Column number to rewrite.
    /// @param size 
    ///   Maximal data size.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// @param discard_results
    ///   Discard all resultsets that might be returned from server
    ///   if this value is set to true.
    /// 
    /// @return 
    ///   Newly created send-data object.
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool discard_results = true);

    /// Delete the last fetched row.
    /// NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
    virtual bool Delete(const string& table_name);

    /// Get the number of fetched rows
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    /// Close the cursor.
    /// Return FALSE if the cursor is closed already (or not opened yet)
    virtual bool Close();

    // Destructor
    virtual ~CDB_CursorCmd();

private:
    impl::CBaseCmd* m_CmdImpl;

    CDB_CursorCmd(impl::CBaseCmd* cur);

    // Prohibit default- and copy- constructors, and assignment
    CDB_CursorCmd& operator= (const CDB_CursorCmd&);
    CDB_CursorCmd(const CDB_CursorCmd&);
    CDB_CursorCmd();

    void ReleaseImpl(void)
    {
        m_CmdImpl = NULL;;
    }

    // The constructor should be called by "I_Connection" only!
    friend class impl::CConnection;
    friend class CInterfaceHook<CDB_CursorCmd>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_SendDataCmd : public I_SendDataCmd
{
public:
    /// Send chunk of data to the server.
    /// Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* data, size_t size);
    virtual bool Cancel(void);

    /// Get result set
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;

    /// Dump the results of the command
    /// If result processor is installed for this connection, then it will be
    /// called for each result set
    virtual void DumpResults();

    // Destructor
    virtual ~CDB_SendDataCmd();

private:
    impl::CSendDataCmd* m_CmdImpl;

    CDB_SendDataCmd(impl::CSendDataCmd* c);

    // Prohibit default- and copy- constructors, and assignment
    CDB_SendDataCmd& operator= (const CDB_SendDataCmd&);
    CDB_SendDataCmd(const CDB_CursorCmd&);
    CDB_SendDataCmd();

    void ReleaseImpl(void)
    {
        m_CmdImpl = NULL;;
    }

    // The constructor should be called by "I_Connection" only!
    friend class impl::CConnection;
    friend class CInterfaceHook<CDB_SendDataCmd>;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_ITDescriptor : public I_ITDescriptor
{
public:
    enum ETDescriptorType {eUnknown, eText, eBinary};

    CDB_ITDescriptor(const string& table_name,
                     const string& column_name,
                     const string& search_conditions,
                     ETDescriptorType column_type = eUnknown);
    virtual ~CDB_ITDescriptor(void);

    virtual int DescriptorType(void) const;

    const string& TableName()        const { return m_TableName;        }
    void SetTableName(const string& name)  { m_TableName = name;        }
    const string& ColumnName()       const { return m_ColumnName;       }
    void SetColumnName(const string& name) { m_ColumnName = name;       }
    const string& SearchConditions() const { return m_SearchConditions; }
    void SetSearchConditions(const string& cond) { m_SearchConditions = cond; }

    ETDescriptorType GetColumnType(void) const
    {
        return m_ColumnType;
    }
    void SetColumnType(ETDescriptorType type)
    {
        m_ColumnType = type;
    }

protected:
    string              m_TableName;
    string              m_ColumnName;
    string              m_SearchConditions;
    ETDescriptorType    m_ColumnType;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_ResultProcessor
{
public:
    CDB_ResultProcessor(CDB_Connection* c);
    virtual ~CDB_ResultProcessor();

    /// The default implementation just dumps all rows.
    /// To get the data you will need to override this method.
    virtual void ProcessResult(CDB_Result& res);

private:
    // Prohibit default- and copy- constructors, and assignment
    CDB_ResultProcessor();
    CDB_ResultProcessor& operator= (const CDB_ResultProcessor&);
    CDB_ResultProcessor(const CDB_ResultProcessor&);

    void ReleaseConn(void);
    void SetConn(CDB_Connection* c);

    CDB_Connection*      m_Con;
    CDB_ResultProcessor* m_Prev;
    CDB_ResultProcessor* m_Next;

    friend class impl::CConnection;
};


////////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CAutoTrans
{
public:
    /// Helper class to allow safe initialization from higher-layer objects.
    class CSubject {
    public:
        CSubject(CDB_Connection& connection) : m_Connection(connection) { }

    private:
        CDB_Connection& m_Connection;
        friend class CAutoTrans;
    };

    CAutoTrans(const CSubject& subject);
    ~CAutoTrans(void);

public:
    bool Continue(void) const
    {
        return m_Abort;
    }
    void Finish(void)
    {
        m_Abort = false;
    }

private:
    CAutoTrans(const CAutoTrans&);
    CAutoTrans& operator =(const CAutoTrans&);

    void BeginTransaction(void);
    void Commit(void);
    void Rollback(void);
    int GetTranCount(void);

private:
    bool m_Abort;
    CDB_Connection& m_Conn;
    int m_TranCount;
};


////////////////////////////////////////////////////////////////////////////////
inline
CAutoTrans::CSubject DBAPI_MakeTrans(CDB_Connection& connection)
{
    return CAutoTrans::CSubject(connection);
}


////////////////////////////////////////////////////////////////////////////////
/// RAII transaction support.
/// Resource Acquisition Is Initialization (RAII) programming style in intended 
/// to revert a transaction automatically if any exception occurs in a code block.
#define DBAPI_TRANSACTION(connection) \
for(ncbi::CAutoTrans auto_trans(ncbi::DBAPI_MakeTrans(connection)); \
    auto_trans.Continue(); \
    auto_trans.Finish())


END_NCBI_SCOPE


/* @} */


#endif  /* DBAPI_DRIVER___PUBLIC__HPP */
