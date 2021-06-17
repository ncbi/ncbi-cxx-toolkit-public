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

/** @file blast_aascan.h
 *  Routines for creating protein BLAST lookup tables.
 *  Contains definitions and prototypes for the lookup 
 *  table scanning phase of blastp and RPS blast.
 */

#ifndef ALGO_BLAST_CORE__BLAST_AASCAN__H
#define ALGO_BLAST_CORE__BLAST_AASCAN__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/lookup_wrap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Generic prototype for nucleotide subject scanning routines */
typedef Int4 (*TAaScanSubjectFunction)(const LookupTableWrap* lookup_wrap,
                                  const BLAST_SequenceBlk* subject,
                                  BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                                  Int4 max_hits,
                                  Int4 * s_range);

/** Choose the most appropriate function to scan through
 * protein subject sequences
 * @param lookup_wrap Structure containing lookup table [in][out]
 */
NCBI_XBLAST_EXPORT
void BlastChooseProteinScanSubject(LookupTableWrap *lookup_wrap);

/**
 * Scans the RPS query sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap the lookup table [in]
 * @param sequence the subject sequence [in]
 * @param offset the offset in the subject at which to begin scanning [in/out]
 * @return The number of hits found.
 */
NCBI_XBLAST_EXPORT
Int4 BlastRPSScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *sequence,
                        Int4* offset);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_AASCAN__H */
