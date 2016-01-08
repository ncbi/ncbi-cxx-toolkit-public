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
    
    void PrintElement(const string& s, const CQueryParseNode& qnode)
    {
        int i;
        for (i = 0; i < m_Level; ++i) {
            m_OStream << "  ";
        }
        m_OStream << s;
        i += (int)s.length();
        for( ;i < 40; i++) {
            m_OStream << " ";
        }
        if (qnode.IsNot()) {
            m_OStream << " !";
            ++i;
        }
        m_OStream << " [" << qnode.GetOrig() << "]";
        i += 2 + (int)qnode.GetOrig().length();
        if (!qnode.IsExplicit()) {
            m_OStream << " implicit";
            i += 9;
        }
        for( ;i < 55; i++) {
            m_OStream << " ";
        }
        const CQueryParseNode::SSrcLoc& sl = qnode.GetLoc();
        m_OStream << " Line:" << sl.line << " pos=" << sl.pos;
     
        const IQueryParseUserObject* uo = qnode.GetUserObject();
        if (uo) {
            string v = uo->GetVisibleValue();
            if (!v.empty()) {
                m_OStream << "  UValue=" << uo->GetVisibleValue();
            }
        }
    }
      
    ETreeTraverseCode 
    operator()(const CTreeNode<CQueryParseNode>& tr, int delta) 
    {
        const CQueryParseNode& qnode = tr.GetValue();

        m_Level += delta;

        if (delta < 0)
            return eTreeTraverse;


        switch (qnode.GetType()) {
        case CQueryParseNode::eIdentifier:
            PrintElement(qnode.GetStrValue(), qnode);
            break;
        case CQueryParseNode::eString:
            PrintElement('"' + qnode.GetStrValue() + '"', qnode);
            break;
        case CQueryParseNode::eIntConst:
            PrintElement(NStr::IntToString(qnode.GetInt()) + "L", qnode);
            break;
        case CQueryParseNode::eFloatConst:
            PrintElement(NStr::DoubleToString(qnode.GetDouble()) + "F", qnode);
            break;
        case CQueryParseNode::eBoolConst:
            PrintElement(NStr::BoolToString(qnode.GetBool()), qnode);
            break;            
        case CQueryParseNode::eFunction:
            PrintElement(qnode.GetStrValue() + "()", qnode);
            break;
        case CQueryParseNode::eNot:
            PrintElement("NOT", qnode);
            break;
        case CQueryParseNode::eIn:
            PrintElement("IN", qnode);
            break;
        case CQueryParseNode::eFieldSearch:
            PrintElement("SEARCH", qnode);        
            break;
        case CQueryParseNode::eBetween:
            PrintElement("BETWEEN", qnode);
            break;
        case CQueryParseNode::eLike:
            PrintElement("LIKE", qnode);
            break;
        case CQueryParseNode::eAnd:
            PrintElement("AND", qnode);        
            break;
        case CQueryParseNode::eOr:
            PrintElement("OR", qnode);
            break;
        case CQueryParseNode::eSub:
            PrintElement("MINUS", qnode);
            break;
        case CQueryParseNode::eXor:
            PrintElement("XOR", qnode);
            break;
        case CQueryParseNode::eRange:
            PrintElement("RANGE", qnode);
            break;
        case CQueryParseNode::eSelect:
            PrintElement("SELECT", qnode);
            break;
        case CQueryParseNode::eFrom:
            PrintElement("FROM", qnode);
            break;            
        case CQueryParseNode::eWhere:
            PrintElement("WHERE", qnode);
            break;
        case CQueryParseNode::eList:
            PrintElement("LIST", qnode);
            break;            
        case CQueryParseNode::eEQ:
            if (qnode.IsNot()) {
                PrintElement("!=", qnode);
            } else {
                PrintElement("==", qnode);
            }
            break;
        case CQueryParseNode::eGT:
            PrintElement(">", qnode);
            break;
        case CQueryParseNode::eGE:
            PrintElement(">=", qnode);
            break;
        case CQueryParseNode::eLT:
            PrintElement("<", qnode);
            break;
        case CQueryParseNode::eLE:
            PrintElement("<=", qnode);
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
