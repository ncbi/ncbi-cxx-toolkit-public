static char const rcsid[] =
    "$Id$";
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file psi_pssm_input.cpp
 * Implementation of the concrete strategy to obtain PSSM input data for
 * PSI-BLAST.
 */

#include <ncbi_pch.hpp>
#include <iomanip>
#include <sstream>

// BLAST includes
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "../core/blast_psi_priv.h"

// Object includes
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/Seq_data.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef GAP_IN_ALIGNMENT
    /// Representation of GAP in Seq-align
#   define GAP_IN_ALIGNMENT     ((Uint4)-1)
#endif

//////////////////////////////////////////////////////////////////////////////
// Static function prototypes
static double 
s_GetEvalue(const CScore& score);

static double
s_GetLowestEvalue(const CDense_seg::TScores& scores);

static Uint1* 
s_ExtractSequenceFromSeqVector(CSeqVector& sv);

// End static function prototypes
//////////////////////////////////////////////////////////////////////////////

CPsiBlastInputData::CPsiBlastInputData(const Uint1* query,
                                       unsigned int query_length,
                                       CRef<CSeq_align_set> sset,
                                       CRef<CScope> scope,
                                       const PSIBlastOptions& opts)
{
    if ( !query ) {
        NCBI_THROW(CBlastException, eBadParameter, "NULL query");
    }

    if ( !sset || sset->Get().front()->GetDim() != 2) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Only 2-dimensional alignments are supported");
    }

    m_Query = new Uint1[query_length];
    memcpy((void*) m_Query, (void*) query, query_length);

    m_Scope.Reset(scope);
    m_SeqAlignSet.Reset(sset);
    m_Opts = opts;

    m_MsaDimensions.query_length = query_length;
    m_MsaDimensions.num_seqs = 0;
    m_Msa = NULL;

    const unsigned int kNumHits = m_SeqAlignSet->Get().size();
    m_ProcessHit.reserve(kNumHits);
    m_ProcessHit.assign(kNumHits, Uint1(0));
}

CPsiBlastInputData::~CPsiBlastInputData()
{
    delete [] m_Query;
    m_ProcessHit.clear();
    PSIMsaFree(m_Msa);
}

void
CPsiBlastInputData::Process()
{
    ASSERT(m_Query != NULL);

    // Update the number of aligned sequences
    m_MsaDimensions.num_seqs = x_CountAndSelectQualifyingAlignments();

    // Create multiple alignment data structure and populate with query
    // sequence
    m_Msa = PSIMsaNew(&m_MsaDimensions);
    if ( !m_Msa ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Multiple alignment data "
                   "structure");
    }

    x_CopyQueryToMsa();
    x_ExtractAlignmentData();
    //x_ExtractAlignmentDataUseBestAlign();
}

unsigned int
CPsiBlastInputData::x_CountAndSelectQualifyingAlignments()
{
    unsigned int retval = 0;
    unsigned int seq_index = 0;

    // For each discontinuous Seq-align corresponding to each query-subj pair
    // (i.e.: for each hit)
    ITERATE(CSeq_align_set::Tdata, i, m_SeqAlignSet->Get()) {
        ASSERT((*i)->GetSegs().IsDisc());

        // For each HSP of this query-subj pair
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*i)->GetSegs().GetDisc().Get()) {
            
            // Look for HSP with score less than inclusion_ethresh
            double e = s_GetLowestEvalue((*hsp_itr)->GetScore());
            if (e < m_Opts.inclusion_ethresh) {
                retval++;
                m_ProcessHit[seq_index] = 1U;
                break;
            }
        }
        seq_index++;
    }

    ASSERT(retval <= m_ProcessHit.size());
    ASSERT(seq_index == m_ProcessHit.size());
    ASSERT(m_ProcessHit.size() == m_SeqAlignSet->Get().size());

    return retval;
}

unsigned int
CPsiBlastInputData::GetNumAlignedSequences() const
{
    if (m_MsaDimensions.num_seqs == 0) {
        NCBI_THROW(CBlastException, eInternal, "Number of aligned sequences"
                   "not calculated yet");
    }
    return m_MsaDimensions.num_seqs;
}

inline PSIMsa* 
CPsiBlastInputData::GetData()
{ 
    return m_Msa;
}

inline unsigned char*
CPsiBlastInputData::GetQuery()
{
    return m_Query;
}

inline unsigned int
CPsiBlastInputData::GetQueryLength()
{
    return m_MsaDimensions.query_length;
}

inline const PSIBlastOptions*
CPsiBlastInputData::GetOptions()
{
    return &m_Opts;
}

#if 0
void
CPsiBlastInputData::x_ExtractAlignmentDataUseBestAlign()
{
    TSeqPos seq_index = 1; // Query sequence already processed

    // Note that in this implementation the m_ProcessHit vector is irrelevant
    // because we assume the Seq-align contains only those sequences selected
    // by the user (www-psi-blast). This could also be implemented by letting
    // the calling code populate a vector like m_ProcessHit or specifying a
    // vector of Seq-ids.
    ITERATE(list< CRef<CSeq_align> >, itr, m_SeqAlignSet->Get()) {

        const CSeq_align::C_Segs::TDisc::Tdata& hsp_list =
            (*itr)->GetSegs().GetDisc().Get();
        CSeq_align::C_Segs::TDisc::Tdata::const_iterator best_alignment;
        double min_evalue = numeric_limits<double>::max();

        // Search for the best alignment among all HSPs corresponding to this
        // query-subject pair (hit)
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr, hsp_list) {

            // Note: Std-seg can be converted to Denseg, will need 
            // conversion from Dendiag to Denseg too
            if ( !(*hsp_itr)->GetSegs().IsDenseg() ) {
                NCBI_THROW(CBlastException, eNotSupported, 
                           "Segment type not supported");
            }

            double evalue = s_GetLowestEvalue((*hsp_itr)->GetScore());
            if (evalue < min_evalue) {
                best_alignment = hsp_itr;
                min_evalue = evalue;
            }
        }
        ASSERT(best_alignment != hsp_list.end());

        x_ProcessDenseg((*best_alignment)->GetSegs().GetDenseg(), 
                        seq_index, min_evalue);

        seq_index++;

    }

    ASSERT(seq_index == GetNumAlignedSequences()+1);
}
#endif

void
CPsiBlastInputData::x_CopyQueryToMsa()
{
    ASSERT(m_Msa);

    for (unsigned int i = 0; i < GetQueryLength(); i++) {
        m_Msa->data[kQueryIndex][i].letter = m_Query[i];
        m_Msa->data[kQueryIndex][i].is_aligned = true;
    }
}

void
CPsiBlastInputData::x_ExtractAlignmentData()
{
    // Index into m_ProcessHit vector
    unsigned int seq_index = 0;
    // Index into multiple sequence alignment structure, query sequence 
    // already processed
    unsigned int msa_index = kQueryIndex + 1;  

    // For each hit...
    ITERATE(CSeq_align_set::Tdata, itr, m_SeqAlignSet->Get()) {

        if ( !m_ProcessHit[seq_index++] ) {
            continue;
        }

        // for each HSP...
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*itr)->GetSegs().GetDisc().Get()) {

            // Note: Std-seg (for tblastn results) can be converted to 
            // Dense-seg, but this will need conversion from Dense-diag to 
            // Dense-seg too
            if ( !(*hsp_itr)->GetSegs().IsDenseg() ) {
                NCBI_THROW(CBlastException, eNotSupported, 
                           "Segment type not supported");
            }

            double evalue = s_GetLowestEvalue((*hsp_itr)->GetScore());
            // ... below the e-value inclusion threshold
            if (evalue < m_Opts.inclusion_ethresh) {
                ASSERT(msa_index < GetNumAlignedSequences() + 1);
                x_ProcessDenseg((*hsp_itr)->GetSegs().GetDenseg(), msa_index,
                                evalue);
            }
        }
        msa_index++;
    }

    ASSERT(seq_index == m_ProcessHit.size());
}

#if DEBUG
static Char getRes(Char input)
{
    switch(input) 
      {
      case 0: return('-');
      case 1: return('A');
      case 2: return('B');
      case 3: return('C');
      case 4: return('D');
      case 5: return('E');
      case 6: return('F');
      case 7: return('G');
      case 8: return('H');
      case 9: return('I');
      case 10: return('K');
      case 11: return('L');
      case 12: return('M');
      case 13: return('N');
      case 14: return('P');
      case 15: return('Q');
      case 16: return('R');
      case 17: return('S');
      case 18: return('T');
      case 19: return('V');
      case 20: return('W');
      case 21: return('X');
      case 22: return('Y');
      case 23: return('Z');
      case 24: return('U');
      case 25: return('*');
      default: return('?');
    }
}
#endif

void
CPsiBlastInputData::x_ProcessDenseg(const CDense_seg& denseg, 
                                    unsigned int msa_index, 
                                    double e_value)
{
    ASSERT(denseg.GetDim() == 2);

    const Uint1 GAP = AMINOACID_TO_NCBISTDAA[(Uint1)'-'];
    const vector<TSignedSeqPos>& starts = denseg.GetStarts();
    const vector<TSeqPos>& lengths = denseg.GetLens();
    TSeqPos query_index = 0;        // index into starts vector
    TSeqPos subj_index = 1;         // index into starts vector
    TSeqPos subj_seq_idx = 0;       // index into subject sequence buffer

    // Get the portion of the subject sequence corresponding to this Dense-seg
    TSeqPair seq = x_GetSubjectSequence(denseg, *m_Scope);

    // if this isn't available, reset its corresponding row in the multiple
    // sequence alignment structure
    if (seq.first.get() == NULL) {
        for (unsigned int i = 0; i < GetQueryLength(); i++) {
            m_Msa->data[msa_index][i].letter = (unsigned char) -1;
            m_Msa->data[msa_index][i].is_aligned = false;
        }
        //m_Msa->use_sequences[msa_index] = false;
        // FIXME: this needs to withdraw the sequence!
        return;
    }
    /*
{
    CNcbiDiag log;
    log << "Denseg " << (int)denseg.GetNumseg() 
        << " sgmts" << Endm;
}
*/

    // Iterate over all segments
    for (int segmt_idx = 0; segmt_idx < denseg.GetNumseg(); segmt_idx++) {

        TSeqPos query_offset = starts[query_index];
        TSeqPos subject_offset = starts[subj_index];
        /*
{
    CNcbiDiag log;
    if (denseg.GetNumseg() == 29) 
    log << "Sgmt=" << (int)segmt_idx 
        << " Q=" << (int)query_offset << " S=" << (int)subject_offset
        << " (arrayoffset=" 
        << (int)( (((int)subject_offset)==-1)? -1 : subj_seq_idx) 
        << ")"
        << " L=" << (int)lengths[segmt_idx] 
        << " eval=" << (double)e_value
        << Endm;
}
*/

        // advance the query and subject indices for next iteration
        query_index += denseg.GetDim();
        subj_index += denseg.GetDim();

        if (query_offset == GAP_IN_ALIGNMENT) {

            // gap in query, just skip residues on subject sequence
            subj_seq_idx += lengths[segmt_idx];
            continue;

        } else if (subject_offset == GAP_IN_ALIGNMENT) {

            // gap in subject, initialize appropriately
            for (TSeqPos i = 0; i < lengths[segmt_idx]; i++) {
                PSIMsaCell& msa_cell = m_Msa->data[msa_index][query_offset++];
                if ( !msa_cell.is_aligned ) {
                    msa_cell.letter = GAP;
                    msa_cell.is_aligned = true;
                }
            }

        } else {

            // Aligned segments without any gaps
            for (TSeqPos i = 0; i < lengths[segmt_idx]; i++, subj_seq_idx++) {
                PSIMsaCell& msa_cell =
                    m_Msa->data[msa_index][query_offset++];
                if ( !msa_cell.is_aligned ) {
                    msa_cell.letter = seq.first.get()[subj_seq_idx];
                    msa_cell.is_aligned = true;
    /*
if (msa_index == 1 && (query_offset-1) == 7320) {
    CNcbiDiag log;
    log << "Assigning " 
        << getRes((char) seq.first.get()[subj_seq_idx] )
        << " query_idx " << query_offset-1
        << " subj_idx " <<
        subj_seq_idx << " evalue " << pos_desc.e_value << Endm;
    for(unsigned int in = 0; in < lengths[segmt_idx]; in++) {
        log << getRes((char)m_Msa->desc_matrix[0][(query_offset-1+in)].letter);
    }
    log << Endm;
    for(unsigned int in = 0; in < lengths[segmt_idx]; in++) {
        log << getRes((char)seq.first.get()[subj_seq_idx-1+in]);
    }
    log << Endm;
}
    */
                }
            }
        }

    }

}

CPsiBlastInputData::TSeqPair 
CPsiBlastInputData::x_GetSubjectSequence(const CDense_seg& ds, CScope& scope) 
{
    ASSERT(ds.GetDim() == 2);
    Uint1* retval = NULL;
    TSeqPos subjlen = 0;                    // length of the return value
    TSeqPos subj_start = kInvalidSeqPos;    // start of subject alignment
    bool subj_start_found = false;
    TSeqPos subj_index = 1;                 // index into starts vector

    const vector<TSignedSeqPos>& starts = ds.GetStarts();
    const vector<TSeqPos>& lengths = ds.GetLens();

    for (int i = 0; i < ds.GetNumseg(); i++) {

        if (starts[subj_index] != (TSignedSeqPos)GAP_IN_ALIGNMENT) {
            if ( !subj_start_found ) {
                subj_start = starts[subj_index];
                subj_start_found = true;
            }
            subjlen += lengths[i];
        }

        subj_index += ds.GetDim();
    }
    ASSERT(subj_start_found);

    CSeq_loc seqloc;
    seqloc.SetInt().SetFrom(subj_start);
    seqloc.SetInt().SetTo(subj_start+subjlen-1);
    seqloc.SetInt().SetStrand(eNa_strand_unknown);
    seqloc.SetInt().SetId(const_cast<CSeq_id&>(*ds.GetIds().back()));

    CBioseq_Handle bh;
    try {
        bh = scope.GetBioseqHandle(seqloc);
        CSeqVector sv = bh.GetSequenceView(seqloc,
                                           CBioseq_Handle::eViewConstructed,
                                           CBioseq_Handle::eCoding_Ncbi);
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        retval = s_ExtractSequenceFromSeqVector(sv);
        /*
        {
            CNcbiDiag log;
            log << seqloc.GetInt().GetId().AsFastaString()
                << " " << subj_start << "-" << subj_start+subjlen-1
                << Endm;
            for (unsigned int i = 0; i < subjlen; i++) {
                log << getRes((char)retval[i]);
            }
        }
        */
    } catch (const CException& e) {
        subjlen = 0;
        retval = NULL;
        ERR_POST(Warning << "Failed to retrieve sequence " <<
                 seqloc.GetInt().GetId().AsFastaString());
    }

    return make_pair(AutoPtr<Uint1, ArrayDeleter<Uint1> >(retval), subjlen);
}

//////////////////////////////////////////////////////////////////////////////
// Static function definitions

// Returns the evalue from this score object
static double
s_GetEvalue(const CScore& score)
{
    string score_type = score.GetId().GetStr();
    if (score.GetValue().IsReal() && 
       (score_type == "e_value" || score_type == "sum_e")) {
        return score.GetValue().GetReal();
    }
    return numeric_limits<double>::max();
}

static double 
s_GetLowestEvalue(const CDense_seg::TScores& scores)
{
    double retval = numeric_limits<double>::max();
    double tmp;

    ITERATE(CDense_seg::TScores, i, scores) {
        if ( (tmp = s_GetEvalue(**i)) < retval) {
            retval = tmp;
        }
    }
    return retval;
}

static Uint1* 
s_ExtractSequenceFromSeqVector(CSeqVector& sv) 
{
    Uint1* retval = NULL;

    try {
        retval = new Uint1[sv.size()];
    } catch (const std::bad_alloc&) {
        ostringstream os;
        os << "Could not allocate " << sv.size() << " bytes";
        NCBI_THROW(CBlastException, eOutOfMemory, os.str());
    }
    for (TSeqPos i = 0; i < sv.size(); i++) {
        retval[i] = sv[i];
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/08/04 20:24:53  camacho
 * Renaming of core PSSM structures and removal of unneeded fields.
 *
 * Revision 1.3  2004/08/02 13:29:53  camacho
 * + implementation query sequence methods
 *
 * Revision 1.2  2004/07/29 21:00:22  ucko
 * Tweak call to assign() in constructor to avoid triggering an
 * inappropriate template (that takes two iterators) on some compilers.
 *
 * Revision 1.1  2004/07/29 17:55:05  camacho
 * Initial revision
 *
 * ===========================================================================
 */
