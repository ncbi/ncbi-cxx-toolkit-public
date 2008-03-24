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
*      Classes to hold sets of master/slave pairwise alignments
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/seq/Bioseq.hpp>

#include <map>

#include <objtools/cddalignview/cav_alignset.hpp>
#include <objtools/cddalignview/cav_seqset.hpp>
#include <objtools/cddalignview/cddalignview.h>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_CAV_Alnset


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef list < const CSeq_align * > SeqAlignList;
typedef vector < CRef < CSeq_id > > SeqIdList;

AlignmentSet::AlignmentSet(SequenceSet *sequenceSet, const SeqAnnotList& seqAnnots) :
    status(CAV_ERROR_ALIGNMENTS), master(NULL)
{
    SeqAlignList seqaligns;

    // first, make a list of alignments
    SeqAnnotList::const_iterator n, ne = seqAnnots.end();
    for (n=seqAnnots.begin(); n!=ne; ++n) {

        if (!n->GetObject().GetData().IsAlign()) {
            ERR_POST_X(1, Error << "AlignmentSet::AlignmentSet() - confused by alignment data format");
            return;
        }

        CSeq_annot::C_Data::TAlign::const_iterator
            a, ae = n->GetObject().GetData().GetAlign().end();
        for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; ++a) {

            // verify this is a type of alignment we can deal with
            if (!(a->GetObject().GetType() != CSeq_align::eType_partial ||
                  a->GetObject().GetType() != CSeq_align::eType_diags) ||
                !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2 ||
                (!a->GetObject().GetSegs().IsDendiag() && !a->GetObject().GetSegs().IsDenseg())) {
                ERR_POST_X(2, Error << "AlignmentSet::AlignmentSet() - confused by alignment type");
                return;
            }

            seqaligns.push_back(a->GetPointer());
        }
    }
    if (seqaligns.size() == 0) {
        ERR_POST_X(3, Error << "AlignmentSet::AlignmentSet() - no valid Seq-aligns present");
        return;
    }

    // figure out which sequence is the master
    // if there aren't any structures, then the first sequence of each pair must be the same,
    // and is the master
    else {
        SeqAlignList::const_iterator a, ae = seqaligns.end();
        for (a=seqaligns.begin(); a!=ae; ++a) {
            const SeqIdList& sids = (*a)->GetSegs().IsDendiag() ?
                (*a)->GetSegs().GetDendiag().front()->GetIds() :
                (*a)->GetSegs().GetDenseg().GetIds();
            if (!master) {
                SequenceSet::SequenceList::const_iterator
                    s, se = sequenceSet->sequences.end();
                for (s=sequenceSet->sequences.begin(); s!=se; ++s) {
                    if ((*s)->Matches(sids.front().GetObject())) {
                        master = *s;
                        break;
                    }
                }
            }
            if (master && !master->Matches(sids.front().GetObject())) {
                ERR_POST_X(4, Error << "AlignmentSet::AlignmentSet() - master must be first sequence of every alignment");
                return;
            }
        }
        sequenceSet->master = master;
    }

    if (master) {
        ERR_POST_X(5, Info << "determined that master sequence is " << master->GetTitle());
    } else {
        ERR_POST_X(6, Error << "AlignmentSet::AlignmentSet() - couldn't determine which is master sequence");
        return;
    }

    // then make an alignment from each Seq-align; for any sequence that has structure, make
    // sure that that Sequence object only appears once in the alignment
    SeqAlignList::const_iterator s, se = seqaligns.end();
    int i = 1;
    for (s=seqaligns.begin(); s!=se; ++s, ++i) {
        const MasterSlaveAlignment *alignment =
            new MasterSlaveAlignment(sequenceSet, master, **s);
        if (!alignment || alignment->Status() != CAV_SUCCESS) {
            ERR_POST_X(7, Error << "Error parsing master/slave pairwise alignment #" << i);
            status = alignment->Status();
            return;
        }
        alignments.push_back(alignment);
    }

    ERR_POST_X(8, Info << "number of alignments: " << alignments.size());
    status = CAV_SUCCESS;
}

AlignmentSet::~AlignmentSet(void)
{
    for (unsigned int i=0; i<alignments.size(); ++i) delete alignments[i];
}


///// MasterSlaveAlignment methods /////

MasterSlaveAlignment::MasterSlaveAlignment(
    const SequenceSet *sequenceSet,
    const Sequence *masterSequence,
    const CSeq_align& seqAlign) :
    status(CAV_ERROR_PAIRWISE), master(masterSequence), slave(NULL)
{
    // resize alignment and block vector
    masterToSlave.resize(master->Length(), -1);

    SequenceSet::SequenceList::const_iterator
        s = sequenceSet->sequences.begin(),
        se = sequenceSet->sequences.end();

    // find slave sequence for this alignment, and order (master or slave first)
    const SeqIdList& sids = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds() :
        seqAlign.GetSegs().GetDenseg().GetIds();

    bool masterFirst = true;
    for (; s!=se; ++s) {

        if ((*s)->Matches(master->bioseqASN->GetId())) continue;

        if ((*s)->Matches(sids.back().GetObject())) {
            break;
        } else if ((*s)->Matches(sids.front().GetObject())) {
            masterFirst = false;
            break;
        }
    }
    if (s == se) {
        // special case of master seq. aligned to itself
        if (master->Matches(sids.back().GetObject()) && master->Matches(sids.front().GetObject())) {
            slave = master;
        } else {
            ERR_POST_X(9, Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                                   "couldn't find matching unaligned slave sequence");
            return;
        }
    } else {
        slave = *s;
    }

    unsigned int i;
    int masterRes, slaveRes;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; ++d) {
            const CDense_diag& block = d->GetObject();

            if (!block.IsSetDim() || block.GetDim() != 2 ||
                block.GetIds().size() != 2 ||
                block.GetStarts().size() != 2) {
                ERR_POST_X(10, Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                                        "incorrect dendiag block dimensions");
                return;
            }

            // make sure identities of master and slave sequences match in each block
            if ((masterFirst &&
                    (!master->Matches(block.GetIds().front().GetObject()) ||
                     !slave->Matches(block.GetIds().back().GetObject()))) ||
                (!masterFirst &&
                    (!master->Matches(block.GetIds().back().GetObject()) ||
                     !slave->Matches(block.GetIds().front().GetObject())))) {
                ERR_POST_X(11, Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
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
                if (masterRes >= (int) master->Length() || slaveRes >= (int) slave->Length()) {
                    ERR_POST_X(12, Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                                               "seqloc in dendiag block > length of sequence!");
                    return;
                }
                masterToSlave[masterRes] = slaveRes;
            }
        }
    }

    // unpack denseg alignment
    else if (seqAlign.GetSegs().IsDenseg()) {

        const CDense_seg& block = seqAlign.GetSegs().GetDenseg();

        if (!block.IsSetDim() && block.GetDim() != 2 ||
            block.GetIds().size() != 2 ||
            block.GetStarts().size() != ((unsigned int ) 2 * block.GetNumseg()) ||
            block.GetLens().size() != ((unsigned int ) block.GetNumseg())) {
            ERR_POST_X(13, Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                                    "incorrect denseg block dimension");
            return;
        }

        // make sure identities of master and slave sequences match in each block
        if ((masterFirst &&
                (!master->Matches(block.GetIds().front().GetObject()) ||
                 !slave->Matches(block.GetIds().back().GetObject()))) ||
            (!masterFirst &&
                (!master->Matches(block.GetIds().back().GetObject()) ||
                 !slave->Matches(block.GetIds().front().GetObject())))) {
            ERR_POST_X(14, Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
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
                    ERR_POST_X(15, Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                                               "seqloc in denseg block > length of sequence!");
                    return;
                }
                for (i=0; i<*lens; ++i)
                    masterToSlave[masterRes + i] = slaveRes + i;
            }
        }
    }

    status = CAV_SUCCESS;
}

END_NCBI_SCOPE
