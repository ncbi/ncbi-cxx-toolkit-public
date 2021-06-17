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
 * Author: Ilya Dondoshansky
 *
 */

/** @file link_hsps.h
 * Functions to link HSPs using sum statistics
 */

#ifndef ALGO_BLAST_CORE__LINK_HSPS__H
#define ALGO_BLAST_CORE__LINK_HSPS__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_hits.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Link HSPs using sum statistics.
 * @param program_number BLAST program [in]
 * @param hsp_list List of HSPs [in]
 * @param query_info Query information block [in]
 * @param subject_length Subject sequence length [in]
 * @param sbp Scoring and statistical data [in]
 * @param link_hsp_params Parameters for linking of HSPs [in]
 * @param gapped_calculation Is this a gapped search? [in]
 */
NCBI_XBLAST_EXPORT
Int2 
BLAST_LinkHsps(EBlastProgramType program_number, BlastHSPList* hsp_list, 
   const BlastQueryInfo* query_info, Int4 subject_length, 
   const BlastScoreBlk* sbp, const BlastLinkHSPParameters* link_hsp_params,
   Boolean gapped_calculation);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__LINK_HSPS__H */

