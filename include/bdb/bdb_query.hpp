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

/// @file bdb_query.hpp
/// Queries for BDB library.

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Query
 *
 * @{
 */

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

    /// Constuction of value type node
    CBDB_QueryNode(string value = kEmptyStr);
    /// Construction of logical (AND, OR, etc) node
    CBDB_QueryNode(ELogicalType  ltype);
    /// Construction of operator node
    CBDB_QueryNode(EOperatorType otype, bool not_flag = false);

    CBDB_QueryNode(const CBDB_QueryNode& qnode);
    CBDB_QueryNode& operator=(const CBDB_QueryNode& qnode);

    ~CBDB_QueryNode();

    ENodeType     GetType() const { return m_NodeType; }
    ELogicalType  GetLogicType() const;
    EOperatorType GetOperatorType() const;

    /// @return TRUE when node is an inverted operator (not EQ)
    bool IsNot() const { return m_NotFlag; }
    /// Set NOT flag (NOT EQ, etc)
    void SetNot() { m_NotFlag = true; }

    const string& GetValue() const { return m_Value; }
    string& GetValue() { return m_Value; }
    void SetValue(const string& value) { m_Value = value; }

    bool HasValue() const { return !m_Value.empty(); }

    /// Set node type to eDBField
    /// @param 
    ///   field_idx  database field index
    void SetField(int field_idx);

    int GetFiledIdx() const { return m_SubType.FieldIdx; }

    /// Set alternative value
    void SetAltValue(const string& v) { m_AltValue = v; }

    /// Get alternative value
    const string& GetAltValue() const { return m_AltValue; }

protected:

    ENodeType     m_NodeType;
    union {
        ELogicalType   LogicalType;
        EOperatorType  OperatorType;
        int            FieldIdx;
    } m_SubType;
    bool               m_NotFlag; ///< Inverted function
    string m_Value;
    string m_AltValue; ///< Alternative value
};

/// Query class incapsulates query tree (query clause) and 
/// implements set of utility methods to construct query trees.
///
class NCBI_BDB_EXPORT CBDB_Query
{
public:
    typedef CTreeNode<CBDB_QueryNode> TQueryClause;

    /// Contruct the query. If QueryClause is NOT NULL takes the ownership.
    /// @param 
    ///   qc  Query clause. (Should be created by new)
    CBDB_Query(TQueryClause* qc = 0);
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

    /// Creates new logical node of the query
    /// @sa NewOperatorNode
    static
    TQueryClause* NewLogicalNode(CBDB_QueryNode::ELogicalType otype,  
                                 TQueryClause*  arg1, 
                                 TQueryClause*  arg2);

    /// Creates new operator node of the query
    /// Caller is responsible for destruction 
    /// (in most cases the result is immediately added to the query tree).
    ///
    /// Function receives two argument nodes which are attached to the 
    /// result clause. In terms of memory management caller does not need 
    /// to deallocate them.
    ///
    /// @param
    ///   otype - type of the operator to create
    /// @param 
    ///   arg1  - argument node.
    /// @param 
    ///   arg2  - argument node.
    static
    TQueryClause* NewOperatorNode(CBDB_QueryNode::EOperatorType ltype,  
                                  TQueryClause*  arg1, 
                                  TQueryClause*  arg2);


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
protected:
    CBDB_File&   m_File;   ///< Searched BDB file
};


/// Function prints the query tree on the specified ostream
/// (Intended more for debugging purposes)
NCBI_BDB_EXPORT
void BDB_PrintQueryTree(CNcbiOstream& os, const CBDB_Query& query);

/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/05/17 19:34:53  gorelenk
 * Added missed export prefix to BDB_PrintQueryTree
 *
 * Revision 1.9  2004/03/23 14:50:43  kuznets
 * Implemented logical NOT
 *
 * Revision 1.8  2004/03/11 13:17:02  kuznets
 * Added alternative value to the tree node
 *
 * Revision 1.7  2004/03/10 14:22:55  kuznets
 * CBDB_FileScanner relaxed private to protected for m_File member
 *
 * Revision 1.6  2004/03/01 14:01:59  kuznets
 * Add non const GetValue() accessor to query node
 *
 * Revision 1.5  2004/02/24 16:30:47  kuznets
 * Improved doxygen documentation
 *
 * Revision 1.4  2004/02/24 14:11:44  kuznets
 * Some improvement of CBDB_Query here and there. Additional constructor
 * parameter, NewOperatorNode helper function.
 *
 * Revision 1.3  2004/02/19 17:35:15  kuznets
 * + BDB_PrintQueryTree (tree printing utility function for debugging)
 *
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
