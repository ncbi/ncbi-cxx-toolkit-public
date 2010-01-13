#ifndef CPPCORE__DBAPI__SIMPLE__SDBAPI__HPP
#define CPPCORE__DBAPI__SIMPLE__SDBAPI__HPP

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
 * File Description:  Simple database API interface
 *
 */

#include <string>
#include <map>
#include <vector>

#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE

class CDatabase;
class CDatabaseImpl;
class CBulkInsertImpl;
class CQueryImpl;
struct SQueryParamInfo;


/// Exception class used throughout the API
class CSDB_Exception : public CException
{
public:
    enum EErrCode {
        eEmptyDBName,   ///< Empty database name is used to create CDatabase
        eClosed,        ///< CDatabase/CQuery/CBulkInsert is tried to be used
                        ///< when no connection is opened
        eStarted,       ///< CBulkInsert has already started to send data, no
                        ///< changes in meta-information can be made
        eNotInOrder,    ///< Columns cannot be bound to CBulkInsert randomly
        eInconsistent,  ///< Operation logically incorrect is attempted to be
                        ///< made (increase past end() iterator, incorrect
                        ///< number of values in CBulkInsert etc)
        eUnsupported,   ///< Unsupported data type conversion is requested
        eOutOfBounds,   ///< Conversion of string to integer type exceeded
                        ///< limits of requested type
        eNotExist,      ///< Field/parameter with given name/position does not
                        ///< exist
        eLowLevel       ///< Exception from low level DBAPI was re-thrown with
                        ///< this exception class
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;
    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CSDB_Exception, CException);
};


/// Database types used throughout API
enum ESDB_Type {
    eSDB_Byte,
    eSDB_Short,
    eSDB_Int4,
    eSDB_Int8,
    eSDB_Float,
    eSDB_Double,
    eSDB_String,
    eSDB_Binary,
    eSDB_DateTime,
    eSDB_Text,
    eSDB_Image
};

/// Stored procedure and statement parameter types
enum ESP_ParamType {
    eSP_In,     ///< Parameter is only passed to server, no return is expected
    eSP_InOut   ///< Parameter can be returned from stored procedure
};


/// Object used to execute queries and stored procedures on the database
/// server and retrieve result sets.
class CQuery
{
public:
    /// Empty constructor of query object.
    /// Object created this way cannot be used for anything except assigning
    /// from the other query object.
    CQuery(void);
    ~CQuery(void);

    /// Copying of query object from other one.
    /// The copy of query object behaves with the same internal result set as
    /// the original object. So that if you increment iterator created from
    /// one object it will move to the next row in another query object too.
    CQuery(const CQuery& q);
    CQuery& operator= (const CQuery& q);

    /// Class representing value in result set or output parameter of stored
    /// procedure.
    class CField
    {
    public:
        /// Get value as string.
        /// Any underlying database type will be converted to string.
        string AsString(void) const;
        /// Get value as single byte.
        /// If underlying database type is string or text then attempt to
        /// convert it to byte will be made. If the value cannot be converted
        /// to integer or resulting integer cannot fit into byte exception
        /// will be thrown.
        unsigned char AsByte(void) const;
        /// Get value as short integer.
        /// If underlying database type is string or text then attempt to
        /// convert it to byte will be made. If the value cannot be converted
        /// to integer or resulting integer cannot fit into short data type
        /// exception will be thrown.
        short AsShort(void) const;
        /// Get value as 4-byte integer.
        /// If underlying database type is string or text then attempt to
        /// convert it to integer will be made. If the value cannot be
        /// converted to integer or resulting integer cannot fit into
        /// 4 bytes exception will be thrown.
        Int4 AsInt4(void) const;
        /// Get value as 8-byte integer.
        /// If underlying database type is string or text then attempt to
        /// convert it to integer will be made. If the value cannot be
        /// converted to integer exception will be thrown.
        Int8 AsInt8(void) const;
        /// Get value as float.
        /// If underlying database type is string or text then attempt to
        /// convert it to double will be made. If the value cannot be
        /// converted to double exception will be thrown.
        float AsFloat(void) const;
        /// Get value as double.
        /// If underlying database type is string or text then attempt to
        /// convert it to double will be made. If the value cannot be
        /// converted to double exception will be thrown.
        double AsDouble(void) const;
        /// Get value as bool.
        /// If underlying database type is string or text then attempt to
        /// convert it to integer will be made. If the value cannot be
        /// converted to integer or resulting integer is not equal to 0 or 1
        /// (also if underlying type is integer and it's not equal to 0 or 1)
        /// exception will be thrown.
        bool AsBool(void) const;
        /// Get value as CTime.
        /// If underlying database type is string or text then attempt to
        /// convert it to CTime will be made. If the value cannot be
        /// converted to CTime exception will be thrown.
        CTime AsDateTime(void) const;
        /// Get value as input stream.
        /// Only Text and Image data can be read via stream. Returned stream
        /// should be read completely before any other field is attempted
        /// to be read.
        CNcbiIstream& AsIStream(void) const;
        /// Get value as vector of bytes.
        /// Only Text and Image data can be read as vector. Returned vector
        /// should be read completely before any other field is attempted
        /// to be read.
        const vector<unsigned char>& AsVector(void) const;
        
    private:
        friend class CQueryImpl;
        friend struct Deleter<CField>;

        /// Prohibit any copying
        CField(const CField& f);
        CField& operator= (const CField& f);

        /// Create field for particular column number in result set
        CField(CQueryImpl* q, unsigned int col_num);
        /// Create field for particular parameter in the query
        CField(CQueryImpl* q, SQueryParamInfo* param_info);
        ~CField(void);


        /// Flag showing whether this field is for parameter or column value
        bool                             m_IsParam;
        /// Query the field was created for
        CQueryImpl*                      m_Query;
        /// Parameter the field was created for
        SQueryParamInfo*                 m_ParamInfo;
        /// Column number the field was created for
        unsigned int                     m_ColNum;
        /// Vector to represent Text/Image value
        mutable vector<unsigned char>    m_Vector;
        /// Stream to represent Text/Image value
        mutable AutoPtr<CNcbiIstrstream> m_Stream;
    };

    /// Iterator class doing main navigation through result sets.
    class CRowIterator
    {
    public:
        /// Empty constructor of iterator.
        /// Object constructed this way cannot be used for anything except
        /// assigning from another iterator object.
        CRowIterator(void);
        ~CRowIterator(void);

        /// Copying of iterator object.
        /// Different copies of iterator won't point to different rows in the
        /// result set, i.e. incrementing one iterator will change values
        /// returned from the other.
        CRowIterator(const CRowIterator& ri);
        CRowIterator& operator= (const CRowIterator& ri);

        /// Get row number currently active.
        /// Row numbers are assigned consecutively to each row in all result
        /// sets returned starting with 1. Row number is not reset after
        /// passing result set boundary.
        unsigned int GetRowNo(void);
        /// Get number of currently active result set.
        /// All result sets are numbered starting with 1.
        unsigned int GetResultSetNo(void);

        /// Get column value by its number.
        /// All columns are numbered starting with 1.
        const CField& operator[](int col) const;
        /// Get column value by its name.
        const CField& operator[](CTempString col) const;

        /// Get number of columns in the current result set.
        int GetTotalColumns(void);
        /// Get name of the column with given number in the current result set.
        /// All columns are numbered starting with 1.
        string GetColumnName(unsigned int col);
        /// Get type of the column with given number in the current result set.
        /// All columns are numbered starting with 1.
        ESDB_Type GetColumnType(unsigned int col);

        /// Comparison of iterators.
        /// Only comparison with end() iterator makes sense. Otherwise all
        /// iterators created from the same CQuery object will be equal.
        bool operator==(const CRowIterator& ri) const;
        bool operator!=(const CRowIterator& ri) const;

        /// Advance iterator to the next row in the result set.
        CRowIterator& operator++(void);
        CRowIterator  operator++(int);
        
    private:
        friend class CQuery;

        /// Create iterator for the query.
        /// Two types of iterators are possible - pointer to end and pointer
        /// to anything else.
        CRowIterator(CQueryImpl* q, bool is_end);


        /// Query iterator was created for
        CRef<CQueryImpl> m_Query;
        /// Flag showing whether this is constant pointer to the end or
        /// pointer to some particular row.
        bool             m_IsEnd;
    };
    
    typedef CRowIterator iterator; 
    typedef CRowIterator const_iterator; 
    

    /// Assign string value to the parameter.
    /// If data type requested is not string then attempt to do conversion
    /// will be made. If conversion is impossible exception will be thrown.
    void SetParameter(CTempString   name,
                      const string& value,
                      ESDB_Type     type = eSDB_String,
                      ESP_ParamType param_type = eSP_In);
    /// Assign string value to the parameter.
    /// If data type requested is not string then attempt to do conversion
    /// will be made. If conversion is impossible exception will be thrown.
    void SetParameter(CTempString   name,
                      const char*   value,
                      ESDB_Type     type = eSDB_String,
                      ESP_ParamType param_type = eSP_In);
    /// Assign 8-byte integer value to the parameter.
    /// If data type requested is not 8-byte integer then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      Int8          value,
                      ESDB_Type     type = eSDB_Int8,
                      ESP_ParamType param_type = eSP_In);
    /// Assign 4-byte integer value to the parameter.
    /// If data type requested is not 4-byte integer then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      Int4          value,
                      ESDB_Type     type = eSDB_Int4,
                      ESP_ParamType param_type = eSP_In);
    /// Assign short integer value to the parameter.
    /// If data type requested is not short integer then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      short         value,
                      ESDB_Type     type = eSDB_Short,
                      ESP_ParamType param_type = eSP_In);
    /// Assign byte value to the parameter.
    /// If data type requested is not byte then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      unsigned char value,
                      ESDB_Type     type = eSDB_Byte,
                      ESP_ParamType param_type = eSP_In);
    /// Assign float value to the parameter.
    /// If data type requested is not float then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      float         value,
                      ESDB_Type     type = eSDB_Float,
                      ESP_ParamType param_type = eSP_In);
    /// Assign double value to the parameter.
    /// If data type requested is not double then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      double        value,
                      ESDB_Type     type = eSDB_Double,
                      ESP_ParamType param_type = eSP_In);
    /// Assign CTime value to the parameter.
    /// If data type requested is not datetime then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      const CTime&  value,
                      ESDB_Type     type = eSDB_DateTime,
                      ESP_ParamType param_type = eSP_In);
    /// Assign bool value to the parameter.
    /// If data type requested is not byte then attempt to do
    /// conversion will be made. If conversion is impossible exception
    /// will be thrown.
    void SetParameter(CTempString   name,
                      bool          value,
                      ESDB_Type     type = eSDB_Byte,
                      ESP_ParamType param_type = eSP_In);
    /// Assign null value to the parameter.
    /// Data type should be given explicitly to show which type of data should
    /// be sent to server.
    void SetNullParameter(CTempString   name,
                          ESDB_Type     type,
                          ESP_ParamType param_type = eSP_In);

    /// Get value of the parameter.
    /// For eSP_In parameters value set to it will always be returned. For
    /// eSP_InOut parameters value set to it will be returned before stored
    /// procedure execution and value returned from proecedure after its
    /// execution.
    const CField& GetParameter(CTempString name);

    /// Remove parameter with given name from parameter list.
    void ClearParameter(CTempString name);
    /// Remove all parameters from parameter list.
    void ClearParameters(void);
     
    /// Set current sql statement.
    /// Method does not clear parameter list and doesn't purge result sets
    /// left from previous query execution.
    void SetSql(CTempString sql);
    /// Execute sql statement.
    /// All result sets left from previous statement or stored procedure
    /// execution are purged.
    void Execute(void);
    /// Execute stored procedure with given name.
    /// All result sets left from previous statement or stored procedure
    /// execution are purged.
    void ExecuteSP(CTempString sp);

    /// Get number of rows read after statement execution.
    /// This number is available only when all result sets are read or purged
    /// by call to PurgeResults(). At all other times method returns -1.
    int GetRowCount(void);
    /// Get return status of stored procedure.
    /// Status is available only if stored procedure is executed via
    /// ExecuteSP() method and all result sets returned from the procedure are
    /// read or purged by call to PurgeResults().
    int GetStatus(void);

    /// Check if any more result sets are available for reading.
    /// Advances to the next result set purging all remaining rows in current
    /// one if reading of it was already started (begin() method was called).
    bool HasMoreResultSets(void);

    /// Purge all remaining result sets.
    void PurgeResults(void);

    /// Get total number of columns in the current result set
    int GetTotalColumns(void);
    /// Get name of the column with given number in the current result set.
    /// All columns are numbered starting with 1.
    string GetColumnName(unsigned int col);
    /// Get type of the column with given number in the current result set
    /// All columns are numbered starting with 1.
    ESDB_Type GetColumnType(unsigned int col);
    /// Get number of currently active result set.
    /// All result sets are numbered starting with 1.
    unsigned int GetResultSetNo(void);
    /// Get row number currently active.
    /// Row numbers are assigned consecutively to each row in all result
    /// sets returned starting with 1. Row number is not reset after
    /// passing result set boundary.
    unsigned int GetRowNo(void);

    /// Convert this query to work like only one result set was returned
    /// effectively merging all result sets together. If some result sets
    /// were already read then all the remaining result sets will be merged.
    /// Method impacts not only this CQuery object and all iterators created
    /// from it but all copies of this CQuery object too. Result sets only
    /// from recently executed statement are affected. After re-execution of a
    /// statement default behavior is used - to not merge different result
    /// sets.
    CQuery& SingleSet(void);
    /// Convert this query to not merge different result sets, i.e. iterator
    /// will be equal to end() at the end of each result set and to switch to
    /// the next one you'll have to call begin() again.
    /// Method impacts not only this CQuery object and all iterators created
    /// from it but all copies of this CQuery object too.
    CQuery& MultiSet(void);

    /// Start iterating through next result set.
    /// If some result set was already started to iterate through and end of it
    /// wasn't reached all remaining rows will be purged.
    CRowIterator begin(void);
    /// Get iterator pointing to the end of the current result set or to the
    /// end of all result sets (depending on the setting changed with
    /// SingleSet() and MultiSet()). Method cannot be used to take two
    /// different iterators - one for the end of result set and one for the
    /// end of all result sets. Which end is pointed to depends on the last
    /// call to SingleSet() or MultiSet() even if it was made after call to
    /// this method.
    CRowIterator end(void);

private:
    friend class CDatabase;

    /// Create query object for given database object
    CQuery(CDatabaseImpl* db_impl);


    /// Query implementation object
    CRef<CQueryImpl> m_Impl;
};


/// Object used to perform bulk-inserting operations to database.
class CBulkInsert
{
public:
    /// Empty constructor of bulk-insert object.
    /// Object created this way cannot be used for anything except assigning
    /// from other bulk-insert object.
    CBulkInsert(void);
    ~CBulkInsert(void);

    /// Copying of bulk-insert object.
    /// Any copy will work with the same bulk-insert session on the server.
    /// So that Complete() call on any of the copies will end up this session
    /// and no insertion will be possible to make from other copies.
    CBulkInsert(const CBulkInsert& bi);
    CBulkInsert& operator= (const CBulkInsert& bi);

    /// Type of hint that can be set for bulk insert
    enum EHints {
        eTabLock,
        eCheckConstraints,
        eFireTriggers
    };
    /// Add hint to the bulk insert.
    /// Resets everything that was set by SetHints().
    void AddHint(EHints hint);

    /// Type of hint that requires some value to be provided with it
    enum EHintsWithValue {
        eRowsPerBatch,
        eKilobytesPerBatch
    };
    /// Add hint with value to the bulk insert.
    /// Value should not be equal to zero.
    /// Resets everything that was set by SetHints().
    void AddHint(EHintsWithValue hint, unsigned int value);
    /// Add "ORDER" hint.
    /// Resets everything that was set by SetHints().
    void AddOrderHint(CTempString columns);
    /// Set hints by one call. Resets everything that was set by Add*Hint().
    void SetHints(CTempString hints);

    /// Bind column for bulk insert.
    /// All columns are numbered starting with 1. Columns should be bound in
    /// strict order (1st column first, then 2nd, then 3rd etc) and no columns
    /// can be bound several times. Type given is data type which all values
    /// will be converted to and how data will be sent to the server.
    void Bind(int col, ESDB_Type type);

    /// Put values of different type into bulk-insert row.
    /// Automatic conversion is made to the bound type if possible. If
    /// conversion is not possible then exception is thrown. Number of values
    /// put into the row should be exactly the same as number of columns
    /// bound with calls to Bind().
    CBulkInsert& operator<<(const string& val);
    CBulkInsert& operator<<(const char* val);
    CBulkInsert& operator<<(unsigned char val);
    CBulkInsert& operator<<(short val);
    CBulkInsert& operator<<(Int4 val);
    CBulkInsert& operator<<(Int8 val);
    CBulkInsert& operator<<(float val);
    CBulkInsert& operator<<(double val);
    CBulkInsert& operator<<(bool val);
    CBulkInsert& operator<<(const CTime& val);
    /// Special case of putting NullValue or EndRow into the row.
    CBulkInsert& operator<<(CBulkInsert& (*f)(CBulkInsert&));

    /// Complete bulk insert.
    /// Nothing can be inserted after call to this method.
    void Complete();           
    
private:
    friend class CDatabase;
    friend CBulkInsert& NullValue(CBulkInsert& bi);
    friend CBulkInsert& EndRow(CBulkInsert& bi);

    /// Create bulk-insert object for given database, table name and number of
    /// rows to write before auto-flush occurs.
    CBulkInsert(CDatabaseImpl* db_impl,
                const string&  table_name,
                int            autoflush);


    /// Bulk-insert implementation object
    CRef<CBulkInsertImpl> m_Impl;
};

/// Manipulator putting null value into the bulk-insert row.
CBulkInsert& NullValue(CBulkInsert& bi);
/// Manipulator ending row in the bulk-insert object
CBulkInsert& EndRow(CBulkInsert& bi);
  

/// Database connection object.
class CDatabase
{
public:
    /// Empty constructor of database object.
    /// Object created this way cannot be used for anything except assigning
    /// from another database object.
    CDatabase(void);
    /// Create database object for particular database name
    CDatabase(const string& db_name);
    ~CDatabase(void);

    /// Copying of database object.
    /// Objects copied this way will work over the same physical connection to
    /// the server unless they're reconnected again.
    CDatabase(const CDatabase& db);
    CDatabase& operator= (const CDatabase& db);

    /// Set database property
    void SetProperty(const string& prop_name, const string& prop_value);
    /// Get value of database property
    string GetProperty(const string& prop_name) const;

    /// Connect to the database server with given credentials.
    void Connect(const string& user,
                 const string& password,
                 const string& server);
    /// Close database object.
    /// You cannot do anything with CQuery and CBulkInsert objects created
    /// from this database object after call to this method. Although you can
    /// reconnect to the database server CQuery and CBulkInsert objects will
    /// not work anyway.
    void Close(void);
    /// Clone database object.
    /// While cloning new physical connection to database server is created,
    /// so that resulting database object can be used completely independently
    /// from original one.
    CDatabase Clone(void);           

    /// Get new CQuery object for this database.
    /// Method can be called only when database object is connected to the
    /// database server.
    CQuery NewQuery(void);
    /// Get new CQuery object with particular sql statement for this database.
    /// Method can be called only when database object is connected to the
    /// database server.
    CQuery NewQuery(const string& sql);  
    /// Get new CBulkInsert object.
    ///
    /// @param table_name
    ///   Name of the table to insert to
    /// @param autoflush
    ///   Number of rows to insert before the batch is committed to database
    CBulkInsert NewBulkInsert(const string& table_name, int autoflush);           

private:
    /// Name of database
    string               m_DBName;
    /// User name used during last connect to the server
    string               m_User;
    /// Password used during last connect to the server
    string               m_Password;
    /// Last server object was connected to
    string               m_Server;
    /// Database properties
    map<string, string>  m_Props;
    /// Database implementation object
    CRef<CDatabaseImpl>  m_Impl;
};



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline void
CDatabase::SetProperty(const string& prop_name, const string& prop_value)
{
    m_Props[prop_name] = prop_value;
}

inline string
CDatabase::GetProperty(const string& prop_name) const
{
    map<string, string>::const_iterator it = m_Props.find(prop_name);
    if (it == m_Props.end())
        return string();
    else
        return it->second;
}

inline CQuery
CDatabase::NewQuery(const string& sql)
{
    CQuery q = NewQuery();
    q.SetSql(sql);
    return q;
}


inline bool
CQuery::CRowIterator::operator!= (const CRowIterator& ri) const
{
    return !operator== (ri);
}

inline CQuery::CRowIterator
CQuery::CRowIterator::operator++ (int)
{
    return operator++();
}

END_NCBI_SCOPE

#endif  // CPPCORE__DBAPI__SIMPLE__SDBAPI__HPP