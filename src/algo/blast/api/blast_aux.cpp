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
* Author:  Christiam Camacho
*
*/

/// @file blast_aux.cpp
/// Implements C++ wrapper classes for structures in algo/blast/core as well as
/// some auxiliary functions to convert CSeq_loc to/from BlastMask structures.

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/seqport_util.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_seqsrc_impl.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

#ifndef SKIP_DOXYGEN_PROCESSING

void
CQuerySetUpOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/)
    const
{
    ddc.SetFrame("CQuerySetUpOptions");
    if (!m_Ptr)
        return;

    ddc.Log("filter_string", m_Ptr->filter_string);
    ddc.Log("strand_option", m_Ptr->strand_option);
    ddc.Log("genetic_code", m_Ptr->genetic_code);
}

void
CBLAST_SequenceBlk::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBLAST_SequenceBlk");
    if (!m_Ptr)
        return;

    ddc.Log("sequence", m_Ptr->sequence);
    ddc.Log("sequence_start", m_Ptr->sequence_start);
    ddc.Log("sequence_allocated", m_Ptr->sequence_allocated);
    ddc.Log("sequence_start_allocated", m_Ptr->sequence_start_allocated);
    ddc.Log("length", m_Ptr->length);

}

void
CBlastQueryInfo::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastQueryInfo");
    if (!m_Ptr)
        return;

}
void
CLookupTableOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CLookupTableOptions");
    if (!m_Ptr)
        return;

    ddc.Log("threshold", m_Ptr->threshold);
    ddc.Log("lut_type", m_Ptr->lut_type);
    ddc.Log("word_size", m_Ptr->word_size);
    ddc.Log("mb_template_length", m_Ptr->mb_template_length);
    ddc.Log("mb_template_type", m_Ptr->mb_template_type);
    ddc.Log("max_positions", m_Ptr->max_positions);
    ddc.Log("variable_wordsize", m_Ptr->variable_wordsize);
    ddc.Log("full_byte_scan", m_Ptr->full_byte_scan);
}

void
CLookupTableWrap::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CLookupTableWrap");
    if (!m_Ptr)
        return;

}
void
CBlastInitialWordOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("BlastInitialWordOptions");
    if (!m_Ptr)
        return;

    ddc.Log("window_size", m_Ptr->window_size);
    ddc.Log("ungapped_extension", m_Ptr->ungapped_extension);
    ddc.Log("x_dropoff", m_Ptr->x_dropoff);
}
void
CBlastInitialWordParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastInitialWordParameters");
    if (!m_Ptr)
        return;

}
void
CBlast_ExtendWord::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlast_ExtendWord");
    if (!m_Ptr)
        return;

}

void
CBlastExtensionOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastExtensionOptions");
    if (!m_Ptr)
        return;

    ddc.Log("gap_x_dropoff", m_Ptr->gap_x_dropoff);
    ddc.Log("gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
    ddc.Log("ePrelimGapExt", m_Ptr->ePrelimGapExt);
    ddc.Log("eTbackExt", m_Ptr->eTbackExt);
}

void
CBlastExtensionParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastExtensionParameters");
    if (!m_Ptr)
        return;

    ddc.Log("gap_x_dropoff", m_Ptr->gap_x_dropoff);
    ddc.Log("gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
}

void
CBlastHitSavingOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastHitSavingOptions");
    if (!m_Ptr)
        return;

    ddc.Log("hitlist_size", m_Ptr->hitlist_size);
    ddc.Log("hsp_num_max", m_Ptr->hsp_num_max);
    ddc.Log("total_hsp_limit", m_Ptr->total_hsp_limit);
    ddc.Log("hsp_range_max", m_Ptr->hsp_range_max);
    ddc.Log("culling_limit", m_Ptr->culling_limit);
    ddc.Log("required_start", m_Ptr->required_start);
    ddc.Log("required_end", m_Ptr->required_end);
    ddc.Log("expect_value", m_Ptr->expect_value);
    ddc.Log("cutoff_score", m_Ptr->cutoff_score);
    ddc.Log("percent_identity", m_Ptr->percent_identity);
    ddc.Log("do_sum_stats", m_Ptr->do_sum_stats);
    ddc.Log("longest_intron", m_Ptr->longest_intron);
}
void
CBlastHitSavingParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastHitSavingParameters");
    if (!m_Ptr)
        return;

}
void
CPSIBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CPSIBlastOptions");
    if (!m_Ptr)
        return;

    ddc.Log("pseudo_count", m_Ptr->pseudo_count);
}

void
CBlastGapAlignStruct::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastGapAlignStruct");
    if (!m_Ptr)
        return;

}

void
CBlastEffectiveLengthsOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastEffectiveLengthsOptions");
    if (!m_Ptr)
        return;

    ddc.Log("db_length", (unsigned long)m_Ptr->db_length); // Int8
    ddc.Log("dbseq_num", m_Ptr->dbseq_num);
    ddc.Log("searchsp_eff", (unsigned long)m_Ptr->searchsp_eff); // Int8
}

void
CBlastEffectiveLengthsParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastEffectiveLengthsParameters");
    if (!m_Ptr)
        return;

    ddc.Log("real_db_length", (unsigned long)m_Ptr->real_db_length); // Int8
    ddc.Log("real_num_seqs", m_Ptr->real_num_seqs);
}

void
CBlastScoreBlk::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastScoreBlk");
    if (!m_Ptr)
        return;

    ddc.Log("protein_alphabet", m_Ptr->protein_alphabet);
    ddc.Log("alphabet_size", m_Ptr->alphabet_size);
    ddc.Log("alphabet_start", m_Ptr->alphabet_start);
    ddc.Log("loscore", m_Ptr->loscore);
    ddc.Log("hiscore", m_Ptr->hiscore);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("scale_factor", m_Ptr->scale_factor);
    ddc.Log("read_in_matrix", m_Ptr->read_in_matrix);
    ddc.Log("number_of_contexts", m_Ptr->number_of_contexts);
    ddc.Log("name", m_Ptr->name);
    ddc.Log("ambig_size", m_Ptr->ambig_size);
    ddc.Log("ambig_occupy", m_Ptr->ambig_occupy);
}

void
CBlastScoringOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastScoringOptions");
    if (!m_Ptr)
        return;

    ddc.Log("matrix", m_Ptr->matrix);
    ddc.Log("matrix_path", m_Ptr->matrix_path);
    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("gapped_calculation", m_Ptr->gapped_calculation);
    ddc.Log("gap_open", m_Ptr->gap_open);
    ddc.Log("gap_extend", m_Ptr->gap_extend);
    ddc.Log("shift_pen", m_Ptr->shift_pen);
    ddc.Log("decline_align", m_Ptr->decline_align);
    ddc.Log("is_ooframe", m_Ptr->is_ooframe);
}

void
CBlastScoringParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*d*/)
    const
{
	ddc.SetFrame("CBlastScoringParameters");
    if (!m_Ptr)
        return;

    ddc.Log("reward", m_Ptr->reward);
    ddc.Log("penalty", m_Ptr->penalty);
    ddc.Log("gap_open", m_Ptr->gap_open);
    ddc.Log("gap_extend", m_Ptr->gap_extend);
    ddc.Log("decline_align", m_Ptr->decline_align);
    ddc.Log("shift_pen", m_Ptr->shift_pen);
    ddc.Log("scale_factor", m_Ptr->scale_factor);
}

void
CBlastDatabaseOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastDatabaseOptions");
    if (!m_Ptr)
        return;

}

void
CPSIMatrix::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CPSIMatrix");
    if (!m_Ptr)
        return;

    ddc.Log("ncols", m_Ptr->ncols);
    ddc.Log("nrows", m_Ptr->nrows);
    ddc.Log("lambda", m_Ptr->lambda);
    ddc.Log("kappa", m_Ptr->kappa);
    ddc.Log("h", m_Ptr->h);
    // pssm omitted because it might be too large!
}

void
CPSIDiagnosticsResponse::DebugDump(CDebugDumpContext ddc, 
                                   unsigned int /*depth*/) const
{
    ddc.SetFrame("CPSIDiagnosticsResponse");
    if (!m_Ptr)
        return;

    ddc.Log("alphabet_size", m_Ptr->alphabet_size);
}

void
CBlastSeqSrc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqSrc");
    if (!m_Ptr)
        return;

    /** @todo should the BlastSeqSrc API support names for types of
     * BlastSeqSrc? Might be useful for debugging */
}

void
CBlastSeqSrcIterator::DebugDump(CDebugDumpContext ddc, 
                                unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqSrcIterator");
    if (!m_Ptr)
        return;

    string iterator_type;
    switch (m_Ptr->itr_type) {
    case eOidList: iterator_type = "oid_list"; break;
    case eOidRange: iterator_type = "oid_range"; break;
    default: abort();
    }

    ddc.Log("itr_type", iterator_type);
    ddc.Log("current_pos", m_Ptr->current_pos);
    ddc.Log("chunk_sz", m_Ptr->chunk_sz);
}

void
CBlast_Message::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlast_Message");
    if (!m_Ptr)
        return;

    ddc.Log("severity", m_Ptr->severity);
    ddc.Log("message", m_Ptr->message);
    // code and subcode are unused
}

void
CBlastHSPResults::DebugDump(CDebugDumpContext ddc, 
                            unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastHSPResults");
    if (!m_Ptr)
        return;

    ddc.Log("num_queries", m_Ptr->num_queries);
    // hitlist itself is not printed
}

void 
CBlastMaskLoc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastMaskLoc");
    if (!m_Ptr)
        return;
   
    ddc.Log("total_size", m_Ptr->total_size);
    for (int index = 0; index < m_Ptr->total_size; ++index) {
        ddc.Log("context", index);
        for (BlastSeqLoc* seqloc = m_Ptr->seqloc_array[index];
             seqloc; seqloc = seqloc->next) {
            ddc.Log("left", seqloc->ssr->left);
            ddc.Log("right", seqloc->ssr->right);
        }
    }
}

void 
CBlastSeqLoc::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CBlastSeqLoc");
    if (!m_Ptr)
        return;
   
    for (BlastSeqLoc* tmp = m_Ptr; tmp; tmp = tmp->next) {
        ddc.Log("left", tmp->ssr->left);
        ddc.Log("right", tmp->ssr->right);
    }
}

#endif /* SKIP_DOXYGEN_PROCESSING */

BlastSeqLoc*
CSeqLoc2BlastSeqLoc(const objects::CSeq_loc* slp)
{
    if (!slp || slp->IsNull())
        return NULL;

    _ASSERT(slp->IsInt() || slp->IsPacked_int() || slp->IsMix());

    CBlastSeqLoc retval;

    if (slp->IsInt()) {
        BlastSeqLocNew(&retval, slp->GetInt().GetFrom(), slp->GetInt().GetTo());
    } else if (slp->IsPacked_int()) {
        ITERATE(CPacked_seqint::Tdata, itr, slp->GetPacked_int().Get()) {
            BlastSeqLocNew(&retval, (*itr)->GetFrom(), (*itr)->GetTo());
        }
    } else if (slp->IsMix()) {
        ITERATE(CSeq_loc_mix::Tdata, itr, slp->GetMix().Get()) {
            if ((*itr)->IsInt()) {
                BlastSeqLocNew(&retval, (*itr)->GetInt().GetFrom(), 
                                        (*itr)->GetInt().GetTo());
            } else if ((*itr)->IsPnt()) {
                BlastSeqLocNew(&retval, (*itr)->GetPnt().GetPoint(), 
                                        (*itr)->GetPnt().GetPoint());
            }
        }
    } else {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Unsupported CSeq_loc type");
    }

    return retval.Release();
}

TAutoUint1ArrayPtr
FindGeneticCode(int genetic_code)
{
    Uint1* retval = NULL;
    CSeq_data gc_ncbieaa(CGen_code_table::GetNcbieaa(genetic_code),
            CSeq_data::e_Ncbieaa);
    CSeq_data gc_ncbistdaa;

    TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
            CSeq_data::e_Ncbistdaa);

    ASSERT(gc_ncbistdaa.IsNcbistdaa());
    ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

    try {
        retval = new Uint1[nconv];
    } catch (const bad_alloc&) {
        return NULL;
    }

    for (TSeqPos i = 0; i < nconv; i++)
        retval[i] = gc_ncbistdaa.GetNcbistdaa().Get()[i];

    return retval;
}

EProgram ProgramNameToEnum(const std::string& program_name)
{
    ASSERT( !program_name.empty() );

    string lowercase_program_name(program_name);
    lowercase_program_name = NStr::ToLower(lowercase_program_name);

    if (lowercase_program_name == "blastn") {
        return eBlastn;
    } else if (lowercase_program_name == "blastp") {
        return eBlastp;
    } else if (lowercase_program_name == "blastx") {
        return eBlastx;
    } else if (lowercase_program_name == "tblastn") {
        return eTblastn;
    } else if (lowercase_program_name == "tblastx") {
        return eTblastx;
    } else if (lowercase_program_name == "rpsblast") {
        return eRPSBlast;
    } else if (lowercase_program_name == "rpstblastn") {
        return eRPSTblastn;
    } else if (lowercase_program_name == "megablast") {
        return eMegablast; 
    } else if (lowercase_program_name == "psiblast") {
        return ePSIBlast;
    } else {
        // Handle discontiguous megablast (no established convention AFAIK)
        string::size_type idx_mb = lowercase_program_name.find("megablast");
        string::size_type idx_disco = lowercase_program_name.find("disc");
        if (idx_mb != string::npos && idx_disco != string::npos) {
            return eDiscMegablast;
        }
        
        // Handle others ...
    }
    NCBI_THROW(CBlastException, eNotSupported, 
               "Program type '" + program_name + "' not supported");
}

string Blast_ProgramNameFromType(EBlastProgramType program)
{
    char* program_string(0);
    if (BlastNumber2Program(program, &program_string) == 0) {
        string retval(program_string);
        sfree(program_string);
        return retval;
    } else {
        return NcbiEmptyString;
    }
}

void 
Blast_GetSeqLocInfoVector(EBlastProgramType program, 
                          vector< CRef<CSeq_id> >& seqid_v,
                          const BlastMaskLoc* mask, 
                          TSeqLocInfoVector& mask_v)
{
    const bool kTranslatedQuery = Blast_QueryIsTranslated(program);
    const int kNumFrames = (kTranslatedQuery ? NUM_FRAMES : 1);

    if (seqid_v.size() != (size_t) mask->total_size/kNumFrames) {
        string msg = "Blast_GetSeqLocInfoVector: number of query ids " +
            NStr::IntToString(seqid_v.size()) + 
            " not equal to number of queries in mask " + 
            NStr::IntToString(mask->total_size/kNumFrames);
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }

    for (unsigned int query_index = 0; query_index < seqid_v.size(); 
         ++query_index) {
        list<CRef<CSeqLocInfo> > mask_info_list;

        for (int tmp_index = 0; tmp_index < kNumFrames; tmp_index++) {
            BlastSeqLoc* loc = NULL;
            if (mask)
                loc = mask->seqloc_array[query_index*kNumFrames+tmp_index];
        
            for ( ; loc; loc = loc->next) {
                CRef<CSeqLocInfo> seqloc_info(new CSeqLocInfo());
                int frame = (kTranslatedQuery ? 
                    (int) BLAST_ContextToFrame(program, tmp_index) : 0);
                seqloc_info->SetFrame(frame);

                
                CSeq_interval* seqint =
                    new CSeq_interval(*seqid_v[query_index], loc->ssr->left, 
                                      loc->ssr->right);
            
                seqloc_info->SetInterval(seqint);
                mask_info_list.push_back(seqloc_info);
            }
        }
        mask_v.push_back(mask_info_list);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.78  2005/08/30 15:47:01  camacho
 * Remove redundant implementation of Blast_ProgramNameFromType (call BlastNumber2Program)
 *
 * Revision 1.77  2005/08/29 18:09:43  camacho
 * Fix warning
 *
 * Revision 1.76  2005/08/29 14:39:07  camacho
 * From Ilya Dondoshansky:
 * Added class CSeqLocInfo, type TSeqLocInfoVector and method to construct
 * it from the BlastMaskLoc core structure
 *
 * Revision 1.75  2005/07/07 16:32:11  camacho
 * Revamping of BLAST exception classes and error codes
 *
 * Revision 1.74  2005/06/06 14:56:35  camacho
 * Remove premature optimizations when using BlastSeqLocNew
 *
 * Revision 1.73  2005/05/25 18:58:39  camacho
 * + CBlastSeqLoc::DebugDump
 *
 * Revision 1.72  2005/04/27 19:57:58  dondosha
 * BlastScoreBlk::effective_search_sp field no longer exists
 *
 * Revision 1.71  2005/04/27 14:38:21  papadopo
 * modify debug output to show culling limit
 *
 * Revision 1.70  2005/04/18 14:00:44  camacho
 * Updates following BlastSeqSrc reorganization
 *
 * Revision 1.69  2005/03/28 16:26:06  camacho
 * + CBlastHSPResults::DebugDump
 *
 * Revision 1.68  2005/03/02 16:45:36  camacho
 * Remove use_real_db_size
 *
 * Revision 1.67  2005/02/14 14:09:37  camacho
 *  Removed obsolete fields from the BlastScoreBlk
 *
 * Revision 1.66  2005/01/14 18:00:59  papadopo
 * move FillRPSInfo into CDbBlast, to remove some xblast dependencies on SeqDB
 *
 * Revision 1.65  2005/01/10 19:23:02  papadopo
 * Use SeqDB to recover the path to RPS blast data files
 *
 * Revision 1.64  2005/01/10 13:33:18  madden
 * Fix calls to ddc.Log for added/deleted options
 *
 * Revision 1.63  2004/12/29 15:11:52  camacho
 * +CBlast_Message::DebugDump
 *
 * Revision 1.62  2004/12/28 18:48:13  dondosha
 * Added DebugDump implementation for CBlastMaskLoc wrapper class
 *
 * Revision 1.61  2004/12/28 16:47:43  camacho
 * 1. Use typedefs to AutoPtr consistently
 * 2. Remove exception specification from blast::SetupQueries
 * 3. Use SBlastSequence structure instead of std::pair as return value to
 *    blast::GetSequence
 *
 * Revision 1.60  2004/12/20 21:50:27  camacho
 * + RAII BlastEffectiveLengthsParameters
 *
 * Revision 1.59  2004/12/20 21:47:36  camacho
 * Implement CBlastExtensionParameters::DebugDump
 *
 * Revision 1.58  2004/12/20 16:11:33  camacho
 * + RAII wrapper for BlastScoringParameters
 *
 * Revision 1.57  2004/12/03 22:23:35  camacho
 * Minor change
 *
 * Revision 1.56  2004/11/23 23:00:59  camacho
 * + RAII class for BlastSeqSrc
 *
 * Revision 1.55  2004/11/12 16:42:53  camacho
 * Add handling of missing EProgram values to ProgramNameToEnum
 *
 * Revision 1.54  2004/11/04 15:51:02  papadopo
 * prepend 'Blast' to RPSInfo and related structures
 *
 * Revision 1.53  2004/11/02 18:26:17  madden
 * Remove gap_trigger
 *
 * Revision 1.52  2004/10/26 15:31:08  dondosha
 * Added function Blast_FillRPSInfo, previously static in demo/blast_app.cpp
 *
 * Revision 1.51  2004/10/21 18:04:06  camacho
 * Remove unneeded ostringstream
 *
 * Revision 1.50  2004/09/13 15:55:04  madden
 * Remove unused parameter from CSeqLoc2BlastSeqLoc
 *
 * Revision 1.49  2004/09/13 12:47:06  madden
 * Changes for redefinition of BlastSeqLoc and BlastMaskLoc
 *
 * Revision 1.48  2004/09/08 14:14:31  camacho
 * Doxygen fixes
 *
 * Revision 1.47  2004/08/18 18:14:13  camacho
 * Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
 *
 * Revision 1.46  2004/08/13 22:33:26  camacho
 * Added DebugDump for PSIBlastOptions
 *
 * Revision 1.45  2004/08/11 14:24:50  camacho
 * Move FindGeneticCode
 *
 * Revision 1.44  2004/08/04 20:10:33  camacho
 * + class wrappers for PSIMatrix and PSIDiagnosticsResponse, implemented DebugDump for CBlastScoreBlk
 *
 * Revision 1.43  2004/06/23 14:05:06  dondosha
 * Changed CSeq_loc argument in CSeqLoc2BlastMaskLoc to pointer
 *
 * Revision 1.42  2004/06/08 14:58:46  dondosha
 * Removed is_neighboring option; let application set min_hit_length and percent_identity options instead
 *
 * Revision 1.41  2004/06/02 15:57:06  bealer
 * - Isolate object manager dependent code.
 *
 * Revision 1.40  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.39  2004/05/17 15:33:14  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt
 *
 * Revision 1.38  2004/05/14 16:01:10  madden
 * Rename BLAST_ExtendWord to Blast_ExtendWord in order to fix conflicts with C toolkit
 *
 * Revision 1.37  2004/04/05 16:09:27  camacho
 * Rename DoubleInt -> SSeqRange
 *
 * Revision 1.36  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 *
 * ===========================================================================
 */
