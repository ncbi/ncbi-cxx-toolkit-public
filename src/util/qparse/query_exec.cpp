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
 * File Description:
 *      Query execution implementations
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <util/qparse/query_exec.hpp>

BEGIN_NCBI_SCOPE

CQueryFunctionBase::~CQueryFunctionBase()
{
}

void CQueryFunctionBase::MakeArgVector(CQueryParseTree::TNode& qnode, 
                                       TArgVector&             args)
{
    args.resize(0);
    CTreeNode<CQueryParseNode>::TNodeList_I it = qnode.SubNodeBegin();
    CTreeNode<CQueryParseNode>::TNodeList_I it_end = qnode.SubNodeEnd();
    int i = 0;       
    for (; it != it_end; ++it, ++i) {
        CTreeNode<CQueryParseNode>* arg_node = *it;
        args.push_back(arg_node);
    } // for
}

CQueryParseTree::TNode* 
CQueryFunctionBase::GetArg0(CQueryParseTree::TNode& qnode)
{
    CTreeNode<CQueryParseNode>::TNodeList_I it = qnode.SubNodeBegin();
    CTreeNode<CQueryParseNode>::TNodeList_I it_end = qnode.SubNodeEnd();
    if (it == it_end)
        return 0;
    return *it;
}


CQueryParseTree& CQueryFunctionBase::GetQueryTree() 
{ 
    return *(GetExec().GetQTree()); 
}






CQueryExec::CQueryExec()
: m_FuncReg(CQueryParseNode::eMaxType),
  m_QTree(0),
  m_ExceptionCount(0),
  m_QueriedCount(0)
{
    for (size_t i = 0; i < m_FuncReg.size(); ++i) {
        m_FuncReg[i] = 0;
    }
}

CQueryExec::~CQueryExec()
{
    for (size_t i = 0; i < m_FuncReg.size(); ++i) {
        delete m_FuncReg[i];
    }
}

void CQueryExec::AddFunc(CQueryParseNode::EType func_type, 
                         CQueryFunctionBase*    func)
{
    int i = func_type;
    delete m_FuncReg[i];
    m_FuncReg[i] = func;
    func->SetExec(*this);
}

void CQueryExec::AddImplicitSearchFunc(CQueryFunctionBase* func)
{
    m_ImplicitSearchFunc.reset(func);
}





/// Expression evaluation visitor functor
///
/// @internal
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
        if (delta == 0 || delta == 1) {
            // If node has children, we skip it and process on the way back
            if (!tr.IsLeaf()) {
                return eTreeTraverse;
            }
        }
        CQueryParseNode::EType func_type = qnode.GetType();
        CQueryFunctionBase* func = 0;
        
        // check if execution env has implicit search configured 
        // and value node is derives from a logical node (AND, OR).
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
        double e = sw.Elapsed();
        qnode.SetElapsed(e);
        
        return eTreeTraverse;        
    }
private:
    CQueryExec& m_Exec;
};


void CQueryExec::Evaluate(CQueryParseTree& qtree)
{
    m_QTree = &qtree;
    CQueryExecEvalFunc visitor(*this);
    TreeDepthFirstTraverse(*qtree.GetQueryTree(), visitor);
    m_QTree = 0;
}


void CQueryExec::Evaluate(CQueryParseTree& qtree, CQueryParseTree::TNode& node)
{
    m_QTree = &qtree;
    CQueryExecEvalFunc visitor(*this);
    TreeDepthFirstTraverse(node, visitor);
    m_QTree = 0;
}


END_NCBI_SCOPE
