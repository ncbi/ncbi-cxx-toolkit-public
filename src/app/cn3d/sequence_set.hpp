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

#include <corelib/ncbistd.hpp> // must come first to avoid NCBI type clashes
#include <corelib/ncbistl.hpp>

#include <string>
#include <list>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objloc.h>

#include "structure_base.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class Molecule;
class MasterSlaveAlignment;
class MoleculeIdentifier;

class SequenceSet : public StructureBase
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    SequenceSet(StructureBase *parent, SeqEntryList& seqEntries);

    typedef std::list < const Sequence * > SequenceList;
    SequenceList sequences;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable
};

class Sequence : public StructureBase
{
public:
    Sequence(SequenceSet *parent, ncbi::objects::CBioseq& bioseq);

    // keep a reference to the original asn Bioseq
    ncbi::CRef < ncbi::objects::CBioseq > bioseqASN;

    std::string sequenceString, description;
    const MoleculeIdentifier *identifier;

    // corresponding biopolymer chain (if any)
    const Molecule *molecule;
    bool isProtein;

    int Length(void) const { return sequenceString.size(); }
    int GetOrSetMMDBLink(void) const;
    void AddMMDBAnnotTag(int mmdbID) const;

    // Seq-id stuff (C++ and C)
    ncbi::objects::CSeq_id * CreateSeqId(void) const;
    void FillOutSeqId(ncbi::objects::CSeq_id *sid) const;
    void AddCSeqId(SeqIdPtr *id, bool addAllTypes) const;

    // launch web browser with entrez page for this sequence
    void LaunchWebBrowserWithInfo(void) const;

    // highlight residues matching the given pattern; returns true if the pattern is valid,
    // regardless of whether a match is found, or returns false on error
    bool HighlightPattern(const std::string& pattern) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_SET__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2004/05/21 17:29:51  thiessen
* allow conversion of mime to cdd data
*
* Revision 1.31  2004/02/19 17:05:07  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.30  2003/07/14 18:37:08  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.29  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.28  2002/11/19 21:19:44  thiessen
* more const changes for objects; fix user vs default style bug
*
* Revision 1.27  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.26  2001/11/27 16:26:09  thiessen
* major update to data management system
*
* Revision 1.25  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.24  2001/08/15 20:53:07  juran
* Heed warnings.
*
* Revision 1.23  2001/07/23 20:08:38  thiessen
* add regex pattern search
*
* Revision 1.22  2001/07/19 19:12:46  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.21  2001/06/21 02:01:07  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.20  2001/05/31 18:46:27  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.19  2001/05/02 16:35:18  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.18  2001/04/18 15:46:32  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.17  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.16  2001/02/16 00:36:47  thiessen
* remove unused sequences from asn data
*
* Revision 1.15  2001/02/13 01:03:03  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.14  2001/02/08 23:01:14  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.13  2001/01/04 18:21:00  thiessen
* deal with accession seq-id
*
* Revision 1.12  2000/12/21 23:42:24  thiessen
* load structures from cdd's
*
* Revision 1.11  2000/12/20 23:47:52  thiessen
* load CDD's
*
* Revision 1.10  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.9  2000/11/17 19:47:37  thiessen
* working show/hide alignment row
*
* Revision 1.8  2000/11/11 21:12:07  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.7  2000/10/04 17:40:46  thiessen
* rearrange STL #includes
*
* Revision 1.6  2000/09/03 18:45:57  thiessen
* working generalized sequence viewer
*
* Revision 1.5  2000/08/30 19:49:04  thiessen
* working sequence window
*
* Revision 1.4  2000/08/29 04:34:15  thiessen
* working alignment manager, IBM
*
* Revision 1.3  2000/08/28 23:46:46  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.2  2000/08/28 18:52:18  thiessen
* start unpacking alignments
*
* Revision 1.1  2000/08/27 18:50:56  thiessen
* extract sequence information
*
*/
