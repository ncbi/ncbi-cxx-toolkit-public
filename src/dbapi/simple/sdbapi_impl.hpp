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

class CConnHolder : public CObject
{
public:
    CConnHolder(IConnection* conn);
    virtual ~CConnHolder(void);

    IConnection* GetConn(void) const;
    void AddOpenRef(void);
    void CloseRef(void);

private:
    CConnHolder(const CConnHolder&);
    CConnHolder& operator= (const CConnHolder&);

    IConnection* m_Conn;
    Uint4        m_CntOpen;
};

class CDatabaseImpl : public CObject
{
public:
    CDatabaseImpl(const CDatabaseImpl& other);
    CDatabaseImpl(const CSDB_ConnectionParam& params);
    ~CDatabaseImpl(void);

    bool IsOpen() const;
    void Close();

    IConnection* GetConnection(void);

private:
    CRef<CConnHolder>   m_Conn;
    bool                m_IsOpen;
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


    CRef<CDatabaseImpl> m_DBImpl;
    IBulkInsert*        m_BI;
    vector<CVariant>    m_Cols;
    int                 m_Autoflush;
    int                 m_RowsWritten;
    int                 m_ColsWritten;
    bool                m_WriteStarted;
};


struct SQueryParamInfo
{
    ESP_ParamType   type;
    CVariant*       value;
    CQuery::CField* field;

    SQueryParamInfo(void);
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
    void Execute(void);
    void ExecuteSP(CTempString sp);
    bool HasMoreResultSets(void);
    void PurgeResults(void);
    void BeginNewRS(void);
    void Next(void);
    void RequireRowCount(unsigned int min_rows, unsigned int max_rows);
    void VerifyDone(CQuery::EHowMuch how_much = CQuery::eThisResultSet);
    const CQuery::CField& GetColumn(const CDBParamVariant& col) const;
    const CVariant& GetFieldValue(const CQuery::CField& field);
    bool IsFinished(CQuery::EHowMuch how_much = CQuery::eThisResultSet) const;

    void SetIgnoreBounds(bool is_ignore);
    unsigned int GetResultSetNo(void);
    unsigned int GetRowNo(CQuery::EHowMuch how_much = CQuery::eAllResultSets);
    int GetRowCount(void);
    int GetStatus(void);
    int GetTotalColumns(void);
    string GetColumnName(unsigned int col);
    ESDB_Type GetColumnType(unsigned int col);

    CDatabaseImpl* GetDatabase(void) const;
    IConnection* GetConnection(void);

private:
    void x_CheckCanWork(bool need_rs = false) const;
    void x_SetOutParameter(const string& name, const CVariant& value);
    void x_ClearAllParams(void);
    void x_CheckRowCount(void);
    bool x_Fetch(void);
    void x_InitBeforeExec(void);
    void x_InitRSFields(void);
    void x_Close(void);


    typedef map<string, SQueryParamInfo>        TParamsMap;
    typedef map<string, int>                    TColNumsMap;
    typedef vector< AutoPtr<CQuery::CField> >   TFields;

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
    unsigned int        m_CurRSNo;
    unsigned int        m_CurRowNo;
    unsigned int        m_CurRelRowNo;
    unsigned int        m_MinRowCount;
    unsigned int        m_MaxRowCount;
    int                 m_RowCount;
    int                 m_Status;
    TColNumsMap         m_ColNums;
    TFields             m_Fields;
};


class CBlobBookmarkImpl : public CObject
{
public:
    CBlobBookmarkImpl(CDatabaseImpl* db_impl, I_ITDescriptor* descr);

    CNcbiOstream& GetOStream(size_t blob_size, CQuery::EAllowLog log_it);

private:
    CRef<CDatabaseImpl> m_DBImpl;
    auto_ptr<I_ITDescriptor> m_Descr;
    auto_ptr<CWStream> m_OStream;
};

END_NCBI_SCOPE

#endif  // CPPCORE__DBAPI__SIMPLE__SDBAPI_IMPL__HPP
