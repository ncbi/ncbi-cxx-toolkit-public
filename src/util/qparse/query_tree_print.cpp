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
 *      Query tree debug print implementation. 
 *
 */

#include <ncbi_pch.hpp>
#include <util/qparse/query_parse.hpp>

BEGIN_NCBI_SCOPE


/// The main tree printing functor class.
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
    
    void PrintUnary(const CQueryParseNode& qnode)
    {
        CQueryParseNode::EUnaryOp op = qnode.GetUnaryOp();
        switch (op) {
        case CQueryParseNode::eNot:
            m_OStream << "NOT";
            break;
        default:
            m_OStream << "UNK";
        }
    }
    void PrintBinary(const CQueryParseNode& qnode)
    {
        CQueryParseNode::EBinaryOp op = qnode.GetBinaryOp();
        switch (op) {
        case CQueryParseNode::eAnd:
            m_OStream << "AND";
            break;
        case CQueryParseNode::eOr:
            m_OStream << "OR";
            break;
        case CQueryParseNode::eSub:
            m_OStream << "MINUS";
            break;
        case CQueryParseNode::eNot2:
            m_OStream << "NOT";
            break;
        case CQueryParseNode::eXor:
            m_OStream << "XOR";
            break;
        case CQueryParseNode::eEQ:
            m_OStream << "==";
            break;
        case CQueryParseNode::eGT:
            m_OStream << ">";
            break;
        case CQueryParseNode::eGE:
            m_OStream << ">=";
            break;
        case CQueryParseNode::eLT:
            m_OStream << "<";
            break;
        case CQueryParseNode::eLE:
            m_OStream << "<=";
            break;
        default:
            m_OStream << "UNK";
            break;
        }
    }
  
    ETreeTraverseCode 
    operator()(const CTreeNode<CQueryParseNode>& tr, int delta) 
    {
        const CQueryParseNode& qnode = tr.GetValue();

        m_Level += delta;

        if (delta < 0)
            return eTreeTraverse;

        PrintLevelMargin();
        
        if (qnode.IsNot()) {
            m_OStream << "NOT ";        
        }

        switch (qnode.GetType()) {
        case CQueryParseNode::eIdentifier:
            m_OStream << qnode.GetStrValue();
            break;
        case CQueryParseNode::eString:
            m_OStream << '"' << qnode.GetStrValue() << '"';
            break;
        case CQueryParseNode::eIntConst:
            m_OStream << qnode.GetInt() << "L";
            break;
        case CQueryParseNode::eFloatConst:
            m_OStream << qnode.GetDouble() << "F";
            break;
        case CQueryParseNode::eBoolConst:
            m_OStream << qnode.GetBool();
            break;
        case CQueryParseNode::eUnaryOp:
            PrintUnary(qnode);
            break;
        case CQueryParseNode::eBinaryOp:
            PrintBinary(qnode);
            break;
        default:
            m_OStream << "UNK";
            break;
        } // switch

        m_OStream << "\n";

        return eTreeTraverse;
    }

private:
    CNcbiOstream&  m_OStream;
    int            m_Level;
};



void CQueryParseTree::Print(CNcbiOstream& os) const
{
    // Here I use a const cast hack because TreeDepthFirstTraverse
    // uses a non-cost iterators and semantics in the algorithm.
    // When a const version of TreeDepthFirstTraverse is ready
    // we can get rid of this...

    const CQueryParseTree::TNode* qtree = this->GetQueryTree();
    if (!qtree) {
        return;
    }
    TNode& qtree_nc = const_cast<TNode&>(*qtree);

    CQueryTreePrintFunc func(os);
    TreeDepthFirstTraverse(qtree_nc, func);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2007/01/11 14:50:23  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

