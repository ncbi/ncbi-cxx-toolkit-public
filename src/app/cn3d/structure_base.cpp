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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "remove_header_conflicts.hpp"

#include "structure_base.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

const int AtomPntr::NO_ID = -1;

// store children in parent upon construction
StructureBase::StructureBase(StructureBase *parent)
{
    if (parent) {
        parent->_AddChild(this);
        if (!(parentSet = parent->parentSet))
            ERRORMSG("NULL parent->parentSet");
    } else {
        parentSet = NULL;
    }
    _parent = parent;
}

// delete children upon destruction
StructureBase::~StructureBase(void)
{
    if (_parent) {
        _parent->_RemoveChild(this);
        _parent = NULL;
    }

    while (_children.size() > 0)
        delete _children.front();
}

void StructureBase::_AddChild(StructureBase *child)
{
    _children.push_back(child);
}

void StructureBase::_RemoveChild(const StructureBase *child)
{
    _ChildList::iterator i, ie=_children.end();
    for (i=_children.begin(); i!=ie; ++i) {
        if (*i == child) {
            _children.erase(i);
            return;
        }
    }
    WARNINGMSG("attempted to remove non-existent child");
}

// draws the object and all its children - halt upon false return from Draw or DrawAll
bool StructureBase::DrawAll(const AtomSet *atomSet) const
{
    if (!parentSet->renderer) return false;
    if (!Draw(atomSet)) return true;
    _ChildList::const_iterator i, e=_children.end();
    for (i=_children.begin(); i!=e; ++i)
        if (!((*i)->DrawAll(atomSet))) return true;
    return true;
}

END_SCOPE(Cn3D)
