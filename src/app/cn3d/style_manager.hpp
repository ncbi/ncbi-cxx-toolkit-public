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
#include "cn3d/structure_base.hpp"
#include "cn3d/bond.hpp"


BEGIN_SCOPE(Cn3D)

class StructureObject;
class BondStyle;
class AtomStyle;

class StyleManager
{
public:

    // display styles
    enum eDisplayStyle {
        eSphereAtom,
        eLineBond,
        eCylinderBond,
        eWormBond,
        eNotDisplayed
    };

    // style accessors
    bool GetAtomStyle(const StructureObject *object, const AtomPntr& atom, AtomStyle *atomStyle);
    bool GetBondStyle(const StructureObject *object,
            const AtomPntr& atom1, const AtomPntr& atom2, Bond::eBondOrder order,
            BondStyle *bondStyle);

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

    AtomStyle(void)
    {
        style = StyleManager::eNotDisplayed;
        color = Vector(0,0,0);
        radius = 0.0;
        slices = stacks = 3;
    }
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

    BondStyle(void)
    {
        end1.style = end2.style = StyleManager::eNotDisplayed;
        end1.color = end2.color = Vector(0,0,0);
        end1.radius = end2.radius = 0.0;
        end1.atomCap = end2.atomCap = midCap = false;
        end1.sides = end2.sides = 3;
        end1.segments = end2.segments = 2;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_MANAGER__HPP
