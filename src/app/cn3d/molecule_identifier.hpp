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
*      Class to hold, and factory to generate, general
*      (instance-independent) identifier for any molecule
*
* ===========================================================================
*/

#ifndef CN3D_MOLECULE_IDENTIFIER__HPP
#define CN3D_MOLECULE_IDENTIFIER__HPP

#include <corelib/ncbistl.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <list>
#include <string>


BEGIN_SCOPE(Cn3D)

class Molecule;
class Sequence;

class MoleculeIdentifier
{
public:
    static const int VALUE_NOT_SET;

    // store all Seq-ids, and also mmdb info
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_id > > SeqIdList;
    SeqIdList seqIDs;
    int mmdbID, moleculeID, pdbChain, gi;
    std::string pdbID;

    // # residues (1 for non-biopolymers - hets, solvents)
    unsigned int nResidues;

    // get title string based on identifiers present
    std::string ToString(void) const;

    // create, or retrieve if known, an identifier for an entity
    static const MoleculeIdentifier * GetIdentifier(const Molecule *molecule, const SeqIdList& ids);
    static const MoleculeIdentifier * GetIdentifier(const Sequence *sequence, int mmdbID, const SeqIdList& ids);

    // get identifier for MMDB ID + molecule (NULL if not found)
    static const MoleculeIdentifier * FindIdentifier(int mmdbID, int moleculeID);

    // test for Seq-id match
    bool MatchesSeqId(const ncbi::objects::CSeq_id& sid) const;

    // clear identifier store (e.g. when a new file is loaded)
    static void ClearIdentifiers(void);

    // does this molecule have structure?
    bool HasStructure(void) const
    {
        return (
            (mmdbID != VALUE_NOT_SET && moleculeID != VALUE_NOT_SET) ||
            (pdbID.size() > 0 && pdbChain != VALUE_NOT_SET));
    }

    // comparison of identifiers (e.g. for sorting) - floats PDB's to top, then gi's in numerical order
    static bool CompareIdentifiers(const MoleculeIdentifier *a, const MoleculeIdentifier *b);

    // get general label (e.g. an accession)
    std::string GetLabel(void) const
    {
        std::string label;
        if (seqIDs.size() > 0)
            seqIDs.front()->GetLabel(&label, ncbi::objects::CSeq_id::eContent, 0);
        return label;
    }

private:
    // can't create one of these directly - must use GetIdentifier()
    MoleculeIdentifier(void) :
        mmdbID(VALUE_NOT_SET), moleculeID(VALUE_NOT_SET), pdbChain(VALUE_NOT_SET), gi(VALUE_NOT_SET), nResidues(0)
        { }

    // get identifier based on Seq-id match
    static MoleculeIdentifier * GetIdentifier(const SeqIdList& ids);

    // get identifier based on MMDB ID + molecule, for stuff like ligands that don't have Seq-id
    static MoleculeIdentifier * GetIdentifier(int mmdbID, int moleculeID);

    // save and fill out special id fields from Seq-ids
    void AddFields(const SeqIdList& ids);
};

END_SCOPE(Cn3D)

#endif // CN3D_MOLECULE_IDENTIFIER__HPP
