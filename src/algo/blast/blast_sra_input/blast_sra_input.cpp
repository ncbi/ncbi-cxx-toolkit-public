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
 * Author:  Greg Boratyn
 *
 */

/** @file blast_asn1_input.cpp
 * Convert ASN1-formatted files into blast sequence input
 */

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>

#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


CSraInputSource::CSraInputSource(const vector<string>& accessions,
                                 TSeqPos num_seqs,
                                 bool check_for_paires,
                                 bool validate)
    : m_NumSeqsInBatch(num_seqs),
      m_IsPaired(check_for_paires),
      m_Validate(validate)
{
    // initialize SRA access
    CVDBMgr mgr;
    m_SraDb.reset(new CCSraDb(mgr, accessions[0]));
    m_It.reset(new CCSraShortReadIterator(*m_SraDb));
}



void
CSraInputSource::GetNextNumSequences(CBioseq_set& bioseq_set,
                                     TSeqPos /* num_seqs */)
{
    //Pre-allocate and initialize Seq_entry objects for the batch of queries
    // to be read.
    m_Entries.clear();

    // +1 in case we need to read one more sequence so that a pair is not
    // broken
    m_Entries.resize(m_NumSeqsInBatch + 1);

    // allocate seq_entry objects, they will be reused to minimize
    // time spent on memory management
    for (TSeqPos i=0;i < m_NumSeqsInBatch + 1;i++) {
        m_Entries[i].Reset(new CSeq_entry);
        m_Entries[i]->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
        m_Entries[i]->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    }

    // read sequences
    m_Index = 0;
    if (m_IsPaired) {
        x_ReadPairs(bioseq_set);
    }
    else {
        for (; *m_It && m_Index < (int)m_NumSeqsInBatch; ++(*m_It)) {
            x_ReadOneSeq(bioseq_set);
        }
    }

    // detach CRefs in m_Entries from objects in bioseq_set for thread safety
    m_Entries.clear();
}


int
CSraInputSource::x_ReadOneSeq(CBioseq_set& bioseq_set)
{
    _ASSERT(m_It.get() && *m_It);
    if (m_It->IsTechnicalRead()) {
        return -1;
    }
    CTempString sequence = m_It->GetReadData();
    if (!m_Validate || x_ValidateSequence(sequence.data(), sequence.length())) {
        CBioseq& bioseq = m_Entries[m_Index]->SetSeq();
        bioseq.SetId().clear();
        bioseq.SetId().push_back(m_It->GetShortSeq_id());
        bioseq.SetDescr().Reset();
        bioseq.SetInst().SetLength(sequence.length());
        bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna((string)sequence));
        bioseq_set.SetSeq_set().push_back(m_Entries[m_Index]);

        m_Index++;
        return m_Index - 1;
    }

    return -1;
}


void
CSraInputSource::x_ReadPairs(CBioseq_set& bioseq_set)
{
    CRef<CSeqdesc> seqdesc(new CSeqdesc);
    seqdesc->SetUser().SetType().SetStr("Mapping");
    seqdesc->SetUser().AddField("has_pair", true);

    TVDBRowId spot = -1;
    while (*m_It && m_Index < (int)m_NumSeqsInBatch) {
        // read one sequence and rember spot id
        spot = m_It->GetSpotId();
        int index1 = x_ReadOneSeq(bioseq_set);
 
        // move to the next sequence
        ++(*m_It);
        if (!(*m_It)) {
            break;
        }
        
        // if the last sequence was accepted and the current sequence has
        // the same spot id, read current sequence as a mate, otherwise
        // continue the loop
        if (index1 >= 0 && m_It->GetSpotId() == spot) {
            int index2 = x_ReadOneSeq(bioseq_set);
            if (index2 > 0) {
                CBioseq& bioseq = m_Entries[index1]->SetSeq();
                bioseq.SetDescr().Set().push_back(seqdesc);
            }
            ++(*m_It);
        }
    }
}


bool CSraInputSource::x_ValidateSequence(const char* sequence, int length)
{
    const char* s = sequence;
    const int kNBase = (int)'N';
    const double kMaxFractionAmbiguousBases = 0.5;
    int num = 0;
    for (int i=0;i < length;i++) {
        num += (toupper((int)s[i]) == kNBase);
    }

    if ((double)num / length > kMaxFractionAmbiguousBases) {
        return false;
    }

    int entropy = FindDimerEntropy(sequence, length);
    return entropy > 16;

}

END_SCOPE(blast)
END_NCBI_SCOPE
