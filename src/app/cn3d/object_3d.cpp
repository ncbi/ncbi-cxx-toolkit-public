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
*      holds 3d-objects - helix cylinders and strand bricks
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb3/Model_space_point.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>

#include "object_3d.hpp"
#include "opengl_renderer.hpp"
#include "style_manager.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

const int Object3D::VALUE_NOT_SET = -1;

Object3D::Object3D(StructureBase *parent, const CResidue_pntrs& residues) :
    StructureBase(parent),
    moleculeID(VALUE_NOT_SET), fromResidueID(VALUE_NOT_SET), toResidueID(VALUE_NOT_SET)
{
    if (!residues.IsInterval() || residues.GetInterval().size() != 1) {
        ERRORMSG("Object3D::Object3D() - can't handle this type of Residue-pntrs (yet)!");
        return;
    }

    const CResidue_interval_pntr& interval = residues.GetInterval().front().GetObject();
    moleculeID = interval.GetMolecule_id().Get();
    fromResidueID = interval.GetFrom().Get();
    toResidueID = interval.GetTo().Get();
}

static inline void ModelPoint2Vector(const CModel_space_point& msp, Vector *v)
{
    v->x = msp.GetX();
    v->y = msp.GetY();
    v->z = msp.GetZ();
    *v /= msp.GetScale_factor();
}

Helix3D::Helix3D(StructureBase *parent, const CCylinder& cylinder, const CResidue_pntrs& residues) :
    Object3D(parent, residues)
{
    if (moleculeID == Object3D::VALUE_NOT_SET) return;

    ModelPoint2Vector(cylinder.GetAxis_bottom(), &Nterm);
    ModelPoint2Vector(cylinder.GetAxis_top(), &Cterm);
}

bool Helix3D::Draw(const AtomSet *data) const
{
    if (!parentSet->renderer) {
        ERRORMSG("Helix3D::Draw() - no renderer");
        return false;
    }

    // get object parent
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;

    // get Style
    HelixStyle helixStyle;
    if (!parentSet->styleManager->GetHelixStyle(object, *this, &helixStyle))
        return false;

    // draw the Helix
    if (helixStyle.style != StyleManager::eNotDisplayed)
        parentSet->renderer->DrawHelix(Nterm, Cterm, helixStyle);

    return true;
}

Strand3D::Strand3D(StructureBase *parent, const CBrick& brick, const CResidue_pntrs& residues) :
    Object3D(parent, residues)
{
    if (moleculeID == Object3D::VALUE_NOT_SET) return;

    Vector c1, c2;
    ModelPoint2Vector(brick.GetCorner_000(), &c1);
    ModelPoint2Vector(brick.GetCorner_011(), &c2);
    Nterm = (c1 + c2) / 2;

    ModelPoint2Vector(brick.GetCorner_100(), &c1);
    ModelPoint2Vector(brick.GetCorner_111(), &c2);
    Cterm = (c1 + c2) / 2;

    ModelPoint2Vector(brick.GetCorner_010(), &c1);
    ModelPoint2Vector(brick.GetCorner_000(), &c2);
    unitNormal = c1 - c2;
    unitNormal.normalize();
}

bool Strand3D::Draw(const AtomSet *data) const
{
    if (!parentSet->renderer) {
        ERRORMSG("Strand3D::Draw() - no renderer");
        return false;
    }

    // get object parent
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;

    // get Style
    StrandStyle strandStyle;
    if (!parentSet->styleManager->GetStrandStyle(object, *this, &strandStyle))
        return false;

    // draw the Strand
    if (strandStyle.style != StyleManager::eNotDisplayed)
        parentSet->renderer->DrawStrand(Nterm, Cterm, unitNormal, strandStyle);

    return true;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2004/02/19 17:04:59  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.7  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.6  2001/12/12 14:04:14  thiessen
* add missing object headers after object loader change
*
* Revision 1.5  2001/02/08 23:01:50  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.4  2000/08/17 14:24:06  thiessen
* added working StyleManager
*
* Revision 1.3  2000/08/16 14:18:45  thiessen
* map 3-d objects to molecules
*
* Revision 1.2  2000/08/13 02:43:01  thiessen
* added helix and strand objects
*
* Revision 1.1  2000/08/11 18:24:57  thiessen
* add 3-d objects code
*
*/
