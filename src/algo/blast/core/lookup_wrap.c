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

/** @file lookup_wrap.c
 * @todo FIXME file had copy-and-paste description!
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/blast_rps.h>

Int2 LookupTableWrapInit(BLAST_SequenceBlk* query, 
        const LookupTableOptions* lookup_options,	
        BlastSeqLoc* lookup_segments, BlastScoreBlk* sbp, 
        LookupTableWrap** lookup_wrap_ptr, BlastRPSInfo *rps_info)
{
   LookupTableWrap* lookup_wrap;
   Boolean is_na;

   /* Construct the lookup table. */
   *lookup_wrap_ptr = lookup_wrap = 
      (LookupTableWrap*) calloc(1, sizeof(LookupTableWrap));
   lookup_wrap->lut_type = lookup_options->lut_type;

   switch ( lookup_options->lut_type ) {
   case AA_LOOKUP_TABLE:
       {
       Int4** matrix = NULL;
       if (lookup_options->use_pssm) {
           matrix = sbp->psi_matrix->pssm->data;
       } else {
           matrix = sbp->matrix->data;
       }
       BlastAaLookupNew(lookup_options, (BlastLookupTable* *)
                        &lookup_wrap->lut);
       BlastAaLookupIndexQuery( (BlastLookupTable*) lookup_wrap->lut, matrix, 
                                 query, lookup_segments);
       _BlastAaLookupFinalize((BlastLookupTable*) lookup_wrap->lut);
       }
      break;
   case MB_LOOKUP_TABLE:
      MB_LookupTableNew(query, lookup_segments, 
         (BlastMBLookupTable* *) &(lookup_wrap->lut), lookup_options);
      break;
   case NA_LOOKUP_TABLE:
      LookupTableNew(lookup_options, 
         (BlastLookupTable* *) &(lookup_wrap->lut), FALSE);
	    
      BlastNaLookupIndexQuery((BlastLookupTable*) lookup_wrap->lut, query,
                              lookup_segments);
      _BlastAaLookupFinalize((BlastLookupTable*) lookup_wrap->lut);
      break;
   case PHI_AA_LOOKUP: case PHI_NA_LOOKUP:
      is_na = (lookup_options->lut_type == PHI_NA_LOOKUP);
      PHILookupTableNew(lookup_options, 
                   (BlastPHILookupTable* *) &(lookup_wrap->lut), is_na, sbp);
      /* Initialize the "pattern space" by number of pattern occurrencies 
         in query, effectively setting number of patterns in database to 1
         at this time. */
      sbp->effective_search_sp = 
         PHIBlastIndexQuery((BlastPHILookupTable*) lookup_wrap->lut, query,
                            lookup_segments, is_na);
      break;
   case RPS_LOOKUP_TABLE:
      RPSLookupTableNew(rps_info, (BlastRPSLookupTable* *)(&lookup_wrap->lut));
      break;
      
   default:
      {
         /* FIXME - emit error condition here */
      }
   } /* end switch */

   return 0;
}

LookupTableWrap* LookupTableWrapFree(LookupTableWrap* lookup)
{
   if (!lookup)
       return NULL;

   if (lookup->lut_type == MB_LOOKUP_TABLE) {
      lookup->lut = (void*) 
         MBLookupTableDestruct((BlastMBLookupTable*)lookup->lut);
   } else if (lookup->lut_type == PHI_AA_LOOKUP || 
              lookup->lut_type == PHI_NA_LOOKUP) {
      lookup->lut = (void*)
         PHILookupTableDestruct((BlastPHILookupTable*)lookup->lut);
   } else if (lookup->lut_type == RPS_LOOKUP_TABLE) {
      lookup->lut = (void*) 
         RPSLookupTableDestruct((BlastRPSLookupTable*)lookup->lut);
   } else {
      lookup->lut = (void*) 
         LookupTableDestruct((BlastLookupTable*)lookup->lut);
   }
   sfree(lookup);
   return NULL;
}

Int4 GetOffsetArraySize(LookupTableWrap* lookup)
{
   Int4 offset_array_size;

   switch (lookup->lut_type) {
   case MB_LOOKUP_TABLE:
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastMBLookupTable*)lookup->lut)->longest_chain;
      break;
   case PHI_AA_LOOKUP: case PHI_NA_LOOKUP:
      offset_array_size = MIN_PHI_LOOKUP_SIZE;
      break;
   case AA_LOOKUP_TABLE: case NA_LOOKUP_TABLE:
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastLookupTable*)lookup->lut)->longest_chain;
      break;
   case RPS_LOOKUP_TABLE:
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastRPSLookupTable*)lookup->lut)->longest_chain;
      break;
   default:
      offset_array_size = OFFSET_ARRAY_SIZE;
      break;
   }
   return offset_array_size;
}
