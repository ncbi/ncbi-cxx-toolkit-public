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

   /* always use megablast lookup table and word size 16 for mapping */
   if (Blast_ProgramIsMapping(lookup_options->program_number) &&
       lookup_options->word_size >= 16) {

       *lut_width = 16;
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
     stop = MAX( 0, locations->ssr->left-1);

     if (stop - start > 2)
        tail = BlastSeqLocNew(&retval, start, stop);

     while (locations)
     {
         start = locations->ssr->right+1;
         locations = locations->next;

         if (locations)
             stop = locations->ssr->left-1;
         else
             stop = length-1;

            if (retval == NULL)
               tail = BlastSeqLocNew(&retval, start, stop);
            else
               tail = BlastSeqLocNew(&tail, start, stop);
     }
     return retval;
}

/** Determine whether mask at hash is enabled from the QuerySetUpOptions */
static Boolean s_HasMaskAtHashEnabled(const QuerySetUpOptions* query_options)
{
    if ( !query_options ) {
        return FALSE;
    }
    if (SBlastFilterOptionsMaskAtHash(query_options->filtering_options)) {
        return TRUE;
    }
    if (query_options->filter_string && 
        strstr(query_options->filter_string, "m")) {
        return TRUE;
    }
    return FALSE;
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
    if (locations && 
        lookup->word_length > lookup->lut_word_length ) {
        /* because we use compressed query, we must always check masked location*/
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
                           const QuerySetUpOptions* query_options,
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
    if (locations && 
        lookup->word_length > lookup->lut_word_length && 
        s_HasMaskAtHashEnabled(query_options)) {
        lookup->masked_locations = s_SeqLocListInvert(locations, query->length);
    }
    s_BlastNaLookupFinalize(thin_backbone, lookup);
    sfree(thin_backbone);
    return 0;
}

BlastNaLookupTable *BlastNaLookupTableDestruct(BlastNaLookupTable * lookup)
{
    sfree(lookup->thick_backbone);
    sfree(lookup->overflow);
    if (lookup->masked_locations)
       lookup->masked_locations = BlastSeqLocFree(lookup->masked_locations);
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

static Int2 
s_FillPV(BLAST_SequenceBlk* query, 
        BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt,
        const LookupTableOptions* lookup_options) 

{
   BlastSeqLoc* loc;
   /* 12-mers (or perhaps 8-mers) are used to build the lookup table 
      and this is what kLutWordLength specifies. */
   const Int4 kLutWordLength = mb_lt->lut_word_length;
   const Int8 kLutMask = mb_lt->hashsize - 1;
   /* The user probably specified a much larger word size (like 28) 
      and this is what full_word_size is. */
   Int4 full_word_size = mb_lt->word_length;
   Int4 index;
   PV_ARRAY_TYPE *pv_array;
   Int4 pv_array_bts;

   ASSERT(mb_lt);

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   for (loc = location; loc; loc = loc->next) {
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries.  */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - kLutWordLength;
      Int8 ecode = 0;
      Int4 last_offset;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;
//      int counter = 1; /* collect this many adjacent words */

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

         PV_SET(pv_array, ecode, pv_array_bts);
      }
   }

   return 0;
}


/* Remove words that appear in polyA tails from the lookup table: string of As,
   string of Ts, and As and Ts with one error. */
static Int2 s_RemovePolyAWords(BlastMBLookupTable* mb_lt)
{
    Int4 word_size = mb_lt->lut_word_length;
    Int8 word;
    Int4 i, k;

    ASSERT(word_size == 16);

    /* remove As and Ts */
    mb_lt->hashtable[0] = 0;
    mb_lt->hashtable[(Int8)0xffffffff] = 0;

    /* remove As with a single error */
    for (i = 1;i < 4;i++) {
        word = i;
        for (k = 0;k < word_size;k++) {
            mb_lt->hashtable[word << (k * 2)] = 0;
        }
    }

    /* remove Ts with a single error */
    for (i = 0;i < 3;i++) {
        for (k = 0;k < word_size;k++) {
            word = ((0xffffffff ^ (3 << k*2)) | (i << k*2)) & 0xffffffff;
            mb_lt->hashtable[word] = 0;
        }
    }

    return 0;
}


#define MAX_WORD_COUNT (10)

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
        BlastMBLookupTable* mb_lt,
        const LookupTableOptions* lookup_options,
        Uint1* counts)
{
   BlastSeqLoc* loc;
   /* 12-mers (or perhaps 8-mers) are used to build the lookup table 
      and this is what kLutWordLength specifies. */
   const Int4 kLutWordLength = mb_lt->lut_word_length;
   const Int8 kLutMask = mb_lt->hashsize - 1;
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
   const Boolean kDbFilter = lookup_options->db_filter;

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

   /* if filtering by database word counts, then reset the pv array to avoid
      too many bits set for database scanning */
   if (kDbFilter) {
       memset(pv_array, 0,
              (mb_lt->hashsize >> mb_lt->pv_array_bts) * PV_ARRAY_BYTES);
   }

   for (loc = location; loc; loc = loc->next) {
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries.  */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - kLutWordLength;
      Int8 ecode = 0;
      Int4 last_offset;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;
//      int counter = 1; /* collect this many adjacent words */
      int shift = 0;
      int pos_shift = 0;
      if (lookup_options->stride > 0) {
          shift = lookup_options->stride - 1;
          pos_shift = kLutWordLength + 1;
      }


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

         /* if filtering by database word count, then do not add words
            with too many counts */
         if (kDbFilter) {
             if (!(ecode & 1)) {
                 if ((counts[ecode / 2] >> 4) >= MAX_WORD_COUNT) {
                     continue;
                 }
             }
             else {
                 if ((counts[ecode / 2] & 0xf) >= MAX_WORD_COUNT) {
                     continue;
                 }
             }
         }


	 /* collect 1 word and skip lookup_options->stride */
	 
 /*
	 if (!counter) {
	     pos = seq + lookup_options->stride;
	     counter = 1;
		 	     
	     continue;
	 }
	 if (lookup_options->stride) {
	     counter--;
	 }
 */

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

         /* skip shift words */
         index += shift;
         seq += shift;
         pos = seq + pos_shift;
      }
   }

   /* FIXME: should this be done only for spliced reads? */
   if (Blast_ProgramIsMapping(lookup_options->program_number) &&
       mb_lt->lut_word_length == 16) {
       s_RemovePolyAWords(mb_lt);
   }

   longest_chain = 2;
   for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
       longest_chain = MAX(longest_chain, helper_array[index]);

   mb_lt->longest_chain = longest_chain;
   sfree(helper_array);
   return 0;
}


/** Scan a subject sequecne and update words counters, for 16-base words with
 *  scan step of 1. The counters are 4-bit and counting is done up to 10.
 *
 * @param sequence Subject sequence [in]
 * @param mb_lt Megablast lookup table [in|out]
 * @param counts Word counters [in|out]
 */
static Int2
s_MBCountWordsInSubject_16_1(const BLAST_SequenceBlk* sequence,
                             BlastMBLookupTable* mb_lt,
                             Uint1* counts)
{
    Uint1 *s;
    Int4 i;
    Int8 mask = mb_lt->hashsize - 1;
    Int8 word, index, w;
    const Int4 kNumWords
        = sequence->length - mb_lt->lut_word_length;

    PV_ARRAY_TYPE* pv = mb_lt->pv_array;
    Int4 pv_array_bts = mb_lt->pv_array_bts;
    Int4 shift;
        
    if (!sequence || !counts || !mb_lt || !pv) {
        return -1;
    }

    ASSERT(mb_lt->lut_word_length == 16);

    /* scan the words in the sequence */
    shift = 8;
    s = sequence->sequence;
    w = (Int8)s[0] << 24 | (Int8)s[1] << 16 | (Int8)s[2] << 8 | s[3];
    for (i = 0;i < kNumWords;i++) {

        if (i % COMPRESSION_RATIO == 0) {
            shift = 8;
            w = (w << 8) | (Int8)s[i / COMPRESSION_RATIO + 4];
        }
        else {
            shift -= 2;
            ASSERT(shift > 0);
        }

        word = (w >> shift) & mask;

        /* skip words that do not appear in the query */
        if (!PV_TEST(pv, word, pv_array_bts)) {
            continue;
        }

        /* update the counter */
        index = word / 2;
        if (word & 1) {
            if ((counts[index] & 0xf) < MAX_WORD_COUNT) {
                counts[index]++;
            }
        }
        else {
            if ((counts[index] >> 4) < MAX_WORD_COUNT) {
                counts[index] += 1 << 4;
            }
        }
    }

    return 0;
}

/** Scan database sequences and count query words that appear in the database.
 *  Then reset pv_array bits that correspond to words that do not appear in
 *  in the database, or appear 10 or more times
 *
 * @param seq_src Source for subject sequences [in]
 * @param mb_lt Megablast lookuptable [in|out]
 */
static Int2
s_ScanSubjectForWordCounts(BlastSeqSrc* seq_src,
                           BlastMBLookupTable* mb_lt,
                           Uint1* counts)
{
    BlastSeqSrcIterator* itr;
    BlastSeqSrcGetSeqArg seq_arg;
    PV_ARRAY_TYPE* pv = mb_lt->pv_array;

    if (!seq_src || !pv || !counts) {
        return -1;
    }

    memset(&seq_arg, 0, sizeof(seq_arg));
    seq_arg.encoding = eBlastEncodingProtein;

    /* scan subject sequences and update the counters for each */
    BlastSeqSrcResetChunkIterator(seq_src);
    itr = BlastSeqSrcIteratorNewEx(MAX(BlastSeqSrcGetNumSeqs(seq_src)/100,1));
    while ((seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr))
           != BLAST_SEQSRC_EOF) {

        BlastSeqSrcGetSequence(seq_src, &seq_arg);
        s_MBCountWordsInSubject_16_1(seq_arg.seq, mb_lt, counts);
        BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
    }

    BlastSequenceBlkFree(seq_arg.seq);
    BlastSeqSrcIteratorFree(itr);

    return 0;
}


/* Documentation in mb_lookup.h */
Int2 BlastMBLookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable** mb_lt_ptr,
        const LookupTableOptions* lookup_options,
        const QuerySetUpOptions* query_options,
        Int4 approx_table_entries,
        Int4 lut_width,
        BlastSeqSrc* seqsrc)
{
   Int4 pv_size;
   Int2 status = 0;
   BlastMBLookupTable* mb_lt;
   const Int4 kTargetPVSize = 131072;
   const Int4 kSmallQueryCutoff = 15000;
   const Int4 kLargeQueryCutoff = 800000;
   Uint1* counts = NULL; /* array of word counts */
   
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
/*   mb_lt->skip = lookup_options->skip; */
   mb_lt->stride = lookup_options->stride > 0;
   mb_lt->lut_word_length = lut_width;
   mb_lt->hashsize = 1UL << (BITS_PER_NUC * mb_lt->lut_word_length);

   mb_lt->hashtable = (Int4*)calloc(mb_lt->hashsize, sizeof(Int4)); 
   if (mb_lt->hashtable == NULL) {
       BlastMBLookupTableDestruct(mb_lt);
       return -1;
   }

   if (location && 
       mb_lt->word_length > mb_lt->lut_word_length && 
       s_HasMaskAtHashEnabled(query_options)) {
       mb_lt->masked_locations = s_SeqLocListInvert(location, query->length);
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

   if (mb_lt->lut_word_length <= 12) {
       if (mb_lt->hashsize <= 8 * kTargetPVSize)
           pv_size = mb_lt->hashsize >> PV_ARRAY_BTS;
       else
           pv_size = kTargetPVSize / PV_ARRAY_BYTES;
   }
   else {
       /* use 8M-byte pv array for large lut word (only size 16 implemented
          currently) */
           pv_size = kTargetPVSize * 64 / PV_ARRAY_BYTES;
   }

   if(!lookup_options->db_filter &&
      (approx_table_entries <= kSmallQueryCutoff ||
       approx_table_entries >= kLargeQueryCutoff)) {
       pv_size = pv_size / 2;
   }

   mb_lt->pv_array_bts = ilog2(mb_lt->hashsize / pv_size);
   mb_lt->pv_array = calloc(PV_ARRAY_BYTES, pv_size);
   if (mb_lt->pv_array == NULL) {
       BlastMBLookupTableDestruct(mb_lt);
       return -1;
   }


   /* allocate word counters, to save memory we are using 4 bits per word */
   if (lookup_options->db_filter) {
       counts = (Uint1*)calloc(mb_lt->hashsize / 2, sizeof(Uint1));
       if (counts == NULL) {
           BlastMBLookupTableDestruct(mb_lt);
           return -1;
       }
   }  

   if (lookup_options->db_filter) {
       s_FillPV(query, location, mb_lt, lookup_options);
       s_ScanSubjectForWordCounts(seqsrc, mb_lt, counts);
   }

   if (lookup_options->mb_template_length > 0) {
        /* discontiguous megablast */
        mb_lt->scan_step = 1;
        status = s_FillDiscMBTable(query, location, mb_lt, lookup_options);
   }
   else {
        /* contiguous megablast */
        mb_lt->scan_step = mb_lt->word_length - mb_lt->lut_word_length + 1;
        status = s_FillContigMBTable(query, location, mb_lt, lookup_options,
                                     counts);

        if (status) {
            BlastMBLookupTableDestruct(mb_lt);
            return -1;
        }
   }

   if (lookup_options->db_filter && counts) {
       free(counts);
   }


   if (status > 0) {
      BlastMBLookupTableDestruct(mb_lt);
      return status;
   }

   *mb_lt_ptr = mb_lt;

#ifdef LOOKUP_VERBOSE
   printf("lookup table size: %ld (%d letters)\n", mb_lt->hashsize,
                                        mb_lt->lut_word_length);
   printf("words in table: %d\n", mb_lt->num_words_added);
   printf("filled entries: %d (%f%%)\n", mb_lt->num_unique_pos_added,
                        100.0 * mb_lt->num_unique_pos_added / mb_lt->hashsize);
   printf("PV array size: %d bytes (%ld table entries/bit)\n",
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
   if (mb_lt->masked_locations)
      mb_lt->masked_locations = BlastSeqLocFree(mb_lt->masked_locations);
   sfree(mb_lt);
   return mb_lt;
}


/* Hash function: Fowler-Noll-Vo (FNV) hash 
   http://www.isthe.com/chongo/tech/comp/fnv/index.html */
static Uint4 FNV_hash(Uint1* seq, Uint4 mask)
{
    const Uint4 fnv_prime = 16777619u;
    const Uint4 fnv_offset_basis = 2166136261u;
    Int4 i;
    Uint4 hash;
    
    hash = fnv_offset_basis;
    for (i = 0;i < 4;i++) {
        hash *= fnv_prime;
        hash ^= seq[i];
    }

    return hash & mask;
}


static Int2
s_NaHashLookupFillPV(BLAST_SequenceBlk* query, 
                     BlastSeqLoc* locations,
                     BlastNaHashLookupTable* lookup)
{
    BlastSeqLoc *loc;
    Int4 word_length;
    Int4 lut_word_length;
    PV_ARRAY_TYPE* pv = NULL;
    const Int4 pv_array_bts = lookup->pv_array_bts;

    ASSERT(lookup);

    word_length = lookup->word_length;
    lut_word_length = lookup->lut_word_length;
    pv = lookup->pv;
    ASSERT(pv);
    
    for (loc = locations; loc; loc = loc->next) {
        /* We want index to be always pointing to the start of the word.
           Since sequence pointer points to the end of the word, subtract
           word length from the loop boundaries.  */
        Int4 from = loc->ssr->left;
        Int4 to = loc->ssr->right;
        Uint4 ecode = 0;
        Uint1* pos;
        Uint1* seq;
        Uint1* end;
        Uint1 base;

        /* case of unmasked region >=  kLutWordLength but < full_word_size,
           so no hits should be generated. */
        if (word_length > (loc->ssr->right - loc->ssr->left + 1)) {
            continue; 
        }

        seq = query->sequence + from;
        pos = seq + lut_word_length - 1;
        end = query->sequence + to + 1;

        for (; seq < end; seq++) {

            base = *seq;
            /* if an ambiguity is encountered, do not add
               any words that would contain it */
            if ((base & BLAST2NA_MASK) != 0) {
                ecode = 0;
                pos = seq + lut_word_length;
                continue;
            }

            /* get next base */
            ecode = (ecode << BITS_PER_NUC) | base;
            if (seq < pos) {
                continue;
            }

            PV_SET(pv, (Int8)ecode, pv_array_bts);
        }
    }
        
    return 0;
}


/** Scan a subject sequecne and update words counters, for 16-base words with
 *  scan step of 1. The counters are 4-bit and counting is done up to 10.
 *
 * @param sequence Subject sequence [in]
 * @param lookup Hashed lookup table [in|out]
 * @param counts Word counters [in|out]
 */
static Int2
s_NaHashLookupCountWordsInSubject_16_1(const BLAST_SequenceBlk* sequence,
                                       BlastNaHashLookupTable* lookup,
                                       Uint1* counts)
{
    Uint1 *s;
    Int4 i;
    Int8 mask = (1UL << (16 * BITS_PER_NUC)) - 1;
    Int8 word, index, w;
    const Int4 kNumWords
        = sequence->length - lookup->lut_word_length;

    PV_ARRAY_TYPE* pv = lookup->pv;
    Int4 pv_array_bts = lookup->pv_array_bts;
    Int4 shift;

    if (!sequence || !counts || !lookup || !pv) {
        return -1;
    }

    ASSERT(lookup->lut_word_length == 16);

    /* scan the words in the sequence */
    shift = 8;
    s = sequence->sequence;
    w = (Int8)s[0] << 24 | (Int8)s[1] << 16 | (Int8)s[2] << 8 | s[3];
    for (i = 0;i < kNumWords;i++) {

        if (i % COMPRESSION_RATIO == 0) {
            shift = 8;
            w = (w << 8) | (Int8)s[i / COMPRESSION_RATIO + 4];
        }
        else {
            shift -= 2;
            ASSERT(shift > 0);
        }

        word = (w >> shift) & mask;

        /* skip words that do not appear in the query */
        if (!PV_TEST(pv, word, pv_array_bts)) {
            continue;
        }

        /* update the counter */
        index = word / 2;
        if (word & 1) {
            if ((counts[index] & 0xf) < MAX_WORD_COUNT) {
                counts[index]++;
            }
        }
        else {
            if ((counts[index] >> 4) < MAX_WORD_COUNT) {
                counts[index] += 1 << 4;
            }
        }
    }

    return 0;
}

/** Scan database sequences and count query words that appear in the database.
 *  Then reset pv_array bits that correspond to words that do not appear in
 *  in the database, or appear 10 or more times
 *
 * @param seq_src Source for subject sequences [in]
 * @param lookup Hashed lookuptable [in|out]
 */
static Int2
s_NaHashLookupScanSubjectForWordCounts(BlastSeqSrc* seq_src,
                                       BlastNaHashLookupTable* lookup,
                                       Uint1* counts)
{
    BlastSeqSrcIterator* itr;
    BlastSeqSrcGetSeqArg seq_arg;

    if (!seq_src || !lookup || !lookup->pv || !counts) {
        return -1;
    }

    memset(&seq_arg, 0, sizeof(seq_arg));
    seq_arg.encoding = eBlastEncodingProtein;

    /* scan subject sequences and update the counters for each */
    BlastSeqSrcResetChunkIterator(seq_src);
    itr = BlastSeqSrcIteratorNewEx(MAX(BlastSeqSrcGetNumSeqs(seq_src)/100,1));
    while ((seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr))
           != BLAST_SEQSRC_EOF) {

        BlastSeqSrcGetSequence(seq_src, &seq_arg);
        s_NaHashLookupCountWordsInSubject_16_1(seq_arg.seq, lookup, counts);
        BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
    }

    BlastSequenceBlkFree(seq_arg.seq);
    BlastSeqSrcIteratorFree(itr);

    return 0;
}


static void s_NaHashLookupRemoveWordFromThinBackbone(
                                         BackboneCell** thin_backbone,
                                         Uint4 word,
                                         TNaLookupHashFunction hash_func,
                                         Uint4 mask)
{
    BackboneCell* prev = NULL;
    BackboneCell* next = NULL;
    BackboneCell* b = NULL;
    Int8 index = hash_func((Uint1*)&word, mask);

    if (!thin_backbone[index]) {
        return;
    }

    /* if word present in the first enrty for the hashed value */
    if (thin_backbone[index]->word == word) {
        next = thin_backbone[index]->next;
        thin_backbone[index]->next = NULL;
        BackboneCellFree(thin_backbone[index]);
        thin_backbone[index] = next;

        return;
    }

    /* in case of a collision check the remaining words with the same hash
       value */
    prev = thin_backbone[index];
    b = thin_backbone[index]->next;
    for (; b; prev = prev->next, b = b->next) {
        if (b->word == word) {
            next = b->next;
            b->next = NULL;
            BackboneCellFree(b);
            prev->next = next;

            break;
        }
    }
}


static Int2 s_NaHashLookupRemovePolyAWords(BackboneCell** thin_backbone,
                                           Int4 size,
                                           TNaLookupHashFunction hash_func,
                                           Uint4 mask)
{
    Int4 word_size = 16;
    Int8 word;
    Int4 i, k;

    ASSERT(word_size == 16);

    /* remove As and Ts */
    s_NaHashLookupRemoveWordFromThinBackbone(thin_backbone, 0, hash_func, mask);
    s_NaHashLookupRemoveWordFromThinBackbone(thin_backbone, 0xffffffff,
                                             hash_func, mask);

    /* remove As with a single error */
    for (i = 1;i < 4;i++) {
        word = i;
        for (k = 0;k < word_size;k++) {
            s_NaHashLookupRemoveWordFromThinBackbone(thin_backbone,
                                                     word << (k * 2),
                                                     hash_func,
                                                     mask);
        }
    }

    /* remove Ts with a single error */
    for (i = 0;i < 3;i++) {
        for (k = 0;k < word_size;k++) {
            word = ((0xffffffff ^ (3 << k*2)) | (i << k*2)) & 0xffffffff;
            s_NaHashLookupRemoveWordFromThinBackbone(thin_backbone, word,
                                                     hash_func, mask);
        }
    }

    return 0;
}


/** Pack the data structures comprising a nucleotide lookup table
 * into their final form
 * @param thin_backbone structure containing indexed query offsets [in][out]
 * @param lookup the lookup table [in]
 */
static void s_BlastNaHashLookupFinalize(BackboneCell** thin_backbone,
                                        BlastNaHashLookupTable* lookup)
{
    Int4 i;
    Int4 overflow_cells_needed = 0;
    Int4 overflow_cursor = 0;
    Int4 longest_chain = 0;
    PV_ARRAY_TYPE *pv;
    const Int4 pv_array_bts = lookup->pv_array_bts;
#ifdef LOOKUP_VERBOSE
    Int4 backbone_occupancy = 0;
    Int4 thick_backbone_occupancy = 0;
    Int4 num_overflows = 0;
    Int4 words_per_hash[5] = {0,};
#endif

    ASSERT(lookup->lut_word_length == 16);

    /* allocate the new lookup table */
    lookup->thick_backbone = (NaHashLookupBackboneCell *)calloc(
                                           lookup->backbone_size, 
                                           sizeof(NaHashLookupBackboneCell));
    ASSERT(lookup->thick_backbone != NULL);

    pv = lookup->pv;
    ASSERT(pv != NULL);
    /* reset PV array, it might have been set earlier to count database words,
       and a few bits may need to be reset */
    memset(pv, 0, (lookup->backbone_size >> pv_array_bts) * PV_ARRAY_BYTES);

    /* remove polyA words from the lookup table */
    s_NaHashLookupRemovePolyAWords(thin_backbone, lookup->backbone_size,
                                   lookup->hash_callback, lookup->mask);

    /* find out how many cells are needed for the overflow array */
    for (i = 0; i < lookup->backbone_size; i++) {
        BackboneCell* b = thin_backbone[i];
        Int4 num_hits = 0;
        Int4 num_words = 0;
        for (; b; b = b->next) {
            num_hits += b->num_offsets;
            num_words++;
        }

        if (num_words > NA_WORDS_PER_HASH || num_hits > NA_OFFSETS_PER_HASH) {
            /* +1 because we store unhashed word to resolve hash collisions
               +1 for number of offsets */
            overflow_cells_needed += num_hits + (num_words * 2);
        }
        longest_chain = MAX(longest_chain, num_hits);
    }

    lookup->longest_chain = longest_chain;

    /* allocate the overflow array */
    if (overflow_cells_needed > 0) {
        lookup->overflow = (Int4*)calloc(overflow_cells_needed, sizeof(Uint4));
        ASSERT(lookup->overflow != NULL);
    }

    /* for each position in the lookup table backbone, */
    for (i = 0; i < lookup->backbone_size; i++) {

        Int4 num_words = 0;
        Int4 num_offsets = 0;
        NaHashLookupBackboneCell* cell = lookup->thick_backbone + i;
        BackboneCell* head = thin_backbone[i];
        BackboneCell* b = NULL;
        Boolean is_overflow = FALSE;
        
        if (!head) {
            continue;
        }

#ifdef LOOKUP_VERBOSE
        thick_backbone_occupancy++;
#endif

        /* for each cell with the same hash value in the thin backbone
           count number of words and offsets stored */
        for (b = head; b; b = b->next) {
            num_words++;
            num_offsets += b->num_offsets;

#ifdef LOOKUP_VERBOSE
            backbone_occupancy++;
#endif
        }
        cell->num_words = num_words;

#ifdef LOOKUP_VERBOSE
        words_per_hash[((num_words < 6) ? num_words : 5) - 1]++;
#endif

        /* if the thin cell stores at most 3 words and 9 offsets, store them
           all in the thick backbone */
        if (num_words <= NA_WORDS_PER_HASH &&
            num_offsets <= NA_OFFSETS_PER_HASH) {
            Int4 k = 0;
            Int4 n = 0;
            
            for (b = head; b; b = b->next, k++) {
                Int4 j;
                cell->words[k] = b->word;
                cell->num_offsets[k] = b->num_offsets;

                PV_SET(pv, (Int8)b->word, pv_array_bts);

                for (j = 0;j < b->num_offsets;j++) {
                    ASSERT(n <= NA_OFFSETS_PER_HASH);
                    cell->offsets[n++] = b->offsets[j];
                }
            }
        }
        /* otherwise, store them in the overflow array */
        else if (num_words <= NA_WORDS_PER_HASH) {
            Int4 k = 0;
            for (b = head; b; b = b->next, k++) {
                cell->words[k] = b->word;
            }
            is_overflow = TRUE;
        }
        else {
            is_overflow = TRUE;
        }

        /* add words and offsets to overflow array: word, number of offsets,
           offsets */
        if (is_overflow) {
#ifdef LOOKUP_VERBOSE
            num_overflows++;
#endif
            cell->offsets[0] = overflow_cursor;
            for (b = head; b; b = b->next) {
                Int4 j;
                lookup->overflow[overflow_cursor++] = *(Int4*)(&b->word);
                lookup->overflow[overflow_cursor++] = b->num_offsets;
                for (j = 0;j < b->num_offsets;j++) {
                    lookup->overflow[overflow_cursor++] = b->offsets[j];
                }
                ASSERT(overflow_cursor <= overflow_cells_needed);
                PV_SET(pv, (Int8)b->word, pv_array_bts);
            }
        }

        /* done with this chain */
        thin_backbone[i] = BackboneCellFree(thin_backbone[i]);
    }

    lookup->offsets_size = overflow_cursor;

#ifdef LOOKUP_VERBOSE
    printf("backbone size: %d\n", lookup->backbone_size);
    printf("backbone occupancy: %d (%f%%)\n", backbone_occupancy,
           100.0 * backbone_occupancy / lookup->backbone_size);
    printf("thick_backbone occupancy: %d (%f%%)\n",
           thick_backbone_occupancy,
           100.0 * thick_backbone_occupancy / lookup->backbone_size);
    printf("num_overflows: %d\n", num_overflows);
    printf("\tnumber of words per hash\tcount\n");
    {
        Int4 ii;
        for (ii = 0;ii < 5;ii++) {
            printf("\t%d\t%d\n", ii + 1, words_per_hash[ii]);
        }
    }
    printf("overflow size: %d\n", overflow_cells_needed);
    printf("longest chain: %d\n", longest_chain);
#endif
}


BlastNaHashLookupTable*
BlastNaHashLookupTableDestruct(BlastNaHashLookupTable* lookup)
{
    sfree(lookup->thick_backbone);
    sfree(lookup->overflow);
    if (lookup->masked_locations)
       lookup->masked_locations = BlastSeqLocFree(lookup->masked_locations);
    sfree(lookup->pv);
    sfree(lookup);

    return NULL;
}


Int4 BlastNaHashLookupTableNew(BLAST_SequenceBlk* query,
                               BlastSeqLoc* locations,
                               BlastNaHashLookupTable** lut,
                               const LookupTableOptions* opt, 
                               const QuerySetUpOptions* query_options,
                               BlastSeqSrc* seqsrc)
{
    BackboneCell **thin_backbone = NULL;
    BlastNaHashLookupTable *lookup = *lut =
        (BlastNaHashLookupTable*) calloc(1, sizeof(BlastNaHashLookupTable));
    /* Number of possible 16-base words */
    const Int8 kNumWords = (1UL << 32);
    Uint1* counts = NULL;
    Int4 num_hash_bits = 24;
    Int8 database_length;

    ASSERT(lookup != NULL);

    /* use more bits for hash function for larger databases to decrease number
       of collisions */
    /* FIXME: number of bits may also depend on concatenated query length */
    database_length = BlastSeqSrcGetTotLen(seqsrc);
    if (database_length > 500000000L) {
        num_hash_bits = 25;
    }

    lookup->word_length = opt->word_size;
    lookup->lut_word_length = 16;
    /* 16-base words are hashed to 24-bit or 25-bit values */
    lookup->backbone_size = 1 << num_hash_bits;
    lookup->mask = lookup->backbone_size - 1;
    lookup->overflow = NULL;
    lookup->scan_step = lookup->word_length - lookup->lut_word_length + 1;
    lookup->hash_callback = FNV_hash;

    thin_backbone = (BackboneCell**)calloc(lookup->backbone_size,
                                          sizeof(BackboneCell*));
    ASSERT(thin_backbone != NULL);

    /* PV array does not use hashing, and uses 64 words per bit */
    lookup->pv_array_bts = 11;
    lookup->pv = (PV_ARRAY_TYPE*)calloc(kNumWords / 64 / PV_ARRAY_BYTES,
                                        sizeof(PV_ARRAY_TYPE));
    ASSERT(lookup->pv);

    
    /* allocate word counters, to save memory we are using 4 bits per word */
    if (opt->db_filter) {
        ASSERT(lookup->word_length == 16);
        counts = (Uint1*)calloc(kNumWords / 2, sizeof(Uint1));
        if (counts == NULL) {
            BlastNaHashLookupTableDestruct(lookup);
            return -1;
        }
    }  

    /* count words in the database */
    if (opt->db_filter) {
        s_NaHashLookupFillPV(query, locations, lookup);
        s_NaHashLookupScanSubjectForWordCounts(seqsrc, lookup, counts);
    }

    
    BlastHashLookupIndexQueryExactMatches(thin_backbone,
                                          lookup->word_length,
                                          BITS_PER_NUC,
                                          lookup->lut_word_length,
                                          query, locations,
                                          lookup->hash_callback,
                                          lookup->mask,
                                          counts);
    if (locations && 
        lookup->word_length > lookup->lut_word_length && 
        s_HasMaskAtHashEnabled(query_options)) {
        lookup->masked_locations = s_SeqLocListInvert(locations, query->length);
    }
    s_BlastNaHashLookupFinalize(thin_backbone, lookup);
    sfree(thin_backbone);
    if (counts) {
        sfree(counts);
    }

    return 0;
}

