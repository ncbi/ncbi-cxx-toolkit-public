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
*      Classes to hold chemical bonds
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/11 13:45:28  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Atom_id.hpp>

#include "cn3d/bond.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

Bond::Bond(StructureBase *parent, 
    int mID1, int rID1, int aID1, 
    int mID2, int rID2, int aID2, 
    int bondOrder) :
    StructureBase(parent)
{
    atom1.mID = mID1;
    atom1.rID = rID1;
    atom1.aID = aID1;
    atom2.mID = mID2;
    atom2.rID = rID2;
    atom2.aID = aID2;
    order = static_cast<eBondOrder>(bondOrder);
}

Bond::Bond(StructureBase *parent,
    const CAtom_pntr& atomPtr1, const CAtom_pntr& atomPtr2, int bondOrder) :
    StructureBase(parent)
{
    atom1.mID = atomPtr1.GetMolecule_id().Get();
    atom1.rID = atomPtr1.GetResidue_id().Get();
    atom1.aID = atomPtr1.GetAtom_id().Get();
    atom2.mID = atomPtr2.GetMolecule_id().Get();
    atom2.rID = atomPtr2.GetResidue_id().Get();
    atom2.aID = atomPtr2.GetAtom_id().Get();
    order = static_cast<eBondOrder>(bondOrder);
}

bool Bond::Draw(void) const
{
    // prune draw tree here
    TESTMSG("not drawing Bond yet");
    return false;
}

END_SCOPE(Cn3D)

