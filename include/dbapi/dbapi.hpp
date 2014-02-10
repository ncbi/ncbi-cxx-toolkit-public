#ifndef DBAPI___DBAPI__HPP
#define DBAPI___DBAPI__HPP

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
 * Author:  Michael Kholodov
 *
 * File Description:  Database API interface
 *
 */

/// @file dbapi.hpp
/// Defines the database API innterfaces for a variety of DBMS.

#include <corelib/ncbiobj.hpp>
#include <corelib/reader_writer.hpp>
#include <dbapi/driver_mgr.hpp>
#include <dbapi/variant.hpp>


/** @addtogroup DbAPI
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
///  EDataSource --
///
///  Data source platform
///
/// enum EDataSource {
///   eSybase,
///   eMsSql
/// };



/////////////////////////////////////////////////////////////////////////////
///
///  EAllowLog --
///
///  Allow transaction log (general, to avoid using bools).
///
enum EAllowLog {
   eDisableLog,     ///< Disables log.
   eEnableLog       ///< Enables log.
};



/////////////////////////////////////////////////////////////////////////////
///
///  IResultSetMetaData --
///
///  Interface class defines retrieving column information from a resultset,
///  such as total number of columns, type, name, etc.

class NCBI_DBAPI_EXPORT IResultSetMetaData
{
public:
    /// Destructor.
    ///
    /// Clean up the metadata for the resultset.
    virtual ~IResultSetMetaData(void);

    /// Get total number of columns in resultset.
    virtual unsigned int  GetTotalColumns(void) const = 0;

    /// Get data type for column in the resultset.
    ///
    /// @param param
    ///   Column number or name
    virtual EDB_Type GetType(const CDBParamVariant& param) const = 0;

    /// Get maximum size in bytes for column.
    ///
    /// @param col
    ///   Column number
    /// 
    /// @return 
    ///   Max number of bytes needed to hold the returned data. 
    virtual int GetMaxSize (const CDBParamVariant& param) const = 0;

    /// Get name of column.
    ///
    /// @param param
    ///   Column number or name
    virtual string GetName (const CDBParamVariant& param) const = 0;

    /// Get parameter's direction (in/out/inout).
    ///
    /// @param param
    ///   Column number or name
    virtual CDBParams::EDirection GetDirection(const CDBParamVariant& param) const = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  IResultSet --
///
///  Used to retrieve a resultset from a query or cursor

class IConnection;

class NCBI_DBAPI_EXPORT IResultSet
{
public:
    /// Destructor.
    ///
    /// Clean up the resultset.
    virtual ~IResultSet();

    /// Get result type.
    ///
    /// @sa
    ///   See in <dbapi/driver/interfaces.hpp> for the list of result types.
    virtual EDB_ResType GetResultType() = 0;

    /// Get next row.
    ///
    /// NOTE: no results are fetched before first call to this function.
    virtual bool Next() = 0;

    /// Retrieve a CVariant class describing the data stored in a given column.
    /// Note that the index supplied is one-based, not zero-based; the first
    /// column is column 1.
    ///
    /// @param param
    ///   Column number (one-based) or name
    /// @return
    ///   All data (for BLOB data see below) is returned as CVariant.
    virtual const CVariant& GetVariant(const CDBParamVariant& param) = 0;

    /// Disables column binding.
    /// @note
    ///   When binding is disabled all columns must be read with Read()
    ///   method, GetVariant() method will always return NULL in this case.
    ///
    /// False by default.
    /// @param
    ///   Disables column binding when set to true.
    virtual void DisableBind(bool b) = 0;

    /// Bind blob to variant.
    ///
    /// If this mode is true, BLOB data is returned as CVariant
    /// False by default.
    /// @note
    ///   When binding of blobs to variant is disabled all columns in
    ///   resultset placed after first blob column must be read with Read()
    ///   method, GetVariant() method will always return NULL for these
    ///   columns.
    ///
    /// @param
    ///   Enables blob binding when set to true.
    virtual void BindBlobToVariant(bool b) = 0;

    /// Read unformatted data.
    ///
    /// Reads unformatted data, returns bytes actually read.
    /// Advances to next column as soon as data is read from the previous one.
    /// Returns 0 when the column data is fully read
    /// Valid only when the column binding is off (see DisableBind())
    /// @param buf
    ///   Buffer to read data.
    /// @param size
    ///   Amount of data to read.
    /// @return
    ///   Actual number of bytes read.
    virtual size_t Read(void* buf, size_t size) = 0;

    /// Determine if last column was NULL.
    ///
    /// Valid only when the column binding is off.
    /// @return
    ///   Return true if the last column read was NULL.
    /// @sa
    ///   DisableBind().
    virtual bool WasNull() = 0;

    /// Get column number, currently available for Read()
    ///
    /// @return
    ///    Returns current item number we can retrieve (1,2,...) using Read()
    ///    Returns "0" if no more items left (or available) to read
    virtual int GetColumnNo() = 0;

    /// Get total columns.
    ///
    /// @return
    ///   Returns total number of columns in the resultset
    virtual unsigned int GetTotalColumns() = 0;

    /// Get Blob input stream.
    ///
    /// @param buf_size
    ///   buf_size is the size of internal buffer, default 4096.
    virtual CNcbiIstream& GetBlobIStream(size_t buf_size = 0) = 0;

    /// Get Blob output stream. The existing connection is
    /// cloned for writing blob.
    ///
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param log_it
    ///    Enables transaction log for BLOB (enabled by default).
    ///    Make sure you have enough log segment space, or disable it.
    /// @param buf_size
    ///   The size of internal buffer, default 4096.
    virtual CNcbiOstream& GetBlobOStream(size_t blob_size,
                                         EAllowLog log_it = eEnableLog,
                                         size_t buf_size = 0) = 0;

    /// Get Blob output stream with explicit additional connection.
    ///
    /// @param conn
    ///   addtional connection used for writing blob (the above method
    ///   clones the existing connection implicitly)
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param log_it
    ///    Enables transaction log for BLOB (enabled by default).
    ///    Make sure you have enough log segment space, or disable it.
    /// @param buf_size
    ///   The size of internal buffer, default 4096.
    virtual CNcbiOstream& GetBlobOStream(IConnection *conn,
                                         size_t blob_size,
                                         EAllowLog log_it = eEnableLog,
                                         size_t buf_size = 0) = 0;

    /// Get a Blob Reader.
    ///
    /// @param
    ///  Pointer to the Blob Reader.
    virtual IReader* GetBlobReader() = 0;

    /// Close resultset.
    virtual void Close() = 0;

    /// Get Metadata.
    ///
    /// @return
    ///   Pointer to result metadata.
    virtual const IResultSetMetaData* GetMetaData(EOwnership ownership = eNoOwnership) = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  IStatement --
///
///  Interface for a SQL statement

class I_ITDescriptor;

class NCBI_DBAPI_EXPORT IStatement
{
public:
    /// Destructor.
    virtual ~IStatement();

    /// Get resulset.
    ///
    /// @return
    ///   Pointer to resultset. For statements with no resultset return 0.
    virtual IResultSet* GetResultSet() = 0;

    /// Check for more results available.
    ///
    /// Each call advances to the next result and the current one
    /// will be cancelled it not retrieved before next call.
    /// The amount of retured results may be bigger than the expected amount
    /// due to auxiliary results returned depending on the driver and server
    /// platform.
    ///
    /// @return
    ///   Return true, if there are more results available.
    virtual bool HasMoreResults() = 0;

    /// Check if the statement failed.
    ///
    /// @return
    ///   Return true, if the statement failed.
    virtual bool Failed() = 0;

    /// Check if resultset has rows.
    ///
    /// @return
    ///   Return true, if resultset has rows.
    virtual bool HasRows() = 0;

    /// Purge results.
    ///
    /// Calls fetch for every resultset received until
    /// finished.
    virtual void PurgeResults() = 0;

    /// Cancel statement.
    ///
    /// Rolls back current transaction.
    virtual void Cancel() = 0;

    /// Close statement.
    virtual void Close() = 0;

    /// Sends one or more SQL statements to the SQL server
    ///
    /// @param sql
    ///   SQL statement to execute.
    virtual void SendSql(const string& sql) = 0;

    /// Sends one or more SQL statements to the SQL server (NOTE: replaced by
    /// the SendSql())
    ///
    /// @param sql
    ///   SQL statement to execute.
    /// @deprecated
    ///   Use SendSql() instead
    virtual void Execute(const string& sql) = 0;

    /// Executes SQL statement with no results returned.
    ///
    /// All resultsets are discarded.
    /// @param sql
    ///   SQL statement to execute.
    virtual void ExecuteUpdate(const string& sql) = 0;

    /// Exectues SQL statement and returns the first resultset.
    ///
    /// If there is more than one resultset, the rest remain
    /// pending unless either PurgeResults() is called or next statement
    /// is run or the statement is closed.
    /// NOTE: Provided only for queries containing a single sql statement returning rows.
    /// @param sql
    ///   SQL statement to execute.
    /// @return
    ///   Pointer to result set. Ownership of IResultSet* belongs to IStatement.
    ///   It is not allowed to use auto_ptr<> to manage life-time of
    ///   IResultSet*.
    virtual IResultSet* ExecuteQuery(const string& sql) = 0;

    /// Executes the last command (with changed parameters, if any).
    virtual void ExecuteLast() = 0;

    /// Set input/output parameter.
    ///
    /// @param v
    ///   Parameter value.
    /// @param name
    ///   Parameter name.
    virtual void SetParam(const CVariant& v,
                          const CDBParamVariant& param) = 0;

    /// Clear parameter list.
    virtual void ClearParamList() = 0;

    /// Get total of rows returned.
    ///
    /// Valid only after all rows are retrieved from a resultset
    virtual int GetRowCount() = 0;

    /// Get a writer for writing BLOBs using previously created
    /// CDB_ITDescriptor
    /// @param d
    ///   Descriptor
    /// @param blob_size
    ///   Size of BLOB to write
    /// @param log_it
    ///   Enable or disable logging
    virtual IWriter* GetBlobWriter(I_ITDescriptor &d,
                                   size_t blob_size,
                                   EAllowLog log_it) = 0;

    /// Get an ostream for writing BLOBs using previously created
    /// CDB_ITDescriptor
    /// @param d
    ///   Descriptor
    /// @param blob_size
    ///   Size of BLOB to write
    /// @param log_it
    ///   Enable or disable logging
    /// @param buf_size
    ///   Buffer size, default 4096
    virtual CNcbiOstream& GetBlobOStream(I_ITDescriptor &d,
                                         size_t blob_size,
                                         EAllowLog log_it = eEnableLog,
                                         size_t buf_size = 0) = 0;

    /// Get the parent connection.
    ///
    /// If the original connections was cloned, returns cloned
    /// connection.
    virtual class IConnection* GetParentConn() = 0;

    /// Set auto-clear input parameter flag
    ///
    /// @param flag
    ///   auto-clear input parameter flag
    /// In case when flag == true implicitly clear a statement's parameter list
    /// after each Execute, ExecuteUpdate and ExecuteQuery call. Default value
    //. is true.
    virtual void SetAutoClearInParams(bool flag = true) = 0;

    /// Get auto-clear input parameter flag value
    ///
    /// @return
    ///   auto-clear input parameter flag value
    virtual bool IsAutoClearInParams(void) const = 0;

    /// Get input parameters metadata.
    ///
    /// @return
    ///   Pointer to result metadata.
    virtual const IResultSetMetaData& GetParamsMetaData(void) = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  ICallableStatement --
///
///  Used for calling a stored procedure thru RPC call

class NCBI_DBAPI_EXPORT ICallableStatement : public virtual IStatement
{
public:
    /// Destructor.
    virtual ~ICallableStatement();

    /// Execute stored procedure.
    virtual void Execute() = 0;

    /// Executes stored procedure no results returned.
    ///
    /// NOTE: All resultsets are discarded.
    virtual void ExecuteUpdate() = 0;

    /// Get return status from the stored procedure.
    virtual int GetReturnStatus() = 0;

    /// Set input parameters.
    ///
    /// @param v
    ///   Parameter value.
    /// @param name
    ///   Parameter name.
    virtual void SetParam(const CVariant& v,
                          const CDBParamVariant& param) = 0;

    /// Set output parameter, which will be returned as resultset.
    ///
    /// NOTE: Use CVariant(EDB_Type type) constructor or
    /// factory method CVariant::<type>(0) to create empty object
    /// of a particular type.
    /// @param v
    ///   Parameter value.
    /// @param name
    ///   Parameter name.
    virtual void SetOutputParam(const CVariant& v, 
            const CDBParamVariant& param) = 0;

protected:
    // Mask unused methods
    virtual void SendSql(const string& /*sql*/);
    virtual void Execute(const string& /*sql*/);
    virtual void ExecuteUpdate(const string& /*sql*/);
    virtual IResultSet* ExecuteQuery(const string& /*sql*/);

};


/////////////////////////////////////////////////////////////////////////////
///
///  ICursor --
///
///  Interface for a cursor.

class NCBI_DBAPI_EXPORT ICursor
{
public:
    /// Destructor.
    virtual ~ICursor();

    /// Set input parameter.
    ///
    /// @param v
    ///   Parameter value.
    /// @param name
    ///   Parameter name.
    virtual void SetParam(const CVariant& v,
                          const CDBParamVariant& param) = 0;

    /// Open cursor and get corresponding resultset.
    virtual IResultSet* Open() = 0;

    /// Get output stream for BLOB updates, requires BLOB column number.
    ///
    /// @param col
    ///   Column number.
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param log_it
    ///    Enables transaction log for BLOB (enabled by default).
    ///    Make sure you have enough log segment space, or disable it.
    /// @param buf_size
    ///   The size of internal buffer, default 4096.
    virtual CNcbiOstream& GetBlobOStream(unsigned int col,
                                         size_t blob_size,
                                         EAllowLog log_it = eEnableLog,
                                         size_t buf_size = 0) = 0;

    /// Get Blob Writer
    ///
    /// Implementation of IWriter interface
    /// @param col
    ///   Column number.
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param log_it
    ///   Enables transaction log for BLOB (enabled by default).
    ///   Make sure you have enough log segment space, or disable it.
    virtual IWriter* GetBlobWriter(unsigned int col,
                                   size_t blob_size,
                                   EAllowLog log_it = eEnableLog) = 0;
    /// Update statement for cursor.
    ///
    /// @param table
    ///   table name.
    /// @param updateSql
    ///   SQL statement.
    virtual void Update(const string& table, const string& updateSql) = 0;

    /// Delete statement for cursor.
    ///
    /// @param table
    ///   table name.
    virtual void Delete(const string& table) = 0;

    /// Cancel cursor
    virtual void Cancel() = 0;

    /// Close cursor
    virtual void Close() = 0;

    /// Get the parent connection
    ///
    /// NOTE: If the original connections was cloned, returns cloned
    /// connection.
    virtual class IConnection* GetParentConn() = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  IBulkInsert --
///
///  Interface for bulk insert

class NCBI_DBAPI_EXPORT IBulkInsert
{
public:
    /// Destructor.
    virtual ~IBulkInsert();

    /// Set hints by one call. Resets everything that was set by Add*Hint().
    virtual void SetHints(CTempString hints) = 0;

    /// Type of hint that can be set.
    enum EHints {
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
    virtual void AddHint(EHints hint, unsigned int value = 0) = 0;

    /// Add "ORDER" hint.
    /// Resets everything that was set by SetHints().
    virtual void AddOrderHint(CTempString columns) = 0;

    /// Bind column.
    ///
    /// @param col
    ///   Column number.
    /// @param v
    ///   Variant value.
    virtual void Bind(const CDBParamVariant& param, CVariant* v) = 0;

    /// Add row to the batch
    virtual void AddRow() = 0;

    /// Store batch of rows
    virtual void StoreBatch() = 0;

    /// Cancel bulk insert
    virtual void Cancel() = 0;

    /// Complete batch
    virtual void Complete() = 0;

    /// Close
    virtual void Close() = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  IConnection::
///
///  Interface for a database connection.

class IConnValidator;

class NCBI_DBAPI_EXPORT IConnection
{
public:
    /// Which connection mode.
    enum EConnMode {
        /// Bulk insert mode.
        /// This value is not needed anymore because BCP mode is enabled
        /// all the time in all drivers now.
        eBulkInsert = I_DriverContext::fBcpIn,
        /// Encrypted password mode.
        ePasswordEncrypted = I_DriverContext::fPasswordEncrypted
    };

    /// Destructor.
    virtual ~IConnection();

    // Connection modes

    /// Set connection mode.
    ///
    /// @param mode
    ///   Mode to set to.
    virtual void SetMode(EConnMode mode) = 0;

    /// Reset connection mode.
    ///
    /// @param mode
    ///   Mode to reset to.
    virtual void ResetMode(EConnMode mode) = 0;

    /// Get mode mask.
    virtual unsigned int GetModeMask() = 0;

    /// Force single connection mode, default false
    ///
    /// Disable this mode before using BLOB output streams
    /// from IResultSet, because extra connection is needed
    /// in this case.
    virtual void ForceSingle(bool enable) = 0;

    /// Get parent datasource object.
    virtual IDataSource* GetDataSource() = 0;

    /// Connect to a database.
    ///
    /// @param user
    ///   User name.
    /// @param password
    ///   User's password.
    /// @param server
    ///   Server to connect to.
    /// @param database
    ///   Database to connect to.
    virtual void Connect(const string& user,
             const string& password,
             const string& server,
             const string& database = kEmptyStr) = 0;

    /// Connect to a database.
    ///
    /// @param params
    ///   Connection parameters. Parameters should include all necessary
    ///   settings because all info set via SetMode() or ResetMode() will
    ///   be ignored.
    virtual void Connect(const CDBConnParams& params) = 0;

    /// Connect to a database using connect validator
    ///
    /// @param validator
    ///   Validator implementation class.
    /// @param user
    ///   User name.
    /// @param password
    ///   User's password.
    /// @param server
    ///   Server to connect to.
    /// @param database
    ///   Database to connect to.
    virtual void ConnectValidated(IConnValidator& validator,
             const string& user,
             const string& password,
             const string& server,
             const string& database = kEmptyStr) = 0;

    /// Clone existing connection. All settings are copied except
    /// message handlers
    /// Set ownership to eTakeOwnership to prevent deleting
    /// connection upon deleting parent object
    virtual IConnection* CloneConnection(EOwnership ownership = eNoOwnership) = 0;

    /// Set current database.
    ///
    /// @param name
    ///   Name of database to set to.
    virtual void SetDatabase(const string& name) = 0;

    /// Get current database
    virtual string GetDatabase() = 0;

    /// Check if the connection is alive
    virtual bool IsAlive() = 0;

    // NEW INTERFACE: no additional connections created
    // while using the next four methods.
    // Objects obtained with these methods can't be used
    // simultaneously (like opening cursor while a stored
    // procedure is running on the same connection).

    /// Get statement object for regular SQL queries.
    virtual IStatement* GetStatement() = 0;

    /// Get callable statement object for stored procedures.
    ///
    /// @param proc
    ///   Stored procedure name.
    /// @param nofArgs
    ///   Number of arguments.
    virtual ICallableStatement* GetCallableStatement(const string& proc) = 0;
    NCBI_DEPRECATED 
    ICallableStatement* GetCallableStatement(const string& proc, int) 
    {
        return GetCallableStatement(proc);
    }

    /// Get cursor object.
    virtual ICursor* GetCursor(const string& name,
                               const string& sql,
                               int batchSize) = 0;
    ICursor* GetCursor(const string& name,
                       const string& sql) 
    {
        return GetCursor(name, sql, 1);
    }
    NCBI_DEPRECATED 
    ICursor* GetCursor(const string& name,
                       const string& sql,
                       int,
                       int batchSize) 
    {
        return GetCursor(name, sql, batchSize);
    }

    /// Create bulk insert object.
    ///
    /// @param table_name
    ///   table name.
    /// @param nof_cols
    ///   Number of columns.
    virtual IBulkInsert* GetBulkInsert(const string& table_name) = 0;
    NCBI_DEPRECATED 
    IBulkInsert* GetBulkInsert(const string& table_name, unsigned int)
    {
        return GetBulkInsert(table_name);
    }

    // END OF NEW INTERFACE

    /// Get statement object for regular SQL queries.
    virtual IStatement* CreateStatement() = 0;

    /// Get callable statement object for stored procedures.
    virtual ICallableStatement* PrepareCall(const string& proc) = 0;
    NCBI_DEPRECATED 
    ICallableStatement* PrepareCall(const string& proc, int)
    {
        return PrepareCall(proc);
    }

    /// Get cursor object.
    virtual ICursor* CreateCursor(const string& name,
                                  const string& sql,
                                  int batchSize) = 0;
    ICursor* CreateCursor(const string& name,
                          const string& sql) 
    {
        return CreateCursor(name, sql, 1);
    }
    NCBI_DEPRECATED 
    ICursor* CreateCursor(const string& name,
                          const string& sql,
                          int,
                          int batchSize)
    {
        return CreateCursor(name, sql, batchSize);
    }

    /// Create bulk insert object.
    virtual IBulkInsert* CreateBulkInsert(const string& table_name) = 0;
    NCBI_DEPRECATED 
    IBulkInsert* CreateBulkInsert(const string& table_name, unsigned int)
    {
        return CreateBulkInsert(table_name);
    }

    /// Close connecti
    virtual void Close() = 0;

    /// Abort connection.
    virtual void Abort() = 0;

    /// Set connection timeout.
    /// NOTE:  if "nof_secs" is zero or is "too big" (depends on the underlying
    ///        DB API), then set the timeout to infinite.
    virtual void SetTimeout(size_t nof_secs) = 0;

    /// Set timeout for command cancellation and connection closing
    virtual void SetCancelTimeout(size_t nof_secs) {}

    /// If enabled, redirects all error messages
    /// to CDB_MultiEx object (see below).
    virtual void MsgToEx(bool v) = 0;

    /// Returns all error messages as a CDB_MultiEx object.
    virtual CDB_MultiEx* GetErrorAsEx() = 0;

    /// Returns all error messages as a single string
    virtual string GetErrorInfo() = 0;

    /// Returns the internal driver connection object
    virtual CDB_Connection* GetCDB_Connection() = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  IDataSource --
///
///  Interface for a datasource

class NCBI_DBAPI_EXPORT IDataSource
{
    friend class CDriverManager;

protected:
    /// Protected Destructor.
    ///
    /// Prohibits explicit deletion.
    /// Use CDriverManager::DestroyDs() call, instead.
    virtual ~IDataSource();

public:
    // Get connection
    // Set ownership to eTakeOwnership to prevent deleting
    // connection upon deleting parent object
    virtual IConnection* CreateConnection(EOwnership ownership = eNoOwnership) = 0;

    /// Set login timeout.
    virtual void SetLoginTimeout(unsigned int i) = 0;

    /// Set the output stream for server messages.
    ///
    /// Set it to zero to disable any output and collect
    /// messages in CDB_MultiEx (see below).
    /// @param out
    ///   Output stream to set to.
    virtual void SetLogStream(ostream* out) = 0;

    /// Returns all server messages as a CDB_MultiEx object.
    virtual CDB_MultiEx* GetErrorAsEx() = 0;

    /// Returns all server messages as a single string.
    virtual string GetErrorInfo() = 0;

    /// Returns the pointer to the general driver interface.
    virtual I_DriverContext* GetDriverContext() = 0;
    virtual const I_DriverContext* GetDriverContext() const = 0;

    // app_name defines the application name that a connection will use when
    // connecting to a server.
    void SetApplicationName(const string& app_name);
    string GetApplicationName(void) const;
};


////////////////////////////////////////////////////////////////////////////////
inline
CAutoTrans::CSubject DBAPI_MakeTrans(IConnection& connection)
{
    return DBAPI_MakeTrans(*connection.GetCDB_Connection());
}


END_NCBI_SCOPE


/* @} */

#endif  /* DBAPI___DBAPI__HPP */
