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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/08/11 18:24:57  thiessen
* add 3-d objects code
*
* ===========================================================================
*/

#include <objects/mmdb3/Residue_interval_pntr.hpp>

#include "cn3d/object_3d.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

const int Object3D::NOT_SET = -1;

Object3D::Object3D(StructureBase *parent, const CResidue_pntrs& residues) :
    StructureBase(parent), molecule(NOT_SET), fromResidue(NOT_SET), toResidue(NOT_SET)
{
    if (!residues.IsInterval() || residues.GetInterval().size() != 1) {
        ERR_POST(Error << "Object3D::Object3D() - can't handle this type of Residue-pntrs (yet)!");
        return;
    }

    const CResidue_interval_pntr& interval = residues.GetInterval().front().GetObject();
    molecule = interval.GetMolecule_id().Get();
    fromResidue = interval.GetFrom().Get();
    toResidue = interval.GetTo().Get();
}

Helix3D::Helix3D(StructureBase *parent, const CCylinder& cylinder, const CResidue_pntrs& residues) :
    Object3D(parent, residues)
{
    if (molecule == Object3D::NOT_SET) return;
    TESTMSG("got helix: " << molecule << ' ' << fromResidue << ' ' << toResidue);
}

Strand3D::Strand3D(StructureBase *parent, const CBrick& brick, const CResidue_pntrs& residues) :
    Object3D(parent, residues)
{
    if (molecule == Object3D::NOT_SET) return;
    TESTMSG("got strand: " << molecule << ' ' << fromResidue << ' ' << toResidue);
}

END_SCOPE(Cn3D)

