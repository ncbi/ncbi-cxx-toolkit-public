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
* Author:  Lewis Geer
*
* File Description:
*   code for CNCBINode
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/01/25 19:32:44  vasilche
* Virtual destructors now work properly.
*
* Revision 1.7  1999/01/04 20:06:14  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.6  1998/12/28 20:29:19  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/12/23 21:21:04  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.4  1998/12/21 22:25:04  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/11/23 23:42:29  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:13:06  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:36:05  lewisg
* new html lib and test program
*
* ===========================================================================
*/

#include <html/node.hpp>

BEGIN_NCBI_SCOPE

CNCBINode::CNCBINode(void)
    : m_ParentNode(0), m_Initialized(false)
{
}

CNCBINode::CNCBINode(const string& name)
    : m_ParentNode(0), m_Initialized(false), m_Name(name)
{
}

// we leak memory like crazy until the sunpro compiler is upgraded
// it can't handle a virtual destructor in any form for some reason.
CNCBINode::~CNCBINode(void)
{
    while ( !m_ChildNodes.empty() ) {
        delete *ChildBegin();
    }
    DetachFromParent();
}

CNCBINode::CNCBINode(const CNCBINode& origin)
    : m_ParentNode(0), m_Initialized(origin.m_Initialized),
      m_Name(origin.m_Name), m_Attributes(origin.m_Attributes)
{
}

CNCBINode* CNCBINode::Clone() const
{
    CNCBINode* copy = CloneSelf();

    CloneChildrenTo(copy);

    return copy;
}

void CNCBINode::CloneChildrenTo(CNCBINode* copy) const
{
    for (TChildList::const_iterator i = ChildBegin(); i != ChildEnd(); ++i) {
        CNCBINode* childOrigin = *i;
        CNCBINode* child = childOrigin->CloneSelf();
        copy->AppendChild(child);
        childOrigin->CloneChildrenTo(child);
    }
}

CNCBINode* CNCBINode::CloneSelf() const
{
    return new CNCBINode(*this);
}

void CNCBINode::DetachFromParent(void)
{
    if ( m_ParentNode ) {
        m_ParentNode->RemoveChild(this);
        _ASSERT(m_ParentNode == 0);
    }
}

void CNCBINode::RemoveChild(CNCBINode* child)
{
    if ( child->m_ParentNode != this )
        return;

    m_ChildNodes.erase(FindChild(child));
    child->m_ParentNode = 0;
}

CNCBINode::TChildList::iterator CNCBINode::FindChild(CNCBINode* child)
{
    for (TChildList::iterator i = ChildBegin(); i != ChildEnd(); ++i) {
        if (*i == child) {
            return i;
        }
    }
    throw runtime_error("Broken CNCBINode tree");
}

// append a child
void CNCBINode::DoAppendChild(CNCBINode * child)
{
    child->DetachFromParent();
    m_ChildNodes.push_back(child);
    child->m_ParentNode = this;
}


// insert a child before the given node
CNCBINode* CNCBINode::InsertBefore(CNCBINode * newChild, CNCBINode * refChild)
{
    if ( !newChild || !refChild || refChild->m_ParentNode != this )
        return this;

    newChild->DetachFromParent();

    m_ChildNodes.insert(++FindChild(refChild), newChild);
    newChild->m_ParentNode = this;

    return this;
}

bool CNCBINode::HaveAttribute(const string& name) const
{
    TAttributes& Attributes = const_cast<TAttributes&>(m_Attributes); // SW_01
    return Attributes.find(name) != Attributes.end();
}

string CNCBINode::GetAttribute(const string& name) const
{
    TAttributes& Attributes = const_cast<TAttributes&>(m_Attributes); // SW_01
    TAttributes::iterator ptr = Attributes.find(name);
    if ( ptr == Attributes.end() )
        return NcbiEmptyString;
    return ptr->second;
}

const string* CNCBINode::GetAttributeValue(const string& name) const
{
    TAttributes& Attributes = const_cast<TAttributes&>(m_Attributes); // SW_01
    TAttributes::iterator ptr = Attributes.find(name);
    if ( ptr == Attributes.end() )
        return 0;
    return &ptr->second;
}

void CNCBINode::SetAttribute(const string& name)
{
    m_Attributes[name];
}

void CNCBINode::SetAttribute(const string& name, const string& value)
{
    m_Attributes[name] = value;
}

void CNCBINode::SetAttribute(const char* name)
{
    m_Attributes[name];
}

void CNCBINode::SetAttribute(const char* name, const string& value)
{
    m_Attributes[name] = value;
}

// this function searches for a text string in a Text node and replaces it with a node
CNCBINode* CNCBINode::MapTag(const string& tagname)
{
    if ( !m_ParentNode )
        return 0;

    return m_ParentNode->MapTag(tagname);
}

// print the whole node tree (with possible initialization)
CNcbiOstream& CNCBINode::Print(CNcbiOstream& out)
{
    if ( !m_Initialized ) { // if there is no children
        CreateSubNodes();             // create them
        m_Initialized = true;
    }

    PrintBegin(out);
    PrintChildren(out);
    PrintEnd(out);
    return out;
}

CNcbiOstream& CNCBINode::PrintBegin(CNcbiOstream& out)
{
    return out;
}

CNcbiOstream& CNCBINode::PrintEnd(CNcbiOstream& out)
{
    return out;
}

// print all children
CNcbiOstream& CNCBINode::PrintChildren(CNcbiOstream& out)
{
    for (TChildList::iterator i = ChildBegin(); i != ChildEnd(); ++i) {
        (*i)->Print(out);
    }
    return out;
}

// by default, we don't create any subnode
void CNCBINode::CreateSubNodes(void)
{
}

END_NCBI_SCOPE



