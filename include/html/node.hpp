#ifndef NODE__HPP
#define NODE__HPP

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
*   standard node class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  1998/12/28 20:29:13  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.6  1998/12/23 21:20:58  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.5  1998/12/23 14:28:08  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.4  1998/12/21 22:24:57  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/11/23 23:47:50  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:15:52  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:34:31  lewisg
* html library includes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>
#include <list>

BEGIN_NCBI_SCOPE

// base class for a graph node

class CNCBINode
{
public:    
    typedef list<CNCBINode*> TChildList;
    typedef map<string, string> TAttributes;

    // 'structors
    CNCBINode(void);
    CNCBINode(const string& name);
    virtual ~CNCBINode();

    // removes a child
    void RemoveChild(CNCBINode* child);
    // adds a child to the child list at iterator.
    // returns this for chained AppendChild
    CNCBINode* InsertBefore(CNCBINode* newChild, CNCBINode* refChild);
    // add a Node * to the end of m_ChildNodes
    // returns this for chained AppendChild
    CNCBINode* AppendChild(CNCBINode* child);

    // helpers
    TChildList::iterator ChildBegin(void);
    TChildList::iterator ChildEnd(void);
    TChildList::const_iterator ChildBegin(void) const;
    TChildList::const_iterator ChildEnd(void) const;

    // finds iterator pointing to child
    TChildList::iterator FindChild(CNCBINode* child);

    virtual CNcbiOstream& Print(CNcbiOstream& out);
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out);
    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out);

    // this method will be called when printing and "this" node doesn't
    // contain any children
    virtual void CreateSubNodes(void);
    // finds and replaces text with a node    
    virtual CNCBINode* MapTag(const string& tagname);

    const string& GetName(void) const;
    void SetName(const string& namein);

    // retreive attribute
    bool HaveAttribute(const string& name) const;
    string GetAttribute(const string& name) const;

    // set attribute
    void SetAttribute(const string& name, const string& value);
    void SetAttribute(const string& name);
    void SetAttribute(const string& name, int value);
    void SetOptionalAttribute(const string& name, const string& value);
    void SetOptionalAttribute(const string& name, bool set);

    void SetAttribute(const char* name, const string& value);
    void SetAttribute(const char* name);
    void SetAttribute(const char* name, int value);
    void SetOptionalAttribute(const char* name, const string& value);
    void SetOptionalAttribute(const char* name, bool set);

    // clone whole node tree
    CNCBINode* Clone() const;

protected:
    TChildList m_ChildNodes;  // Child nodes.
    CNCBINode* m_ParentNode;  // Parent node (or null).

    bool m_Initialized;

    string m_Name; // node name

    TAttributes m_Attributes; // attributes, e.g. href="link.html"

    // Support for cloning
    void CloneChildrenTo(CNCBINode* copy) const;
    virtual CNCBINode* CloneSelf() const;
    CNCBINode(const CNCBINode& origin);

private:
    void DoAppendChild(CNCBINode* child);
    void DetachFromParent();
};

// inline functions are defined here:
#include <html/node.inl>

END_NCBI_SCOPE
#endif
