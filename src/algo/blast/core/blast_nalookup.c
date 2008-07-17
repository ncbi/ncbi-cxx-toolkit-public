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

/** @file blast_nalookup.c
 * Functions for constructing nucleotide blast lookup tables
 */

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

/** bitfield used to detect ambiguities in uncompressed
 *  nucleotide letters
 */
#define BLAST2NA_MASK 0xfc

/** number of bits in a compressed nucleotide letter */
#define BITS_PER_NUC 2

ELookupTableType
BlastChooseNaLookupTable(const LookupTableOptions* lookup_options,	
                         Int4 approx_table_entries, Int4 max_q_off,
                         Int4 *lut_width)
{
   ELookupTableType lut_type;

   /* Choose the width of the lookup table. The width may be any number
      <= the word size, but the most efficient width is a compromise
      between small values (which have better cache performance and
      allow a larger scanning stride) and large values (which have fewer 
      accesses and allow fewer word extensions) The number of entries 
      where one table width becomes better than another is probably 
      machine-dependent */

   ASSERT(lookup_options->word_size >= 4);

   /* Discontiguous megablast must always use a megablast table */

   if (lookup_options->mb_template_length > 0) {
      *lut_width = lookup_options->word_size;
      return eMBLookupTable;
   }

   switch(lookup_options->word_size) {
   case 4:
   case 5:
   case 6:
      lut_type = eSmallNaLookupTable;
      *lut_width = lookup_options->word_size;
      break;

   case 7:
      lut_type = eSmallNaLookupTable;
      if (approx_table_entries < 250)
         *lut_width = 6;
      else
         *lut_width = 7;
      break;

   case 8:
      lut_type = eSmallNaLookupTable;
      if (approx_table_entries < 8500)
         *lut_width = 7;
      else
         *lut_width = 8;
      break;

   case 9:
      if (approx_table_entries < 1250) {
         *lut_width = 7;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 21000) {
         *lut_width = 8;
         lut_type = eSmallNaLookupTable;
      } else {
         *lut_width = 9;
         lut_type = eMBLookupTable;
      }
      break;

   case 10:
      if (approx_table_entries < 1250) {
         *lut_width = 7;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 8500) {
         *lut_width = 8;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 18000) {
         *lut_width = 9;
         lut_type = eMBLookupTable;
      } else {
         *lut_width = 10;
         lut_type = eMBLookupTable;
      }
      break;
      
   case 11:
      if (approx_table_entries < 12000) {
         *lut_width = 8;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 180000) {
         *lut_width = 10;
         lut_type = eMBLookupTable;
      } else {
         *lut_width = 11;
         lut_type = eMBLookupTable;
      }
      break;

   case 12:
      if (approx_table_entries < 8500) {
         *lut_width = 8;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 18000) {
         *lut_width = 9;
         lut_type = eMBLookupTable;
      } else if (approx_table_entries < 60000) {
         *lut_width = 10;
         lut_type = eMBLookupTable;
      } else if (approx_table_entries < 900000) {
         *lut_width = 11;
         lut_type = eMBLookupTable;
      } else {
         *lut_width = 12;
         lut_type = eMBLookupTable;
      }
      break;

   default:
      if (approx_table_entries < 8500) {
         *lut_width = 8;
         lut_type = eSmallNaLookupTable;
      } else if (approx_table_entries < 300000) {
         *lut_width = 11;
         lut_type = eMBLookupTable;
      } else {
         *lut_width = 12;
         lut_type = eMBLookupTable;
      }
      break;
   }

   /* we only use the ordinary blastn table for cases where
      the number of words to index, or the maximum query offset,
      exceeds the range of the 15-bit values used in the 
      small blastn lookup table */

   if (lut_type == eSmallNaLookupTable && 
       (approx_table_entries >= 32767 || max_q_off >= 32768)) {
      lut_type = eNaLookupTable;
   }
   return lut_type;
}

/*--------------------- Small nucleotide table  ----------------------*/

/** Pack the data structures comprising a small nucleotide lookup table
 * into their final form
 * @param thin_backbone structure containing indexed query offsets [in][out]
 * @param lookup the lookup table [in]
 * @param query the query sequence [in][out]
 * @return zero if packing process succeeded
 */
static Int4 s_BlastSmallNaLookupFinalize(Int4 **thin_backbone,
                                         BlastSmallNaLookupTable * lookup,
                                         BLAST_SequenceBlk *query)
{
    Int4 i;
    Int4 overflow_cells_needed = 2;
    Int4 overflow_cursor = 2;
    Int4 longest_chain = 0;
#ifdef LOOKUP_VERBOSE
    Int4 backbone_occupancy = 0;
    Int4 thick_backbone_occupancy = 0;
    Int4 num_overflows = 0;
#endif

    /* find out how many cells need the overflow array. The
       backbone holds at most one hit per cell, so any cells 
       that need more than that must go into the overflow array 
       (along with a trailing null). */
       
    for (i = 0; i < lookup->backbone_size; i++) {
        if (thin_backbone[i] != NULL) {
            Int4 num_hits = thin_backbone[i][1];
            if (num_hits > 1)
                overflow_cells_needed += num_hits + 1;
            longest_chain = MAX(longest_chain, num_hits);
        }
    }

    /* there is a hard limit to the number of query offsets
       allowed in the overflow array. Although unlikely, it
       is technically possible to index a query sequence that
       has so many trailing nulls in the overflow array that
       the limit gets exceeded */

    if (overflow_cells_needed >= 32768) {
       for (i = 0; i < lookup->backbone_size; i++)
          sfree(thin_backbone[i]);
       return -1;
    }

    /* compute a compressed representation of the query, used
       for computing ungapped extensions */

    BlastCompressBlastnaSequence(query);

    /* allocate the new lookup table */
    lookup->final_backbone = (Int2 *)malloc(
                               lookup->backbone_size * sizeof(Int2));
    ASSERT(lookup->final_backbone != NULL);

    lookup->longest_chain = longest_chain;

    /* allocate the overflow array */
    if (overflow_cells_needed > 0) {
        lookup->overflow = (Int2 *) malloc(overflow_cells_needed * 
                                           sizeof(Int2));
        ASSERT(lookup->overflow != NULL);
    }

    /* for each position in the lookup table backbone, */
    for (i = 0; i < lookup->backbone_size; i++) {

        Int4 j, num_hits;

        /* skip if there are no hits in cell i */
        if (thin_backbone[i] == NULL) {
            lookup->final_backbone[i] = -1;
            continue;
        }

#ifdef LOOKUP_VERBOSE
        backbone_occupancy++;
#endif
        num_hits = thin_backbone[i][1];

        if (num_hits == 1) {

           /* if there is only one hit, it goes into the backbone */

#ifdef LOOKUP_VERBOSE
            thick_backbone_occupancy++;
#endif
            lookup->final_backbone[i] = thin_backbone[i][2];
        } 
        else {
#ifdef LOOKUP_VERBOSE
            num_overflows++;
#endif
            /* for more than one hit, the backbone stores
               -(overflow offset where hits occur). Since a
               cell value of -1 is reserved to mean 'empty cell',
               the value stored begins at -2 */
            lookup->final_backbone[i] = -overflow_cursor;
            for (j = 0; j < num_hits; j++) {
                lookup->overflow[overflow_cursor++] =
                    thin_backbone[i][j + 2];
            }

            /* we don't have the room to store the number of hits,
               so append a null to the end of the list to signal
               that the current chain is finished */
            lookup->overflow[overflow_cursor++] = -1;
        }

        /* done with this chain */
        sfree(thin_backbone[i]);
    }

    lookup->overflow_size = overflow_cursor;

#ifdef LOOKUP_VERBOSE
    printf("SmallNa\n");
    printf("backbone size: %d\n", lookup->backbone_size);
    printf("backbone occupancy: %d (%f%%)\n", backbone_occupancy,
           100.0 * backbone_occupancy / lookup->backbone_size);
    printf("thick_backbone occupancy: %d (%f%%)\n",
           thick_backbone_occupancy,
           100.0 * thick_backbone_occupancy / lookup->backbone_size);
    printf("num_overflows: %d\n", num_overflows);
    printf("overflow size: %d\n", overflow_cells_needed);
    printf("longest chain: %d\n", longest_chain);
#endif

    return 0;
}

/** Changes the list of locations into a list of 
   the intervals between locations (the inverse).
   @param locations input list [in]
   @param length (query) sequence length [in]
   @return inverted BlastSeqLoc
*/

static BlastSeqLoc* s_SeqLocListInvert(const BlastSeqLoc* locations, Int4 length)
{
     BlastSeqLoc* retval = NULL;
     BlastSeqLoc* tail = NULL;  /* Tail of the list. */
     Int4 start, stop;

     ASSERT(locations);

     start = 0;
     stop = MAX( 0, locations->ssr->left);

     if (stop - start > 2)
        tail = BlastSeqLocNew(&retval, start, stop);

     while (locations)
     {
         start = locations->ssr->right;
         locations = locations->next;

         if (locations)
             stop = locations->ssr->left;
         else
             stop = length;

         if (stop - start > 2)
         {
            if (retval == NULL)
               tail = BlastSeqLocNew(&retval, start, stop);
            else
               tail = BlastSeqLocNew(&tail, start, stop);
         }
     }
     return retval;
}

Int4 BlastSmallNaLookupTableNew(BLAST_SequenceBlk* query, 
                           BlastSeqLoc* locations,
                           BlastSmallNaLookupTable * *lut,
                           const LookupTableOptions * opt, 
                           const QuerySetUpOptions* query_options,
                           Int4 lut_width)
{
    Int4 status = 0;
    Int4 **thin_backbone;
    BlastSmallNaLookupTable *lookup = 
        (BlastSmallNaLookupTable *) calloc(1, sizeof(BlastSmallNaLookupTable));

    ASSERT(lookup != NULL);

    lookup->word_length = opt->word_size;
    lookup->lut_word_length = lut_width;
    lookup->backbone_size = 1 << (BITS_PER_NUC * lookup->lut_word_length);
    lookup->mask = lookup->backbone_size - 1;
    lookup->overflow = NULL;
    lookup->scan_step = lookup->word_length - lookup->lut_word_length + 1;

    thin_backbone = (Int4 **) calloc(lookup->backbone_size, sizeof(Int4 *));
    ASSERT(thin_backbone != NULL);

    BlastLookupIndexQueryExactMatches(thin_backbone,
                                      lookup->word_length,
                                      BITS_PER_NUC,
                                      lookup->lut_word_length,
                                      query, locations);
    if (locations && lookup->word_length > lookup->lut_word_length && 
       ((query_options->filtering_options && SBlastFilterOptionsMaskAtHash(query_options->filtering_options)) ||
        (query_options->filter_string && strstr(query_options->filter_string, "m"))))
    {
       lookup->masked_locations = s_SeqLocListInvert(locations, query->length);
    }

    status = s_BlastSmallNaLookupFinalize(thin_backbone, lookup, query);
    if (status != 0) {
        lookup = BlastSmallNaLookupTableDestruct(lookup);
    }

    sfree(thin_backbone);
    *lut = lookup;
    return status;
}

BlastSmallNaLookupTable *BlastSmallNaLookupTableDestruct(
                                    BlastSmallNaLookupTable * lookup)
{
    sfree(lookup->final_backbone);
    sfree(lookup->overflow);
    if (lookup->masked_locations)
       lookup->masked_locations = BlastSeqLocFree(lookup->masked_locations);
    sfree(lookup);
    return NULL;
}


/*--------------------- Standard nucleotide table  ----------------------*/

/** Pack the data structures comprising a nucleotide lookup table
 * into their final form
 * @param thin_backbone structure containing indexed query offsets [in][out]
 * @param lookup the lookup table [in]
 */
static void s_BlastNaLookupFinalize(Int4 **thin_backbone,
                                    BlastNaLookupTable * lookup)
{
    Int4 i;
    Int4 overflow_cells_needed = 0;
    Int4 overflow_cursor = 0;
    Int4 longest_chain = 0;
    PV_ARRAY_TYPE *pv;
#ifdef LOOKUP_VERBOSE
    Int4 backbone_occupancy = 0;
    Int4 thick_backbone_occupancy = 0;
    Int4 num_overflows = 0;
#endif

    /* allocate the new lookup table */
    lookup->thick_backbone = (NaLookupBackboneCell *)calloc(
                                           lookup->backbone_size, 
                                           sizeof(NaLookupBackboneCell));
    ASSERT(lookup->thick_backbone != NULL);

    /* allocate the pv_array */
    pv = lookup->pv = (PV_ARRAY_TYPE *)calloc(
                              (lookup->backbone_size >> PV_ARRAY_BTS) + 1,
                              sizeof(PV_ARRAY_TYPE));
    ASSERT(pv != NULL);

    /* find out how many cells need the overflow array */
    for (i = 0; i < lookup->backbone_size; i++) {
        if (thin_backbone[i] != NULL) {
            Int4 num_hits = thin_backbone[i][1];
            if (num_hits > NA_HITS_PER_CELL)
                overflow_cells_needed += num_hits;
            longest_chain = MAX(longest_chain, num_hits);
        }
    }

    lookup->longest_chain = longest_chain;

    /* allocate the overflow array */
    if (overflow_cells_needed > 0) {
        lookup->overflow = (Int4 *) calloc(overflow_cells_needed, sizeof(Int4));
        ASSERT(lookup->overflow != NULL);
    }

    /* for each position in the lookup table backbone, */
    for (i = 0; i < lookup->backbone_size; i++) {

        Int4 j, num_hits;

        /* skip if there are no hits in cell i */
        if (thin_backbone[i] == NULL)
            continue;

#ifdef LOOKUP_VERBOSE
        backbone_occupancy++;
#endif
        num_hits = thin_backbone[i][1];
        lookup->thick_backbone[i].num_used = num_hits;

        PV_SET(pv, i, PV_ARRAY_BTS);

        /* if there are few enough hits, copy them into 
           the thick_backbone cell; otherwise copy all 
           hits to the overflow array */

        if (num_hits <= NA_HITS_PER_CELL) {
#ifdef LOOKUP_VERBOSE
            thick_backbone_occupancy++;
#endif
            for (j = 0; j < num_hits; j++) {
                lookup->thick_backbone[i].payload.entries[j] =
                                     thin_backbone[i][j + 2];
            }
        } 
        else {
#ifdef LOOKUP_VERBOSE
            num_overflows++;
#endif
            lookup->thick_backbone[i].payload.overflow_cursor =
                                         overflow_cursor;
            for (j = 0; j < num_hits; j++) {
                lookup->overflow[overflow_cursor] =
                    thin_backbone[i][j + 2];
                overflow_cursor++;
            }
        }

        /* done with this chain */
        sfree(thin_backbone[i]);
    }

    lookup->overflow_size = overflow_cursor;

#ifdef LOOKUP_VERBOSE
    printf("backbone size: %d\n", lookup->backbone_size);
    printf("backbone occupancy: %d (%f%%)\n", backbone_occupancy,
           100.0 * backbone_occupancy / lookup->backbone_size);
    printf("thick_backbone occupancy: %d (%f%%)\n",
           thick_backbone_occupancy,
           100.0 * thick_backbone_occupancy / lookup->backbone_size);
    printf("num_overflows: %d\n", num_overflows);
    printf("overflow size: %d\n", overflow_cells_needed);
    printf("longest chain: %d\n", longest_chain);
#endif
}

Int4 BlastNaLookupTableNew(BLAST_SequenceBlk* query, 
                           BlastSeqLoc* locations,
                           BlastNaLookupTable * *lut,
                           const LookupTableOptions * opt, 
                           Int4 lut_width)
{
    Int4 **thin_backbone;
    BlastNaLookupTable *lookup = *lut =
        (BlastNaLookupTable *) calloc(1, sizeof(BlastNaLookupTable));

    ASSERT(lookup != NULL);

    lookup->word_length = opt->word_size;
    lookup->lut_word_length = lut_width;
    lookup->backbone_size = 1 << (BITS_PER_NUC * lookup->lut_word_length);
    lookup->mask = lookup->backbone_size - 1;
    lookup->overflow = NULL;
    lookup->scan_step = lookup->word_length - lookup->lut_word_length + 1;

    thin_backbone = (Int4 **) calloc(lookup->backbone_size, sizeof(Int4 *));
    ASSERT(thin_backbone != NULL);

    BlastLookupIndexQueryExactMatches(thin_backbone,
                                      lookup->word_length,
                                      BITS_PER_NUC,
                                      lookup->lut_word_length,
                                      query, locations);
    s_BlastNaLookupFinalize(thin_backbone, lookup);
    sfree(thin_backbone);
    return 0;
}

BlastNaLookupTable *BlastNaLookupTableDestruct(BlastNaLookupTable * lookup)
{
    sfree(lookup->thick_backbone);
    sfree(lookup->overflow);
    sfree(lookup->pv);
    sfree(lookup);
    return NULL;
}


/*--------------------- Megablast table ---------------------------*/

/** Convert weight, template length and template type from input options into
    an MBTemplateType enum
*/
static EDiscTemplateType 
s_GetDiscTemplateType(Int4 weight, Uint1 length, 
                      EDiscWordType type)
{
   if (weight == 11) {
      if (length == 16) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_16_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_16_Optimal;
      } else if (length == 18) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_18_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_18_Optimal;
      } else if (length == 21) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_21_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_21_Optimal;
      }
   } else if (weight == 12) {
      if (length == 16) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_16_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_16_Optimal;
      } else if (length == 18) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_18_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_18_Optimal;
      } else if (length == 21) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_21_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_21_Optimal;
      }
   }
   return eDiscTemplateContiguous; /* All unsupported cases default to 0 */
}

/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the discontiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup 
 *              table structure [in|out]
 * @param lookup_options specifies the word_size and template options [in]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillDiscMBTable(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt,
        const LookupTableOptions* lookup_options)

{
   BlastSeqLoc* loc;
   EDiscTemplateType template_type;
   EDiscTemplateType second_template_type = eDiscTemplateContiguous;
   const Boolean kTwoTemplates = 
      (lookup_options->mb_template_type == eMBWordTwoTemplates);
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_array_bts;
   Int4 index;
   Int4 template_length;
   /* The calculation of the longest chain can be cpu intensive for 
      long queries or sets of queries. So we use a helper_array to 
      keep track of this, but compress it by kCompressionFactor so 
      it stays in cache.  Hence we only end up with a conservative 
      (high) estimate for longest_chain, but this does not seem to 
      affect the overall performance of the rest of the program. */
   Uint4 longest_chain;
   Uint4* helper_array = NULL;     /* Helps to estimate longest chain. */
   Uint4* helper_array2 = NULL;    /* Helps to estimate longest chain. */
   const Int4 kCompressionFactor=2048; /* compress helper_array by this much */

   ASSERT(mb_lt);
   ASSERT(lookup_options->mb_template_length > 0);

   mb_lt->next_pos = (Int4 *)calloc(query->length + 1, sizeof(Int4));
   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                  sizeof(Uint4));
   if (mb_lt->next_pos == NULL || helper_array == NULL)
      return -1;

   template_type = s_GetDiscTemplateType(lookup_options->word_size,
                      lookup_options->mb_template_length, 
                      (EDiscWordType)lookup_options->mb_template_type);

   ASSERT(template_type != eDiscTemplateContiguous);

   mb_lt->template_type = template_type;
   mb_lt->two_templates = kTwoTemplates;
   /* For now leave only one possibility for the second template.
      Note that the intention here is to select both the coding
      and the optimal templates for one combination of word size
      and template length. */
   if (kTwoTemplates) {
      /* Use the temporaray to avoid annoying ICC warning. */
      int temp_int = template_type + 1;
      second_template_type = 
           mb_lt->second_template_type = (EDiscTemplateType) temp_int;

      mb_lt->hashtable2 = (Int4*)calloc(mb_lt->hashsize, sizeof(Int4));
      mb_lt->next_pos2 = (Int4*)calloc(query->length + 1, sizeof(Int4));
      helper_array2 = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                      sizeof(Uint4));
      if (mb_lt->hashtable2 == NULL ||
          mb_lt->next_pos2 == NULL ||
          helper_array2 == NULL)
         return -1;
   }

   mb_lt->discontiguous = TRUE;
   mb_lt->template_length = lookup_options->mb_template_length;
   template_length = lookup_options->mb_template_length;
   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   for (loc = location; loc; loc = loc->next) {
      Int4 from;
      Int4 to;
      Uint8 accum = 0;
      Int4 ecode1 = 0;
      Int4 ecode2 = 0;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;

      /* A word is added to the table after the last base 
         in the word is read in. At that point, the start 
         offset of the word is (template_length-1) positions 
         behind. This index is also incremented, because 
         lookup table indices are 1-based (offset 0 is reserved). */

      from = loc->ssr->left - (template_length - 2);
      to = loc->ssr->right - (template_length - 2);
      seq = query->sequence_start + loc->ssr->left;
      pos = seq + template_length;

      for (index = from; index <= to; index++) {
         val = *++seq;
         /* if an ambiguity is encountered, do not add
            any words that would contain it */
         if ((val & BLAST2NA_MASK) != 0) {
            accum = 0;
            pos = seq + template_length;
            continue;
         }

         /* get next base */
         accum = (accum << BITS_PER_NUC) | val;
         if (seq < pos)
            continue;

#ifdef LOOKUP_VERBOSE
         mb_lt->num_words_added++;
#endif
         /* compute the hashtable index for the first template
            and add 'index' at that position */

         ecode1 = ComputeDiscontiguousIndex(accum, template_type);
         if (mb_lt->hashtable[ecode1] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            PV_SET(pv_array, ecode1, pv_array_bts);
         }
         else {
            helper_array[ecode1/kCompressionFactor]++; 
         }
         mb_lt->next_pos[index] = mb_lt->hashtable[ecode1];
         mb_lt->hashtable[ecode1] = index;

         if (!kTwoTemplates)
            continue;

         /* repeat for the second template, if applicable */
         
         ecode2 = ComputeDiscontiguousIndex(accum, second_template_type);
         if (mb_lt->hashtable2[ecode2] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            PV_SET(pv_array, ecode2, pv_array_bts);
         }
         else {
            helper_array2[ecode2/kCompressionFactor]++; 
         }
         mb_lt->next_pos2[index] = mb_lt->hashtable2[ecode2];
         mb_lt->hashtable2[ecode2] = index;
      }
   }

   longest_chain = 2;
   for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
       longest_chain = MAX(longest_chain, helper_array[index]);
   mb_lt->longest_chain = longest_chain;
   sfree(helper_array);

   if (kTwoTemplates) {
      longest_chain = 2;
      for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
         longest_chain = MAX(longest_chain, helper_array2[index]);
      mb_lt->longest_chain += longest_chain;
      sfree(helper_array2);
   }
   return 0;
}

/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the contiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup table structure [in|out]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillContigMBTable(BLAST_SequenceBlk* query, 
        BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt) 

{
   BlastSeqLoc* loc;
   /* 12-mers (or perhaps 8-mers) are used to build the lookup table 
      and this is what kLutWordLength specifies. */
   const Int4 kLutWordLength = mb_lt->lut_word_length;
   const Int4 kLutMask = mb_lt->hashsize - 1;
   /* The user probably specified a much larger word size (like 28) 
      and this is what full_word_size is. */
   Int4 full_word_size = mb_lt->word_length;
   Int4 index;
   PV_ARRAY_TYPE *pv_array;
   Int4 pv_array_bts;
   /* The calculation of the longest chain can be cpu intensive for 
      long queries or sets of queries. So we use a helper_array to 
      keep track of this, but compress it by kCompressionFactor so 
      it stays in cache.  Hence we only end up with a conservative 
      (high) estimate for longest_chain, but this does not seem to 
      affect the overall performance of the rest of the program. */
   const Int4 kCompressionFactor=2048; /* compress helper_array by this much */
   Uint4 longest_chain;
   Uint4* helper_array;


   ASSERT(mb_lt);

   mb_lt->next_pos = (Int4 *)calloc(query->length + 1, sizeof(Int4));
   if (mb_lt->next_pos == NULL)
      return -1;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                  sizeof(Uint4));
   if (helper_array == NULL)
	return -1;

   for (loc = location; loc; loc = loc->next) {
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries.  */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - kLutWordLength;
      Int4 ecode = 0;
      Int4 last_offset;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;

     /* case of unmasked region >=  kLutWordLength but < full_word_size,
        so no hits should be generated. */
      if (full_word_size > (loc->ssr->right - loc->ssr->left + 1))
         continue; 

      seq = query->sequence_start + from;
      pos = seq + kLutWordLength;
      
      /* Also add 1 to all indices, because lookup table indices count 
         from 1. */
      from -= kLutWordLength - 2;
      last_offset = to + 2;

      for (index = from; index <= last_offset; index++) {
         val = *++seq;
         /* if an ambiguity is encountered, do not add
            any words that would contain it */
         if ((val & BLAST2NA_MASK) != 0) {
            ecode = 0;
            pos = seq + kLutWordLength;
            continue;
         }

         /* get next base */
         ecode = ((ecode << BITS_PER_NUC) & kLutMask) + val;
         if (seq < pos) 
            continue;

#ifdef LOOKUP_VERBOSE
         mb_lt->num_words_added++;
#endif
         if (mb_lt->hashtable[ecode] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            PV_SET(pv_array, ecode, pv_array_bts);
         }
         else {
            helper_array[ecode/kCompressionFactor]++; 
         }
         mb_lt->next_pos[index] = mb_lt->hashtable[ecode];
         mb_lt->hashtable[ecode] = index;
      }
   }

   longest_chain = 2;
   for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
       longest_chain = MAX(longest_chain, helper_array[index]);

   mb_lt->longest_chain = longest_chain;
   sfree(helper_array);
   return 0;
}


/* Documentation in mb_lookup.h */
Int2 BlastMBLookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable** mb_lt_ptr,
        const LookupTableOptions* lookup_options,
        Int4 approx_table_entries,
        Int4 lut_width)
{
   Int4 pv_size;
   Int2 status = 0;
   BlastMBLookupTable* mb_lt;
   const Int4 kTargetPVSize = 131072;
   const Int4 kSmallQueryCutoff = 15000;
   const Int4 kLargeQueryCutoff = 800000;
   
   *mb_lt_ptr = NULL;

   if (!location || !query) {
     /* Empty sequence location provided */
     return -1;
   }

   mb_lt = (BlastMBLookupTable*)calloc(1, sizeof(BlastMBLookupTable));
   if (mb_lt == NULL) {
	return -1;
   }
    
   ASSERT(lut_width >= 9);
   mb_lt->word_length = lookup_options->word_size;
   mb_lt->lut_word_length = lut_width;
   mb_lt->hashsize = 1 << (BITS_PER_NUC * mb_lt->lut_word_length);
   mb_lt->hashtable = (Int4*)calloc(mb_lt->hashsize, sizeof(Int4));
   if (mb_lt->hashtable == NULL) {
      BlastMBLookupTableDestruct(mb_lt);
      return -1;
   }

   /* Allocate the PV array. To fit in the external cache of 
      latter-day microprocessors, the PV array cannot have one
      bit for for every lookup table entry. Instead we choose
      a size that should fit in cache and make a single bit
      of the PV array handle multiple hashtable entries if
      necessary.

      If the query is too small or too large, the compression 
      should be higher. Small queries don't reuse the PV array,
      and large queries saturate it. In either case, cache
      is better used on something else */

   if (mb_lt->hashsize <= 8 * kTargetPVSize)
      pv_size = mb_lt->hashsize >> PV_ARRAY_BTS;
   else
      pv_size = kTargetPVSize / PV_ARRAY_BYTES;

   if(approx_table_entries <= kSmallQueryCutoff ||
      approx_table_entries >= kLargeQueryCutoff) {
         pv_size = pv_size / 2;
   }
   mb_lt->pv_array_bts = ilog2(mb_lt->hashsize / pv_size);
   mb_lt->pv_array = calloc(PV_ARRAY_BYTES, pv_size);
   if (mb_lt->pv_array == NULL) {
      BlastMBLookupTableDestruct(mb_lt);
      return -1;
   }

   if (lookup_options->mb_template_length > 0) {
        /* discontiguous megablast */
        mb_lt->scan_step = 1;
        status = s_FillDiscMBTable(query, location, mb_lt, lookup_options);
   }
   else {
        /* contiguous megablast */
        mb_lt->scan_step = mb_lt->word_length - mb_lt->lut_word_length + 1;
        status = s_FillContigMBTable(query, location, mb_lt);
   }

   if (status > 0) {
      BlastMBLookupTableDestruct(mb_lt);
      return status;
   }

   *mb_lt_ptr = mb_lt;

#ifdef LOOKUP_VERBOSE
   printf("lookup table size: %d (%d letters)\n", mb_lt->hashsize,
                                        mb_lt->lut_word_length);
   printf("words in table: %d\n", mb_lt->num_words_added);
   printf("filled entries: %d (%f%%)\n", mb_lt->num_unique_pos_added,
                        100.0 * mb_lt->num_unique_pos_added / mb_lt->hashsize);
   printf("PV array size: %d bytes (%d table entries/bit)\n",
                                 pv_size * PV_ARRAY_BYTES, 
                                 mb_lt->hashsize / (pv_size << PV_ARRAY_BTS));
   printf("longest chain: %d\n", mb_lt->longest_chain);
#endif
   return 0;
}

BlastMBLookupTable* BlastMBLookupTableDestruct(BlastMBLookupTable* mb_lt)
{
   if (!mb_lt)
      return NULL;

   sfree(mb_lt->hashtable);
   sfree(mb_lt->next_pos);
   sfree(mb_lt->hashtable2);
   sfree(mb_lt->next_pos2);
   sfree(mb_lt->pv_array);
   sfree(mb_lt);
   return mb_lt;
}
