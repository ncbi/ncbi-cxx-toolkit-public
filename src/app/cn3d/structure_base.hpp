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

    // also keep parent so we can move up the tree (see GetParentOfType())
    StructureBase* _parent;

protected:
    void _RemoveChild(const StructureBase *child);

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2004/05/28 21:01:45  thiessen
* namespace/typename fixes for GCC 3.4
*
* Revision 1.23  2003/02/03 19:20:07  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.22  2001/05/31 18:46:27  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.21  2001/05/17 18:34:01  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.20  2001/05/15 23:49:21  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.19  2001/05/15 14:57:48  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* Revision 1.18  2001/05/11 13:45:10  thiessen
* set up data directory
*
* Revision 1.17  2001/02/22 00:29:48  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.16  2000/11/11 21:12:08  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.15  2000/10/04 17:40:47  thiessen
* rearrange STL #includes
*
* Revision 1.14  2000/08/18 23:07:03  thiessen
* minor efficiency tweaks
*
* Revision 1.13  2000/08/18 18:57:44  thiessen
* added transparent spheres
*
* Revision 1.12  2000/08/17 14:22:01  thiessen
* added working StyleManager
*
* Revision 1.11  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.10  2000/08/04 22:49:11  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.9  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.8  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.7  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.6  2000/07/12 23:28:28  thiessen
* now draws basic CPK model
*
* Revision 1.5  2000/07/11 13:49:29  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.4  2000/07/01 15:44:23  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.3  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:16  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:08:13  thiessen
* initial checkin
*
*/
