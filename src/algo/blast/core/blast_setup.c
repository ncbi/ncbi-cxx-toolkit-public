static char const rcsid[] = "$Id$";
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_setup.c

Author: Tom Madden

Contents: Utilities initialize/setup BLAST.

$Revision$

******************************************************************************/

#include <blast_setup.h>
#include <blast_util.h>
#include <blast_filter.h>

/* BlastScoreBlkGappedFill, fills the ScoreBlkPtr for a gapped search.  
 *	Should be moved to blastkar.c (or it's successor) in the future.
*/
Int2 
BlastScoreBlkGappedFill(BLAST_ScoreBlkPtr sbp, 
   const BlastScoringOptionsPtr scoring_options, Uint1 program)
{

	if (sbp == NULL || scoring_options == NULL)
		return 1;


	if (program == blast_type_blastn)
	{
		sbp->penalty = scoring_options->penalty;
		sbp->reward = scoring_options->reward;
        BlastScoreBlkMatFill(sbp, scoring_options->matrix);
	}
	else
	{
		Int2 tmp_index;	/* loop variable. */

		sbp->read_in_matrix = TRUE;
		if (scoring_options->matrix)
			BlastScoreBlkMatFill(sbp, scoring_options->matrix);
		else
			BlastScoreBlkMatFill(sbp, "BLOSUM62");

		for (tmp_index=0; tmp_index<sbp->number_of_contexts; tmp_index++)
		{
			sbp->kbp_gap_std[tmp_index] = BlastKarlinBlkCreate();
			BlastKarlinBlkGappedCalcEx(sbp->kbp_gap_std[tmp_index], 
				scoring_options->gap_open, scoring_options->gap_extend, 
				scoring_options->decline_align, sbp->name, NULL);
		}

		sbp->kbp_gap = sbp->kbp_gap_std;
	}

	return 0;
}

/** Masks the letters in buffer.
 * @param buffer the sequence to be masked (will be modified). [out]
 * @param max_length the sequence to be masked (will be modified). [in]
 * @param is_na nucleotide if TRUE [in]
 * @param mask_loc the SeqLoc to use for masking [in] 
 * @param reverse minus strand if TRUE [in]
 * @param offset how far along sequence is 1st residuse in buffer [in]
 *
*/
static Int2
BlastSetUp_MaskTheResidues(Uint1Ptr buffer, Int4 max_length, Boolean is_na,
	ValNodePtr mask_loc, Boolean reverse, Int4 offset)
{
   DoubleIntPtr loc = NULL;
   Int2 status = 0;
   Int4 index, start, stop;
   Uint1 mask_letter;
   
   if (is_na)
      mask_letter = 14;
   else
      mask_letter = 21;
   
   for ( ; mask_loc; mask_loc = mask_loc->next) {
      loc = (DoubleIntPtr) mask_loc->data.ptrvalue;
      if (reverse)	{
         start = max_length - 1 - loc->i2;
         stop = max_length - 1 - loc->i1;
      } else {
         start = loc->i1;
         stop = loc->i2;
      }
      
      start -= offset;
      stop  -= offset;
      
      for (index = start; index <= stop; index++)
         buffer[index] = mask_letter;
   }
   
   return status;
}

Int2 BLAST_MainSetUp(Uint1 program_number,
        const QuerySetUpOptionsPtr qsup_options,
        const BlastScoringOptionsPtr scoring_options,
        const LookupTableOptionsPtr lookup_options,	
        const BlastHitSavingOptionsPtr hit_options,
        BLAST_SequenceBlkPtr query_blk,
        BlastQueryInfoPtr query_info, BlastSeqLocPtr *lookup_segments,
        BlastMaskPtr *filter_out,
        BLAST_ScoreBlkPtr *sbpp, Blast_MessagePtr *blast_message)
{
   BLAST_ScoreBlkPtr sbp;
   Boolean mask_at_hash;/* mask only for making lookup table? */
   Boolean is_na;	/* Is this nucleotide? */
   Int2 context=0, index;/* Loop variables. */
   Int2 total_num_contexts=0;/* number of different strands, sequences, etc. */
   Int2 status=0;	/* return value */
   Int4 query_length=0;	/* Length of query described by SeqLocPtr. */
   BlastSeqLocPtr filter_slp=NULL;/* SeqLocPtr computed for filtering. */
   BlastSeqLocPtr filter_slp_combined;/* Used to hold combined SeqLoc's */
   BlastSeqLocPtr loc; /* Iterator variable */
   BlastMaskPtr last_filter_out = NULL; 
   Uint1Ptr buffer;	/* holds sequence for plus strand or protein. */
   Boolean reverse; /* Indicates the strand when masking filtered locations */
   BlastMaskPtr mask_slp, next_mask_slp;/* Auxiliary locations for lower 
                                              case masks */
   Int4 context_offset;
   
   if ((status=
        BlastScoringOptionsValidate(program_number, scoring_options, blast_message)) != 0)
      return status;

   if ((status=
        LookupTableOptionsValidate(program_number, lookup_options, blast_message)) != 0)
      return status;
   
   if ((status =
        BlastHitSavingOptionsValidate(program_number, hit_options, 
                                      blast_message)) != 0)
      return status;
   
   /* At this stage query sequences are nucleotide only for blastn */
   is_na = (program_number == blast_type_blastn);
   total_num_contexts = query_info->last_context + 1;

   sbp = *sbpp;
   if (!sbp) {
      if (is_na)
         sbp = BLAST_ScoreBlkNew(BLASTNA_SEQ_CODE, total_num_contexts);
      else
         sbp = BLAST_ScoreBlkNew(BLASTAA_SEQ_CODE, total_num_contexts);
      
      /* Set the ambiguous residue before the ScoreBlk is filled. */
      if (is_na) {
         if (scoring_options->matrix && scoring_options->matrix[0] != NULLB) {
            sbp->read_in_matrix = TRUE;
         } else {
            sbp->read_in_matrix = FALSE;
         }
         BlastScoreSetAmbigRes(sbp, 'N');
      } else {
         sbp->read_in_matrix = TRUE;
         BlastScoreSetAmbigRes(sbp, 'X');
      }
      
      /* Fills in block for gapped blast. */
      status = BlastScoreBlkGappedFill(sbp, scoring_options, program_number);
      if (status)
         return status;
      
      *sbpp = sbp;
   }
   
   next_mask_slp = qsup_options->lcase_mask;
   mask_slp = NULL;
   
   for (context = query_info->first_context; 
        context <= query_info->last_context; ++context) {
      context_offset = query_info->context_offsets[context];
      buffer = &query_blk->sequence[context_offset];
      query_length = BLAST_GetQueryLength(query_info, context);

      reverse = (is_na && ((context & 1) != 0));
      index = (is_na ? context/2 : context);

      /* It is not necessary to do filtering on the reverse strand - the 
         masking locations are the same as on the forward strand */
      if (!reverse) {
         if ((status = BlastSetUp_Filter(program_number, buffer, 
                         query_length, 0, qsup_options->filter_string, 
                         &mask_at_hash, &filter_slp)))
            return status;
      
         /* Extract the mask locations corresponding to this query 
            (frame, strand), detach it from other masks.
            NB: for translated search the mask locations are expected in 
            protein coordinates. The nucleotide locations must be converted
            to protein coordinates prior to the call to BLAST_MainSetUp.
         */
         if (next_mask_slp && (next_mask_slp->index == index)) {
            mask_slp = next_mask_slp;
            next_mask_slp = mask_slp->next;
         } else {
            mask_slp = NULL;
         }
         
         /* Attach the lower case mask locations to the filter locations 
            and combine them */
         if (filter_slp && mask_slp) {
            for (loc = filter_slp; loc->next; loc = loc->next);
            loc->next = mask_slp->loc_list;
            /* Set location list to NULL, to allow safe memory deallocation */
            mask_slp->loc_list = NULL;
         }
         
         filter_slp_combined = NULL;
         CombineMaskLocations(filter_slp, &filter_slp_combined);
         filter_slp = BlastSeqLocFree(filter_slp);
         
         /* NB: for translated searches filter locations are returned in 
            protein coordinates, because the DNA lengths of sequences are 
            not available here. The caller must take care of converting 
            them back to nucleotide coordinates. */
         if (filter_slp_combined) {
            if (!last_filter_out) {
               last_filter_out = *filter_out = 
                  (BlastMaskPtr) calloc(1, sizeof(BlastMask));
            } else {
               last_filter_out->next = 
                  (BlastMaskPtr) calloc(1, sizeof(BlastMask));
               last_filter_out = last_filter_out->next;
            }
            last_filter_out->index = index;
            last_filter_out->loc_list = filter_slp_combined;
         }
      }
      if (buffer) {	
         if (!mask_at_hash) {
            if((status = 
                BlastSetUp_MaskTheResidues(buffer, query_length, is_na, 
                   filter_slp_combined, reverse, 0)))
                  return status;
         }
            
         if ((status=BlastScoreBlkFill(sbp, (CharPtr) buffer, 
                                       query_length, context)))
            return status;
      }
   }
   
   if (program_number == blast_type_blastx && 
       scoring_options->is_ooframe) {
      BLAST_InitDNAPSequence(query_blk, query_info);
   }

   *lookup_segments = NULL;
   BLAST_ComplementMaskLocations(program_number, query_info, *filter_out, 
                                 lookup_segments);

   /* Free the filtering locations if masking done for lookup table only */
   if (mask_at_hash) {
      *filter_out = BlastMaskFree(*filter_out);
   }

   /* Get "ideal" values if the calculated Karlin-Altschul params bad. */
   if (program_number == blast_type_blastx || 
       program_number == blast_type_tblastx) {
      sbp->kbp = sbp->kbp_std;
      BlastKarlinBlkStandardCalc(sbp, 0, total_num_contexts-1);
      /* Adjust the Karlin parameters for ungapped blastx/tblastx. */
      sbp->kbp = sbp->kbp_gap;
      BlastKarlinBlkStandardCalc(sbp, 0, sbp->number_of_contexts-1);
   }

   /* Why are there so many Karlin block names?? */
   sbp->kbp = sbp->kbp_std;
   sbp->kbp_gap = sbp->kbp_gap_std;

   return 0;
}
