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

/** @file blast_diagnostics.c
 * Manipulating diagnostics data returned from BLAST
 */


#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_diagnostics.h>
#include <algo/blast/core/blast_def.h>

BlastDiagnostics* Blast_DiagnosticsFree(BlastDiagnostics* diagnostics)
{
   if (diagnostics) {
      sfree(diagnostics->ungapped_stat);
      sfree(diagnostics->gapped_stat);
      sfree(diagnostics->cutoffs);
      if (diagnostics->mt_lock)
         diagnostics->mt_lock = MT_LOCK_Delete(diagnostics->mt_lock);
      sfree(diagnostics);
   }
   return NULL;
}

BlastDiagnostics* Blast_DiagnosticsInit() 
{
   BlastDiagnostics* diagnostics = 
      (BlastDiagnostics*) calloc(1, sizeof(BlastDiagnostics));

   diagnostics->ungapped_stat = 
      (BlastUngappedStats*) calloc(1, sizeof(BlastUngappedStats));
   diagnostics->gapped_stat = 
      (BlastGappedStats*) calloc(1, sizeof(BlastGappedStats));
   diagnostics->cutoffs = 
      (BlastRawCutoffs*) calloc(1, sizeof(BlastRawCutoffs));

   return diagnostics;
}

BlastDiagnostics* Blast_DiagnosticsInitMT(MT_LOCK mt_lock)
{
   BlastDiagnostics* retval = Blast_DiagnosticsInit();
   retval->mt_lock = mt_lock;

   return retval;
}

void Blast_UngappedStatsUpdate(BlastUngappedStats* ungapped_stats, 
                               Int4 total_hits, Int4 extended_hits,
                               Int4 saved_hits)
{
   if (!ungapped_stats || total_hits == 0)
      return;

   ungapped_stats->lookup_hits += total_hits;
   ++ungapped_stats->num_seqs_lookup_hits;
   ungapped_stats->init_extends += extended_hits;
   ungapped_stats->good_init_extends += saved_hits;
   if (saved_hits > 0)
      ++ungapped_stats->num_seqs_passed;
}

void 
Blast_DiagnosticsUpdate(BlastDiagnostics* global, BlastDiagnostics* local)
{
    if (!local)
        return;

   if (global->mt_lock) 
      MT_LOCK_Do(global->mt_lock, eMT_Lock);

   if (global->ungapped_stat && local->ungapped_stat) {
      global->ungapped_stat->lookup_hits += 
         local->ungapped_stat->lookup_hits;
      global->ungapped_stat->num_seqs_lookup_hits += 
         local->ungapped_stat->num_seqs_lookup_hits;
      global->ungapped_stat->init_extends += 
         local->ungapped_stat->init_extends;
      global->ungapped_stat->good_init_extends += 
         local->ungapped_stat->good_init_extends;
      global->ungapped_stat->num_seqs_passed += 
         local->ungapped_stat->num_seqs_passed;
   }

   if (global->gapped_stat && local->gapped_stat) {
      global->gapped_stat->seqs_ungapped_passed += 
         local->gapped_stat->seqs_ungapped_passed;
      global->gapped_stat->extra_extensions += 
         local->gapped_stat->extra_extensions;
      global->gapped_stat->extensions += 
         local->gapped_stat->extensions;
      global->gapped_stat->good_extensions += 
         local->gapped_stat->good_extensions;
      global->gapped_stat->num_seqs_passed += 
         local->gapped_stat->num_seqs_passed;
   }

   if (global->cutoffs && local->cutoffs) {
      global->cutoffs->x_drop_ungapped = local->cutoffs->x_drop_ungapped;
      global->cutoffs->x_drop_gap = local->cutoffs->x_drop_gap;
      global->cutoffs->x_drop_gap_final = local->cutoffs->x_drop_gap_final;
      global->cutoffs->ungapped_cutoff = local->cutoffs->ungapped_cutoff;
      global->cutoffs->cutoff_score = local->cutoffs->cutoff_score;
   }

   if (global->mt_lock) 
      MT_LOCK_Do(global->mt_lock, eMT_Unlock);
}
