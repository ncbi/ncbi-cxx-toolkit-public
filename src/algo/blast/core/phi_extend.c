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

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/phi_extend.h>


Int2 PHIBlastWordFinder(BLAST_SequenceBlk* subject, 
        BLAST_SequenceBlk* query, LookupTableWrap* lookup_wrap,
        Int4** matrix, const BlastInitialWordParameters* word_params,
        Blast_ExtendWord* ewp, Uint4* query_offsets, Uint4* subject_offsets,
        Int4 max_hits, BlastInitHitList* init_hitlist, 
        BlastUngappedStats* ungapped_stats)
{
   PHILookupTable* lookup = (PHILookupTable*) lookup_wrap->lut;
   Int4 hits=0;
   Int4 totalhits=0;
   Int4 first_offset = 0;
   Int4 last_offset  = subject->length;
   Int4 i;

   while(first_offset < last_offset)
   {
      /* scan the subject sequence for hits */

      hits = PHIBlastScanSubject(lookup_wrap, query, subject, &first_offset, 
                query_offsets, subject_offsets,	max_hits);

      totalhits += hits;
      /* for each hit, */
      for (i = 0; i < hits; ++i) {
         /* do an extension */
         BlastSaveInitHsp(init_hitlist, 
            lookup->start_offsets[query_offsets[i]], subject_offsets[i], 
            lookup->start_offsets[query_offsets[i]], subject_offsets[i], 
            lookup->lengths[query_offsets[i]], 0);
      } /* end for */
   } /* end while */

   Blast_UngappedStatsUpdate(ungapped_stats, totalhits, totalhits, totalhits);
   return 0;
}
