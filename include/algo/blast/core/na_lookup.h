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
* ===========================================================================
*/
/*****************************************************************************

File name: na_lookup.h

Author: Ilya Dondoshansky

Contents: Functions responsible for the creation of a lookup table for BLASTn

Detailed Contents: 

******************************************************************************
 * $Revision$
 */

#ifndef NA_LOOKUP__H
#define NA_LOOKUP__H

#ifdef __cplusplus
extern "C" {
#endif

/* Macro to test the presence vector array value for a lookup table index */
#define NA_PV_TEST(pv_array, index, pv_array_bts) (pv_array[(index)>>pv_array_bts]&(((PV_ARRAY_TYPE) 1)<<((index)&PV_ARRAY_MASK)))

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  old BLASTn approach - looking up words at every byte (4 bases) of the 
 *  sequence. Lookup table is presumed to have a traditional BLASTn structure.
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 BlastNaScanSubject(const LookupTableWrapPtr lookup_wrap,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits, 
       Int4Ptr end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  arbitrary stride. Lookup table is presumed to have a traditional BLASTn 
 *  structure. 
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 BlastNaScanSubject_AG(const LookupTableWrapPtr lookup_wrap,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits, 
       Int4Ptr end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the
 * old MegaBLAST approach - looking up words at every byte (4 bases) of the 
 * sequence. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_ScanSubject(const LookupTableWrapPtr lookup,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,
       Int4Ptr end_offset);

/** Scan the compressed subject sequence, returning all word hits, looking up 
 * discontiguous words. Lookup table is presumed to have a traditional 
 * MegaBLAST structure containing discontiguous word positions.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_DiscWordScanSubject(const LookupTableWrapPtr lookup,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset, 
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,     
       Int4Ptr end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 * arbitrary stride. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_AG_ScanSubject(const LookupTableWrapPtr lookup,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,
       Int4Ptr end_offset); 

/** Deallocate memory for the lookup table */
LookupTableWrapPtr BlastLookupTableDestruct(LookupTableWrapPtr lookup);

/** Fill the lookup table for a given query sequence or partial sequence.
 * @param lookup Pointer to the lookup table structure [in] [out]
 * @param query The query sequence [in]
 * @param location What locations on the query sequence to index? [in]
 */
Int4 BlastNaLookupIndexQuery(LookupTablePtr lookup, BLAST_SequenceBlkPtr query,
                             ValNodePtr location);

#ifdef __cplusplus
}
#endif

#endif /* NA_LOOKUP__H */
