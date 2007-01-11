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


CQueryParseNode::CQueryParseNode()
: m_Type(eNotSet), 
  m_Explicit(false),
  m_Not(false)  
{
}

CQueryParseNode::CQueryParseNode(const string& value, 
                                 const string& orig_text, 
                                 bool isIdent)
: m_Type(isIdent ? eIdentifier : eString),
  m_Value(value),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)  
{
}

CQueryParseNode::CQueryParseNode(Int4   val, const string& orig_text)
: m_Type(eIntConst),
  m_IntConst(val),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)  
{
}

CQueryParseNode::CQueryParseNode(bool   val, const string& orig_text)
: m_Type(eBoolConst),
  m_BoolConst(val),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)  
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

CQueryParseNode::CQueryParseNode(EBinaryOp op, const string& orig_text)
: m_Type(eBinaryOp),
  m_BinaryOp(op),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)  
{
}

CQueryParseNode::CQueryParseNode(EUnaryOp op, const string& orig_text)
: m_Type(eUnaryOp),
  m_UnaryOp(op),
  m_OrigText(orig_text),
  m_Explicit(true),
  m_Not(false)  
{
}


CQueryParseNode::EBinaryOp CQueryParseNode::GetBinaryOp() const
{
    if (m_Type != eBinaryOp) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_BinaryOp;
}

CQueryParseNode::EUnaryOp CQueryParseNode::GetUnaryOp() const
{
    if (m_Type != eUnaryOp) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_UnaryOp;
}

const string& CQueryParseNode::GetStrValue() const
{
    if (m_Type != eIdentifier && m_Type != eString) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_Value;
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
    if (m_Type != eIdentifier) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_Value;
}

void CQueryParseNode::SetIdentIdx(int idx)
{
    if (m_Type != eIdentifier) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    m_IdentIdx = idx;
}

int CQueryParseNode::GetIdentIdx() const
{
    if (m_Type != eIdentifier) {
        NCBI_THROW(CQueryParseException, eIncorrectNodeType, 
                   "Incorrect query node type");
    }
    return m_IdentIdx;
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
    return new TNode(CQueryParseNode(value, orig_text, isIdent));
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateNode(Int4   value, const string&  orig_text)
{
    return new TNode(CQueryParseNode(value, orig_text));
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
CQueryParseTree::CreateBinaryNode(CQueryParseNode::EBinaryOp op,
                                  CQueryParseTree::TNode*    arg1, 
                                  CQueryParseTree::TNode*    arg2,
                                  const string&              orig_text)
{
    _ASSERT(arg1 && arg2);
   auto_ptr<TNode> node(new TNode(CQueryParseNode(op, orig_text)));
   if (arg1) {
        node->AddNode(arg1);
   }
   if (arg2) {
       node->AddNode(arg2);
   }
   return node.release();
}

CQueryParseTree::TNode* 
CQueryParseTree::CreateUnaryNode(CQueryParseNode::EUnaryOp op, 
                                 CQueryParseTree::TNode*   arg,
                                 const string&             orig_text)
{
    _ASSERT(arg);
   auto_ptr<TNode> node(new TNode(CQueryParseNode(op, orig_text)));
   if (arg) {
        node->AddNode(arg);
   }
   return node.release();
}

//////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2007/01/11 14:49:51  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.2  2007/01/11 01:04:17  ucko
 * Give CQueryParseNode a private default constructor, as a formality for
 * CTreeNode<CQueryParseNode> (granting the latter friend-level access).
 * Rename "identificator" to the proper word "identifier".
 * Indent with spaces, not tabs.
 *
 * Revision 1.1  2007/01/10 16:14:01  kuznets
 * initial revision
 *
 * ===========================================================================
 */
