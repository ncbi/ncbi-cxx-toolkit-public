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
* File Description:
*   C++ Wrappers to NewBlast structures
*
*/

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


void
CQuerySetUpOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth)
    const
{
    ddc.SetFrame("CQuerySetUpOptions");
    if (!m_Ptr)
        return;

    ddc.Log("filter_string", m_Ptr->filter_string);
    ddc.Log("strand_option", (unsigned long)m_Ptr->strand_option);
    ddc.Log("genetic_code", (long)m_Ptr->genetic_code);
}

void
CBLAST_SequenceBlk::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBLAST_SequenceBlk");
    if (!m_Ptr)
        return;

    ddc.Log("sequence", m_Ptr->sequence);
    ddc.Log("sequence_start", m_Ptr->sequence_start);
    ddc.Log("sequence_allocated", (bool)m_Ptr->sequence_allocated);
    ddc.Log("sequence_start_allocated", (bool)m_Ptr->sequence_start_allocated);
    ddc.Log("length", (long)m_Ptr->length);

}

void
CBlastQueryInfo::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastQueryInfo");
    if (!m_Ptr)
        return;

}
void
CLookupTableOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CLookupTableOptions");
    if (!m_Ptr)
        return;

    ddc.Log("threshold", (long)m_Ptr->threshold);
    ddc.Log("lut_type", (long)m_Ptr->lut_type);
    ddc.Log("word_size", (long)m_Ptr->word_size);
    ddc.Log("alphabet_size", (long)m_Ptr->alphabet_size);
    ddc.Log("mb_template_length", (unsigned long)m_Ptr->mb_template_length);
    ddc.Log("mb_template_type", (unsigned long)m_Ptr->mb_template_type);
    ddc.Log("max_positions", (long)m_Ptr->max_positions);
    ddc.Log("scan_step", (unsigned long)m_Ptr->scan_step);
}

void
CLookupTableWrap::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CLookupTableWrap");
    if (!m_Ptr)
        return;

}
void
CBlastInitialWordOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("BlastInitialWordOptions");
    if (!m_Ptr)
        return;

    ddc.Log("window_size", (long)m_Ptr->window_size);
    ddc.Log("container_type", (long)m_Ptr->container_type);
    ddc.Log("extension_method", (unsigned long)m_Ptr->extension_method);
    ddc.Log("variable_wordsize", (bool)m_Ptr->variable_wordsize);
    ddc.Log("ungapped_extension", (bool)m_Ptr->ungapped_extension);
    ddc.Log("x_dropoff", m_Ptr->x_dropoff);
}
void
CBlastInitialWordParameters::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastInitialWordParameters");
    if (!m_Ptr)
        return;

}
void
CBLAST_ExtendWord::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBLAST_ExtendWord");
    if (!m_Ptr)
        return;

}

void
CBlastExtensionOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastExtensionOptions");
    if (!m_Ptr)
        return;

    ddc.Log("gap_x_dropoff", m_Ptr->gap_x_dropoff);
    ddc.Log("gap_x_dropoff_final", m_Ptr->gap_x_dropoff_final);
    ddc.Log("gap_trigger", m_Ptr->gap_trigger);
    ddc.Log("algorithm_type", (long)m_Ptr->algorithm_type);
}

void
CBlastExtensionParameters::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastExtensionParameters");
    if (!m_Ptr)
        return;

}

void
CBlastHitSavingOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastHitSavingOptions");
    if (!m_Ptr)
        return;

    ddc.Log("hitlist_size", (long)m_Ptr->hitlist_size);
    ddc.Log("hsp_num_max", (long)m_Ptr->hsp_num_max);
    ddc.Log("total_hsp_limit", (long)m_Ptr->total_hsp_limit);
    ddc.Log("hsp_range_max", (long)m_Ptr->hsp_range_max);
    ddc.Log("perform_culling", (bool)m_Ptr->perform_culling);
    ddc.Log("required_start", (long)m_Ptr->required_start);
    ddc.Log("required_end", (long)m_Ptr->required_end);
    ddc.Log("expect_value", m_Ptr->expect_value);
    ddc.Log("original_expect_value", m_Ptr->original_expect_value);
    ddc.Log("single_hsp_evalue", m_Ptr->single_hsp_evalue);
    ddc.Log("cutoff_score", (long)m_Ptr->cutoff_score);
    ddc.Log("single_hsp_score", (long)m_Ptr->single_hsp_score);
    ddc.Log("percent_identity", m_Ptr->percent_identity);
    ddc.Log("do_sum_stats", (bool)m_Ptr->do_sum_stats);
    ddc.Log("longest_longron", (long)m_Ptr->longest_intron);
    ddc.Log("is_neighboring", (bool)m_Ptr->is_neighboring);
}
void
CBlastHitSavingParameters::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastHitSavingParameters");
    if (!m_Ptr)
        return;

}
void
CPSIBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CPSIBlastOptions");
    if (!m_Ptr)
        return;

}
void
CBlastGapAlignStruct::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastGapAlignStruct");
    if (!m_Ptr)
        return;

}

void
CBlastEffectiveLengthsOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastEffectiveLengthsOptions");
    if (!m_Ptr)
        return;

    ddc.Log("db_length", (long)m_Ptr->db_length);
    ddc.Log("dbseq_num", (long)m_Ptr->dbseq_num);
    ddc.Log("searchsp_eff", (long)m_Ptr->searchsp_eff);
    ddc.Log("use_real_db_size", (bool)m_Ptr->use_real_db_size);
}

void
CBlastScoringOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastScoringOptions");
    if (!m_Ptr)
        return;

    ddc.Log("matrix", m_Ptr->matrix);
    ddc.Log("matrix_path", m_Ptr->matrix_path);
    ddc.Log("reward", (long)m_Ptr->reward);
    ddc.Log("penalty", (long)m_Ptr->penalty);
    ddc.Log("gapped_calculation", (bool)m_Ptr->gapped_calculation);
    ddc.Log("gap_open", (long)m_Ptr->gap_open);
    ddc.Log("gap_extend", (long)m_Ptr->gap_extend);
    ddc.Log("shift_pen", (long)m_Ptr->shift_pen);
    ddc.Log("decline_align", (long)m_Ptr->decline_align);
    ddc.Log("is_ooframe", (bool)m_Ptr->is_ooframe);
}

void
CBlastDatabaseOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
	ddc.SetFrame("CBlastDatabaseOptions");
    if (!m_Ptr)
        return;

}


BlastMask*
CSeqLoc2BlastMask(const CSeq_loc &slp, int index)
{
    if (slp.IsNull())
        return NULL;

    _ASSERT(slp.IsInt() || slp.IsPacked_int() || slp.IsMix());

    BlastSeqLoc* bsl = NULL,* curr = NULL,* tail = NULL;
    BlastMask* mask = NULL;

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

    mask = (BlastMask*) calloc(1, sizeof(BlastMask));
    mask->index = index;
    mask->loc_list = (ListNode *) bsl;

    return mask;
}

#define NUM_FRAMES 6
void BlastMaskDNAToProtein(BlastMask** mask_ptr, const CSeq_loc &seqloc, 
                           CScope* scope)
{
   BlastMask* last_mask = NULL,* head_mask = NULL,* mask_loc; 
   Int4 dna_length;
   BlastSeqLoc* dna_loc,* prot_loc_head,* prot_loc_last;
   DoubleInt* dip;
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
           head_mask = last_mask = (BlastMask *) calloc(1, sizeof(BlastMask));
       } else {
           last_mask->next = (BlastMask *) calloc(1, sizeof(BlastMask));
           last_mask = last_mask->next;
       }
         
       last_mask->index = NUM_FRAMES * mask_loc->index + context;
       prot_loc_last = prot_loc_head = NULL;
       
       frame = BLAST_ContextToFrame(blast_type_blastx, context);
       
       for (dna_loc = mask_loc->loc_list; dna_loc; 
            dna_loc = dna_loc->next) {
           dip = (DoubleInt*) dna_loc->ptr;
           if (frame < 0) {
               from = (dna_length + frame - dip->i2)/CODON_LENGTH;
               to = (dna_length + frame - dip->i1)/CODON_LENGTH;
           } else {
               from = (dip->i1 - frame + 1)/CODON_LENGTH;
               to = (dip->i2 - frame + 1)/CODON_LENGTH;
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
   BlastMaskFree(mask_loc);
   /* Return the new mask with protein coordinates */
   *mask_ptr = head_mask;
}

void BlastMaskProteinToDNA(BlastMask** mask_ptr, TSeqLocVector &slp)
{
   BlastMask* mask_loc;
   BlastSeqLoc* loc;
   DoubleInt* dip;
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
         dip = (DoubleInt*) loc->ptr;
         if (frame < 0)	{
            to = dna_length - CODON_LENGTH*dip->i1 + frame;
            from = dna_length - CODON_LENGTH*dip->i2 + frame + 1;
         } else {
            from = CODON_LENGTH*dip->i1 + frame - 1;
            to = CODON_LENGTH*dip->i2 + frame - 1;
         }
         dip->i1 = from;
         dip->i2 = to;
      }
   }
}

END_SCOPE(blast)
END_NCBI_SCOPE
