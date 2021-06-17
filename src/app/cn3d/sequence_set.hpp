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
*      Classes to hold sets of sequences
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_SET__HPP
#define CN3D_SEQUENCE_SET__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <string>
#include <list>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "structure_base.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class Molecule;
class MasterDependentAlignment;
class MoleculeIdentifier;

class SequenceSet : public StructureBase
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    SequenceSet(StructureBase *parent, SeqEntryList& seqEntries);

    typedef std::list < const Sequence * > SequenceList;
    SequenceList sequences;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable

    const Sequence * FindMatchingSequence(const ncbi::objects::CBioseq::TId& ids) const;
};

class Sequence : public StructureBase
{
public:
    Sequence(SequenceSet *parent, ncbi::objects::CBioseq& bioseq);

    // keep a reference to the original asn Bioseq
    ncbi::CRef < ncbi::objects::CBioseq > bioseqASN;

    const MoleculeIdentifier *identifier;

    // corresponding biopolymer chain (if any)
    const Molecule *molecule;
    bool isProtein;

    std::string sequenceString, title, taxonomy;
    std::string GetDescription(void) const;

    unsigned int Length(void) const { return sequenceString.size(); }
    int GetOrSetMMDBLink(void) const;
    void AddMMDBAnnotTag(int mmdbID) const;

    // Seq-id stuff
    ncbi::objects::CSeq_id * CreateSeqId(void) const;
    void FillOutSeqId(ncbi::objects::CSeq_id *sid) const;

    // launch web browser with entrez page for this sequence
    void LaunchWebBrowserWithInfo(void) const;

    // highlight residues matching the given pattern; returns true if the pattern is valid,
    // regardless of whether a match is found, or returns false on error;
    // if the 'restrictTo' map is not empty, it restricts highlights to only those matches
    // within this map (for searching within a pre-selected subset)
    typedef std::map < const MoleculeIdentifier *, std::vector < bool > > MoleculeHighlightMap;
    bool HighlightPattern(const std::string& pattern, const MoleculeHighlightMap& restrictTo) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_SET__HPP
