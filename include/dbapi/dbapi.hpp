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
#include <dbapi/driver_mgr.hpp>
#include <dbapi/variant.hpp>
#include <util/reader_writer.hpp>

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
    virtual ~IResultSetMetaData();

    /// Get total number of columns in resultset.
    virtual unsigned int GetTotalColumns() const = 0;

    /// Get data type for column in the resultset.
    ///
    /// @param col
    ///   Column number
    virtual EDB_Type     GetType    (unsigned int col) const = 0;

    /// Get maximum size for column.
    ///
    /// @param col
    ///   Column number
    virtual int          GetMaxSize (unsigned int col) const = 0;

    /// Get name of column.
    ///
    /// @param col
    ///   Column number
    virtual string       GetName    (unsigned int col) const = 0;
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
    /// @param col
    ///   Column number (one-based)
    /// @return
    ///   All data (for BLOB data see below) is returned as CVariant.
    virtual const CVariant& GetVariant(unsigned int col) = 0;

    /// Get variant given a column name.
    ///
    /// @param colName
    ///   Column name.
    /// @return
    ///   All data (for BLOB data see below) is returned as CVariant.
    virtual const CVariant& GetVariant(const string& colName) = 0;

    /// Disables column binding.
    ///
    /// False by default.
    /// @param
    ///   Disables binding when set to true.
    virtual void DisableBind(bool b) = 0;

    /// Bind blob to variant.
    ///
    /// If this mode is true, BLOB data is returned as CVariant
    /// False by default.
    /// @param
    ///   Disables blob binding when set to true.
    virtual void BindBlobToVariant(bool b) = 0;
  
    /// Read unformtted data.
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

    /// Get column number.
    ///
    /// @return
    ///   Returns current column number (while using Read()).
    virtual int GetColumnNo() = 0;

    /// Get total columns.
    ///
    /// @return
    ///   Returns total number of columns in the resultset
    virtual unsigned int GetTotalColumns() = 0;
  
    /// Get Blob output stream.
    ///
    /// @param buf_size
    ///   buf_size is the size of internal buffer, default 1024.
    virtual istream& GetBlobIStream(size_t buf_size = 1024) = 0;

    /// Get Blob output stream. The existing connection is
    /// cloned for writing blob.
    ///
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param log_it
    ///    Enables transaction log for BLOB (enabled by default).
    ///    Make sure you have enough log segment space, or disable it.
    /// @param buf_size
    ///   The size of internal buffer, default 1024.
    virtual ostream& GetBlobOStream(size_t blob_size, 
                                    EAllowLog log_it = eEnableLog,
                                    size_t buf_size = 1024) = 0;

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
    ///   The size of internal buffer, default 1024.
    virtual ostream& GetBlobOStream(IConnection *conn,
                                    size_t blob_size, 
                                    EAllowLog log_it = eEnableLog,
                                    size_t buf_size = 1024) = 0;
    /// Get the Blob Reader.
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
    virtual const IResultSetMetaData* GetMetaData() = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  IStatement --
///
///  Interface for a SQL statement

class CDB_ITDescriptor;

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

    /// Check for more results available..
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
  
    /// Executes one or more SQL statements.
    ///
    /// @param sql
    ///   SQL statement to execute.
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
    /// @param sql
    ///   SQL statement to execute.
    /// @return
    ///   Pointer to result set.
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
	  		  const string& name) = 0;

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
    virtual IWriter* GetBlobWriter(CDB_ITDescriptor &d, 
                                   size_t blob_size, 
                                   EAllowLog log_it) = 0;

    /// Get the parent connection.
    ///
    /// If the original connections was cloned, returns cloned
    /// connection.
    virtual class IConnection* GetParentConn() = 0;

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
                          const string& name) = 0;

    /// Set output parameter, which will be returned as resultset.
    ///
    /// NOTE: Use CVariant(EDB_Type type) constructor or 
    /// factory method CVariant::<type>(0) to create empty object
    /// of a particular type.
    /// @param v
    ///   Parameter value.
    /// @param name
    ///   Parameter name.
    virtual void SetOutputParam(const CVariant& v, const string& name) = 0;

protected:
    // Mask unused methods
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
			const string& name) = 0;

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
    ///   The size of internal buffer, default 1024.
    virtual ostream& GetBlobOStream(unsigned int col,
                                    size_t blob_size, 
                                    EAllowLog log_it = eEnableLog,
                                    size_t buf_size = 1024) = 0;

    /// Get Blob Writer.
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
  
    /// Bind column.
    ///
    /// @param col
    ///   Column number.
    /// @param v
    ///   Variant value.
    virtual void Bind(unsigned int col, CVariant* v) = 0;

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

class NCBI_DBAPI_EXPORT IConnection 
{
public:
    /// Which connection mode.
    enum EConnMode {
        eBulkInsert = I_DriverContext::fBcpIn,  ///< Bulk insert mode.
        ePasswordEncrypted  ///< Encrypted password mode.
                    = I_DriverContext::fPasswordEncrypted 
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

    /// Connect to the database.
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

    // Clone existing connection. All settings are copied except
    // message handlers
    // Set ownership to eTakeOwnership to prevent deleting
    // connection upon deleting parent object
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
    virtual ICallableStatement* GetCallableStatement(const string& proc, 
                                                     int nofArgs = 0) = 0;

    /// Get cursor object.
    virtual ICursor* GetCursor(const string& name, 
                               const string& sql,
                               int nofArgs = 0,
                               int batchSize = 1) = 0;

    /// Create bulk insert object.
    ///
    /// @param table_name
    ///   table name.
    /// @param nof_cols
    ///   Number of columns.
    virtual IBulkInsert* GetBulkInsert(const string& table_name,
                                       unsigned int nof_cols) = 0;
    // END OF NEW INTERFACE

    /// Get statement object for regular SQL queries.
    virtual IStatement* CreateStatement() = 0;
  
    /// Get callable statement object for stored procedures.
    virtual ICallableStatement* PrepareCall(const string& proc, 
                                            int nofArgs = 0) = 0;

    /// Get cursor object.
    virtual ICursor* CreateCursor(const string& name, 
                                  const string& sql,
                                  int nofArgs = 0,
                                  int batchSize = 1) = 0;

    /// Create bulk insert object.
    virtual IBulkInsert* CreateBulkInsert(const string& table_name,
                                          unsigned int nof_cols) = 0;
    /// Close connection.
    virtual void Close() = 0;

    /// Abort connection.
    virtual void Abort() = 0;

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
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.37  2005/02/24 19:50:42  kholodov
 * Added: IConnection::Abort() method
 *
 * Revision 1.36  2005/01/31 14:22:07  kholodov
 * Added: use of CDB_ITDescriptor for writing BLOBs
 *
 * Revision 1.35  2005/01/13 15:30:03  dicuccio
 * Adjusted comment for GetVariant(): note that column index is 1-based, not
 * 0-based
 *
 * Revision 1.34  2004/11/16 19:58:28  kholodov
 * Added: GetBlobOStream() with explicit connection
 *
 * Revision 1.33  2004/07/28 18:40:56  kholodov
 * Added: setting ownership for connection objects
 *
 * Revision 1.32  2004/07/26 14:41:38  siyan
 * Added doxygen comments.
 *
 * Revision 1.31  2004/07/20 17:55:09  kholodov
 * Added: IReader/IWriter support for BLOB I/O
 *
 * Revision 1.30  2004/04/26 14:17:40  kholodov
 * Added: ExecuteQuery() method
 *
 * Revision 1.29  2004/04/22 15:15:09  kholodov
 * Added: PurgeResults()
 *
 * Revision 1.28  2004/04/22 14:22:51  kholodov
 * Added: Cancel()
 *
 * Revision 1.27  2004/03/09 20:37:37  kholodov
 * Added: four new public methods
 *
 * Revision 1.26  2003/12/15 20:05:40  ivanov
 * Added export specifier for building DLLs in MS Windows.
 *
 * Revision 1.25  2003/11/18 17:32:26  ucko
 * Make IConnection::CloneConnection pure virtual.
 *
 * Revision 1.24  2003/11/18 17:00:36  kholodov
 * Added: CloneConnection() method to IConnection interface
 *
 * Revision 1.23  2003/09/10 18:28:04  kholodov
 * Added: GetCDB_Connection() method
 *
 * Revision 1.22  2003/05/16 20:17:01  kholodov
 * Modified: default 0 arguments in PrepareCall()
 *
 * Revision 1.21  2003/04/11 17:45:55  siyan
 * Added doxygen support
 *
 * Revision 1.20  2003/03/07 21:22:31  kholodov
 * Added: IsAlive() method
 *
 * Revision 1.19  2003/02/12 15:53:23  kholodov
 * Added: WasNull() method
 *
 * Revision 1.18  2003/02/10 17:17:15  kholodov
 * Modified: made IDataSource::dtor() non-public
 *
 * Revision 1.17  2002/11/27 17:21:30  kholodov
 * Added: Error output redirection to CToMultiExHandler object
 * in IConnection interface.
 *
 * Revision 1.16  2002/10/31 22:37:12  kholodov
 * Added: DisableBind(), GetColumnNo(), GetTotalColumns() methods
 *
 * Revision 1.15  2002/10/21 20:38:17  kholodov
 * Added: GetParentConn() method to get the parent connection from IStatement,
 * ICallableStatement and ICursor objects.
 *
 * Revision 1.14  2002/10/04 12:53:20  kholodov
 * Fixed: IStatement::SetParam() accepts both imput and output parameters
 *
 * Revision 1.13  2002/10/03 18:51:03  kholodov
 * Added: IStatement::ExecuteLast() method
 * Added: IBulkInsert::Close() method
 *
 * Revision 1.12  2002/09/30 20:45:38  kholodov
 * Added: ForceSingle() method to enforce single connection used
 *
 * Revision 1.11  2002/09/23 18:24:12  kholodov
 * Added: IDataSource: GetErrorInfo() and GetErrorAsEx() methods.
 * Added: IConnection: GetDataSource() method.
 *
 * Revision 1.10  2002/09/16 19:34:41  kholodov
 * Added: bulk insert support
 *
 * Revision 1.9  2002/09/09 20:49:49  kholodov
 * Added: IStatement::Failed() method
 *
 * Revision 1.8  2002/08/26 15:37:58  kholodov
 * Added EAllowLog general purpose enum.
 * Modified GetBlobOStream() methods to allow disabling transaction log while updating BLOBs
 *
 * Revision 1.7  2002/07/08 15:57:53  kholodov
 * Added: GetBlobOStream() for BLOB updates to ICursor interface
 *
 * Revision 1.6  2002/04/25 13:35:05  kholodov
 * Added GetDriverContext() returning pointer to underlying I_DriverContext interface
 *
 * Revision 1.5  2002/04/05 19:36:16  kholodov
 * Added: IResultset::GetVariant(const string& colName) to retrieve
 * column values by column name
 * Added: ICallableStatement::ExecuteUpdate() to skip all resultsets returned
 * if any
 *
 * Revision 1.4  2002/02/08 22:43:12  kholodov
 * Set/GetDataBase() renamed to Set/GetDatabase() respectively
 *
 * Revision 1.3  2002/02/08 21:29:55  kholodov
 * SetDataBase() restored, connection cloning algorithm changed
 *
 * Revision 1.2  2002/02/08 17:47:35  kholodov
 * Removed SetDataBase() method
 *
 * Revision 1.1  2002/01/30 14:51:24  kholodov
 * User DBAPI implementation, first commit
 *
 * ===========================================================================
 */

#endif  /* DBAPI___DBAPI__HPP */
