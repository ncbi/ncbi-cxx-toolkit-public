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


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef list < const CSeq_align * > SeqAlignList;
typedef vector < CRef < CSeq_id > > SeqIdList;

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
    if (sid.IsLocal() && sid.GetLocal().IsStr()) {
        if (sid.GetLocal().GetStr() == seq->pdbID)
            return true;
        return false;
    }
    if (sid.IsGenbank() && sid.GetGenbank().IsSetAccession()) {
        if (sid.GetGenbank().GetAccession() == seq->accession)
            return true;
        return false;
    }
    if (sid.IsSwissprot() && sid.GetSwissprot().IsSetAccession()) {
        return (sid.GetSwissprot().GetAccession() == seq->accession);
    }
    ERR_POST(Error << "IsAMatch - can't match this type of Seq-id");
    return false;
}

AlignmentSet::AlignmentSet(SequenceSet *sequenceSet, const SeqAnnotList& seqAnnots) :
    master(NULL), status(CAV_ERROR_ALIGNMENTS)
{
    SeqAlignList seqaligns;

    // first, make a list of alignments
    SeqAnnotList::const_iterator n, ne = seqAnnots.end();
    for (n=seqAnnots.begin(); n!=ne; ++n) {

        if (!n->GetObject().GetData().IsAlign()) {
            ERR_POST(Error << "AlignmentSet::AlignmentSet() - confused by alignment data format");
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
        sequenceSet->master = master;
    }

    if (master) {
        ERR_POST(Info << "determined that master sequence is " << master->GetTitle());
    } else {
        ERR_POST(Error << "AlignmentSet::AlignmentSet() - couldn't determine which is master sequence");
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
            ERR_POST(Error << "Error parsing master/slave pairwise alignment #" << i);
            status = alignment->Status();
            return;
        }
        alignments.push_back(alignment);
    }

    ERR_POST(Info << "number of alignments: " << alignments.size());
    status = CAV_SUCCESS;
}

AlignmentSet::~AlignmentSet(void)
{
    for (int i=0; i<alignments.size(); ++i) delete alignments[i];
}


///// MasterSlaveAlignment methods /////

static bool SameSequence(const Sequence *s, const CBioseq& bioseq)
{
    CBioseq::TId::const_iterator i, ie = bioseq.GetId().end();
    for (i=bioseq.GetId().begin(); i!=ie; ++i) {
        if (IsAMatch(s, **i))
            return true;
    }
    return false;
}

MasterSlaveAlignment::MasterSlaveAlignment(
    const SequenceSet *sequenceSet,
    const Sequence *masterSequence,
    const CSeq_align& seqAlign) :
    master(masterSequence), slave(NULL), status(CAV_ERROR_PAIRWISE)
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

        if (SameSequence(*s, master->bioseqASN.GetObject())) continue;

        if (IsAMatch(*s, sids.back().GetObject())) {
            break;
        } else if (IsAMatch(*s, sids.front().GetObject())) {
            masterFirst = false;
            break;
        }
    }
    if (s == se) {
        // special case of master seq. aligned to itself
        if (IsAMatch(master, sids.back().GetObject()) && IsAMatch(master, sids.front().GetObject())) {
            slave = master;
        } else {
            ERR_POST(Error << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
                "couldn't find matching unaligned slave sequence");
            return;
        }
    } else {
        slave = *s;
    }

    int i, masterRes, slaveRes;

    // unpack dendiag alignment
    if (seqAlign.GetSegs().IsDendiag()) {

        CSeq_align::C_Segs::TDendiag::const_iterator d , de = seqAlign.GetSegs().GetDendiag().end();
        for (d=seqAlign.GetSegs().GetDendiag().begin(); d!=de; ++d) {
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
            for (i=0; i<block.GetLen(); ++i) {
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
                    ERR_POST(Critical << "MasterSlaveAlignment::MasterSlaveAlignment() - \n"
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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/05/08 10:56:20  thiessen
* better handling of repeated sequences
*
* Revision 1.3  2004/03/15 18:51:27  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.2  2003/06/02 16:06:41  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.1  2003/03/19 19:04:12  thiessen
* move again
*
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.7  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.6  2002/07/11 17:16:54  thiessen
* change for STL type in object lib
*
* Revision 1.5  2002/02/08 19:53:17  thiessen
* add annotation to text/HTML displays
*
* Revision 1.4  2001/07/12 21:32:12  thiessen
* update for swissprot accessions
*
* Revision 1.3  2001/01/29 18:13:33  thiessen
* split into C-callable library + main
*
* Revision 1.2  2001/01/22 15:55:11  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:23  thiessen
* initial checkin
*
*/
