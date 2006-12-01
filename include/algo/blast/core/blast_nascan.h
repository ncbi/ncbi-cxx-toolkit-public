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
 */

/** @file blast_nascan.h
 *  Routines for scanning nucleotide BLAST lookup tables.
 */

#ifndef ALGO_BLAST_CORE__BLAST_NTSCAN__H
#define ALGO_BLAST_CORE__BLAST_NTSCAN__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/lookup_wrap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  old BLASTn approach - looking up words at every byte (4 bases) of the 
 *  sequence. Lookup table is presumed to have a traditional BLASTn structure.
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 BlastNaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk* subject,
                        Int4 start_offset,
                        BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                        Int4 max_hits, 
                        Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  arbitrary stride. Lookup table is presumed to have a traditional BLASTn 
 *  structure. 
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
 * @return The number of hits found from the lookup table
*/
Int4 BlastNaScanSubject_AG(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk* subject,
                        Int4 start_offset,
                        BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                        Int4 max_hits, 
                        Int4* end_offset);

/** Similar to BlastNaScanSubject, except that a lookup table is
 * used that is more efficient for small queries
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 BlastSmallNaScanSubject(const LookupTableWrap* lookup_wrap,
                             const BLAST_SequenceBlk* subject,
                             Int4 start_offset,
                             BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                             Int4 max_hits, 
                             Int4* end_offset);

/** Similar to BlastNaScanSubject_AG, except that a lookup table is
 * used that is more efficient for small queries
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
 * @return The number of hits found from the lookup table
*/
Int4 BlastSmallNaScanSubject_AG(const LookupTableWrap* lookup_wrap,
                                const BLAST_SequenceBlk* subject,
                                Int4 start_offset,
                                BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, 
                                Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, looking up 
 * discontiguous words. Lookup table is presumed to have a traditional 
 * MegaBLAST structure containing discontiguous word positions.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset, 
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,     
       Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 * arbitrary stride. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,
       Int4* end_offset); 

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_NTSCAN__H */
