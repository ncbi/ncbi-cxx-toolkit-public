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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

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
        alignments.push_back(new MasterSlaveAlignment(this, master, **a));
    TRACEMSG("number of alignments: " << alignments.size());
}

AlignmentSet::~AlignmentSet(void)
{
    if (newAsnAlignmentData) delete newAsnAlignmentData;
}

AlignmentSet * AlignmentSet::CreateFromMultiple(StructureBase *parent,
    const BlockMultipleAlignment *multiple, const vector < int >& rowOrder)
{
    // sanity check on the row order map
    map < int, int > rowCheck;
    for (int i=0; i<rowOrder.size(); ++i) rowCheck[rowOrder[i]] = i;
    if (rowOrder.size() != multiple->NRows() || rowCheck.size() != multiple->NRows() || rowOrder[0] != 0) {
        ERRORMSG("AlignmentSet::CreateFromMultiple() - bad row order vector");
        return NULL;
    }

    // create a single Seq-annot, with 'align' data that holds one Seq-align per slave
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
        int newRow;
        for (int row=1; row<multiple->NRows(); ++row, ++sa) {
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


///// MasterSlaveAlignment methods /////

MasterSlaveAlignment::MasterSlaveAlignment(StructureBase *parent, const Sequence *masterSequence,
    const ncbi::objects::CSeq_align& seqAlign) :
    StructureBase(parent), master(masterSequence), slave(NULL)
{
    // resize alignment and block vector
    masterToSlave.resize(master->Length(), -1);
    blockStructure.resize(master->Length(), -1);

    // find slave sequence for this alignment, and order (master or slave first)
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
        ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - couldn't find matching sequences; "
            << "both " << frontSeqId.AsFastaString() << " and "
            << backSeqId.AsFastaString() << " must be in the sequence list for this file!");
        return;
    } else {
        slave = *s;
    }

    int i, masterRes, slaveRes, blockNum = 0;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; ++d, ++blockNum) {
            const CDense_diag& block = d->GetObject();

            if (block.GetDim() != 2 || block.GetIds().size() != 2 || block.GetStarts().size() != 2) {
                ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                    "incorrect dendiag block dimensions");
                return;
            }

            // make sure identities of master and slave sequences match in each block
            if ((masterFirst &&
                    (!master->identifier->MatchesSeqId(block.GetIds().front().GetObject()) ||
                     !slave->identifier->MatchesSeqId(block.GetIds().back().GetObject()))) ||
                (!masterFirst &&
                    (!master->identifier->MatchesSeqId(block.GetIds().back().GetObject()) ||
                     !slave->identifier->MatchesSeqId(block.GetIds().front().GetObject())))) {
                ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - "
                    "mismatched Seq-id in dendiag block");
                return;
            }

            // finally, actually unpack the data into the alignment vector
            for (i=0; i<block.GetLen(); ++i) {
                if (masterFirst) {
                    masterRes = block.GetStarts().front() + i;
                    slaveRes = block.GetStarts().back() + i;
                } else {
                    masterRes = block.GetStarts().back() + i;
                    slaveRes = block.GetStarts().front() + i;
                }
                if (masterRes >= master->Length() || slaveRes >= slave->Length()) {
                    ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - "
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
            ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "incorrect denseg block dimension");
            return;
        }

        // make sure identities of master and slave sequences match in each block
        if ((masterFirst &&
                (!master->identifier->MatchesSeqId(block.GetIds().front().GetObject()) ||
                 !slave->identifier->MatchesSeqId(block.GetIds().back().GetObject()))) ||
            (!masterFirst &&
                (!master->identifier->MatchesSeqId(block.GetIds().back().GetObject()) ||
                 !slave->identifier->MatchesSeqId(block.GetIds().front().GetObject())))) {
            ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "mismatched Seq-id in denseg block");
            return;
        }

        // finally, actually unpack the data into the alignment vector
        CDense_seg::TStarts::const_iterator starts = block.GetStarts().begin();
        CDense_seg::TLens::const_iterator lens, le = block.GetLens().end();
        for (lens=block.GetLens().begin(); lens!=le; ++lens) {
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
                    ERRORMSG("MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                        "seqloc in denseg block > length of sequence!");
                    return;
                }
                for (i=0; i<*lens; ++i) {
                    masterToSlave[masterRes + i] = slaveRes + i;
                    blockStructure[masterRes + i] = blockNum;
                }
                ++blockNum; // a "block" of a denseg is an aligned (non-gap) segment
            }
        }
    }

    //TESTMSG("got alignment for slave gi " << slave->identifier->gi);
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.31  2004/03/15 17:17:56  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.30  2004/02/19 17:04:40  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.29  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.28  2003/06/12 14:21:11  thiessen
* when saving single-row alignment, make master-master alignment in asn
*
* Revision 1.27  2003/04/14 20:25:27  thiessen
* diagnose sequence not present in sequences list
*
* Revision 1.26  2003/04/08 21:54:52  thiessen
* fix for non-optional Dense-diag dim
*
* Revision 1.25  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.24  2002/07/01 15:30:20  thiessen
* fix for container type switch in Dense-seg
*
* Revision 1.23  2001/11/27 16:26:06  thiessen
* major update to data management system
*
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
*/
