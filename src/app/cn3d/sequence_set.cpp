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
* Revision 1.1  2000/08/27 18:52:22  thiessen
* extract sequence information
*
* ===========================================================================
*/

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>

#include "cn3d/sequence_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

SequenceSet::SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries) :
    StructureBase(parent)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; s++) {
        if (s->GetObject().IsSeq()) {
            const Sequence *sequence = new Sequence(this, s->GetObject().GetSeq());
            sequences.push_back(sequence);
        } else { // Bioseq-set
            CBioseq_set::TSeq_set::const_iterator q, qe = s->GetObject().GetSet().GetSeq_set().end();
            for (q=s->GetObject().GetSet().GetSeq_set().begin(); q!=qe; q++) {
                if (q->GetObject().IsSeq()) {

                    // only store amino acid sequences
                    if (q->GetObject().GetSeq().GetInst().GetMol() != CSeq_inst::eMol_aa)
                        continue;

                    const Sequence *sequence = new Sequence(this, q->GetObject().GetSeq());
                    sequences.push_back(sequence);
                } else {
                    ERR_POST(Critical << "SequenceSet::SequenceSet() - can't handle nested Bioseq-set");
                    return;
                }
            }
        }
    }
}

const int Sequence::NOT_SET = -1;

Sequence::Sequence(StructureBase *parent, const ncbi::objects::CBioseq& bioseq) :
    StructureBase(parent), gi(NOT_SET), pdbChain(NOT_SET)
{
    // get Seq-id info
    CBioseq::TId::const_iterator s, se = bioseq.GetId().end();
    for (s=bioseq.GetId().begin(); s!=se; s++) {
        if (s->GetObject().IsGi()) {
            gi = s->GetObject().GetGi();
        } else if (s->GetObject().IsPdb()) {
            pdbID = s->GetObject().GetPdb().GetMol().Get();
            if (s->GetObject().GetPdb().IsSetChain())
                pdbChain = s->GetObject().GetPdb().GetChain();
        }
    }
    TESTMSG("sequence gi " << gi << ", PDB " << pdbID << " chain " << pdbChain);

    // get sequence string
    if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_raw && bioseq.GetInst().IsSetSeq_data()) {
        if (bioseq.GetInst().GetSeq_data().IsNcbieaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetNcbieaa().Get();
        } else if (bioseq.GetInst().GetSeq_data().IsIupacaa()) {
            sequenceString = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();
        } else {
            ERR_POST(Critical << "Sequence::Sequence() - confused by sequence string format");
            return;
        }
        if (bioseq.GetInst().IsSetLength() && bioseq.GetInst().GetLength() != sequenceString.length()) {
            ERR_POST(Critical << "Sequence::Sequence() - sequence string length mismatch");
            return;
        }
    } else {
        ERR_POST(Critical << "Sequence::Sequence() - confused by sequence representation");
        return;
    }
    TESTMSG(sequenceString);
}

END_SCOPE(Cn3D)

