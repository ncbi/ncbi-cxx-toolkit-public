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

#include <aa_ungapped.h>

static inline Int4 DiagCheckLevel(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset);

static inline Int4 DiagUpdateLevel(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset, Int4 subject_extension);

static inline Int4 DiagGetLastHit(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset);

static inline Int4 DiagSetLastHit(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset);

static void BlastAaSaveInitHsp(BlastInitHitListPtr ungapped_hsps, Int4 q_off, Int4 s_off, Int4 len, Int4 score);

/*
 * this is currently somewhat broken since the diagonal array is NULL
 * and the offset arrays are allocated for every subject sequence.  this
 * must be factored out!
 */
Int4 BlastAaWordFinder(BLAST_SequenceBlkPtr subject,
                       BLAST_SequenceBlkPtr query,
                       LookupTableWrapPtr lut_wrap,
                       Int4Ptr PNTR matrix,
                       BlastInitialWordParametersPtr word_params,
                       BLAST_ExtendWordPtr ewp,
		       Uint4Ptr query_offsets,
		       Uint4Ptr subject_offsets,
		       Int4 offset_array_size,
                       BlastInitHitListPtr init_hitlist)
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

Int4 BlastAaWordFinder_TwoHit(const BLAST_SequenceBlkPtr subject,
			      const BLAST_SequenceBlkPtr query,
			      const LookupTableWrapPtr lookup_wrap,
			      BLAST_DiagTablePtr diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * query_offsets,
			      Uint4 * subject_offsets,
			      Int4 array_size,
			      BlastInitHitListPtr ungapped_hsps)
{
LookupTablePtr lookup = lookup_wrap->lut;
Int4 i;
Int4 hits=0;
Int4 totalhits=0;
Int4 first_offset = 0;
Int4 last_offset  = subject->length - lookup->wordsize;
Int4 score;
Int4 hsp_q, hsp_s, hsp_len;
Int4 diff;
Int4 window;

if (diag == NULL)
  window = 0;
else
  window = diag->window;

while(first_offset <= last_offset)
    {
      hits = BlastAaScanSubject(lookup_wrap,
				subject,
				&first_offset,
				query_offsets,
				subject_offsets,
				array_size);

      totalhits += hits;

      /* for each hit, */
      i=0;
      while(i<hits)
      {
	Int4 last_hit;
        Int4 level_diff;

	/* if we have previously extended past this hit, */

	if ( (level_diff = DiagCheckLevel(diag, query_offsets[i],
                                          subject_offsets[i])) < 0 )
	  {
	    /* skip it. */
	    DiagSetLastHit(diag, query_offsets[i], subject_offsets[i]);
	    i++;

	    continue;
	  }

	/* if the last hit is within the window, */

	last_hit = DiagGetLastHit(diag, query_offsets[i], subject_offsets[i]);

	diff = subject_offsets[i] - last_hit;

	if ( (window == 0) || 
             ( (diff < window) && (level_diff >= lookup->wordsize )) )
	  {
	    hsp_len=0;
	    
	    /* initiate a two-hit extension. */
	    score=BlastAaExtendTwoHit(diag,
				      matrix,
				      subject,
				      query,
				      last_hit,
				      subject_offsets[i],
				      query_offsets[i],
				      dropoff,
				      &hsp_q,
				      &hsp_s,
				      &hsp_len);


	    /* if the hsp meets the score threshold, report it */
	    
	      if (score > cutoff)
                BlastAaSaveInitHsp(ungapped_hsps, hsp_q, hsp_s, hsp_len, score);
              /* end two-hit extension case */
	  } else if (diff >= window) {
             /* Update level, but only if it is a new hit */
             DiagUpdateLevel(diag, query_offsets[i], subject_offsets[i], 
                             subject_offsets[i]);
          }
	
	
	/* update the diagonal array to reflect this hit. */
        DiagSetLastHit(diag, query_offsets[i], subject_offsets[i]);
	i++;
	
      } /* end while - done with this batch of hits, call scansubject again. */
            
    } /* end while - done with the entire sequence. */
 
/* increment the offset in the diagonal array */
 
 DiagUpdate(diag, subject->length + diag->window); 
 
 return totalhits;
}

Int4 BlastAaWordFinder_OneHit(const BLAST_SequenceBlkPtr subject,
			      const BLAST_SequenceBlkPtr query,
			      const LookupTableWrapPtr lookup_wrap,
			      BLAST_DiagTablePtr diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * query_offsets,
			      Uint4 * subject_offsets,
			      Int4 array_size,
                              BlastInitHitListPtr ungapped_hsps)
{
LookupTablePtr lookup = lookup_wrap->lut;
Int4 hits=0;
Int4 totalhits=0;
Int4 first_offset = 0;
Int4 last_offset  = subject->length - lookup->wordsize;
Int4 hsp_q, hsp_s, hsp_len;
Int4 i;
Int4 score;

 /* clear the diagonal array */

 DiagClear(diag);

while(first_offset <= last_offset)
    {
      /* scan the subject sequence for hits */

      hits = BlastAaScanSubject(lookup_wrap,
				subject,
				&first_offset,
				query_offsets,
				subject_offsets,
				array_size);

      totalhits += hits;

      /* for each hit, */
      i=0;
      while(i<hits)
	{
	  /* do an extension */

	  score=BlastAaExtendOneHit(diag,
				    matrix,
				    subject,
				    query,
				    subject_offsets[i],
				    query_offsets[i],
				    dropoff,
				    &hsp_q,
				    &hsp_s,
				    &hsp_len);


	/* if the hsp meets the score threshold, report it */
	if (score > cutoff)
            BlastAaSaveInitHsp(ungapped_hsps,
                                hsp_q,
                                hsp_s,
                                hsp_len,
                                score);
	i++;
	}

    } /* end while */

 return totalhits;
}

/** Given a hit, determine the diagonal on which that hit lies, and then determine the previous hit on that diagonal.
 *
 * @param diag the diagonal table [in]
 * @param query_offset the query offset of the hit [in]
 * @param subject_offset the subject offset of the hit [in]
 * @return the subject offset of the previous hit
 */

static inline Int4 DiagGetLastHit(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset)
{
  Int4 diag_coord;

  if (diag == NULL)
    return 0;

  diag_coord = (subject_offset - query_offset) & diag->diag_mask;
  
    return diag->diag_array[diag_coord].last_hit - diag->offset;
}

/** Given a hit, determine the diagonal on which that hit lies, and then record the subject offset in the last_hit field of the diagonal array.
 *
 * @param diag the diagonal table [in]
 * @param query_offset the query offset of the hit [in]
 * @param subject_offset the subject offset of the hit [in]
 * @return Zero.
 */

static inline Int4 DiagSetLastHit(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset)
{
  Int4 diag_coord;
  
  if (diag == NULL)
    return 0;
  
  diag_coord = (subject_offset - query_offset) & diag->diag_mask;
  
  diag->diag_array[diag_coord].last_hit = subject_offset + diag->offset;
  return 0;
}

/** Given a hit, determine if we have previously extended past it.
 *
 * see the corresponding function DiagUpdateLevel() in extend.c
 *
 * @param diag the diagonal table [in]
 * @param query_offset the query offset of the hit [in]
 * @param subject_offset the subject offset of the hit [in]
 * @return 1 if we've previously extended past this hit, 0 otherwise.
 */


static inline Int4 DiagCheckLevel(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset)
{
  Int4 diag_coord;
  Int4 level;

  if (diag == NULL)
    return 0;

  diag_coord = (subject_offset  - query_offset) & diag->diag_mask;

  /*
   * if we've previously extended past the current subject offset,
   * we can safely skip this extension.
   */
  level = diag->diag_array[diag_coord].diag_level - diag->offset;
  if (subject_offset < level )
    return -1;
  else
    return subject_offset - level;
}

static void BlastAaSaveInitHsp(BlastInitHitListPtr ungapped_hsps, Int4 q_off, Int4 s_off, Int4 len, Int4 score)
{
  BlastUngappedDataPtr ungapped_data = NULL;

  ungapped_data = (BlastUngappedDataPtr) Malloc(sizeof(BlastUngappedData));

  ungapped_data->q_start = q_off;
  ungapped_data->s_start = s_off;
  ungapped_data->length  = len;
  ungapped_data->score   = score;
  ungapped_data->frame   = 0;

  BLAST_SaveInitialHit(ungapped_hsps, q_off, s_off, ungapped_data);

  return;
}

Int4 BlastAaExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlkPtr subject,
			const BLAST_SequenceBlkPtr query,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4Ptr displacement)
{
  Int4 i,n;
  Int4 score=0;
  Int4 maxscore=0;

  Uint1Ptr s, q;
  n = MIN( subject->length - s_off , query->length - q_off );

  s=subject->sequence + s_off;
  q=query->sequence + q_off;

  for(i=0;i<n;i++)
    {
      score += matrix[ s[i]] [ q[i] ];

      if (score > maxscore)
	maxscore = score;

      if (score < 0)
	break;

      if ((maxscore - score) > dropoff)
	break;
    }

  *displacement = i;
  return maxscore;
}

Int4 BlastAaExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlkPtr subject,
		       const BLAST_SequenceBlkPtr query,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4Ptr displacement)
{
  Int4 i,n;
  Int4 score=0;
  Int4 maxscore=0;

  Uint1Ptr s,q;


  n = MIN( s_off , q_off );

  s=subject->sequence + s_off - n;
  q=query->sequence + q_off - n;

  for(i=n;i>0;i--)
    {
      score += matrix[ s[i] ] [ q[i] ];

      if (score > maxscore)
	maxscore = score;

      if (score < 0)
	break;

      if ((maxscore - score) > dropoff)
	break;
    }

  *displacement = n-i;
  return maxscore;
}

Int4 BlastAaExtendOneHit(const BLAST_DiagTablePtr diag,
			 Int4 ** matrix,
                         const BLAST_SequenceBlkPtr subject,
                         const BLAST_SequenceBlkPtr query,
                         Int4 s_off,
                         Int4 q_off,
                         Int4 dropoff,
			 Int4Ptr hsp_q,
			 Int4Ptr hsp_s,
			 Int4Ptr hsp_len)
{
  Int4 left_score, right_score;
  Int4 left_disp, right_disp;

/* first, extend to the left. */
left_score = BlastAaExtendLeft(matrix, subject,query,s_off,q_off,dropoff,&left_disp);

/* then, extend to the right. */
right_score = BlastAaExtendRight(matrix, subject, query, s_off, q_off, dropoff,&right_disp);

*hsp_q   = q_off - left_disp;
*hsp_s   = s_off - left_disp;
*hsp_len = left_disp + right_disp;

 DiagUpdateLevel(diag, q_off, s_off, *hsp_s + *hsp_len);

return left_score + right_score;
}

Int4 BlastAaExtendTwoHit(const BLAST_DiagTablePtr diag,
			 Int4 ** matrix,
                         const BLAST_SequenceBlkPtr subject,
                         const BLAST_SequenceBlkPtr query,
                         Int4 s_left_off,
                         Int4 s_right_off,
                         Int4 q_right_off,
                         Int4 dropoff,
			 Int4Ptr hsp_q,
			 Int4Ptr hsp_s,
			 Int4Ptr hsp_len)
{
Int4 left_d, right_d; /* left and right displacements */
Int4 left_score, right_score; /* left and right scores */

/* first, try to extend left, from the second hit to the first hit. */

left_score=BlastAaExtendLeft(matrix,
			     subject,
			     query,
			     s_right_off,
			     q_right_off,
			     dropoff,
			     &left_d);
 
/* if we didn't make it back to the first hit, don't bother extending to the right. */
 if (left_d < (s_right_off - s_left_off))
   {
     return 0;
   }

 /* otherwise, try to extend to the right. */

right_score=BlastAaExtendRight(matrix,
			       subject,
			       query,
			       s_right_off,
			       q_right_off,
			       dropoff,
			       &right_d);

 *hsp_q = q_right_off - left_d;
 *hsp_s = s_right_off - left_d;
 *hsp_len = left_d + right_d;

 DiagUpdateLevel(diag, q_right_off, s_right_off, *hsp_s + *hsp_len);

return left_score + right_score;
}

/** Save the rightmost subject offset for this diagonal, so that
 * subsequent extensions that may start on this diagonal do not
 * do the same work over again.
 *
 * @param diag diagonal array [in/out]
 * @param query_offset query offset, used to compute the diagonal [in]
 * @param subject_offset subject offset, used to compute the diagonal [in]
 * @param subject_extension the rightmost subject offset
 * @return Zero.
 */

static inline Int4 DiagUpdateLevel(BLAST_DiagTablePtr diag, Int4 query_offset, Int4 subject_offset, Int4 subject_extension)
{
  Int4 diag_coord;

  if (diag==NULL)
    return 0;

  diag_coord = (subject_offset - query_offset) & diag->diag_mask;
  
  diag->diag_array[diag_coord].diag_level = subject_extension + diag->offset;

  return 0;
}

/* stores diagonal information (for multiple hit heuristic) as well
 * as extension information (for ungapped extension)
 */

/* for query and subject lengths qlen and slen respectively,
 * there are qlen + slen - 1 diagonals. let maxlen = max(qlen,slen) .
 * then let ndiags = 2 * 2^(ceil(log2(maxlen))),
 * or, twice the next larger power of two of the max of the subject and query lengths.
 * in order to ensure that all diagonals
 * are positive (they are used as array indices, after all), we bias the
 * subject offset by ndiags / 2.
 * thus, the following relation holds when converting from offset
 * coordinates to diagonal coordinates:
 *
 * diagonal = ( s_off + (ndiags / 2) ) - q_off
 * displacement = min(q_off, s_off)
 *
 * 
 * the last_hit element stores the subject offset of the last hit.
 * the diag_level element stores how far to the right the last hit
 * was extended. this is used to prevent subsequent hits (and extensions)
 * from extending over previous extensions.
 *
 */

Int4 DiagNew(BLAST_DiagTablePtr * diag, Int4 window_size, Int4 longest_seq)
{
  *diag = Malloc(sizeof(BLAST_DiagTable));

  (*diag)->diag_array_length = 2 * longest_seq;
  (*diag)->diag_array = Malloc( (*diag)->diag_array_length * sizeof(DiagStruct));
  (*diag)->window = window_size;
  DiagClear(*diag);

return 0;
}

Int4 DiagUpdate(BLAST_DiagTablePtr diag, Int4 length)
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


Int4 DiagClear(BLAST_DiagTablePtr diag)
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

Int4 DiagFree(BLAST_DiagTablePtr diag)
{
  if (diag == NULL)
    return 0;

free(diag->diag_array);
free(diag);
return 0;
}
