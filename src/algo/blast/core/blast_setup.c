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
* ===========================================================================*/

/*****************************************************************************

File name: blast_setup.c

Author: Tom Madden

Contents: Utilities initialize/setup BLAST.

$Revision$

******************************************************************************/

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>

/** BlastScoreBlkGappedFill, fills the ScoreBlkPtr for a gapped search.  
 *      Should be moved to blastkar.c (or it's successor) in the future.
 * @param sbp Contains fields to be set, should not be NULL. [out]
 * @param scoring_options Scoring_options [in]
 * @param program_number Used to set fields on sbp [in]
 * @param query_info Query information containing context information [in]
 *
*/
static Int2
BlastScoreBlkGappedFill(BlastScoreBlk * sbp,
                        const BlastScoringOptions * scoring_options,
                        Uint1 program, BlastQueryInfo * query_info)
{
    Int2 status;

    if (sbp == NULL || scoring_options == NULL)
        return 1;

    if (program == blast_type_blastn) {

        BLAST_ScoreSetAmbigRes(sbp, 'N');
        sbp->penalty = scoring_options->penalty;
        sbp->reward = scoring_options->reward;
        if (scoring_options->matrix_path &&
            *scoring_options->matrix_path != NULLB &&
            scoring_options->matrix && *scoring_options->matrix != NULLB) {

            sbp->read_in_matrix = TRUE;
            sbp->name = strdup(scoring_options->matrix);

        } else {
            char buffer[50];
            sbp->read_in_matrix = FALSE;
            sprintf(buffer, "blastn matrix:%ld %ld",
                    (long) sbp->reward, (long) sbp->penalty);
            sbp->name = strdup(buffer);
        }
        status = BLAST_ScoreBlkMatFill(sbp, scoring_options->matrix_path);

    } else {
        Int2 tmp_index;         /* loop variable. */
        char* p = NULL;

        sbp->read_in_matrix = TRUE;
        BLAST_ScoreSetAmbigRes(sbp, 'X');
        sbp->name = p = strdup(scoring_options->matrix);
        /* protein matrices are in all caps by convention */
        while (*p != NULLB) *p++ = toupper(*p);
        status = BLAST_ScoreBlkMatFill(sbp, scoring_options->matrix_path);
        if (status)
            return status;

        for (tmp_index = query_info->first_context;
             tmp_index <= query_info->last_context; tmp_index++) {
            sbp->kbp_gap_std[tmp_index] = BLAST_KarlinBlkCreate();
            BLAST_KarlinBlkGappedCalc(sbp->kbp_gap_std[tmp_index],
                                      scoring_options->gap_open,
                                      scoring_options->gap_extend,
                                      scoring_options->decline_align,
                                      sbp->name, NULL);
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
BlastSetUp_MaskTheResidues(Uint1 * buffer, Int4 max_length, Boolean is_na,
                           ListNode * mask_loc, Boolean reverse, Int4 offset)
{
    DoubleInt *loc = NULL;
    Int2 status = 0;
    Int4 index, start, stop;
    Uint1 mask_letter;

    if (is_na)
        mask_letter = 14;
    else
        mask_letter = 21;

    for (; mask_loc; mask_loc = mask_loc->next) {
        loc = (DoubleInt *) mask_loc->ptr;
        if (reverse) {
            start = max_length - 1 - loc->i2;
            stop = max_length - 1 - loc->i1;
        } else {
            start = loc->i1;
            stop = loc->i2;
        }

        start -= offset;
        stop -= offset;

        for (index = start; index <= stop; index++)
            buffer[index] = mask_letter;
    }

    return status;
}

Int2 BLAST_MainSetUp(Uint1 program_number,
                     const QuerySetUpOptions * qsup_options,
                     const BlastScoringOptions * scoring_options,
                     const LookupTableOptions * lookup_options,
                     const BlastHitSavingOptions * hit_options,
                     BLAST_SequenceBlk * query_blk,
                     BlastQueryInfo * query_info,
                     BlastSeqLoc ** lookup_segments, BlastMask * *filter_out,
                     BlastScoreBlk * *sbpp, Blast_Message * *blast_message)
{
    BlastScoreBlk *sbp;
    Boolean mask_at_hash;       /* mask only for making lookup table? */
    Boolean is_na;              /* Is this nucleotide? */
    Int2 context = 0, index;    /* Loop variables. */
    Int2 total_num_contexts = 0;        /* number of different strands, sequences, etc. */
    Int2 status = 0;            /* return value */
    Int4 query_length = 0;      /* Length of query described by SeqLocPtr. */
    BlastSeqLoc *filter_slp = NULL;     /* SeqLocPtr computed for filtering. */
    BlastSeqLoc *filter_slp_combined;   /* Used to hold combined SeqLoc's */
    BlastSeqLoc *loc;           /* Iterator variable */
    BlastMask *last_filter_out = NULL;
    Uint1 *buffer;              /* holds sequence for plus strand or protein. */
    Boolean reverse;            /* Indicates the strand when masking filtered locations */
    BlastMask *mask_slp, *next_mask_slp;        /* Auxiliary locations for lower 
                                                   case masks */
    Int4 context_offset;

    if ((status =
         BlastScoringOptionsValidate(program_number, scoring_options,
                                     blast_message)) != 0)
        return status;

    if ((status =
         LookupTableOptionsValidate(program_number, lookup_options,
                                    blast_message)) != 0)
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
            sbp = BlastScoreBlkNew(BLASTNA_SEQ_CODE, total_num_contexts);
        else
            sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, total_num_contexts);

        /* Fills in block for gapped blast. */
        status = BlastScoreBlkGappedFill(sbp, scoring_options, program_number,
                                         query_info);
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
        index = (is_na ? context / 2 : context);

        /* It is not necessary to do filtering on the reverse strand - the 
           masking locations are the same as on the forward strand */
        if (!reverse) {
            if ((status = BlastSetUp_Filter(program_number, buffer,
                                            query_length, 0,
                                            qsup_options->filter_string,
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
                for (loc = filter_slp; loc->next; loc = loc->next) ;
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
                        (BlastMask *) calloc(1, sizeof(BlastMask));
                } else {
                    last_filter_out->next =
                        (BlastMask *) calloc(1, sizeof(BlastMask));
                    last_filter_out = last_filter_out->next;
                }
                last_filter_out->index = index;
                last_filter_out->loc_list = filter_slp_combined;
            }
        }
        if (buffer) {
            if (!mask_at_hash) {
                if ((status =
                     BlastSetUp_MaskTheResidues(buffer, query_length, is_na,
                                                filter_slp_combined, reverse,
                                                0)))
                    return status;
            }

            if ((status = BLAST_ScoreBlkFill(sbp, (char *) buffer,
                                             query_length, context)))
                return status;
        }
    }

    if (program_number == blast_type_blastx && scoring_options->is_ooframe) {
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
        BLAST_KarlinBlkStandardCalc(sbp, query_info->first_context,
                                    query_info->last_context);
        /* Adjust the Karlin parameters for ungapped blastx/tblastx. */
        sbp->kbp = sbp->kbp_gap;
        BLAST_KarlinBlkStandardCalc(sbp, query_info->first_context,
                                    query_info->last_context);
    }

    /* Why are there so many Karlin block names?? */
    sbp->kbp = sbp->kbp_std;
    sbp->kbp_gap = sbp->kbp_gap_std;

    return 0;
}
