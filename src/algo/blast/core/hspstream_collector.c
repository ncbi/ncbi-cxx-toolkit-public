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

static char const rcsid[] = 
    "$Id$";


#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/hspstream_collector.h>

/** Default hit saving stream methods */

BlastHSPStream* BlastHSPListCollectorFree(BlastHSPStream* hsp_stream) 
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);
   Blast_HSPResultsFree(stream_data->results);
   sfree(stream_data);
   sfree(hsp_stream);
   return NULL;
}

void BlastHSPListCollectorClose(BlastHSPStream* hsp_stream)
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   if (stream_data->results == NULL || stream_data->results_sorted)
      return;

   if (stream_data->sort_on_read) {
      Blast_HSPResultsReverseSort(stream_data->results);
   }
   stream_data->results_sorted = TRUE;
}

int BlastHSPListCollectorRead(BlastHSPStream* hsp_stream, 
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
      BlastHSPListCollectorClose(hsp_stream);

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

int BlastHSPListCollectorWrite(BlastHSPStream* hsp_stream, 
                              BlastHSPList** hsp_list)
{
   BlastHSPListCollectorData* stream_data = 
      (BlastHSPListCollectorData*) GetData(hsp_stream);

   /** @todo Lock the mutex here */

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
   if (stream_data->program == blast_type_rpsblast || 
       stream_data->program == blast_type_rpstblastn) {
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

   /** @todo Unlock the mutex here */

   return kBlastHSPStream_Success;
}

BlastHSPStream* 
BlastHSPListCollectorNew(BlastHSPStream* hsp_stream, void* args) 
{
    BlastHSPStreamFunctionPointerTypes fnptr;

    fnptr.dtor = &BlastHSPListCollectorFree;
    SetMethod(hsp_stream, eDestructor, fnptr);
    fnptr.method = &BlastHSPListCollectorRead;
    SetMethod(hsp_stream, eRead, fnptr);
    fnptr.method = &BlastHSPListCollectorWrite;
    SetMethod(hsp_stream, eWrite, fnptr);
    fnptr.closeFn = &BlastHSPListCollectorClose;
    SetMethod(hsp_stream, eClose, fnptr);

    SetData(hsp_stream, args);
    return hsp_stream;
}

BlastHSPStream* 
Blast_HSPListCollectorInit(Uint1 program, BlastHitSavingOptions* hit_options,
                           Int4 num_queries, Boolean sort_on_read)
{
    BlastHSPListCollectorData* stream_data = 
       (BlastHSPListCollectorData*) malloc(sizeof(BlastHSPListCollectorData));
    BlastHSPStreamNewInfo info;

    stream_data->program = program;
    stream_data->hit_options = hit_options;
    if (program == blast_type_rpsblast || program == blast_type_rpstblastn) {
       /* For RPS BLAST, there is only one query, and num_queries variable
        * is in fact the number of database sequences. */
       Blast_HSPResultsInit(1, &stream_data->results);
       stream_data->results->hitlist_array[0] = 
          Blast_HitListNew(num_queries);
    } else {
       Blast_HSPResultsInit(num_queries, &stream_data->results);
    }
    stream_data->results_sorted = FALSE;
    stream_data->sort_on_read = sort_on_read;
    stream_data->first_query_index = 0;

    info.constructor = &BlastHSPListCollectorNew;
    info.ctor_argument = (void*)stream_data;

    return BlastHSPStreamNew(&info);
}
