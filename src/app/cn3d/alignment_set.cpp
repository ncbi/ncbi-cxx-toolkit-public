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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <map>
#include <memory>

#include "alignment_set.hpp"
#include "sequence_set.hpp"
#include "structure_set.hpp"
#include "block_multiple_alignment.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

typedef list < const CSeq_align * > SeqAlignList;

AlignmentSet::AlignmentSet(StructureBase *parent, const Sequence *masterSequence,
    const SeqAnnotList& seqAnnots) :
    StructureBase(parent), master(masterSequence), newAsnAlignmentData(NULL)
{
    if (!master || !parentSet->sequenceSet) {
        ERRORMSG("AlignmentSet::AlignmentSet() - need sequenceSet and master before parsing alignments");
        return;
    }

    // assume the data manager has collapsed all valid seqaligns into a single seqannot
    CSeq_annot::C_Data::TAlign::const_iterator
        a, ae = seqAnnots.front()->GetData().GetAlign().end();
    for (a=seqAnnots.front()->GetData().GetAlign().begin(); a!=ae; ++a)
        alignments.push_back(new MasterDependentAlignment(this, master, **a));
    TRACEMSG("number of alignments: " << alignments.size());
}

AlignmentSet::~AlignmentSet(void)
{
    if (newAsnAlignmentData) delete newAsnAlignmentData;
}

AlignmentSet * AlignmentSet::CreateFromMultiple(StructureBase *parent,
    const BlockMultipleAlignment *multiple, const vector < unsigned int >& rowOrder)
{
    // sanity check on the row order map
    map < unsigned int, unsigned int > rowCheck;
    for (unsigned int i=0; i<rowOrder.size(); ++i) rowCheck[rowOrder[i]] = i;
    if (rowOrder.size() != multiple->NRows() || rowCheck.size() != multiple->NRows() || rowOrder[0] != 0) {
        ERRORMSG("AlignmentSet::CreateFromMultiple() - bad row order vector");
        return NULL;
    }

    // create a single Seq-annot, with 'align' data that holds one Seq-align per dependent
    auto_ptr<SeqAnnotList> newAsnAlignmentData(new SeqAnnotList(1));
    CSeq_annot *seqAnnot = new CSeq_annot();
    newAsnAlignmentData->back().Reset(seqAnnot);

    CSeq_annot::C_Data::TAlign& seqAligns = seqAnnot->SetData().SetAlign();
    seqAligns.resize((multiple->NRows() == 1) ? 1 : multiple->NRows() - 1);
    CSeq_annot::C_Data::TAlign::iterator sa = seqAligns.begin();

    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);

    // create Seq-aligns; if there's only one row (the master), then cheat and create an alignment
    // of the master with itself, because asn data doesn't take well to single-row "alignment"
    if (multiple->NRows() > 1) {
        unsigned int newRow;
        for (unsigned int row=1; row<multiple->NRows(); ++row, ++sa) {
          newRow = rowOrder[row];
          CSeq_align *seqAlign = CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks, newRow);
          sa->Reset(seqAlign);
        }
    } else
        sa->Reset(CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks, 0));

    auto_ptr<AlignmentSet> newAlignmentSet;
    try {
        newAlignmentSet.reset(new AlignmentSet(parent, multiple->GetMaster(), *newAsnAlignmentData));
    } catch (exception& e) {
        ERRORMSG(
            "AlignmentSet::CreateFromMultiple() - failed to create AlignmentSet from new asn object; "
            << "exception: " << e.what());
        return NULL;
    }

    newAlignmentSet->newAsnAlignmentData = newAsnAlignmentData.release();
    return newAlignmentSet.release();
}


///// MasterDependentAlignment methods /////

MasterDependentAlignment::MasterDependentAlignment(StructureBase *parent, const Sequence *masterSequence,
    const ncbi::objects::CSeq_align& seqAlign) :
    StructureBase(parent), master(masterSequence), dependent(NULL)
{
    // resize alignment and block vector
    masterToDependent.resize(master->Length(), -1);
    blockStructure.resize(master->Length(), -1);

    // find dependent sequence for this alignment, and order (master or dependent first)
    const CSeq_id& frontSeqId = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
        seqAlign.GetSegs().GetDenseg().GetIds().front().GetObject();
    const CSeq_id& backSeqId = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds().back().GetObject() :
        seqAlign.GetSegs().GetDenseg().GetIds().back().GetObject();

    bool masterFirst = true;
    SequenceSet::SequenceList::const_iterator
        s, se = master->parentSet->sequenceSet->sequences.end();
    for (s=master->parentSet->sequenceSet->sequences.begin(); s!=se; ++s) {
        if (master->identifier->MatchesSeqId(frontSeqId) &&
            (*s)->identifier->MatchesSeqId(backSeqId)) {
            break;
        } else if ((*s)->identifier->MatchesSeqId(frontSeqId) &&
                   master->identifier->MatchesSeqId(backSeqId)) {
            masterFirst = false;
            break;
        }
    }
    if (s == se) {
        ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - couldn't find matching sequences; "
            << "both " << frontSeqId.AsFastaString() << " and "
            << backSeqId.AsFastaString() << " must be in the sequence list for this file!");
        return;
    } else {
        dependent = *s;
    }

    unsigned int i, blockNum = 0;
    int masterRes, dependentRes;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; ++d, ++blockNum) {
            const CDense_diag& block = d->GetObject();

            if (block.GetDim() != 2 || block.GetIds().size() != 2 || block.GetStarts().size() != 2) {
                ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - \n"
                    "incorrect dendiag block dimensions");
                return;
            }

            // make sure identities of master and dependent sequences match in each block
            if ((masterFirst &&
                    (!master->identifier->MatchesSeqId(block.GetIds().front().GetObject()) ||
                     !dependent->identifier->MatchesSeqId(block.GetIds().back().GetObject()))) ||
                (!masterFirst &&
                    (!master->identifier->MatchesSeqId(block.GetIds().back().GetObject()) ||
                     !dependent->identifier->MatchesSeqId(block.GetIds().front().GetObject())))) {
                ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - "
                    "mismatched Seq-id in dendiag block");
                return;
            }

            // finally, actually unpack the data into the alignment vector
            for (i=0; i<block.GetLen(); ++i) {
                if (masterFirst) {
                    masterRes = block.GetStarts().front() + i;
                    dependentRes = block.GetStarts().back() + i;
                } else {
                    masterRes = block.GetStarts().back() + i;
                    dependentRes = block.GetStarts().front() + i;
                }
                if (masterRes < 0 || masterRes >= (int)master->Length() || dependentRes < 0 || dependentRes >= (int)dependent->Length()) {
                    ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - seqloc in dendiag block > length of sequence!");
                    return;
                }
                masterToDependent[masterRes] = dependentRes;
                blockStructure[masterRes] = blockNum;
            }
        }
    }

    // unpack denseg alignment
    else if (seqAlign.GetSegs().IsDenseg()) {

        const CDense_seg& block = seqAlign.GetSegs().GetDenseg();

        if (!block.IsSetDim() && block.GetDim() != 2 ||
            block.GetIds().size() != 2 ||
            (int)block.GetStarts().size() != 2 * block.GetNumseg() ||
            (int)block.GetLens().size() != block.GetNumseg()) {
            ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - \n"
                "incorrect denseg block dimension");
            return;
        }

        // make sure identities of master and dependent sequences match in each block
        if ((masterFirst &&
                (!master->identifier->MatchesSeqId(block.GetIds().front().GetObject()) ||
                 !dependent->identifier->MatchesSeqId(block.GetIds().back().GetObject()))) ||
            (!masterFirst &&
                (!master->identifier->MatchesSeqId(block.GetIds().back().GetObject()) ||
                 !dependent->identifier->MatchesSeqId(block.GetIds().front().GetObject())))) {
            ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - \n"
                "mismatched Seq-id in denseg block");
            return;
        }

        // finally, actually unpack the data into the alignment vector
        CDense_seg::TStarts::const_iterator starts = block.GetStarts().begin();
        CDense_seg::TLens::const_iterator lens, le = block.GetLens().end();
        for (lens=block.GetLens().begin(); lens!=le; ++lens) {
            if (masterFirst) {
                masterRes = *(starts++);
                dependentRes = *(starts++);
            } else {
                dependentRes = *(starts++);
                masterRes = *(starts++);
            }
            if (masterRes != -1 && dependentRes != -1) { // skip gaps
                if ((masterRes + *lens - 1) >= master->Length() ||
                    (dependentRes + *lens - 1) >= dependent->Length()) {
                    ERRORMSG("MasterDependentAlignment::MasterDependentAlignment() - \n"
                        "seqloc in denseg block > length of sequence!");
                    return;
                }
                for (i=0; i<*lens; ++i) {
                    masterToDependent[masterRes + i] = dependentRes + i;
                    blockStructure[masterRes + i] = blockNum;
                }
                ++blockNum; // a "block" of a denseg is an aligned (non-gap) segment
            }
        }
    }

    //TESTMSG("got alignment for dependent gi " << dependent->identifier->gi);
}

END_SCOPE(Cn3D)
