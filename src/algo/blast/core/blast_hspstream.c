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

/** @file blast_hspstream.c
 * BlastHSPStream is used to save hits from preliminary stage and
 * pass on to the traceback stage.
 */


#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/blast_util.h>
#include "blast_hspstream_mt_utils.h"

/** Default hit saving stream methods */

/** Free the BlastHSPStream with its HSP list collector data structure.
 * @param hsp_stream The HSP stream to free [in]
 * @return NULL.
 */
BlastHSPStream* BlastHSPStreamFree(BlastHSPStream* hsp_stream)
{
   int index=0;
   BlastHSPPipe *p;

   if (!hsp_stream) {
       return NULL;
   }

   hsp_stream->x_lock = MT_LOCK_Delete(hsp_stream->x_lock);
   Blast_HSPResultsFree(hsp_stream->results);
   for (index=0; index < hsp_stream->num_hsplists; index++)
   {
        hsp_stream->sorted_hsplists[index] =
            Blast_HSPListFree(hsp_stream->sorted_hsplists[index]);
   }
   sfree(hsp_stream->sort_by_score);
   sfree(hsp_stream->sorted_hsplists);

   if (hsp_stream->writer) {
       (hsp_stream->writer->FreeFnPtr) (hsp_stream->writer);
       hsp_stream->writer = NULL;
   }

   /* free un-used pipes */
   while (hsp_stream->pre_pipe) {
       p = hsp_stream->pre_pipe;
       hsp_stream->pre_pipe = p->next;
       sfree(p);
   }
   while (hsp_stream->tback_pipe) {
       p = hsp_stream->tback_pipe;
       hsp_stream->tback_pipe = p->next;
       sfree(p);
   }

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

/** certain hspstreams (such as besthit and culling) uses its own data structure
 * and therefore must be finalized before reading/merging
 */
static void s_FinalizeWriter(BlastHSPStream* hsp_stream)
{
   BlastHSPPipe *pipe;
   if (!hsp_stream || !hsp_stream->results || hsp_stream->writer_finalized)
      return;

   /* perform post-writer clean ups */
   if (hsp_stream->writer) {
       if (!hsp_stream->writer_initialized) {
           /* some filter (e.g. hsp_queue) always needs finalization */
           (hsp_stream->writer->InitFnPtr)
                (hsp_stream->writer->data, hsp_stream->results);
       }
       (hsp_stream->writer->FinalFnPtr)
            (hsp_stream->writer->data, hsp_stream->results);
   }

   /* apply preliminary stage pipes */
   while (hsp_stream->pre_pipe) {
       pipe = hsp_stream->pre_pipe;
       hsp_stream->pre_pipe = pipe->next;
       (pipe->RunFnPtr) (pipe->data, hsp_stream->results);
       (pipe->FreeFnPtr) (pipe);
   }

   hsp_stream->writer_finalized = TRUE;
}

/** Prohibit any future writing to the HSP stream when all results are written.
 * Also perform sorting of results here to prepare them for reading.
 * @param hsp_stream The HSP stream to close [in] [out]
 */
void BlastHSPStreamClose(BlastHSPStream* hsp_stream)
{
   Int4 i, j, k;
   Int4 num_hsplists;
   BlastHSPResults *results;

   if (!hsp_stream || !hsp_stream->results || hsp_stream->results_sorted)
      return;

   s_FinalizeWriter(hsp_stream);

   if (hsp_stream->sort_by_score) {
       if (hsp_stream->sort_by_score->sort_on_read) {
           Blast_HSPResultsReverseSort(hsp_stream->results);
       } else {
           /* Reverse the order of HSP lists, because they will be returned
              starting from end, for the sake of convenience */
           Blast_HSPResultsReverseOrder(hsp_stream->results);
       }
       hsp_stream->results_sorted = TRUE;
       hsp_stream->x_lock = MT_LOCK_Delete(hsp_stream->x_lock);
       return;
   }

   results = hsp_stream->results;
   num_hsplists = hsp_stream->num_hsplists;

   /* concatenate all the HSPLists from 'results' */

   for (i = 0; i < results->num_queries; i++) {

       BlastHitList *hitlist = results->hitlist_array[i];
       if (hitlist == NULL)
           continue;

       /* grow the list if necessary */

       if (num_hsplists + hitlist->hsplist_count >
                                hsp_stream->num_hsplists_alloc) {

           Int4 alloc = MAX(num_hsplists + hitlist->hsplist_count + 100,
                            2 * hsp_stream->num_hsplists_alloc);
           hsp_stream->num_hsplists_alloc = alloc;
           hsp_stream->sorted_hsplists = (BlastHSPList **)realloc(
                                         hsp_stream->sorted_hsplists,
                                         alloc * sizeof(BlastHSPList *));
       }

       for (j = k = 0; j < hitlist->hsplist_count; j++) {

           BlastHSPList *hsplist = hitlist->hsplist_array[j];
           if (hsplist == NULL)
               continue;

           hsplist->query_index = i;
           hsp_stream->sorted_hsplists[num_hsplists + k] = hsplist;
           k++;
       }

       hitlist->hsplist_count = 0;
       num_hsplists += k;
   }

   /* sort in order of decreasing subject OID. HSPLists will be
      read out from the end of hsplist_array later */

   hsp_stream->num_hsplists = num_hsplists;
   if (num_hsplists > 1) {
      qsort(hsp_stream->sorted_hsplists, num_hsplists,
                    sizeof(BlastHSPList *), s_SortHSPListByOid);
   }

   hsp_stream->results_sorted = TRUE;
   hsp_stream->x_lock = MT_LOCK_Delete(hsp_stream->x_lock);
}

void BlastHSPStreamMappingClose(BlastHSPStream* hsp_stream,
                                BlastMappingResults* results)
{
   if (!hsp_stream || !hsp_stream->writer)
      return;

   if (!hsp_stream->writer_initialized) {
       (hsp_stream->writer->InitFnPtr)
           (hsp_stream->writer->data, hsp_stream->results);
   }
   (hsp_stream->writer->FinalFnPtr)
       (hsp_stream->writer->data, results);

   hsp_stream->writer_finalized = TRUE;
   hsp_stream->x_lock = MT_LOCK_Delete(hsp_stream->x_lock);
}

void BlastHSPStreamSimpleClose(BlastHSPStream* hsp_stream)
{
   if (!hsp_stream)
      return;

   s_FinalizeWriter(hsp_stream);

   hsp_stream->x_lock = MT_LOCK_Delete(hsp_stream->x_lock);
}

/** Closing the HSP after traceback is done.
 * This is mainly to provide a chance to apply post-traceback pipes.
 * @param hsp_stream The HSP stream to close [in] [out]
 * @param results The traceback results [in] [out]
 */
void BlastHSPStreamTBackClose(BlastHSPStream* hsp_stream,
                              BlastHSPResults* results)
{
   BlastHSPPipe *pipe;

   if (!hsp_stream || !results) {
       return;
   }

   /* apply traceback stage pipes */
   while (hsp_stream->tback_pipe) {
       pipe = hsp_stream->tback_pipe;
       hsp_stream->tback_pipe = pipe->next;
       (pipe->RunFnPtr) (pipe->data, results);
       (pipe->FreeFnPtr) (pipe);
   }
   return;
}

const int kBlastHSPStream_Error = -1;
const int kBlastHSPStream_Success = 0;
const int kBlastHSPStream_Eof = 1;

/** Read one HSP list from the results saved in an HSP list collector. Once an
 * HSP list is read from the stream, it relinquishes ownership and removes it
 * from the internal results data structure.
 * @param hsp_stream The HSP stream to read from [in]
 * @param hsp_list_out The read HSP list. [out]
 * @return Success, error, or end of reading, when nothing left to read.
 */
int BlastHSPStreamRead(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list_out)
{
   *hsp_list_out = NULL;

   if (!hsp_stream)
      return kBlastHSPStream_Error;

   if (!hsp_stream->results)
      return kBlastHSPStream_Eof;

   /* If this stream is not yet closed for writing, close it. In particular,
      this includes sorting of results.
      NB: to lift the prohibition on write after the first read, the
      following 2 lines should be removed, and stream closure for writing
      should be done outside of the read function. */
   if (!hsp_stream->results_sorted)
       BlastHSPStreamClose(hsp_stream);

   if (hsp_stream->sort_by_score) {
       Int4 last_hsplist_index = -1, index = 0;
       BlastHitList* hit_list = NULL;
       BlastHSPResults* results = hsp_stream->results;

       /* Find index of the first query that has results. */
       for (index = hsp_stream->sort_by_score->first_query_index;
            index < results->num_queries; ++index) {
          if (results->hitlist_array[index] &&
              results->hitlist_array[index]->hsplist_count > 0)
             break;
       }
       if (index >= results->num_queries)
          return kBlastHSPStream_Eof;

       hsp_stream->sort_by_score->first_query_index = index;

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
          ++hsp_stream->sort_by_score->first_query_index;
       }
   } else {
       /* return the next HSPlist out of the collection stored */

       if (!hsp_stream->num_hsplists)
          return kBlastHSPStream_Eof;

       *hsp_list_out =
           hsp_stream->sorted_hsplists[--hsp_stream->num_hsplists];

   }
   return kBlastHSPStream_Success;
}

/** Write an HSP list to the collector HSP stream. The HSP stream assumes
 * ownership of the HSP list and sets the dereferenced pointer to NULL.
 * @param hsp_stream Stream to write to. [in] [out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 * @return Success or error, if stream is already closed for writing.
 */
int BlastHSPStreamWrite(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list)
{
   Int2 status = 0;

   if (!hsp_stream)
      return kBlastHSPStream_Error;

   /** Lock the mutex, if necessary */
   MT_LOCK_Do(hsp_stream->x_lock, eMT_Lock);

   /** Prohibit writing after reading has already started. This prohibition
    *  can be lifted later. There is no inherent problem in using read and
    *  write in any order, except that sorting would have to be done on
    *  every read after a write.
    */
   if (hsp_stream->results_sorted) {
      MT_LOCK_Do(hsp_stream->x_lock, eMT_Unlock);
      return kBlastHSPStream_Error;
   }

   if (hsp_stream->writer) {
       /** if writer has not been initialized, initialize it first */
      if (!(hsp_stream->writer_initialized)) {
          (hsp_stream->writer->InitFnPtr)
                   (hsp_stream->writer->data, hsp_stream->results);
          hsp_stream->writer_initialized = TRUE;
      }

      /** filtering processing */
      status = (hsp_stream->writer->RunFnPtr)
               (hsp_stream->writer->data, *hsp_list);
   }

   if (status != 0) {
      MT_LOCK_Do(hsp_stream->x_lock, eMT_Unlock);
      return kBlastHSPStream_Error;
   }
   /* Results structure is no longer sorted, even if it was before.
      The following assignment is only necessary if the logic to prohibit
      writing after the first read is removed. */
   hsp_stream->results_sorted = FALSE;

   /* Free the caller from this pointer's ownership. */
   *hsp_list = NULL;

   /** Unlock the mutex */
   MT_LOCK_Do(hsp_stream->x_lock, eMT_Unlock);

   return kBlastHSPStream_Success;
}

/* #define _DEBUG_VERBOSE 1 */
/** Merge two HSPStreams. The HSPs from the first stream are
 *  moved to the second stream.
 * @param squery_blk Structure controlling the merge process [in]
 * @param chunk_num Unique integer assigned to hsp_stream [in]
 * @param stream1 The stream to merge [in][out]
 * @param stream2 The stream that will contain the
 *         HSPLists of the first stream [in][out]
 */
int BlastHSPStreamMerge(SSplitQueryBlk *squery_blk,
                             Uint4 chunk_num,
                             BlastHSPStream* stream1,
                             BlastHSPStream* stream2)
{
   Int4 i, j, k;
   BlastHSPResults *results1 = NULL;
   BlastHSPResults *results2 = NULL;
   Int4 contexts_per_query = 0;
#ifdef _DEBUG
   Int4 num_queries = 0, num_ctx = 0, num_ctx_offsets = 0;
   Int4 max_ctx;
#endif

   Uint4 *query_list = NULL, *offset_list = NULL, num_contexts = 0;
   Int4 *context_list = NULL;


   if (!stream1 || !stream2)
       return kBlastHSPStream_Error;

   s_FinalizeWriter(stream1);
   s_FinalizeWriter(stream2);

   results1 = stream1->results;
   results2 = stream2->results;

   contexts_per_query = BLAST_GetNumberOfContexts(stream2->program);

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
   for (num_ctx = 0, max_ctx = INT4_MIN; num_ctx < (Int4)num_contexts; num_ctx++)
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
           split_points[j] = -1;
       }

       for (j = 0; j < contexts_per_query; j++) {
           Int4 local_context = i * contexts_per_query + j;
           if (context_list[local_context] >= 0) {
               split_points[context_list[local_context] % contexts_per_query] =
                                offset_list[local_context];
           }
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
                          SplitQueryBlk_GetChunkOverlapSize(squery_blk),
                          SplitQueryBlk_AllowGap(squery_blk));
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

int BlastHSPStreamBatchRead(BlastHSPStream* hsp_stream,
                            BlastHSPStreamResultBatch* batch)
{
   Int4 i;
   Int4 num_hsplists;
   Int4 target_oid;
   BlastHSPList *hsplist;

   if (!hsp_stream || !batch)
       return kBlastHSPStream_Error;

   /* If this stream is not yet closed for writing, close it. In particular,
      this includes sorting of results.
      NB: to lift the prohibition on write after the first read, the
      following 2 lines should be removed, and stream closure for writing
      should be done outside of the read function. */
   if (!hsp_stream->results_sorted)
      BlastHSPStreamClose(hsp_stream);

   batch->num_hsplists = 0;
   if (!hsp_stream->results)
      return kBlastHSPStream_Eof;

   /* return all the HSPlists with the same subject OID as the
      last HSPList in the collection stored. We assume there is
      at most one HSPList per query sequence */

   num_hsplists = hsp_stream->num_hsplists;
   if (num_hsplists == 0)
      return kBlastHSPStream_Eof;

   hsplist = hsp_stream->sorted_hsplists[num_hsplists - 1];
   target_oid = hsplist->oid;

   for (i = 0; i < num_hsplists; i++) {
       hsplist = hsp_stream->sorted_hsplists[num_hsplists - 1 - i];
       if (hsplist->oid != target_oid)
           break;

       batch->hsplist_array[i] = hsplist;
   }

   hsp_stream->num_hsplists = num_hsplists - i;
   batch->num_hsplists = i;

   return kBlastHSPStream_Success;
}

BlastHSPStreamResultBatch *
Blast_HSPStreamResultBatchInit(Int4 num_hsplists)
{
    BlastHSPStreamResultBatch *retval = (BlastHSPStreamResultBatch *)
                             calloc(1, sizeof(BlastHSPStreamResultBatch));

    retval->hsplist_array = (BlastHSPList **)calloc((size_t)num_hsplists,
                                               sizeof(BlastHSPList *));
    return retval;
}

BlastHSPStreamResultBatch *
Blast_HSPStreamResultBatchFree(BlastHSPStreamResultBatch *batch)
{
    if (batch != NULL) {
        if (batch->hsplist_array) {
            sfree(batch->hsplist_array);
        }
        sfree(batch);
    }
    return NULL;
}

BlastHSPStreamResultBatch* Blast_HSPStreamResultBatchReset(BlastHSPStreamResultBatch *batch)
{
    Int4 i;
    if (batch != NULL) {
        for (i = 0; i < batch->num_hsplists; i++) {
            batch->hsplist_array[i] =
               Blast_HSPListFree(batch->hsplist_array[i]);
        }
        batch->num_hsplists = 0;
    }
    return batch;
}

BlastHSPStream*
BlastHSPStreamNew(EBlastProgramType program,
                  const BlastExtensionOptions* extn_opts,
                  Boolean sort_on_read,
                  Int4 num_queries,
                  BlastHSPWriter *writer)
{
    BlastHSPStream* hsp_stream =
       (BlastHSPStream*) malloc(sizeof(BlastHSPStream));

    hsp_stream->program = program;

    hsp_stream->num_hsplists = 0;
    hsp_stream->num_hsplists_alloc = 100;
    hsp_stream->sorted_hsplists = (BlastHSPList **)malloc(
                                           hsp_stream->num_hsplists_alloc *
                                           sizeof(BlastHSPList *));

    /* FIXME: This will not be needed for mapper when the new mapping
       results structure is implemented */
    hsp_stream->results = Blast_HSPResultsNew(num_queries);

    hsp_stream->results_sorted = FALSE;

    /* This is needed to meet a pre-condition of the composition-based
     * statistics code */
    if ((Blast_QueryIsProtein(program) || Blast_QueryIsPssm(program)) &&
        extn_opts->compositionBasedStats != 0) {
        hsp_stream->sort_by_score =
            (SSortByScoreStruct*)calloc(1, sizeof(SSortByScoreStruct));
        hsp_stream->sort_by_score->sort_on_read = sort_on_read;
        hsp_stream->sort_by_score->first_query_index = 0;
    } else {
        hsp_stream->sort_by_score = NULL;
    }
    hsp_stream->x_lock = NULL;
    hsp_stream->writer = writer;
    hsp_stream->writer_initialized = FALSE;
    hsp_stream->writer_finalized = FALSE;
    hsp_stream->pre_pipe = NULL;
    hsp_stream->tback_pipe = NULL;

    return hsp_stream;
}

int BlastHSPStreamRegisterMTLock(BlastHSPStream* hsp_stream,
                                 MT_LOCK lock)
{
    /* only one lock can be registered */
    if (!hsp_stream || (hsp_stream->x_lock && lock)) {
        MT_LOCK_Delete(lock);
        return -1;
    }
    hsp_stream->x_lock = lock;
    return 0;
}

int BlastHSPStreamRegisterPipe(BlastHSPStream* hsp_stream,
                               BlastHSPPipe* pipe,
                               EBlastStage stage)
{
    BlastHSPPipe *p;

    if (!hsp_stream || !pipe) {
        return -1;
    }

    pipe->next = NULL;

    switch(stage) {
    case ePrelimSearch:
        p = hsp_stream->pre_pipe;
        if (!p) {
            hsp_stream->pre_pipe = pipe;
            return 0;
        }
        break;
    case eTracebackSearch:
        p = hsp_stream->tback_pipe;
        if (!p) {
            hsp_stream->tback_pipe = pipe;
            return 0;
        }
        break;
    default:
        return -1;
    }

    /* insert the pipe at the end */
    for (; p && p->next; p = p->next);
    p->next = pipe;

    return 0;
}

BlastHSPWriter*
BlastHSPWriterNew (BlastHSPWriterInfo** writer_info,
                   BlastQueryInfo* query_info,
                   BLAST_SequenceBlk* query)
{
    BlastHSPWriter * writer = NULL;
    if(writer_info && *writer_info) {
        writer = ((*writer_info)->NewFnPtr) ((*writer_info)->params, query_info,
                                             query);
        sfree(*writer_info);
    }
    ASSERT(writer_info && *writer_info == NULL);
    return writer;
}

BlastHSPPipeInfo*
BlastHSPPipeInfo_Add(BlastHSPPipeInfo** head,
                     BlastHSPPipeInfo* node)
{
    if (head) {
        if (*head) {
            BlastHSPPipeInfo* tmp = *head;
            while (tmp->next) {
                tmp = tmp->next;
            }
            tmp->next = node;
        } else {
            *head = node;
        }
    }
    return node;
}

BlastHSPPipe*
BlastHSPPipeNew (BlastHSPPipeInfo** pipe_info,
                 BlastQueryInfo* query_info)
{
    BlastHSPPipe *pipe = NULL;
    BlastHSPPipe *p = pipe;
    BlastHSPPipeInfo *info = *pipe_info;
    BlastHSPPipeInfo *q = info;

    while(info) {
        if (p) {
            p->next = (info->NewFnPtr) (info->params, query_info);
            p = p->next;
        } else {
            pipe = (info->NewFnPtr) (info->params, query_info);
            p = pipe;
        }
        p->next = NULL;
        q = info;
        info = info->next;
        sfree(q);
    }
    *pipe_info = NULL;
    return pipe;
}

void s_TrimHitList(BlastHitList* hitlist, int count)
{
	int old_count = hitlist->hsplist_count;
	int index =0;
	for (index = count; index < old_count; index++) {
		hitlist->hsplist_array[index] = Blast_HSPListFree(hitlist->hsplist_array[index]);
	}
	hitlist->hsplist_count = count;
}

void BlastHSPCBSStreamClose(BlastHSPStream* hsp_stream, int hitlist_size)
{
   BlastHitList* hit_list;
   BlastHSPResults * results;
   int index =0;

   if (!hsp_stream || !hsp_stream->results || hsp_stream->results_sorted)
      return;

   s_FinalizeWriter(hsp_stream);

   results = hsp_stream->results;

   for (index = 0; index < results->num_queries; index++) {
      hit_list = results->hitlist_array[index];
      if (hit_list) {
    	  const int REF_HIT_NUM = MAX(500, hitlist_size);
    	  const int MIN_BUF_SZ = REF_HIT_NUM + 600;
    	  if (MIN_BUF_SZ + 100 < hit_list->hsplist_count) {
        	  int max_index = hit_list->hsplist_count -1;
        	  double best_evalue = 0, evalue_limit = 0;
        	  int mag = -180, pct = 90, i = 0;
        	  Blast_HitListSortByEvalue(hit_list);
        	  best_evalue = hit_list->hsplist_array[REF_HIT_NUM]->best_evalue;
        	  if (best_evalue != 0 ){
        		  mag = log10(hit_list->hsplist_array[REF_HIT_NUM]->best_evalue);
        	  }
        	  if (mag < -20){
        		  mag = MAX(mag*pct/100, mag +10);
        	  }
        	  else {
        		 mag = mag/2;
        	  }
        	  evalue_limit = (mag >= 0)? best_evalue*3: 9.9* pow(10, mag);

        	  i = MIN_BUF_SZ;
        	  for(; i < max_index; i +=100) {
        		  if((hit_list->hsplist_array[i]->best_evalue != 0) &&
        		     (evalue_limit < hit_list->hsplist_array[i]->best_evalue)){
        			  s_TrimHitList(hit_list, i);
        			  break;
        		  }
        	  }
          }
      }
   }

   BlastHSPStreamClose(hsp_stream);
   return;

}
