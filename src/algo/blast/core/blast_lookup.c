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
 * ===========================================================================
 */

/** @file blast_lookup.c
 * Functions interacting with the standard BLAST lookup table.
 * Lookup table consists of a backbone hash table and an overflow array. Each
 * backbone entry is an array of 4 integers. The first is number of offsets in
 * the query, corresponding to this index. If number of offsets is <= 3, then
 * they are all placed in the backbone, otherwise all offsets are stored in the 
 * overflow array. 
 */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/blast_rps.h>
#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_encoding.h>
#include "blast_inline.h"

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/** Structure containing information needed for adding neighboring words. 
 */
typedef struct NeighborInfo {
    BlastLookupTable *lookup; /**< Lookup table */
    Uint1 *query_word;   /**< the word whose neighbors we are computing */
    Uint1 *subject_word; /**< the computed neighboring word */
    Int4 alphabet_size;  /**< number of letters in the alphabet */
    Int4 wordsize;       /**< number of residues in a word */
    Int4 **matrix;       /**< the substitution matrix */
    Int4 *row_max;       /**< maximum possible score for each row of the matrix */
    Int4 *offset_list;   /**< list of offsets where the word occurs in the query */
    Int4 threshold;      /**< the score threshold for neighboring words */
    Int4 query_bias;     /**< bias all stored offsets for multiple queries */
} NeighborInfo;

/** Add neighboring words to the lookup table.
 * @param lookup Pointer to the lookup table.
 * @param matrix Pointer to the substitution matrix.
 * @param query Pointer to the query sequence.
 * @param offset_list list of offsets where the word occurs in the query
 * @param query_bias bias all stored offsets for multiple queries
 * @param row_max maximum possible score for each row of the matrix
 */
static void AddWordHits( BlastLookupTable *lookup,
			 Int4** matrix,
			 Uint1* query,
			 Int4* offset_list,
			 Int4 query_bias,
                         Int4 *row_max);

/** Add neighboring words to the lookup table using NeighborInfo structure.
 * @param info Pointer to the NeighborInfo structure.
 * @param score The partial sum of the score.
 * @param current_pos The current offset.
 */
static void _AddWordHits(NeighborInfo *info, 
                         Int4 score, 
                         Int4 current_pos);

/** Add neighboring words to the lookup table in case of a position-specific 
 * matrix.
 * @param lookup Pointer to the lookup table.
 * @param matrix The position-specific matrix.
 * @param query_bias bias all stored offsets for multiple queries
 * @param row_max maximum possible score for each row of the matrix
 */
static void AddPSSMWordHits( BlastLookupTable *lookup,
			 Int4** matrix,
			 Int4 query_bias,
                         Int4 *row_max);

/** Add neighboring words to the lookup table in case of a position-specific 
 * matrix, using NeighborInfo structure.
 * @param info Pointer to the NeighborInfo structure.
 * @param score The partial sum of the score.
 * @param current_pos The current offset.
 */
static void _AddPSSMWordHits(NeighborInfo *info, 
                         Int4 score, 
                         Int4 current_pos);


Int4 BlastAaLookupNew(const LookupTableOptions* opt,
		      BlastLookupTable* * lut)
{
  return LookupTableNew(opt, lut, 0, TRUE);
}

Int4 RPSLookupTableNew(const BlastRPSInfo *info,
		      BlastRPSLookupTable* * lut)
{
   Int4 i;
   BlastRPSLookupFileHeader *lookup_header;
   BlastRPSProfileHeader *profile_header;
   BlastRPSLookupTable* lookup = *lut = 
      (BlastRPSLookupTable*) calloc(1, sizeof(BlastRPSLookupTable));
   Int4* pssm_start;
   Int4 num_pssm_rows;
   Int4 longest_chain;

   ASSERT(info != NULL);

   /* Fill in the lookup table information. */

   lookup_header = info->lookup_header;
   if (lookup_header->magic_number != RPS_MAGIC_NUM)
      return -1;

   lookup->wordsize = BLAST_WORDSIZE_PROT;
   lookup->alphabet_size = BLASTAA_SIZE;
   lookup->charsize = ilog2(lookup->alphabet_size) + 1;
   lookup->backbone_size = 1 << (lookup->wordsize * lookup->charsize);
   lookup->mask = lookup->backbone_size - 1;
   lookup->rps_backbone = (RPSBackboneCell *)((Uint1 *)lookup_header + 
                          lookup_header->start_of_backbone);
   lookup->overflow = (Int4 *)((Uint1 *)lookup_header + 
   			lookup_header->start_of_backbone + 
			(lookup->backbone_size + 1)* sizeof(RPSBackboneCell));
   lookup->overflow_size = lookup_header->overflow_hits;

   /* fill in the pv_array */
   
   lookup->pv = (PV_ARRAY_TYPE *)
      calloc((lookup->backbone_size >> PV_ARRAY_BTS) , sizeof(PV_ARRAY_TYPE));

   longest_chain = 0;
   for (i = 0; i < lookup->backbone_size; i++) {
      if (lookup->rps_backbone[i].num_used > 0) {
	 PV_SET(lookup,i);
      }
      if (lookup->rps_backbone[i].num_used > longest_chain) {
         longest_chain = lookup->rps_backbone[i].num_used;
      }
   }
   lookup->longest_chain = longest_chain;

   /* Fill in the PSSM information */

   profile_header = info->profile_header;
   if (profile_header->magic_number != RPS_MAGIC_NUM)
      return -2;

   lookup->rps_seq_offsets = profile_header->start_offsets;
   lookup->num_profiles = profile_header->num_profiles;
   num_pssm_rows = lookup->rps_seq_offsets[lookup->num_profiles];
   lookup->rps_pssm = (Int4 **)malloc((num_pssm_rows+1) * sizeof(Int4 *));
   pssm_start = profile_header->start_offsets + lookup->num_profiles + 1;

   for (i = 0; i < num_pssm_rows + 1; i++) {
      lookup->rps_pssm[i] = pssm_start;
      pssm_start += lookup->alphabet_size;
   }

   return 0;
}

Int4 LookupTableNew(const LookupTableOptions* opt,
		      BlastLookupTable* * lut,
                      Int4 approx_table_entries,
		      Boolean is_protein)
{
   BlastLookupTable* lookup = *lut = 
      (BlastLookupTable*) calloc(1, sizeof(BlastLookupTable));
   const Int4 kAlphabetSize = ((is_protein == TRUE) ? 
                                       BLASTAA_SIZE : BLAST2NA_SIZE);

   ASSERT(lookup != NULL);

   if (is_protein) {
      Int4 i;
      lookup->charsize = ilog2(kAlphabetSize) + 1;
      lookup->word_length = opt->word_size;
  
      for(i=0;i<lookup->word_length;i++)
         lookup->backbone_size |= (kAlphabetSize - 1) << (i * lookup->charsize);
      lookup->backbone_size += 1;
  
      lookup->mask = makemask(opt->word_size * lookup->charsize);
   } else {
      const Int4 kSmallQueryCutoff = 1250;  /* probably machine-dependent */
      const Int4 kLargeQueryCutoff = 30000;
 
      lookup->word_length = opt->word_size;
      lookup->charsize = ilog2(kAlphabetSize);
      lookup->ag_scanning_mode = TRUE; /* striding is on by default */
 
      ASSERT(lookup->word_length >= 4);
 
      /* Choose the width of the lookup table. The width may
         be any number <= the word size, but the most efficient
         width is a compromise between small values (which have
         better cache performance and allow a larger scanning 
         stride) and large values (which have fewer accesses 
         and allow fewer word extensions) */
 
      switch (lookup->word_length) {
      case 4:
      case 5:
      case 6:
         lookup->lut_word_length = lookup->word_length;
         break;
      case 7:
         if (approx_table_entries < kSmallQueryCutoff / 2)
            lookup->lut_word_length = 6;
         else
            lookup->lut_word_length = 7;
         break;
      case 8:
         lookup->lut_word_length = 7;
         break;
      case 9:
         if (approx_table_entries < kSmallQueryCutoff)
            lookup->lut_word_length = 7;
         else
            lookup->lut_word_length = 8;
         break;
      case 10:
         if (approx_table_entries < kSmallQueryCutoff)
            lookup->lut_word_length = 7;
         else
            lookup->lut_word_length = 8;
         break;
      case 11:
         lookup->lut_word_length = 8;
         lookup->ag_scanning_mode = FALSE;
         break;
      default:
         lookup->lut_word_length = 8;
         break;
      }
 
      /* if the query is large, use the largest applicable
         table width */
 
      if (approx_table_entries > kLargeQueryCutoff)
         lookup->lut_word_length = MIN(8, lookup->word_length);
 
      if (lookup->ag_scanning_mode == TRUE)
         lookup->scan_step = lookup->word_length - lookup->lut_word_length + 1;
 
      lookup->backbone_size = iexp(kAlphabetSize, lookup->lut_word_length);
      lookup->mask = lookup->backbone_size - 1;
   }
   lookup->alphabet_size = kAlphabetSize;
   lookup->threshold = opt->threshold;
   lookup->thin_backbone = 
      (Int4**) calloc(lookup->backbone_size , sizeof(Int4*));
   ASSERT(lookup->thin_backbone != NULL);

   lookup->overflow=NULL;
   return 0;
}

Int4 BlastAaLookupAddWordHit(BlastLookupTable* lookup,
                             Uint1* w,
			     Int4 query_offset)
{
  Int4 index=0;
  Int4 chain_size = 0; /* total number of elements in the chain */
  Int4 hits_in_chain = 0; /* number of occupied elements in the chain, not including the zeroth and first positions */ 
  Int4 * chain = NULL;
    
  /* compute its index, */

  _ComputeIndex(lookup->word_length,lookup->charsize,lookup->mask, w, &index);

  ASSERT(index < lookup->backbone_size);

  /* if backbone cell is null, initialize a new chain */
  if (lookup->thin_backbone[index] == NULL)
    {
      chain_size = 8;
      hits_in_chain = 0;
      chain = (Int4*) calloc( chain_size, sizeof(Int4) );
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
      ASSERT(chain != NULL);

      lookup->thin_backbone[index] = chain;
      chain[0] = chain_size;
    }
  
  /* add the hit */
  chain[ chain[1] + 2 ] = query_offset;
  chain[1] += 1;

  return 0;
}

Int4 _BlastAaLookupFinalize(BlastLookupTable* lookup)
{
  Int4 i;
  Int4 overflow_cells_needed=0;
  Int4 overflow_cursor = 0;
  Int4 longest_chain=0;
#ifdef LOOKUP_VERBOSE
  Int4 backbone_occupancy=0;
  Int4 thick_backbone_occupancy=0;
  Int4 num_overflows=0;
#endif
  
/* allocate the new lookup table */
 lookup->thick_backbone = (LookupBackboneCell *)
    calloc(lookup->backbone_size , sizeof(LookupBackboneCell));
    ASSERT(lookup->thick_backbone != NULL);

 /* allocate the pv_array */
 lookup->pv = (PV_ARRAY_TYPE *)
    calloc((lookup->backbone_size >> PV_ARRAY_BTS) + 1 , sizeof(PV_ARRAY_TYPE));
  ASSERT(lookup->pv != NULL);

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
 if (overflow_cells_needed > 0)
   {
   lookup->overflow = (Int4*) calloc( overflow_cells_needed, sizeof(Int4) );
   ASSERT(lookup->overflow != NULL);
   }

/* for each position in the lookup table backbone, */
for(i=0;i<lookup->backbone_size;i++)
    {	
    /* if there are hits there, */
    if ( lookup->thin_backbone[i] != NULL )
        {
        /* set the corresponding bit in the pv_array */
	  PV_SET(lookup,i);
#ifdef LOOKUP_VERBOSE
	  backbone_occupancy++;
#endif

        /* if there are three or fewer hits, */
        if ( (lookup->thin_backbone[i])[1] <= HITS_ON_BACKBONE )
            /* copy them into the thick_backbone cell */
            {
            Int4 j;
#ifdef LOOKUP_VERBOSE
	    thick_backbone_occupancy++;
#endif

	    lookup->thick_backbone[i].num_used = lookup->thin_backbone[i][1];

            for(j=0;j<lookup->thin_backbone[i][1];j++)
                lookup->thick_backbone[i].payload.entries[j] = lookup->thin_backbone[i][j+2];
            }
        else
	  /* more than three hits; copy to overflow array */
            {
	      Int4 j;

#ifdef LOOKUP_VERBOSE
	      num_overflows++;
#endif

	      lookup->thick_backbone[i].num_used = lookup->thin_backbone[i][1];
	      lookup->thick_backbone[i].payload.overflow_cursor = overflow_cursor;
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
 printf("backbone size: %d\n", lookup->backbone_size);
 printf("backbone occupancy: %d (%f%%)\n",backbone_occupancy,
                100.0 * backbone_occupancy/lookup->backbone_size);
 printf("thick_backbone occupancy: %d (%f%%)\n", 
                thick_backbone_occupancy,
                100.0 * thick_backbone_occupancy/lookup->backbone_size);
 printf("num_overflows: %d\n", num_overflows);
 printf("overflow size: %d\n", overflow_cells_needed);
 printf("longest chain: %d\n", longest_chain);
 printf("exact matches: %d\n", lookup->exact_matches);
 printf("neighbor matches: %d\n", lookup->neighbor_matches);
#endif

 return 0;
}


Int4 BlastAaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *subject,
                        Int4* offset,
                        BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                        Int4 array_size
		   )
{
  Int4 index=0;
  Uint1* s=NULL;
  Uint1* s_first=NULL;
  Uint1* s_last=NULL;
  Int4 numhits = 0; /* number of hits found for a given subject offset */
  Int4 totalhits = 0; /* cumulative number of hits found */
  BlastLookupTable* lookup;

  ASSERT(lookup_wrap->lut_type == AA_LOOKUP_TABLE);
  lookup = (BlastLookupTable*) lookup_wrap->lut;

  s_first = subject->sequence + *offset;
  s_last  = subject->sequence + subject->length - lookup->word_length; 

  _ComputeIndex(lookup->word_length - 1, /* prime the index */
		lookup->charsize,
		lookup->mask,
		s_first,
		&index);

  for(s=s_first; s <= s_last; s++)
    {
      /* compute the index value */
      _ComputeIndexIncremental(lookup->word_length,lookup->charsize,lookup->mask, s, &index);

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
		src = & (lookup->overflow [ lookup->thick_backbone[index].payload.overflow_cursor ] );
	      
	      /* copy the hits. */
	      for(i=0;i<numhits;i++)
		{
		  offset_pairs[i + totalhits].qs_offsets.q_off = src[i];
		  offset_pairs[i + totalhits].qs_offsets.s_off = s - subject->sequence;
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

Int4 BlastRPSScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *sequence,
                        Int4* offset,
                        BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                        Int4 array_size
		   )
{
  Int4 index=0;
  Int4 table_correction;
  Uint1* s=NULL;
  Uint1* s_first=NULL;
  Uint1* s_last=NULL;
  Int4 numhits = 0; /* number of hits found for a given subject offset */
  Int4 totalhits = 0; /* cumulative number of hits found */
  BlastRPSLookupTable* lookup;
  RPSBackboneCell *cell;

  ASSERT(lookup_wrap->lut_type == RPS_LOOKUP_TABLE);
  lookup = (BlastRPSLookupTable*) lookup_wrap->lut;

  s_first = sequence->sequence + *offset;
  s_last  = sequence->sequence + sequence->length - lookup->wordsize; 

  /* Calling code expects the returned sequence offsets to
     refer to the *first letter* in a word. The legacy RPS blast
     lookup table stores offsets to the *last* letter in each
     word, and so a correction is needed */

  table_correction = lookup->wordsize - 1;

  _ComputeIndex(lookup->wordsize - 1, /* prime the index */
		lookup->charsize,
		lookup->mask,
		s_first,
		&index);

  for(s=s_first; s <= s_last; s++)
    {
      /* compute the index value */
      _ComputeIndexIncremental(lookup->wordsize,lookup->charsize,lookup->mask,
                               s, &index);

      /* if there are hits... */
      if (PV_TEST(lookup, index))
	{
	  cell = &lookup->rps_backbone[index];
	  numhits = cell->num_used;

          ASSERT(numhits != 0);
    
	  if ( numhits <= (array_size - totalhits) )
	    {
	      Int4* src;
	      Int4 i;
	      if ( numhits <= RPS_HITS_PER_CELL ) {
		/* hits live in thick_backbone */
	        for(i=0;i<numhits;i++)
		  {
		    offset_pairs[i + totalhits].qs_offsets.q_off = 
                        cell->entries[i] - table_correction;
		    offset_pairs[i + totalhits].qs_offsets.s_off = 
                        s - sequence->sequence;
		  }
              }
	      else {
		/* hits (past the first) live in overflow array */
		src = lookup->overflow + (cell->entries[1] / sizeof(Int4));
		offset_pairs[totalhits].qs_offsets.q_off = 
                    cell->entries[0] - table_correction;
		offset_pairs[totalhits].qs_offsets.s_off = s - sequence->sequence;
	        for(i=0;i<(numhits-1);i++)
		  {
		    offset_pairs[i+totalhits+1].qs_offsets.q_off = 
                        src[i] - table_correction;
		    offset_pairs[i+totalhits+1].qs_offsets.s_off = 
                        s - sequence->sequence;
		  }
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
  *offset = s - sequence->sequence;

  return totalhits;
}


Int4 BlastAaLookupIndexQuery(BlastLookupTable* lookup,
			       Int4 ** matrix,
			       BLAST_SequenceBlk* query,
			       BlastSeqLoc* locations)
{

return _BlastAaLookupIndexQuery(lookup,
                               matrix, 
                               (lookup->use_pssm == TRUE) ? NULL : query, 
                               locations, 0);
}

Int4 _BlastAaLookupIndexQuery(BlastLookupTable* lookup,
			      Int4 ** matrix,
			      BLAST_SequenceBlk* query,
			      BlastSeqLoc* location,
                              Int4 query_bias)
{
    if (lookup->use_pssm)
        AddPSSMNeighboringWords(lookup, matrix, 
                                query_bias, location);
    else
        AddNeighboringWords(lookup, matrix, query,
                            query_bias, location);
  return 0;
}

Int4 AddNeighboringWords(BlastLookupTable* lookup, Int4 ** matrix, BLAST_SequenceBlk* query, Int4 query_bias, BlastSeqLoc* location)
{
  Int4 offset;
  Int4 i, j;
  Int4 **exact_backbone;
  Int4 **old_backbone;
  BlastSeqLoc* loc;
  Int4 *row_max;

  /* Determine the maximum possible score for
     each row of the score matrix */

  row_max = (Int4 *)malloc(lookup->alphabet_size * sizeof(Int4));
  ASSERT(row_max != NULL);

  for (i = 0; i < lookup->alphabet_size; i++) {
      row_max[i] = matrix[i][0];
      for (j = 1; j < lookup->alphabet_size; j++)
          row_max[i] = MAX(row_max[i], matrix[i][j]);
  }

  /* Swap out the existing thin backbone for an empty
     backbone */

  old_backbone = lookup->thin_backbone;
  lookup->thin_backbone = (Int4 **)calloc(lookup->backbone_size, 
                                           sizeof(Int4 *));

  /* find all the exact matches, grouping together all
     offsets of identical query words. The query bias
     is not used here, since the next stage will need real 
     offsets into the query sequence */

  for(loc=location; loc; loc=loc->next)
  {
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - lookup->word_length + 1;
      for (offset = from; offset <= to; offset++) 
      {
          Uint1* w = query->sequence + offset;
          BlastAaLookupAddWordHit(lookup, w, offset);

#ifdef LOOKUP_VERBOSE
          lookup->exact_matches++;
#endif
      }
  }

  /* return the original thin backbone */

  exact_backbone = lookup->thin_backbone;
  lookup->thin_backbone = old_backbone;

  /* walk though the list of exact matches
     previously computed. Find neighboring words
     for entire lists at a time */

  for (i = 0; i < lookup->backbone_size; i++)
  {
      if (exact_backbone[i] != NULL)
      {
          AddWordHits(lookup, matrix, query->sequence,
                      exact_backbone[i], query_bias, row_max);
          sfree(exact_backbone[i]);
      }
  }

  sfree(exact_backbone);
  sfree(row_max);
  return 0;
}

static void AddWordHits(BlastLookupTable* lookup, Int4** matrix, 
			Uint1* query, Int4 *offset_list, 
                        Int4 query_bias, Int4 *row_max)
{
  Uint1* w;
  Uint1 s[32];
  Int4 score;
  Int4 i;
  NeighborInfo info;

  /* All of the offsets in the list refer to the
     same query word. Thus, neighboring words only
     have to be found for the first offset in the
     list (since all other offsets would have the 
     same neighbors) */

  w = query + offset_list[2];

  /* Compute the self-score of this word */

  score = matrix[w[0]][w[0]];
  for (i = 1; i < lookup->word_length; i++)
      score += matrix[w[i]][w[i]];

  /* If the self-score is above the threshold, then the
     neighboring computation will automatically add the
     word to the lookup table. Otherwise, either the score
     is too low or neighboring is not done at all, so that
     all of these exact matches must be explicitly added
     to the lookup table */

  if (lookup->threshold == 0 || score < lookup->threshold)
  {
      for (i = 0; i < offset_list[1]; i++)
          BlastAaLookupAddWordHit(lookup, w,
                                  query_bias + offset_list[i+2]);
  }
  else
  {
      lookup->neighbor_matches -= offset_list[1];
  }

  /* check if neighboring words need to be found */

  if (lookup->threshold == 0)
      return;

  /* Set up the structure of information to be used
     during the recursion */

  info.lookup = lookup;
  info.query_word = w;
  info.subject_word = s;
  info.alphabet_size = lookup->alphabet_size;
  info.wordsize = lookup->word_length;
  info.matrix = matrix;
  info.row_max = row_max;
  info.offset_list = offset_list;
  info.threshold = lookup->threshold;
  info.query_bias = query_bias;

  /* compute the largest possible score that any neighboring
     word can have; this maximum will gradually be replaced 
     by exact scores as subject words are built up */

  score = row_max[w[0]];
  for (i = 1; i < lookup->word_length; i++)
      score += row_max[w[i]];

  _AddWordHits(&info, score, 0);
}

static void _AddWordHits(NeighborInfo *info, Int4 score, Int4 current_pos)
{
    Int4 alphabet_size = info->alphabet_size;
    Int4 threshold = info->threshold;
    Uint1 *query_word = info->query_word;
    Uint1 *subject_word = info->subject_word;
    Int4 *row;
    Int4 i;

    /* remove the maximum score of letters that
       align with the query letter at position 
       'current_pos'. Later code will align the
       entire alphabet with this letter, and compute
       the exact score each time. Also point to the 
       row of the score matrix corresponding to the
       query letter at current_pos */

    score -= info->row_max[query_word[current_pos]];
    row = info->matrix[query_word[current_pos]];

    if (current_pos == info->wordsize - 1) {

        /* The recursion has bottomed out, and we can
           produce complete subject words. Pass the
           entire alphabet through the last position
           in the subject word, then save the list of
           query offsets in all positions corresponding
           to subject words that yield a high enough score */

        Int4 *offset_list = info->offset_list;
        Int4 query_bias = info->query_bias;
        BlastLookupTable *lookup = info->lookup;
        Int4 j;

        for (i = 0; i < alphabet_size; i++) {
            if (score + row[i] >= threshold) {
                subject_word[current_pos] = i;
                for (j = 0; j < offset_list[1]; j++) {
                    BlastAaLookupAddWordHit(lookup, subject_word,
                                            query_bias + offset_list[j+2]);
                }
                lookup->neighbor_matches += offset_list[1];
            }
        }
        return;
    }

    /* Otherwise, pass the entire alphabet through position
       current_pos of the subject word, and recurse on all
       words that could possibly exceed the threshold later */

    for (i = 0; i < alphabet_size; i++) {
        if (score + row[i] >= threshold) {
            subject_word[current_pos] = i;
            _AddWordHits(info, score + row[i], current_pos + 1);
        }
    }
}

Int4 AddPSSMNeighboringWords(BlastLookupTable* lookup, Int4 ** matrix, Int4 query_bias, BlastSeqLoc *location)
{
  Int4 offset;
  Int4 i, j;
  BlastSeqLoc* loc;
  Int4 *row_max;
  Int4 wordsize = lookup->word_length;

  /* for PSSMs, we only have to track the maximum
     score of 'wordsize' matrix columns */

  row_max = (Int4 *)malloc(lookup->word_length * sizeof(Int4));
  ASSERT(row_max != NULL);

  for(loc=location; loc; loc=loc->next)
  {
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - wordsize + 1;
      Int4 **row = matrix + from;

      /* prepare to start another run of adjacent query
         words. Find the maximum possible score for the
         first wordsize-1 rows of the PSSM */

      if (to >= from)
      {
          for (i = 0; i < wordsize - 1; i++) 
          {
              row_max[i] = row[i][0];
              for (j = 1; j < lookup->alphabet_size; j++)
                  row_max[i] = MAX(row_max[i], row[i][j]);
          }
      }

      for (offset = from; offset <= to; offset++, row++) 
      {
          /* find the maximum score of the next PSSM row */

          row_max[wordsize - 1] = row[wordsize - 1][0];
          for (i = 1; i < lookup->alphabet_size; i++)
              row_max[wordsize - 1] = MAX(row_max[wordsize - 1], 
                                          row[wordsize - 1][i]);

          /* find all neighboring words */

          AddPSSMWordHits(lookup, row, offset + query_bias, row_max);

          /* shift the list of maximum scores over by one,
             to make room for the next maximum in the next
             loop iteration */

          for (i = 0; i < wordsize - 1; i++)
              row_max[i] = row_max[i+1];
      }
  }

  sfree(row_max);
  return 0;
}

static void AddPSSMWordHits(BlastLookupTable* lookup, Int4** matrix, 
			    Int4 offset, Int4 *row_max)
{
  Uint1 s[32];
  Int4 score;
  Int4 i;
  NeighborInfo info;

  /* Set up the structure of information to be used
     during the recursion */

  info.lookup = lookup;
  info.query_word = NULL;
  info.subject_word = s;
  info.alphabet_size = lookup->alphabet_size;
  info.wordsize = lookup->word_length;
  info.matrix = matrix;
  info.row_max = row_max;
  info.offset_list = NULL;
  info.threshold = lookup->threshold;
  info.query_bias = offset;

  /* compute the largest possible score that any neighboring
     word can have; this maximum will gradually be replaced 
     by exact scores as subject words are built up */

  score = row_max[0];
  for (i = 1; i < lookup->word_length; i++)
      score += row_max[i];

  _AddPSSMWordHits(&info, score, 0);
}

static void _AddPSSMWordHits(NeighborInfo *info, Int4 score, Int4 current_pos)
{
    Int4 alphabet_size = info->alphabet_size;
    Int4 threshold = info->threshold;
    Uint1 *subject_word = info->subject_word;
    Int4 *row;
    Int4 i;

    /* remove the maximum score of letters that
       align with the query letter at position 
       'current_pos'. Later code will align the
       entire alphabet with this letter, and compute
       the exact score each time. Also point to the 
       row of the score matrix corresponding to the
       query letter at current_pos */

    score -= info->row_max[current_pos];
    row = info->matrix[current_pos];

    if (current_pos == info->wordsize - 1) {

        /* The recursion has bottomed out, and we can
           produce complete subject words. Pass the
           entire alphabet through the last position
           in the subject word, then save the query offset
           in all lookup table positions corresponding
           to subject words that yield a high enough score */

        Int4 offset = info->query_bias;
        BlastLookupTable *lookup = info->lookup;

        for (i = 0; i < alphabet_size; i++) {
            if (score + row[i] >= threshold) {
                subject_word[current_pos] = i;
                BlastAaLookupAddWordHit(lookup, subject_word, offset);
                lookup->neighbor_matches++;
            }
        }
        return;
    }

    /* Otherwise, pass the entire alphabet through position
       current_pos of the subject word, and recurse on all
       words that could possibly exceed the threshold later */

    for (i = 0; i < alphabet_size; i++) {
        if (score + row[i] >= threshold) {
            subject_word[current_pos] = i;
            _AddPSSMWordHits(info, score + row[i], current_pos + 1);
        }
    }
}

/******************************************************
 *
 * Nucleotide BLAST specific functions and definitions
 *
 ******************************************************/

/* Description in na_lookup.h */
Int4 BlastNaScanSubject_AG(const LookupTableWrap* lookup_wrap,
                           const BLAST_SequenceBlk* subject,
                           Int4 start_offset,
                           BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                           Int4 max_hits, Int4* end_offset)
{
   BlastLookupTable* lookup;
   Uint1* s;
   Uint1* abs_start;
   Int4 q_off, s_off;
   Int4* lookup_pos;
   Int4 num_hits;
   Int4 index;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 scan_step;
   Int4 mask;
   Int4 lut_word_length;
   Int4 last_offset;

   ASSERT(lookup_wrap->lut_type == NA_LOOKUP_TABLE);
   lookup = (BlastLookupTable*) lookup_wrap->lut;
  
   pv_array = lookup->pv;

   abs_start = subject->sequence;
   mask = lookup->mask;
   scan_step = lookup->scan_step;
   lut_word_length = lookup->lut_word_length;
   last_offset = subject->length - lut_word_length;

   ASSERT(lookup->scan_step > 0);

   if (lut_word_length > 5) {

      /* perform scanning for lookup tables of width 6, 7, and 8.
         These widths require two bytes of the compressed subject
         sequence, and possibly a third if the word is not aligned
         on a 4-base boundary */

      if (scan_step % COMPRESSION_RATIO == 0) { 
   
         /* for strides that are a multiple of 4, words are
            always aligned and two bytes of the subject sequence 
            will always hold a complete word (plus possible extra bases 
            that must be shifted away). s_end below points to either
            the last byte of the subject sequence or the second to
            last; all subject sequences must therefore have a sentinel
            byte */

         Uint1* s_end = abs_start + (subject->length - lut_word_length) /
                                           COMPRESSION_RATIO;
         Int4 shift = 2 * (FULL_BYTE_SHIFT - lut_word_length);
         s = abs_start + start_offset/COMPRESSION_RATIO;
         scan_step = scan_step / COMPRESSION_RATIO;
   
         for ( ; s <= s_end; s += scan_step) {
            index = s[0] << 8 | s[1];
            index = index >> shift;
            
            if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
               num_hits = lookup->thick_backbone[index].num_used;
               ASSERT(num_hits != 0);
               if (num_hits > (max_hits - total_hits))
                  break;
   
               /* determine if hits live in the backbone or the
                  overflow array */
   
               if (num_hits <= HITS_ON_BACKBONE)
                  lookup_pos = lookup->thick_backbone[index].payload.entries;
               else
                  lookup_pos = lookup->overflow + 
                        lookup->thick_backbone[index].payload.overflow_cursor;
               
               s_off = (s - abs_start)*COMPRESSION_RATIO;
               while (num_hits) {
                  q_off = *((Uint4 *) lookup_pos); /* get next query offset */
                  lookup_pos++;
                  num_hits--;
                  
                  offset_pairs[total_hits].qs_offsets.q_off = q_off;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               }
            }
         }
         *end_offset = (s - abs_start)*COMPRESSION_RATIO;
      } else {
         /* when the stride is not a multiple of 4, extra bases
            may occur both before and after every word read from
            the subject sequence. The portion of each 12-base
            region that contains the actual word depends on the
            offset of the word and the lookup table width, and
            must be recalculated for each 12-base region */

         for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {
   
            Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);
            
            index = s[0] << 16 | s[1] << 8 | s[2];
            index = (index >> shift) & mask;
   
            if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
               num_hits = lookup->thick_backbone[index].num_used;
               ASSERT(num_hits != 0);
               if (num_hits > (max_hits - total_hits))
                  break;
   
               if (num_hits <= HITS_ON_BACKBONE)
                  lookup_pos = lookup->thick_backbone[index].payload.entries;
               else
                  lookup_pos = lookup->overflow + 
                      lookup->thick_backbone[index].payload.overflow_cursor;
               
               while (num_hits) {
                  q_off = *((Uint4 *) lookup_pos); /* get next query offset */
                  lookup_pos++;
                  num_hits--;
                  
                  offset_pairs[total_hits].qs_offsets.q_off = q_off;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               }
            }
         }
         *end_offset = s_off;
      }
   }
   else {
      /* perform scanning for lookup tables of width 4 and 5.
         Here the stride will never be a multiple of 4 (these tables
         are only used for very small word sizes) */

      for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {

         Int4 shift = 2*(8 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
         
         index = s[0] << 8 | s[1];
         index = (index >> shift) & mask;

         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            ASSERT(num_hits != 0);
            if (num_hits > (max_hits - total_hits))
               break;

            if (num_hits <= HITS_ON_BACKBONE)
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               lookup_pos = lookup->overflow + 
                   lookup->thick_backbone[index].payload.overflow_cursor;
            
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               offset_pairs[total_hits].qs_offsets.q_off = q_off;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
            }
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

/* Description in blast_lookup.h */
Int4 BlastNaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk* subject, 
                        Int4 start_offset,
                        BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                        Int4 max_hits, Int4* end_offset)
{
   Uint1* s;
   Uint1* abs_start,* s_end;
   Int4 q_off, s_off;
   BlastLookupTable* lookup;
   Int4* lookup_pos;
   Int4 num_hits;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 lut_word_length;

   ASSERT(lookup_wrap->lut_type == NA_LOOKUP_TABLE);
   lookup = (BlastLookupTable*) lookup_wrap->lut;

   pv_array = lookup->pv;
   lut_word_length = lookup->lut_word_length;
   ASSERT(lut_word_length == 8);

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   s_end = abs_start + (subject->length - lut_word_length) /
                                        COMPRESSION_RATIO;

   for (; s <= s_end; s++) {

      Int4 index = s[0] << 8 | s[1];

      if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
         num_hits = lookup->thick_backbone[index].num_used;
         ASSERT(num_hits != 0);

         if (num_hits > (max_hits - total_hits))
            break;

         if (num_hits <= HITS_ON_BACKBONE)
            lookup_pos = lookup->thick_backbone[index].payload.entries;
         else
            lookup_pos = lookup->overflow + 
                       lookup->thick_backbone[index].payload.overflow_cursor;
         
         s_off = (s - abs_start)*COMPRESSION_RATIO;
         while (num_hits) {
            q_off = *((Uint4 *) lookup_pos); /* get next query offset */
            lookup_pos++;
            num_hits--;
            
            offset_pairs[total_hits].qs_offsets.q_off = q_off;
            offset_pairs[total_hits++].qs_offsets.s_off = s_off;
         }
      }
   }
   *end_offset = (s - abs_start)*COMPRESSION_RATIO;

   return total_hits;
}

BlastLookupTable* LookupTableDestruct(BlastLookupTable* lookup)
{
   sfree(lookup->thick_backbone);
   sfree(lookup->overflow);
   sfree(lookup->pv);
   sfree(lookup);
   return NULL;
}

BlastRPSLookupTable* RPSLookupTableDestruct(BlastRPSLookupTable* lookup)
{
   /* The following will only free memory that was 
      allocated by RPSLookupTableNew. */
   sfree(lookup->rps_pssm);
   sfree(lookup->pv);
   sfree(lookup);
   return NULL;
}

/** Add a word information to the lookup table 
 * @param lookup Pointer to the lookup table structure [in] [out]
 * @param w Pointer to the start of a word [in]
 * @param query_offset Offset into the query sequence where this word ends [in]
 */
static Int4 BlastNaLookupAddWordHit(BlastLookupTable* lookup, Uint1* w,
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
      chain = calloc( chain_size, sizeof(Int4) );
      ASSERT(chain != NULL);
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
      ASSERT(chain != NULL);
      lookup->thin_backbone[index] = chain;
      chain[0] = chain_size;
    }
  
  /* Add the hit */
  chain[ chain[1] + 2 ] = query_offset;
  chain[1] += 1;

  return 0;
}

/* See description in blast_lookup.h */
Int4 BlastNaLookupIndexQuery(BlastLookupTable* lookup, BLAST_SequenceBlk* query,
			BlastSeqLoc* location)
{
  BlastSeqLoc* loc;
  Int4 offset;
  Uint1* sequence;

  for(loc=location; loc; loc=loc->next) {
     Int4 from = loc->ssr->left;
     Int4 to = loc->ssr->right + 1;

     /* if this location is too small to fit a complete
        word, skip the location */

     if (lookup->word_length > (to - from))
         continue;  
     
     /* Compute the last offset such that a full lookup table word
        can be created */
     to -= lookup->lut_word_length;
     sequence = query->sequence + from;
     for(offset = from; offset <= to; offset++) {
	BlastNaLookupAddWordHit(lookup, sequence, offset);
	++sequence;
     }
  }

  return 0;
}
