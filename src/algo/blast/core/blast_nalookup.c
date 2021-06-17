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
       lookup_options->word_size >= 16 && lookup_options->db_filter) {

       *lut_width = 16;
       return eNaHashLookupTable;
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

    /* remove As and Ts */
    mb_lt->hashtable[0] = 0;
    mb_lt->hashtable[(Int8)((1 << (2 * word_size)) - 1)] = 0;

    if (word_size < 16) {
        return 0;
    }

    ASSERT(word_size == 16);

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
      Uint1 max_word_count = lookup_options->max_db_word_count;
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
                 if ((counts[ecode / 2] >> 4) >= max_word_count) {
                     continue;
                 }
             }
             else {
                 if ((counts[ecode / 2] & 0xf) >= max_word_count) {
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

   if (Blast_ProgramIsMapping(lookup_options->program_number)) {
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
                             Uint1* counts,
                             Uint1 max_word_count)
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
            if ((counts[index] & 0xf) < max_word_count) {
                counts[index]++;
            }
        }
        else {
            if ((counts[index] >> 4) < max_word_count) {
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
                           Uint1* counts,
                           Uint1 max_word_count)
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
        s_MBCountWordsInSubject_16_1(seq_arg.seq, mb_lt, counts,
                                     max_word_count);
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
   mb_lt->hashsize = 1ULL << (BITS_PER_NUC * mb_lt->lut_word_length);

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
       s_ScanSubjectForWordCounts(seqsrc, mb_lt, counts,
                                  lookup_options->max_db_word_count);
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

/* Get number of set bits (adapted 	 
   from http://graphics.stanford.edu/~seander/bithacks.html) 	 
   @param v Bit vector [in] 	 
   @return Number of set bits 	 
*/ 	 
static Uint4 s_Popcount(Uint4 v) 	 
{ 	 
    if (v==0) return 0; // early bailout for sparse vectors 	 
    v = v - ((v >> 1) & 0x55555555); 	 
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333); 	 
    v = ((v + (v >> 4)) & 0xF0F0F0F); 	 
    v = v * 0x1010101; 	 
	  	 
    return v >> 24; // count 	 
} 	 

/** Sparse array of Uint1 implemented with a bitfield. The implementation
    assumes that indices present are known beforehand and the array is used
    only to access values with certain indices */
typedef struct BlastSparseUint1Array
{
    Uint4* bitfield;       /**< bitfield with bits set for present indices */
    Uint1* values;         /**< array of values for present indices */
    Int4* counts;          /**< cumulative number of bits set */
    Uint4 num_elements;    /**< number of values present in the array */
    Uint4 length;          /**< length of the bitfield */
} BlastSparseUint1Array;


static BlastSparseUint1Array*
BlastSparseUint1ArrayFree(BlastSparseUint1Array* array)
{
    if (!array) {
        return NULL;
    }

    if (array->values) {
        free(array->values);
    }

    if (array->counts) {
        free(array->counts);
    }

    free(array);

    return NULL;
}

static BlastSparseUint1Array*
BlastSparseUint1ArrayNew(Uint4* bitfield, Int8 len)
{
    Int4 i;
    BlastSparseUint1Array* retval = calloc(1, sizeof(BlastSparseUint1Array));
    
    if (!retval || !bitfield) {
        return NULL;
    }

    retval->bitfield = bitfield;
    retval->length = len >> PV_ARRAY_BTS;
    retval->counts = calloc(retval->length, sizeof(Int4));
    if (!retval->counts) {
        BlastSparseUint1ArrayFree(retval);
        return NULL;
    }

    retval->counts[0] = s_Popcount(retval->bitfield[0]);
    for (i = 1;i < retval->length;i++) {
        retval->counts[i] = retval->counts[i - 1] +
            s_Popcount(retval->bitfield[i]);
    }

    Int4 num_elements = retval->counts[retval->length - 1];
    retval->num_elements = num_elements;
    retval->values = calloc(num_elements, sizeof(Uint1));
    if (!retval->values) {
        BlastSparseUint1ArrayFree(retval);
        return NULL;
    }

    return retval;
}

/* Get index into array->values for a given vector index */
static Int4
BlastSparseUint1ArrayGetIndex(BlastSparseUint1Array* array, Int8 index)
{
    /* index into bitfield */
    Int4 idx = index >> PV_ARRAY_BTS;

    /* bit number within a bitfield cell (mod 32) */
    Int4 bit_number = index & PV_ARRAY_MASK;

    /* number of bits set before a specified bit */
    Int4 bit_count = 0;

    if (!array || idx >= array->length) {
        return -1;
    }

    /* get number of bits set up to idx */
    bit_count = (idx > 0) ? array->counts[idx - 1] : 0;
    ASSERT(array->bitfield[idx] & (1 << bit_number));

    /* add number of bits set up to bit number in the cell */
    bit_count += s_Popcount(array->bitfield[idx] & ((1 << bit_number) - 1));
    bit_count++;


    ASSERT(bit_count > 0);
    return bit_count - 1;
}

/* Get a pointer to a non zero element in the  sparse vector */
static Uint1*
BlastSparseUint1ArrayGetElement(BlastSparseUint1Array* array, Int8 index)
{
    Int4 sparse_index;

    if (!array) {
        return NULL;
    }

    sparse_index = BlastSparseUint1ArrayGetIndex(array, index);
    ASSERT(sparse_index < array->num_elements);
    if (sparse_index < 0 || sparse_index > array->num_elements) {
        return NULL;
    }

    return array->values + sparse_index;
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
                                       BlastSparseUint1Array* counts,
                                       Uint1 max_word_count)
{
    Uint1 *s;
    Int4 i;
    Int8 mask = (1ULL << (16 * BITS_PER_NUC)) - 1;
    Int8 word, w;
    const Int4 kNumWords
        = sequence->length - lookup->lut_word_length;

    PV_ARRAY_TYPE* pv = lookup->pv;
    Int4 pv_array_bts = lookup->pv_array_bts;
    Int4 shift;
    Uint1* pelem;

    if (!sequence || !counts || !lookup || !pv) {
        return -1;
    }

    ASSERT(lookup->lut_word_length == 16);
    if (sequence->length < lookup->lut_word_length) {
      return -1;
    }

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
        pelem = BlastSparseUint1ArrayGetElement(counts, word);
        if (*pelem < max_word_count) {
            (*pelem)++;
        }
    }

    return 0;
}


/* Thread local data for database word counting phase of lookup table
   generation (for Magic-BLAST) */
typedef struct NaHashLookupThreadData
{
    BlastSeqSrcGetSeqArg* seq_arg;
    BlastSeqSrcIterator** itr;
    BlastSeqSrc** seq_src;
    BlastSparseUint1Array** word_counts;
    Int4 num_threads;
} NaHashLookupThreadData;


static NaHashLookupThreadData* NaHashLookupThreadDataFree(
                                                  NaHashLookupThreadData* th)
{
    if (!th) {
        return NULL;
    }

    if (th->seq_arg) {
        Int4 i;
        for (i = 0;i < th->num_threads;i++) {
            BlastSequenceBlkFree(th->seq_arg[i].seq);
        }
        free(th->seq_arg);
    }

    if (th->itr) {
        Int4 i;
        for (i = 0;i < th->num_threads;i++) {
            BlastSeqSrcIteratorFree(th->itr[i]);
        }
        free(th->itr);
    }

    if (th->seq_src) {
        Int4 i;
        for (i = 0;i < th->num_threads;i++) {
            BlastSeqSrcFree(th->seq_src[i]);
        }
        free(th->seq_src);
    }

    if (th->word_counts) {
        Int4 i;
        for (i = 1;i < th->num_threads;i++) {
            if (th->word_counts[i]) {
                if (th->word_counts[i]->values) {
                    free(th->word_counts[i]->values);
                }
                free(th->word_counts[i]);
            }


        }
        BlastSparseUint1ArrayFree(th->word_counts[0]);
        free(th->word_counts);
    }

    free(th);

    return NULL;
}


static NaHashLookupThreadData* NaHashLookupThreadDataNew(Int4 num_threads,
                                               BlastNaHashLookupTable* lookup,
                                               BlastSeqSrc* seq_src)
{
    Int4 i;

    if (num_threads < 1 || !lookup || !seq_src) {
        return NULL;
    }

    NaHashLookupThreadData* retval = calloc(1, sizeof(NaHashLookupThreadData));
    if (!retval) {
        return NULL;
    }

    retval->seq_arg = calloc(num_threads, sizeof(BlastSeqSrcGetSeqArg));
    if (!retval->seq_arg) {
        NaHashLookupThreadDataFree(retval);
        return NULL;
    }

    retval->itr = calloc(num_threads, sizeof(BlastSeqSrcIterator*));
    if (!retval->itr) {
        NaHashLookupThreadDataFree(retval);
        return NULL;
    }

    retval->seq_src = calloc(num_threads, sizeof(BlastSeqSrc*));
    if (!retval->seq_src) {
        NaHashLookupThreadDataFree(retval);
        return NULL;
    }

    retval->word_counts = calloc(num_threads, sizeof(BlastSparseUint1Array*));
    if (!retval->word_counts) {
        NaHashLookupThreadDataFree(retval);
        return NULL;
    }

    for (i = 0;i < num_threads;i++) {
            
        retval->seq_arg[i].encoding = eBlastEncodingProtein;

        retval->seq_src[i] = BlastSeqSrcCopy(seq_src);
        if (!retval->seq_src[i]) {
            NaHashLookupThreadDataFree(retval);
            return NULL;
        }

        /* each thread must have its own iterator, the small batch seems to
           work better for work balansing between threads */
        retval->itr[i] = BlastSeqSrcIteratorNewEx(1);
        if (!retval->itr[i]) {
            NaHashLookupThreadDataFree(retval);
            return NULL;
        }

        if (i == 0) {
            retval->word_counts[i] = BlastSparseUint1ArrayNew(lookup->pv,
                                        1LL << (2 * lookup->lut_word_length));

            if (!retval->word_counts[i]) {
                NaHashLookupThreadDataFree(retval);
                return NULL;
            }
        }
        else {
            /* Make shallow copies of the counts array. We do not copy data
               that are read only to save memory. */
            retval->word_counts[i] = malloc(sizeof(BlastSparseUint1Array));
            if (!retval->word_counts[i]) {
                NaHashLookupThreadDataFree(retval);
                return NULL;
            }
            memcpy(retval->word_counts[i], retval->word_counts[0],
                   sizeof(BlastSparseUint1Array));

            retval->word_counts[i]->values = calloc(
                                        retval->word_counts[i]->num_elements,
                                        sizeof(Uint1));

            if (!retval->word_counts[i]->values) {
                NaHashLookupThreadDataFree(retval);
                return NULL;
            }
        }
    }

    retval->num_threads = num_threads;

    return retval;
}

/** Scan database sequences and count query words that appear in the database.
 *  Then reset pv_array bits that correspond to words that do not appear in
 *  in the database, or appear 10 or more times
 *
 * @param seq_src Source for subject sequences [in]
 * @param lookup Hashed lookuptable [in|out]
 * @param num_threads Number of threads to use [in]
 */
static Int2
s_NaHashLookupScanSubjectForWordCounts(BlastSeqSrc* seq_src,
                                       BlastNaHashLookupTable* lookup,
                                       Uint4 in_num_threads,
                                       Uint1 max_word_count)
{
    Int8 i;
    Int4 k, b;
    Int4 num_db_seqs, th_batch;
    NaHashLookupThreadData* th_data = NULL;
    Uint4 num_threads;

    if (!seq_src || !lookup || !lookup->pv) {
        return -1;
    }

    ASSERT(lookup->lut_word_length == 16);

    /* pv array must be one bit per word */
    ASSERT(lookup->pv_array_bts == 5);

    num_db_seqs = BlastSeqSrcGetNumSeqs(seq_src);
    num_threads = MIN(in_num_threads, num_db_seqs);
    th_batch = BlastSeqSrcGetNumSeqs(seq_src) / num_threads;

    th_data = NaHashLookupThreadDataNew(num_threads, lookup, seq_src);
    if (!th_data) {
        return -1;
    }

    /* reset database iterator */
    BlastSeqSrcResetChunkIterator(seq_src);

    /* scan subject sequences and update the counters for each */
#pragma omp parallel for if (num_threads > 1) num_threads(num_threads) \
   default(none) shared(num_threads, th_data, lookup, \
                        th_batch, max_word_count) private(i) \
    schedule(dynamic, 1)

    for (i = 0;i < num_threads;i++) {
        Int4 j;
        for (j = 0;j < th_batch;j++) {

#pragma omp critical (get_sequence_for_word_counts)
            {
                th_data->seq_arg[i].oid = BlastSeqSrcIteratorNext(
                                                         th_data->seq_src[i],
                                                         th_data->itr[i]);

                if (th_data->seq_arg[i].oid != BLAST_SEQSRC_EOF) {
                    BlastSeqSrcGetSequence(th_data->seq_src[i],
                                           &th_data->seq_arg[i]);
                }
            }

            if (th_data->seq_arg[i].oid != BLAST_SEQSRC_EOF) {

                s_NaHashLookupCountWordsInSubject_16_1(th_data->seq_arg[i].seq,
                                                       lookup,
                                                       th_data->word_counts[i],
                                                       max_word_count);
                BlastSeqSrcReleaseSequence(th_data->seq_src[i],
                                           &th_data->seq_arg[i]);
            }
        }
    }

    /* scan the last sequences */
    while ((th_data->seq_arg[0].oid = BlastSeqSrcIteratorNext(seq_src,
                                                              th_data->itr[0]))
           != BLAST_SEQSRC_EOF) {

        BlastSeqSrcGetSequence(seq_src, &th_data->seq_arg[0]);
        s_NaHashLookupCountWordsInSubject_16_1(th_data->seq_arg[0].seq, lookup,
                                               th_data->word_counts[0],
                                               max_word_count);
        BlastSeqSrcReleaseSequence(seq_src, &th_data->seq_arg[0]);
    }

    /* aggregate counts */
    for (i = 0;i < th_data->word_counts[0]->num_elements;i++) {
        for (k = 1;k < num_threads;k++) {
            th_data->word_counts[0]->values[i] =
                MIN(th_data->word_counts[0]->values[i] +
                    th_data->word_counts[k]->values[i],
                    max_word_count);
        }
    }
    
    /* iterate over word counts and clear bits for words that appear too
       often or not at all */
    i = 0;
    b = 1;
    k = 0;
    while (i < th_data->word_counts[0]->length) {

        /* skip bit field array elements with all bits cleared */
        if (th_data->word_counts[0]->bitfield[i] == 0) {
            i++;
            b = 1;
            continue;
        }

        if (th_data->word_counts[0]->bitfield[i] & b) {
            ASSERT(k < th_data->word_counts[0]->num_elements);

            /* clear bit if word count is too low or too large */
            if (th_data->word_counts[0]->values[k] == 0 ||
                th_data->word_counts[0]->values[k] >= max_word_count) {

                th_data->word_counts[0]->bitfield[i] &= ~b;
            }
            k++;
        }
        b <<= 1;
        if (b == 0) {
            i++;
            b = 1;
        }
    }

    NaHashLookupThreadDataFree(th_data);

    return 0;
}


static Int2 s_NaHashLookupRemovePolyAWords(BlastNaHashLookupTable* lookup)
{
    Int8 word;
    Int4 word_size;
    Int4 i, k;
    PV_ARRAY_TYPE* pv = NULL;
    Int4 pv_array_bts;

    if (!lookup) {
        return -1;
    }

    ASSERT(lookup->lut_word_length == 16);

    /* a bit must represent a single word */
    ASSERT(lookup->pv_array_bts == 5);

    pv = lookup->pv;
    pv_array_bts = lookup->pv_array_bts;
    word_size = lookup->lut_word_length;

    /* remove As and Ts */
    pv[0] &= ~(PV_ARRAY_TYPE)1;
    pv[0xffffffff >> pv_array_bts] &=
        ~((PV_ARRAY_TYPE)1 << (0xffffffff & PV_ARRAY_MASK));

    /* remove As with a single error */
    for (i = 1;i < 4;i++) {
        word = i;
        for (k = 0;k < word_size;k++) {
            pv[word >> pv_array_bts] &=
                ~((PV_ARRAY_TYPE)1 << (word & PV_ARRAY_MASK));
        }
    }

    /* remove Ts with a single error */
    for (i = 0;i < 3;i++) {
        for (k = 0;k < word_size;k++) {
            word = ((0xffffffff ^ (3 << k*2)) | (i << k*2)) & 0xffffffff;

            pv[word >> pv_array_bts] &=
                ~((PV_ARRAY_TYPE)1 << (word & PV_ARRAY_MASK));
        }
    }

    return 0;
}


/** Pack the data structures comprising a nucleotide lookup table
 * into their final form
 * @param thin_backbone structure containing indexed query offsets [in][out]
 * @param lookup the lookup table [in]
 */
static void s_BlastNaHashLookupFinalize(BackboneCell* thin_backbone,
                                        Int4* offsets,
                                        BlastNaHashLookupTable* lookup)
{
    Int4 i;
    Int4 overflow_cells_needed = 0;
    Int4 overflow_cursor = 0;
    Int4 longest_chain = 0;
    PV_ARRAY_TYPE *pv;
    const Int4 pv_array_bts = lookup->pv_array_bts;
    const Int8 kNumWords = 1LL << (2 * lookup->lut_word_length);
#ifdef LOOKUP_VERBOSE
    Int4 backbone_occupancy = 0;
    Int4 thick_backbone_occupancy = 0;
    Int4 num_overflows = 0;
    Int4 words_per_hash[5] = {0,};
#endif

    ASSERT(lookup->lut_word_length == 16);

    if (!lookup->pv) {

        lookup->pv = (PV_ARRAY_TYPE*)calloc(kNumWords >> lookup->pv_array_bts,
                        sizeof(PV_ARRAY_TYPE));
        ASSERT(lookup->pv);
    }
    else {
        /* reset PV array, it might have been set earlier to count database
           words, and a few bits may need to be reset */
        memset(lookup->pv, 0, (kNumWords >> lookup->pv_array_bts) *
               sizeof(PV_ARRAY_TYPE));
    }
    pv = lookup->pv;
    ASSERT(pv != NULL);

    /* allocate the new lookup table */
    lookup->thick_backbone = (NaHashLookupBackboneCell *)calloc(
                                           lookup->backbone_size, 
                                           sizeof(NaHashLookupBackboneCell));
    ASSERT(lookup->thick_backbone != NULL);

    /* find out how many cells are needed for the overflow array */
    for (i = 0; i < lookup->backbone_size; i++) {
        BackboneCell* b = &thin_backbone[i];
        Int4 num_hits = 0;
        Int4 num_words = 0;
        if (b->num_offsets > 0) {
            for (; b; b = b->next) {
                num_hits += b->num_offsets;
                num_words++;
            }
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
        lookup->overflow = (Int4*)calloc(overflow_cells_needed, sizeof(Int4));
        ASSERT(lookup->overflow != NULL);
    }

    /* for each position in the lookup table backbone, */
    for (i = 0; i < lookup->backbone_size; i++) {

        Int4 num_words = 0;
        Int4 num_offsets = 0;
        NaHashLookupBackboneCell* cell = lookup->thick_backbone + i;
        BackboneCell* head = &thin_backbone[i];
        BackboneCell* b = NULL;
        Boolean is_overflow = FALSE;
        
        if (head->num_offsets == 0) {
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

                j = b->offset;
                while (j != 0) {
                    ASSERT(n <= NA_OFFSETS_PER_HASH);
                    /* offsets array stores 1-based offsets */
                    cell->offsets[n++] = j - 1;
                    j = offsets[j];
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

                j = b->offset;
                while (j != 0) {
                    /* offsets array stores 1-based offsets */
                    lookup->overflow[overflow_cursor++] = j - 1;
                    j = offsets[j];
                }

                ASSERT(overflow_cursor <= overflow_cells_needed);
                PV_SET(pv, (Int8)b->word, pv_array_bts);
            }
        }

        /* done with this chain */
        BackboneCellFree(thin_backbone[i].next);
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
    if (lookup->pv)
        sfree(lookup->pv);
    sfree(lookup);

    return NULL;
}


Int4 BlastNaHashLookupTableNew(BLAST_SequenceBlk* query,
                               BlastSeqLoc* locations,
                               BlastNaHashLookupTable** lut,
                               const LookupTableOptions* opt, 
                               const QuerySetUpOptions* query_options,
                               BlastSeqSrc* seqsrc,
                               Uint4 num_threads)
{
    BackboneCell *thin_backbone = NULL;
    Int4* offsets = NULL;
    BlastNaHashLookupTable *lookup = *lut =
        (BlastNaHashLookupTable*) calloc(1, sizeof(BlastNaHashLookupTable));
    /* Number of possible 16-base words */
    const Int8 kNumWords = (1ULL << 32);
    Int4 num_hash_bits = 8;
    Int4 i, num_unique_words = 0;
    
    ASSERT(lookup != NULL);

    if (opt->db_filter && !seqsrc) {
        return -1;
    }

    lookup->word_length = opt->word_size;
    lookup->lut_word_length = 16;
    lookup->overflow = NULL;
    lookup->hash_callback = FNV_hash;

    if (opt->db_filter) {
        /* with database filtering some query words are not put in the lookup
           table and neighboring query words would be missed with larger scan
           step */
        lookup->scan_step = 1;
    }
    else {
        lookup->scan_step = lookup->word_length - lookup->lut_word_length + 1;
    }

    /* PV array does not use hashing */
    lookup->pv_array_bts = PV_ARRAY_BTS;
    lookup->pv = (PV_ARRAY_TYPE*)calloc(kNumWords >> lookup->pv_array_bts,
                                        sizeof(PV_ARRAY_TYPE));
    if (!lookup->pv) {
        return BLASTERR_MEMORY;
    }

    s_NaHashLookupFillPV(query, locations, lookup);
    s_NaHashLookupRemovePolyAWords(lookup);

    /* count words in the database */
    if (opt->db_filter) {
        s_NaHashLookupScanSubjectForWordCounts(seqsrc, lookup, num_threads,
                                               opt->max_db_word_count);
    }

    /* find number of unique query words */
    for (i = 0;i < kNumWords >> lookup->pv_array_bts; i++) {
        num_unique_words += s_Popcount(lookup->pv[i]);
    }

    /* find number of bits to use for hash function */
    while (num_hash_bits < 32 &&
           (1LL << num_hash_bits) < num_unique_words) {
        num_hash_bits++;
    }

    lookup->backbone_size = 1 << num_hash_bits;
    lookup->mask = lookup->backbone_size - 1;

    thin_backbone = calloc(lookup->backbone_size, sizeof(BackboneCell));
    if (!thin_backbone) {
        return BLASTERR_MEMORY;
    }

    /* it will store 1-based offsets, hence length + 1 */
    offsets = calloc(query->length + 1, sizeof(Int4));
    if (!offsets) {
        return BLASTERR_MEMORY;
    }

    BlastHashLookupIndexQueryExactMatches(thin_backbone,
                                          offsets,
                                          lookup->word_length,
                                          BITS_PER_NUC,
                                          lookup->lut_word_length,
                                          query, locations,
                                          lookup->hash_callback,
                                          lookup->mask,
                                          lookup->pv);

    if (locations && 
        lookup->word_length > lookup->lut_word_length && 
        s_HasMaskAtHashEnabled(query_options)) {
        lookup->masked_locations = s_SeqLocListInvert(locations, query->length);
    }
    s_BlastNaHashLookupFinalize(thin_backbone, offsets, lookup);
    sfree(thin_backbone);
    sfree(offsets);

    return 0;
}

