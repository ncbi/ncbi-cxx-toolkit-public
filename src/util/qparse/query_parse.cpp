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
 * Authors: Anatoliy Kuznetsov, Mike DiCuccio, Maxim Didenko
 *
 * File Description: Query parser implementation
 *
 */

#include <ncbi_pch.hpp>
#include <util/qparse/query_parse.hpp>


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////


CQueryParseNode::CQueryParseNode()
: m_Type(eNotSet), 
  m_Explicit(false),
  m_Not(false),
  m_Elapsed(0.0)  
{
}

CQueryParseNode::CQueryParseNode(const string& value, 
                                 const string& orig_text, 
                                 bool isIdent)
: m_Type(isIdent ? eIdentifier : eString),
  m_Value(value),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false),  
  m_Elapsed(0.0)  
{
}

CQueryParseNode::CQueryParseNode(Int4   val, const string& orig_text)
: m_Type(eIntConst),
  m_IntConst(val),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false),  
  m_Elapsed(0.0)  
{
}

CQueryParseNode::CQueryParseNode(bool   val, const string& orig_text)
: m_Type(eBoolConst),
  m_BoolConst(val),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false),
  m_Elapsed(0.0)    
{
}

CQueryParseNode::CQueryParseNode(double val, const string& orig_text)
: m_Type(eFloatConst),
  m_DoubleConst(val),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)    
{
}

CQueryParseNode::CQueryParseNode(EType op, const string& orig_text)
: m_Type(op),
  m_Value(orig_text),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false),
  m_Elapsed(0.0)    
{
}

const string& CQueryParseNode::GetStrValue() const
{
    if (m_Type == eIdentifier || m_Type == eString || m_Type == eFunction) {
        return m_Value;
    } else if (m_Type == eIntConst || m_Type == eFloatConst || m_Type == eList) {
        return m_OrigText;
    }
    NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
               "Incorrect query node type");
}

Int4 CQueryParseNode::GetInt() const
{
    if (m_Type != eIntConst) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_IntConst;
}

bool CQueryParseNode::GetBool() const
{
    if (m_Type != eBoolConst) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_BoolConst;
}

double CQueryParseNode::GetDouble() const
{
    if (m_Type != eFloatConst) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_DoubleConst;
}

const string& CQueryParseNode::GetIdent() const
{
    if (m_Type != eIdentifier || m_Type != eFunction) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_Value;
}


void CQueryParseNode::AttachUserObject(IQueryParseUserObject* obj)
{
    m_UsrObj.Reset(obj);
}

void CQueryParseNode::ResetUserObject()
{
    IQueryParseUserObject* p = m_UsrObj.GetPointer();
    if (p) {
        p->Reset();
    }
}


//////////////////////////////////////////////////////////////////////////////

CQueryParseTree::CQueryParseTree(TNode *clause)
: m_Tree(clause)
{
}

CQueryParseTree::~CQueryParseTree()
{
}

void CQueryParseTree::SetQueryTree(TNode* qtree)
{
    m_Tree.reset(qtree);
}


CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(const string&  value, 
                            const string&  orig_text, 
                            bool           isIdent)
{
    CQueryParseTree::TNode* tn = 
        new TNode(CQueryParseNode(value, orig_text, isIdent));
//cerr << "CreateNode(string): " << tn << " " << value << endl;
    return tn;
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(Int4   value, const string&  orig_text)
{
    CQueryParseTree::TNode* tn = 
        new TNode(CQueryParseNode(value, orig_text));
//cerr << "CreateNode(int): " << tn << " " << value<< endl;
    return tn;
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(bool   value, const string&  orig_text)
{
    return new TNode(CQueryParseNode(value, orig_text));
}


CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(double   value, const string&  orig_text)
{
    return new TNode(CQueryParseNode(value, orig_text));
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(CQueryParseNode::EType     op,
                            CQueryParseTree::TNode*    arg1, 
                            CQueryParseTree::TNode*    arg2,
                            const string&              orig_text)
{
   auto_ptr<TNode> node(new TNode(CQueryParseNode(op, orig_text)));
   if (arg1) {
        node->AddNode(arg1);
   }
   if (arg2) {
       node->AddNode(arg2);
   }
//cerr << "CreateNode(Bin-op): " << node.get() << endl;
   return node.release();
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateFuncNode(const string&  func_name)
{
    return new TNode(CQueryParseNode(CQueryParseNode::eFunction, func_name));
}


/// Reset query node
///
/// @internal
///
class CQueryTreeResetFunc
{
public:
    CQueryTreeResetFunc(){}
    
    ETreeTraverseCode 
    operator()(CTreeNode<CQueryParseNode>& tr, int delta) 
    {
        CQueryParseNode& qnode = tr.GetValue();
        if (delta < 0)
            return eTreeTraverse;
        qnode.ResetUserObject();

        return eTreeTraverse;
    }
};



void CQueryParseTree::ResetUserObjects()
{
    CQueryParseTree::TNode* qtree = this->GetQueryTree();
    if (!qtree) {
        return;
    }
    TNode& qtree_nc = const_cast<TNode&>(*qtree);
    CQueryTreeResetFunc func;
    TreeDepthFirstTraverse(qtree_nc, func);
}


//////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

