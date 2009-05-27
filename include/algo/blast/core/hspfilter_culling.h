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
 * Author:  Tom Madden
 *
 */

/** @file hspfilter_culling.h
 * Implementation of the BlastHSPWriter interface to perform
 * culling.  The implementation is based upon the algorithm
 * described in [1], though the original implementation only
 * applied to the preliminary stage and was later rewritten 
 * to use interval trees by Jason Papadopoulos.
 *
 * [1] Berman P, Zhang Z, Wolf YI, Koonin EV, Miller W. Winnowing sequences from a
 * database search. J Comput Biol. 2000 Feb-Apr;7(1-2):293-302. PubMed PMID:
 * 10890403.
 */

#ifndef ALGO_BLAST_CORE__HSPFILTER_CULLING_H
#define ALGO_BLAST_CORE__HSPFILTER_CULLING_H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hspfilter.h>
#include <algo/blast/core/blast_hits.h>
#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Keeps parameters used in best hit algorithm.*/
typedef struct BlastHSPCullingParams {
   EBlastProgramType program;/**< program type */
   Int4 prelim_hitlist_size; /**< number of hits saved during preliminary
                                  part of search. */
   Int4 hsp_num_max;         /**< number of HSPs to save per db sequence.*/
   Int4 culling_max;	     /**< number of HSPs allowed per query region. */
} BlastHSPCullingParams;

/** create a set of parameters 
 * @param program Blast program type.[in]
 * @param hit_options field hitlist_size and hsp_num_max needed, a pointer to 
 *      this structure will be stored on resulting structure.[in]
 * @param overhang Specifies the ratio of overhang to length, which is used to
        determine if hit A is contained in hit B
 * @return the pointer to the allocated parameter
 */
NCBI_XBLAST_EXPORT
BlastHSPCullingParams*
BlastHSPCullingParamsNew(const BlastHitSavingOptions* hit_options,
                         const BlastHSPCullingOptions* best_hit_opts,
                         Int4 compositionBasedStats,
                         Boolean gapped_calculation);

/** Deallocates the BlastHSPCullingParams structure passed in
 * @param opts structure to deallocate [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastHSPCullingParams*
BlastHSPCullingParamsFree(BlastHSPCullingParams* opts);

/** WriterInfo and PipeInfo to create a best hit writer/pipe
 * @param params Specifies writer parameters. [in]
 * @return the newly allocated writer/pipe info
 */
NCBI_XBLAST_EXPORT
BlastHSPWriterInfo* 
BlastHSPCullingInfoNew(BlastHSPCullingParams* params);

NCBI_XBLAST_EXPORT
BlastHSPPipeInfo*
BlastHSPCullingPipeInfoNew(BlastHSPCullingParams* params);
                 
#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__HSPFILTER_CULLING__H */
