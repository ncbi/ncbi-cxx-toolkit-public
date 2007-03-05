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
*      Base class for objects that will hold structure data
*
* ===========================================================================
*/

#ifndef CN3D_STRUCTUREBASE__HPP
#define CN3D_STRUCTUREBASE__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidiag.hpp>

#include <list>


BEGIN_SCOPE(Cn3D)

class StructureSet;
class AtomSet;

class StructureBase
{
public:
    // will store StructureBase-derived children in parent upon construction
    StructureBase(StructureBase *parent);

    // will automatically delete children upon destruction with this destructor.
    // But note that derived classes can have their own constructors that will
    // *also* be called upon deletion; derived class destructors should *not*
    // delete any StructureBase-derived objects.
    virtual ~StructureBase(void);

    // overridable default Draws the object and all its children; 'data' will
    // be passed along to self and children's Draw methods
    virtual bool DrawAll(const AtomSet *atomSet = NULL) const;

    // function to draw this object, called before children are Drawn. Return
    // false to halt recursive drawing process. An AtomSet can be passed along
    // for cases (e.g. NMR structures) where a graph can be applied to multiple
    // sets of atom coordinates
    virtual bool Draw(const AtomSet *atomSet = NULL) const { return true; }

    // for convenience/efficiency accessing stuff attached to StructureSet
    StructureSet *parentSet;

private:
    // no default construction
    StructureBase(void);
    // keep track of StructureBase-derived children, so that top-down operations
    // like drawing or deconstructing can trickle down automatically
    typedef std::list < StructureBase * > _ChildList;
    _ChildList _children;
    void _AddChild(StructureBase *child);
    void _RemoveChild(const StructureBase *child);

    // also keep parent so we can move up the tree (see GetParentOfType())
    StructureBase* _parent;

public:
    // go up the hierarchy to find a parent of the desired type
    template < class T >
    bool GetParentOfType(const T* *ptr, bool warnIfNotFound = true) const
    {
        *ptr = NULL;
        for (const StructureBase *parent=this->_parent; parent; parent=parent->_parent) {
            if ((*ptr = dynamic_cast<const T*>(parent)) != NULL) {
                return true;
            }
        }
        if (warnIfNotFound)
            ERR_POST(ncbi::Warning << "can't get parent of requested type");
        return false;
    }
};


// for convenience when passing around atom pointers
class AtomPntr
{
public:
    static const int NO_ID;
    int mID, rID, aID;

    AtomPntr(int m = NO_ID, int r = NO_ID, int a = NO_ID) : mID(m), rID(r), aID(a) { }
    AtomPntr& operator = (const AtomPntr& a)
        { mID = a.mID; rID = a.rID; aID = a.aID; return *this; }
};


END_SCOPE(Cn3D)

#endif // CN3D_STRUCTUREBASE__HPP
