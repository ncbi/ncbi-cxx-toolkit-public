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
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Atom_id.hpp>

#include "bond.hpp"
#include "atom_set.hpp"
#include "structure_set.hpp"
#include "coord_set.hpp"
#include "opengl_renderer.hpp"
#include "chemical_graph.hpp"
#include "periodic_table.hpp"
#include "style_manager.hpp"
#include "molecule.hpp"
#include "show_hide_manager.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

Bond::Bond(StructureBase *parent) : StructureBase(parent), order(eUnknown),
    previousVirtual(NULL), nextVirtual(NULL)
{
}

const Bond* MakeBond(StructureBase *parent,
    int mID1, int rID1, int aID1,
    int mID2, int rID2, int aID2,
    int bondOrder)
{
    // get StructureObject* parent
    const StructureObject *object;
    if (!parent->GetParentOfType(&object) || object->coordSets.size() == 0) {
        ERRORMSG("Bond() : parent doesn't have any CoordSets");
        return NULL;
    }

    AtomPntr ap1(mID1, rID1, aID1), ap2(mID2, rID2, aID2);

    Bond *bond = new Bond(parent);
    bond->atom1 = ap1;
    bond->atom2 = ap2;
    bond->order = static_cast<Bond::eBondOrder>(bondOrder);
    return bond;
}

const Bond* MakeBond(StructureBase *parent,
    const CAtom_pntr& atomPtr1, const CAtom_pntr& atomPtr2, int bondOrder)
{
    return MakeBond(parent,
        atomPtr1.GetMolecule_id().Get(),
        atomPtr1.GetResidue_id().Get(),
        atomPtr1.GetAtom_id().Get(),
        atomPtr2.GetMolecule_id().Get(),
        atomPtr2.GetResidue_id().Get(),
        atomPtr2.GetAtom_id().Get(),
        bondOrder
    );
}

bool Bond::Draw(const AtomSet *atomSet) const
{
    if (!parentSet->renderer) {
        ERRORMSG("Bond::Draw() - no renderer");
        return false;
    }

    // get AtomCoord* for appropriate altConf
    if (!atomSet) {
        ERRORMSG("Bond::Draw(data) - NULL AtomSet*");
        return false;
    }
    bool overlayEnsembles = parentSet->showHideManager->OverlayConfEnsembles();
    const AtomCoord *a1 = atomSet->GetAtom(atom1, overlayEnsembles, true);
    if (!a1) return true;
    const AtomCoord *a2 = atomSet->GetAtom(atom2, overlayEnsembles, true);
    if (!a2) return true;

    // get Style
    BondStyle bondStyle;
    if (!parentSet->styleManager->GetBondStyle(this, atom1, a1, atom2, a2,
            (a1->site - a2->site).length(), &bondStyle))
        return false;

    // don't show bond if either atom isn't visible
    if (bondStyle.end1.style == StyleManager::eNotDisplayed &&
        bondStyle.end2.style == StyleManager::eNotDisplayed)
        return true;

    // get prev, next alphas if drawing worm virtual bond
    const Vector *site0 = NULL, *site3 = NULL;
    if (bondStyle.end1.style == StyleManager::eLineWormBond ||
        bondStyle.end1.style == StyleManager::eThickWormBond ||
        bondStyle.end2.style == StyleManager::eLineWormBond ||
        bondStyle.end2.style == StyleManager::eThickWormBond) {

        if (previousVirtual) {
            const AtomCoord *a0 = atomSet->GetAtom(previousVirtual->atom1, overlayEnsembles, true);
            if (a0) site0 = &(a0->site);
        }
        if (!site0) site0 = &(a1->site);

        if (nextVirtual) {
            const AtomCoord *a3 = atomSet->GetAtom(nextVirtual->atom2, overlayEnsembles, true);
            if (a3) site3 = &(a3->site);
        }
        if (!site3) site3 = &(a2->site);
    }

    // draw the bond
    parentSet->renderer->DrawBond(a1->site, a2->site, bondStyle, site0, site3);

    return true;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/02/19 17:04:44  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.19  2003/06/21 08:18:58  thiessen
* show all atoms with coordinates, even if not in all coord sets
*
* Revision 1.18  2003/02/03 19:20:01  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.17  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.16  2001/06/29 18:13:57  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.15  2001/03/23 04:18:52  thiessen
* parse and display disulfides
*
* Revision 1.14  2000/12/01 19:35:56  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.13  2000/08/25 14:22:00  thiessen
* minor tweaks
*
* Revision 1.12  2000/08/24 18:43:52  thiessen
* tweaks for transparent sphere display
*
* Revision 1.11  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.10  2000/08/07 00:21:17  thiessen
* add display list mechanism
*
* Revision 1.9  2000/08/04 22:49:02  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.8  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.7  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.6  2000/07/18 16:50:10  thiessen
* more friendly rotation center setting
*
* Revision 1.5  2000/07/18 02:41:32  thiessen
* fix bug in virtual bonds and altConfs
*
* Revision 1.4  2000/07/17 22:37:17  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.3  2000/07/17 04:20:49  thiessen
* now does correct structure alignment transformation
*
* Revision 1.2  2000/07/16 23:19:10  thiessen
* redo of drawing system
*
* Revision 1.1  2000/07/11 13:45:28  thiessen
* add modules to parse chemical graph; many improvements
*
*/
