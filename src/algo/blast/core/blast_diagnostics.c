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

File name: blast_diagnostics.c

Author: Ilya Dondoshansky

Contents: Manipulating diagnostics data returned from BLAST

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_diagnostics.h>
#include <algo/blast/core/blast_def.h>

BlastDiagnostics* Blast_DiagnosticsFree(BlastDiagnostics* diagnostics)
{
   if (diagnostics) {
      sfree(diagnostics->ungapped_stat);
      sfree(diagnostics->gapped_stat);
      sfree(diagnostics->cutoffs);
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
