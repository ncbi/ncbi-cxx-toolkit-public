#ifndef NODE__HPP
#define NODE__HPP


/*  $RCSfile$  $Revision$  $Date$
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
* Revision 1.2  1998/10/29 16:15:52  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:34:31  lewisg
* html library includes
*
* ===========================================================================
*/



#include <stl.hpp>

// base class for a graph node

class CNCBINode
{
public:    
    list<CNCBINode *>::iterator ChildBegin() { return m_ChildNodes.begin(); }
    list<CNCBINode *>::iterator ChildEnd() { return m_ChildNodes.end(); }
    virtual CNCBINode * InsertBefore(CNCBINode * newChild, CNCBINode * refChild);  // adds a child to the child list at iterator.
    virtual CNCBINode * AppendChild(CNCBINode *);  // add a Node * to the end of m_ChildNodes
    CNCBINode() { m_ParentNode = NULL; }
    virtual ~CNCBINode();
    // need to include explicit copy and assignment op.  I don't think the child list should be copied, nor parent.

protected:
    list<CNCBINode *> m_ChildNodes;  // Child nodes.
    CNCBINode * m_ParentNode;
    list<CNCBINode *>::iterator m_SelfIter;  // points to self in *parent's* m_ChildNodes list.
};

#endif
