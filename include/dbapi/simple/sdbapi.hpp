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
#include <corelib/rwstream.hpp>
#include <util/ncbi_url.hpp>
#include <dbapi/dbapi.hpp>
#include <dbapi/error_codes.hpp>


BEGIN_NCBI_SCOPE


class CSDBAPI;
class CDatabase;
class CQuery;
class CBlobBookmark;
class CDatabaseImpl;
class CBulkInsertImpl;
class CQueryImpl;
class CBlobBookmarkImpl;
struct SQueryParamInfo;


/// Exception class used throughout the API
class CSDB_Exception : public CException
{
public:
    enum EErrCode {
        eURLFormat,     ///< Incorrectly formated URL is used to create
                        ///< CSDB_ConnectionParam
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
        eLowLevel,      ///< Exception from low level DBAPI was re-thrown with
                        ///< this exception class
        eWrongParams    ///< Wrong parameters provided to the method
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    /// Returns any underlying DBAPI exception, or else NULL.
    const CDB_Exception* GetDBException(void) const;

    /// Returns any underlying DBAPI error code, or else CException::eInvalid.
    CDB_Exception::TErrCode GetDBErrCode(void) const;

    const string& GetServerName(void) const;
    const string& GetUserName(void) const;
    const string& GetDatabaseName(void) const;

    /// Returns any additional context (typically, the relevant SQL
    /// statement or database operation).
    const string& GetExtraMsg(void) const;

    void ReportExtra(ostream& os) const;

    // Standard exception boilerplate code.
    CSDB_Exception(const CDiagCompileInfo& info,
                   const CException* prev_exception,
                   EErrCode err_code,
                   const CDB_Exception::SMessageInContext& message,
                   EDiagSev severity = eDiag_Error)
        : CException(info, prev_exception, (CException::EErrCode)err_code,
                     message.message, severity)
        , m_Context(message.context)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CSDB_Exception, CException);

protected:
    CSDB_Exception(const CDiagCompileInfo& info,
                   const CException* prev_exception,
                   const CDB_Exception::SMessageInContext& message,
                   EDiagSev severity = eDiag_Error,
                   CException::TFlags flags = 0);

    void x_Init(const CDiagCompileInfo& info, const string& message,
                const CException* prev_exception, EDiagSev severity);

private:
    CConstRef<CDB_Exception::SContext> m_Context;
};


class CSDB_DeadlockException : public CSDB_Exception
{
public:
    void x_Init(const CDiagCompileInfo& info, const string& message,
                const CException* prev_exception, EDiagSev severity);

    void x_InitErrCode(CException::EErrCode err_code);

    NCBI_EXCEPTION_DEFAULT(CSDB_DeadlockException, CSDB_Exception);
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
    eSDB_StringUCS2,
    eSDB_Binary,
    eSDB_DateTime,
    eSDB_Text,
    eSDB_TextUCS2,
    eSDB_Image
};

/// Stored procedure and statement parameter types
enum ESP_ParamType {
    eSP_In,     ///< Parameter is only passed to server, no return is expected
    eSP_InOut   ///< Parameter can be returned from stored procedure
};

/// (S)DBAPI_TRANSACTION glue for CDatabase.
CAutoTrans::CSubject DBAPI_MakeTrans(CDatabase& db);
/// (S)DBAPI_TRANSACTION glue for CQuery.
CAutoTrans::CSubject DBAPI_MakeTrans(CQuery& query);

/// Establish an automatically managed anonymous SQL transaction on
/// the specified object's connection for the duration of the
/// immediately following code block: SDBAPI_TRANSACTION(obj) { ... }
///
/// Automatically COMMIT upon reaching the end of the block normally.
/// Automatically ROLLBACK upon leaving the block early, via a break
/// or return statement or uncaught exception.  A client crash or
/// severed connection will trigger an implicit server-side ROLLBACK.
///
/// Nested transactions are possible, and will use savepoints for
/// inner transactions so they can fail cleanly (without also
/// canceling outer transactions).
#define SDBAPI_TRANSACTION(obj) DBAPI_TRANSACTION(obj)

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

    ///  Allow transaction log (general, to avoid using bools).
    enum EAllowLog {
        eDisableLog,     ///< Disables log.
        eEnableLog       ///< Enables log.
    };

    /// Class representing value in result set or output parameter of stored
    /// procedure.
    class CField
    {
    public:
        /// Get value as UTF-8 string.
        /// Any underlying database type will be converted to a string
        /// using the variable-width UTF-8 encoding.
        /// @sa CUtf8::AsBasicString, CUtf8::AsSingleByteString
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

        /// Check if value is NULL.
        bool IsNull(void) const;

        /// Get a blob output stream, on top of a cloned connection.
        /// The original connection should NOT have an active transaction.
        /// 
        /// NOTE: You won't be able to write a blob using this method if its
        /// value is set to NULL in the database. To use this method you should
        /// pre-set blob value to empty string or anything else of your choice.
        ///
        /// @param blob_size
        ///   blob_size is the size of the BLOB to be written.
        /// @param flags
        ///   @see EBlobOStreamFlags.
        /// @param log_it
        ///    Enables transaction log for BLOB (enabled by default).
        ///    Make sure you have enough log segment space, or disable it.
        CNcbiOstream& GetOStream(size_t blob_size, TBlobOStreamFlags flags = 0)
            const;
        CNcbiOstream& GetOStream(size_t blob_size, EAllowLog log_it) const;

        /// Get bookmark for the blob. This bookmark can be used to change
        /// blob data later when all results from this query are processed.
        /// 
        /// NOTE: You won't be able to write a blob using this method if its
        /// value is set to NULL in the database. To use this method you should
        /// pre-set blob value to empty string or anything else of your choice.
        /// Blob value should be set before the query execution, i.e. query
        /// should return non-NULL value. Setting non-NULL value after query
        /// execution won't work.
        ///
        /// @sa CDatabase::NewBookmark
        CBlobBookmark GetBookmark(void) const;

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

        const CDB_Exception::SContext& x_GetContext(void) const;

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
        /// String to represent Text/Image value
        mutable string                   m_ValueForStream;
        /// Stream to represent Text/Image value
        mutable auto_ptr<CNcbiIstrstream> m_IStream;
        /// Stream to change Text/Image value
        mutable auto_ptr<CWStream>       m_OStream;
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
        unsigned int GetRowNo(void) const;
        /// Get number of currently active result set.
        /// All result sets are numbered starting with 1.
        unsigned int GetResultSetNo(void) const;

        /// Get column value by its number.
        /// All columns are numbered starting with 1.
        const CField& operator[](int col) const;
        /// Get column value by its name.
        const CField& operator[](CTempString col) const;

        /// Get number of columns in the current result set.
        int GetTotalColumns(void) const;
        /// Get name of the column with given number in the current result set.
        /// All columns are numbered starting with 1.
        string GetColumnName(unsigned int col) const;
        /// Get type of the column with given number in the current result set.
        /// All columns are numbered starting with 1.
        ESDB_Type GetColumnType(unsigned int col) const;

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

        const CDB_Exception::SContext& x_GetContext(void) const;

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
    /// Declare an output-only parameter.
    /// Equivalent for now to calling SetNullParameter with a
    /// param_type value of eSP_InOut because MSSQL and Sybase (and
    /// hence FreeTDS) don't support true output-only parameters.
    /// However, if SDBAPI ever gains support for database engines
    /// with this feature, this method will arrange to make use of it
    /// as appropriate.
    void SetOutputParameter(CTempString name, ESDB_Type type);

    /// Get value of the parameter.
    /// For eSP_In parameter value set to it will always be returned. For
    /// eSP_InOut parameter value set to it will be returned before stored
    /// procedure execution and value returned from procedure after its
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
    /// Explicitly execute sql statement.
    /// All result sets left from previous statement or stored procedure
    /// execution are purged.  The query reverts to SingleSet mode, with
    /// no row count requirements.
    void Execute(void);
    /// Execute stored procedure with given name.
    /// All result sets left from previous statement or stored procedure
    /// execution are purged.  The query reverts to SingleSet mode, with
    /// no row count requirements.
    void ExecuteSP(CTempString sp);

    /// Get number of rows read after statement execution.
    /// This number is available only when all result sets are read or purged
    /// by call to PurgeResults().  At all other times, the method throws an
    /// exception.
    int GetRowCount(void) const;
    /// Get return status of stored procedure.
    /// Status is available only if stored procedure is executed via
    /// ExecuteSP() method and all result sets returned from the procedure are
    /// read or purged by call to PurgeResults().
    int GetStatus(void) const;

    /// Check if any more result sets are available for reading.
    /// Advances to the next result set purging all remaining rows in current
    /// one if reading of it was already started (begin() method was called).
    bool HasMoreResultSets(void);

    /// Purge all remaining result sets.
    void PurgeResults(void);

    /// Whether to consider just the current result set or all result
    /// sets, in MultiSet mode.  (In SingleSet mode, always consider all.)
    enum EHowMuch {
        eThisResultSet,
        eAllResultSets
    };

    /// Indicate precisely how many rows the active query should return.
    /// In MultiSet mode, the requirement applies to individual result
    /// sets.  (Callers may specify the requirement for each set as it
    /// comes up, or let it carry over unchanged.)  Any call to this
    /// method must follow Execute or ExecuteSP, which reset any such
    /// requirements.
    void RequireRowCount(unsigned int n);
    /// Indicate the minimum and maximum number of rows the active query
    /// should return.  In MultiSet mode, the requirement applies to
    /// individual result sets.  (Callers may specify the requirement
    /// for each set as it comes up, or let it carry over unchanged.)
    /// Any call to this method must follow Execute or ExecuteSP, which
    /// reset any such requirements.
    /// @param min_rows
    ///  Minimum valid row count.
    /// @param max_rows
    ///  Maximum valid row count.  (kMax_Auto for no limit.)
    void RequireRowCount(unsigned int min_rows, unsigned int max_rows);
    /// Ensure that no unread rows remain, and that the total number of
    /// rows satisfies any constraints specified by RequireRowCount.
    /// Throw an exception (after purging any unread rows) if not.
    void VerifyDone(EHowMuch how_much = eThisResultSet);

    /// Get total number of columns in the current result set
    int GetTotalColumns(void) const;
    /// Get name of the column with given number in the current result set.
    /// All columns are numbered starting with 1.
    string GetColumnName(unsigned int col) const;
    /// Get type of the column with given number in the current result set
    /// All columns are numbered starting with 1.
    ESDB_Type GetColumnType(unsigned int col) const;
    /// Get number of currently active result set.
    /// All result sets are numbered starting with 1.
    unsigned int GetResultSetNo(void) const;
    /// Get row number currently active.
    /// Row numbers are assigned consecutively to each row in all result
    /// sets returned starting with 1.  With eAllResultSets (default) or
    /// in MultiSet mode, row number is not reset after passing result set
    /// boundary.
    unsigned int GetRowNo(EHowMuch how_much = eAllResultSets) const;

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
    /// If a query was supplied but not explicitly executed, automatically
    /// execute it before proceeding.  If iteration was already in
    /// progress, purge the remainder of the current result set and advance
    /// to the next, if there is one.
    CRowIterator begin(void) const;
    /// Get iterator pointing to the end of the current result set or to the
    /// end of all result sets (depending on the setting changed with
    /// SingleSet() and MultiSet()). Method cannot be used to take two
    /// different iterators - one for the end of result set and one for the
    /// end of all result sets. Which end is pointed to depends on the last
    /// call to SingleSet() or MultiSet() even if it was made after call to
    /// this method.
    CRowIterator end(void) const;

private:
    friend class CDatabase;

    friend CAutoTrans::CSubject DBAPI_MakeTrans(CQuery& query);

    /// Create query object for given database object
    CQuery(CDatabaseImpl* db_impl);


    /// Query implementation object
    CRef<CQueryImpl> m_Impl;
};


/// Object used to store bookmarks to blobs to be changed later.
///
/// @sa CQuery::CField::GetBookmark, CDatabase::NewBookmark
class CBlobBookmark
{
public:
    /// Blob type (if known). 
    enum EBlobType {
        eUnknown,
        eText,
        eBinary
    };

    /// Get Blob output stream. Blob will be updated using the same
    /// database connection which this bookmark was create on.
    /// Thus it shouldn't have any active queries or bulk inserts by the time
    /// this method is called and until the stream is not used anymore.
    ///
    /// @param blob_size
    ///   blob_size is the size of the BLOB to be written.
    /// @param flags
    ///   @see EBlobOStreamFlags.
    /// @param log_it
    ///    Enables transaction log for BLOB (enabled by default).
    ///    Make sure you have enough log segment space, or disable it.
    CNcbiOstream& GetOStream(size_t blob_size, TBlobOStreamFlags flags = 0)
        const;
    CNcbiOstream& GetOStream(size_t blob_size, CQuery::EAllowLog log_it) const;

    /// Empty constructor of bookmark object.
    /// Object created this way cannot be used for anything except assigning
    /// from the other bookmark object.
    CBlobBookmark(void);
    ~CBlobBookmark(void);

    /// Copy of bookmark object
    CBlobBookmark(const CBlobBookmark& bm);
    CBlobBookmark& operator= (const CBlobBookmark& bm);

private:
    friend class CDatabase;
    friend class CQuery::CField;

    /// Create bookmark with the given implementation
    CBlobBookmark(CBlobBookmarkImpl* bm_impl);


    /// Bookmark implementation object
    CRef<CBlobBookmarkImpl> m_Impl;
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
    /// Special case to support inserting into NVARCHAR and NTEXT fields
    CBulkInsert& operator<<(const TStringUCS2& val);
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
  

class CDBConnParamsBase;


/// Database password decryptor.
///
/// The general structure is public, but the full default
/// implementation is only available within NCBI.
class CSDB_Decryptor : public CObject
{
public:
    string Decrypt(const string& ciphertext, const CTempString& key_id);

protected:
    virtual string x_Decrypt(const string& ciphertext, const string& key)
#ifndef HAVE_LIBCONNEXT
        = 0
#endif
        ;

    virtual string x_GetKey(const CTempString& key_id);
};


/// Convenience class to initialize database connection parameters from
/// URL-like strings and/or application configuration and/or hard-code.
class CSDB_ConnectionParam
{
public:
    /// Get database connection parameters from a string and from
    /// the application configuration.
    ///
    /// @param url_string
    ///   Get connection parameters from this URL-like string, formatted as:
    ///   dbapi://[username[:password]@][server[:port]][/database][?k1=v1;...]
    ///   or
    ///   dbapi://[username[:password]@][service][/database][?k1=v1;...]
    ///   or
    ///   service
    ///
    ///   Each token must be URL-encoded.
    ///
    ///   Also application configuration is looked for section with name
    ///   "service.dbservice" ("service" can be passed to the ctor or to Set()
    ///   method). If such section exists then the following parameters are
    ///   read from it, and if present will override corresponding parameters
    ///   extracted from the URL or specified later via Set.
    ///   - username
    ///   - password
    ///   - service
    ///   - port
    ///   - database
    ///   - args
    ///   - login_timeout
    ///   - io_timeout
    ///   - exclusive_server
    ///   - use_conn_pool
    ///   - conn_pool_minsize
    ///   - conn_pool_maxsize
    ///   - password_file
    ///   - password_key
    ///
    ///   The last 8 parameters can also come as named URL parameters;
    ///   "args" is a catch-all for any other parameters, which can appear
    ///   directly as URL parameters.  (Settings from the configuration file's
    ///   "args" string override URL parameters on an individual basis.)
    CSDB_ConnectionParam(const string& url_string = kEmptyStr);

    /// Flags affecting URL composition.
    ///
    /// @sa ComposeUrl
    enum EComposeUrlFlags {
        /// Throw an exception if missing any "essential" parameters.
        /// (It's normally necessary to have the server name, the database
        /// name, the username, and either a password or a password file.)
        fThrowIfIncomplete = 0x1,
        fHidePassword      = 0x2, /// Obscure passwords
        // For compatibility with older code
        eThrowIfIncomplete = fThrowIfIncomplete,
        eAllowIncomplete   = 0
    };
    typedef int TComposeUrlFlags; // Bitwise OR of EComposeUrlFlags

    /// Compose database connection URL string out of this class.
    ///
    /// @param flags
    ///   Any desired flags from EComposeUrlFlags.
    string ComposeUrl(TComposeUrlFlags flags = 0) const;

    bool IsEmpty(void) const;

    /// "Essential" (e.g. those located before '?' in the URL) database
    /// connection parameters
    enum EParam {
        eUsername,
        ePassword,
        ePasswordFile,
        ePasswordKeyID,
        eService,   ///< Database server (or alias, including LBSM service)
        ePort,      ///< Database server's port
        eDatabase,
        eLoginTimeout,
        eIOTimeout,
        eExclusiveServer,
        eUseConnPool,
        eConnPoolMinSize,
        eConnPoolMaxSize,
        eArgsString
    };
 
    /// Whether to report values from configuration files, or just
    /// those set in code (which have a lower priority).
    enum EWithOverrides {
        eWithoutOverrides,
        eWithOverrides
    };

    /// Get one of the "essential" database connection parameters.
    /// Empty string means that it is not set.
    string Get(EParam param,
               EWithOverrides with_overrides = eWithoutOverrides) const;

    /// Flags affecting parameter setting.
    ///
    /// @sa Set
    enum ESetFlags {
        /// The specified value is merely a default, which Set should ignore
        /// if a non-empty setting for that parameter is already present.
        fAsDefault = 0x1
    };
    typedef int TSetFlags; // Bitwise OR of ESetFlags

    /// Set one of the "essential" database connection parameters,
    /// unless overridden in a configuration file.
    ///
    /// Settings from [*.dbservice] sections always take precedence
    /// because they're more visible and easier to adjust.
    ///
    /// @param param
    ///   Parameter to set
    /// @param value
    ///   The value to set the parameter to. Empty string un-sets it.
    /// @param flags
    ///   Any desired flags from ESetFlags.
    CSDB_ConnectionParam& Set(EParam param, const string& value,
                              TSetFlags flags = 0);
    /// Merge existing settings with those from url_string.  The flags
    /// indicate how to resolve conflicts.
    CSDB_ConnectionParam& Set(const string& url_string, TSetFlags flags = 0);
    /// Merge existing settings with those from param.  The flags
    /// indicate how to resolve conflicts.
    CSDB_ConnectionParam& Set(const CSDB_ConnectionParam& param,
                              TSetFlags flags = 0);    

    /// Access to additional (e.g. those located after '?' in the URL)
    /// database connection parameters
    const CUrlArgs& GetArgs(void) const;
    /// Access to additional (e.g. those located after '?' in the URL)
    /// database connection parameters
    CUrlArgs& GetArgs(void);

    /// Use the specified password decryptor.
    ///
    /// @sa GetGlobalDecryptor
    static void SetGlobalDecryptor(CRef<CSDB_Decryptor> decryptor);
    /// Get the current password decryptor, if any.
    ///
    /// @sa SetGlobalDecryptor
    static CRef<CSDB_Decryptor> GetGlobalDecryptor(void);

    /// Copy ctor (explicit, to avoid accidental copying)
    explicit CSDB_ConnectionParam(const CSDB_ConnectionParam& param);
    /// Assignment
    CSDB_ConnectionParam& operator= (const CSDB_ConnectionParam& param);

private:
    friend class CSDBAPI;
    friend class CDatabaseImpl;

    /// Populate m_ParamMap according to the current server or service name.
    void x_FillParamMap(void);

    /// Fill parameters for low-level DBAPI from what is set here and in the
    /// configuration file.
    void x_FillLowerParams(CDBConnParamsBase* params) const;

    /// Determine what password to use, accounting for possible
    /// encryption or indirection.
    string x_GetPassword() const;

    static const char* x_GetName(EParam param);

    static bool x_IsKnownArg(const string& name);

    void x_ReportOverride(const CTempString& name, CTempString code_value,
                          CTempString reg_value) const;

    /// URL storing all parameters set in code.
    mutable CUrl m_Url;

    /// Map of any parameters set in the configuration file, which override
    /// those set in code.
    typedef map<EParam, string> TParamMap;
    TParamMap m_ParamMap;
};


/// Database connection object.
class CDatabase
{
public:
    /// How thoroughly IsConnected should actually check the connection.
    enum EConnectionCheckMethod {
        eNoCheck,   //< Confirm that Connect() was called and Close() wasn't.
        eFastCheck, //< Also verify that the driver still sees a connection.
        eFullCheck, //< Further confirm that a simple query succeeds.
    };

    /// Empty constructor of database object.
    /// Object created this way cannot be used for anything except assigning
    /// from another database object.
    CDatabase(void);
    /// Create database object for particular database parameters
    CDatabase(const CSDB_ConnectionParam& param);
    /// Create database object and take database parameters from given URL and
    /// application configuration. See CSDB_ConnectionParam ctor
    /// for URL format and rules of overriding.
    CDatabase(const string& url_string);
    ~CDatabase(void);

    /// Copying of database object.
    /// Objects copied this way will work over the same physical connection to
    /// the server unless they're reconnected again.
    CDatabase(const CDatabase& db);
    CDatabase& operator= (const CDatabase& db);

    /// Get connection parameters
    CSDB_ConnectionParam& GetConnectionParam(void);
    const CSDB_ConnectionParam& GetConnectionParam(void) const;

    /// Connect to the database server
    void Connect(void);
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
    /// Check if database object was already connected to database server.
    /// By default, this checks only that Connect() method was called
    /// and Close() wasn't called, but more thorough checks are possible.
    /// NB: eFullCheck mode cannot coexist with active queries.
    bool IsConnected(EConnectionCheckMethod check_method = eNoCheck);

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
    /// Get new CBlobBookmark object.
    ///
    /// @param table_name
    ///   Name of the table to update.
    /// @param column_name
    ///   Name of the column to update.
    /// @param search_conditions
    ///   SQL expression identifying the relevant row.  (Failure to
    ///   match exactly one row yields undefined behavior, and may
    ///   result in updating multiple rows in some cases.)
    /// @param column_type
    ///   General column type (text or binary), if known.  (Optional.)
    /// @sa CQuery::CField::GetBookmark
    CBlobBookmark NewBookmark(const string&            table_name,
                              const string&            column_name,
                              const string&            search_conditions,
                              CBlobBookmark::EBlobType column_type
                                  = CBlobBookmark::eUnknown);


private:
    friend CAutoTrans::CSubject DBAPI_MakeTrans(CDatabase& db);

    /// Database parameters
    CSDB_ConnectionParam m_Params;
    /// Database implementation object
    CRef<CDatabaseImpl>  m_Impl;
};


class CSDBAPI
{
public:
    /// Initialize SDBAPI.
    /// Creates minimum number of connections required for each pool configured
    /// in application's configuration file. If openning of some of those
    /// connections failed then method will return FALSE, otherwise TRUE.
    static bool Init(void);

    /// Report the specified application name to servers.
    ///
    /// By default, this will be the same name reported to AppLog
    /// (typically autodetected).  Any changes will take effect for
    /// subsequent connections, but won't affect preexisting ones.
    ///
    /// @sa GetApplicationName()
    static void SetApplicationName(const CTempString& name);

    /// Check SDBAPI's application name setting.
    ///
    /// @sa SetApplicationName()
    static string GetApplicationName(void);

    /// @sa UpdateMirror
    enum EMirrorStatus {
        eMirror_Steady,      ///< Mirror is working on the same server as before
        eMirror_NewMaster,   ///< Switched to a new master
        eMirror_Unavailable  ///< All databases in the mirror are unavailable
    };

    /// Check for master/mirror switch. If switch is detected or if all databases
    /// in the mirror become unavailable, then all connections
    /// to the "old" master server will be immediately invalidated, so that any
    /// subsequent database operation on them (via objects CQuery and
    /// CBulkInsert) would cause an error. The affected CDatabase objects will
    /// be automatically invalidated too. User code will have to explicitly re-connect
    /// (which will open connection to the new master, if any).
    /// @note
    ///   If the database resource is in any way misconfigured, then an exception
    ///   will be thrown.
    /// @param dbservice
    ///   Database resource name 
    /// @param servers
    ///   List of database servers, with the master one first.
    /// @param error_message
    ///   Detailed error message (if any).
    /// @return
    ///   Result code
    static EMirrorStatus UpdateMirror(const string& dbservice,
                                      list<string>* servers = NULL,
                                      string*       error_message = NULL);

private:
    CSDBAPI(void);
};



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline
CSDB_Exception::CSDB_Exception(const CDiagCompileInfo& info,
                               const CException* prev_exception,
                               const CDB_Exception::SMessageInContext& message,
                               EDiagSev severity, CException::TFlags flags)
    : CException(info, prev_exception, message.message, severity, flags),
      m_Context(message.context)
{
    x_Init(info, message, prev_exception, severity);
}

inline
const CDB_Exception* CSDB_Exception::GetDBException(void) const
{
    return dynamic_cast<const CDB_Exception*>(GetPredecessor());
}

inline
CException::TErrCode CSDB_Exception::GetDBErrCode(void) const
{
    const CDB_Exception* dbex = GetDBException();
    return dbex ? dbex->GetErrCode() : eInvalid;
}

inline
const string& CSDB_Exception::GetServerName(void) const
{
    return m_Context->server_name;
}

inline
const string& CSDB_Exception::GetUserName(void) const
{
    return m_Context->username;
}

inline
const string& CSDB_Exception::GetDatabaseName(void) const
{
    return m_Context->database_name;
}

inline
const string& CSDB_Exception::GetExtraMsg(void) const
{
    return m_Context->extra_msg;
}

inline
void CSDB_DeadlockException::x_Init(const CDiagCompileInfo&, const string&,
                                    const CException* prev_exception, EDiagSev)
{
    _ASSERT(dynamic_cast<const CDB_DeadlockEx*>(prev_exception));
}

inline
void CSDB_DeadlockException::x_InitErrCode(CException::EErrCode err_code)
{
    _ASSERT((TErrCode)err_code == (TErrCode)CSDB_Exception::eLowLevel);
}


inline
void CQuery::SetOutputParameter(CTempString name, ESDB_Type type)
{
    SetNullParameter(name, type, eSP_InOut);
}


inline
CSDB_ConnectionParam::CSDB_ConnectionParam(const string& url_string /* = kEmptyStr */)
{
    if (url_string.empty()) {
        m_Url.SetScheme("dbapi");
        m_Url.SetIsGeneric(true);
        m_Url.GetArgs();
        return;
    }

    if (NStr::StartsWith(url_string, "dbapi://"))
        m_Url.SetUrl(url_string);
    else
        m_Url.SetUrl("dbapi://" + url_string);
    // Force arguments to exist
    m_Url.GetArgs();
    x_FillParamMap();
}

inline
CSDB_ConnectionParam::CSDB_ConnectionParam(const CSDB_ConnectionParam& param)
    : m_Url(param.m_Url), m_ParamMap(param.m_ParamMap)
{}

inline CSDB_ConnectionParam&
CSDB_ConnectionParam::operator= (const CSDB_ConnectionParam& param)
{
    m_Url = param.m_Url;
    m_ParamMap = param.m_ParamMap;
    return *this;
}

inline CUrlArgs&
CSDB_ConnectionParam::GetArgs(void)
{
    return m_Url.GetArgs();
}

inline const CUrlArgs&
CSDB_ConnectionParam::GetArgs(void) const
{
    return m_Url.GetArgs();
}

inline const char*
CSDB_ConnectionParam::x_GetName(EParam param)
{
    switch (param) {
    case eUsername:         return "username";
    case ePassword:         return "password";
    case ePasswordFile:     return "password_file";
    case ePasswordKeyID:    return "password_key";
    case eService:          return "service";
    case ePort:             return "port";
    case eDatabase:         return "database";
    case eLoginTimeout:     return "login_timeout";
    case eIOTimeout:        return "io_timeout";
    case eExclusiveServer:  return "exclusive_server";
    case eUseConnPool:      return "use_conn_pool";
    case eConnPoolMinSize:  return "conn_pool_minsize"; 
    case eConnPoolMaxSize:  return "conn_pool_maxsize";
    case eArgsString:       return "args_string";
    }
    _TROUBLE;
    return kEmptyCStr;
}

inline bool
CSDB_ConnectionParam::x_IsKnownArg(const string& name)
{
    for (int p = eLoginTimeout;  p < eArgsString;  ++p) {
        if (name == x_GetName(static_cast<EParam>(p))) {
            return true;
        }
    }
    return false;
}

inline string
CSDB_ConnectionParam::Get(EParam param, EWithOverrides with_overrides) const
{
    if (with_overrides == eWithOverrides) {
        TParamMap::const_iterator it = m_ParamMap.find(param);
        if (it != m_ParamMap.end()) {
            return it->second;
        }
    }

    switch (param) {
    case eUsername:
        return m_Url.GetUser();
    case ePassword:
        return m_Url.GetPassword();
    case eService:
        return m_Url.GetHost();
    case ePort:
        return m_Url.GetPort();
    case eDatabase:
        return m_Url.GetPath().empty()? kEmptyStr: m_Url.GetPath().substr(1);
    case ePasswordFile:
    {
        bool   found  = false;
        string pwfile = m_Url.GetArgs().GetValue("password_file", &found);
        if (found  &&  !pwfile.empty()  &&  !m_Url.GetPassword().empty()) {
            NCBI_THROW(CSDB_Exception, eURLFormat,
                       "Password and password_file URL parameters"
                       " are mutually exclusive.");
            return kEmptyStr;
        }
        return pwfile;
    }
    case ePasswordKeyID:
    case eLoginTimeout:
    case eIOTimeout:
    case eExclusiveServer:
    case eUseConnPool:
    case eConnPoolMinSize:
    case eConnPoolMaxSize:
    {
        bool found_dummy = false;
        return m_Url.GetArgs().GetValue(x_GetName(param), &found_dummy);
    }
    case eArgsString:
        return m_Url.GetOriginalArgsString();
    }
    _ASSERT(false);
    return string();
}

inline CSDB_ConnectionParam&
CSDB_ConnectionParam::Set(EParam param, const string& value, TSetFlags flags)
{
    TParamMap::const_iterator it = m_ParamMap.find(param);
    if (it != m_ParamMap.end()) {
        x_ReportOverride(x_GetName(it->first), value, it->second);
    }

    if ((flags & fAsDefault) != 0
        &&  ( !Get(param).empty()
              ||  (param == ePasswordFile  &&  !Get(ePassword).empty())
              ||  (param == ePassword      &&  !Get(ePasswordFile).empty()) ))
    {
        return *this;
    }

    switch (param) {
    case eUsername:
        m_Url.SetUser(value);
        return *this;
    case ePassword:
        if ( !value.empty()  &&  !Get(ePasswordFile).empty() ) {
            // XXX - issue diagnostics?
            Set(ePasswordFile, kEmptyStr);
        }
        m_Url.SetPassword(value);
        return *this;
    case eService:
        m_Url.SetHost(value);
        x_FillParamMap();
        return *this;
    case ePort:
        m_Url.SetPort(value);
        return *this;
    case eDatabase:
        m_Url.SetPath("/" + value);
        return *this;
    case ePasswordFile:
        if ( !value.empty()  &&  !Get(ePassword).empty() ) {
            // XXX - issue diagnostics?
            Set(ePassword, kEmptyStr);
        }
        if ( !value.empty()  ||  m_Url.GetArgs().IsSetValue("password_file")) {
            m_Url.GetArgs().SetValue("password_file", value);
        }
        return *this;
    case ePasswordKeyID:
    case eLoginTimeout:
    case eIOTimeout:
    case eExclusiveServer:
    case eUseConnPool:
    case eConnPoolMinSize:
    case eConnPoolMaxSize:
    {
        string name = x_GetName(param);
        if ( !value.empty()  ||  m_Url.GetArgs().IsSetValue(name)) {
            m_Url.GetArgs().SetValue(name, value);
        }
        return *this;
    }
    case eArgsString:
        m_Url.GetArgs().GetArgs().clear();
        m_Url.GetArgs().SetQueryString(value);
        return *this;
    }
    _ASSERT(false);
    return *this;
}


inline CSDB_ConnectionParam&
CSDB_ConnectionParam::Set(const string& url_string, TSetFlags flags)
{
    CSDB_ConnectionParam param(url_string);
    return Set(param, flags);
}


inline CSDB_ConnectionParam&
CDatabase::GetConnectionParam(void)
{
    return m_Params;
}

inline const CSDB_ConnectionParam&
CDatabase::GetConnectionParam(void) const
{
    return m_Params;
}

inline CQuery
CDatabase::NewQuery(const string& sql)
{
    CQuery q = NewQuery();
    q.SetSql(sql);
    return q;
}


inline
string CSDB_Decryptor::Decrypt(const string& ciphertext,
                               const CTempString& key)
{
    return x_Decrypt(ciphertext, x_GetKey(key));
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
