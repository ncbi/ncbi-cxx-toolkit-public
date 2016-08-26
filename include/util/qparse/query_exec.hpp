#ifndef UTIL__QUERY_EXEC_HPP__
#define UTIL__QUERY_EXEC_HPP__

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
 * Authors: Anatoliy Kuznetsov
 *
 * File Description: Query execution components
 *
 */

/// @file query_exec.hpp
/// Query parser execution implementations


#include <util/qparse/query_parse.hpp>
#include <corelib/ncbitime.hpp>

#include <vector>

BEGIN_NCBI_SCOPE


/** @addtogroup QParser
 *
 * @{
 */

class CQueryExec;

/// Base class for evaluation functions
///
/// <pre>
/// Implementation guidelines for derived classes:
///    1. Make it stateless. 
///       All evaluation results should go to query node user object.
///    2. Make it reentrant
///    3. Make it thread safe and thread sync. (some functions may run in parallel)
///       (stateless class satisfies both thread safety and reenterability)
/// </pre>
///
/// @sa IQueryParseUserObject
///
class NCBI_XUTIL_EXPORT CQueryFunctionBase
{
public:
    /// Vector for easy argument access
    ///
    typedef vector<CQueryParseTree::TNode*> TArgVector;

public:
    virtual ~CQueryFunctionBase();

    /// Do we evaluate before visiting the nodes children or after. Visiting
    /// after can allow us to avoid evaluating some sub-expressions
    /// @return true if children should all be evaluated before parent for
    ///         this type of node, otherwise false
    virtual bool EvaluateChildrenFirst() const { return true; }
    
    /// Query node evaluation function 
    /// (performs actual programmed by the node action)
    ///
    virtual void Evaluate(CQueryParseTree::TNode& qnode) = 0;

protected:    
    /// @name Utility functions
    /// @{
    
    /// Created vector of arguments (translate sub-nodes to vector)
    ///
    /// @param qnode 
    ///     Query node (all sub-nodes are treated as parameters
    /// @param args
    ///     Output vector
    void MakeArgVector(CQueryParseTree::TNode& qnode, 
                       TArgVector&             args);

    /// Get first sub-node
    ///                       
    CQueryParseTree::TNode* GetArg0(CQueryParseTree::TNode& qnode);
    
    /// @}

    /// Get reference on parent execution environment
    CQueryExec& GetExec() { return *m_QExec; }
    
    /// Get query tree (execution context)
    CQueryParseTree& GetQueryTree();
    
private:    
    friend class CQueryExec;
    /// Set reference on parent query execution class
    void SetExec(CQueryExec& qexec) { m_QExec = &qexec; }
    
protected:
    CQueryExec*  m_QExec;
};


/// Query execution environment holds the function registry and the execution 
/// context. Execution context is vaguely defined term, it can be anything
/// query functions need: 
///   open files, databases, network connections, pools of threads, etc.
/// 
/// <pre>
///
/// Query execution consists of distinct several phases:
/// 1.Construction of execution environment
///    All function implementations are registered in CQueryExec
/// 2.Query parsing (see CQueryParseTree). This step transforms query (string)
///   into query tree, the tree reflects operations precedence
/// 3.Parsing tree validation and transformation (optional)
/// 4.Query execution-evaluation. Execution engine traverses the query tree calling
///   appropriate functions to do expression(query tree node) evaluation.
///   Execution results for every tree node are stored in the tree as user
///   objects (classes derived from IQueryParseUserObject).
///
/// </pre>
///
class NCBI_XUTIL_EXPORT CQueryExec
{
public:
    // Subclasses can optionally pass in a field identifier
    // to resolve values (faster for trees, tables..)
    typedef unsigned int TFieldID;

public:
    CQueryExec();
    virtual ~CQueryExec();
    
    /// Register function implementation. 
    /// (with ownership transfer)
    ///
    void AddFunc(CQueryParseNode::EType func_type, CQueryFunctionBase* func);
    
    /// This is a callback for implicit search nodes
    /// Our syntax allows queries like  (a=1) AND "search_term"
    /// Execution recognises "search_term" connected with logical operation 
    /// represents a special case. 
    /// This method defines a reactor (function implementation).
    ///
    void AddImplicitSearchFunc(CQueryFunctionBase* func);
    CQueryFunctionBase* GetImplicitSearchFunc() 
                    { return m_ImplicitSearchFunc.get(); }
    
    /// Return query function pointer (if registered).
    /// 
    CQueryFunctionBase* GetFunc(CQueryParseNode::EType func_type)
                                    { return m_FuncReg[(size_t)func_type]; }
    
    
    /// Run query tree evaluation
    ///
    virtual void Evaluate(CQueryParseTree& qtree);

    // Evaluate subtree of 'qtree' beginning from 'node'
    virtual void Evaluate(CQueryParseTree& qtree, CQueryParseTree::TNode& node);

    /// If query has an identifier, this will resolve it in an
    /// application-specific way. Returns true if resolved, with the
    /// data item's value in 'value'. Using field identifiers, if available,
    /// such as column numbers or biotree feature IDs will be faster.
    ///
    virtual bool ResolveIdentifier(const std::string& /* identifier */, 
                                   bool& /* value */) { return false; }
    virtual bool ResolveIdentifier(const std::string& /* identifier */, 
                                   int& /* value */) { return false; }
    virtual bool ResolveIdentifier(const std::string& /* identifier */, 
                                   double& /* value */) { return false; }
    virtual bool ResolveIdentifier(const std::string& /* identifier */, 
                                   std::string& /* value */) { return false; }

    virtual bool ResolveIdentifier(const TFieldID& /* id */,
                                    bool& /* value */) { return false; }
    virtual bool ResolveIdentifier(const TFieldID& /* id */,
                                   int& /* value */) { return false; }
    virtual bool ResolveIdentifier(const TFieldID& /* id */,
                                   double& /* value */) { return false; }
    virtual bool ResolveIdentifier(const TFieldID& /* id */,
                                   std::string& /* value */) { return false; }

    virtual bool HasIdentifier(const std::string& /* identifier */) { return false; }
    virtual TFieldID GetIdentifier(const std::string& /* identifier */) { return TFieldID(-1); }
    
    /// Some applications may know the type of an identifier.  This hook
    /// should be overriden to return an identifier's type, when available.
    /// Return one of eIntConst, eBoolConst, eFloatConst, eString, or eNotSet.
    virtual CQueryParseNode::EType IdentifierType(const std::string& /* identifier */)
        { return CQueryParseNode::eNotSet; }

    virtual void EvalStart() {}
    /// Move to the next row for eval, return false if table size < m_EvalRow+1
    virtual bool EvalNext(CQueryParseTree& /* qtree */) {return false;}
    virtual bool EvalComplete() {return true;}

    int GetQueriedCount() const { return  m_QueriedCount; }
    int GetExceptionCount() const { return m_ExceptionCount; }
    
protected:
    friend class CQueryFunctionBase;
    CQueryParseTree* GetQTree() { return m_QTree; }
    
private:
    CQueryExec(const CQueryExec&);
    CQueryExec& operator=(const CQueryExec&);
    
protected:
    typedef vector<CQueryFunctionBase*> TFuncReg;
protected:
    TFuncReg                       m_FuncReg;
    auto_ptr<CQueryFunctionBase>   m_ImplicitSearchFunc;
    CQueryParseTree*               m_QTree;

    int                            m_ExceptionCount;
    int                            m_QueriedCount;
};

/// Expression evaluation visitor functor
///
class CQueryExecEvalFunc
{
public:
    CQueryExecEvalFunc(CQueryExec& exec)
        : m_Exec(exec)
    {}

    ETreeTraverseCode
        operator()(CTreeNode<CQueryParseNode>& tr, int delta)
    {
        CQueryParseNode& qnode = tr.GetValue();
        CQueryParseNode::EType func_type = qnode.GetType();        

        if ((delta == 0 || delta == 1) && !tr.IsLeaf()) {
            CQueryFunctionBase* func = m_Exec.GetFunc(func_type);

            // if current query node requires child query nodes to be 
            // evaluated first, continue depth-first traversal
            if (func == NULL || func->EvaluateChildrenFirst()) {
                return eTreeTraverse;
            }
            // process query node before processing its children. Allows
            // node processing to stop early for effciency whenever possible
            // (specifically for AND and OR nodes)
            else {
                CStopWatch sw(CStopWatch::eStart);
                {{
                    func->Evaluate(tr);
                }}
                qnode.SetElapsed(sw.Elapsed());

                return eTreeTraverseStepOver;
            }
        }

        CQueryFunctionBase* func = 0;

        // check if execution env has implicit search configured 
        // and value node is child of a logical node (AND, OR).
        // in this case we fire implicit search
        //
        if (m_Exec.GetImplicitSearchFunc()) {
            if (qnode.IsValue()) {
                CTreeNode<CQueryParseNode>* parent = tr.GetParent();
                if (parent && parent->GetValue().IsLogic()) {
                    func = m_Exec.GetImplicitSearchFunc();
                }
            }
        }

        if (!func) {
            func = m_Exec.GetFunc(func_type);
        }
        if (func == 0) { // function not registered
            // values (string, int, etc) do not require evaluation
            if (qnode.IsValue()) {
                return eTreeTraverse;
            }
            NCBI_THROW(CQueryParseException, eUnknownFunction,
                "Query execution failed. Unknown function:" + qnode.GetOrig());
        }

        CStopWatch sw(CStopWatch::eStart);
        {{
            func->Evaluate(tr);
        }}
        qnode.SetElapsed(sw.Elapsed());

        return eTreeTraverse;
    }
private:
    CQueryExec& m_Exec;
};


/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_EXEC_HPP__
