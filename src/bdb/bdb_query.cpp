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

#include <ncbi_pch.hpp>
#include <bdb/bdb_query.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include <util/resource_pool.hpp>
#include <util/strsearch.hpp>

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


CBDB_QueryNode::CBDB_QueryNode(EOperatorType otype,
                               bool          not_flag)
 : m_NodeType(eOperator),
   m_NotFlag(not_flag)
{
    m_SubType.OperatorType = otype;
}


CBDB_QueryNode::CBDB_QueryNode(const CBDB_QueryNode& qnode)
{
    m_NodeType = qnode.m_NodeType;
    m_Value = qnode.m_Value;
    m_SubType.LogicalType = qnode.m_SubType.LogicalType;
    m_NotFlag = qnode.m_NotFlag;
}


CBDB_QueryNode& CBDB_QueryNode::operator=(const CBDB_QueryNode& qnode)
{
    m_NodeType = qnode.m_NodeType;
    m_Value = qnode.m_Value;
    m_SubType.LogicalType = qnode.m_SubType.LogicalType;
    m_NotFlag = qnode.m_NotFlag;

    return *this;
}

CBDB_QueryNode::~CBDB_QueryNode()
{
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

CBDB_Query::CBDB_Query(TQueryClause* qc)
{
    if (qc) {
        m_QueryClause = qc;
    } else {
        m_QueryClause = new TQueryClause;
    }
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
    qnode.SetAltValue(kEmptyStr);

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

CBDB_Query::TQueryClause* 
CBDB_Query::NewOperatorNode(CBDB_QueryNode::EOperatorType otype,
                            TQueryClause*  arg1, 
                            TQueryClause*  arg2)
{
    auto_ptr<TQueryClause> tr(new TQueryClause(CBDB_QueryNode(otype)));
    if (arg1)
        tr->AddNode(arg1);
    if (arg2)
        tr->AddNode(arg2);

    return tr.release();
}

CBDB_Query::TQueryClause* 
CBDB_Query::NewLogicalNode(CBDB_QueryNode::ELogicalType ltype,  
                           TQueryClause*  arg1, 
                           TQueryClause*  arg2)
{
    auto_ptr<TQueryClause> tr(new TQueryClause(CBDB_QueryNode(ltype)));
    if (arg1)
        tr->AddNode(arg1);
    if (arg2)
        tr->AddNode(arg2);

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
            string& fvalue = qnode.GetValue();
            int len = fvalue.length(); 
            // if string value is in apostrophe marks: remove it
            if (fvalue[0] == '\'' && fvalue[len-1] == '\'') {
                len -= 2;
                if (len) {
                    qnode.SetValue(fvalue.substr(1, len));
                } else {
                    qnode.SetValue(kEmptyStr);
                }
            } else {
                CBDB_File::TUnifiedFieldIndex fidx = m_File.GetFieldIdx(fvalue);
                if (fidx) {
                    qnode.SetField(fidx);
                }
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

/// Create new matcher
///
/// @internal
static
CBoyerMooreMatcher* s_MakeNewMatcher(const string& search_value)
{
    CBoyerMooreMatcher* matcher =
        new CBoyerMooreMatcher(search_value,
                               NStr::eNocase,
                               CBoyerMooreMatcher::ePrefixMatch);
    matcher->InitCommonDelimiters();
    return matcher;
}


/// Base class for functions of the evaluation engine
///
/// @internal
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

/// Base class for N argument functions
///
/// @internal
class CScannerFunctorArgN : public CScannerFunctor
{
public:
    /// Enum indicates how to interpret the plain value
    /// tree elements. It can be takes as is (eNoCheck) or
    /// converted to a "in any field" variant of check
    /// (similar to what PubMed does)
    enum EAllFieldsCheck 
    {
        eNoCheck,
        eCheckAll
    };

    /// Vector of strings borrowed from the query environment
    /// pool to keep temporary values during the query execution
    typedef vector<string*>       TArgValueVector;
    
    /// Vector of arguments, elements can point on values from 
    /// TArgValueVector or variables located in the query tree itself
    typedef vector<const string*> TArgVector;

    /// String matchers used for substring search
    typedef vector<CBoyerMooreMatcher*> TStringMatcherVector;

    /// If argument is a db field corresponding vector element 
    /// contains the field pointer
    typedef vector<const CBDB_Field*>  TFieldVector;

public:
    CScannerFunctorArgN(CQueryExecEnv& env)
     : CScannerFunctor(env)
    {}

    ~CScannerFunctorArgN();

    /// Checks if value is equal to any field in the database
    bool IsAnyField(CBDB_File& dbf, 
                    const string& search_value,
                    unsigned int arg_idx)
    {
        CBoyerMooreMatcher* matcher = GetMatcher(search_value, arg_idx);
        CBDB_File::TUnifiedFieldIndex fidx = 
                BDB_find_field(dbf, *matcher, &m_TmpStr);
        
        return (fidx != 0);
    }

    CBoyerMooreMatcher* GetMatcher(const string& search_value, 
                                   unsigned int  arg_idx)
    {
        CBoyerMooreMatcher* matcher = m_MatcherVector[arg_idx];
        if (!matcher) {
            m_MatcherVector[arg_idx] = matcher = 
                s_MakeNewMatcher(search_value);
        }
        return matcher;
    }

    /// Extract function arguments from the parsing tree
    void GetArguments(CTreeNode<CBDB_QueryNode>& tr, 
                      EAllFieldsCheck            check_mode = eNoCheck)
    {
        CBDB_File& db_file = m_QueryEnv.GetFile();
        CResourcePool<string>& str_pool = m_QueryEnv.GetStrPool();

        typedef CTreeNode<CBDB_QueryNode>  TTree;
        TTree::TNodeList_CI it = tr.SubNodeBegin();
        TTree::TNodeList_CI it_end = tr.SubNodeEnd();


        for (unsigned i = 0; it != it_end; ++it, ++i) {
            const TTree* t = *it;
            const CBDB_QueryNode& qnode = t->GetValue();

            ResizeVectors(i + 1);

            // Check if the argument should be taken from the
            // db field
            if (qnode.GetType() == CBDB_QueryNode::eDBField) {

                int fidx = qnode.GetFiledIdx();
                const CBDB_Field& fld = db_file.GetField(fidx);
                
                SyncArgValue(i, str_pool);                                
                string& pool_str = *m_ArgValueVector[i];
                
                if (fld.IsNull()) {
                    pool_str = kEmptyStr;
                } else {
                    fld.ToString(pool_str);
                }

                m_FieldVector[i] = &fld;

                continue;
            }

            // field value check mode
            // eCheckAll is coming from logical functions if they
            // support "value AND anothervalue" notation
            // (search services work like that)
            if ((check_mode == eCheckAll) && 
                (qnode.GetType() == CBDB_QueryNode::eValue)) {
                const string& v = qnode.GetValue();
                const char* sz = "0";
                if (IsAnyField(db_file, v, i)) {
                    sz = "1";
                }

                SyncArgValue(i, str_pool);

                *m_ArgValueVector[i] = sz;
                continue;
            }

            // Get constant value or return type of the subnodes

            const string& str = qnode.GetValue();
            m_ArgVector[i] = &str;

        } // for 
    }

    const string* GetArg(unsigned int idx) const
    {
        _ASSERT(idx < m_ArgVector.size());
        return m_ArgVector[idx];
    }

    const CBDB_Field* GetArgField(unsigned int idx) const
    {
        _ASSERT(idx < m_FieldVector.size());
        return m_FieldVector[idx];
    }

    void SetResult(CBDB_QueryNode& qnode, bool res)
    {
        if (res) {
            qnode.SetValue("1");
        } else {
            qnode.SetValue("0");
        }
    }

private:

    /// Syncronously resize all arguments vectors
    void ResizeVectors(unsigned int new_size)
    {
        TArgVector::size_type old_size = m_ArgVector.size();
        _ASSERT(new_size <= old_size + 1);  // 1 increments only
        if (new_size > old_size) {
            m_ArgVector.push_back(0);
            m_ArgValueVector.push_back(0);
            m_MatcherVector.push_back(0);
            m_FieldVector.push_back(0);
        }
    }

    /// m_ArgVector[idx] = m_ArgValueVector[idx] 
    void SyncArgValue(unsigned int idx, CResourcePool<string>& str_pool)
    {
        if (m_ArgValueVector[idx] == 0) {
            m_ArgValueVector[idx] = str_pool.Get();
        }

        m_ArgVector[idx] = m_ArgValueVector[idx];
    }

protected:
    TArgVector            m_ArgVector;
    TArgValueVector       m_ArgValueVector;
    TStringMatcherVector  m_MatcherVector;
    TFieldVector          m_FieldVector;
    string                m_TmpStr;
};


CScannerFunctorArgN::~CScannerFunctorArgN()
{
    CResourcePool<string>& str_pool = m_QueryEnv.GetStrPool();

    unsigned int size = m_ArgValueVector.size();
    for (unsigned int i = 0; i < size; ++i) {
        string* str = m_ArgValueVector[i];
        if (str) {
            str_pool.Return(str);
        }
        CBoyerMooreMatcher* matcher = m_MatcherVector[i];
        delete matcher;
    }
}


/// EQ function 
///
/// @internal
class CScannerFunctorEQ : public CScannerFunctorArgN
{
public:
    CScannerFunctorEQ(CQueryExecEnv& env, bool not_flag)
     : CScannerFunctorArgN(env),
       m_NotFlag(not_flag)
    {}

    bool IsNot() const { return m_NotFlag; }

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr);

        CBDB_QueryNode& qnode = tr.GetValue();

        const string* arg0 = GetArg(0);
        const string* arg1 = GetArg(1);

        const CBDB_Field* fld0 = GetArgField(0);
        const CBDB_Field* fld1 = GetArgField(1);

        // here we check two cases: 
        //   field = "value"
        //   "value" == field

        if (fld0 != 0 && fld1 == 0) {
            CBoyerMooreMatcher* matcher = GetMatcher(*arg1, 0);
            int pos = matcher->Search(*arg0);
            if (pos == -1) { // not found
                qnode.SetValue("0");
            } else {
                qnode.SetValue("1");
            }
            return;
        }

        if (fld0 == 0 && fld1 != 0) {
            CBoyerMooreMatcher* matcher = GetMatcher(*arg0, 0);
            int pos = matcher->Search(*arg1);
            if (pos == -1) { // not found
                qnode.SetValue("0");
            } else {
                qnode.SetValue("1");
            }
            return;
        }

        // Plain equal

        bool res = (*arg0 == *arg1);
        if (IsNot())
            res = !res;
        SetResult(qnode, res);
    }
protected:
    bool   m_NotFlag;
};

/// Basic class for all comparison (GT, LT, etc) functors
///
/// @internal
class CScannerFunctorComp : public CScannerFunctorArgN
{
public:
    CScannerFunctorComp(CQueryExecEnv& env)
     : CScannerFunctorArgN(env)
    {}

    int CmpEval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr);

        const string* arg0 = GetArg(0);
        const string* arg1 = GetArg(1);

        int cmpres = arg0->compare(*arg1);
        return cmpres;
    }
};


/// GT function 
///
/// @internal
class CScannerFunctorGT : public CScannerFunctorComp
{
public:
    CScannerFunctorGT(CQueryExecEnv& env)
     : CScannerFunctorComp(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        int cmpres = CmpEval(tr);

        bool res = cmpres > 0;
        SetResult(tr.GetValue(), res);
    }
};

/// GE function 
///
/// @internal
class CScannerFunctorGE : public CScannerFunctorComp
{
public:
    CScannerFunctorGE(CQueryExecEnv& env)
     : CScannerFunctorComp(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        int cmpres = CmpEval(tr);

        bool res = cmpres >= 0;
        SetResult(tr.GetValue(), res);
    }
};

/// LT function 
///
/// @internal
class CScannerFunctorLT : public CScannerFunctorComp
{
public:
    CScannerFunctorLT(CQueryExecEnv& env)
     : CScannerFunctorComp(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        int cmpres = CmpEval(tr);

        bool res = cmpres < 0;
        SetResult(tr.GetValue(), res);
    }
};


/// LE function 
///
/// @internal
class CScannerFunctorLE : public CScannerFunctorComp
{
public:
    CScannerFunctorLE(CQueryExecEnv& env)
     : CScannerFunctorComp(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        int cmpres = CmpEval(tr);

        bool res = cmpres <= 0;
        SetResult(tr.GetValue(), res);
    }
};


/// AND function 
///
/// @internal
class CScannerFunctorAND : public CScannerFunctorArgN
{
public:
    CScannerFunctorAND(CQueryExecEnv& env)
     : CScannerFunctorArgN(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr, eCheckAll);

        CBDB_QueryNode& qnode = tr.GetValue();

        unsigned int size = m_ArgVector.size();
        _ASSERT(size);

        for (unsigned int i = 0; i < size; ++i) {
            const string* arg = GetArg(i);
            if (*arg == "0") {
                qnode.SetValue("0");
                return;
            }
        }
        qnode.SetValue("1");
    }
};



/// OR function 
///
/// @internal
class CScannerFunctorOR : public CScannerFunctorArgN
{
public:
    CScannerFunctorOR(CQueryExecEnv& env)
     : CScannerFunctorArgN(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr, eCheckAll);

        CBDB_QueryNode& qnode = tr.GetValue();

        unsigned int size = m_ArgVector.size();
        _ASSERT(size);

        for (unsigned int i = 0; i < size; ++i) {
            const string* arg = GetArg(i);
            if (*arg == "1") {
                qnode.SetValue("1");
                return;
            }
        }

        qnode.SetValue("0");
    }
};


/// NOT function 
///
/// @internal
class CScannerFunctorNOT : public CScannerFunctorArgN
{
public:
    CScannerFunctorNOT(CQueryExecEnv& env)
     : CScannerFunctorArgN(env)
    {}

    void Eval(CTreeNode<CBDB_QueryNode>& tr)
    {
        GetArguments(tr, eCheckAll);

        CBDB_QueryNode& qnode = tr.GetValue();

        unsigned int size = m_ArgVector.size();
        _ASSERT(size);

        const string* arg = GetArg(0);
        if (*arg == "0") {
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
        query.ResetQueryClause();
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
        query.ResetQueryClause();
    } // while
}

/// The main tree evaluation functor
///
/// @internal

class CScannerEvaluateFunc
{
public: 
    CScannerEvaluateFunc(CQueryExecEnv& env)
    : m_QueryEnv(env),
      m_Matcher(0)
    {
    }

    ~CScannerEvaluateFunc()
    {
        delete m_Matcher;
    }

    CScannerEvaluateFunc(const CScannerEvaluateFunc& func)
    : m_QueryEnv(func.m_QueryEnv),
      m_Matcher(0)
    {
    }
  
    ETreeTraverseCode 
    operator()(CTreeNode<CBDB_QueryNode>& tr, int delta);

protected:
    CQueryExecEnv&       m_QueryEnv;
    CBoyerMooreMatcher*  m_Matcher;

};


ETreeTraverseCode 
CScannerEvaluateFunc::operator()(CTreeNode<CBDB_QueryNode>& tr, int delta)
{
    CBDB_QueryNode& qnode = tr.GetValue();

    // cout << delta << " " << tr.GetValue().GetValue() << endl;

    if (delta == 0 || delta == 1) {
        // If node has children, we skip it and process on the way back
        if (!tr.IsLeaf())
            return eTreeTraverse;
    }

    if (qnode.GetType() == CBDB_QueryNode::eValue) {
        if (tr.GetParent() == 0) { // single top node
            CBDB_File& dbf = m_QueryEnv.GetFile();
                
            if (!m_Matcher) {
                const string& search_value = qnode.GetValue();
                m_Matcher = s_MakeNewMatcher(search_value);
            }
            CBDB_File::TUnifiedFieldIndex fidx;
            fidx = BDB_find_field(dbf, *m_Matcher);

            qnode.SetAltValue(fidx ? "1" : "0");
        }
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
                CScannerFunctorEQ func(m_QueryEnv, qnode.IsNot());
                func.Eval(tr);
            }
            break;
            case CBDB_QueryNode::eGT:
            {
                CScannerFunctorGT func(m_QueryEnv);
                func.Eval(tr);
            }
            break;
            case CBDB_QueryNode::eGE:
            {
                CScannerFunctorGE func(m_QueryEnv);
                func.Eval(tr);
            }
            break;
            case CBDB_QueryNode::eLT:
            {
                CScannerFunctorLT func(m_QueryEnv);
                func.Eval(tr);
            }
            break;
            case CBDB_QueryNode::eLE:
            {
                CScannerFunctorLE func(m_QueryEnv);
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
            case CBDB_QueryNode::eNot:
            {
                CScannerFunctorNOT func(m_QueryEnv);
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


bool CBDB_FileScanner::Evaluate(CBDB_Query& query)
{
    ResolveFields(query);

    CBDB_Query::TQueryClause& qtree = query.GetQueryClause();

    CQueryExecEnv query_env(m_File);
    CScannerEvaluateFunc scanner_eval(query_env);

    TreeDepthFirstTraverse(qtree, scanner_eval);

    const CBDB_QueryNode& qnode = qtree.GetValue();
    const string& v_alt = qnode.GetAltValue();

    if (v_alt.empty()) {
        const string& v = qnode.GetValue();

        if (v == "0")
            return false;
        return true;
    } else {
        if (v_alt == "0")
            return false;
    }
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
                    if (qnode.IsNot()) {
                        m_OStream << "NOT EQ";
                    } else {
                        m_OStream << "EQ";
                    }
                    break;
                case CBDB_QueryNode::eGT:
                    m_OStream << "GT";
                    break;
                case CBDB_QueryNode::eGE:
                    m_OStream << "GE";
                    break;
                case CBDB_QueryNode::eLT:
                    m_OStream << "LT";
                    break;
                case CBDB_QueryNode::eLE:
                    m_OStream << "LE";
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
                case CBDB_QueryNode::eNot:
                    m_OStream << "NOT";
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
 * Revision 1.14  2004/06/28 12:16:49  kuznets
 * Fixed bug incorrect search in NULL fields
 *
 * Revision 1.13  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/03/23 16:58:18  kuznets
 * Fixed compilation warning (GCC)
 *
 * Revision 1.11  2004/03/23 16:37:54  kuznets
 * Implemented NOT predicate
 *
 * Revision 1.10  2004/03/23 14:51:19  kuznets
 * Implemented logical NOT, <, <=, >, >=
 *
 * Revision 1.9  2004/03/17 16:35:42  kuznets
 * EQ functor improved to enable substring searches
 *
 * Revision 1.8  2004/03/11 22:27:49  ucko
 * Pull the bodies of CScannerFunctorArgN::~CScannerFunctorArgN and
 * CScannerEvaluateFunc::operator() out of line so that they can call
 * s_MakeNewMatcher.  (WorkShop prohibits inline or template functions
 * from calling [file-]static functions.)
 *
 * Revision 1.7  2004/03/11 18:42:01  kuznets
 * code cleaned up, minor bug fix
 *
 * Revision 1.6  2004/03/11 13:16:10  kuznets
 * Bug fixed corner case when query is one word only
 *
 * Revision 1.5  2004/03/08 13:35:07  kuznets
 * Modified queries to do full text searches
 *
 * Revision 1.4  2004/03/01 14:03:57  kuznets
 * CQueryTreeFieldResolveFunc improved to remove string marks
 *
 * Revision 1.3  2004/02/24 14:12:45  kuznets
 * CBDB_Query add new constructor parameter and several helper functions
 *
 * Revision 1.2  2004/02/19 17:35:57  kuznets
 * + BDB_PrintQueryTree (tree printing utility function for debugging)
 *
 * Revision 1.1  2004/02/17 17:26:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
