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
  return LookupTableNew(opt, lut, TRUE);
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
		      Boolean is_protein)
{
   BlastLookupTable* lookup = *lut = 
      (BlastLookupTable*) calloc(1, sizeof(BlastLookupTable));
   const Int4 kAlphabetSize = ((is_protein == TRUE) ? BLASTAA_SIZE : BLASTNA_SIZE);
   const Int4 kMinAGWordSize = 13;

   ASSERT(lookup != NULL);

 lookup->ag_scanning_mode = FALSE;  /* Set to TRUE if appropriate below. */ 
 if (is_protein)
 {
    Int4 i;
    lookup->charsize = ilog2(kAlphabetSize) + 1;
    lookup->wordsize = opt->word_size;

    lookup->backbone_size = 0;
    for(i=0;i<lookup->wordsize;i++)
        lookup->backbone_size |= (kAlphabetSize - 1) << (i * lookup->charsize);
    lookup->backbone_size += 1;

    lookup->mask = makemask(opt->word_size * lookup->charsize);
  } else {
     lookup->word_length = opt->word_size;
     lookup->charsize = ilog2(kAlphabetSize/COMPRESSION_RATIO); 
     if (opt->word_size >= kMinAGWordSize)  
     {
          lookup->scan_step = CalculateBestStride(opt->word_size, opt->variable_wordsize, 
               opt->lut_type);
          lookup->wordsize = (opt->word_size - MIN(lookup->scan_step,COMPRESSION_RATIO) + 1) 
               / COMPRESSION_RATIO;
          lookup->ag_scanning_mode = TRUE;
     }
     else
     {    
          lookup->scan_step = 0;
          lookup->wordsize = (opt->word_size - COMPRESSION_RATIO + 1) / COMPRESSION_RATIO;
     }


     lookup->reduced_wordsize = (lookup->wordsize >= 2) ? 2 : 1;
     lookup->backbone_size = 
       iexp(2,lookup->reduced_wordsize*lookup->charsize*COMPRESSION_RATIO);

     lookup->mask = lookup->backbone_size - 1;
  }
  lookup->alphabet_size = kAlphabetSize;
  lookup->exact_matches=0;
  lookup->neighbor_matches=0;
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

  _ComputeIndex(lookup->wordsize,lookup->charsize,lookup->mask, w, &index);

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
  Int4 backbone_occupancy=0;
  Int4 thick_backbone_occupancy=0;
  Int4 num_overflows=0;
  Int4 longest_chain=0;
  
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
 printf("backbone size : %d\nbackbone occupancy: %d (%f%%)\nthick_backbone occupancy: %d (%f%%)\nnum_overflows: %d\noverflow size: %d\nlongest chain: %d\n",lookup->backbone_size, backbone_occupancy, 100.0 * (float) backbone_occupancy/ (float) lookup->backbone_size, thick_backbone_occupancy, 100.0 * (float) thick_backbone_occupancy / (float) lookup->backbone_size, num_overflows, overflow_cells_needed,longest_chain);

 printf("exact matches : %d\nneighbor matches : %d\n",lookup->exact_matches,lookup->neighbor_matches);
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
      Int4 to = loc->ssr->right - lookup->wordsize + 1;
      for (offset = from; offset <= to; offset++) 
      {
          Uint1* w = query->sequence + offset;
          BlastAaLookupAddWordHit(lookup, w, offset);
          lookup->exact_matches++;
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
  for (i = 1; i < lookup->wordsize; i++)
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
  info.wordsize = lookup->wordsize;
  info.matrix = matrix;
  info.row_max = row_max;
  info.offset_list = offset_list;
  info.threshold = lookup->threshold;
  info.query_bias = query_bias;

  /* compute the largest possible score that any neighboring
     word can have; this maximum will gradually be replaced 
     by exact scores as subject words are built up */

  score = row_max[w[0]];
  for (i = 1; i < lookup->wordsize; i++)
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
  Int4 wordsize = lookup->wordsize;

  /* for PSSMs, we only have to track the maximum
     score of 'wordsize' matrix columns */

  row_max = (Int4 *)malloc(lookup->wordsize * sizeof(Int4));
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
  info.wordsize = lookup->wordsize;
  info.matrix = matrix;
  info.row_max = row_max;
  info.offset_list = NULL;
  info.threshold = lookup->threshold;
  info.query_bias = offset;

  /* compute the largest possible score that any neighboring
     word can have; this maximum will gradually be replaced 
     by exact scores as subject words are built up */

  score = row_max[0];
  for (i = 1; i < lookup->wordsize; i++)
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
   Int4  index=0, s_off;
   Int4* lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 compressed_wordsize, compressed_scan_step;
   Int4 i;

   ASSERT(lookup_wrap->lut_type == NA_LOOKUP_TABLE);
   lookup = (BlastLookupTable*) lookup_wrap->lut;
  
   pv_array = lookup->pv;

   ASSERT(lookup->scan_step > 0);

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   compressed_scan_step = lookup->scan_step / COMPRESSION_RATIO;
   compressed_wordsize = lookup->reduced_wordsize;

   index = 0;
   
   /* NB: s in this function always points to the start of the word!
    */
   if (lookup->scan_step % COMPRESSION_RATIO == 0) {  /* scan step is a multiple of four letters that fit into one byte. */
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
               lookup_pos = & ( lookup->overflow[lookup->thick_backbone[index].payload.overflow_cursor] );
            
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
            BlastNaLookupAdjustIndex(s, index, lookup->mask, bit);
         
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
               lookup_pos = & (lookup->overflow [ lookup->thick_backbone[adjusted_index].payload.overflow_cursor]);
            
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
   Int4  index=0, s_off;
   BlastLookupTable* lookup;
   Int4* lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 reduced_word_length;
   Int4 i;

   ASSERT(lookup_wrap->lut_type == NA_LOOKUP_TABLE);
   lookup = (BlastLookupTable*) lookup_wrap->lut;

   pv_array = lookup->pv;
   reduced_word_length = lookup->reduced_wordsize*COMPRESSION_RATIO;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   /* s_end points to the place right after the last full sequence byte */ 
   s_end = 
      abs_start + (*end_offset + reduced_word_length)/COMPRESSION_RATIO;

   index = 0;
   
   /* Compute the first index */
   for (i = 0; i < lookup->reduced_wordsize; ++i)
      index = ((index)<<FULL_BYTE_SHIFT) | *s++;

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
            lookup_pos = & (lookup->overflow[lookup->thick_backbone[index].payload.overflow_cursor]);
         
         /* Save the hits offsets */
         s_off = (s - abs_start)*COMPRESSION_RATIO - reduced_word_length;
         while (num_hits) {
            q_off = *((Uint4 *) lookup_pos); /* get next query offset */
            lookup_pos++;
            num_hits--;
            
            offset_pairs[total_hits].qs_offsets.q_off = q_off;
            offset_pairs[total_hits++].qs_offsets.s_off = s_off;
         }
      }

      /* Compute the next value of the index */
      index = (((index)<<FULL_BYTE_SHIFT) & lookup->mask) | (*s++);  

   }
   /* Ending offset should point to the start of the word that ends 
      at s */
   *end_offset = (s - abs_start)*COMPRESSION_RATIO - reduced_word_length;

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

     /* If the line below is not true we can never find an exact match of lookup->word_length
     bases to initiate an extension.  This happens when the user specified word length is longer
     than the one used for the lookup table. */
     if (lookup->word_length > (to - from))
         continue;  
     
     sequence = query->sequence + from;
     /* Last offset is such that full word fits in the sequence */
     to -= lookup->reduced_wordsize*COMPRESSION_RATIO;
     for(offset = from; offset <= to; offset++) {
	BlastNaLookupAddWordHit(lookup, sequence, offset);
	++sequence;
     }
  }

  return 0;
}
