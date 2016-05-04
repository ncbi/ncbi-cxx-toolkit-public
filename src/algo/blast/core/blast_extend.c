/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
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
 *
 */

/** @file blast_extend.c
 * Functions to initialize structures used for BLAST extension
 */

#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_options.h>

/** Allocates memory for the BLAST_DiagTable*. This function also 
 * sets many of the parametes such as diag_array_length etc.
 * @param qlen Length of the query [in]
 * @param multiple_hits Specifies whether multiple hits method is used [in]
 * @param window_size The max. distance between two hits that are extended [in]
 * @return The allocated BLAST_DiagTable structure
*/
static BLAST_DiagTable*
s_BlastDiagTableNew (Int4 qlen, Boolean multiple_hits, Int4 window_size)

{
        BLAST_DiagTable* diag_table;
        Int4 diag_array_length;

        diag_table= (BLAST_DiagTable*) calloc(1, sizeof(BLAST_DiagTable));

        if (diag_table)
        {
                diag_array_length = 1;
                /* What power of 2 is just longer than the query? */
                while (diag_array_length < (qlen+window_size))
                {
                        diag_array_length = diag_array_length << 1;
                }
                /* These are used in the word finders to shift and mask
                rather than dividing and taking the remainder. */
                diag_table->diag_array_length = diag_array_length;
                diag_table->diag_mask = diag_array_length-1;
                diag_table->multiple_hits = multiple_hits;
                diag_table->offset = window_size;
                diag_table->window = window_size;
        }
        return diag_table;
}

/** Deallocate memory for the diagonal table structure 
 * @param diag_table the object to be freed [in]
 * @return NULL
*/
static BLAST_DiagTable* 
s_BlastDiagTableFree(BLAST_DiagTable* diag_table)
{
   if (diag_table) {
      sfree(diag_table->hit_level_array);
      sfree(diag_table->hit_len_array);               
      sfree(diag_table);
   }
   return NULL;
}

/** Reset the diagonal array structure. Used when offset has wrapped around.
 * @param diag pointer to the diagonal array structure [in]
 */
static Int4 s_BlastDiagClear(BLAST_DiagTable * diag)
{
    Int4 i, n;
    DiagStruct *diag_struct_array;

    if (diag == NULL)
        return 0;

    n = diag->diag_array_length;

    diag->offset = diag->window;

    diag_struct_array = diag->hit_level_array;

    for (i = 0; i < n; i++) {
        diag_struct_array[i].flag = 0;
        diag_struct_array[i].last_hit = -diag->window;
        if (diag->hit_len_array) diag->hit_len_array[i] = 0;
    }
    return 0;
}

/* Description in blast_extend.h */
Int2 BlastExtendWordNew(Uint4 query_length,
                        const BlastInitialWordParameters * word_params,
                        Blast_ExtendWord ** ewp_ptr)
{
    Blast_ExtendWord *ewp;

    *ewp_ptr = ewp = (Blast_ExtendWord *) calloc(1, sizeof(Blast_ExtendWord));

    if (!ewp) {
        return -1;
    }

    if (word_params->container_type == eDiagHash) {
        ewp->hash_table =
            (BLAST_DiagHash *) calloc(1, sizeof(BLAST_DiagHash));

        ewp->hash_table->num_buckets = DIAGHASH_NUM_BUCKETS;
        ewp->hash_table->backbone =
            calloc(ewp->hash_table->num_buckets, sizeof(Uint4));
        ewp->hash_table->capacity = DIAGHASH_CHAIN_LENGTH;
        ewp->hash_table->chain =
            calloc(ewp->hash_table->capacity, sizeof(DiagHashCell));
        ewp->hash_table->occupancy = 1;
        ewp->hash_table->window = word_params->options->window_size;
        ewp->hash_table->offset = word_params->options->window_size;
    } else {                    /* container_type == eDiagArray */

        Boolean multiple_hits = (word_params->options->window_size > 0);
        BLAST_DiagTable *diag_table;

        ewp->diag_table = diag_table =
            s_BlastDiagTableNew(query_length, multiple_hits,
                              word_params->options->window_size);
        /* Allocate the buffer to be used for diagonal array. */

        diag_table->hit_level_array = (DiagStruct *)
            calloc(diag_table->diag_array_length, sizeof(DiagStruct));
        if (word_params->options->window_size) {
            diag_table->hit_len_array = (Uint1 *)
                 calloc(diag_table->diag_array_length, sizeof(Uint1));
        }
        if (!diag_table->hit_level_array) {
            sfree(ewp);
            return -1;
        }
    }
    *ewp_ptr = ewp;

    return 0;
}

Int2
Blast_ExtendWordExit(Blast_ExtendWord * ewp, Int4 subject_length)
{
    if (!ewp)
        return -1;

    if (ewp->diag_table) {
        if (ewp->diag_table->offset >= INT4_MAX / 4) {
            ewp->diag_table->offset = ewp->diag_table->window;
            s_BlastDiagClear(ewp->diag_table);
        } else {
            ewp->diag_table->offset += subject_length + ewp->diag_table->window;
        }
    } else if (ewp->hash_table) {
        if (ewp->hash_table->offset >= INT4_MAX / 4) {
	    ewp->hash_table->occupancy = 1;
            ewp->hash_table->offset = ewp->hash_table->window;
            memset(ewp->hash_table->backbone, 0,
                   ewp->hash_table->num_buckets * sizeof(Int4));
        } else {
            ewp->hash_table->offset += subject_length + ewp->hash_table->window;
        }
    }
    return 0;
}

/** Deallocate memory for the hash table structure.
 * @param hash_table The hash table structure to free. [in]
 * @return NULL.
 */
static BLAST_DiagHash *s_BlastDiagHashFree(BLAST_DiagHash * hash_table)
{
    if (!hash_table)
        return NULL;

    sfree(hash_table->backbone);
    sfree(hash_table->chain);
    sfree(hash_table);

    return NULL;
}

Blast_ExtendWord *BlastExtendWordFree(Blast_ExtendWord * ewp)
{

    if (ewp == NULL)
        return NULL;

    s_BlastDiagTableFree(ewp->diag_table);
    s_BlastDiagHashFree(ewp->hash_table);
    sfree(ewp);
    return NULL;
}


BlastInitHitList *BLAST_InitHitListNew(void)
{
    BlastInitHitList *init_hitlist = (BlastInitHitList *)
        calloc(1, sizeof(BlastInitHitList));

    init_hitlist->allocated = MIN_INIT_HITLIST_SIZE;

    init_hitlist->init_hsp_array = (BlastInitHSP *)
        malloc(MIN_INIT_HITLIST_SIZE * sizeof(BlastInitHSP));

    return init_hitlist;
}

void BlastInitHitListReset(BlastInitHitList * init_hitlist)
{
    Int4 index;

    for (index = 0; index < init_hitlist->total; ++index)
        sfree(init_hitlist->init_hsp_array[index].ungapped_data);
    init_hitlist->total = 0;
}


/** empty an init hitlist but do not deallocate the base structure
 * @param hi list of initial hits to clean [in][out]
 */
static void s_BlastInitHitListClean(BlastInitHitList * hi)
{
    BlastInitHitListReset(hi);
    sfree(hi->init_hsp_array);
}

void BlastInitHitListMove(BlastInitHitList * dst, 
                          BlastInitHitList * src)
{
    ASSERT(dst != 0);
    ASSERT(src != 0);
    ASSERT(!dst->do_not_reallocate);

    s_BlastInitHitListClean(dst);
    memmove((void *)dst, (const void *)src, sizeof(BlastInitHitList));
    src->total = src->allocated = 0;
    src->init_hsp_array = 0;
}

BlastInitHitList *BLAST_InitHitListFree(BlastInitHitList * init_hitlist)
{
    if (init_hitlist == NULL)
        return NULL;

   s_BlastInitHitListClean(init_hitlist);
   sfree(init_hitlist);
   return NULL;
}

/** Callback for sorting an array of initial HSP structures (not pointers to
 * structures!) by score. 
 */
static int score_compare_match(const void *v1, const void *v2)
{
    BlastInitHSP *h1, *h2;
    int result = 0;

    h1 = (BlastInitHSP *) v1;
    h2 = (BlastInitHSP *) v2;

    /* Check if ungapped_data substructures are initialized. If not, move
       those array elements to the end. In reality this should never happen. */
    if (h1->ungapped_data == NULL && h2->ungapped_data == NULL)
        return 0;
    else if (h1->ungapped_data == NULL)
        return 1;
    else if (h2->ungapped_data == NULL)
        return -1;

    if (0 == (result = BLAST_CMP(h2->ungapped_data->score,
                                 h1->ungapped_data->score)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->s_start,
                                 h2->ungapped_data->s_start)) &&
        0 == (result = BLAST_CMP(h2->ungapped_data->length,
                                 h1->ungapped_data->length)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->q_start,
                                 h2->ungapped_data->q_start))) {
        result = BLAST_CMP(h2->ungapped_data->length,
                           h1->ungapped_data->length);
    }

    return result;
}

void Blast_InitHitListSortByScore(BlastInitHitList * init_hitlist)
{
    qsort(init_hitlist->init_hsp_array, init_hitlist->total,
          sizeof(BlastInitHSP), score_compare_match);
}

Boolean Blast_InitHitListIsSortedByScore(BlastInitHitList * init_hitlist)
{
    Int4 index;
    BlastInitHSP *init_hsp_array = init_hitlist->init_hsp_array;

    for (index = 0; index < init_hitlist->total - 1; ++index) {
        if (score_compare_match(&init_hsp_array[index],
                                &init_hsp_array[index + 1]) > 0)
            return FALSE;
    }
    return TRUE;
}

Boolean BLAST_SaveInitialHit(BlastInitHitList * init_hitlist,
                             Int4 q_off, Int4 s_off,
                             BlastUngappedData * ungapped_data)
{
    BlastInitHSP *match_array;
    Int4 num, num_avail;

    num = init_hitlist->total;
    num_avail = init_hitlist->allocated;

    match_array = init_hitlist->init_hsp_array;
    if (num >= num_avail) {
        if (init_hitlist->do_not_reallocate)
            return FALSE;
        num_avail *= 2;
        match_array = (BlastInitHSP *)
            realloc(match_array, num_avail * sizeof(BlastInitHSP));
        if (!match_array) {
            init_hitlist->do_not_reallocate = TRUE;
            return FALSE;
        } else {
            init_hitlist->allocated = num_avail;
            init_hitlist->init_hsp_array = match_array;
        }
    }

    match_array[num].offsets.qs_offsets.q_off = q_off;
    match_array[num].offsets.qs_offsets.s_off = s_off;
    match_array[num].ungapped_data = ungapped_data;

    init_hitlist->total++;
    return TRUE;
}

void
BlastSaveInitHsp(BlastInitHitList * ungapped_hsps, Int4 q_start, Int4 s_start,
                 Int4 q_off, Int4 s_off, Int4 len, Int4 score)
{
    BlastUngappedData *ungapped_data = NULL;

    ungapped_data = (BlastUngappedData *) malloc(sizeof(BlastUngappedData));

    ungapped_data->q_start = q_start;
    ungapped_data->s_start = s_start;
    ungapped_data->length = len;
    ungapped_data->score = score;

    BLAST_SaveInitialHit(ungapped_hsps, q_off, s_off, ungapped_data);

    return;
}
