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

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/lookup_util.h>

static char const rcsid[] = "$Id$";

static void AddWordHits( LookupTable *lookup,
			 Int4** matrix,
			 Uint1* word,
			 Int4 offset);

static void AddPSSMWordHits( LookupTable *lookup,
			 Int4** matrix,
			 Int4 offset);

static NCBI_INLINE void  _ComputeIndex(Int4 wordsize,
				  Int4 charsize,
				  Int4 mask,
				  const Uint1* word,
				  Int4* index);

static NCBI_INLINE void  _ComputeIndexIncremental(Int4 wordsize,
					     Int4 charsize,
					     Int4 mask,
					     const Uint1* word,
					     Int4* index);

Int4 BlastAaLookupNew(const LookupTableOptions* opt,
		      LookupTable* * lut)
{
  return LookupTableNew(opt, lut, TRUE);
}

Int4 LookupTableNew(const LookupTableOptions* opt,
		      LookupTable* * lut,
		      Boolean is_protein)
{
   LookupTable* lookup = *lut = 
      (LookupTable*) malloc(sizeof(LookupTable));

  if (is_protein) {
    lookup->charsize = ilog2(opt->alphabet_size) + 1;
    lookup->wordsize = opt->word_size;
    lookup->backbone_size = iexp(2,lookup->charsize*opt->word_size);
    lookup->mask = makemask(opt->word_size * lookup->charsize);
  } else {
     lookup->word_length = opt->word_size;
     lookup->scan_step = opt->scan_step;
     lookup->charsize = ilog2(opt->alphabet_size / COMPRESSION_RATIO); 
     lookup->wordsize = 
        (opt->word_size - MIN(lookup->scan_step,COMPRESSION_RATIO) + 1) 
        / COMPRESSION_RATIO;
     lookup->reduced_wordsize = (lookup->wordsize >= 2) ? 2 : 1;
     lookup->backbone_size = 
       iexp(2,lookup->reduced_wordsize*lookup->charsize*COMPRESSION_RATIO);
     lookup->mask = lookup->backbone_size - 1;
  }
  lookup->alphabet_size = opt->alphabet_size;
  lookup->exact_matches=0;
  lookup->neighbor_matches=0;
  lookup->threshold = opt->threshold;
  lookup->thin_backbone = 
     (Int4**) calloc(lookup->backbone_size , sizeof(Int4*));
  lookup->use_pssm = opt->use_pssm;
  lookup->neighbors=NULL;
  return 0;
}

Int4 BlastAaLookupAddWordHit(LookupTable* lookup, /* in/out: the lookup table */
                             Uint1* w,
			     Int4 query_offset)
{
  Int4 index=0;
  Int4 chain_size = 0; /* total number of elements in the chain */
  Int4 hits_in_chain = 0; /* number of occupied elements in the chain, not including the zeroth and first positions */ 
  Int4 * chain = NULL;
    
  /* compute its index, */
  _ComputeIndex(lookup->wordsize,lookup->charsize,lookup->mask, w, &index);

  ASSERT(index < lookup->backbone_size);
      
  /* if backbone cell is null, initialize a new chain */
  if (lookup->thin_backbone[index] == NULL)
    {
      chain_size = 8;
      hits_in_chain = 0;
      chain = (Int4*) malloc( chain_size * sizeof(Int4) );
      ASSERT(chain != NULL);
      chain[0] = chain_size;
      chain[1] = hits_in_chain;
      lookup->thin_backbone[index] = chain;
    }
  else
    /* otherwise, use the existing chain */
    {
      chain = lookup->thin_backbone[index];
      chain_size = chain[0];
      hits_in_chain = chain[1];
    }
  
  /* if the chain is full, allocate more room */
  if ( (hits_in_chain + 2) == chain_size )
    {
      chain_size = chain_size * 2;
      chain = (Int4*) realloc(chain, chain_size * sizeof(Int4) );
      lookup->thin_backbone[index] = chain;
      chain[0] = chain_size;
    }
  
  /* add the hit */
  chain[ chain[1] + 2 ] = query_offset;
  chain[1] += 1;

  return 0;
}

Int4 _BlastAaLookupFinalize(LookupTable* lookup)
{
  Int4 i;
  Int4 overflow_cells_needed=0;
  Int4 overflow_cursor = 0;
  Int4 backbone_occupancy=0;
  Int4 thick_backbone_occupancy=0;
  Int4 num_overflows=0;
  Int4 longest_chain=0;
  
/* allocate the new lookup table */
 lookup->thick_backbone = (LookupBackboneCell *)
    calloc(lookup->backbone_size , sizeof(LookupBackboneCell));

 /* allocate the pv_array */
 lookup->pv = (PV_ARRAY_TYPE *)
    calloc((lookup->backbone_size >> PV_ARRAY_BTS) , sizeof(PV_ARRAY_TYPE));

 /* find out how many cells have >3 hits */
 for(i=0;i<lookup->backbone_size;i++)
   if (lookup->thin_backbone[i] != NULL)
     {
     if (lookup->thin_backbone[i][1] > HITS_ON_BACKBONE)
       overflow_cells_needed += lookup->thin_backbone[i][1];

     if (lookup->thin_backbone[i][1] > longest_chain)
       longest_chain = lookup->thin_backbone[i][1];
     }

 lookup->longest_chain = longest_chain;

 /* allocate the overflow array */
 lookup->overflow = (Int4*) malloc( overflow_cells_needed * sizeof(Int4) );

/* for each position in the lookup table backbone, */
for(i=0;i<lookup->backbone_size;i++)
    {	
    /* if there are hits there, */
    if ( lookup->thin_backbone[i] != NULL )
        {
        /* set the corresponding bit in the pv_array */
	  PV_SET(lookup,i);
	  backbone_occupancy++;

        /* if there are three or fewer hits, */
        if ( (lookup->thin_backbone[i])[1] <= HITS_ON_BACKBONE )
            /* copy them into the thick_backbone cell */
            {
            Int4 j;
	    thick_backbone_occupancy++;

	    lookup->thick_backbone[i].num_used = lookup->thin_backbone[i][1];

            for(j=0;j<lookup->thin_backbone[i][1];j++)
                lookup->thick_backbone[i].payload.entries[j] = lookup->thin_backbone[i][j+2];
            }
        else
	  /* more than three hits; copy to overflow array */
            {
	      Int4 j;

	      num_overflows++;

	      lookup->thick_backbone[i].num_used = lookup->thin_backbone[i][1];
	      lookup->thick_backbone[i].payload.overflow = & (lookup->overflow[overflow_cursor]);
	      for(j=0;j<lookup->thin_backbone[i][1];j++)
		{
                lookup->overflow[overflow_cursor] = lookup->thin_backbone[i][j+2];
		overflow_cursor++;
		}
            }

    /* done with this chain- free it */
        sfree(lookup->thin_backbone[i]);
	lookup->thin_backbone[i]=NULL;
        }

    else
        /* no hits here */
        {
        lookup->thick_backbone[i].num_used=0;
        }
    } /* end for */

 lookup->overflow_size = overflow_cursor;

/* done copying hit info- free the backbone */
 sfree(lookup->thin_backbone);
 lookup->thin_backbone=NULL;

#ifdef LOOKUP_VERBOSE
 printf("backbone size : %d\nbackbone occupancy: %d (%f%%)\nthick_backbone occupancy: %d (%f%%)\nnum_overflows: %d\noverflow size: %d\nlongest chain: %d\n",lookup->backbone_size, backbone_occupancy, 100.0 * (float) backbone_occupancy/ (float) lookup->backbone_size, thick_backbone_occupancy, 100.0 * (float) thick_backbone_occupancy / (float) lookup->backbone_size, num_overflows, overflow_cells_needed,longest_chain);

 printf("exact matches : %d\nneighbor matches : %d\n",lookup->exact_matches,lookup->neighbor_matches);
#endif

 return 0;
}

/** Given a word, compute its index value from scratch.
 *
 * @param wordsize length of the word, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param mask value used to mask the index so that only the bottom wordsize * charsize bits remain [in]
 * @param word pointer to the beginning of the word [in]
 * @param index the computed index value [out] 
 */

static NCBI_INLINE void  _ComputeIndex(Int4 wordsize,
				  Int4 charsize,
				  Int4 mask,
				  const Uint1* word,
				  Int4* index)
{
  Int4 i;

  *index = 0;
  for(i=0;i<wordsize;i++)
{
    *index = ((*index << charsize) | word[i]) & mask;
}

  return;
}

/** Given a word, compute its index value, reusing a previously 
 *  computed index value.
 *
 * @param wordsize length of the word - 1, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param mask value used to mask the index so that only the bottom wordsize * charsize bits remain [in]
 * @param word pointer to the beginning of the word [in]
 * @param index the computed index value [in/out]
 */

static NCBI_INLINE void  _ComputeIndexIncremental(Int4 wordsize,
					     Int4 charsize,
					     Int4 mask,
					     const Uint1* word,
					     Int4* index)
{
  *index = ((*index << charsize) | word[wordsize - 1]) & mask;
  return;
}


Int4 BlastAaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *subject,
                        Int4* offset,
                        Uint4 * query_offsets,
                        Uint4 * subject_offsets,
                        Int4 array_size
		   )
{
  Int4 index=0;
  Uint1* s=NULL;
  Uint1* s_first=NULL;
  Uint1* s_last=NULL;
  Int4 numhits = 0; /* number of hits found for a given subject offset */
  Int4 totalhits = 0; /* cumulative number of hits found */
  LookupTable* lookup = lookup_wrap->lut;

  s_first = subject->sequence + *offset;
  s_last  = subject->sequence + subject->length - lookup->wordsize; 

  _ComputeIndex(lookup->wordsize - 1, /* prime the index */
		lookup->charsize,
		lookup->mask,
		s_first,
		&index);

  for(s=s_first; s <= s_last; s++)
    {
      /* compute the index value */
      _ComputeIndexIncremental(lookup->wordsize,lookup->charsize,lookup->mask, s, &index);

      /* if there are hits... */
      if (PV_TEST(lookup, index))
	{
	  numhits = lookup->thick_backbone[index].num_used;

          ASSERT(numhits != 0);
    
	  /* ...and there is enough space in the destination array, */
	  if ( numhits <= (array_size - totalhits) )
	    /* ...then copy the hits to the destination */
	    {
	      Int4* src;
	      Int4 i;
	      if ( numhits <= HITS_ON_BACKBONE )
		/* hits live in thick_backbone */
		src = lookup->thick_backbone[index].payload.entries;
	      else
		/* hits live in overflow array */
		src = lookup->thick_backbone[index].payload.overflow;
	      
	      /* copy the hits. */
	      for(i=0;i<numhits;i++)
		{
		  query_offsets[i + totalhits] = src[i];
		  subject_offsets[i + totalhits] = s - subject->sequence;
		}

	      totalhits += numhits;
	    }
	  else
	    /* not enough space in the destination array; return early */
	    {
	      break;
	    }
	}
      else
	/* no hits found */
	{
	}
    }

  /* if we get here, we fell off the end of the sequence */
  *offset = s - subject->sequence;

  return totalhits;
}


Int4 BlastAaLookupIndexQueries(LookupTable* lookup,
			       Int4 ** matrix,
			       BLAST_SequenceBlk* query,
			       ListNode* locations,
			       Int4 num_queries)
{
  Int4 i;

  /* create neighbor array */

  if (lookup->threshold>0)
    {
      MakeAllWordSequence(lookup);
    }

  /* index queries */
  for(i=0;i<num_queries;i++)
    {
      _BlastAaLookupIndexQuery(lookup, matrix, (lookup->use_pssm == TRUE) ? NULL : &(query[i]), 
         &(locations[i]));
    }

  /* free neighbor array*/
  if (lookup->neighbors != NULL)
    sfree(lookup->neighbors);

  return 0;
}

Int4 _BlastAaLookupIndexQuery(LookupTable* lookup,
			      Int4 ** matrix,
			      BLAST_SequenceBlk* query,
			      ListNode* location)
{
  ListNode* loc;
  Int4 from, to;
  Int4 w;

  for(loc=location; loc; loc=loc->next)
    {
      from = ((DoubleInt*) loc->ptr)->i1;
      to = ((DoubleInt*) loc->ptr)->i2 - lookup->wordsize + 1;

      for(w=from;w<=to;w++)
	{
	  AddNeighboringWords(lookup,
			      matrix,
			      query,
			      w);
	}      
    }
  return 0;
}

Int4 MakeAllWordSequence(LookupTable* lookup)
{
  Int4 k,n;
  Int4 i;
  Int4 len;
  k=lookup->alphabet_size;
  n=lookup->wordsize;

  /* need k^n bytes for the de Bruijn sequence and n-1 to unwrap it. */ 

  len = iexp(k,n) + (n-1);
  
  lookup->neighbors_length = len;

  lookup->neighbors = (Uint1*) malloc( len );

  /* generate the de Bruijn sequence */

  debruijn(n,k,lookup->neighbors,NULL);

  /* unwrap it */

  for(i=0;i<(n-1);i++)
    lookup->neighbors[len-((n-1)+1)+i] = lookup->neighbors[i];
  
  return 0;
}

Int4 AddNeighboringWords(LookupTable* lookup, Int4 ** matrix, BLAST_SequenceBlk* query, Int4 offset)
{
  Uint1* w = NULL;

  ASSERT(query != NULL);

  w = query->sequence + offset;
  
  if (lookup->threshold == 0)
    {
      BlastAaLookupAddWordHit(lookup, w, offset);
      lookup->exact_matches++;
      return 0;
    }
  
  if (lookup->use_pssm)
      AddPSSMWordHits(lookup, matrix, offset);
  else
      AddWordHits(lookup, matrix, w, offset);
  return 0;
}

static void AddWordHits(LookupTable* lookup, Int4** matrix, 
			Uint1* word, Int4 offset)
{
  Uint1* s = lookup->neighbors;
  Uint1* s_end=s + lookup->neighbors_length - lookup->wordsize;
  Uint1* w = word;
  Uint1 different;
  Int4 score;
  Int4 threshold = lookup->threshold;
  Int4 i;
  Int4* p0;
  Int4* p1;
  Int4* p2;

  /* For each group of 'wordsize' bytes starting at 'w', 
   * add the group to the lookup table at each offset 's' if
   * either
   *    - w matches s, or 
   *    - the score derived from aligning w and s is high enough
   */

  switch (lookup->wordsize)
    {
      case 1:
	p0 = matrix[w[0]];

        while (s < s_end)
          {
            if ( (s[0] == w[0]) || (p0[s[0]] >= threshold) )
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;

      case 2:
	p0 = matrix[w[0]];
	p1 = matrix[w[1]];

        while (s < s_end)
          {
            score = p0[s[0]] + p1[s[1]];
	    different = (w[0] ^ s[0]) | (w[1] ^ s[1]);
  
            if ( !different || (score >= threshold) )
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
	  }

        break;

      case 3:
	p0 = matrix[w[0]];
	p1 = matrix[w[1]];
	p2 = matrix[w[2]];

        while (s < s_end)
          {
            score = p0[s[0]] + p1[s[1]] + p2[s[2]];
	    different = (w[0] ^ s[0]) | (w[1] ^ s[1]) | (w[2] ^ s[2]);
  
            if ( !different || (score >= threshold) )
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;

      default:
        while (s < s_end)
          {
            score = 0;
            different = 0;
            for (i = 0; i < lookup->wordsize; i++)
              {
                score += matrix[w[i]][s[i]];
                different |= w[i] ^ s[i];
              }
  
            if ( !different || (score >= threshold) )
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;
    }
}


static void AddPSSMWordHits(LookupTable* lookup, Int4** matrix, Int4 offset)
{
  Uint1* s = lookup->neighbors;
  Uint1* s_end=s + lookup->neighbors_length - lookup->wordsize;
  Int4 score;
  Int4 threshold = lookup->threshold;
  Int4 i;
  Int4* p0;
  Int4* p1;
  Int4* p2;

  /* Equivalent to AddWordHits(), except that the word score
   * is derived from a Position-Specific Scoring Matrix (PSSM) 
   */

  switch (lookup->wordsize)
    {
      case 1:
	p0 = matrix[offset];

        while (s < s_end)
          {
            if (p0[s[0]] >= threshold)
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;

      case 2:
	p0 = matrix[offset];
	p1 = matrix[offset+1];

        while (s < s_end)
          {
            score = p0[s[0]] + p1[s[1]];
  
            if (score >= threshold)
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;

      case 3:
	p0 = matrix[offset];
	p1 = matrix[offset + 1];
	p2 = matrix[offset + 2];

        while (s < s_end)
          {
            score = p0[s[0]] + p1[s[1]] + p2[s[2]];
  
            if (score >= threshold)
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;

      default:
        while (s < s_end)
          {
            score = 0;
            for(i=0;i<lookup->wordsize;i++)
                score += matrix[offset + i][s[i]];
  
            if (score >= threshold)
                BlastAaLookupAddWordHit(lookup,s,offset);
            s++;
  	  }

        break;
    }
}


/******************************************************
 *
 * Nucleotide BLAST specific functions and definitions
 *
 ******************************************************/

/** Given a starting position of a word in a compressed nucleotide sequence, 
 *  compute this word's lookup table index
 * @param length The length of the word [in]
 * @param word Pointer to the start of the word [in]
 * @param index Lookup table index [out]
 * @return Pointer to the next byte after the end of the word
 */
static NCBI_INLINE Uint1* BlastNaLookupInitIndex(Int4 length,
		          const Uint1* word, Int4* index)
{
   Int4 i;
   
   *index = 0;
   for (i = 0; i < length; ++i)
      *index = ((*index)<<FULL_BYTE_SHIFT) | word[i];
   return (Uint1 *) (word + length);
}

/** Recompute the word index given its previous value and the new location 
 *  of the last byte of the word
 * @param scan_shift The number of bits to shift the previous word [in]
 * @param mask The mask to cut off the higher bits of the previous word [in]
 * @param word Pointer to the beginning of the previous word [in]
 * @param index The lookup index value for the previous word [in]
 * @return The value of the lookup index for the next word 
 */
static NCBI_INLINE Int4 BlastNaLookupComputeIndex(Int4 scan_shift, Int4 mask, 
		      const Uint1* word, Int4 index)
{
   return (((index)<<scan_shift) & mask) | *(word);  
   
}

/** Given a word computed from full bytes of a compressed sequence, 
 *  shift it by 0-3 bases 
 * @param s Pointer to the start of a word in the compressed sequence [in]
 * @param index The unadjusted lookup index [in] 
 * @param mask The mask to cut off the unneeded bits from the shifted word [in]
 * @param bit By how many bits the word should be shifted for adjusted index
 *        recomputation
 * @return The adjusted value of the lookup index
 */
static NCBI_INLINE Int4 BlastNaLookupAdjustIndex(Uint1* s, Int4 index, 
                      Int4 mask, Uint1 bit)
{
   return (((index)<<bit) & mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));

}

/* Description in na_lookup.h */
Int4 BlastNaScanSubject_AG(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,  
       Int4* end_offset)
{
   LookupTable* lookup = (LookupTable*) lookup_wrap->lut;
   Uint1* s;
   Uint1* abs_start;
   Int4  index=0, s_off;
   
   Int4* lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array = lookup->pv;
   Int4 total_hits = 0;
   Int4 compressed_wordsize, compressed_scan_step;
   Boolean full_byte_scan = (lookup->scan_step % COMPRESSION_RATIO == 0);
   Int4 i;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   compressed_scan_step = lookup->scan_step / COMPRESSION_RATIO;
   compressed_wordsize = lookup->reduced_wordsize;

   index = 0;
   
   /* NB: s in this function always points to the start of the word!
    */
   if (full_byte_scan) {
      Uint1* s_end = abs_start + subject->length/COMPRESSION_RATIO - 
         compressed_wordsize;
      for ( ; s <= s_end; s += compressed_scan_step) {
         index = 0;
         for (i = 0; i < compressed_wordsize; ++i)
            index = ((index)<<FULL_BYTE_SHIFT) | s[i];
         
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            ASSERT(num_hits != 0);
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = (Int4*)
                  (lookup->thick_backbone[index].payload.overflow);
            
            s_off = 
               ((s - abs_start) + compressed_wordsize)*COMPRESSION_RATIO;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }
      }
      *end_offset = (s - abs_start)*COMPRESSION_RATIO;
   } else {
      Int4 reduced_word_length = compressed_wordsize*COMPRESSION_RATIO;
      Int4 last_offset = subject->length - reduced_word_length;
      Uint1 bit;
      Int4 adjusted_index;

      for (s_off = start_offset; s_off <= last_offset; 
           s_off += lookup->scan_step) {
         s = abs_start + (s_off / COMPRESSION_RATIO);
         bit = 2*(s_off % COMPRESSION_RATIO);
         /* Compute index for a word made of full bytes */
         index = 0;
         for (i = 0; i < compressed_wordsize; ++i)
            index = ((index)<<FULL_BYTE_SHIFT) | (*s++);
         adjusted_index = 
            (((index)<<bit) & lookup->mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));
         
         if (NA_PV_TEST(pv_array, adjusted_index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[adjusted_index].num_used;
            ASSERT(num_hits != 0);
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[adjusted_index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = (Int4*)
                 (lookup->thick_backbone[adjusted_index].payload.overflow);
            
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off + reduced_word_length;
            }
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

/* Description in na_lookup.h */
Int4 BlastNaScanSubject(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits, 
       Int4* end_offset)
{
   Uint1* s;
   Uint1* abs_start,* s_end;
   Int4  index=0, s_off;
   LookupTable* lookup = (LookupTable*) lookup_wrap->lut;
   Int4* lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array = lookup->pv;
   Int4 total_hits = 0;
   Boolean full_byte_scan = (lookup->scan_step % COMPRESSION_RATIO == 0);
   Int4 i;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   /* s_end points to the place right after the last full sequence byte */ 
   s_end = abs_start + (*end_offset)/COMPRESSION_RATIO;
   index = 0;
   
   /* Compute the first index */
   for (i = 0; i < lookup->reduced_wordsize; ++i)
      index = ((index)<<FULL_BYTE_SHIFT) | *s++;

   if (full_byte_scan) {
      /* s points to the byte right after the end of the current word */
      while (s <= s_end) {
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            ASSERT(num_hits != 0);
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = 
                  (Int4*)(lookup->thick_backbone[index].payload.overflow);
            
            /* Save the hits offsets */
            s_off = (s - abs_start)*COMPRESSION_RATIO;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }

         /* Compute the next value of the index */
         index = (((index)<<FULL_BYTE_SHIFT) & lookup->mask) | (*s++);  

      }
      /* Ending offset should point to the start of the word that ends 
         at s */
      *end_offset = ((s - abs_start) - lookup->reduced_wordsize)*COMPRESSION_RATIO;
   } else {
      Int4 scan_shift = 2*lookup->scan_step;
      Uint1 bit = 2*(start_offset % COMPRESSION_RATIO);
      Int4 adjusted_index;

      /* s points to the byte right after the end of the current word */
      while (s <= s_end) {
         /* Adjust the word index by the base within a byte */
         adjusted_index = BlastNaLookupAdjustIndex(s, index, lookup->mask,
                                                   bit);
         
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            ASSERT(num_hits != 0);
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = 
                  (Int4*)(lookup->thick_backbone[index].payload.overflow);
            
            /* Save the hits offsets */
            s_off = (s - abs_start)*COMPRESSION_RATIO + bit/2;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }
         bit += scan_shift;
         if (bit >= FULL_BYTE_SHIFT) {
            /* Advance to the next full byte */
            bit -= FULL_BYTE_SHIFT;
            index = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                                lookup->mask, s++, index);
         }
      }
      /* Ending offset should point to the start of the word that ends 
         at s */
      *end_offset = 
         ((s - abs_start) - lookup->reduced_wordsize)*COMPRESSION_RATIO
         + bit/2;
   }

   return total_hits;
}

LookupTable* LookupTableDestruct(LookupTable* lookup)
{
   sfree(lookup->thick_backbone);
   sfree(lookup->overflow);
   sfree(lookup->pv);
   sfree(lookup);
   return NULL;
}

/** Compute the lookup index for a word in an uncompressed sequence, without 
 *  using any previous index information.
 * @param lookup Pointer to the traditional BLASTn lookup table structure [in]
 * @param word Pointer to the start of the word [in]
 * @param index The lookup index [out]
 */
#define BLAST2NA_MASK 0xfc
static NCBI_INLINE Int2
Na_LookupComputeIndex(LookupTable* lookup, Uint1* word, Int4* index)
{
   Int4 i;
   Int4 wordsize = lookup->reduced_wordsize*COMPRESSION_RATIO; /* i.e. 8 or 4 */

   *index = 0;
   for (i = 0; i < wordsize; ++i) {
      if ((word[i] & BLAST2NA_MASK) != 0) {
	 *index = 0;
	 return -1;
      } else {
	 *index = (((*index)<<lookup->charsize) & lookup->mask) | word[i];
      }
   }
   return 0;
}

/** Add a word information to the lookup table 
 * @param lookup Pointer to the lookup table structure [in] [out]
 * @param w Pointer to the start of a word [in]
 * @param query_offset Offset into the query sequence where this word ends [in]
 */
static Int4 BlastNaLookupAddWordHit(LookupTable* lookup, Uint1* w,
                                    Int4 query_offset)
{
  Int4 index=0;
  Int4 chain_size = 0; /* Total number of elements in the chain */
  Int4 hits_in_chain = 0; /* Number of occupied elements in the chain, not 
                             including the zeroth and first positions */ 
  Int4* chain = NULL;

  /* compute its index */
  if (Na_LookupComputeIndex(lookup, w, &index) == -1)
     /* Word contains ambiguities, skip it */
     return 0;

  ASSERT(index < lookup->backbone_size);
      
  /* If backbone cell is null, initialize a new chain */
  if (lookup->thin_backbone[index] == NULL)
    {
      chain_size = 8;
      hits_in_chain = 0;
      chain = malloc( chain_size * sizeof(Int4) );
      chain[0] = chain_size;
      chain[1] = hits_in_chain;
      lookup->thin_backbone[index] = chain;
    }
  else
    /* Otherwise, use the existing chain */
    {
      chain = lookup->thin_backbone[index];
      chain_size = chain[0];
      hits_in_chain = chain[1];
    }
  
  /* If the chain is full, allocate more room */
  if ( (hits_in_chain + 2) == chain_size )
    {
      chain_size = chain_size * 2;
      chain = realloc(chain, chain_size * sizeof(Int4) );
      lookup->thin_backbone[index] = chain;
      chain[0] = chain_size;
    }
  
  /* Add the hit */
  chain[ chain[1] + 2 ] = query_offset;
  chain[1] += 1;

  return 0;
}

/** Fill the BLASTn lookup table.
 * @param lookup The lookup table structure [in] [out]
 * @param query The query sequence [in]
 * @param location What part of the query should be indexed? [in]
 */
Int4 BlastNaLookupIndexQuery(LookupTable* lookup, BLAST_SequenceBlk* query,
			ListNode* location)
{
  ListNode* loc;
  Int4 from, to;
  Int4 offset;
  Uint1* sequence;

  for(loc=location; loc; loc=loc->next) {
     from = ((DoubleInt*) loc->ptr)->i1;
     to = ((DoubleInt*) loc->ptr)->i2;
     
     sequence = query->sequence + from;
     /* Make the offsets point to the ends of the words */
     from += lookup->reduced_wordsize*COMPRESSION_RATIO;
     for(offset = from; offset < to; offset++) {
	BlastNaLookupAddWordHit(lookup, sequence, offset);
	++sequence;
     }
  }

  return 0;
}
