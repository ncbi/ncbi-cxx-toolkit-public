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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file phi_gapalign.h
 * Function prototypes used for PHI BLAST gapped extension and gapped extension
 * with traceback.
 */

#ifndef ALGO_BLAST_CORE__PHI_GAPALIGN_H
#define ALGO_BLAST_CORE__PHI_GAPALIGN_H

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/pattern.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Perform a gapped alignment with traceback for PHI BLAST
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param gap_align The gapped alignment structure [in] [out]
 * @param score_params Scoring parameters [in]
 * @param q_start Offset in query where to start alignment [in]
 * @param s_start Offset in subject where to start alignment [in]
 * @param query_length Maximal allowed extension in query [in]
 * @param subject_length Maximal allowed extension in subject [in]
 * @param q_pat_length Length of this pattern in query. [in]
 * @param s_pat_length Length of this pattern in subject. [in]
 * @param pattern_blk Detailed pattern information. [in]
 * @return Status, 0 on success, -1 on failure.
 */
Int2 PHIGappedAlignmentWithTraceback(Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length,
        Int4 q_pat_length, Int4 s_pat_length,
        SPHIPatternSearchBlk *pattern_blk);

/** Preliminary gapped alignment for PHI BLAST.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence block [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [in]
 * @param subject The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param ext_params Parameters related to extensions (not used) [in]
 * @param hit_params Parameters related to saving hits [in]
 * @param init_hitlist List of initial HSPs, including offset pairs and
 *                     pattern match lengths [in]
 * @param hsp_list_ptr Structure containing all saved HSPs [out]
 * @param gapped_stats Return statistics (not filled if NULL) [out]
 * @return Status, 0 on success, -1 on failure.
 */
Int2 PHIGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__PHI_GAPALIGN_H */
