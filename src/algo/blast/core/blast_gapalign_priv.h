#ifndef ALGO_BLAST_CORE___BLAST_GAPALIGN_PRI__H
#define ALGO_BLAST_CORE___BLAST_GAPALIGN_PRI__H

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

/** @file blast_gapalign_pri.h
 *  Private interface for blast_gapalign.c
 */


#ifdef __cplusplus
extern "C" {
#endif

Int4
ALIGN_EX(Uint1* A, Uint1* B, Int4 M, Int4 N, Int4* S, Int4* a_offset,
        Int4* b_offset, Int4** sapp, BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* scoringParams, Int4 query_offset,
        Boolean reversed, Boolean reverse_sequence);


/** Converts a traceback produced by the ALIGN or ALIGN_EX 
 * routine to a GapEditBlock, which is normally then further 
 * processed to a SeqAlignPtr.  Note: the old routine had two
 * unused parameters that are not present here.
 * @param S The traceback obtained from ALIGN [in]
 * @param M Length of alignment in query [in]
 * @param N Length of alignment in subject [in]
 * @param start1 Starting query offset [in]
 * @param start2 Starting subject offset [in]
 * @param edit_block The constructed edit block [out]
 */
Int2
BLAST_TracebackToGapEditBlock(Int4* S, Int4 M, Int4 N, Int4 start1,
                               Int4 start2, GapEditBlock** edit_block);




#ifdef __cplusplus
}
#endif


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/05/18 13:23:26  madden
 * Private declarations for blast_gapalign.c
 *
 *
 * ===========================================================================
 */

#endif /* !ALGO_BLAST_CORE__BLAST_GAPALIGN_PRI__H */
