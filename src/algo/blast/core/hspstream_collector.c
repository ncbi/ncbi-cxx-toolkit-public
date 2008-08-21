/*  $Id$
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
 *
 * Author:  Ilya Dondoshansky
 *
 */

/** @file hspstream_collector.c
 * Default implementation of the BlastHSPStream interface to save hits from
 * a BLAST search, and subsequently return them in sorted order.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */


#include <algo/blast/core/hspstream_collector.h>
#include <algo/blast/core/blast_util.h>

/** Default hit saving stream methods */

/** Free the BlastHSPStream with its HSP list collector data structure.
 * @param hsp_stream The HSP stream to free [in]
 * @return NULL.
 */
static BlastHSPStream* 
s_BlastHSPListCollectorFree(BlastHSPStream* hsp_stream) 
{
   int index=0;
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);
   stream_data->x_lock = MT_LOCK_Delete(stream_data->x_lock);
   SBlastHitsParametersFree(stream_data->blasthit_params);
   Blast_HSPResultsFree(stream_data->results);
   for (index=0; index < stream_data->num_hsplists; index++)
   {
        stream_data->sorted_hsplists[index] =
            Blast_HSPListFree(stream_data->sorted_hsplists[index]);
   }
   sfree(stream_data->sort_by_score);
   sfree(stream_data->sorted_hsplists);
   sfree(stream_data);
   sfree(hsp_stream);
   return NULL;
}

/** callback used to sort HSP lists in order of decreasing OID
 * @param x First HSP list [in]
 * @param y Second HSP list [in]
 * @return compare result
 */           
static int s_SortHSPListByOid(const void *x, const void *y)
{   
        BlastHSPList **xx = (BlastHSPList **)x;
            BlastHSPList **yy = (BlastHSPList **)y;
                return (*yy)->oid - (*xx)->oid;
}

/** Prohibit any future writing to the HSP stream when all results are written.
 * Also perform sorting of results here to prepare them for reading.
 * @param hsp_stream The HSP stream to close [in] [out]
 */ 
static void 
s_BlastHSPListCollectorClose(BlastHSPStream* hsp_stream)
{
   Int4 i, j, k;
   Int4 num_hsplists;
   BlastHSPResults *results;

   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   if (stream_data->results == NULL || stream_data->results_sorted)
      return;

   if (stream_data->sort_by_score) {
       if (stream_data->sort_by_score->sort_on_read) {
           Blast_HSPResultsReverseSort(stream_data->results);
       } else {
           /* Reverse the order of HSP lists, because they will be returned
              starting from end, for the sake of convenience */
           Blast_HSPResultsReverseOrder(stream_data->results);
       }
       stream_data->results_sorted = TRUE;
       stream_data->x_lock = MT_LOCK_Delete(stream_data->x_lock);
       return;
   }

   results = stream_data->results;
   num_hsplists = stream_data->num_hsplists;

   /* concatenate all the HSPLists from 'results' */

   for (i = 0; i < results->num_queries; i++) {

       BlastHitList *hitlist = results->hitlist_array[i];
       if (hitlist == NULL)
           continue;

       /* grow the list if necessary */

       if (num_hsplists + hitlist->hsplist_count > 
                                stream_data->num_hsplists_alloc) {

           Int4 alloc = MAX(num_hsplists + hitlist->hsplist_count + 100,
                            2 * stream_data->num_hsplists_alloc);
           stream_data->num_hsplists_alloc = alloc;
           stream_data->sorted_hsplists = (BlastHSPList **)realloc(
                                         stream_data->sorted_hsplists,
                                         alloc * sizeof(BlastHSPList *));
       }

       for (j = k = 0; j < hitlist->hsplist_count; j++) {

           BlastHSPList *hsplist = hitlist->hsplist_array[j];
           if (hsplist == NULL)
               continue;

           hsplist->query_index = i;
           stream_data->sorted_hsplists[num_hsplists + k] = hsplist;
           k++;
       }

       hitlist->hsplist_count = 0;
       num_hsplists += k;
   }

   /* sort in order of decreasing subject OID. HSPLists will be
      read out from the end of hsplist_array later */

   stream_data->num_hsplists = num_hsplists;
   if (num_hsplists > 1) {
      qsort(stream_data->sorted_hsplists, num_hsplists, 
                    sizeof(BlastHSPList *), s_SortHSPListByOid);
   }

   stream_data->results_sorted = TRUE;
   stream_data->x_lock = MT_LOCK_Delete(stream_data->x_lock);
}

/** Read one HSP list from the results saved in an HSP list collector. Once an
 * HSP list is read from the stream, it relinquishes ownership and removes it
 * from the internal results data structure.
 * @param hsp_stream The HSP stream to read from [in]
 * @param hsp_list_out The read HSP list. [out]
 * @return Success, error, or end of reading, when nothing left to read.
 */
static int 
s_BlastHSPListCollectorRead(BlastHSPStream* hsp_stream, 
                          BlastHSPList** hsp_list_out) 
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   *hsp_list_out = NULL;
   if (!stream_data->results)
      return kBlastHSPStream_Eof;

   /* If this stream is not yet closed for writing, close it. In particular,
      this includes sorting of results. 
      NB: to lift the prohibition on write after the first read, the 
      following 2 lines should be removed, and stream closure for writing 
      should be done outside of the read function. */
   if (!stream_data->results_sorted)
      s_BlastHSPListCollectorClose(hsp_stream);

   if (stream_data->sort_by_score) {
       Int4 last_hsplist_index = -1, index = 0;
       BlastHitList* hit_list = NULL;
       BlastHSPResults* results = stream_data->results;

       /* Find index of the first query that has results. */
       for (index = stream_data->sort_by_score->first_query_index; 
            index < results->num_queries; ++index) {
          if (results->hitlist_array[index] && 
              results->hitlist_array[index]->hsplist_count > 0)
             break;
       }
       if (index >= results->num_queries)
          return kBlastHSPStream_Eof;

       stream_data->sort_by_score->first_query_index = index;

       hit_list = results->hitlist_array[index];
       last_hsplist_index = hit_list->hsplist_count - 1;

       *hsp_list_out = hit_list->hsplist_array[last_hsplist_index];
       /* Assign the query index here so the caller knows which query this HSP 
          list comes from */
       (*hsp_list_out)->query_index = index;
       /* Dequeue this HSP list by decrementing the HSPList count */
       --hit_list->hsplist_count;
       if (hit_list->hsplist_count == 0) {
          /* Advance the first query index, without checking that the next
           * query has results - that will be done on the next call. */
          ++stream_data->sort_by_score->first_query_index;
       }
   } else {
       /* return the next HSPlist out of the collection stored */

       if (!stream_data->num_hsplists)
          return kBlastHSPStream_Eof;

       *hsp_list_out = 
           stream_data->sorted_hsplists[--stream_data->num_hsplists];

   }
   return kBlastHSPStream_Success;
}

/** Write an HSP list to the collector HSP stream. The HSP stream assumes 
 * ownership of the HSP list and sets the dereferenced pointer to NULL.
 * @param hsp_stream Stream to write to. [in] [out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 * @return Success or error, if stream is already closed for writing.
 */
static int 
s_BlastHSPListCollectorWrite(BlastHSPStream* hsp_stream, 
                           BlastHSPList** hsp_list)
{
   Int2 status = 0;
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   /** Lock the mutex, if necessary */
   MT_LOCK_Do(stream_data->x_lock, eMT_Lock);

   /** Prohibit writing after reading has already started. This prohibition
    *  can be lifted later. There is no inherent problem in using read and
    *  write in any order, except that sorting would have to be done on 
    *  every read after a write. 
    */
   if (stream_data->results_sorted) {
      MT_LOCK_Do(stream_data->x_lock, eMT_Unlock);
      return kBlastHSPStream_Error;
   }

   /* For RPS BLAST saving procedure is different, because HSPs from different
      subjects are bundled in one HSP list */
   if (Blast_ProgramIsRpsBlast(stream_data->program)) {
      status = Blast_HSPResultsSaveRPSHSPList(stream_data->program, 
         stream_data->results, *hsp_list, stream_data->blasthit_params);
   } else {
      status = Blast_HSPResultsSaveHSPList(stream_data->program, stream_data->results, 
                                  *hsp_list, stream_data->blasthit_params);
   }
   if (status != 0)
   {
      MT_LOCK_Do(stream_data->x_lock, eMT_Unlock);
      return kBlastHSPStream_Error;
   }
   /* Results structure is no longer sorted, even if it was before. 
      The following assignment is only necessary if the logic to prohibit
      writing after the first read is removed. */
   stream_data->results_sorted = FALSE;

   /* Free the caller from this pointer's ownership. */
   *hsp_list = NULL;

   /** Unlock the mutex */
   MT_LOCK_Do(stream_data->x_lock, eMT_Unlock);

   return kBlastHSPStream_Success;
}

/* #define _DEBUG_VERBOSE 1 */
/** Merge two HSPStreams. The HSPs from the first stream are
 *  moved to the second stream.
 * @param squery_blk Structure controlling the merge process [in]
 * @param chunk_num Unique integer assigned to hsp_stream [in]
 * @param hsp_stream The stream to merge [in][out]
 * @param combined_hsp_stream The stream that will contain the
 *         HSPLists of the first stream [in][out]
 */
static int
s_BlastHSPListCollectorMerge(SSplitQueryBlk *squery_blk,
                             Uint4 chunk_num,
                             BlastHSPStream* hsp_stream,
                             BlastHSPStream* combined_hsp_stream)
{
    Int4 i, j, k;
   BlastHSPListCollectorData* stream1 = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);
   BlastHSPListCollectorData* stream2 = 
      (BlastHSPListCollectorData*) GetData(combined_hsp_stream);
   BlastHSPResults *results1 = stream1->results;
   BlastHSPResults *results2 = stream2->results;
   Int4 contexts_per_query = BLAST_GetNumberOfContexts(stream2->program);
#ifdef _DEBUG
   Int4 num_queries = 0, num_ctx = 0, num_ctx_offsets = 0;
   Int4 max_ctx;
#endif

   Uint4 *query_list = NULL, *offset_list = NULL, num_contexts = 0;
   Int4 *context_list = NULL;

   SplitQueryBlk_GetQueryIndicesForChunk(squery_blk, chunk_num, &query_list);
   SplitQueryBlk_GetQueryContextsForChunk(squery_blk, chunk_num, 
                                          &context_list, &num_contexts);
   SplitQueryBlk_GetContextOffsetsForChunk(squery_blk, chunk_num, &offset_list);

#if defined(_DEBUG_VERBOSE)
   fprintf(stderr, "Chunk %d\n", chunk_num);
   fprintf(stderr, "Queries : ");
   for (num_queries = 0; query_list[num_queries] != UINT4_MAX; num_queries++)
       fprintf(stderr, "%d ", query_list[num_queries]);
   fprintf(stderr, "\n");
   fprintf(stderr, "Contexts : ");
   for (num_ctx = 0; num_ctx < num_contexts; num_ctx++)
       fprintf(stderr, "%d ", context_list[num_ctx]);
   fprintf(stderr, "\n");
   fprintf(stderr, "Context starting offsets : ");
   for (num_ctx_offsets = 0; offset_list[num_ctx_offsets] != UINT4_MAX;
        num_ctx_offsets++)
       fprintf(stderr, "%d ", offset_list[num_ctx_offsets]);
   fprintf(stderr, "\n");
#elif defined(_DEBUG)
   for (num_queries = 0; query_list[num_queries] != UINT4_MAX; num_queries++) ;
   for (num_ctx = 0, max_ctx = INT4_MIN; num_ctx < num_contexts; num_ctx++) 
       max_ctx = MAX(max_ctx, context_list[num_ctx]);
   for (num_ctx_offsets = 0; offset_list[num_ctx_offsets] != UINT4_MAX;
        num_ctx_offsets++) ;
#endif

   for (i = 0; i < results1->num_queries; i++) {
       BlastHitList *hitlist = results1->hitlist_array[i];
       Int4 global_query = query_list[i];
       Int4 split_points[NUM_FRAMES];
#ifdef _DEBUG
       ASSERT(i < num_queries);
#endif

       if (hitlist == NULL) {
#if defined(_DEBUG_VERBOSE)
fprintf(stderr, "No hits to query %d\n", global_query);
#endif
           continue;
       }

       /* we will be mapping HSPs from the local context to
          their place on the unsplit concatenated query. Once
          that's done, overlapping HSPs need to get merged, and
          to do that we must know the offset within each context
          where the last chunk ended and the current chunk begins */
       for (j = 0; j < contexts_per_query; j++) {
           Int4 local_context = i * contexts_per_query + j;
           split_points[context_list[local_context] % contexts_per_query] = 
                                offset_list[local_context];
       }

#if defined(_DEBUG_VERBOSE)
       fprintf(stderr, "query %d split points: ", i);
       for (j = 0; j < contexts_per_query; j++) {
           fprintf(stderr, "%d ", split_points[j]);
       }
       fprintf(stderr, "\n");
#endif

       for (j = 0; j < hitlist->hsplist_count; j++) {
           BlastHSPList *hsplist = hitlist->hsplist_array[j];

           for (k = 0; k < hsplist->hspcnt; k++) {
               BlastHSP *hsp = hsplist->hsp_array[k];
               Int4 local_context = hsp->context;
#ifdef _DEBUG
               ASSERT(local_context <= max_ctx);
               ASSERT(local_context < num_ctx);
               ASSERT(local_context < num_ctx_offsets);
#endif

               hsp->context = context_list[local_context];
               hsp->query.offset += offset_list[local_context];
               hsp->query.end += offset_list[local_context];
               hsp->query.gapped_start += offset_list[local_context];
               hsp->query.frame = BLAST_ContextToFrame(stream2->program,
                                                       hsp->context);
           }

           hsplist->query_index = global_query;
       }

       Blast_HitListMerge(results1->hitlist_array + i,
                          results2->hitlist_array + global_query,
                          contexts_per_query, split_points,
                          SplitQueryBlk_GetChunkOverlapSize(squery_blk));
   }

   /* Sort to the canonical order, which the merge may not have done. */
   for (i = 0; i < results2->num_queries; i++) {
       BlastHitList *hitlist = results2->hitlist_array[i];
       if (hitlist == NULL)
           continue;

       for (j = 0; j < hitlist->hsplist_count; j++)
           Blast_HSPListSortByScore(hitlist->hsplist_array[j]);
   }

   stream2->results_sorted = FALSE;

#if _DEBUG_VERBOSE
   fprintf(stderr, "new results: %d queries\n", results2->num_queries);
   for (i = 0; i < results2->num_queries; i++) {
       BlastHitList *hitlist = results2->hitlist_array[i];
       if (hitlist == NULL)
           continue;

       for (j = 0; j < hitlist->hsplist_count; j++) {
           BlastHSPList *hsplist = hitlist->hsplist_array[j];
           fprintf(stderr, 
                   "query %d OID %d\n", hsplist->query_index, hsplist->oid);

           for (k = 0; k < hsplist->hspcnt; k++) {
               BlastHSP *hsp = hsplist->hsp_array[k];
               fprintf(stderr, "c %d q %d-%d s %d-%d score %d\n", hsp->context,
                      hsp->query.offset, hsp->query.end,
                      hsp->subject.offset, hsp->subject.end,
                      hsp->score);
           }
       }
   }
#endif

   sfree(query_list);
   sfree(context_list);
   sfree(offset_list);

   return kBlastHSPStream_Success;
}

/** Batch read function for this BlastHSPStream implementation.      
 * @param hsp_stream The BlastHSPStream object [in]
 * @param batch List of HSP lists for the HSPStream to return. The caller
 * acquires ownership of all HSP lists returned [out]
 * @return kBlastHSPStream_Success on success, kBlastHSPStream_Error, or
 * kBlastHSPStream_Eof on end of stream
 */
int s_BlastHSPListCollectorBatchRead(BlastHSPStream* hsp_stream,
                                     BlastHSPStreamResultBatch* batch) 
{
   Int4 i;
   Int4 num_hsplists;
   Int4 target_oid;
   BlastHSPList *hsplist;
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   batch->num_hsplists = 0;
   if (!stream_data->results)
      return kBlastHSPStream_Eof;

   /* If this stream is not yet closed for writing, close it. In particular,
      this includes sorting of results. 
      NB: to lift the prohibition on write after the first read, the 
      following 2 lines should be removed, and stream closure for writing 
      should be done outside of the read function. */
   if (!stream_data->results_sorted)
      s_BlastHSPListCollectorClose(hsp_stream);

   /* return all the HSPlists with the same subject OID as the
      last HSPList in the collection stored. We assume there is
      at most one HSPList per query sequence */

   num_hsplists = stream_data->num_hsplists;
   if (num_hsplists == 0)
      return kBlastHSPStream_Eof;

   hsplist = stream_data->sorted_hsplists[num_hsplists - 1];
   target_oid = hsplist->oid;

   for (i = 0; i < num_hsplists; i++) {
       hsplist = stream_data->sorted_hsplists[num_hsplists - 1 - i];
       if (hsplist->oid != target_oid)
           break;

       batch->hsplist_array[i] = hsplist;
   }

   stream_data->num_hsplists = num_hsplists - i;
   batch->num_hsplists = i;

   return kBlastHSPStream_Success;
}

/** Initialize function pointers and data structure in a collector HSP stream.
 * @param hsp_stream The stream to initialize [in] [out]
 * @param args Pointer to the collector data structure. [in]
 * @return Filled HSP stream.
 */
static BlastHSPStream* 
s_BlastHSPListCollectorNew(BlastHSPStream* hsp_stream, void* args) 
{
    BlastHSPStreamFunctionPointerTypes fnptr;

    fnptr.dtor = &s_BlastHSPListCollectorFree;
    SetMethod(hsp_stream, eDestructor, fnptr);
    fnptr.method = &s_BlastHSPListCollectorRead;
    SetMethod(hsp_stream, eRead, fnptr);
    fnptr.method = &s_BlastHSPListCollectorWrite;
    SetMethod(hsp_stream, eWrite, fnptr);
    fnptr.batch_read = &s_BlastHSPListCollectorBatchRead;
    SetMethod(hsp_stream, eBatchRead, fnptr);
    fnptr.mergeFn = &s_BlastHSPListCollectorMerge;
    SetMethod(hsp_stream, eMerge, fnptr);
    fnptr.closeFn = &s_BlastHSPListCollectorClose;
    SetMethod(hsp_stream, eClose, fnptr);

    SetData(hsp_stream, args);
    return hsp_stream;
}

BlastHSPStream* 
Blast_HSPListCollectorInitMT(EBlastProgramType program, 
                             SBlastHitsParameters* blasthit_params,
                             const BlastExtensionOptions* extn_opts,
                             Boolean sort_on_read,
                             Int4 num_queries, MT_LOCK lock)
{
    BlastHSPListCollectorData* stream_data = 
       (BlastHSPListCollectorData*) malloc(sizeof(BlastHSPListCollectorData));
    BlastHSPStreamNewInfo info;

    stream_data->program = program;
    stream_data->blasthit_params = blasthit_params;

    stream_data->num_hsplists = 0;
    stream_data->num_hsplists_alloc = 100;
    stream_data->sorted_hsplists = (BlastHSPList **)malloc(
                                           stream_data->num_hsplists_alloc *
                                           sizeof(BlastHSPList *));
    stream_data->results = Blast_HSPResultsNew(num_queries);

    stream_data->results_sorted = FALSE;

    /* This is needed to meet a pre-condition of the composition-based
     * statistics code */
    if ((Blast_QueryIsProtein(program) || Blast_QueryIsPssm(program)) &&
        extn_opts->compositionBasedStats != 0) {
        stream_data->sort_by_score = 
            (SSortByScoreStruct*)calloc(1, sizeof(SSortByScoreStruct));
        stream_data->sort_by_score->sort_on_read = sort_on_read;
        stream_data->sort_by_score->first_query_index = 0;
    } else {
        stream_data->sort_by_score = NULL;
    }
    stream_data->x_lock = lock;

    info.constructor = &s_BlastHSPListCollectorNew;
    info.ctor_argument = (void*)stream_data;

    return BlastHSPStreamNew(&info);
}

BlastHSPStream* 
Blast_HSPListCollectorInit(EBlastProgramType program, 
                           SBlastHitsParameters* blasthit_params,
                           const BlastExtensionOptions* extn_opts,
                           Boolean sort_on_read,
                           Int4 num_queries)
{
   return Blast_HSPListCollectorInitMT(program, blasthit_params, extn_opts,
                                       sort_on_read, num_queries, NULL);
}

