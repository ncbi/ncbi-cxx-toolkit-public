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
#include <algo/blast/core/aa_lookup.h>
#include <algo/blast/core/util.h>

static char const rcsid[] = "$Id$";

static NCBI_INLINE Int4 ComputeWordScore(Int4 ** matrix,
				    Int4 wordsize,
				    const Uint1* w1,
				    const Uint1* w2,
				    Boolean *exact);

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
     lookup->charsize = ilog2(opt->alphabet_size / COMPRESSION_RATIO); 
     lookup->wordsize = (opt->word_size - 3) / COMPRESSION_RATIO;
     lookup->reduced_wordsize = (lookup->wordsize >= 2) ? 2 : 1;
     lookup->backbone_size = 
       iexp(2,lookup->reduced_wordsize*lookup->charsize*COMPRESSION_RATIO);
     lookup->mask = lookup->backbone_size - 1;
     if (opt->scan_step > 0) {
        /* For old style word finding, scan_step MUST be set to non-zero in 
           options */
        lookup->scan_step = opt->scan_step;
     } else { /* For AG style word finding */
        lookup->scan_step = lookup->word_length - 
           lookup->reduced_wordsize*COMPRESSION_RATIO + 1;
     }
  }
  lookup->alphabet_size = opt->alphabet_size;
  lookup->exact_matches=0;
  lookup->neighbor_matches=0;
  lookup->threshold = opt->threshold;
  lookup->thin_backbone = 
     (Int4**) calloc(lookup->backbone_size , sizeof(Int4*));

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
    *index = ((*index << charsize) | word[i]) & mask;
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
      _BlastAaLookupIndexQuery(lookup, matrix, &(query[i]), &(locations[i]));
    }

  /* free neighbor array*/
  sfree(lookup->neighbors);
  lookup->neighbors=NULL;

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
      to = ((DoubleInt*) loc->ptr)->i2 - lookup->wordsize;

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
  Uint1* s = lookup->neighbors;
  Uint1* s_end=s + lookup->neighbors_length - lookup->wordsize;
  Boolean exact;
  Int4 score;
  Uint1* w = query->sequence + offset;
  
  if (lookup->threshold == 0)
    {
      BlastAaLookupAddWordHit(lookup, w, offset);
      lookup->exact_matches++;
      return 0;
    }
  
  while (s < s_end)
    {
      score = ComputeWordScore(matrix,lookup->wordsize,w,s,&exact);
      
      /*
       * If the score is higher than the threshold or this is an exact
       * match, add the word to the LUT.
       */
      
      if (exact)
	{
        lookup->exact_matches++;
        /*printf("exact match at offset %d\n",offset);*/
        }
        
      if ((score >= lookup->threshold) && (!exact))
	{
        lookup->neighbor_matches++;
        /*printf("neighbor match at offset %d\n",offset);*/
        }
        
      if ( (score >= lookup->threshold) || exact )
        BlastAaLookupAddWordHit(lookup,s,offset);
      s ++;
    }
  return 0;
}

/**
 *  Compute the score of a pair of words using a subtitution matrix.
 *
 * @param matrix the substitution matrix [in]
 * @param wordsize the word size [in]
 * @param w1 the first word [in
 * @param w2 the second word [in]
 * @param exact a pointer to a boolean which specifies whether or not this was an exact match or not.
 * @return The score of the pair of words.
 */
static NCBI_INLINE Int4 ComputeWordScore(Int4 ** matrix,
				    Int4 wordsize,
				    const Uint1* w1, /* the first word */
				    const Uint1* w2, /* the second word */
				    Boolean *exact)
{
Int4 i;
Int4 score=0;

*exact=TRUE;

for(i=0;i<wordsize;i++)
  {
    score += matrix[ w1[i] ][ w2[i] ];
    if ( w1[i] != w2[i] )
      *exact=FALSE;
  }
 
return score;
}
