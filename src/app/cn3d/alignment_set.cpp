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
* Revision 1.1  2000/08/28 18:52:41  thiessen
* start unpacking alignments
*
* ===========================================================================
*/

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>

#include "cn3d/alignment_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

typedef LIST_TYPE < const CSeq_align * > SeqAlignList;

AlignmentSet::AlignmentSet(StructureBase *parent, const SeqAnnotList& seqAnnots) :
    StructureBase(parent), master(NULL)
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

        CSeq_annot::C_Data::TAlign::const_iterator
            a, ae = n->GetObject().GetData().GetAlign().end();
        for (a=n->GetObject().GetData().GetAlign().begin(); a!=ae; a++) {

            // verify this is a type of alignment we can deal with
            if (a->GetObject().GetType() != CSeq_align::eType_partial ||
                !a->GetObject().IsSetDim() || a->GetObject().GetDim() != 2) {
                ERR_POST("AlignmentSet::AlignmentSet() - confused by alignment type");
                return;
            }

            seqaligns.push_back(a->GetPointer());
        }
    }

    // figure out which sequence is the master
    if (parentSet->sequenceSet->master) master = parentSet->sequenceSet->master;

    if (!master) {
        ERR_POST(Error << "AlignmentSet::AlignmentSet() - couldn't determine which is master sequence");
        return;
    }

    // then make an alignment from each Seq-align
    SeqAlignList::const_iterator s, se = seqaligns.end();
    for (s=seqaligns.begin(); s!=se; s++) {
        const MasterSlaveAlignment *alignment =
            new MasterSlaveAlignment(this, master, **s);
        alignments.push_back(alignment);
    }
}

static bool IsAMatch(const Sequence *seq, const CSeq_id& sid)
{
    if (sid.IsGi()) {
        if (sid.GetGi() == seq->gi)
            return true;
        return false;
    }
    if (sid.IsPdb()) {
        if (sid.GetPdb().GetMol().Get() == seq->pdbID) {
            if (sid.GetPdb().IsSetChain()) {
                if (sid.GetPdb().GetChain() != seq->pdbChain) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    ERR_POST(Error << "VerifyMatch - can't match this type of Seq-id");
    return false;
}

MasterSlaveAlignment::MasterSlaveAlignment(StructureBase *parent,
    const Sequence *masterSequence,
    const ncbi::objects::CSeq_align& seqAlign) :
    StructureBase(parent), master(masterSequence), slave(NULL)
{
    // make sure we can deal with this type of alignment
    if (!seqAlign.GetSegs().IsDendiag() && !seqAlign.GetSegs().IsDenseg()) {
        ERR_POST(Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
            "unknown alignment data type");
        return;
    }

    // resize alignment vector
    masterToSlave.resize(master->sequenceString.size(), -1);

    SequenceSet::SequenceList::const_iterator
        s = parentSet->sequenceSet->sequences.begin(),
        se = parentSet->sequenceSet->sequences.end();

    // look for unaligned slave sequence, see if it matches this alignment
    typedef list< CRef< CSeq_id > > SeqIdList;
    const SeqIdList& sids = seqAlign.GetSegs().IsDendiag() ?
        seqAlign.GetSegs().GetDendiag().front()->GetIds() :
        seqAlign.GetSegs().GetDenseg().GetIds();
        
    bool masterFirst = true;
    for (; s!=se; s++) {
        if ((*s)->alignment != NULL || *s == master) continue;
        if (IsAMatch(*s, sids.back().GetObject())) {
            break;
        } else if (IsAMatch(*s, sids.front().GetObject())) {
            masterFirst = false;
            break;
        }
    }
    if (s == se) {
        ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
            "couldn't find matching unaligned slave sequence");
        return;
    }
    slave = *s;

    int i, masterRes, slaveRes;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; d++) {
            const CDense_diag& block = d->GetObject();
                
            if ((block.IsSetDim() && block.GetDim() != 2) ||
                block.GetIds().size() != 2 ||
                block.GetStarts().size() != 2) {
                ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                    "incorrect dendiag block dimension");
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
                    "mismatched Seq-id in Dendiag block");
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
                if (masterRes >= master->sequenceString.size() ||
                    slaveRes >= slave->sequenceString.size()) {
                    ERR_POST(Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                        "seqloc in dendiag block > length of sequence!");
                    return;
                }
                masterToSlave[masterRes] = slaveRes;
            }
        }
    }

    // unpack denseg alignment
    else if (seqAlign.GetSegs().IsDenseg()) {
    }

    // mark sequence as aligned
    (const_cast<Sequence*>(slave))->alignment = this;
    TESTMSG("got alignment for slave gi " << slave->gi);
}

END_SCOPE(Cn3D)

