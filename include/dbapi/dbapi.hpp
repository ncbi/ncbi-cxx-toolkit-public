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
 * File Description: Database API interface
 *
 */

#include <corelib/ncbiobj.hpp>
#include <dbapi/driver_mgr.hpp>
#include <dbapi/variant.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  EDataSource::
//
//  Data source platform
//
// enum EDataSource {
//   eSybase,
//   eMsSql
// };



/////////////////////////////////////////////////////////////////////////////
//
//  EParamType::
//
//  Parameter type
//
// enum EParamType {
//   eIn,
//   eOut
// };  



/////////////////////////////////////////////////////////////////////////////
//
//  IResultSetMetaData::
//
//  Used for retrieving column information from a resultset, such as 
//  total number of columns, type, name, etc.
//

class IResultSetMetaData 
{
public:
    virtual ~IResultSetMetaData();

    virtual unsigned int GetTotalColumns() const = 0;
    virtual EDB_Type     GetType    (unsigned int col) const = 0;
    virtual int          GetMaxSize (unsigned int col) const = 0;
    virtual string       GetName    (unsigned int col) const = 0;
};



/////////////////////////////////////////////////////////////////////////////
//
//  IResultSet::
//
//  Used to retrieve a resultset from a query or cursor
//

class IResultSet 
{
public:
    virtual ~IResultSet();

    // See in <dbapi/driver/interfaces.hpp> for the list of result types
    virtual EDB_ResType GetResultType() = 0;

    // Get next row.
    // NOTE:  no results are fetched before first call to this function.
    virtual bool Next() = 0;

    // All data (except for BLOB data) is returned as CVariant.
    virtual const CVariant& GetVariant(unsigned int col) = 0;
    virtual const CVariant& GetVariant(const string& colName) = 0;
  
    // Reads unformatted data, returns bytes actually read.
    // Advances to next column as soon as data is read from the previous one.
    virtual size_t Read(void* buf, size_t size) = 0;
  
    // Streams for handling BLOBs.
    // NOTE: buf_size is the size of internal buffer, default 1024
    // blob_size is the size of the BLOB to be written
    virtual istream& GetBlobIStream(size_t buf_size = 1024) = 0;
    virtual ostream& GetBlobOStream(size_t blob_size, 
                                    size_t buf_size = 1024) = 0;

    // Close resultset
    virtual void Close() = 0;

    // Get column description.
    virtual const IResultSetMetaData* GetMetaData() = 0;
};


/////////////////////////////////////////////////////////////////////////////
//
//  IStatement::
//
//  Interface for a SQL statement
//
class IStatement
{
public:
    virtual ~IStatement();
  
    // Get resulset. For statements with no resultset returns 0
    virtual IResultSet* GetResultSet() = 0;

    // Check if there is more results available
    virtual bool HasMoreResults() = 0;
  
    // Check if resultset is not empty
    virtual bool HasRows() = 0;

    // Cancel statement
    virtual void Cancel() = 0;

    // Close statement
    virtual void Close() = 0;
  
    // Executes one or more SQL statements
    virtual void Execute(const string& sql) = 0;

    // Executes SQL statement with no results returned
    // NOTE: All resultsets are discarded.
    virtual void ExecuteUpdate(const string& sql) = 0;

    // Set input parameter
    virtual void SetParam(const CVariant& v, 
	  		  const string& name) = 0;

    // Get total of rows returned. 
    // NOTE: Valid only after all rows are retrieved from a resultset
    virtual int GetRowCount() = 0;

};

/////////////////////////////////////////////////////////////////////////////
//
//  ICallableStatement::
//
//  Used for calling a stored procedure thru RPC call
//
class ICallableStatement : public virtual IStatement
{
public:
    virtual ~ICallableStatement();
  
    // Execute stored procedure
    virtual void Execute() = 0;

    // Executes stored procedure no results returned
    // NOTE: All resultsets are discarded.
    virtual void ExecuteUpdate() = 0;

    // Get return status from the stored procedure
    virtual int GetReturnStatus() = 0;

    // Set input parameters
    virtual void SetParam(const CVariant& v, 
                          const string& name) = 0;

    // Set output parameter, which will be returned as resultset
    // NOTE: use CVariant(EDB_Type type) constructor or 
    // factory method CVariant::<type>(0) to create empty object
    // of a particular type
    virtual void SetOutputParam(const CVariant& v, const string& name) = 0;

protected:
    // Mask unused methods
    virtual void Execute(const string& /*sql*/);
    virtual void ExecuteUpdate(const string& /*sql*/);
  

};

/////////////////////////////////////////////////////////////////////////////
//
//  ICursor::
//
//  Interface for a cursor
//
class ICursor
{
public:

    virtual ~ICursor();
  
    // Set input parameter
    virtual void SetParam(const CVariant& v, 
			const string& name) = 0;

    // Open cursor and get corresponding resultset
    virtual IResultSet* Open() = 0;

    // Update statement for cursor
    virtual void Update(const string& table, const string& updateSql) = 0;

    // Delete statement for cursor
    virtual void Delete(const string& table) = 0;

    // Close cursor
    virtual void Close() = 0;
  
};



/////////////////////////////////////////////////////////////////////////////
//
//  IConnection::
//
//  Interface for a database connection
//
class IConnection 
{
public:
    virtual ~IConnection();

    // Connect to the database
    virtual void Connect(const string& user,
			 const string& password,
			 const string& server,
			 const string& database = kEmptyStr) = 0;

    // Set current database
    virtual void SetDatabase(const string& name) = 0;

    // Get current database
    virtual string GetDatabase() = 0;

    // Get statement object for regular SQL queries
    virtual IStatement* CreateStatement() = 0;
  
    // Get callable statement object for stored procedures
    virtual ICallableStatement* PrepareCall(const string& proc, 
					    int nofArgs) = 0;

    // Get cursor object
    virtual ICursor* CreateCursor(const string& name, 
				  const string& sql,
				  int nofArgs = 0,
				  int batchSize = 1) = 0;
    // Close connection
    virtual void Close() = 0;
};

/////////////////////////////////////////////////////////////////////////////
//
//  IDataSource::
//
//  Interface for a datasource
//
class IDataSource
{
public:


    virtual ~IDataSource();

    // Get connection
    virtual IConnection* CreateConnection() = 0;

    virtual void SetLoginTimeout(unsigned int i) = 0;
    virtual void SetLogStream(ostream* out) = 0;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
