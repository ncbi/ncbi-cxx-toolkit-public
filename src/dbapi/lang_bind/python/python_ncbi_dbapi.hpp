/*  $Id$
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
* Author: Sergey Sikorskiy
*
* File Description:
* Status: *Initial*
*
* ===========================================================================
*/

#include <dbapi/dbapi.hpp>

#include "pythonpp/pythonpp_ext.hpp"
#include "pythonpp/pythonpp_dict.hpp"

BEGIN_NCBI_SCOPE

namespace python
{

//////////////////////////////////////////////////////////////////////////////
class CBinary : public pythonpp::CExtObject<CBinary>
{
public:
    CBinary(void);
    ~CBinary(void);
};

//////////////////////////////////////////////////////////////////////////////
class CNumber : public pythonpp::CExtObject<CNumber>
{
public:
    CNumber(void);
    ~CNumber(void);
};

//////////////////////////////////////////////////////////////////////////////
class CRowID : public pythonpp::CExtObject<CRowID>
{
public:
    CRowID(void);
    ~CRowID(void);
};

//////////////////////////////////////////////////////////////////////////////
class CConnection;

class CCursor : public pythonpp::CExtObject<CCursor>
{
public:
    CCursor(CConnection* conn);
    ~CCursor(void);

public:
    // Python methods ...

    /// Call a stored database procedure with the given name. The
    /// sequence of parameters must contain one entry for each
    /// argument that the procedure expects. The result of the
    /// call is returned as modified copy of the input
    /// sequence. Input parameters are left untouched, output and
    /// input/output parameters replaced with possibly new values.
    /// callproc(procname[,parameters]);
    pythonpp::CObject callproc(const pythonpp::CTuple& args);
    /// Close the cursor now (rather than whenever __del__ is
    /// called).  The cursor will be unusable from this point
    /// forward; an Error (or subclass) exception will be raised
    /// if any operation is attempted with the cursor.
    /// close();
    pythonpp::CObject close(const pythonpp::CTuple& args);
    /// Prepare and execute a database operation (query or
    /// command).  Parameters may be provided as sequence or
    /// mapping and will be bound to variables in the operation.
    /// Variables are specified in a database-specific notation
    /// (see the module's paramstyle attribute for details). [5]
    /// execute(operation[,parameters]);
    pythonpp::CObject execute(const pythonpp::CTuple& args);
    /// Prepare a database operation (query or command) and then
    /// execute it against all parameter sequences or mappings
    /// found in the sequence seq_of_parameters.
    /// executemany(operation,seq_of_parameters);
    pythonpp::CObject executemany(const pythonpp::CTuple& args);
    /// Fetch the next row of a query result set, returning a
    /// single sequence, or None when no more data is
    /// available. [6]
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchone();
    pythonpp::CObject fetchone(const pythonpp::CTuple& args);
    /// Fetch the next set of rows of a query result, returning a
    /// sequence of sequences (e.g. a list of tuples). An empty
    /// sequence is returned when no more rows are available.
    /// The number of rows to fetch per call is specified by the
    /// parameter.  If it is not given, the cursor's arraysize
    /// determines the number of rows to be fetched. The method
    /// should try to fetch as many rows as indicated by the size
    /// parameter. If this is not possible due to the specified
    /// number of rows not being available, fewer rows may be
    /// returned.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchmany([size=cursor.arraysize]);
    pythonpp::CObject fetchmany(const pythonpp::CTuple& args);
    /// Fetch all (remaining) rows of a query result, returning
    /// them as a sequence of sequences (e.g. a list of tuples).
    /// Note that the cursor's arraysize attribute can affect the
    /// performance of this operation.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchall();
    pythonpp::CObject fetchall(const pythonpp::CTuple& args);
    /// This method will make the cursor skip to the next
    /// available set, discarding any remaining rows from the
    /// current set.
    /// If there are no more sets, the method returns
    /// None. Otherwise, it returns a true value and subsequent
    /// calls to the fetch methods will return rows from the next
    /// result set.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// nextset();
    pythonpp::CObject nextset(const pythonpp::CTuple& args);
    /// This can be used before a call to executeXXX() to
    /// predefine memory areas for the operation's parameters.
    /// sizes is specified as a sequence -- one item for each
    /// input parameter.  The item should be a Type Object that
    /// corresponds to the input that will be used, or it should
    /// be an integer specifying the maximum length of a string
    /// parameter.  If the item is None, then no predefined memory
    /// area will be reserved for that column (this is useful to
    /// avoid predefined areas for large inputs).
    /// This method would be used before the executeXXX() method
    /// is invoked.
    /// setinputsizes(sizes);
    pythonpp::CObject setinputsizes(const pythonpp::CTuple& args);
    /// Set a column buffer size for fetches of large columns
    /// (e.g. LONGs, BLOBs, etc.).  The column is specified as an
    /// index into the result sequence.  Not specifying the column
    /// will set the default size for all large columns in the
    /// cursor.
    /// This method would be used before the executeXXX() method
    /// is invoked.
    /// setoutputsize(size[,column]);
    pythonpp::CObject setoutputsize(const pythonpp::CTuple& args);

private:
    enum EStatementType {
        estNone,
        estSelect,
        estInsert,
        estDelete,
        estUpdate,
        estCreate,
        estDrop,
        estAlter,
        estFunction
        };

private:
    IConnection* GetDBConnection(void) const;
    IStatement& GetStmt(void);
    ICallableStatement& GetCallableStmt(void);
    static EStatementType RetrieveStatementType(const string& stmt);
    const pythonpp::CTuple MakeResultTuple(const IResultSet& rs) const;
    CVariant GetCVariant(const pythonpp::CObject& obj) const;
    void SetUpParameters(const pythonpp::CDict& dict);
    void ExecuteCurrStatement(void);

    static bool isDML(EStatementType stmtType)
    {
        return stmtType == estInsert || stmtType == estDelete || stmtType == estUpdate;
    }
    static bool isDDL(EStatementType stmtType)
    {
        return stmtType == estCreate || stmtType == estDrop || stmtType == estAlter;
    }
    string GetStmtStr(void) const
    {
        return m_StmtStr;
    }

private:
    pythonpp::CObject               m_PythonConnection; //< For reference counting purposes only
    CConnection*                    m_ParentConnection; //< A connection to which belongs this cursor object
    auto_ptr<IStatement>            m_Stmt;             //< DBAPI SQL statement interface
    auto_ptr<ICallableStatement>    m_CallableStmt;     //< DBAPI stored procedure interface
    string                          m_StmtStr;
    int                             m_RowsNum;
    auto_ptr<IResultSet>            m_RS;
    EStatementType                  m_StatementType;
    size_t                          m_ArraySize;
};

//////////////////////////////////////////////////////////////////////////////
class CConnection : public pythonpp::CExtObject<CConnection>
{
public:
    CConnection(
        const string& driver_name,
        const string& db_type,
        const string& server_name,
        const string& db_name,
        const string& user_name,
        const string& user_pswd
        );
    ~CConnection(void);

public:
    pythonpp::CObject close(const pythonpp::CTuple& args);
    pythonpp::CObject cursor(const pythonpp::CTuple& args);
    pythonpp::CObject commit(const pythonpp::CTuple& args);
    pythonpp::CObject rollback(const pythonpp::CTuple& args);

public:
    IConnection* GetDBConnection(void) const;

protected:
    enum EServerType {
        eSybase,    //< Sybase server
        eMsSql,     //< Microsoft SQL server
        eOracle,    //< ORACLE server
        eSqlite,    //< SQLITE database
        eMySql,     //< MySQL server
        eUnknown    //< Server type is not known
    };
    typedef map<string, string> TDatabaseParameters;

protected:
    /// Return current driver name
    const string& GetDriverName(void) const
    {
        return m_driver_name;
    }
    /// Return current server type
    EServerType GetServerType(void) const
    {
        return m_ServerType;
    }
    IStatement& GetStmt(void)
    {
        return *m_Stmt;
    }

protected:
    void CreateConnection(void);

private:
    const string m_driver_name;
    const string m_db_type;
    const string m_server_name;
    const string m_db_name;
    const string m_user_name;
    const string m_user_pswd;

    EServerType             m_ServerType;
    CDriverManager&         m_DM;
    IDataSource*            m_DS;
    auto_ptr<IConnection>   m_Conn;
    auto_ptr<IStatement>    m_Stmt;
};

inline
IConnection*
CCursor::GetDBConnection(void) const
{
    return m_ParentConnection->GetDBConnection();
}

}

END_NCBI_SCOPE

/* ===========================================================================
*
* $Log$
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
