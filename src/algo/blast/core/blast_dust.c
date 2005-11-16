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
 * ==========================================================================
 *
 * Authors: Richa Agarwala (based upon versions variously worked upon by Roma 
 *          Tatusov, John Kuzio, and Ilya Dondoshansky).
 *   
 * ==========================================================================
 */

/** @file blast_dust.c
 * A utility to find low complexity NA regions. This parallels functionality 
 * of dust.c from the C toolkit, but without using the structures generated 
 * from ASN.1 spec.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_dust.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_filter.h>


/** Declared in blast_def.h as extern const. */
const int kDustLevel = 20;
const int kDustWindow = 64;
const int kDustLinker = 1;


/* local, file scope, structures and variables */

/** endpoints
 * @todo expand documentation
 */
typedef struct DREGION { 
	struct	DREGION*	next;
	Int4	from, to;
} DREGION;

/** localcurrents 
 * @todo expand documentation
 */
typedef struct DCURLOC { 
	Int4	cursum, curstart, curend;
	Int2	curlength;
} DCURLOC;

Uint1 *SUM_THRESHOLD;

/* local functions */

static void wo (Int4, Uint1*, Int4, DCURLOC*, Uint1*, Uint1, Int4);
static Boolean wo1 (Int4, Uint1*, Int4, DCURLOC*);
static Int4 dust_triplet_find (Uint1*, Int4, Int4, Uint1*);

/* entry point for dusting */

static Int4 dust_segs (Uint1* sequence, Int4 length, Int4 start,
		       DREGION* reg,
		       Int4 level, Int4 windowsize, Int4 linker)
{
   Int4    len;
   Int4	i;
   Uint1* seq;
   DREGION* regold = NULL;
   DCURLOC	cloc;
   Int4	nreg;
   
   /* defaults are more-or-less in keeping with original dust */
   if (level < 2 || level > 64) level = kDustLevel;
   if (windowsize < 8 || windowsize > 64) windowsize = kDustWindow;
   if (linker < 1 || linker > 32) linker = kDustLinker;
   
   nreg = 0;
   seq = (Uint1*) calloc(1, windowsize);			/* triplets */
   if (!seq) {
      return -1;
   }
   SUM_THRESHOLD = (Uint1 *) calloc(windowsize, sizeof(Uint1));  
   if (!SUM_THRESHOLD) {
      return -1;
   }
   SUM_THRESHOLD[0] = 1;
   for (i=1; i < windowsize; i++)
       SUM_THRESHOLD[i] = (level * i)/10;

   if (length < windowsize) windowsize = length;

   /* Consider smaller windows in beginning of the sequence */
   for (i = 2; i <= windowsize-1; i++) {
      len = i-1;
      wo (len, sequence, 0, &cloc, seq, 1, level);
      
      if (cloc.cursum*10 > level*cloc.curlength) {
         if (nreg &&
             regold->to + linker >= cloc.curstart+start &&
             regold->from <= cloc.curend + start + linker) {
            /* overlap windows nicely if needed */
            if (regold->to < cloc.curend +  start)
                regold->to = cloc.curend +  start;
            if (regold->from > cloc.curstart + start)
                regold->from = cloc.curstart + start;
         } else	{
            /* new window or dusted regions do not overlap */
            reg->from = cloc.curstart + start;
            reg->to = cloc.curend + start;
            regold = reg;
            reg = (DREGION*) calloc(1, sizeof(DREGION));
            if (!reg) {
               sfree(seq);
               sfree(SUM_THRESHOLD);
               return -1;
            }
            reg->next = NULL;
            regold->next = reg;
            nreg++;
         }
      }				/* end 'if' high score	*/
   }					/* end for */

   for (i = 1; i < length-2; i++) {
      len = (Int4) ((length > i+windowsize) ? windowsize : length-i);
      len -= 2;
      if (length >= i+windowsize)
          wo (len, sequence, i, &cloc, seq, 2, level);
      else /* remaining portion of sequence is less than windowsize */
          wo (len, sequence, i, &cloc, seq, 3, level);
      
      if (cloc.cursum*10 > level*cloc.curlength) {
         if (nreg &&
             regold->to + linker >= cloc.curstart+i+start &&
             regold->from <= cloc.curend + i + start + linker) {
            /* overlap windows nicely if needed */
            if (regold->to < cloc.curend + i + start)
                regold->to = cloc.curend + i + start;
            if (regold->from > cloc.curstart + i + start)
                regold->from = cloc.curstart + i + start;
         } else	{
            /* new window or dusted regions do not overlap */
            reg->from = cloc.curstart + i + start;
            reg->to = cloc.curend + i + start;
            regold = reg;
            reg = (DREGION*) calloc(1, sizeof(DREGION));
            if (!reg) {
               sfree(seq);
               sfree(SUM_THRESHOLD);
               return -1;
            }
            reg->next = NULL;
            regold->next = reg;
            nreg++;
         }
      }				/* end 'if' high score	*/
   }					/* end for */
   sfree (seq);
   sfree(SUM_THRESHOLD);
   return nreg;
}

static void wo (Int4 len, Uint1* seq_start, Int4 iseg, DCURLOC* cloc, 
                Uint1* seq, Uint1 FIND_TRIPLET, Int4 level)
{
	Int4 smaller_window_start, mask_window_end;
        Boolean SINGLE_TRIPLET;

	cloc->cursum = 0;
	cloc->curlength = 1;
	cloc->curstart = 0;
	cloc->curend = 0;

	if (len < 1)
		return;

        /* get the chunk of sequence in triplets */
	if (FIND_TRIPLET==1) /* Append one */
	{
		seq[len-1] = seq[len] = seq[len+1] = 0;
		dust_triplet_find (seq_start, iseg+len-1, 1, seq+len-1);
	}
	if (FIND_TRIPLET==2) /* Copy suffix as prefix and find one */
	{
		memmove(seq,seq+1,(len-1)*sizeof(Uint1));
		seq[len-1] = seq[len] = seq[len+1] = 0;
		dust_triplet_find (seq_start, iseg+len-1, 1, seq+len-1);
	}
	if (FIND_TRIPLET==3) /* Copy suffix */
		memmove(seq,seq+1,len*sizeof(Uint1));

        /* dust the chunk */
	SINGLE_TRIPLET = wo1 (len, seq, 0, cloc); /* dust at start of window */

        /* consider smaller windows only if anything interesting 
           found for starting position  and smaller windows have potential of
           being at higher level */
        if ((cloc->cursum*10 > level*cloc->curlength) && (!SINGLE_TRIPLET)) {
		mask_window_end = cloc->curend-1;
		smaller_window_start = 1;
                while ((smaller_window_start < mask_window_end) &&
                       (!SINGLE_TRIPLET)) {
			SINGLE_TRIPLET = wo1(mask_window_end-smaller_window_start,
                             seq+smaller_window_start, smaller_window_start, cloc);
                	smaller_window_start++;
	        }
	}

	cloc->curend += cloc->curstart;
}

/** returns TRUE if there is single triplet in the sequence considered */
static Boolean wo1 (Int4 len, Uint1* seq, Int4 iwo, DCURLOC* cloc)
{
   Uint4 sum;
	Int4 loop;

	Int2* countsptr;
	Int2 counts[4*4*4];
	Uint1 triplet_count = 0;

	memset (counts, 0, sizeof (counts));
/* zero everything */
	sum = 0;

/* dust loop -- specific for triplets	*/
	for (loop = 0; loop < len; loop++)
	{
		countsptr = &counts[*seq++];
		if (*countsptr)
		{
			sum += (Uint4)(*countsptr);

		    if (sum >= SUM_THRESHOLD[loop])
		    {
			if ((Uint4)cloc->cursum*loop < sum*cloc->curlength)
			{
				cloc->cursum = sum;
				cloc->curlength = loop;
				cloc->curstart = iwo;
				cloc->curend = loop + 2; /* triplets */
			}
		    }
		}
		else
			triplet_count++;
		(*countsptr)++;
	}

	if (triplet_count > 1)
		return(FALSE);
	return(TRUE);
}

/** Fill an array with 2-bit encoded triplets.
 * @param seq_start Pointer to the start of the sequence in blastna 
 *                  encoding [in]
 * @param icur Offset at which to start extracting triplets [in]
 * @param max Maximal length of the sequence segment to be processed [in]
 * @param s1 Array of triplets [out]
 * @return How far was the sequence processed?
 */
static Int4 
dust_triplet_find (Uint1* seq_start, Int4 icur, Int4 max, Uint1* s1)
{
   Int4 n;
   Uint1* s2,* s3;
   Int2 c;
   Uint1* seq = &seq_start[icur];
   Uint1 end_byte = NCBI4NA_TO_BLASTNA[NULLB];
   
   n = 0;
   
   s2 = s1 + 1;
   s3 = s1 + 2;
   
   /* set up 1 */
   if ((c = *seq++) == end_byte)
      return n;
   c &= NCBI2NA_MASK;
   *s1 |= c;
   *s1 <<= 2;
   
   /* set up 2 */
   if ((c = *seq++) == end_byte)
      return n;
   c &= NCBI2NA_MASK;
   *s1 |= c;
   *s2 |= c;
   
   /* triplet fill loop */
   while (n < max && (c = *seq++) != end_byte) {
      c &= NCBI2NA_MASK;
      *s1 <<= 2;
      *s2 <<= 2;
      *s1 |= c;
      *s2 |= c;
      *s3 |= c;
      s1++;
      s2++;
      s3++;
      n++;
   }
   
   return n;
}

/** Look for dustable locations */
static Int2 
GetDustLocations (BlastSeqLoc** loc, DREGION* reg, Int4 nreg)
{
   BlastSeqLoc* tail = NULL;   /* pointer to tail of loc linked list */
        
   if (!loc)
      return -1;
   
   *loc = NULL;

   /* point to dusted locations */
   if (nreg > 0) {
      Int4 i;
      for (i = 0; reg && i < nreg; i++) {
         /* Cache the tail of the list to avoid the overhead of traversing the
          * list when appending to it */
         tail = BlastSeqLocNew(tail ? &tail : loc, reg->from, reg->to);
         reg = reg->next;
      }
   }
#if 0
   /* N.B.: 
    * additional error checking added below results in unit test failures!
    */

   /* point to dusted locations */
   if (nreg > 0) {
      Int4 i;
      for (i = 0; reg && i < nreg; i++) {
         if (BlastSeqLocNew(loc, reg->from, reg->to) == NULL) {
             break;
         }
         reg = reg->next;
      }
      /* either BlastSeqLocNew failed or nreg didn't match the number of
       * elements in reg, so return error */
      if (reg) {
          *loc = BlastSeqLocFree(*loc);
          return -1;
      }
   }
#endif
   return 0;
}

Int2 SeqBufferDust (Uint1* sequence, Int4 length, Int4 offset,
                    Int2 level, Int2 window, Int2 linker,
                    BlastSeqLoc** dust_loc)
{
	DREGION* reg,* regold;
	Int4 nreg;
   Int2 status = 0;

        /* place for dusted regions */
	regold = reg = (DREGION*) calloc(1, sizeof(DREGION));
	if (!reg)
           return -1;

        nreg = dust_segs (sequence, length, offset, reg, (Int4)level, 
                  (Int4)window, (Int4)linker);

        status = GetDustLocations(dust_loc, reg, nreg);

        /* clean up memory */
	reg = regold;
	while (reg)
	{
		regold = reg;
		reg = reg->next;
		sfree (regold);
	}

	return status;
}
