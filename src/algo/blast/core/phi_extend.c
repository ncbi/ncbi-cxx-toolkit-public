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
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file phi_extend.c
 * Word finder functions for PHI-BLAST
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/phi_extend.h>

/** Saves a pattern hit in a BlastInitHitList.
 * @param offset_pair Pattern start and stop in subject [in]
 * @param init_hitlist Initial hit list structure to save the hit in. [in] [out]
 */
static Int2 
s_PHISaveInitialHit(BlastInitHitList* init_hitlist, BlastOffsetPair* offset_pair)
{
    /* BlastOffsetPair is a union of two structures representing a pair of 
       offsets. Use common function BLAST_SaveInitialHit, with correct order of
       offsets to be saved. */
    return 
        BLAST_SaveInitialHit(init_hitlist, offset_pair->phi_offsets.s_start, 
                             offset_pair->phi_offsets.s_end, NULL);
}

Int2 
PHIBlastWordFinder(BLAST_SequenceBlk* subject, 
                   BLAST_SequenceBlk* query, LookupTableWrap* lookup_wrap,
                   Int4** matrix, const BlastInitialWordParameters* word_params,
                   Blast_ExtendWord* ewp, BlastOffsetPair* offset_pairs,
                   Int4 max_hits, BlastInitHitList* init_hitlist, 
                   BlastUngappedStats* ungapped_stats)
{
   Int4 hits=0;
   Int4 totalhits=0;
   Int4 first_offset = 0;
   Int4 last_offset  = subject->length;

   while(first_offset < last_offset)
   {
       Int4 hit_index;
      /* scan the subject sequence for hits */

      hits = PHIBlastScanSubject(lookup_wrap, query, subject, &first_offset, 
                                 offset_pairs, max_hits);

      totalhits += hits;

      /* Save all database pattern hits. */
      for (hit_index = 0; hit_index < hits; ++hit_index) {
          s_PHISaveInitialHit(init_hitlist, &offset_pairs[hit_index]);
      } /* End loop over hits. */
   } /* end while */

   Blast_UngappedStatsUpdate(ungapped_stats, totalhits, 0, 0);
   return 0;
}

