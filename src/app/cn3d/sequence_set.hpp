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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_SET__HPP
#define CN3D_SEQUENCE_SET__HPP

#include <corelib/ncbistl.hpp>

#include <string>
#include <list>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "cn3d/structure_base.hpp"


BEGIN_SCOPE(Cn3D)

typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;

class Sequence;
class Molecule;
class MasterSlaveAlignment;

class SequenceSet : public StructureBase
{
public:
    SequenceSet(StructureBase *parent, ncbi::objects::CSeq_entry& seqEntry);
    SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries);

    typedef LIST_TYPE < const Sequence * > SequenceList;
    SequenceList sequences;

    // there is one and only one sequence in this set that will be the master
    // for all alignments of sequences from this set
    const Sequence *master;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable
};

class Sequence : public StructureBase
{
public:
    Sequence(StructureBase *parent, ncbi::objects::CBioseq& bioseq);

    // keep a reference to the original asn Bioseq
    const ncbi::CRef < ncbi::objects::CBioseq > bioseqASN;

    static const int VALUE_NOT_SET;
    int gi, pdbChain, mmdbLink;
    std::string pdbID, sequenceString, accession, description;
    bool isProtein;

    // corresponding protein chain (if any)
    const Molecule *molecule;

    int Length(void) const { return sequenceString.size(); }
	ncbi::objects::CSeq_id * CreateSeqId(void) const;
    std::string GetTitle(void) const;
    int GetOrSetMMDBLink(void) const;

    // launch web browser with entrez page for this sequence
    void LaunchWebBrowserWithInfo(void) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_SET__HPP
