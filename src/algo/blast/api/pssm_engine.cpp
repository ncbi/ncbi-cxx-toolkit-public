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

/** @file blast_psi_cxx.cpp
 * Implementation of the C++ API for the PSI-BLAST PSSM generation engine.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_psi.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <corelib/ncbi_limits.hpp>

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

// Core includes
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_encoding.h>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <sstream>

#include "../core/blast_psi_priv.h"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

//////////////////////////////////////////////////////////////////////////////
// Static function prototypes
static double 
s_GetEvalue(const CScore& score);

static Uint1* 
s_ExtractSequenceFromSeqVector(CSeqVector& sv);

static double
s_GetLowestEvalue(const CDense_seg::TScores& scores);

// Debugging functions
static void 
s_DBG_printSequence(const Uint1* seq, TSeqPos len, ostream& out, 
                    bool show_markers = true, TSeqPos chars_per_line = 80);

// End static function prototypes
//////////////////////////////////////////////////////////////////////////////


CScoreMatrixBuilder::CScoreMatrixBuilder(const Uint1* query,
                                         TSeqPos query_sz,
                                         CRef<CSeq_align_set> sset,
                                         CRef<CScope> scope,
                                         const PSIBlastOptions& opts)
{
    if (!query) {
        NCBI_THROW(CBlastException, eBadParameter, "NULL query");
    }

    if (!sset || sset->Get().front()->GetDim() != 2) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Only 2-dimensional alignments are supported");
    }

    m_Scope.Reset(scope);
    m_SeqAlignSet.Reset(sset);
    m_Opts = const_cast<PSIBlastOptions&>(opts);

    m_PssmDimensions.query_sz = query_sz;
    m_PssmDimensions.num_seqs = sset->Get().size();
    m_AlignmentData.reset(PSIAlignmentDataNew(query, &m_PssmDimensions));
}

void
CScoreMatrixBuilder::Run()
{
    // this method should just extract information from Seq-align set and
    // populate alignment data structure and delegate everything else to core
    // PSSM library
    SelectSequencesForProcessing();
    ExtractAlignmentData();

    // @todo Will need a function to populate the score block for psiblast
    // add a new program type psiblast
    //PsiMatrix* retval = PSICreatePSSM(m_AlignmentData.get(),
                                      //&m_Opts, sbp, NULL);

    // @todo populate Score-mat ASN.1 structure with retval and sbp contents
    
}

// Selects those sequences which have an evalue lower than the inclusion
// threshold in PsiInfo structure
void
CScoreMatrixBuilder::x_SelectByEvalue()
{
    TSeqPos seq_index = 0;
    m_AlignmentData->use_sequences[seq_index++] = true; // always use query

    // For each discontinuous Seq-align corresponding to each query-subj pair
    ITERATE(CSeq_align_set::Tdata, i, m_SeqAlignSet->Get()) {
        ASSERT((*i)->GetSegs().IsDisc());

        // For each HSP of this query-subj pair
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*i)->GetSegs().GetDisc().Get()) {
            
            // Look for HSP with score less than inclusion_ethresh
            double e = s_GetLowestEvalue((*hsp_itr)->GetScore());
            if (e < m_Opts.inclusion_ethresh) {
                m_AlignmentData->use_sequences[seq_index] = true;
                break;
            }
        }
        seq_index++;
    }
    ASSERT(seq_index == GetNumAlignedSequences()+1);
}

void
CScoreMatrixBuilder::x_SelectBySeqId(const vector< CRef<CSeq_id> >& /*ids*/)
{
    NCBI_THROW(CBlastException, eNotSupported, "select by id not implemented");
}

// Finds sequences in alignment that warrant further processing. The criteria
// to determine this includes:
// 1) Include those that have evalues that are lower than the inclusion evalue 
//    threshold
// 2) Could use some functor object if more criteria are needed
void
CScoreMatrixBuilder::SelectSequencesForProcessing()
{
    // Reset the use_sequences array sequence vector
    std::fill_n(m_AlignmentData->use_sequences, GetNumAlignedSequences() + 1,
                false);

    x_SelectByEvalue();
}

void
CScoreMatrixBuilder::ExtractAlignmentData()
{
    // Query sequence already processed
    TSeqPos seq_index = 1;  

    ITERATE(list< CRef<CSeq_align> >, itr, m_SeqAlignSet->Get()) {

        if ( !m_AlignmentData->use_sequences[seq_index] ) {
            seq_index++;
            continue;
        }

        const list< CRef<CSeq_align> >& hsp_list = 
            (*itr)->GetSegs().GetDisc().Get();
        list< CRef<CSeq_align> >::const_iterator hsp;

        // HSPs with the same query-subject pair
        if (m_Opts.use_best_alignment) {

            // Search for the best alignment
            // FIXME: refactor to use std::find()?
            list< CRef<CSeq_align> >::const_iterator best_alignment;
            double min_evalue = numeric_limits<double>::max();

            for (hsp = hsp_list.begin(); hsp != hsp_list.end(); ++hsp) {

                // Note: Std-seg can be converted to Denseg, will need 
                // conversion from Dendiag to Denseg too
                if ( !(*hsp)->GetSegs().IsDenseg() ) {
                    NCBI_THROW(CBlastException, eNotSupported, 
                               "Segment type not supported");
                }

                double evalue = s_GetLowestEvalue((*hsp)->GetScore());
                if (evalue < min_evalue) {
                    best_alignment = hsp;
                    min_evalue = evalue;
                }
            }
            ASSERT(best_alignment != hsp_list.end());
            // this could be refactored to support other segment types, will
            // need to pass portion of subj. sequence and evalue
            x_ProcessDenseg((*best_alignment)->GetSegs().GetDenseg(), 
                            seq_index, min_evalue);

        } else {

            for (hsp = hsp_list.begin(); hsp != hsp_list.end(); ++hsp) {

                // Note: Std-seg can be converted to Denseg, will need 
                // conversion from Dendiag to Denseg too
                if ( !(*hsp)->GetSegs().IsDenseg() ) {
                    NCBI_THROW(CBlastException, eNotSupported, 
                               "Segment type not supported");
                }

                double evalue = s_GetLowestEvalue((*hsp)->GetScore());

                // this could be refactored to support other segment types, will
                // need to pass portion of subj. sequence and evalue
                x_ProcessDenseg((*hsp)->GetSegs().GetDenseg(), seq_index, 
                                evalue);
            }
        }
        seq_index++;

    }

    ASSERT(seq_index == GetNumAlignedSequences()+1);
}

/// Iterates over Dense-seg for a given HSP and extracts alignment information 
/// to PsiAlignmentData structure. 
void
CScoreMatrixBuilder::x_ProcessDenseg(const CDense_seg& denseg, 
                                     TSeqPos seq_index, double e_value)
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

    // Iterate over all segments
    for (int segmt_idx = 0; segmt_idx < denseg.GetNumseg(); segmt_idx++) {

        TSeqPos query_offset = starts[query_index];
        TSeqPos subject_offset = starts[subj_index];

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
                PsiDesc& pos_desc =
                    m_AlignmentData->desc_matrix[seq_index][query_offset++];
                pos_desc.letter = GAP;
                pos_desc.used = true;
                pos_desc.e_value = kDefaultEvalueForPosition;
            }

        } else {

            // Aligned segments without any gaps
            for (TSeqPos i = 0; i < lengths[segmt_idx]; i++) {
                PsiDesc& pos_desc =
                    m_AlignmentData->desc_matrix[seq_index][query_offset++];
                pos_desc.letter = seq.first.get()[subj_seq_idx++];
                pos_desc.used = true;
                pos_desc.e_value = e_value;
            }
        }

    }

}

// Should be called once per HSP
// @todo merge with blast_objmgr_tools next week
CScoreMatrixBuilder::TSeqPair 
CScoreMatrixBuilder::x_GetSubjectSequence(const CDense_seg& ds, 
                                          CScope& scope) 
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

    CBioseq_Handle bh = scope.GetBioseqHandle(seqloc);
    CSeqVector sv = bh.GetSequenceView(seqloc,
                                       CBioseq_Handle::eViewConstructed,
                                       CBioseq_Handle::eCoding_Ncbi);
    sv.SetCoding(CSeq_data::e_Ncbistdaa);
    retval = s_ExtractSequenceFromSeqVector(sv);

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

static Uint1* 
s_ExtractSequenceFromSeqVector(CSeqVector& sv) 
{
    Uint1* retval = NULL;

    try {
        retval = new Uint1[sv.size()];
    } catch (const std::bad_alloc&) {
        ostringstream os;
        os << sv.size();
        string msg = string("Could not allocate ") + os.str() + " bytes";
        NCBI_THROW(CBlastException, eOutOfMemory, msg);
    }
    for (TSeqPos i = 0; i < sv.size(); i++) {
        retval[i] = sv[i];
    }

    return retval;
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

//////////////////////////////////////////////////////////////////////////////
// Debugging code

unsigned char ncbistdaa_to_ncbieaa[] = {
'-','A','B','C','D','E','F','G','H','I','K','L','M',
'N','P','Q','R','S','T','V','W','X','Y','Z','U','*'};

// Pretty print sequence
static void 
s_DBG_printSequence(const Uint1* seq, TSeqPos len, ostream& out,
                    bool show_markers, TSeqPos chars_per_line)
{
    TSeqPos nlines = len/chars_per_line;

    for (TSeqPos line = 0; line < nlines + 1; line++) {

        // print chars_per_line residues/bases
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < len); i++) {
            out << ncbistdaa_to_ncbieaa[seq[i]];
        }
        out << endl;

        if ( !show_markers )
            continue;

        // print the residue/base markers
        for (TSeqPos i = (chars_per_line*line); 
             i < chars_per_line*(line+1) && (i < len); i++) {
            if (i == 0 || ((i%10) == 0)) {
                out << i;
                ostringstream os;
                os << i;
                TSeqPos marker_length = os.str().size();
                i += (marker_length-1);
            } else {
                out << " ";
            }
        }
        out << endl;
    }
}

std::string
PsiAlignmentData2String(const PsiAlignmentData* alignment)
{
    if ( !alignment ) {
        return "";
    }

    stringstream ss;

    for (TSeqPos i = 0; i < alignment->dimensions->num_seqs+1; i++) {
        ss << "Seq " << setw(3) << i;
        if ( !alignment->use_sequences[i] ) {
            ss << " NOT USED";
        }
        ss << endl;
        for (TSeqPos j = 0; j < alignment->dimensions->query_sz; j++) {
            if ( !alignment->use_sequences[i] ) {
                continue;
            }

            ss << setw(3) << j << " {" 
               << ncbistdaa_to_ncbieaa[alignment->desc_matrix[i][j].letter]
               << ",";
            if (alignment->desc_matrix[i][j].used)
                ss << "used";
            else
                ss << "unused";
            ss << "," << alignment->desc_matrix[i][j].e_value
               << "," << alignment->desc_matrix[i][j].extents.left
               << "," << alignment->desc_matrix[i][j].extents.right
               << "}" << endl;
        }
        ss << "*****************************************************" << endl;
    }
    ss << endl;

    // Print the number of matching sequences per column
    ss << "Matching sequences at each position of the query:" << endl;
    for (TSeqPos i = 0; i < alignment->dimensions->query_sz; i++) {
        ss << setw(3) << i << " ";
    }
    for (TSeqPos i = 0; i < alignment->dimensions->query_sz; i++) { 
        ss << setw(4) << alignment->match_seqs[i] << " "; 
    }
    ss << endl;
    ss << "*****************************************************" << endl;

    // Print the number of distinct residues per position
    ss << "Residue counts in the matrix" << endl;
    ss << "   ";
    for (TSeqPos i = 0; i < alignment->dimensions->query_sz; i++) {
        ss << setw(3) << i << " ";
    }
    for (TSeqPos j = 0; j < BLASTAA_SIZE; j++) {
        ss << ncbistdaa_to_ncbieaa[j] << ": ";
        for (TSeqPos i = 0; i < alignment->dimensions->query_sz; i++) {
            ss << setw(3) << alignment->res_counts[i][j] << " ";
        }
        ss << endl;
    }

    return ss.str();
}

ostream&
operator<<(ostream& out, const CScoreMatrixBuilder& smb)
{
    TSeqPos query_sz = smb.m_AlignmentData->dimensions->query_sz;

    // Print query sequence in IUPAC
    out << "QUERY:\n";
    s_DBG_printSequence(smb.m_AlignmentData->query, query_sz, out);

    // Print description of alignment and associated data
    out << PsiAlignmentData2String(smb.m_AlignmentData.get());
    out << endl;

    return out;
}

// End debugging code
//////////////////////////////////////////////////////////////////////////////

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/06/21 12:53:05  camacho
* Replace PSI_ALPHABET_SIZE for BLASTAA_SIZE
*
* Revision 1.4  2004/06/09 14:32:23  camacho
* Added use_best_alignment option
*
* Revision 1.3  2004/05/28 17:42:02  camacho
* Fix compiler warning
*
* Revision 1.2  2004/05/28 17:15:43  camacho
* Fix NCBI {BEGIN,END}_SCOPE macro usage, remove warning
*
* Revision 1.1  2004/05/28 16:41:39  camacho
* Initial revision
*
*
* ===========================================================================
*/
