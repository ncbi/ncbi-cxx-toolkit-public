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
*      Classes to hold sets of alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.21  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.20  2001/06/02 17:22:45  thiessen
* fixes for GCC
*
* Revision 1.19  2001/05/31 18:47:06  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.18  2001/05/14 16:04:31  thiessen
* fix minor row reordering bug
*
* Revision 1.17  2001/05/02 13:46:27  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.16  2001/04/17 20:15:39  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.15  2001/03/28 23:02:16  thiessen
* first working full threading
*
* Revision 1.14  2001/01/30 20:51:19  thiessen
* minor fixes
*
* Revision 1.13  2001/01/04 18:20:52  thiessen
* deal with accession seq-id
*
* Revision 1.12  2000/12/29 19:23:38  thiessen
* save row order
*
* Revision 1.11  2000/12/26 16:47:36  thiessen
* preserve block boundaries
*
* Revision 1.10  2000/12/20 23:47:47  thiessen
* load CDD's
*
* Revision 1.9  2000/11/30 15:49:35  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.8  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.7  2000/11/02 16:56:01  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.6  2000/09/20 22:22:26  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.5  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.4  2000/09/03 18:46:48  thiessen
* working generalized sequence viewer
*
* Revision 1.3  2000/08/29 04:34:26  thiessen
* working alignment manager, IBM
*
* Revision 1.2  2000/08/28 23:47:18  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.1  2000/08/28 18:52:41  thiessen
* start unpacking alignments
*
* ===========================================================================
*/

#include <corelib/ncbistl.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <map>
#include <memory>

#include "cn3d/alignment_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/molecule_identifier.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

typedef std::list < const CSeq_align * > SeqAlignList;
typedef std::list < CRef < CSeq_id > > SeqIdList;

bool IsAMatch(const Sequence *seq, const CSeq_id& sid)
{
    if (sid.IsGi()) {
        return (sid.GetGi() == seq->identifier->gi);
    }
    if (sid.IsPdb()) {
        if (sid.GetPdb().GetMol().Get() == seq->identifier->pdbID) {
            if (sid.GetPdb().IsSetChain()) {
                if (sid.GetPdb().GetChain() != seq->identifier->pdbChain) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    if (sid.IsLocal() && sid.GetLocal().IsStr()) {
        return (sid.GetLocal().GetStr() == seq->identifier->accession);
    }
    if (sid.IsGenbank() && sid.GetGenbank().IsSetAccession()) {
        return (sid.GetGenbank().GetAccession() == seq->identifier->accession);
    }
    if (sid.IsSwissprot() && sid.GetSwissprot().IsSetAccession()) {
        return (sid.GetSwissprot().GetAccession() == seq->identifier->accession);
    }
    ERR_POST(Error << "IsAMatch - can't match this type of Seq-id");
    return false;
}

AlignmentSet::AlignmentSet(StructureBase *parent, const SeqAnnotList& seqAnnots) :
    StructureBase(parent), master(NULL), newAsnAlignmentData(NULL)
{
    if (!parentSet->sequenceSet) {
        ERR_POST(Error << "AlignmentSet::AlignmentSet() - need sequenceSet before parsing alignments");
        return;
    }

    SeqAlignList seqaligns;

    // first, make a list of alignments
    SeqAnnotList::const_iterator n, ne = seqAnnots.end();
    for (n=seqAnnots.begin(); n!=ne; n++) {

        if (!n->GetObject().GetData().IsAlign()) {
            ERR_POST(Error << "AlignmentSet::AlignmentSet() - confused by alignment data format");
            return;
        }
        if (n != seqAnnots.begin()) TESTMSG("multiple Seq-annots");

        CSeq_annot::C_Data::TAlign::const_iterator
            a, ae = n->GetObject().GetData().GetAlign().end();
        for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; a++) {

            // verify this is a type of alignment we can deal with
            if (!(a->GetObject().GetType() != CSeq_align::eType_partial ||
                  a->GetObject().GetType() != CSeq_align::eType_diags) ||
                !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2 ||
                (!a->GetObject().GetSegs().IsDendiag() && !a->GetObject().GetSegs().IsDenseg())) {
                ERR_POST(Error << "AlignmentSet::AlignmentSet() - confused by alignment type");
                return;
            }

            seqaligns.push_back(a->GetPointer());
        }
    }
    if (seqaligns.size() == 0) {
        ERR_POST(Error << "AlignmentSet::AlignmentSet() - no valid Seq-aligns present");
        return;
    }

    // figure out which sequence is the master
    if (parentSet->sequenceSet->master) {
        // will be already set if >= 2 StructureObjects present
        master = parentSet->sequenceSet->master;
    }

    // otherwise, if there's one StructureObject, in each alignment one sequence should
    // have a corresponding Molecule, and one shouldn't - the master is the one with structure
    else if (parentSet->objects.size() == 1) {
        const SeqIdList& sids = seqaligns.front()->GetSegs().IsDendiag() ?
            seqaligns.front()->GetSegs().GetDendiag().front()->GetIds() :
            seqaligns.front()->GetSegs().GetDenseg().GetIds();
        // find first sequence that matches both ids
        const Sequence *seq1=NULL, *seq2=NULL;
        SequenceSet::SequenceList::const_iterator
            s, se = parentSet->sequenceSet->sequences.end();
        for (s=parentSet->sequenceSet->sequences.begin(); s!=se; s++) {
            if (!seq1 && IsAMatch(*s, sids.front().GetObject()))
                seq1 = *s;
            else if (!seq2 && IsAMatch(*s, sids.back().GetObject()))
                seq2 = *s;
            if (seq1 && seq2) break;
        }
        if (!seq1 || !seq2) {
            ERR_POST(Error << "AlignmentSet::AlignmentSet() - couldn't match both alignment Seq-ids");
            return;
        }
        if (seq1->molecule && !seq2->molecule)
            master = (const_cast<SequenceSet*>(parentSet->sequenceSet))->master = seq1;
        else if (seq2->molecule && !seq1->molecule)
            master = (const_cast<SequenceSet*>(parentSet->sequenceSet))->master = seq2;
    }

    // if there aren't any structures, then the first sequence of each pair must be the same,
    // and is the master
    else {
        SeqAlignList::const_iterator a, ae = seqaligns.end();
        for (a=seqaligns.begin(); a!=ae; a++) {
            const SeqIdList& sids = (*a)->GetSegs().IsDendiag() ?
                (*a)->GetSegs().GetDendiag().front()->GetIds() :
                (*a)->GetSegs().GetDenseg().GetIds();
            if (!master) {
                SequenceSet::SequenceList::const_iterator
                    s, se = parentSet->sequenceSet->sequences.end();
                for (s=parentSet->sequenceSet->sequences.begin(); s!=se; s++) {
                    if (IsAMatch(*s, sids.front().GetObject())) {
                        master = *s;
                        break;
                    }
                }
            }
            if (!IsAMatch(master, sids.front().GetObject())) {
                ERR_POST(Error << "AlignmentSet::AlignmentSet() - master must be first sequence of every alignment");
                return;
            }
        }
        (const_cast<SequenceSet*>(parentSet->sequenceSet))->master = master;
    }

    if (master) {
        TESTMSG("determined that master sequence is " << master->identifier->ToString());
    } else {
        ERR_POST(Error << "AlignmentSet::AlignmentSet() - couldn't determine which is master sequence");
        return;
    }

    // then make an alignment from each Seq-align; for any sequence that has structure, make
    // sure that that Sequence object only appears once in the alignment
    MasterSlaveAlignment::UsedSequenceList usedStructuredSequences;
    SeqAlignList::const_iterator s, se = seqaligns.end();
    for (s=seqaligns.begin(); s!=se; s++) {
        const MasterSlaveAlignment *alignment =
            new MasterSlaveAlignment(this, master, **s, &usedStructuredSequences);
        alignments.push_back(alignment);
    }
    TESTMSG("number of alignments: " << alignments.size());
}

AlignmentSet::~AlignmentSet(void)
{
    if (newAsnAlignmentData) delete newAsnAlignmentData;
}

AlignmentSet * AlignmentSet::CreateFromMultiple(StructureBase *parent,
    const BlockMultipleAlignment *multiple, const std::vector < int >& rowOrder)
{
    // sanity check on the row order map
    std::map < int, int > rowCheck;
    for (int i=0; i<rowOrder.size(); i++) rowCheck[rowOrder[i]] = i;
    if (rowOrder.size() != multiple->NRows() || rowCheck.size() != multiple->NRows() || rowOrder[0] != 0) {
        ERR_POST(Error << "AlignmentSet::CreateFromMultiple() - bad row order vector");
        return NULL;
    }

    // create a single Seq-annot, with 'align' data that holds one Seq-align per slave
    auto_ptr<SeqAnnotList> newAsnAlignmentData(new SeqAnnotList(1));
    CSeq_annot *seqAnnot = new CSeq_annot();
    newAsnAlignmentData->back().Reset(seqAnnot);

    CSeq_annot::C_Data::TAlign& seqAligns = seqAnnot->SetData().SetAlign();
    seqAligns.resize(multiple->NRows() - 1);
    CSeq_annot::C_Data::TAlign::iterator sa = seqAligns.begin();

    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks(multiple->GetUngappedAlignedBlocks());

    // create Seq-aligns
    int newRow;
    for (int row=1; row<multiple->NRows(); row++, sa++) {
        newRow = rowOrder[row];
        CSeq_align *seqAlign = CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks.get(), newRow);
        sa->Reset(seqAlign);
    }

    auto_ptr<AlignmentSet> newAlignmentSet;
    try {
        newAlignmentSet.reset(new AlignmentSet(parent, *newAsnAlignmentData));
    } catch (exception& e) {
        ERR_POST(Error
            << "AlignmentSet::CreateFromMultiple() - failed to create AlignmentSet from new asn object; "
            << "exception: " << e.what());
        return NULL;
    }

    newAlignmentSet->newAsnAlignmentData = newAsnAlignmentData.release();
    return newAlignmentSet.release();
}


///// MasterSlaveAlignment methods /////

MasterSlaveAlignment::MasterSlaveAlignment(StructureBase *parent, const Sequence *masterSequence,
    const ncbi::objects::CSeq_align& seqAlign, UsedSequenceList *usedStructuredSequences) :
    StructureBase(parent), master(masterSequence), slave(NULL)
{
    // resize alignment and block vector
    masterToSlave.resize(master->Length(), -1);
    blockStructure.resize(master->Length(), -1);

    SequenceSet::SequenceList::const_iterator
        s = master->parentSet->sequenceSet->sequences.begin(),
        se = master->parentSet->sequenceSet->sequences.end();

    // find slave sequence for this alignment, and order (master or slave first)
    const SeqIdList& sids = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds() :
        seqAlign.GetSegs().GetDenseg().GetIds();

    bool masterFirst = true;
    for (; s!=se; s++) {

        if (*s == master ||
                ((*s)->molecule != NULL && usedStructuredSequences &&
                 usedStructuredSequences->find(*s) != usedStructuredSequences->end()))
            continue;

        if (IsAMatch(*s, sids.back().GetObject())) {
            if (usedStructuredSequences && (*s)->molecule != NULL) (*usedStructuredSequences)[*s] = true;
            break;
        } else if (IsAMatch(*s, sids.front().GetObject())) {
            masterFirst = false;
            if (usedStructuredSequences && (*s)->molecule != NULL) (*usedStructuredSequences)[*s] = true;
            break;
        }
    }
    if (s == se) {
        // special case of master seq. aligned to itself - but only allowed if it doesn't have structure
        if (!master->molecule &&
            IsAMatch(master, sids.back().GetObject()) && IsAMatch(master, sids.front().GetObject())) {
            slave = master;
        } else {
            ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "couldn't find matching unaligned slave sequence");
            return;
        }
    } else {
        slave = *s;
    }

    int i, masterRes, slaveRes, blockNum = 0;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; d++, blockNum++) {
            const CDense_diag& block = d->GetObject();

            if (!block.IsSetDim() || block.GetDim() != 2 ||
                block.GetIds().size() != 2 ||
                block.GetStarts().size() != 2) {
                ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                    "incorrect dendiag block dimensions");
                return;
            }

            // make sure identities of master and slave sequences match in each block
            if ((masterFirst &&
                    (!IsAMatch(master, block.GetIds().front().GetObject()) ||
                     !IsAMatch(slave, block.GetIds().back().GetObject()))) ||
                (!masterFirst &&
                    (!IsAMatch(master, block.GetIds().back().GetObject()) ||
                     !IsAMatch(slave, block.GetIds().front().GetObject())))) {
                ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                    "mismatched Seq-id in dendiag block");
                return;
            }

            // finally, actually unpack the data into the alignment vector
            for (i=0; i<block.GetLen(); i++) {
                if (masterFirst) {
                    masterRes = block.GetStarts().front() + i;
                    slaveRes = block.GetStarts().back() + i;
                } else {
                    masterRes = block.GetStarts().back() + i;
                    slaveRes = block.GetStarts().front() + i;
                }
                if (masterRes >= master->Length() || slaveRes >= slave->Length()) {
                    ERR_POST(Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                        "seqloc in dendiag block > length of sequence!");
                    return;
                }
                masterToSlave[masterRes] = slaveRes;
                blockStructure[masterRes] = blockNum;
            }
        }
    }

    // unpack denseg alignment
    else if (seqAlign.GetSegs().IsDenseg()) {

        const CDense_seg& block = seqAlign.GetSegs().GetDenseg();

        if (!block.IsSetDim() && block.GetDim() != 2 ||
            block.GetIds().size() != 2 ||
            block.GetStarts().size() != 2 * block.GetNumseg() ||
            block.GetLens().size() != block.GetNumseg()) {
            ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "incorrect denseg block dimension");
            return;
        }

        // make sure identities of master and slave sequences match in each block
        if ((masterFirst &&
                (!IsAMatch(master, block.GetIds().front().GetObject()) ||
                 !IsAMatch(slave, block.GetIds().back().GetObject()))) ||
            (!masterFirst &&
                (!IsAMatch(master, block.GetIds().back().GetObject()) ||
                 !IsAMatch(slave, block.GetIds().front().GetObject())))) {
            ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "mismatched Seq-id in denseg block");
            return;
        }

        // finally, actually unpack the data into the alignment vector
        CDense_seg::TStarts::const_iterator starts = block.GetStarts().begin();
        CDense_seg::TLens::const_iterator lens, le = block.GetLens().end();
        for (lens=block.GetLens().begin(); lens!=le; lens++) {
            if (masterFirst) {
                masterRes = *(starts++);
                slaveRes = *(starts++);
            } else {
                slaveRes = *(starts++);
                masterRes = *(starts++);
            }
            if (masterRes != -1 && slaveRes != -1) { // skip gaps
                if ((masterRes + *lens - 1) >= master->Length() ||
                    (slaveRes + *lens - 1) >= slave->Length()) {
                    ERR_POST(Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                        "seqloc in denseg block > length of sequence!");
                    return;
                }
                for (i=0; i<*lens; i++) {
                    masterToSlave[masterRes + i] = slaveRes + i;
                    blockStructure[masterRes + i] = blockNum;
                }
                blockNum++; // a "block" of a denseg is an aligned (non-gap) segment
            }
        }
    }

    //TESTMSG("got alignment for slave gi " << slave->identifier->gi);
}

END_SCOPE(Cn3D)

