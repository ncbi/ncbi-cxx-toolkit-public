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
 * File Description:  BDB libarary query implementation
 *
 */

#include <bdb/bdb_query.hpp>
#include <bdb/bdb_cursor.hpp>
#include <util/resource_pool.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CBDB_QueryNode
//


CBDB_QueryNode::CBDB_QueryNode(string value)
 : m_NodeType(eValue),
   m_Value(value)
{
}

CBDB_QueryNode::CBDB_QueryNode(ELogicalType  ltype)
 : m_NodeType(eLogical)
{
    m_SubType.LogicalType = ltype;
}


CBDB_QueryNode::CBDB_QueryNode(EOperatorType otype)
 : m_NodeType(eOperator)
{
    m_SubType.OperatorType = otype;
}


CBDB_QueryNode::CBDB_QueryNode(const CBDB_QueryNode& qnode)
{
    m_NodeType = qnode.m_NodeType;
    m_Value = qnode.m_Value;
    m_SubType.LogicalType = qnode.m_SubType.LogicalType;
}


CBDB_QueryNode& CBDB_QueryNode::operator=(const CBDB_QueryNode& qnode)
{
    m_NodeType = qnode.m_NodeType;
    m_Value = qnode.m_Value;
    m_SubType.LogicalType = qnode.m_SubType.LogicalType;

    return *this;
}

CBDB_QueryNode::ELogicalType CBDB_QueryNode::GetLogicType() const
{
    if (m_NodeType == eLogical) {
        return m_SubType.LogicalType;
    }
    // Caller asking to get sub-type as logical when the node type is not
    BDB_THROW(eQueryError, "Incorrect query node type");
}

CBDB_QueryNode::EOperatorType CBDB_QueryNode::GetOperatorType() const
{
    if (m_NodeType == eOperator) {
        return m_SubType.OperatorType;
    }
    // Caller asking to get sub-type as operator when the node type is not
    BDB_THROW(eQueryError, "Incorrect query node type");
}

void CBDB_QueryNode::SetField(int field_idx)
{
    m_NodeType = eDBField;
    m_SubType.FieldIdx = field_idx;
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_Query
//

CBDB_Query::CBDB_Query()
{
    m_QueryClause = new TQueryClause;
}

CBDB_Query::~CBDB_Query()
{
    delete m_QueryClause;
}

/// Reset all intermidiate results of tree evaluation (values)
/// value constants will not be destroyed
static 
ETreeTraverseCode s_ResetQueryNode(CTreeNode<CBDB_QueryNode>& tr, int delta)
{
    if (delta < 0)
        return eTreeTraverse;

    CBDB_QueryNode& qnode = tr.GetValue();
    if (qnode.GetType() != CBDB_QueryNode::eValue) {
        qnode.SetValue(kEmptyStr);
    }

    return eTreeTraverse;
}

void CBDB_Query::ResetQueryClause()
{
    TreeDepthFirstTraverse(*m_QueryClause, s_ResetQueryNode);
}

void CBDB_Query::SetQueryClause(TQueryClause* query_clause)
{
    delete m_QueryClause;
    m_QueryClause = query_clause;
}

CBDB_Query::TQueryClause* 
CBDB_Query::NewOperatorNode(CBDB_QueryNode::EOperatorType otype,
                            const string&                 arg1, 
                            const string&                 arg2)
{
    auto_ptr<TQueryClause> tr(new TQueryClause(CBDB_QueryNode(otype)));

    tr->AddNode(new TQueryClause(CBDB_QueryNode(arg1)));
    tr->AddNode(new TQueryClause(CBDB_QueryNode(arg2)));

    return tr.release();
}


/////////////////////////////////////////////////////////////////////////////
//  Internal evaluation functors
//

/// Field resolution functor
///
/// Every node is inspected if it contains a field value and
/// marks it using CBDB_File::TUnifiedFieldIndex
///
class CQueryTreeFieldResolveFunc
{
public: 
    CQueryTreeFieldResolveFunc(CBDB_File& db_file)
    : m_File(db_file)
    {}
  
    ETreeTraverseCode 
    operator()(CTreeNode<CBDB_QueryNode>& tr, int delta)
    {
        CBDB_QueryNode& qnode = tr.GetValue();
        if (delta == 0 || delta == 1) {
            // If node has children, we skip it and process on the way back
            if (!tr.IsLeaf())
                return eTreeTraverse;
        }
        if (qnode.GetType() == CBDB_QueryNode::eDBField)
                return eTreeTraverse;

        if (qnode.HasValue()) {
            const string& fname = qnode.GetValue();
            CBDB_File::TUnifiedFieldIndex fidx = m_File.GetFieldIdx(fname);
            if (fidx) {
                qnode.SetField(fidx);
            }
        }
        return eTreeTraverse;
    }
private:
    CBDB_File&  m_File;
};

/// Query execition environment
class CQueryExecEnv
{
public:

    CQueryExecEnv(CBDB_File& db_file)
    : m_File(db_file)
    {}

    CBDB_File& GetFile(void) { return m_File; }
    CResourcePool<string>&  GetStrPool() { return m_StrPool; }

private:
    CQueryExecEnv(const CQueryExecEnv& qenv);
    CQueryExecEnv& operator=(const CQueryExecEnv& qenv);

private:
    CBDB_File&            m_File;      /// Query database file
    CResourcePool<string> m_StrPool;   /// Pool of string variables   
};


/// Base class for functions of the evaluation engine
///
class CScannerFunctor
{
public:
    CScannerFunctor(CQueryExecEnv& env)
     : m_QueryEnv(env)
    {}

private:
    CScannerFunctor(const CScannerFunctor&);
    CScannerFunctor& operator=(const CScannerFunctor&);

protected:
    CQueryExecEnv&               m_QueryEnv;
};

/// Base class for 2 argument functions
class CScannerFunctorArg2 : public CScannerFunctor
{
public:
    CScannerFunctorArg2(CQueryExecEnv& env)
     : CScannerFunctor(env),
       m_Arg1(0),
       m_Arg2(0),
       m_ArgValue1(0),
       m_ArgValue2(0)
    {}

    ~CScannerFunctorArg2()
    {
        CResourcePool<string>& str_pool = m_QueryEnv.GetStrPool();
        if (m_ArgValue1) {
            str_pool.Return(m_ArgValue1);
        }
        if (m_ArgValue2) {
            str_pool.Return(m_ArgValue2);
        }
    }

    /// Extract function arguments from the parsing tree
    void GetArguments(CTreeNode<CBDB_QueryNode>& tr)
    {
        m_Arg1 = m_Arg2 = 0;

        CBDB_File& db_file = m_QueryEnv.GetFile();
        CResourcePool<string>& str_pool = m_QueryEnv.GetStrPool();

        typedef CTreeNode<CBDB_QueryNode>  TTree;
        TTree::TNodeList_CI it = tr.SubNodeBegin();
        TTree::TNodeList_CI it_end = tr.SubNodeEnd();

        for (; it != it_end; ++it) {
            const TTree* t = *it;
            const CBDB_QueryNode& qnode = t->GetValue();

            // Check if the argument should be taken from the
            // db field
            if (qnode.GetType() == CBDB_QueryNode::eDBField) {
                int fidx = qnode.GetFiledIdx();
                const CBDB_Field& fld = db_file.GetField(fidx);
                if (m_Arg1 == 0) {
                    if (m_ArgValue1 == 0) {
                        m_ArgValue1 = str_pool.Get();
                    }
                    *m_ArgValue1 = fld.GetString();
                    m_Arg1 = m_ArgValue1;
                } else {
                    if (m_ArgValue2 == 0) {
                        m_ArgValue2 = str_pool.Get();
                    }
                    *m_ArgValue2 = fld.GetString();
                    m_Arg2 = m_ArgValue2;
                }
                continue;
            }

            // Get constant value or return type of the subnodes

            const string& str = qnode.GetValue();

            if (m_Arg1 == 0) {
                m_Arg1 = &str;
            } else {
                _ASSERT(m_Arg2 == 0);
                m_Arg2 = &str;
            }
        } // for 
    }

protected:
    const string*  m_Arg1;
    const string*  m_Arg2;
    string*        m_ArgValue1;
    string*        m_ArgValue2;
};


/// EQ function 
class CScannerFunctorEQ : public CScannerFunctorArg2
{
public:
    CScannerFunctorEQ(CQueryExecEnv& env)
     : CScannerFunctorArg2(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr);

        CBDB_QueryNode& qnode = tr.GetValue();

        if (*m_Arg1 == *m_Arg2) {
            qnode.SetValue("1");
        } else {
            qnode.SetValue("0");
        }
    }
};


/// AND function 
class CScannerFunctorAND : public CScannerFunctorArg2
{
public:
    CScannerFunctorAND(CQueryExecEnv& env)
     : CScannerFunctorArg2(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr);

        CBDB_QueryNode& qnode = tr.GetValue();

        if (*m_Arg1 != "0" && *m_Arg2 != "0") {
            qnode.SetValue("1");
        } else {
            qnode.SetValue("0");
        }
    }
};



/// OR function 
class CScannerFunctorOR : public CScannerFunctorArg2
{
public:
    CScannerFunctorOR(CQueryExecEnv& env)
     : CScannerFunctorArg2(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr);

        CBDB_QueryNode& qnode = tr.GetValue();

        if (*m_Arg1 != "0" || *m_Arg2 != "0") {
            qnode.SetValue("1");
        } else {
            qnode.SetValue("0");
        }
    }
};



/////////////////////////////////////////////////////////////////////////////
//  CBDB_FileScanner
//

CBDB_FileScanner::CBDB_FileScanner(CBDB_File& db_file)
 : m_File(db_file)
{
}


CBDB_FileScanner::~CBDB_FileScanner()
{
}


CBDB_FileScanner::EScanAction CBDB_FileScanner::OnRecordFound()
{
    return eContinue;
}


void CBDB_FileScanner::Scan(CBDB_Query& query)
{
    ResolveFields(query);
    CBDB_FileCursor cur(m_File);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {

        bool res = Evaluate(query);
        if (res) {
            EScanAction act = OnRecordFound();
            if (act == eStop) {
                break;
            }
        }
    } // while
}

void CBDB_FileScanner::Scan(CBDB_FileCursor& cur, 
                            CBDB_Query&      query)
{
    ResolveFields(query);

    while (cur.Fetch() == eBDB_Ok) {

        bool res = Evaluate(query);
        if (res) {
            EScanAction act = OnRecordFound();
            if (act == eStop) {
                break;
            }
        }
    } // while
}

/// The main tree evaluation functor
class CScannerEvaluateFunc
{
public: 
    CScannerEvaluateFunc(CQueryExecEnv& env)
    : m_QueryEnv(env)
    {}
  
    ETreeTraverseCode 
    operator()(CTreeNode<CBDB_QueryNode>& tr, int delta)
    {
        CBDB_QueryNode& qnode = tr.GetValue();

// cout << delta << " " << tr.GetValue().GetValue() << endl;

        if (delta == 0 || delta == 1) {
            // If node has children, we skip it and process on the way back
            if (!tr.IsLeaf())
                return eTreeTraverse;
        }

        if (!qnode.HasValue()) {
            switch (qnode.GetType()) {
            case CBDB_QueryNode::eValue:
                break;
            case CBDB_QueryNode::eOperator:
                {
                    CBDB_QueryNode::EOperatorType eop = qnode.GetOperatorType();
                    switch (eop) {
                    case CBDB_QueryNode::eEQ:
                        {
                        CScannerFunctorEQ func(m_QueryEnv);
                        func.Eval(tr);
                        }
                        break;
                    default:
                        _ASSERT(0);
                    } // switch eop
                }
                break;
            case CBDB_QueryNode::eLogical:
                {
                    CBDB_QueryNode::ELogicalType elogic = qnode.GetLogicType();
                    switch (elogic) {
                    case CBDB_QueryNode::eAnd:
                        {
                        CScannerFunctorAND func(m_QueryEnv);
                        func.Eval(tr);
                        }
                        break;
                    case CBDB_QueryNode::eOr:
                        {
                        CScannerFunctorOR func(m_QueryEnv);
                        func.Eval(tr);
                        }
                        break;
                    default:
                        _ASSERT(0);
                    } // switch elogic
                }
                break;
            default:
                break;
            } // switch node type

        } // if

        return eTreeTraverse;
    }

private:
    CQueryExecEnv&  m_QueryEnv;
};


bool CBDB_FileScanner::Evaluate(CBDB_Query& query)
{
    ResolveFields(query);

    CBDB_Query::TQueryClause& qtree = query.GetQueryClause();

    CQueryExecEnv query_env(m_File);
    CScannerEvaluateFunc scanner_eval(query_env);

    TreeDepthFirstTraverse(qtree, scanner_eval);

    const string& v = qtree.GetValue().GetValue();

    if (v == "0")
        return false;
    return true;
}

void CBDB_FileScanner::ResolveFields(CBDB_Query& query)
{
    CBDB_Query::TQueryClause& qtree = query.GetQueryClause();
    CQueryTreeFieldResolveFunc resolve_func(m_File);

    TreeDepthFirstTraverse(qtree, resolve_func);
}






/// The main tree printing functor class.
/// Used for internal debugging purposes.
///
/// @internal
///
class CQueryTreePrintFunc
{
public: 
    CQueryTreePrintFunc(CNcbiOstream& os)
    : m_OStream(os),
      m_Level(0)
    {}

    void PrintLevelMargin()
    {
        for (int i = 0; i < m_Level; ++i) {
            m_OStream << "  ";
        }
    }
  
    ETreeTraverseCode 
    operator()(const CTreeNode<CBDB_QueryNode>& tr, int delta) 
    {
        const CBDB_QueryNode& qnode = tr.GetValue();

        m_Level += delta;

        if (delta < 0)
            return eTreeTraverse;

        PrintLevelMargin();

        switch (qnode.GetType()) {
        case CBDB_QueryNode::eValue:
            m_OStream << qnode.GetValue();
            break;
        case CBDB_QueryNode::eOperator:
            {
                CBDB_QueryNode::EOperatorType eop = qnode.GetOperatorType();
                switch (eop) {
                case CBDB_QueryNode::eEQ:
                    m_OStream << "EQ";
                    break;
                default:
                    _ASSERT(0);
                } // switch eop
            }
            if (qnode.HasValue()) {
                m_OStream << " => " << qnode.GetValue();
            }
            break;
        case CBDB_QueryNode::eLogical:
            {
                CBDB_QueryNode::ELogicalType elogic = qnode.GetLogicType();
                switch (elogic) {
                case CBDB_QueryNode::eAnd:
                    m_OStream << "AND";
                    break;
                case CBDB_QueryNode::eOr:
                    m_OStream << "OR";
                    break;
                default:
                    _ASSERT(0);
                } // switch elogic
            }
            if (qnode.HasValue()) {
                m_OStream << " => " << qnode.GetValue();
            }
            break;
        case CBDB_QueryNode::eDBField:
            m_OStream << "@" << qnode.GetValue();
            break;
        default:
            if (qnode.HasValue()) {
                m_OStream << qnode.GetValue();
            }
            break;
        } // switch node type


        m_OStream << "\n";

        return eTreeTraverse;
    }

private:
    CNcbiOstream&  m_OStream;
    int            m_Level;
};



void BDB_PrintQueryTree(CNcbiOstream& os, const CBDB_Query& query)
{
    // Here I use a const cast hack because TreeDepthFirstTraverse
    // uses a non-cost iterators and semantics in the algorithm.
    // When a const version of TreeDepthFirstTraverse is ready
    // we can get rid of this...

    const CBDB_Query::TQueryClause& qtree = query.GetQueryClause();
    CBDB_Query::TQueryClause& qtree_nc = 
           const_cast<CBDB_Query::TQueryClause&>(qtree);

    CQueryTreePrintFunc func(os);
    TreeDepthFirstTraverse(qtree_nc, func);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/02/19 17:35:57  kuznets
 * + BDB_PrintQueryTree (tree printing utility function for debugging)
 *
 * Revision 1.1  2004/02/17 17:26:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
