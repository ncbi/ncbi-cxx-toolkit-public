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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

#ifndef CN3D_STRUCTUREBASE__HPP
#define CN3D_STRUCTUREBASE__HPP

// container type used for various lists
#include <list>
#define LIST_TYPE std::list

#include <map>

#include <corelib/ncbidiag.hpp>

USING_NCBI_SCOPE;

#define TESTMSG(stream) ERR_POST(Info << stream)
//#define TESTMSG(stream)


BEGIN_SCOPE(Cn3D)

class StructureBase
{
public:
    // will store StructureBase-derived children in parent upon construction
    StructureBase(StructureBase *parent, bool warnOnNULL = true);

    // will automatically delete children upon destruction with this desctuctor.
    // But note that derived classes can have their own constructors that will
    // *also* be called upon deletion; derived class destructors should *not*
    // delete any StructureBase-derived objects.
    virtual ~StructureBase(void);

    // Draws the object (if visible) and all its children - do not override!
    bool DrawAll(void) const;

    // function to draw this object, called before children are Drawn. Return
    // false to halt recursive drawing process
    virtual bool Draw(void) const { ERR_POST(Fatal << "in StructureBase::Draw()"); return false; }

    // called after this object and its children have been Drawn
    //virtual bool PostDraw(void) const { TESTMSG("in PostDraw()"); return true; }

private:
    // no default construction
    StructureBase(void);

    // keep track of StructureBase-derived children, so that top-down operations
    // like drawing or deconstructing can trickle down automatically
    typedef std::map < StructureBase * , int > _ChildList;
    _ChildList _children;
    void _AddChild(StructureBase *child);
    void _RemoveChild(StructureBase *child);
    StructureBase* _parent;

#if 0  // don't know if we'll actually need/want these
    // misc flags
    enum _eFlags {
        _eVisible = 1
    };
    typedef unsigned char FlagType;
    FlagType _flags;

    // to set/query all flags
    bool GetFlag(FlagType flag) const { return ((_flags & flag) > 0); }
    void SetFlag(FlagType flag, bool on)
    {
        if (on) _flags |= flag; else _flags &= ~flag;
    }

    // to set/query specific flags
    bool IsVisible(void) const { return GetFlag(_eVisible); }
    void SetVisible(bool on) { SetFlag(_eVisible, on); }
#endif

public:
    // go up the hierarchy to find a parent of the desired type
    template < class T >
    bool GetParentOfType(T* *ptr) const
    {
        *ptr = NULL;
        for (const StructureBase *parent=this; parent; parent=parent->_parent) {
            if ((*ptr = dynamic_cast<T*>(parent)) != NULL) {
                return true;
            }
        }
        return false;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTUREBASE__HPP
