#ifndef CPPCORE__DBAPI__SIMPLE__SDBAPI_IMPL__HPP
#define CPPCORE__DBAPI__SIMPLE__SDBAPI_IMPL__HPP

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
 * Author:  Pavel Ivanov
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <dbapi/dbapi.hpp>

#include <dbapi/simple/sdbapi.hpp>

BEGIN_NCBI_SCOPE

class CConnHolder;

class CSDB_UserHandler : public CDB_UserHandler_Exception
{
public:
    CSDB_UserHandler(CConnHolder& conn)
        : m_Conn(conn)
        { }
    bool HandleMessage(int severity, int msgnum, const string& message);
    
private:
    CConnHolder& m_Conn;
};

class CConnHolder : public CObject
{
public:
    CConnHolder(IConnection* conn);
    virtual ~CConnHolder(void);

    IConnection* GetConn(void) const;
    void AddOpenRef(void);
    void CloseRef(void);
    
    const CDB_Exception::SContext& GetContext(void) const;
    void SetTimeout(const CTimeout& timeout);
    void ResetTimeout(void);
    const list<string>& GetPrintOutput(void) const;
    void ResetPrintOutput();

private:
    CConnHolder(const CConnHolder&);
    CConnHolder& operator= (const CConnHolder&);

    IConnection* m_Conn;
    size_t       m_DefaultTimeout;
    bool         m_HasCustomTimeout;
    Uint4        m_CntOpen;
    list<string> m_PrintOutput;
    CRef<CDB_Exception::SContext> m_Context;
    CRef<CSDB_UserHandler> m_Handler;
    CMutex       m_Mutex;

    friend class CSDB_UserHandler;
};

class CDatabaseImpl : public CObject
{
public:
    CDatabaseImpl(void);
    CDatabaseImpl(const CDatabaseImpl& other);
    ~CDatabaseImpl(void);

    void Connect(const CSDB_ConnectionParam& params);
    bool IsOpen() const;
    bool EverConnected() const;
    void Close();

    IConnection* GetConnection(void);
    void SetTimeout(const CTimeout& timeout);
    void ResetTimeout(void);
    const list<string>& GetPrintOutput(void) const;
    void ResetPrintOutput();

    const CDB_Exception::SContext& GetContext(void) const;

private:
    CRef<CConnHolder>   m_Conn;
    bool                m_IsOpen;
    bool                m_EverConnected;
};


class CBulkInsertImpl : public CObject
{
public:
    CBulkInsertImpl(CDatabaseImpl* db_impl,
                    const string&  tableName,
                    int            autoflush);
    ~CBulkInsertImpl(void);

    void SetHints(CTempString hints);
    void AddHint(IBulkInsert::EHints hint, unsigned int value);
    void AddOrderHint(CTempString columns);

    void Bind(int col, ESDB_Type type);
    void EndRow(void);
    void Complete(void);

    void WriteNull(void);
    template <class T>
    void WriteVal(const T& val);

private:
    void x_CheckCanWrite(int col);
    void x_CheckWriteStarted(void);

    const CDB_Exception::SContext& x_GetContext(void) const;

    CRef<CDatabaseImpl> m_DBImpl;
    IBulkInsert*        m_BI;
    vector<CVariant>    m_Cols;
    int                 m_Autoflush;
    int                 m_RowsWritten;
    int                 m_ColsWritten;
    bool                m_WriteStarted;

    CRef<CDB_Exception::SContext> m_Context;
};


class IQueryFieldBasis
{
public:
    virtual ~IQueryFieldBasis() { }
    
    virtual const CVariant*                GetValue(void) const = 0;
    virtual const CDB_Exception::SContext& x_GetContext(void) const = 0;

    virtual CNcbiOstream* GetOStream(size_t blob_size,
                                     TBlobOStreamFlags flags) const;
    virtual CBlobBookmark GetBookmark(void) const;
};

class CRemoteQFB : public IQueryFieldBasis
{
public:
    CRemoteQFB(CQueryImpl& query, unsigned int col_num)
        : m_Query(query), m_ColNum(col_num)
        { }
    
    const CVariant*                GetValue(void) const override;
    const CDB_Exception::SContext& x_GetContext(void) const override;

    CNcbiOstream* GetOStream(size_t blob_size,
                             TBlobOStreamFlags flags) const override;
    CBlobBookmark GetBookmark(void) const override;

private:
    CQueryImpl&  m_Query;
    unsigned int m_ColNum;
};

class CLocalQFB : public IQueryFieldBasis
{
public:
    CLocalQFB(CVariant* value, const CDB_Exception::SContext& context)
        : m_Value(value), m_Context(new CDB_Exception::SContext(context))
        { }

    const CVariant*                GetValue(void) const override
        { return m_Value.get(); }
    const CDB_Exception::SContext& x_GetContext(void) const override
        { return *m_Context; }

private:
    unique_ptr<CVariant>                m_Value;
    unique_ptr<CDB_Exception::SContext> m_Context;
};

class CParamQFB : public CLocalQFB
{
public:
    CParamQFB(CVariant* value, const CDB_Exception::SContext& context,
              ESP_ParamType param_type)
        : CLocalQFB(value, context), m_ParamType(param_type)
        { }

    ESP_ParamType GetParamType(void) const         { return m_ParamType; }
    void          SetParamType(ESP_ParamType type) { m_ParamType = type; }

private:
    ESP_ParamType m_ParamType;
};


class CQueryFieldImpl : public CObject
{
public:
    CQueryFieldImpl(CQueryImpl* q, unsigned int col_num);
    CQueryFieldImpl(CQueryImpl* q, CVariant* v, ESP_ParamType param_type);

    virtual CRef<CQueryFieldImpl> Detach(void);

    const CVariant* GetValue(void) const;
    virtual CNcbiIstream& AsIStream(void) const;
    virtual const vector<unsigned char>& AsVector(void) const;
    virtual CNcbiOstream& GetOStream(size_t blob_size,
                                     TBlobOStreamFlags flags) const;
    virtual CBlobBookmark GetBookmark(void) const;

protected:
    friend class CQueryImpl;

    // Takes ownership of *qf.m_Basis!
    CQueryFieldImpl(CQueryFieldImpl& qf);
    
    const CDB_Exception::SContext& x_GetContext(void) const;

    unique_ptr<IQueryFieldBasis> m_Basis;
};

class CQueryBlobImpl : public CQueryFieldImpl
{
public:
    CQueryBlobImpl(CQueryImpl* q, unsigned int col_num);
    CQueryBlobImpl(CQueryImpl* q, CVariant* v, ESP_ParamType param_type);

    CRef<CQueryFieldImpl> Detach(void) override;

    CNcbiIstream& AsIStream(void) const override;
    const vector<unsigned char>& AsVector(void) const override;
    CNcbiOstream& GetOStream(size_t blob_size,
                             TBlobOStreamFlags flags) const override;
    CBlobBookmark GetBookmark(void) const override;

private:
    // Takes ownership of *qb.m_Basis!
    CQueryBlobImpl(CQueryBlobImpl& qb);

    /// Vector to cache BLOB value
    mutable vector<unsigned char>       m_Vector;
    /// String to cache BLOB value
    mutable string                      m_ValueForStream;
    /// Stream to cache BLOB value
    mutable unique_ptr<CNcbiIstrstream> m_IStream;
    /// Stream to change BLOB value
    mutable unique_ptr<CNcbiOstream>    m_OStream;
};

struct SQueryRSMetaData : public CObject
{
    typedef map<string, int> TColNumsMap;
    
    TColNumsMap       col_nums;
    vector<string>    col_names;
    vector<ESDB_Type> col_types;

    CConstRef<CDB_Exception::SContext> exception_context;
};

class CQueryImpl: public CObject
{
public:
    CQueryImpl(CDatabaseImpl* db_impl);
    ~CQueryImpl(void);

    template <class T>
    void SetParameter(CTempString name, const T& value, ESDB_Type type, ESP_ParamType param_type);
    void SetNullParameter(CTempString name, ESDB_Type type, ESP_ParamType param_type);
    const CQuery::CField& GetParameter(CTempString name);
    void ClearParameter(CTempString name);
    void ClearParameters(void);

    void SetSql(CTempString sql);
    void Execute(const CTimeout& timeout);
    void ExecuteSP(CTempString sp, const CTimeout& timeout);
    void Cancel(void);
    bool HasMoreResultSets(void);
    void PurgeResults(void);
    void BeginNewRS(void);
    void Next(void);
    void RequireRowCount(unsigned int min_rows, unsigned int max_rows);
    void VerifyDone(CQuery::EHowMuch how_much = CQuery::eThisResultSet);
    const CQuery::CRow& GetRow(void) const;
    const CQuery::CField& GetColumn(const CDBParamVariant& col) const;
    const CVariant& GetFieldValue(unsigned int col_num);
    bool IsFinished(CQuery::EHowMuch how_much = CQuery::eThisResultSet) const;

    void SetIgnoreBounds(bool is_ignore);
    unsigned int GetResultSetNo(void) const;
    unsigned int GetRowNo(CQuery::EHowMuch how_much = CQuery::eAllResultSets) const;
    int GetRowCount(void) const;
    int GetStatus(void) const;
    unsigned int GetTotalColumns(void) const;
    string GetColumnName(unsigned int col) const;
    ESDB_Type GetColumnType(unsigned int col) const;

    CDatabaseImpl* GetDatabase(void) const;
    IConnection* GetConnection(void);

    const list<string>& GetPrintOutput(void) const;
    // Historically private, but useful for CQuery's inner classes too.
    const CDB_Exception::SContext& x_GetContext(void) const;

private:
    void x_CheckCanWork(bool need_rs = false) const;
    void x_SetOutParameter(const string& name, const CVariant& value);
    void x_ClearAllParams(void);
    void x_CheckRowCount(void);
    bool x_Fetch(void);
    void x_InitBeforeExec(void);
    void x_InitRSFields(void);
    void x_DetachAllFields(void);
    void x_Close(void);


    typedef map<string, CQuery::CField>         TParamsMap;

    CRef<CDatabaseImpl> m_DBImpl;
    IStatement*         m_Stmt;
    ICallableStatement* m_CallStmt;
    TParamsMap          m_Params;
    string              m_Sql;
    IResultSet*         m_CurRS;
    bool                m_IgnoreBounds;
    bool                m_HasExplicitMode;
    bool                m_RSBeginned;
    bool                m_RSFinished;
    bool                m_Executed;
    bool                m_ReportedWrongRowCount;
    bool                m_IsSP;
    bool                m_RowUnderConstruction;
    unsigned int        m_CurRSNo;
    unsigned int        m_CurRowNo;
    unsigned int        m_CurRelRowNo;
    unsigned int        m_MinRowCount;
    unsigned int        m_MaxRowCount;
    int                 m_RowCount;
    int                 m_Status;
    CQuery::CRow        m_Row;
    mutable CRef<CDB_Exception::SContext> m_Context;
};


class CBlobBookmarkImpl : public CObject
{
public:
    CBlobBookmarkImpl(CDatabaseImpl* db_impl, I_BlobDescriptor* descr);

    CNcbiOstream& GetOStream(size_t blob_size, TBlobOStreamFlags flags);

private:
    const CDB_Exception::SContext& x_GetContext(void) const;

    CRef<CDatabaseImpl> m_DBImpl;
    auto_ptr<I_BlobDescriptor> m_Descr;
    auto_ptr<CWStream> m_OStream;
};

END_NCBI_SCOPE

#endif  // CPPCORE__DBAPI__SIMPLE__SDBAPI_IMPL__HPP
