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

#define TESTMSG(stream) ERR_POST(Info << stream);
//#define TESTMSG(stream)

// container type used for various lists
#include <deque>
#define LIST_TYPE deque

// convenience macros for lists of StructureBase-derived objects
#define DELETE_ALL(theClass, theList) \
    for (theClass::iterator i=(theList).begin(), e=(theList).end(); i!=e; i++) delete *i   

#define DRAW_ALL(theClass, theList) \
    for (theClass::const_iterator i=(theList).begin(), e=(theList).end(); i!=e; i++) (*i)->Draw()


BEGIN_SCOPE(Cn3D)

class StructureBase
{
public:
    // pure virtual so can't instantiate this class; implemented in StructureSet.cpp
    virtual ~StructureBase(void) = 0;
    virtual void Draw(void) const { TESTMSG("can't draw this class!"); }
private:
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTUREBASE__HPP
