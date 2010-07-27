/* $Id$
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
 */

/** @file blast_engine.c
 * Function calls to actually perform a BLAST search (high level).
 * The hierarchy of function calls, starting from
 * the top level in the BLAST core, is described below.
 * <pre>
 * Preliminary stage of the BLAST search:
 *
 *    Blast_RunPreliminarySearch 
 *        BLAST_GapAlignSetUp
 *        BLAST_PreliminarySearchEngine
 *            if (RPS BLAST) {
 *                s_RPSPreliminarySearchEngine
 *                    s_BlastSearchEngineCore
 *            } else {
 *                for (all sequences in the database) 
 *                    s_BlastSearchEngineCore
 *            }
 * 
 * Full BLAST search, including preliminary search and traceback:
 *
 *    Blast_RunFullSearch 
 *        BLAST_GapAlignSetUp
 *        BLAST_PreliminarySearchEngine
 *        BLAST_ComputeTraceback
 *
 * </pre>
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/blast_aascan.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_nascan.h>
#include <algo/blast/core/blast_sw.h>
#include <algo/blast/core/aa_ungapped.h>
#include <algo/blast/core/na_ungapped.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/phi_gapalign.h>
#include <algo/blast/core/phi_extend.h>
#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/mb_indexed_lookup.h>
#include <algo/blast/core/gencode_singleton.h>
#include "blast_gapalign_priv.h"

/** Converts nucleotide coordinates to protein */
#define CONV_NUCL2PROT_COORDINATES(length) (length) / CODON_LENGTH

NCBI_XBLAST_EXPORT const int   kBlastMajorVersion = 2;
NCBI_XBLAST_EXPORT const int   kBlastMinorVersion = 2;
NCBI_XBLAST_EXPORT const int   kBlastPatchVersion = 24;
NCBI_XBLAST_EXPORT const char* kBlastReleaseDate = "Aug-02-2010";

/** Structure to be passed to s_BlastSearchEngineCore, containing pointers 
    to various preallocated structures and arrays. */
typedef struct BlastCoreAuxStruct {

    Blast_ExtendWord* ewp; /**< Structure for keeping track of diagonal
                               information for initial word matches */
    BlastWordFinderType WordFinder; /**< Word finder function pointer */
    BlastGetGappedScoreType GetGappedScore; /**< Gapped extension function
                                              pointer */
    BlastInitHitList* init_hitlist; /**< Placeholder for HSPs after 
                                        ungapped extension */
    BlastOffsetPair* offset_pairs; /**< Array of offset pairs for initial seeds. */
    Uint1* translation_buffer; /**< Placeholder for translated subject
                                   sequences */
    Uint1* translation_table; /**< Translation table for forward strand */
    Uint1* translation_table_rc; /**< Translation table for reverse 
                                     strand */
} BlastCoreAuxStruct;

/** Deallocates all memory in BlastCoreAuxStruct */
static BlastCoreAuxStruct* 
s_BlastCoreAuxStructFree(BlastCoreAuxStruct* aux_struct)
{
    BlastExtendWordFree(aux_struct->ewp);
    BLAST_InitHitListFree(aux_struct->init_hitlist);
    sfree(aux_struct->offset_pairs);
    
    sfree(aux_struct);
    return NULL;
}

/** Structure used for subject sequence split */
typedef struct SubjectSplitStruct {

    Uint1* sequence;        /**< backup of original sequence */
    SSeqRange  full_range;  /**< full sequence range */

    SSeqRange* seq_ranges;  /**< backup of original sequence range */
    Int4 num_seq_ranges;    /**< backup of original number of items in seq_ranges */
    Int4 allocated;         /**< number of seq_range allocated for subject */

    SSeqRange* hard_ranges; /**< sequence ranges for hard masking */
    Int4 num_hard_ranges;   /**< number of hard masking ranges */
    Int4 hm_index;          /**< the current hard masking range index*/

    SSeqRange* soft_ranges; /**< sequence ranges for soft masking */
    Int4 num_soft_ranges;   /**< number of soft masking ranges */
    Int4 sm_index;          /**< the current soft masking range index*/

    Int4 offset;            /**< the offset of current chunk */
    Int4 next;              /**< the offset of next chunk */

} SubjectSplitStruct;

static void s_BackupSubject(BLAST_SequenceBlk* subject,
                            SubjectSplitStruct* backup)
{
    if (backup->sequence) return;

    backup->sequence = subject->sequence;
    backup->full_range.left = 0;
    backup->full_range.right = subject->length;

    backup->seq_ranges = subject->seq_ranges;
    backup->num_seq_ranges = subject->num_seq_ranges;
    backup->allocated = 0;

    backup->hard_ranges = &(backup->full_range);
    backup->num_hard_ranges = 1;
    backup->hm_index = 0;

    backup->soft_ranges = &(backup->full_range);
    backup->num_soft_ranges = 1;
    backup->sm_index = 0;

    if (subject->mask_type == DB_MASK_SOFT) {
        ASSERT (backup->seq_ranges);
        ASSERT (backup->num_seq_ranges >= 1);
        backup->soft_ranges = backup->seq_ranges;
        backup->num_soft_ranges = backup->num_seq_ranges;
    } else if (subject->mask_type == DB_MASK_HARD) {
        ASSERT (backup->seq_ranges);
        ASSERT (backup->num_seq_ranges >= 1);
        backup->hard_ranges = backup->seq_ranges;
        backup->num_hard_ranges = backup->num_seq_ranges;
    } 

    backup->offset = backup->hard_ranges[0].left;
    backup->next = backup->offset;
    subject->chunk = -1; 

}

static void s_AllocateSeqRange(BLAST_SequenceBlk* subject,
                               SubjectSplitStruct* backup,
                               Int4 num_seq_ranges)
{
    ASSERT(num_seq_ranges >= 1);
    subject->num_seq_ranges = num_seq_ranges;
    if (backup->allocated >= num_seq_ranges) return;
    if (backup->allocated) {
        sfree(subject->seq_ranges);
    }

    backup->allocated = num_seq_ranges;
    subject->seq_ranges = (SSeqRange *) calloc(backup->allocated,
                                   sizeof(SSeqRange));
}

static void s_RestoreSubject(BLAST_SequenceBlk* subject,
                             SubjectSplitStruct* backup)
{
    if (! backup->sequence) return;

    subject->sequence = backup->sequence;
    subject->length = backup->full_range.right;

    if (backup->allocated) {
        sfree(subject->seq_ranges);
    }
    subject->seq_ranges = backup->seq_ranges;
    subject->num_seq_ranges = backup->num_seq_ranges;

    backup->sequence = NULL;
}

const Int2 SUBJECT_SPLIT_DONE = 0;      /**< return value indicating hitting the end */
const Int2 SUBJECT_SPLIT_OK = 1;        /**< return value indicating OK */
const Int2 SUBJECT_SPLIT_NO_RANGE = 2;  /**< return value indicating all masked out */

static Int2 s_GetNextSubjectChunk(BLAST_SequenceBlk* subject,
                                  SubjectSplitStruct *backup,
                                  Boolean is_nucleotide)
{
    int start, len, i;

    ASSERT(subject);
    ASSERT(backup);

    if (backup->next >= backup->full_range.right) return SUBJECT_SPLIT_DONE;

    backup->offset = backup->next;
    subject->sequence = backup->sequence + ((is_nucleotide) ?
                        backup->offset /COMPRESSION_RATIO : backup->offset);

    if (backup->offset + MAX_DBSEQ_LEN < 
        backup->hard_ranges[backup->hm_index].right) {

        subject->length = MAX_DBSEQ_LEN;
        backup->next += MAX_DBSEQ_LEN - DBSEQ_CHUNK_OVERLAP;

    } else {

        subject->length = backup->hard_ranges[backup->hm_index].right 
                        - backup->offset;
        backup->hm_index++;
        backup->next = (backup->hm_index < backup->num_hard_ranges) ?
                        backup->hard_ranges[backup->hm_index].left :
                        backup->full_range.right;
    }

    (subject->chunk)++;

    /* if no chunking is performed */
    if (backup->offset == 0 && backup->next == backup->full_range.right) {
        subject->seq_ranges = backup->soft_ranges;
        subject->num_seq_ranges = backup->num_soft_ranges;
        return SUBJECT_SPLIT_OK;
    }

    /* if soft masking is off */
    if (subject->mask_type != DB_MASK_SOFT) {
        s_AllocateSeqRange(subject, backup, 1);
        subject->seq_ranges[0].left = 0;
        subject->seq_ranges[0].right = subject->length;
        return SUBJECT_SPLIT_OK;
    }
      
    /* soft masking is on, sequence is chunked, must re-allocate and adjust */
    start = backup->offset;
    len = start + subject->length;
    i = backup->sm_index;

    while (backup->soft_ranges[i].right < start) ++i;
    start = i;

    while (i < backup->num_soft_ranges
        && backup->soft_ranges[i].left < len) ++i;
    len = i - start;
    backup->sm_index = i - 1;

    ASSERT(len >= 0);
    ASSERT(backup->sm_index >= 0);

    if (len == 0) return SUBJECT_SPLIT_NO_RANGE;

    s_AllocateSeqRange(subject, backup, len);

    for (i=0; i<len; i++) {
        subject->seq_ranges[i].left = backup->soft_ranges[i+start].left - backup->offset;
        subject->seq_ranges[i].right = backup->soft_ranges[i+start].right - backup->offset;
    }

    if (subject->seq_ranges[0].left < 0) 
        subject->seq_ranges[0].left = 0;
    if (subject->seq_ranges[len-1].right > subject->length)
        subject->seq_ranges[len-1].right = subject->length;

    return SUBJECT_SPLIT_OK;
}

/** Adjust HSP coordinates for out-of-frame gapped extension.
 * @param program One of blastx or tblastn [in]
 * @param init_hitlist List of hits after ungapped extension [in]
 * @param query_info Query information containing context offsets;
 *                   needed for blastx only [in]
 * @param subject_frame Frame of the subject sequence; tblastn only [in]
 * @param subject_length Length of the original nucleotide subject sequence;
 *                       tblastn only [in]
 * @param offset Shift in the subject sequence protein coordinates [in]
 */
static void 
s_TranslateHSPsToDNAPCoord(EBlastProgramType program, 
                           BlastInitHitList* init_hitlist, 
                           const BlastQueryInfo* query_info,
                           Int2 subject_frame, Int4 subject_length, 
                           Int4 offset)
{
    BlastInitHSP * init_hsp = 0;
    Int4 index = 0;

    for(index = 0; index < init_hitlist->total; ++index) {
        BlastContextInfo * contexts = query_info->contexts;
        init_hsp = &init_hitlist->init_hsp_array[index];
        
        if (program == eBlastTypeBlastx) {
            Int4 context_idx    = 0; /* Index of this HSP's context */
            Int4 frame_idx      = 0; /* Index of this frame within set of
                                        frames with same query and sign */
            Int4 init_frame_idx = 0; /* First frame of this query */
            Int4 frame_pos      = 0; /* Start of this frame in DNA */
           
            /* Find context containing this HSP */
            context_idx = 
               BSearchContextInfo(init_hsp->offsets.qs_offsets.q_off, 
                                  query_info);
           
            frame_idx = context_idx % CODON_LENGTH;
            init_frame_idx = context_idx - frame_idx;
           
            frame_pos = contexts[init_frame_idx].query_offset + frame_idx;
           
            init_hsp->offsets.qs_offsets.q_off =
                (init_hsp->offsets.qs_offsets.q_off -
                 contexts[context_idx].query_offset) * CODON_LENGTH + frame_pos;
           
            init_hsp->ungapped_data->q_start =
                (init_hsp->ungapped_data->q_start -
                 contexts[context_idx].query_offset) * CODON_LENGTH + frame_pos;
        } else {
            init_hsp->offsets.qs_offsets.s_off += offset;
            init_hsp->ungapped_data->s_start += offset;
            if (subject_frame > 0) {
                init_hsp->offsets.qs_offsets.s_off = 
                    (init_hsp->offsets.qs_offsets.s_off * CODON_LENGTH) + 
                    subject_frame - 1;
                init_hsp->ungapped_data->s_start = 
                    (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
                    subject_frame - 1;
            } else {
                init_hsp->offsets.qs_offsets.s_off = 
                    (init_hsp->offsets.qs_offsets.s_off * CODON_LENGTH) + 
                    subject_length - subject_frame;
                init_hsp->ungapped_data->s_start = 
                    (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
                    subject_length - subject_frame;
            }
        }
    }
    Blast_InitHitListSortByScore(init_hitlist);
}

/** Set up context offsets for the auxiliary BlastQueryInfo structure that is
 * created for the concatenated database in RPS BLAST search. Calls the public 
 * function OffsetArrayToContextOffsets with a blastp program, because subjects
 * are protein sequences. This guarantees that all frames are set to 0.
 * @param info The preallocated structure [in] [out]
 * @param new_offsets The array context offsets to fill [in]
 */
static void
s_RPSOffsetArrayToContextOffsets(BlastQueryInfo    * info,
                                 Int4              * new_offsets)
{
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    OffsetArrayToContextOffsets(info, new_offsets, kProgram);
}

/** Searches only one context of a database sequence, but does all chunks if it is split.
 * @param program_number BLAST program type [in]
 * @param query Query sequence structure [in]
 * @param query_info Query information [in]
 * @param subject Subject sequence structure [in]
 * @param orig_length original length of query before translation [in]
 * @param lookup Lookup table [in]
 * @param gap_align Structure for gapped alignment information [in]
 * @param score_params Scoring parameters [in]
 * @param word_params Initial word finding and ungapped extension 
 *                    parameters [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param diagnostics Hit counts and other diagnostics [in] [out]
 * @param aux_struct Structure containing different auxiliary data and memory
 *                   for the preliminary stage of the BLAST search [in]
 * @param hsp_list_out_ptr List of HSPs found for a given subject sequence [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 *                   search [in, optional]
 * @param progress_info contains information about the progress of the current
 *                   BLAST search [in|out]
 */

static Int2
s_BlastSearchEngineOneContext(EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, Int4 orig_length, LookupTableWrap* lookup, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params, 
        const BlastInitialWordParameters* word_params, 
        const BlastExtensionParameters* ext_params, 
        const BlastHitSavingParameters* hit_params, 
        BlastDiagnostics* diagnostics,
        BlastCoreAuxStruct* aux_struct,
        BlastHSPList** hsp_list_out_ptr,
        TInterruptFnPtr interrupt_search, 
        SBlastProgress* progress_info)
{
    Int2 status = 0; /* return value */
    BlastHSPList* combined_hsp_list = NULL;
    BlastHSPList* hsp_list = NULL;
    BlastInitHitList* init_hitlist = aux_struct->init_hitlist;
    BlastScoringOptions* score_options = score_params->options;
    BlastUngappedStats* ungapped_stats = NULL;
    BlastGappedStats* gapped_stats = NULL;
    Int4 **matrix = (gap_align->positionBased) ?
                     gap_align->sbp->psi_matrix->pssm->data :
                     gap_align->sbp->matrix->data;
    const Boolean kTranslatedSubject = 
       (Blast_SubjectIsTranslated(program_number) || program_number == eBlastTypeRpsTblastn);
    const Boolean kNucleotide = (program_number == eBlastTypeBlastn ||
       program_number == eBlastTypePhiBlastn);
    const int kHspNumMax = BlastHspNumMax(score_options->gapped_calculation, hit_params->options);
    const int kScanSubjectOffsetArraySize = GetOffsetArraySize(lookup);
    Int4 overlap;

    SubjectSplitStruct backup; 
    backup.sequence = NULL;

    if (diagnostics) {
        ungapped_stats = diagnostics->ungapped_stat;
        gapped_stats = diagnostics->gapped_stat;
    }

    s_BackupSubject(subject, &backup);

    while (TRUE) {
        status = s_GetNextSubjectChunk(subject, &backup, kNucleotide);
        if (status == SUBJECT_SPLIT_DONE) break;
        if (status == SUBJECT_SPLIT_NO_RANGE) continue;
        ASSERT(status == SUBJECT_SPLIT_OK);
        ASSERT(subject->num_seq_ranges >= 1);
        ASSERT(subject->seq_ranges);

        /* Delete if not done in last loop iteration to prevent memory leak. */
        hsp_list = Blast_HSPListFree(hsp_list);

        BlastInitHitListReset(init_hitlist);

        if (aux_struct->WordFinder) {
            aux_struct->WordFinder(subject, query, query_info, lookup, matrix, 
                                   word_params, aux_struct->ewp, 
                                   aux_struct->offset_pairs, 
                                   kScanSubjectOffsetArraySize,
                                   init_hitlist, ungapped_stats);

            if (init_hitlist->total == 0) continue;
        }

        if (score_options->gapped_calculation) {
            Int4 prot_length = 0;
            if (score_options->is_ooframe) {
                /* Convert query offsets in all HSPs into the mixed-frame  
                   coordinates */
                s_TranslateHSPsToDNAPCoord(program_number, init_hitlist, 
                       query_info, subject->frame, orig_length, backup.offset);
            if (kTranslatedSubject) {
                prot_length = subject->length;
                subject->length = orig_length;
            }
        }
        /** NB: If queries are concatenated, HSP offsets must be adjusted
          * inside the following function call, so coordinates are
          * relative to the individual contexts (i.e. queries, strands or
          * frames). Contexts should also be filled in HSPs when they 
          * are saved.
          */
        /* fence_hit is null, since this is only for prelim stage. */
        status = aux_struct->GetGappedScore(program_number, query, query_info, 
                    subject, gap_align, score_params, ext_params, hit_params, 
                    init_hitlist, &hsp_list, gapped_stats, NULL);
        if (status) break;

        /* Removes redundant HSPs. */
        Blast_HSPListPurgeHSPsWithCommonEndpoints(program_number, hsp_list);

        /* For nucleotide search, if match score is = 2, the odd scores
           are rounded down to the nearest even number. */
        Blast_HSPListAdjustOddBlastnScores(hsp_list, score_options->gapped_calculation, gap_align->sbp);

        Blast_HSPListSortByScore(hsp_list);

        if (score_options->is_ooframe && kTranslatedSubject)
            subject->length = prot_length;
        } else {
            BLAST_GetUngappedHSPList(init_hitlist, query_info, subject, 
                    hit_params->options, &hsp_list);
        }

        if (hsp_list->hspcnt == 0) continue;

        /* The subject ordinal id is not yet filled in this HSP list */
        hsp_list->oid = subject->oid;

        /* check for interrupt */
        if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
            combined_hsp_list = Blast_HSPListFree(combined_hsp_list);
            BlastInitHitListReset(init_hitlist);
            status = BLASTERR_INTERRUPTED;
            break;
        }

        Blast_HSPListAdjustOffsets(hsp_list, backup.offset);
        overlap = (backup.offset == backup.hard_ranges[backup.hm_index].left) ?
                  0 : DBSEQ_CHUNK_OVERLAP;
        status = Blast_HSPListsMerge(&hsp_list, &combined_hsp_list,  
                     kHspNumMax, &(backup.offset), INT4_MIN,
                     overlap, score_options->gapped_calculation);
    } /* End loop on chunks of subject sequence */

    s_RestoreSubject(subject, &backup);

    hsp_list = Blast_HSPListFree(hsp_list);  /* In case this was not freed in above loop. */

    *hsp_list_out_ptr = combined_hsp_list;

    return status;
}

/** Clean up function for s_BlastSearchEngineCore
 * @param program_number BLAST program type [in]
 * @param query_info Query information structure local to
 * s_BlastSearchEngineCore, which may or may not be deallocated [in]
 * @param query_info_in Query information [in]
 * @param translation_buffer buffer containing translated sequence data [in]
 * @param frame_offsets_a FIXME
 */
static void
s_BlastSearchEngineCoreCleanUp(EBlastProgramType program_number,
        BlastQueryInfo* query_info,
        const BlastQueryInfo* query_info_in,
        Uint1* translation_buffer,
        Int4* frame_offsets_a)
{
    /* Free the local query info structure when needed (in RPS BLAST). */
    if (query_info != query_info_in)
        BlastQueryInfoFree(query_info);

    /* Free translation buffer and frame offsets, except for RPS tblastn,
     * where they are taken from different structures, and hence shouldn't 
     * be freed here. 
     */
    if (program_number != eBlastTypeRpsTblastn) {
        if (translation_buffer) {
            sfree(translation_buffer);
        }
    }

    if (frame_offsets_a) {
        sfree(frame_offsets_a);
    }
}

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 * @param program_number BLAST program type [in]
 * @param query Query sequence structure [in]
 * @param query_info_in Query information [in]
 * @param subject Subject sequence structure [in]
 * @param lookup Lookup table [in]
 * @param gap_align Structure for gapped alignment information [in]
 * @param score_params Scoring parameters [in]
 * @param word_params Initial word finding and ungapped extension 
 *                    parameters [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param db_options Database options [in]
 * @param diagnostics Hit counts and other diagnostics [in] [out]
 * @param aux_struct Structure containing different auxiliary data and memory
 *                   for the preliminary stage of the BLAST search [in]
 * @param hsp_list_out_ptr List of HSPs found for a given subject sequence [in]
 * @param interrupt_search function callback to allow interruption of BLAST
 *                   search [in, optional]
 * @param progress_info contains information about the progress of the current
 *                   BLAST search [in|out]
 */
static Int2
s_BlastSearchEngineCore(EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, 
        BlastQueryInfo* query_info_in, 
        BLAST_SequenceBlk* subject, 
        LookupTableWrap* lookup, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params, 
        const BlastInitialWordParameters* word_params, 
        const BlastExtensionParameters* ext_params, 
        const BlastHitSavingParameters* hit_params, 
        const BlastDatabaseOptions* db_options,
        BlastDiagnostics* diagnostics,
        BlastCoreAuxStruct* aux_struct,
        BlastHSPList** hsp_list_out_ptr,
        TInterruptFnPtr interrupt_search, 
        SBlastProgress* progress_info)
{
    BlastHSPList* hsp_list_out=NULL;
    Uint1* translation_buffer = NULL;
    Int4* frame_offsets   = NULL;
    Int4* frame_offsets_a = NULL; /* Will be freed if non-null */
    BlastHitSavingOptions* hit_options = hit_params->options;
    BlastScoringOptions* score_options = score_params->options;
    Int2 status = 0;
    Uint4 context, first_context, last_context;
    BlastQueryInfo* query_info = query_info_in;
    Int4 orig_length = subject->length;

    const Boolean kTranslatedSubject = 
        (Blast_SubjectIsTranslated(program_number) || program_number == eBlastTypeRpsTblastn);
    const Boolean kNucleotide = (program_number == eBlastTypeBlastn ||
                                program_number == eBlastTypePhiBlastn);
    const int kHspNumMax = BlastHspNumMax(score_options->gapped_calculation, hit_options);

    SubjectSplitStruct backup;
    backup.sequence = NULL;

    *hsp_list_out_ptr = NULL;

    if (kTranslatedSubject) {

        s_BackupSubject(subject, &backup);
        if (subject->mask_type) {
            s_AllocateSeqRange(subject, &backup, backup.num_seq_ranges);
        } else {
            subject->num_seq_ranges = 0;
            subject->seq_ranges = NULL;
        }

        first_context = 0;
        last_context = 5;
        if (score_options->is_ooframe) {
            BLAST_GetAllTranslations(backup.sequence, eBlastEncodingNcbi2na,
                                     backup.full_range.right, 
                                     subject->gen_code_string, &translation_buffer,
                                     &frame_offsets, &subject->oof_sequence);
            subject->oof_sequence_allocated = TRUE;
            frame_offsets_a = frame_offsets;
        } else if (program_number == eBlastTypeRpsTblastn ) {
            /* For RPS tblastn, subject is actually query, which has already 
               been translated during the setup stage. */
            translation_buffer = backup.sequence - 1;
            frame_offsets_a = frame_offsets = ContextOffsetsToOffsetArray(query_info_in);
        } else {
            BLAST_GetAllTranslations(backup.sequence, eBlastEncodingNcbi2na,
                                     backup.full_range.right, 
                                     subject->gen_code_string, &translation_buffer,
                                     &frame_offsets, NULL);
            frame_offsets_a = frame_offsets;
            /* The following limits the search to plus or minus strand if desired. */
            if (subject->subject_strand == 1) {
                first_context = 0;
                last_context = 2;
            } else if (subject->subject_strand == 2) {
                first_context = 3;
                last_context = 5;
            }
        }
    } else if (kNucleotide) {
        first_context = 1;
        last_context = 1;
    } else {
        first_context = 0;
        last_context = 0;
    }


    /* Substitute query info by concatenated database info for RPS BLAST search */
    if (Blast_ProgramIsRpsBlast(program_number)) {
        BlastRPSLookupTable* lut = (BlastRPSLookupTable*) lookup->lut;
        query_info = BlastQueryInfoNew(eBlastTypeRpsBlast, lut->num_profiles);
        /* Since this will really be "subject info", not "query info",
           pass program argument such that all frames will be set to 0. */
        s_RPSOffsetArrayToContextOffsets(query_info, lut->rps_seq_offsets);
    }

    /* Loop over frames of the subject sequence */
    for (context=first_context; context<=last_context; context++) {
        BlastHSPList* hsp_list_for_chunks = NULL;

        if (kTranslatedSubject) {
            Uint4 i;
            subject->frame = BLAST_ContextToFrame(eBlastTypeBlastx, context);
            subject->sequence = translation_buffer + frame_offsets[context] + 1;
            subject->length = frame_offsets[context+1] - frame_offsets[context] - 1;

            /* perform per-context mask translation */
            if (context == 0) { /* first positive context */
                for (i = 0; i < subject->num_seq_ranges; i++) {
                    subject->seq_ranges[i].left =
                        CONV_NUCL2PROT_COORDINATES(backup.seq_ranges[i].left);
                    subject->seq_ranges[i].right =
                        CONV_NUCL2PROT_COORDINATES(backup.seq_ranges[i].right);
                }
            } else if (context == 3) { /* first negative context */
                for (i = 0; i < subject->num_seq_ranges; i++) {
                    subject->seq_ranges[subject->num_seq_ranges-i-1].left = 
                        subject->length - CONV_NUCL2PROT_COORDINATES(backup.seq_ranges[i].right);
                    subject->seq_ranges[subject->num_seq_ranges-i-1].right = 
                        subject->length - CONV_NUCL2PROT_COORDINATES(backup.seq_ranges[i].left);
                }
            }
        } else {
            subject->frame = context;
        }

        status = s_BlastSearchEngineOneContext(program_number, query, query_info, 
                                               subject, orig_length, lookup, 
                                               gap_align, score_params, 
                                               word_params, ext_params, 
                                               hit_params, diagnostics, 
                                               aux_struct, &hsp_list_for_chunks,
                                               interrupt_search, progress_info);
        if (status != 0)  break;
     
        if (Blast_HSPListAppend(&hsp_list_for_chunks, &hsp_list_out, kHspNumMax)) {
            status = 1;
            break;
        }
      
        /* if searching was interrupted, delete accumulated results
           but continue execution so temporary structures get freed */
        if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
            status = BLASTERR_INTERRUPTED;
            break;
        }
    } /* End loop on frames */

    /* restore mask ranges  */
    if (kTranslatedSubject) {
        s_RestoreSubject(subject, &backup);
    }

    if (status) {
        hsp_list_out = Blast_HSPListFree(hsp_list_out);
        s_BlastSearchEngineCoreCleanUp(program_number, query_info, 
                                       query_info_in, translation_buffer, 
                                       frame_offsets_a);
        return status;
    }

    if (hit_params->link_hsp_params) {
        status = BLAST_LinkHsps(program_number, hsp_list_out, query_info,
                  subject->length, gap_align->sbp, hit_params->link_hsp_params, 
                  score_options->gapped_calculation);
    } else if (!Blast_ProgramIsPhiBlast(program_number) &&
              !Blast_ProgramIsRpsBlast(program_number)) {
        /* Calculate e-values for all HSPs. Skip this step
           for PHI and RPS BLAST, since calculating the E values 
           requires precomputation that has not been done yet */
        status = Blast_HSPListGetEvalues(query_info, hsp_list_out, 
                                         score_options->gapped_calculation, 
                                         gap_align->sbp, 0, 1.0);
    }
    
    /* Discard HSPs that don't pass the e-value test. */
    status = Blast_HSPListReapByEvalue(hsp_list_out, hit_options);

    /* If there are no HSPs left, destroy the HSP list too. */
    if (hsp_list_out && hsp_list_out->hspcnt == 0)
        *hsp_list_out_ptr = hsp_list_out = Blast_HSPListFree(hsp_list_out);

    if (diagnostics && diagnostics->gapped_stat && hsp_list_out && hsp_list_out->hspcnt > 0) {
        BlastGappedStats* gapped_stats = diagnostics->gapped_stat;
        ++gapped_stats->num_seqs_passed;
        gapped_stats->good_extensions += hsp_list_out->hspcnt;
    }

    s_BlastSearchEngineCoreCleanUp(program_number, query_info, query_info_in,
                                   translation_buffer, frame_offsets_a);
    
    *hsp_list_out_ptr = hsp_list_out;

    return status;
}

/** Fills the output information about the cutoffs uses in a BLAST search. 
 * @param return_cutoffs Structure for saving cutoffs information [in] [out]
 * @param score_params Scoring parameters, containing the scaling factor [in]
 * @param word_params Initial word parameters [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Hit saving parameters [in]
 */
static Int2 
s_FillReturnCutoffsInfo(BlastRawCutoffs* return_cutoffs, 
                        const BlastScoringParameters* score_params, 
                        const BlastInitialWordParameters* word_params, 
                        const BlastExtensionParameters* ext_params,
                        const BlastHitSavingParameters* hit_params)
{
    /* since the cutoff score here will be used for display
      purposes, strip out any internal scaling of the scores 
    
      If this was a multi-query search, use the least stringent
      cutoff and most generous dropoff value among all the 
      possible sequences */

    Int4 scale_factor = (Int4)score_params->scale_factor;

    if (!return_cutoffs)
      return -1;

    return_cutoffs->x_drop_ungapped = word_params->x_dropoff_max / scale_factor;
    return_cutoffs->x_drop_gap = ext_params->gap_x_dropoff / scale_factor;
    return_cutoffs->x_drop_gap_final = ext_params->gap_x_dropoff_final / 
                                                        scale_factor;
    return_cutoffs->ungapped_cutoff = word_params->cutoff_score_min / 
                                                        scale_factor;
    return_cutoffs->cutoff_score = hit_params->cutoff_score_min / scale_factor;

    return 0;
}

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param seq_src Sequence source information, with callbacks to get 
 *             sequences, their lengths, etc. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_params Parameters for initial word finding and ungapped 
 *                    extension. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
static Int2 
s_BlastSetUpAuxStructures(const BlastSeqSrc* seq_src,
    LookupTableWrap* lookup_wrap,    
    const BlastInitialWordParameters* word_params,
    const BlastExtensionOptions* ext_options,
    const BlastHitSavingOptions* hit_options,
    BLAST_SequenceBlk* query, BlastCoreAuxStruct** aux_struct_ptr)
{
    Int2 status = 0;
    BlastCoreAuxStruct* aux_struct;
    Boolean blastp = (lookup_wrap->lut_type == eAaLookupTable ||
                     lookup_wrap->lut_type == eCompressedAaLookupTable);
    Boolean rpsblast = (lookup_wrap->lut_type == eRPSLookupTable);
    Boolean indexed_mb_lookup = (lookup_wrap->lut_type == eIndexedMBLookupTable);
    Boolean phi_lookup = (lookup_wrap->lut_type == ePhiLookupTable ||
                         lookup_wrap->lut_type == ePhiNaLookupTable);
    Boolean smith_waterman = 
                 (ext_options->ePrelimGapExt == eSmithWatermanScoreOnly);
    Int4 offset_array_size = GetOffsetArraySize(lookup_wrap);

    ASSERT(seq_src);

    *aux_struct_ptr = aux_struct = (BlastCoreAuxStruct*)
      calloc(1, sizeof(BlastCoreAuxStruct));

    if ((status = BlastExtendWordNew(query->length, word_params, 
                                    &aux_struct->ewp)) != 0)
      return status;

    if (smith_waterman) {
        aux_struct->WordFinder = NULL;
    } else if (indexed_mb_lookup) {
        aux_struct->WordFinder = MB_IndexedWordFinder;
    } else if (phi_lookup) {
        aux_struct->WordFinder = PHIBlastWordFinder;
    } else if (blastp) {
        BlastChooseProteinScanSubject(lookup_wrap);
        aux_struct->WordFinder = BlastAaWordFinder;
    } else if (rpsblast) {
        aux_struct->WordFinder = BlastRPSWordFinder;
    } else {
        BlastChooseNucleotideScanSubject(lookup_wrap);
        BlastChooseNaExtend(lookup_wrap);
        aux_struct->WordFinder = BlastNaWordFinder;
    }
    
    aux_struct->offset_pairs = 
      (BlastOffsetPair*) malloc(offset_array_size * sizeof(BlastOffsetPair));
    
    aux_struct->init_hitlist = BLAST_InitHitListNew();
    /* Pick which gapped alignment algorithm to use. */
    if (phi_lookup)
        aux_struct->GetGappedScore = PHIGetGappedScore;
    else if (smith_waterman)
        aux_struct->GetGappedScore = BLAST_SmithWatermanGetGappedScore;
    else 
        aux_struct->GetGappedScore = BLAST_GetGappedScore;

    return status;
}

/** Performs the preliminary stage of an RPS BLAST search, after all set up has
 * already been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param score_params Hit scoring parameters [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param aux_struct Wrapper for auxiliary structures used in preliminary
 *                   search [in]
 * @param word_params Parameters for processing initial word hits [in]
 * @param ext_params Parameters for the gapped extension [in]
 * @param gap_align Structure containing scoring block and memory allocated
 *                  for gapped alignment. [in]
 * @param hit_params Parameters for saving the HSPs [in]
 * @param hsp_stream Placeholder for saving HSP lists [in]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search. Statistics saved only 
 *                    for the allocated parts of the structure. [in] [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 *                   search [in, optional]
 * @param progress_info contains information about the progress of the current
 *                   BLAST search [in|out]
 */
static Int2 
s_RPSPreliminarySearchEngine(EBlastProgramType program_number, 
    BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
    const BlastSeqSrc* seq_src,
    const BlastScoringParameters* score_params, 
    LookupTableWrap* lookup_wrap, BlastCoreAuxStruct* aux_struct,
    const BlastInitialWordParameters* word_params, 
    const BlastExtensionParameters* ext_params, 
    BlastGapAlignStruct* gap_align,
    const BlastHitSavingParameters* hit_params,
    BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics,
    TInterruptFnPtr interrupt_search, SBlastProgress* progress_info)
{
    BlastHSPList* hsp_list = NULL;
    Int2 status = 0;
    Int8 dbsize;
    Int4 num_db_seqs;
    BlastRPSLookupTable *lookup = (BlastRPSLookupTable *)lookup_wrap->lut;
    BLAST_SequenceBlk concat_db;
    BlastQueryInfo* one_query_info = NULL;
    BLAST_SequenceBlk* one_query = NULL;
    Int4 index;

    if ( !Blast_ProgramIsRpsBlast(program_number))
      return -1;

    /* modify scoring and gap alignment structures for
      use with RPS blast. */

    gap_align->positionBased = TRUE;
    RPSPsiMatrixAttach(gap_align->sbp, lookup->rps_pssm,
                      lookup->alphabet_size);

    /* determine the total number of residues in the db.
      This figure must also include one trailing NULL for
      each DB sequence */

    num_db_seqs = BlastSeqSrcGetNumSeqs(seq_src);
    dbsize = BlastSeqSrcGetTotLen(seq_src) + num_db_seqs;
    if (dbsize > INT4_MAX)
      return -3;

    /* Concatenate all of the DB sequences together, and pretend
      this is a large multiplexed sequence. Note that because the
      scoring is position-specific, the actual sequence data is
      not needed */

    memset(&concat_db, 0, sizeof(concat_db)); /* fill in SequenceBlk */
    concat_db.length = (Int4) dbsize;

    /* Change the table of diagonals that will be used for the
      search; we need a diag table that can fit the entire
      concatenated DB */
    BlastExtendWordFree(aux_struct->ewp);
    BlastExtendWordNew(concat_db.length, word_params, &aux_struct->ewp);

    /* Run the search; the input query is what gets scanned
      and the concatenated DB is the sequence associated with
      the score matrix. This essentially means that 'query'
      and 'subject' have opposite conventions for the search. 
    
      Note that while scores can be calculated for any alignment
      found, we have not set up any Karlin parameters or effective
      search space sizes for the concatenated DB. This means that
      E-values cannot be calculated after hits are found. */

    for (index = 0; index < query_info->num_queries; ++index) {
        /* Separate one query from the set: create an auxiliary query_info 
           structure which refers to this single query. */
        if (Blast_GetOneQueryStructs(&one_query_info, &one_query, 
                                    query_info, query, index) != 0)
           return -1;

        /* It is OK to pass NULL for the BlastDatabaseOptions argument, because it
           will not be checked for RPS BLAST program types. */
        status = (Int4)
          s_BlastSearchEngineCore(program_number, &concat_db, one_query_info, 
             one_query, lookup_wrap, gap_align, score_params, 
             word_params, ext_params, hit_params, NULL, 
             diagnostics, aux_struct, &hsp_list, interrupt_search, 
             progress_info);

        if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
            hsp_list = Blast_HSPListFree(hsp_list);
            status = BLASTERR_INTERRUPTED;
            break;
        }

        /* Save the resulting list of HSPs. 'query' and 'subject' are
           still reversed */
        if (hsp_list && hsp_list->hspcnt > 0) {
            hsp_list->query_index = index;
            /* Save the HSP list */
            BlastHSPStreamWrite(hsp_stream, &hsp_list);
        }
    }

    BlastQueryInfoFree(one_query_info);
    BlastSequenceBlkFree(one_query);

    /* Restore original settings in the gapped alignment structure. */
    RPSPsiMatrixDetach(gap_align->sbp);
    gap_align->positionBased = FALSE;

    /* Fill the cutoff values in the diagnostics structure */
    if (diagnostics && diagnostics->cutoffs) {
      s_FillReturnCutoffsInfo(diagnostics->cutoffs, score_params, word_params,
                              ext_params, hit_params);
    }

    return status;
}


Int4 
BLAST_PreliminarySearchEngine(EBlastProgramType program_number, 
    BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
    const BlastSeqSrc* seq_src, BlastGapAlignStruct* gap_align,
    BlastScoringParameters* score_params, 
    LookupTableWrap* lookup_wrap,
    const BlastInitialWordOptions* word_options, 
    BlastExtensionParameters* ext_params, 
    BlastHitSavingParameters* hit_params,
    BlastEffectiveLengthsParameters* eff_len_params,
    const PSIBlastOptions* psi_options, 
    const BlastDatabaseOptions* db_options,
    BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics,
    TInterruptFnPtr interrupt_search, SBlastProgress* progress_info)
{
    BlastCoreAuxStruct* aux_struct = NULL;
    BlastHSPList* hsp_list = NULL; 
    BlastSeqSrcGetSeqArg seq_arg;
    Int2 status = 0;
    Int8 db_length = 0;
    const BlastScoringOptions* score_options = score_params->options;
    const BlastHitSavingOptions* hit_options = hit_params->options;
    const BlastExtensionOptions* ext_options = ext_params->options;
    BlastInitialWordParameters* word_params = NULL;
    Boolean gapped_calculation = score_options->gapped_calculation;
    BlastScoreBlk* sbp = gap_align->sbp;
    BlastSeqSrcIterator* itr;
    const Boolean kNucleotide = (program_number == eBlastTypeBlastn ||
                                program_number == eBlastTypePhiBlastn);

    BlastInitialWordParametersNew(program_number, word_options, 
      hit_params, lookup_wrap, sbp, query_info, 
      BlastSeqSrcGetAvgSeqLen(seq_src), &word_params);

    if ((status = 
       s_BlastSetUpAuxStructures(seq_src, lookup_wrap, word_params, 
          ext_options, hit_options, query, &aux_struct)) != 0)
      return status;

    /* remember the current search state */
    if (progress_info)
       progress_info->stage = ePrelimSearch;

    /* For RPS BLAST, there is no loop over subject sequences, so the preliminary
      search engine is done in a separate function. */
    if (Blast_ProgramIsRpsBlast(program_number)) {
       status =         
         s_RPSPreliminarySearchEngine(program_number, query, query_info, 
            seq_src, score_params, lookup_wrap, aux_struct, word_params, 
            ext_params, gap_align, hit_params, hsp_stream, diagnostics,
            interrupt_search, progress_info);
       word_params = BlastInitialWordParametersFree(word_params);
       s_BlastCoreAuxStructFree(aux_struct);
       return status;
    }

    /* Update the parameters for linking HSPs, if necessary. */
    BlastLinkHSPParametersUpdate(word_params, hit_params, gapped_calculation);
    
    memset((void*) &seq_arg, 0, sizeof(seq_arg));

    /* Encoding is set so there are no sentinel bytes, and protein/nucleotide
      sequences are retieved in ncbistdaa/ncbi2na encodings respectively. */
    seq_arg.encoding = eBlastEncodingProtein; 

    db_length = BlastSeqSrcGetTotLen(seq_src);

    itr = BlastSeqSrcIteratorNewEx(MAX(BlastSeqSrcGetNumSeqs(seq_src)/100,1));

    /* iterate over all subject sequences */
    while ( (seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr)) 
           != BLAST_SEQSRC_EOF) {
       if (seq_arg.oid == BLAST_SEQSRC_ERROR)
           break;
       if (BlastSeqSrcGetSequence(seq_src, &seq_arg) < 0)
           continue;
       if (db_length == 0) {
           /* This is not a database search, hence need to recalculate and save
            the effective search spaces and length adjustments for all 
            queries based on the length of the current single subject 
            sequence. */
           if ((status = BLAST_OneSubjectUpdateParameters(program_number, 
                          seq_arg.seq->length, score_options, query_info, 
                          sbp, hit_params, word_params, 
                          eff_len_params)) != 0)
              return status;
      }

      /* Calculate cutoff scores for linking HSPs. Do this only for
         ungapped protein searches and ungapped translated
         searches. */
      if (hit_params->link_hsp_params && !kNucleotide &&
          !gapped_calculation) {
          CalculateLinkHSPCutoffs(program_number, query_info, sbp, 
            hit_params->link_hsp_params, word_params, db_length, 
            seq_arg.seq->length); 
      }

      if (Blast_SubjectIsTranslated(program_number)) {
          /* If the subject is translated and the BlastSeqSrc implementation
           * doesn't provide a genetic code string, use the default genetic
           * code for all subjects (as in the C toolkit) */
          if (seq_arg.seq->gen_code_string == NULL) {
              seq_arg.seq->gen_code_string = 
                  GenCodeSingletonFind(db_options->genetic_code);
          }
          ASSERT(seq_arg.seq->gen_code_string);
      }
      status = 
         s_BlastSearchEngineCore(program_number, query, query_info,
            seq_arg.seq, lookup_wrap, gap_align, score_params, word_params, 
            ext_params, hit_params, db_options, diagnostics, aux_struct, 
            &hsp_list, interrupt_search, progress_info);
      if (status) {
          break;
      }

      if (hsp_list && hsp_list->hspcnt > 0) {
         if (!gapped_calculation) {
            /* The following must be performed for any ungapped 
               search with a nucleotide database. */
               status = 
                  Blast_HSPListReevaluateUngapped(
                            program_number, hsp_list, query, 
                            seq_arg.seq, word_params, hit_params, 
                            query_info, sbp, score_params, seq_src, 
                            seq_arg.seq->gen_code_string);
               if (status) {
                  BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
                  return status;
               }
               /* Relink HSPs if sum statistics is used, because scores might
                * have changed after reevaluation with ambiguities, and there
                * will be no traceback stage where relinking is done normally.
                * If sum statistics are not used, just recalculate e-values. 
                */
               if (hit_params->link_hsp_params) {
                   status = 
                       BLAST_LinkHsps(program_number, hsp_list, query_info,
                                      seq_arg.seq->length, sbp, 
                                      hit_params->link_hsp_params, 
                                      gapped_calculation);
               } else {
                  Blast_HSPListGetEvalues(query_info, hsp_list, 
                                          gapped_calculation, 
                                          sbp, 0, 1.0);
               }
               status = Blast_HSPListReapByEvalue(hsp_list, 
                                          hit_params->options);
             
            /* Calculate and fill the bit scores, since there will be no
               traceback stage where this can be done. */
            Blast_HSPListGetBitScores(hsp_list, gapped_calculation, sbp);
         } 
         
         /* Save the results. */
         status = BlastHSPStreamWrite(hsp_stream, &hsp_list);
         if (status != 0)
            break;
      }
      
      BlastSeqSrcReleaseSequence(seq_src, &seq_arg);

      /* check for interrupt */
      if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
          status = BLASTERR_INTERRUPTED;
          break;
      }
    }
    
    hsp_list = Blast_HSPListFree(hsp_list);  /* in case we were interrupted */
    BlastSequenceBlkFree(seq_arg.seq);
    itr = BlastSeqSrcIteratorFree(itr);

    /* Fill the cutoff values in the diagnostics structure */
    if (diagnostics && diagnostics->cutoffs) {
      s_FillReturnCutoffsInfo(diagnostics->cutoffs, score_params, word_params, 
                              ext_params, hit_params);
    }

    word_params = BlastInitialWordParametersFree(word_params);
    s_BlastCoreAuxStructFree(aux_struct);
    return status;
}

Int2 
Blast_RunPreliminarySearch(EBlastProgramType program, 
    BLAST_SequenceBlk* query, 
    BlastQueryInfo* query_info, 
    const BlastSeqSrc* seq_src, 
    const BlastScoringOptions* score_options,
    BlastScoreBlk* sbp, 
    LookupTableWrap* lookup_wrap,
    const BlastInitialWordOptions* word_options, 
    const BlastExtensionOptions* ext_options,
    const BlastHitSavingOptions* hit_options,
    const BlastEffectiveLengthsOptions* eff_len_options,
    const PSIBlastOptions* psi_options, 
    const BlastDatabaseOptions* db_options, 
    BlastHSPStream* hsp_stream, 
    BlastDiagnostics* diagnostics)
{
    Int2 status = 0;
    BlastScoringParameters* score_params = NULL;/**< Scoring parameters */
    BlastExtensionParameters* ext_params = NULL;/**< Gapped extension 
                                                    parameters */
    BlastHitSavingParameters* hit_params = NULL;/**< Hit saving parameters */
    BlastEffectiveLengthsParameters* eff_len_params = NULL; /**< Parameters 
                                          for effective lengths calculations */
    BlastGapAlignStruct* gap_align = NULL; /**< Gapped alignment structure */
    
    /* Use a local diagnostics structure, because the one passed in an input 
      argument can be shared between multiple threads, so we don't want to pass
      it to the engine and have a lot of mutex contention. */
    BlastDiagnostics* local_diagnostics = Blast_DiagnosticsInit();

    if ((status = 
        BLAST_GapAlignSetUp(program, seq_src, score_options, 
                            eff_len_options, ext_options, hit_options, 
                            query_info, sbp, &score_params, &ext_params, 
                            &hit_params, &eff_len_params, &gap_align)) != 0)
      return status;
    
    if ((status=
        BLAST_PreliminarySearchEngine(program, query, query_info, 
                                      seq_src, gap_align, score_params, 
                                      lookup_wrap, word_options, 
                                      ext_params, hit_params, eff_len_params,
                                      psi_options, db_options, hsp_stream, 
                                      local_diagnostics, 0, 0)) != 0)
      return status;

    /* Do not destruct score block here */
    gap_align->sbp = NULL;
    gap_align = BLAST_GapAlignStructFree(gap_align);
    
    score_params = BlastScoringParametersFree(score_params);
    hit_params = BlastHitSavingParametersFree(hit_params);
    ext_params = BlastExtensionParametersFree(ext_params);
    eff_len_params = BlastEffectiveLengthsParametersFree(eff_len_params);
    
    /* Now update the input diagonistics structure. */
    Blast_DiagnosticsUpdate(diagnostics, local_diagnostics);
    Blast_DiagnosticsFree(local_diagnostics);

    return status;
}

/** Function to deallocate data structures allocated in Blast_RunFullSearch */
static void
s_BlastRunFullSearchCleanUp(BlastGapAlignStruct* gap_align,
                            BlastScoringParameters* score_params,
                            BlastExtensionParameters* ext_params,
                            BlastHitSavingParameters* hit_params,
                            BlastEffectiveLengthsParameters* eff_len_params)
{
    /* Do not destruct score block here */
    gap_align->sbp = NULL;
    BLAST_GapAlignStructFree(gap_align);

    BlastScoringParametersFree(score_params);
    BlastHitSavingParametersFree(hit_params);
    BlastExtensionParametersFree(ext_params);
    BlastEffectiveLengthsParametersFree(eff_len_params);
}

Int4 
Blast_RunFullSearch(EBlastProgramType program_number, 
    BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
    const BlastSeqSrc* seq_src,  BlastScoreBlk* sbp,
    const BlastScoringOptions* score_options, 
    LookupTableWrap* lookup_wrap,
    const BlastInitialWordOptions* word_options, 
    const BlastExtensionOptions* ext_options, 
    const BlastHitSavingOptions* hit_options,
    const BlastEffectiveLengthsOptions* eff_len_options,
    const PSIBlastOptions* psi_options, 
    const BlastDatabaseOptions* db_options,
    BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info, 
    BlastDiagnostics* diagnostics, BlastHSPResults** results,
    TInterruptFnPtr interrupt_search,
    SBlastProgress* progress_info)
{
    Int4 status = 0;
    BlastScoringParameters* score_params = NULL;
    BlastExtensionParameters* ext_params = NULL;
    BlastHitSavingParameters* hit_params = NULL;
    BlastEffectiveLengthsParameters* eff_len_params = NULL;
    BlastGapAlignStruct* gap_align = NULL;
    SPHIPatternSearchBlk* pattern_blk = NULL;

    if ((status = 
        BLAST_GapAlignSetUp(program_number, seq_src, score_options, 
           eff_len_options, ext_options, hit_options, query_info, sbp, 
           &score_params, &ext_params, &hit_params, &eff_len_params, 
           &gap_align)) != 0) {
       s_BlastRunFullSearchCleanUp(gap_align, score_params, ext_params, 
                                   hit_params, eff_len_params);
       return status;
    }
      
    if ((status=
        BLAST_PreliminarySearchEngine(program_number, query, query_info, 
           seq_src, gap_align, score_params, lookup_wrap, word_options, 
           ext_params, hit_params, eff_len_params, psi_options, 
           db_options, hsp_stream, diagnostics, interrupt_search, 
           progress_info)) != 0) {
       s_BlastRunFullSearchCleanUp(gap_align, score_params, ext_params, 
                                   hit_params, eff_len_params);
       return status;
    }
    
    /* Prohibit any subsequent writing to the HSP stream. */
    BlastHSPStreamClose(hsp_stream);

    if (Blast_ProgramIsPhiBlast(program_number)) {
       pattern_blk = ((SPHIPatternSearchBlk*) lookup_wrap->lut);
       pattern_blk->num_patterns_db = 
                        (Int4)diagnostics->ungapped_stat->lookup_hits;
    } 

    if ((status = 
        BLAST_ComputeTraceback(program_number, hsp_stream, query, query_info,
                               seq_src, gap_align, score_params, ext_params, 
                               hit_params, eff_len_params, db_options, 
                               psi_options, rps_info, pattern_blk, results,
                               interrupt_search, progress_info))
       != 0) {
       s_BlastRunFullSearchCleanUp(gap_align, score_params, ext_params, 
                                   hit_params, eff_len_params);
       return status;
    }

    s_BlastRunFullSearchCleanUp(gap_align, score_params, ext_params, hit_params,
                               eff_len_params);
    return status;
}
