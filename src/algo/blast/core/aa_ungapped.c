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

static char const rcsid[] = "$Id$";

#include <algo/blast/core/aa_ungapped.h>

Int4 BlastAaWordFinder(BLAST_SequenceBlk* subject,
                       BLAST_SequenceBlk* query,
                       LookupTableWrap* lut_wrap,
                       Int4** matrix,
                       const BlastInitialWordParameters* word_params,
                       Blast_ExtendWord* ewp,
		       Uint4* query_offsets,
		       Uint4* subject_offsets,
		       Int4 offset_array_size,
                       BlastInitHitList* init_hitlist)
{
  Int4 hits=0;

  /* find the word hits and do ungapped extensions */

  if (ewp->diag_table->multiple_hits)
    {
      hits = BlastAaWordFinder_TwoHit(subject,
				      query,
				      lut_wrap,
				      ewp->diag_table,
				      matrix,
				      word_params->cutoff_score,
				      word_params->x_dropoff,
				      query_offsets,
				      subject_offsets,
				      offset_array_size,
				      init_hitlist);
    }
  else
    {
      hits = BlastAaWordFinder_OneHit(subject,
				      query,
				      lut_wrap,
				      ewp->diag_table,
				      matrix,
				      word_params->cutoff_score,
				      word_params->x_dropoff,
				      query_offsets,
				      subject_offsets,
				      offset_array_size,
				      init_hitlist);
    }

  return hits;
}

Int4 BlastAaWordFinder_TwoHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * NCBI_RESTRICT query_offsets,
			      Uint4 * NCBI_RESTRICT subject_offsets,
			      Int4 array_size,
			      BlastInitHitList* ungapped_hsps)
{
   LookupTable* lookup=NULL;
   RPSLookupTable* rps_lookup=NULL;
   Boolean use_pssm;
   Int4 wordsize;
   Int4 i;
   Int4 hits=0;
   Int4 totalhits=0;
   Int4 first_offset = 0;
   Int4 last_offset;
   Int4 score;
   Int4 hsp_q, hsp_s, hsp_len;
   Int4 window;
   Int4 last_hit, s_last_off, diff;
   Int4 diag_offset, diag_coord, diag_mask;
   DiagStruct* diag_array;
   Boolean right_extend;

   if (diag == NULL)
      return -1;

   diag_offset = diag->offset;
   diag_array = diag->diag_array;
   diag_mask = diag->diag_mask;
   window = diag->window;

   if (lookup_wrap->lut_type == RPS_LOOKUP_TABLE) {
      rps_lookup = (RPSLookupTable *)lookup_wrap->lut;
      wordsize = rps_lookup->wordsize;
   }
   else {
      lookup = (LookupTable *)lookup_wrap->lut;
      wordsize = lookup->wordsize;
   }
   last_offset  = subject->length - wordsize;
   use_pssm = (rps_lookup != NULL) || (lookup->use_pssm);

   while(first_offset <= last_offset) {
      /* scan the subject sequence for hits */
      if (rps_lookup)
         hits = BlastRPSScanSubject(lookup_wrap, subject, &first_offset, 
                                query_offsets, subject_offsets, array_size);
      else
         hits = BlastAaScanSubject(lookup_wrap, subject, &first_offset, 
                                query_offsets, subject_offsets, array_size);

      totalhits += hits;
      /* for each hit, */
      for (i = 0; i < hits; ++i)
      {
         /* calculate the diagonal associated with this query-subject
            pair, and find the distance to the last hit on this diagonal */

         diag_coord = 
            (subject_offsets[i]  - query_offsets[i]) & diag_mask;
         
         last_hit = diag_array[diag_coord].last_hit - diag_offset;
         diff = subject_offsets[i] - last_hit;

         if (diff >= window) {
            /* We are beyond the window for this diagonal; start a new hit */
            diag_array[diag_coord].last_hit = subject_offsets[i] + diag_offset;
         } 
         else {
            /* If the difference is negative, or is less than the 
               wordsize (i.e. last hit and this hit overlap), give up */

            if (diff < wordsize)
               continue;

            /* If a previous extension has not already 
               covered this portion of the query and subject 
               sequences, do the ungapped extension */

            right_extend = TRUE;
            if ((Int4)(subject_offsets[i] + diag_offset) > 
                diag_array[diag_coord].diag_level) {

               /* Extend this pair of hits. The extension to the left must 
                  reach the end of the first word in order for extension 
                  to the right to proceed. */
               hsp_len = 0;
               score = BlastAaExtendTwoHit(matrix, subject, query,
                                        last_hit + wordsize, 
                                        subject_offsets[i], query_offsets[i], 
                                        dropoff, &hsp_q, &hsp_s, 
                                        &hsp_len, use_pssm,
                                        wordsize, &right_extend, &s_last_off);

               /* if the hsp meets the score threshold, report it */
               if (score >= cutoff)
                  BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s,
                        query_offsets[i], subject_offsets[i], hsp_len, score);

               /* whether or not the score was high enough, remember
                  the rightmost subject word offset examined. Future hits must
                  lie at least one letter to the right of this point */
               diag_array[diag_coord].diag_level = 
                        s_last_off - (wordsize - 1) + diag_offset;
            }

            /* If an extension to the right happened (or no extension
               at all), reset the last hit so that future hits to this 
               diagonal must start over. Otherwise make the present hit
               into the previous hit for this diagonal */

            if (right_extend)
               diag_array[diag_coord].last_hit = 0;
            else
               diag_array[diag_coord].last_hit = 
                        subject_offsets[i] + diag_offset;
         } 
      }/* end for - done with this batch of hits, call scansubject again. */
   } /* end while - done with the entire sequence. */
 
   /* increment the offset in the diagonal array */
   DiagUpdate(diag, subject->length + window); 
 
   return totalhits;
}

Int4 BlastAaWordFinder_OneHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * NCBI_RESTRICT query_offsets,
			      Uint4 * NCBI_RESTRICT subject_offsets,
			      Int4 array_size,
                              BlastInitHitList* ungapped_hsps)
{
   LookupTable* lookup=NULL;
   RPSLookupTable* rps_lookup=NULL;
   Boolean use_pssm;
   Int4 wordsize;
   Int4 hits=0;
   Int4 totalhits=0;
   Int4 first_offset = 0;
   Int4 last_offset;
   Int4 hsp_q, hsp_s, hsp_len;
   Int4 s_last_off;
   Int4 i;
   Int4 score;
   Int4 diag_offset, diag_coord, diag_mask, diff;
   DiagStruct* diag_array;

   if (!diag) 
      return -1;
   
   diag_offset = diag->offset;
   diag_array = diag->diag_array;
   diag_mask = diag->diag_mask;
   
   if (lookup_wrap->lut_type == RPS_LOOKUP_TABLE) {
      rps_lookup = (RPSLookupTable *)lookup_wrap->lut;
      wordsize = rps_lookup->wordsize;
   }
   else {
      lookup = (LookupTable *)lookup_wrap->lut;
      wordsize = lookup->wordsize;
   }
   last_offset  = subject->length - wordsize;
   use_pssm = (rps_lookup != NULL) || (lookup->use_pssm);

   while(first_offset <= last_offset)
   {
      /* scan the subject sequence for hits */
      if (rps_lookup)
         hits = BlastRPSScanSubject(lookup_wrap, subject, &first_offset, 
                                query_offsets, subject_offsets, array_size);
      else
         hits = BlastAaScanSubject(lookup_wrap, subject, &first_offset,
				query_offsets, subject_offsets, array_size);

      totalhits += hits;
      /* for each hit, */
      for (i = 0; i < hits; ++i) {
         diag_coord = 
            (subject_offsets[i]  - query_offsets[i]) & diag_mask;
           
         diff = subject_offsets[i] - 
            (diag_array[diag_coord].diag_level - diag_offset);

         /* do an extension, but only if we have not already extended
            this far */
         if (diff > 0) {
            score=BlastAaExtendOneHit(matrix, subject, query,
                     subject_offsets[i], query_offsets[i], dropoff,
                     &hsp_q, &hsp_s, &hsp_len, use_pssm, &s_last_off);

            /* if the hsp meets the score threshold, report it */
            if (score >= cutoff) {
               BlastSaveInitHsp(ungapped_hsps, hsp_q, hsp_s, 
                  query_offsets[i], subject_offsets[i], hsp_len, score);
            }
            diag_array[diag_coord].diag_level = 
                  s_last_off - (lookup->wordsize - 1) + diag_offset;
         }
      } /* end for */
   } /* end while */

   /* increment the offset in the diagonal array (no windows used) */
   DiagUpdate(diag, subject->length); 
 
   return totalhits;
}

Int4 BlastAaExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlk* subject,
			const BLAST_SequenceBlk* query,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4* displacement,
                        Int4 maxscore,
                        Int4* s_last_off)
{
  Int4 i, n, best_i = -1;
  Int4 score = maxscore;

  Uint1* s,* q;
  n = MIN( subject->length - s_off , query->length - q_off );

  s=subject->sequence + s_off;
  q=query->sequence + q_off;

  for(i = 0; i < n; i++) {
     score += matrix[ q[i] ] [ s[i] ];

     if (score > maxscore) {
        maxscore = score;
        best_i = i;
     }

     if (score <= 0 || (maxscore - score) >= dropoff)
        break;
  }

  *displacement = best_i;
  *s_last_off = s_off + i;
  return maxscore;
}

Int4 BlastAaExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlk* subject,
		       const BLAST_SequenceBlk* query,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4* displacement)
{
   Int4 i, n, best_i;
   Int4 score = 0;
   Int4 maxscore = 0;
   
   Uint1* s,* q;
   
   n = MIN( s_off , q_off );
   best_i = n + 1;

   s = subject->sequence + s_off - n;
   q = query->sequence + q_off - n;
   
   for(i = n; i >= 0; i--) {
      score += matrix[ q[i] ] [ s[i] ];
      
      if (score > maxscore) {
         maxscore = score;
         best_i = i;
      }
      if ((maxscore - score) >= dropoff)
         break;
   }
   
   *displacement = n - best_i;
   return maxscore;
}

Int4 BlastPSSMExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlk* subject,
			Int4 query_size,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4* displacement,
                        Int4 maxscore,
			Int4* s_last_off)
{
  Int4 i, n, best_i = -1;
  Int4 score = maxscore;
  Uint1* s;

  n = MIN( subject->length - s_off , query_size - q_off );
  s = subject->sequence + s_off;

  for(i = 0; i < n; i++) {
     score += matrix[ q_off+i ] [ s[i] ];

     if (score > maxscore) {
        maxscore = score;
        best_i = i;
     }

     if (score <= 0 || (maxscore - score) >= dropoff)
        break;
  }

  *displacement = best_i;
  *s_last_off = s_off + i;
  return maxscore;
}

Int4 BlastPSSMExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlk* subject,
		       Int4 query_size,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4* displacement)
{
   Int4 i, n, best_i;
   Int4 score = 0;
   Int4 maxscore = 0;
   Uint1* s;
   
   n = MIN( s_off , q_off );
   best_i = n + 1;
   s = subject->sequence + s_off - n;
   
   for(i = n; i >= 0; i--) {
      score += matrix[ q_off-n+i ] [ s[i] ];

      if (score > maxscore) {
         maxscore = score;
         best_i = i;
      }
      if ((maxscore - score) >= dropoff)
         break;
   }
   
   *displacement = n - best_i;
   return maxscore;
}

Int4 BlastAaExtendOneHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_off,
                         Int4 q_off,
                         Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len,
                         Boolean use_pssm,
                         Int4* s_last_off)
{
  Int4 left_score, right_score;
  Int4 left_disp, right_disp;

  if (use_pssm) {
     left_score = BlastPSSMExtendLeft(matrix, subject, query->length, 
                           s_off, q_off, dropoff, &left_disp);
     right_score = BlastPSSMExtendRight(matrix, subject, query->length, 
                           s_off+1, q_off+1, dropoff, &right_disp, 
                           left_score, s_last_off);
  }
  else {
     left_score = BlastAaExtendLeft(matrix, subject, query, 
                            s_off, q_off, dropoff, &left_disp);
     right_score = BlastAaExtendRight(matrix, subject, query, 
                            s_off+1, q_off+1, dropoff, &right_disp, 
                            left_score, s_last_off);
  }
  
  *hsp_q   = q_off - left_disp;
  *hsp_s   = s_off - left_disp;
  *hsp_len = left_disp + right_disp + 2;
  
  return right_score;
}

Int4 BlastAaExtendTwoHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_left_off,
                         Int4 s_right_off,
                         Int4 q_right_off,
                         Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len,
                         Boolean use_pssm,
                         Int4 word_size,
                         Boolean *right_extend,
                         Int4 *s_last_off)
{
   Int4 left_d = 0, right_d = 0; /* left and right displacements */
   Int4 left_score = 0, right_score = 0; /* left and right scores */
   Int4 i, score = 0;
   Uint1 *s = subject->sequence;
   Uint1 *q = query->sequence;

   /* find the position (up to word_size-1 letters to the
      right) that gives the largest starting score */
   for (i = 0; i < word_size; i++) {
      if (use_pssm)
         score += matrix[ q_right_off+i ][ s[s_right_off+i] ];
      else
         score += matrix[ q[q_right_off+i] ][ s[s_right_off+i] ];

      if (score > left_score) {
         left_score = score;
         right_d = i;
      }
   }
   q_right_off += right_d;
   s_right_off += right_d;

   right_d = -1;
   *right_extend = FALSE;
   *s_last_off = s_right_off;

   /* first, try to extend left, from the second hit to the first hit. */
   if (use_pssm)
      left_score = BlastPSSMExtendLeft(matrix, subject, query->length,
    			     s_right_off, q_right_off, dropoff, &left_d);
   else
      left_score = BlastAaExtendLeft(matrix, subject, query,
    			     s_right_off, q_right_off, dropoff, &left_d);

   /* Extend to the right only if left extension reached the first hit. */
   if (left_d >= (s_right_off - s_left_off)) {

      *right_extend = TRUE;
      if (use_pssm)
         right_score = BlastPSSMExtendRight(matrix, subject, query->length,
                                     s_right_off + 1, q_right_off + 1,
                                     dropoff, &right_d, left_score, s_last_off);
      else 
         right_score = BlastAaExtendRight(matrix, subject, query,
                                     s_right_off + 1, q_right_off + 1,
                                     dropoff, &right_d, left_score, s_last_off);
   }

   *hsp_q = q_right_off - left_d;
   *hsp_s = s_right_off - left_d;
   *hsp_len = left_d + right_d + 2;
   return MAX(left_score,right_score);
}

Int4 DiagUpdate(BLAST_DiagTable* diag, Int4 length)
{
  if (diag == NULL)
    return 0;

  if (diag->offset >= INT4_MAX/2)
    {
      DiagClear(diag);
    }
  else
    {
      diag->offset += length;
    }
  return 0;
}


Int4 DiagClear(BLAST_DiagTable* diag)
{
  Int4 i,n;

  if (diag==NULL)
    return 0;

  n=diag->diag_array_length;

  for(i=0;i<n;i++)
    {
      diag->diag_array[i].diag_level = 0;
      diag->diag_array[i].last_hit = - diag->window;
    }
  return 0;
}

