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

File name: blast_traceback.h

Author: Ilya Dondoshansky

Contents: Functions to do gapped alignment with traceback and/or convert 
          results to the SeqAlign form

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_TRACEBACK__
#define __BLAST_TRACEBACK__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_hits.h>

/** Given the preliminary alignment results, redo the gapped alignment,
 * compute traceback if it has not yet been done, and return results in the
 * form of a SeqAlign list.
 * @param results All results after the preliminary gapped alignment [in]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param rdfp BLAST database structure [in]
 * @param subject The subject sequence, in BLAST 2 Sequences case [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options The scoring related options [in]
 * @param hit_params Parameters for saving hits [in]
 * @param seqalign_ptr Resulting SeqAling [out]
 */
Int2 BLAST_ComputeTraceback(BlastResultsPtr results, 
        BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
        ReadDBFILEPtr rdfp, BLAST_SequenceBlkPtr subject, 
        BlastGapAlignStructPtr gap_align,
        BlastScoringOptionsPtr score_options,
        BlastHitSavingParametersPtr hit_params,  
        SeqAlignPtr PNTR seqalign_ptr);


#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_TRACEBACK__ */

