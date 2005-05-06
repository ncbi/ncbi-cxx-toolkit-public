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


#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/hspstream_collector.h>

/** Default hit saving stream methods */

/** Free the BlastHSPStream with its HSP list collector data structure.
 * @param hsp_stream The HSP stream to free [in]
 * @return NULL.
 */
static BlastHSPStream* 
s_BlastHSPListCollectorFree(BlastHSPStream* hsp_stream) 
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);
   stream_data->x_lock = MT_LOCK_Delete(stream_data->x_lock);
   Blast_HSPResultsFree(stream_data->results);
   sfree(stream_data);
   sfree(hsp_stream);
   return NULL;
}

/** Prohibit any future writing to the HSP stream when all results are written.
 * Also perform sorting of results here to prepare them for reading.
 * @param hsp_stream The HSP stream to close [in] [out]
 */ 
static void 
s_BlastHSPListCollectorClose(BlastHSPStream* hsp_stream)
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   if (stream_data->results == NULL || stream_data->results_sorted)
      return;

   if (stream_data->sort_on_read) {
      Blast_HSPResultsReverseSort(stream_data->results);
   } else {
      /* Reverse the order of HSP lists, because they will be returned
	 starting from end, for the sake of convenience. */
      Blast_HSPResultsReverseOrder(stream_data->results);
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
   Int4 last_hsplist_index = -1;
   BlastHitList* hit_list = NULL;
   BlastHSPResults* results = stream_data->results;
   Int4 index;

   *hsp_list_out = NULL;
   if (!results)
      return kBlastHSPStream_Eof;

   /* If this stream is not yet closed for writing, close it. In particular,
      this includes sorting of results. 
      NB: to lift the prohibition on write after the first read, the 
      following 2 lines should be removed, and stream closure for writing 
      should be done outside of the read function. */
   if (!stream_data->results_sorted)
      s_BlastHSPListCollectorClose(hsp_stream);

   /* Find index of the first query that has results. */
   for (index = stream_data->first_query_index; 
        index < results->num_queries; ++index) {
      if (results->hitlist_array[index] && 
          results->hitlist_array[index]->hsplist_count > 0)
         break;
   }
   if (index >= results->num_queries)
      return kBlastHSPStream_Eof;

   stream_data->first_query_index = index;

   hit_list = results->hitlist_array[index];
   last_hsplist_index = hit_list->hsplist_count - 1;

   *hsp_list_out = hit_list->hsplist_array[last_hsplist_index];
   /* Assign the query index here so the caller knows which query this HSP 
      list comes from */
   (*hsp_list_out)->query_index = index;
   /* Dequeue this HSP list by decrementing the HSPList count */
   --hit_list->hsplist_count;
   if (hit_list->hsplist_count == 0) {
      /* Advance the first query index, without checking that the next query 
         has results - that will be done on the next call. */
      ++stream_data->first_query_index;
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
      return kBlastHSPStream_Error;
   }

   /* For RPS BLAST saving procedure is different, because HSPs from different
      subjects are bundled in one HSP list */
   if (Blast_ProgramIsRpsBlast(stream_data->program)) {
      Blast_HSPResultsSaveRPSHSPList(stream_data->program, 
         stream_data->results, *hsp_list, stream_data->hit_options);
   } else {
      Blast_HSPResultsSaveHSPList(stream_data->program, stream_data->results, 
                                  *hsp_list, stream_data->hit_options);
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
    fnptr.closeFn = &s_BlastHSPListCollectorClose;
    SetMethod(hsp_stream, eClose, fnptr);

    SetData(hsp_stream, args);
    return hsp_stream;
}

BlastHSPStream* 
Blast_HSPListCollectorInitMT(EBlastProgramType program, 
                             const BlastHitSavingOptions* hit_options,
                             Int4 num_queries, Boolean sort_on_read,
                             MT_LOCK lock)
{
    BlastHSPListCollectorData* stream_data = 
       (BlastHSPListCollectorData*) malloc(sizeof(BlastHSPListCollectorData));
    BlastHSPStreamNewInfo info;

    stream_data->program = program;
    stream_data->hit_options = hit_options;

    stream_data->results = Blast_HSPResultsNew(num_queries);

    stream_data->results_sorted = FALSE;
    stream_data->sort_on_read = sort_on_read;
    stream_data->first_query_index = 0;
    stream_data->x_lock = lock;

    info.constructor = &s_BlastHSPListCollectorNew;
    info.ctor_argument = (void*)stream_data;

    return BlastHSPStreamNew(&info);
}

BlastHSPStream* 
Blast_HSPListCollectorInit(EBlastProgramType program, 
                           const BlastHitSavingOptions* hit_options,
                           Int4 num_queries, Boolean sort_on_read)
{
   return Blast_HSPListCollectorInitMT(program, hit_options, num_queries, 
                                       sort_on_read, NULL);
}

