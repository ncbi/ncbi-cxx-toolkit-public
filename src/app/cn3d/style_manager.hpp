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
*      manager object to track drawing style of objects at various levels
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/08/13 02:42:13  thiessen
* added helix and strand objects
*
* Revision 1.4  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.3  2000/08/07 00:20:19  thiessen
* add display list mechanism
*
* Revision 1.2  2000/08/04 22:49:11  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.1  2000/08/03 15:14:18  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#ifndef CN3D_STYLE_MANAGER__HPP
#define CN3D_STYLE_MANAGER__HPP

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class StructureObject;
class AtomPntr;
class Bond;
class BondStyle;
class AtomStyle;
class Helix3D;
class HelixStyle;
class Strand3D;
class StrandStyle;

class StyleManager
{
public:

    // display styles
    enum eDisplayStyle {
        eSphereAtom,
        eLineBond,
        eCylinderBond,
        eLineWormBond,
        eThickWormBond,
        eObjectWithArrow,
        eObjectWithoutArrow,
        eNotDisplayed
    };

    // style accessors
    bool GetAtomStyle(const StructureObject *object, const AtomPntr& atom, AtomStyle *atomStyle);
    bool GetBondStyle(const Bond *bond,
            const AtomPntr& atom1, const AtomPntr& atom2,
            BondStyle *bondStyle);
    bool GetHelixStyle(const StructureObject *object, const Helix3D& helix, HelixStyle *helixStyle);
    bool GetStrandStyle(const StructureObject *object, const Strand3D& strand, StrandStyle *strandStyle);

private:
};

class AtomStyle
{
public:
    StyleManager::eDisplayStyle style;
    Vector color;
    double radius;
    int slices, stacks;
    unsigned int name;
};

class BondStyle
{
public:
    struct _endstyle {
        StyleManager::eDisplayStyle style;
        Vector color;
        double radius;
        bool atomCap;
        int sides, segments;
        unsigned int name;
    } end1, end2;
    bool midCap;
    double tension;
};

class HelixStyle
{
public:
    StyleManager::eDisplayStyle style;
    double radius;
    Vector color;
    double arrowLength, arrowBaseWidthProportion, arrowTipWidthProportion;
    int sides;
};

class StrandStyle
{
public:
    StyleManager::eDisplayStyle style;
    double width, thickness;
    Vector color;
    double arrowLength, arrowBaseWidthProportion;
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_MANAGER__HPP
