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

File name: blast_seq.h

Author: Ilya Dondoshansky

Contents: Functions converting from SeqLocs to structures used in BLAST and 
          back.

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_SEQ__
#define __BLAST_SEQ__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

#include <objseq.h>
#include <blast_def.h>
#include <blast_options.h>

#define NUM_FRAMES 6

/** Convert a SeqLoc of type int or packed_int to a BlastMask structure.
 * @param mask_slp The SeqLoc to be converted [in]
 * @param index The ordinal number of the sequence to be assigned to the new 
 *              BlastMask
 * @return Pointer to the allocated BlastMask structure.
 */
BlastMask* BlastMaskFromSeqLoc(SeqLocPtr mask_slp, Int4 index);

/** Convert a BlastMask list to a list of SeqLocs, used for formatting 
 * BLAST results.
 */
SeqLocPtr BlastMaskToSeqLoc(Uint1 program_number, BlastMask* mask_loc, 
                            SeqLocPtr slp);

/** Duplicate masks in 6 frames for each nucleotide sequence, converting
 * all masks' coordinates from DNA to protein.
 * @param mask_loc_ptr Masks list to be modified [in] [out]
 * @param slp List of nucleotide query SeqLoc's [in]
 */
Int2 BlastMaskDNAToProtein(BlastMask** mask_loc_ptr, SeqLocPtr slp);

/** Convert all masks' protein coordinates to nucleotide.
 * @param mask_loc_ptr Masks list to be modified [in] [out]
 * @param slp List of nucleotide query SeqLoc's [in]
 */
Int2 BlastMaskProteinToDNA(BlastMask** mask_loc_ptr, SeqLocPtr slp);

/** Given a list of query SeqLoc's, create the sequence block and the query
 * info structure. This is the last time SeqLoc is needed before formatting.
 * @param query_slp List of query SeqLoc's [in]
 * @param query_options Query setup options, containing genetic code for
 *                      translation [in]
 * @param program_number Type of BLAST program [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [out]
 * @param query_blk Query block, containing (concatenated) sequence [out]
 */
Int2 BLAST_SetUpQuery(Uint1 program_number, SeqLocPtr query_slp, 
        const QuerySetUpOptions* query_options, 
        BlastQueryInfo** query_info, BLAST_SequenceBlk* *query_blk);

/** Set up the subject sequence block in case of two sequences BLAST.
 * @param program_number Type of BLAST program [in]
 * @param subject_slp SeqLoc for the subject sequence [in]
 * @param subject Subject sequence block [out]
 */
Int2 BLAST_SetUpSubject(Uint1 program_number, 
        SeqLocPtr subject_slp, BLAST_SequenceBlk** subject);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_SEQ__ */
