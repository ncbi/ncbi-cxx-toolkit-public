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
 * Author: Alejandro Schaffer
 *
 */

/** @file blast_kappa.h
 * Header file for composition-based statistics
 */

#ifndef ALGO_BLAST_CORE__BLAST_KAPPA__H
#define ALGO_BLAST_CORE__BLAST_KAPPA__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_hspstream.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Top level routine to recompute alignments for each
 *  match found by the gapped BLAST algorithm
 *  A linked list of alignments is returned (param hitList); the alignments 
 *  are sorted according to the lowest E-value of the best alignment for each
 *  matching sequence; alignments for the same matching sequence
 *  are in the list consecutively regardless of the E-value of the
 *  secondary alignments. Ties in sorted order are much rarer than
 *  for the standard BLAST method, but are broken deterministically
 *  based on the index of the matching sequences in the database.
 * @param program_number the type of blast search being performed [in]
 * @param queryBlk query sequence [in]
 * @param query_info query information [in]
 * @param sbp (Karlin-Altschul) information for search [in]
 * @param subjectBlk subject sequence [in]
 * @param seqSrc used to fetch database/match sequences [in]
 * @param db_genetic_code Genetic code to use if database sequences are
 *                        translated, and there is no other guidance on
 *                        which genetic code to use [in]
 * @param thisMatch hit for further processing [in]
 * @param hsp_stream used to fetch hits for further processing [in]
 * @param scoringParams parameters used for scoring (matrix, gap costs etc.) [in]
 * @param extendParams parameters used for extension [in]
 * @param hitParams parameters used for saving hits [in]
 * @param psiOptions options related to psi-blast [in]
 * @param results All HSP results from previous stages of the search [in] [out]
 * @return 0 on success, otherwise failure.
*/

NCBI_XBLAST_EXPORT
Int2
Blast_RedoAlignmentCore(EBlastProgramType program_number,
                  BLAST_SequenceBlk* queryBlk,
                  const BlastQueryInfo* query_info,
                  BlastScoreBlk* sbp,
                  BLAST_SequenceBlk* subjectBlk,
                  const BlastSeqSrc* seqSrc,
                  Int4 db_genetic_code,
                  BlastHSPList* thisMatch,
                  BlastHSPStream* hsp_stream,
                  BlastScoringParameters* scoringParams,
                  const BlastExtensionParameters* extendParams,
                  const BlastHitSavingParameters* hitParams,
                  const PSIBlastOptions* psiOptions,
                  BlastHSPResults* results);

#ifdef __cplusplus

}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_KAPPA__H */
