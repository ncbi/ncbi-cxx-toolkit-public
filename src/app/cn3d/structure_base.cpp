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
* Authors:  Paul Thiessen
*
* File Description:
*      Base class for structure data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.4  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.3  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/11 13:45:31  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.1  2000/07/01 15:47:18  thiessen
* major improvements to StructureBase functionality
*
* ===========================================================================
*/

#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;

BEGIN_SCOPE(Cn3D)

const int AtomPntr::NO_ID = -1;

// store children in parent upon construction
StructureBase::StructureBase(StructureBase *parent)
{
    if (parent) {
        parent->_AddChild(this);
        if (!(parentSet = parent->parentSet))
            ERR_POST(Error << "NULL parent->parentSet");
    } else {
        parentSet = NULL;
    }
    _parent = parent;
}

// delete children upon destruction
StructureBase::~StructureBase(void)
{
    _ChildList::iterator i, e=_children.end();
    for (i=_children.begin(); i!=e; i++) 
        delete i->first;
}

// draws the object and all its children - halt upon false return from Draw or DrawAll
bool StructureBase::DrawAll(const AtomSet *atomSet) const
{
    if (!Draw(atomSet)) return true;
    _ChildList::const_iterator i, e=_children.end();
    for (i=_children.begin(); i!=e; i++) 
        if (!((i->first)->DrawAll(atomSet))) return true;
	return true;
}

// every StructureBase object stored will get a unique ID (although it's
// not really useful or accessible at the moment... just need something to map to)
static unsigned int id = 1;

void StructureBase::_AddChild(StructureBase *child)
{
    _ChildList::const_iterator i = _children.find(child);
    if (i == _children.end())
        _children[child] = id++;
    else
        ERR_POST(Warning << "attempted to add child more than once");
}

void StructureBase::_RemoveChild(StructureBase *child)
{
    _ChildList::iterator i = _children.find(child);
    if (i != _children.end())
        _children.erase(i);
}

END_SCOPE(Cn3D)

