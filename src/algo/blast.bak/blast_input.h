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
* ===========================================================================*/

/*****************************************************************************

File name: blast_input.h

Author: Ilya Dondoshansky

Contents: Reading FASTA sequences for BLAST

$Revision$

******************************************************************************/

#ifndef __BLAST_INPUT__
#define __BLAST_INPUT__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

#include <ncbi.h>
#include <blast_def.h>

/** Read the query sequences from a file, return a SeqLoc list.
 * @param infp The input file [in]
 * @param query_is_na Are sequences nucleotide (or protein)? [in]
 * @param strand Which strands should SeqLocs contain (0 for protein, 
 *               1 for plus, 2 for minus, 3 for both)? [in]
 * @param from Starting offset in query location [in]
 * @param to Ending offset in query location (-1 for end of sequence) [in]
 * @param lcase_mask The lower case masking locations (no lower case masking 
 *                   if NULL [out]
 * @param query_slp List of query SeqLocs [out]
 * @param ctr_start Number from which to start counting local ids [in]
 * @param num_queries Number of sequences read [out]
 * @return Have all sequences been read?
 */
Boolean
BLAST_GetQuerySeqLoc(FILE *infp, Boolean query_is_na, Uint1 strand,
   Int4 from, Int4 to, BlastMask** lcase_mask, SeqLocPtr* query_slp, Int4 ctr_start,
   Int4* num_queries);

/** Given a file containing sequence(s) in fasta format,
 * read a sequence and fill out a BLAST_SequenceBlk structure.
 *
 * @param fasta_fp the file to read from, assumed to already be open
 * @param seq pointer to the sequence block to fill out
 * @return Zero if successful, one on any error.
 */
Int4 MakeBlastSequenceBlkFromFasta(FILE *fasta_fp, BLAST_SequenceBlk* seq);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_INPUT__ */
