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

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/blast_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


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
    ddc.Log("alphabet_size", m_Ptr->alphabet_size);
    ddc.Log("mb_template_length", m_Ptr->mb_template_length);
    ddc.Log("mb_template_type", m_Ptr->mb_template_type);
    ddc.Log("max_positions", m_Ptr->max_positions);
    ddc.Log("scan_step", m_Ptr->scan_step);
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
    ddc.Log("container_type", m_Ptr->container_type);
    ddc.Log("extension_method", m_Ptr->extension_method);
    ddc.Log("variable_wordsize", m_Ptr->variable_wordsize);
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
    ddc.Log("gap_trigger", m_Ptr->gap_trigger);
    ddc.Log("ePrelimGapExt", m_Ptr->ePrelimGapExt);
    ddc.Log("eTbackExt", m_Ptr->eTbackExt);
}

void
CBlastExtensionParameters::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastExtensionParameters");
    if (!m_Ptr)
        return;

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
    ddc.Log("perform_culling", m_Ptr->perform_culling);
    ddc.Log("required_start", m_Ptr->required_start);
    ddc.Log("required_end", m_Ptr->required_end);
    ddc.Log("expect_value", m_Ptr->expect_value);
    ddc.Log("original_expect_value", m_Ptr->original_expect_value);
    ddc.Log("cutoff_score", m_Ptr->cutoff_score);
    ddc.Log("percent_identity", m_Ptr->percent_identity);
    ddc.Log("do_sum_stats", m_Ptr->do_sum_stats);
    ddc.Log("longest_longron", m_Ptr->longest_intron);
    ddc.Log("is_neighboring", m_Ptr->is_neighboring);
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
    ddc.Log("use_real_db_size", m_Ptr->use_real_db_size);
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
CBlastDatabaseOptions::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
	ddc.SetFrame("CBlastDatabaseOptions");
    if (!m_Ptr)
        return;

}


BlastMaskLoc*
CSeqLoc2BlastMaskLoc(const CSeq_loc &slp, int index)
{
    if (slp.IsNull())
        return NULL;

    _ASSERT(slp.IsInt() || slp.IsPacked_int() || slp.IsMix());

    BlastSeqLoc* bsl = NULL,* curr = NULL,* tail = NULL;
    BlastMaskLoc* mask = NULL;

    if (slp.IsInt()) {
        bsl = 
            BlastSeqLocNew(slp.GetInt().GetFrom(), slp.GetInt().GetTo());
    } else if (slp.IsPacked_int()) {
        ITERATE(list< CRef<CSeq_interval> >, itr, 
                slp.GetPacked_int().Get()) {
            curr = BlastSeqLocNew((*itr)->GetFrom(), (*itr)->GetTo());
            if (!bsl) {
                bsl = tail = curr;
            } else {
                tail->next = curr;
                tail = tail->next;
            }
        }
    } else if (slp.IsMix()) {
        ITERATE(CSeq_loc_mix::Tdata, itr, slp.GetMix().Get()) {
            if ((*itr)->IsInt()) {
                curr = BlastSeqLocNew((*itr)->GetInt().GetFrom(), 
                                      (*itr)->GetInt().GetTo());
            } else if ((*itr)->IsPnt()) {
                curr = BlastSeqLocNew((*itr)->GetPnt().GetPoint(), 
                                      (*itr)->GetPnt().GetPoint());
            }

            if (!bsl) {
                bsl = tail = curr;
            } else {
                tail->next = curr;
                tail = tail->next;
            }
        }
    }

    mask = (BlastMaskLoc*) calloc(1, sizeof(BlastMaskLoc));
    mask->index = index;
    mask->loc_list = (ListNode *) bsl;

    return mask;
}

void BlastMaskLocDNAToProtein(BlastMaskLoc** mask_ptr, const CSeq_loc &seqloc, 
                           CScope* scope)
{
   BlastMaskLoc* last_mask = NULL,* head_mask = NULL,* mask_loc; 
   Int4 dna_length;
   BlastSeqLoc* dna_loc,* prot_loc_head,* prot_loc_last;
   SSeqRange* dip;
   Int4 context;
   Int2 frame;
   Int4 from, to;

   if (!mask_ptr)
      return;

   mask_loc = *mask_ptr;

   if (!mask_loc) 
      return;

   dna_length = sequence::GetLength(seqloc, scope);
   /* Reproduce this mask for all 6 frames, with translated 
      coordinates */
   for (context = 0; context < NUM_FRAMES; ++context) {
       if (!last_mask) {
           head_mask = last_mask = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
       } else {
           last_mask->next = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
           last_mask = last_mask->next;
       }
         
       last_mask->index = NUM_FRAMES * mask_loc->index + context;
       prot_loc_last = prot_loc_head = NULL;
       
       frame = BLAST_ContextToFrame(blast_type_blastx, context);
       
       for (dna_loc = mask_loc->loc_list; dna_loc; 
            dna_loc = dna_loc->next) {
           dip = (SSeqRange*) dna_loc->ptr;
           if (frame < 0) {
               from = (dna_length + frame - dip->right)/CODON_LENGTH;
               to = (dna_length + frame - dip->left)/CODON_LENGTH;
           } else {
               from = (dip->left - frame + 1)/CODON_LENGTH;
               to = (dip->right - frame + 1)/CODON_LENGTH;
           }
           if (!prot_loc_last) {
               prot_loc_head = prot_loc_last = BlastSeqLocNew(from, to);
           } else { 
               prot_loc_last->next = BlastSeqLocNew(from, to);
               prot_loc_last = prot_loc_last->next; 
           }
       }
       last_mask->loc_list = prot_loc_head;
   }

   /* Free the mask with nucleotide coordinates */
   BlastMaskLocFree(mask_loc);
   /* Return the new mask with protein coordinates */
   *mask_ptr = head_mask;
}

void BlastMaskLocProteinToDNA(BlastMaskLoc** mask_ptr, TSeqLocVector &slp)
{
   BlastMaskLoc* mask_loc;
   BlastSeqLoc* loc;
   SSeqRange* dip;
   Int4 dna_length;
   Int2 frame;
   Int4 from, to;

   if (!mask_ptr) 
      // Nothing to do - just return
      return;

   for (mask_loc = *mask_ptr; mask_loc; mask_loc = mask_loc->next) {
      dna_length = 
         sequence::GetLength(*slp[mask_loc->index/NUM_FRAMES].seqloc, 
                             slp[mask_loc->index/NUM_FRAMES].scope);
      frame = BLAST_ContextToFrame(blast_type_blastx, 
                                   mask_loc->index % NUM_FRAMES);
      
      for (loc = mask_loc->loc_list; loc; loc = loc->next) {
         dip = (SSeqRange*) loc->ptr;
         if (frame < 0)	{
            to = dna_length - CODON_LENGTH*dip->left + frame;
            from = dna_length - CODON_LENGTH*dip->right + frame + 1;
         } else {
            from = CODON_LENGTH*dip->left + frame - 1;
            to = CODON_LENGTH*dip->right + frame - 1;
         }
         dip->left = from;
         dip->right = to;
      }
   }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
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
