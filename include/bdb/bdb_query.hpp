#ifndef BDB_QUERY__HPP
#define BDB_QUERY__HPP
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *	 BDB Query
 *
 */

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/// Query node class
/// 
/// Query node works as part of the query clause
class NCBI_BDB_EXPORT CBDB_QueryNode
{
public:
    enum ENodeType
    {
        eLogical,  // Logical AND, OR, NOT, etc
        eOperator, // =, <, <=, like
        eFunction,
        eValue,    // Constant value
        eDBField   // Database field
    };

    /// Subtype of ENodeType - eLogical
    enum ELogicalType
    {
        eAnd,
        eOr,
        eNot
    };

    /// Subtype of ENodeType - eOperator
    enum EOperatorType 
    {
        eEQ,
        eGT,
        eGE,
        eLT,
        eLE
    };

public:

    CBDB_QueryNode(string value = kEmptyStr);
    CBDB_QueryNode(ELogicalType  ltype);
    CBDB_QueryNode(EOperatorType otype);

    CBDB_QueryNode(const CBDB_QueryNode& qnode);
    CBDB_QueryNode& operator=(const CBDB_QueryNode& qnode);

    ENodeType     GetType() const { return m_NodeType; }
    ELogicalType  GetLogicType() const;
    EOperatorType GetOperatorType() const;

    const string& GetValue() const { return m_Value; }
    void SetValue(const string& value) { m_Value = value; }

    bool HasValue() const { return !m_Value.empty(); }

    /// Set node type to eDBField
    /// @param 
    ///   field_idx - database field index
    void SetField(int field_idx);

    int GetFiledIdx() const { return m_SubType.FieldIdx; }

protected:

    ENodeType     m_NodeType;
    union {
        ELogicalType   LogicalType;
        EOperatorType  OperatorType;
        int            FieldIdx;
    } m_SubType;

    string m_Value;
};


class NCBI_BDB_EXPORT CBDB_Query
{
public:
    typedef CTreeNode<CBDB_QueryNode> TQueryClause;

    CBDB_Query();
    ~CBDB_Query();

    TQueryClause& GetQueryClause() { return *m_QueryClause; }
    const TQueryClause& GetQueryClause() const { return *m_QueryClause; }

    /// Replace current query clause with the new one.
    /// CBDB_Query object takes ownership on the passed argument.
    void SetQueryClause(TQueryClause* query_clause);

    void ResetQueryClause();

    /// Creates new operator node of the query
    /// Caller is responsible for destruction 
    /// (in most cases the result is immediately added to the query tree).
    static
    TQueryClause* NewOperatorNode(CBDB_QueryNode::EOperatorType otype,  
                                  const string&   arg1, 
                                  const string&   arg2);


private:
    CBDB_Query(const CBDB_Query& query);
    CBDB_Query& operator=(const CBDB_Query& query);

public:
    TQueryClause*     m_QueryClause;
};

class CBDB_File;
class CBDB_FileCursor;

/// Scans the BDB file, searches the matching records
///
/// Search criteria is assigned by CBDB_Query
///
class NCBI_BDB_EXPORT CBDB_FileScanner
{
public:
    /// Scanner control codes
    enum EScanAction
    {
        eContinue,   ///< Keep scanning
        eStop        ///< Stop search process
    };

public:
    CBDB_FileScanner(CBDB_File& db_file);
    virtual ~CBDB_FileScanner();

    /// Scan the BDB file, search the qualified records
    /// When record found function calls OnRecordFound
    /// @sa OnRecordFound
    void Scan(CBDB_Query& query);

    /// Scan the BDB file cursor, search the qualified records
    /// When record found function calls OnRecordFound
    /// @sa OnRecordFound
    void Scan(CBDB_FileCursor& cur, CBDB_Query& query);

    /// Static query evaluation without changing current position 
    /// in the database file. Intended purpose: debugging
    bool StaticEvaluate(CBDB_Query& query) 
    { 
        return Evaluate(query); 
    }

    /// Called when scanner finds a record matching the query terms
    /// Function returns a control code (EScanAction) to confirm or
    /// interrupt scanning process
    virtual EScanAction OnRecordFound();

protected:
    /// Returns TRUE if file record matches the query
    bool Evaluate(CBDB_Query& query);

    void ResolveFields(CBDB_Query& query);

private:
    CBDB_FileScanner(const CBDB_FileScanner&);
    CBDB_FileScanner& operator=(const CBDB_FileScanner&);
private:
    CBDB_File&   m_File;   ///< Searched BDB file
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/02/17 19:06:27  kuznets
 * GCC warning fix
 *
 * Revision 1.1  2004/02/17 17:26:29  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
