/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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

File name: link_hsps.h

Author: Ilya Dondoshansky

Contents: Functions to link HSPs using sum statistics

******************************************************************************
 * $Revision$
 * */
#ifndef __LINK_HSPS__
#define __LINK_HSPS__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_hits.h>

#define BLAST_SMALL_GAPS 0
#define BLAST_LARGE_GAPS 1

/* The helper arrays contains the info used frequently in the inner 
 * for loops. -cfj
 * One array of helpers will be allocated for each thread. See comments 
 * preceding link_hsps in link_hsps.c for more info.
 */
typedef struct link_help_struct{
  BlastHSPPtr ptr;
  Int4 q_off_trim;
  Int4 s_off_trim;
  Int4 sum[BLAST_NUMBER_OF_ORDERING_METHODS];
  Int4 maxsum1;
  Int4 next_larger;
} LinkHelpStruct;

/** Link HSPs using sum statistics.
 * @param program_number BLAST program [in]
 * @param hsp_list List of HSPs [in]
 * @param query_info Query information block [in]
 * @param subject Subject sequence block [in]
 * @param sbp Scoring and statistical data [in]
 * @param hit_params Hit saving parameters [in]
 * @param gapped_calculation Is this a gapped search? [in]
 */
Int2 
BlastLinkHsps(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, BlastHitSavingParametersPtr hit_params,
   Boolean gapped_calculation);

#ifdef __cplusplus
}
#endif
#endif /* !__LINK_HSPS__ */

