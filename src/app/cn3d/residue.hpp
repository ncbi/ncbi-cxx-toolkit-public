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
*      Classes to hold residues
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2001/03/28 23:01:38  thiessen
* first working full threading
*
* Revision 1.13  2001/03/23 23:31:30  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.12  2000/10/04 17:40:46  thiessen
* rearrange STL #includes
*
* Revision 1.11  2000/08/30 19:49:04  thiessen
* working sequence window
*
* Revision 1.10  2000/08/24 18:43:15  thiessen
* tweaks for transparent sphere display
*
* Revision 1.9  2000/08/17 14:22:00  thiessen
* added working StyleManager
*
* Revision 1.8  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.7  2000/08/04 22:49:11  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.6  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.5  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.4  2000/07/17 11:58:58  thiessen
* fix nucleotide virtual bonds
*
* Revision 1.3  2000/07/17 04:21:10  thiessen
* now does correct structure alignment transformation
*
* Revision 1.2  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.1  2000/07/11 13:49:29  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_RESIDUE__HPP
#define CN3D_RESIDUE__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <string>

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Residue.hpp>

#include "cn3d/structure_base.hpp"

BEGIN_SCOPE(Cn3D)

typedef list< ncbi::CRef< ncbi::objects::CResidue_graph > > ResidueGraphList;

// a Residue is a set of bonds that connect one residue of a larger Molecule.
// Its constructor is where most of the work of decoding the ASN1 graph is done,
// based on the standard and local residue dictionaries. Each Residue also holds
// information (AtomInfo) about the nature of the atoms it contains.

class Bond;

class Residue : public StructureBase
{
public:
    Residue(StructureBase *parent,
        const ncbi::objects::CResidue& residue, int moleculeID,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary);
    ~Residue(void);

    // public data
    int id;
    static const char NO_CODE;
    char code;
    std::string name;
    static const int NO_ALPHA_ID;
    int alphaID; // ID of "alpha" atom (C-alpha or P)

    // residue type
    enum eType {
        eDNA = ncbi::objects::CResidue_graph::eResidue_type_deoxyribonucleotide,
        eRNA = ncbi::objects::CResidue_graph::eResidue_type_ribonucleotide,
        eAminoAcid = ncbi::objects::CResidue_graph::eResidue_type_amino_acid,
        eOther = ncbi::objects::CResidue_graph::eResidue_type_other
    };
    eType type;

    // atom type
    enum eAtomClassification {
        eSideChainAtom,
        eAlphaBackboneAtom,     // C-alpha or P
        ePartialBackboneAtom,   // for unbranched backbone trace
        eCompleteBackboneAtom,  // all backbone atoms
        eUnknownAtom            // anything that's not known to be of an amino acid or nucleotide
    };

    typedef struct {
        std::string name, code;
        int atomicNumber;
        eAtomClassification classification;
        unsigned int glName;
        const Residue *residue;  // convenient way to go from atom->residue
        bool isIonizableProton;
        bool isPresentInAllCoordSets;
    } AtomInfo;

    typedef LIST_TYPE < const Bond * > BondList;
    BondList bonds;

    // public methods
    bool HasCode(void) const { return (code != NO_CODE); }
    bool HasName(void) const { return (!name.empty()); }
    bool IsNucleotide(void) const { return (type == eDNA || type == eRNA); }
    bool IsAminoAcid(void) const { return (type == eAminoAcid); }
    bool Draw(const AtomSet *atomSet) const;

    typedef std::map < int , const AtomInfo * > AtomInfoMap;

private:
    // mapped by Atom-id
    AtomInfoMap atomInfos;
    int nAtomsPresentInAllCoordSets;

public:
    int NAtoms(void) const { return nAtomsPresentInAllCoordSets; }

    const AtomInfoMap& GetAtomInfos(void) const { return atomInfos; }
    const AtomInfo * GetAtomInfo(int aID) const
    {
        AtomInfoMap::const_iterator info=atomInfos.find(aID);
        if (info != atomInfos.end()) return (*info).second;
        ERR_POST(ncbi::Warning << "Residue #" << id << ": can't find atom #" << aID);
        return NULL;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_RESIDUE__HPP
