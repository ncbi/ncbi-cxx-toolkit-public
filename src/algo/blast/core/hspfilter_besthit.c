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
 * Author:  Ning Ma
 *
 */

/** @file hspfilter_besthit.c
 * Implementation of the BlastHSPWriter interface to save only best hits from
 * a BLAST search, and subsequently return them in sorted order.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */


#include <algo/blast/core/hspfilter_besthit.h>
#include <algo/blast/core/blast_util.h>
#include "blast_hits_priv.h"
   
/** linked list of HSPs
 *  used to keep best hits for each query.
 */
typedef struct LinkedHSP_BH {
   BlastHSP * hsp;
   Int4 sid;   /* OID for hsp*/
   Int4 begin; /* query offset in plus strand - overhang */
   Int4 end;   /* query end in plus strand + overhang */
   Int4 len;   /* actual length */
   struct LinkedHSP_BH *next;
} LinkedHSP_BH;

typedef struct BlastHSPBestHitData {
   BlastHSPBestHitParams* params;       /**< parameters to control overhang */
   BlastQueryInfo* query_info;          /**< query info */
   LinkedHSP_BH** best_list;            /**< buffer to store best hits */
   Int4* num_hsps;                      /**< field to record the number of hsps in each list */
   Int4* max_hsps;                      /**< max number of hsps to hold before pruning */
} BlastHSPBestHitData;

/*************************************************************/
/** The following are implementations for BlastHSPWriter ADT */

/** Perform pre-run stage-specific initialization 
 * @param data The internal data structure [in][out]
 * @param results The HSP results to operate on  [in]
 */ 
static int 
s_BlastHSPBestHitInit(void* data, BlastHSPResults* results)
{
   int i;
   BlastHSPBestHitData * bh_data = data;
   bh_data->best_list = calloc(results->num_queries, sizeof(LinkedHSP_BH *));
   bh_data->num_hsps = calloc(results->num_queries, sizeof(Int4));
   bh_data->max_hsps = calloc(results->num_queries, sizeof(Int4));
   for (i=0; i<results->num_queries; ++i) 
      /* initially set this to 5 times num_seqs to keep */
      /* the max hsps to keep will eventually be determined adaptively */
      bh_data->max_hsps[i] = bh_data->params->prelim_hitlist_size * 2;
   return 0;
}

/** Export best_list to hitlist
 * @param qid The query index [in]
 * @param data The buffered data structure [in][out]
 * @param hit_list The hitlist to be populated [in][out]
 */
static int
s_ExportToHitlist(int qid,
                  BlastHSPBestHitData *bh_data,
                  BlastHitList * hit_list)
{
   int sid;
   Boolean allocated;
   LinkedHSP_BH *best_list = bh_data->best_list[qid], *p;
   BlastHSPList *list;
   BlastHitList *tmp_hit_list = Blast_HitListNew(bh_data->num_hsps[qid]);
   tmp_hit_list->hsplist_array = calloc(bh_data->num_hsps[qid], 
                                   sizeof(BlastHSPList *));

   while (best_list) {

      p = best_list;
      allocated = FALSE;
      for (sid = 0; sid < tmp_hit_list->hsplist_count; ++sid) {
         list = tmp_hit_list->hsplist_array[sid];
         if (p->sid == list->oid) {
            allocated = TRUE;
            break;
         }
      }

      if (! allocated) {
         list = Blast_HSPListNew(bh_data->params->hsp_num_max);
         list->oid = p->sid;
         list->query_index = qid;
         ASSERT(sid < tmp_hit_list->hsplist_current);
         tmp_hit_list->hsplist_array[sid] = list;
         tmp_hit_list->hsplist_count++;
      }

      if (Blast_HSPListUpdate(list, p->hsp)) {
         p->hsp = Blast_HSPFree(p->hsp);
      }

      best_list = p->next;
      free(p);
   }

   bh_data->best_list[qid] = NULL;
   bh_data->num_hsps[qid] = 0;

   for (sid = 0; sid < tmp_hit_list->hsplist_count; ++sid) {
      Blast_HitListUpdate(hit_list, tmp_hit_list->hsplist_array[sid]);
      tmp_hit_list->hsplist_array[sid] = NULL;
   }

   Blast_HitListFree(tmp_hit_list);
   return 0;
}

/** Import hitlist to best_list (assuming all hsps are besthits)
 * @param qid The query index [in]
 * @param data The buffered data structure [in][out]
 * @param hit_list The hitlist to be populated [in][out]
 */
static int
s_ImportFromHitlist(int qid,
                  BlastHSPBestHitData *bh_data,
                  BlastHitList * hit_list)
{
   int sid, id;
   LinkedHSP_BH *best_list = bh_data->best_list[qid], *p, *q, *r;
   BlastHSPList *list;
   BlastHSP *hsp;
   int qlen = BlastQueryInfoGetQueryLength(bh_data->query_info, 
                                           bh_data->params->program, qid);

   for (sid=0; sid < hit_list->hsplist_count; ++sid) {

      list = hit_list->hsplist_array[sid];
      for (id =0; id < list->hspcnt; ++id) {

         hsp = list->hsp_array[id];
         r = malloc(sizeof(LinkedHSP_BH));
         r->hsp = hsp;
         r->sid = list->oid; 
         r->begin = (bh_data->query_info->contexts[hsp->context].frame < 0 ) ? 
                    qlen - hsp->query.end : hsp->query.offset;
         r->len = hsp->query.end - hsp->query.offset;
         r->end = r->begin + r->len;
         for (q=NULL, p=best_list; p && p->begin < r->begin; q=p, p=p->next);
         r->next = p;
         list->hsp_array[id] = NULL; /* remove it from hsp_list */
         if (q) {
            q->next = r;     
         } else {
            best_list = bh_data->best_list[qid] = r;
         }
         ++(bh_data->num_hsps[qid]);
      }
      hit_list->hsplist_array[sid] = Blast_HSPListFree(list);
   }

   bh_data->max_hsps[qid] = bh_data->num_hsps[qid] * 2;

   return 0; 
}


/** Perform post-run clean-ups
   Blast_HSPListFree(hsp_list);
 * @param data The buffered data structure [in]
 * @param results The HSP results to propagate [in][out]
 */ 
static int 
s_BlastHSPBestHitFinal(void* data, BlastHSPResults* results)
{
   int qid, sid, id;
   BlastHSPBestHitData *bh_data = data;
   LinkedHSP_BH **best_list = bh_data->best_list;
   BlastHitList* hitlist;
   BlastHSPList* list;
   double best_evalue, worst_evalue;
   Int4 low_score;

   /* rip best hits off the best_list and put them to results */
   for (qid=0; qid<results->num_queries; ++qid) {
      if (best_list[qid]) {

         if (!results->hitlist_array[qid]) {
            results->hitlist_array[qid] = 
                Blast_HitListNew(bh_data->params->prelim_hitlist_size);
         }
         hitlist = results->hitlist_array[qid];

         s_ExportToHitlist(qid, bh_data, hitlist);

         /* sort hsplists */
         worst_evalue = 0.0;
         low_score = INT4_MAX;
         for (sid=0; sid < hitlist->hsplist_count; ++sid) {
            list = hitlist->hsplist_array[sid];
            best_evalue = (double) INT4_MAX;
            for (id=0; id < list->hspcnt; ++id) {
                best_evalue = MIN(list->hsp_array[id]->evalue, best_evalue);
            }
            Blast_HSPListSortByScore(list);
            list->best_evalue = best_evalue;
            worst_evalue = MAX(worst_evalue, best_evalue);
            low_score = MIN(list->hsp_array[0]->score, low_score);
         }
         hitlist->worst_evalue = worst_evalue;
         hitlist->low_score = low_score;
      }
   }
   sfree(bh_data->best_list);
   sfree(bh_data->num_hsps);
   sfree(bh_data->max_hsps);
   bh_data->best_list = NULL;
   return 0;
}


/** Perform writing task, will save best hits to best_list
 * @param data To store results to [in][out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 */
static int 
s_BlastHSPBestHitRun(void* data, BlastHSPList* hsp_list)
{
   Int4 i, qid, qlen, begin, end, lenA, lenB, scoreA, scoreB, overhang; 
   Int4 allowed_begin, allowed_end;
   double denA, evalueA, evalueB, param_overhang, param_s;
   BlastHSP *hsp;
   LinkedHSP_BH *p, *q, *r;
   Boolean bad;

   BlastHSPBestHitData* bh_data = data;
   BlastHSPBestHitParams* params = bh_data->params;
   EBlastProgramType program = params->program;
   LinkedHSP_BH **best_list = bh_data->best_list;

   if (!hsp_list) return 0;
   param_overhang = params->overhang;
   param_s = 1.0 - params->score_edge;

   for (i=0; i<hsp_list->hspcnt; ++i) {
      
      hsp     = hsp_list->hsp_array[i];
      qid     = Blast_GetQueryIndexFromContext(hsp->context, program);
      qlen    = BlastQueryInfoGetQueryLength(bh_data->query_info, program, qid);

      begin   = (bh_data->query_info->contexts[hsp->context].frame < 0 ) ? 
                 qlen - hsp->query.end 
               : hsp->query.offset;
      lenA    = hsp->query.end - hsp->query.offset;
      end     = begin + lenA;
      scoreA  = hsp->score;
      evalueA = hsp->evalue;
      denA    = 1.0 * scoreA / lenA / param_s;

      /* See if new hit A is bad */
      bad = FALSE;
      for (p=best_list[qid]; p &&   p->end < end;   p=p->next);
      for (                ; p && p->begin < begin; p=p->next) {
         /* check conditions */
         lenB   = p->len;
         scoreB = p->hsp->score;
         evalueB= p->hsp->evalue;
         if (              p->end >= end               /* condition 1 */
           &&             evalueB <= evalueA           /* condition 2 */
           && 1.0 * scoreB / lenB >  denA)             /* condition 3 */
         {
             /* the new hit is bad, do nothing */
             bad = TRUE;
             break;
         }
      }
      if (bad) continue;  

      /* See if new hit A makes some old hits bad */
      overhang = 2.0 * lenA * param_overhang / (1.0 - 2.0 * param_overhang);
      allowed_begin = begin - overhang;
      allowed_end   = end   + overhang;
      overhang = lenA * param_overhang;
      begin -= overhang;
      end   += overhang;
      denA   = 1.0 * scoreA / lenA * param_s;
      /* use q to remember node before p */
      for (q=NULL, p=best_list[qid]; p && p->begin < allowed_begin; q=p, p=p->next);
      for (; p && p->begin < allowed_end; ) {
         /* check conditions */
         lenB     = p->len;
         scoreB   = p->hsp->score;
         overhang = (p->end - p->begin - lenB)/2;
         evalueB= p->hsp->evalue;
         if ( p->begin + overhang >= begin
           && p->end   - overhang <= end               /* condition 1 */
           &&             evalueB >= evalueA           /* condition 2 */
           && 1.0 * scoreB / lenB <  denA)             /* condition 3 */
         {   /* remove it from best list */
             r = p;
             if (q)      q->next = p->next;
             else best_list[qid] = p->next;
             p = p->next;
             r->hsp = Blast_HSPFree(r->hsp);
             free(r);
             --(bh_data->num_hsps[qid]);
         } else {
             q = p;
             p = p->next;
         }
      }

      /* Insert hit A into the best_list and hit_list */
      for (q=NULL, p=best_list[qid]; p && p->begin < begin; q=p, p=p->next);
      r = malloc(sizeof(LinkedHSP_BH));
      r->hsp   = hsp;
      r->sid   = hsp_list->oid; 
      r->begin = begin;
      r->end   = end;
      r->len   = lenA;
      r->next  = p;
      hsp_list->hsp_array[i] = NULL; /* remove it from hsp_list */
      if (q) {
         q->next = r;     
      } else {
         best_list[qid] = r;
      }

      /* If hsps exceed max limit, prune */
      if ( ++(bh_data->num_hsps[qid]) > bh_data->max_hsps[qid]) {
         BlastHitList *hitlist = Blast_HitListNew(bh_data->params->prelim_hitlist_size);
         s_ExportToHitlist(qid, bh_data, hitlist);
         s_ImportFromHitlist(qid, bh_data, hitlist);
         Blast_HitListFree(hitlist);
      }
   }

   /* now all qualified hits have been moved to best_list, we can remove hsp_list */
   Blast_HSPListFree(hsp_list);

   return 0; 
}

/** Perform writing task for RPS blast, will save best hits to best_list
 * @param data To store results to [in][out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 */
static int 
s_BlastHSPBestHitRun_RPS(void* data, BlastHSPList* hsp_list)
{
   Int4 i, qid, begin, end, lenA, lenB, scoreA, scoreB, overhang; 
   Int4 allowed_begin, allowed_end;
   double denA, evalueA, evalueB, param_overhang, param_s;
   BlastHSP *hsp;
   LinkedHSP_BH *p, *q, *r;
   Boolean bad;

   BlastHSPBestHitData* bh_data = data;
   BlastHSPBestHitParams* params = bh_data->params;
   LinkedHSP_BH **best_list = bh_data->best_list;

   if (!hsp_list) return 0;
   param_overhang = params->overhang;
   param_s = 1.0 - params->score_edge;

   for (i=0; i<hsp_list->hspcnt; ++i) {
      
      hsp     = hsp_list->hsp_array[i];
      qid     = hsp_list->query_index;
      begin   = hsp->query.offset;
      lenA    = hsp->query.end - hsp->query.offset;
      end     = begin + lenA;
      scoreA  = hsp->score;
      evalueA = hsp->evalue;
      denA    = 1.0 * scoreA / lenA / param_s;

      /* See if new hit A is bad */
      bad = FALSE;
      for (p=best_list[qid]; p &&   p->end < end;   p=p->next);
      for (                ; p && p->begin < begin; p=p->next) {
         /* check conditions */
         lenB   = p->len;
         scoreB = p->hsp->score;
         evalueB= p->hsp->evalue;
         if (              p->end >= end               /* condition 1 */
           &&             evalueB <= evalueA           /* condition 2 */
           && 1.0 * scoreB / lenB >  denA)             /* condition 3 */
         {
             /* the new hit is bad, do nothing */
             bad = TRUE;
             break;
         }
      }
      if (bad) continue;  

      /* See if new hit A makes some old hits bad */
      overhang = 2.0 * lenA * param_overhang / (1.0 - 2.0 * param_overhang);
      allowed_begin = begin - overhang;
      allowed_end   = end   + overhang;
      overhang = lenA * param_overhang;
      begin -= overhang;
      end   += overhang;
      denA   = 1.0 * scoreA / lenA * param_s;
      /* use q to remember node before p */
      for (q=NULL, p=best_list[qid]; p && p->begin < allowed_begin; q=p, p=p->next);
      for (; p && p->begin < allowed_end; ) {
         /* check conditions */
         lenB     = p->len;
         scoreB   = p->hsp->score;
         overhang = (p->end - p->begin - lenB)/2;
         evalueB= p->hsp->evalue;
         if ( p->begin + overhang >= begin
           && p->end   - overhang <= end               /* condition 1 */
           &&             evalueB >= evalueA           /* condition 2 */
           && 1.0 * scoreB / lenB <  denA)             /* condition 3 */
         {   /* remove it from best list */
             r = p;
             if (q)      q->next = p->next;
             else best_list[qid] = p->next;
             p = p->next;
             r->hsp = Blast_HSPFree(r->hsp);
             free(r);
         } else {
             q = p;
             p = p->next;
         }
      }

      /* Insert hit A into the best_list and hit_list */
      for (q=NULL, p=best_list[qid]; p && p->begin < begin; q=p, p=p->next);
      r = malloc(sizeof(LinkedHSP_BH));
      r->hsp   = hsp;
      r->sid   = hsp->context;
      hsp->context = qid;
      r->begin = begin;
      r->end   = end;
      r->len   = lenA;
      r->next  = p;
      hsp_list->hsp_array[i] = NULL; /* remove it from hsp_list */
      if (q) {
         q->next = r;     
      } else {
         best_list[qid] = r;
      }
   }

   /* now all qualified hits have been moved to best_list, we can remove hsp_list */
   Blast_HSPListFree(hsp_list);

   return 0; 
}

/** Free the writer 
 * @param writer The writer to free [in]
 * @return NULL.
 */
static 
BlastHSPWriter*
s_BlastHSPBestHitFree(BlastHSPWriter* writer) 
{
   BlastHSPBestHitData *data = writer->data;
   sfree(data->params); 
   sfree(writer->data);
   sfree(writer);
   return NULL;
}

/** create the writer
 * @param params Pointer to the besthit parameter [in]
 * @param query_info BlastQueryInfo [in]
 * @return writer
 */
static
BlastHSPWriter* 
s_BlastHSPBestHitNew(void* params, BlastQueryInfo* query_info)
{
   BlastHSPWriter * writer = NULL;
   BlastHSPBestHitData * data = NULL;
   BlastHSPBestHitParams * bh_param = params;

   /* best hit algo needs query_info */
   if (! query_info) return NULL;

   /* allocate space for writer */
   writer = malloc(sizeof(BlastHSPWriter));

   /* fill up the function pointers */
   writer->InitFnPtr   = &s_BlastHSPBestHitInit;
   writer->FinalFnPtr  = &s_BlastHSPBestHitFinal;
   writer->FreeFnPtr   = &s_BlastHSPBestHitFree;
   writer->RunFnPtr    = (Blast_ProgramIsRpsBlast(bh_param->program))
                       ? &s_BlastHSPBestHitRun_RPS
                       : &s_BlastHSPBestHitRun;

   /* allocate for data structure */
   writer->data = malloc(sizeof(BlastHSPBestHitData));
   data = writer->data;
   data->params = params;
   data->query_info = query_info;
    
   return writer;
}

/** The pipe version of best-hit writer.  
 * @param data To store results to [in][out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 */
static int 
s_BlastHSPBestHitPipeRun(void* data, BlastHSPResults* results)
{
   int qid, sid, num_list;
   s_BlastHSPBestHitInit(data, results);
   for (qid = 0; qid < results->num_queries; ++qid) {
      if (!(results->hitlist_array[qid])) continue;
      num_list = results->hitlist_array[qid]->hsplist_count;
      for (sid = 0; sid < num_list; ++sid) {
         s_BlastHSPBestHitRun(data, 
               results->hitlist_array[qid]->hsplist_array[sid]);
         results->hitlist_array[qid]->hsplist_array[sid] = NULL;
      }
      results->hitlist_array[qid]->hsplist_count = 0;
      Blast_HitListFree(results->hitlist_array[qid]);
      results->hitlist_array[qid] = NULL;
   }
   s_BlastHSPBestHitFinal(data, results);
   return 0;
}

/** Free the pipe
 * @param pipe The pipe to free [in]
 * @return NULL.
 */
static
BlastHSPPipe*
s_BlastHSPBestHitPipeFree(BlastHSPPipe* pipe) 
{
   BlastHSPBestHitData *data = pipe->data;
   sfree(data->params); 
   sfree(pipe->data);
   sfree(pipe);
   return NULL;
}

/** create the pipe
 * @param params Pointer to the besthit parameter [in]
 * @param query_info BlastQueryInfo [in]
 * @return pipe
 */
static
BlastHSPPipe* 
s_BlastHSPBestHitPipeNew(void* params, BlastQueryInfo* query_info)
{
   BlastHSPPipe * pipe = NULL;
   BlastHSPBestHitData * data = NULL;

   /* best hit algo needs query_info */
   if (! query_info) return NULL;

   /* allocate space for writer */
   pipe = malloc(sizeof(BlastHSPPipe));

   /* fill up the function pointers */
   pipe->RunFnPtr = &s_BlastHSPBestHitPipeRun;
   pipe->FreeFnPtr= &s_BlastHSPBestHitPipeFree;

   /* allocate for data structure */
   pipe->data = malloc(sizeof(BlastHSPBestHitData));
   data = pipe->data;
   data->params = params;
   data->query_info = query_info;
   pipe->next = NULL;
    
   return pipe;
}

/**************************************************************/
/** The following are exported functions to be used by APP    */

BlastHSPBestHitParams*
BlastHSPBestHitParamsNew(const BlastHitSavingOptions* hit_options,
                         const BlastHSPBestHitOptions* best_hit_opts,
                         Int4 compositionBasedStats,
                         Boolean gapped_calculation)
{
    BlastHSPBestHitParams* retval = NULL;
    Int4 prelim_hitlist_size = hit_options->hitlist_size;

    if (compositionBasedStats)
         prelim_hitlist_size = prelim_hitlist_size * 2 + 50;
    else if (gapped_calculation)
         prelim_hitlist_size = MIN(2 * prelim_hitlist_size,
                                   prelim_hitlist_size + 50);

    retval = (BlastHSPBestHitParams*) malloc(sizeof(BlastHSPBestHitParams));
    retval->prelim_hitlist_size = MAX(hit_options->hitlist_size, 10);
    retval->hsp_num_max = BlastHspNumMax(gapped_calculation, hit_options);
    retval->program = hit_options->program_number;
    retval->overhang = best_hit_opts->overhang;
    retval->score_edge = best_hit_opts->score_edge;
    return retval;
}

BlastHSPBestHitParams*
BlastHSPBestHitParamsFree(BlastHSPBestHitParams* opts)
{
    if ( !opts )
        return NULL;
    sfree(opts);
    return NULL;
}

BlastHSPWriterInfo*
BlastHSPBestHitInfoNew(BlastHSPBestHitParams* params) {
    BlastHSPWriterInfo * writer_info =
                         malloc(sizeof(BlastHSPWriterInfo));
    writer_info->NewFnPtr = &s_BlastHSPBestHitNew;
    writer_info->params = params;
    return writer_info;
}

BlastHSPPipeInfo*
BlastHSPBestHitPipeInfoNew(BlastHSPBestHitParams* params) {
    BlastHSPPipeInfo * pipe_info =
                         malloc(sizeof(BlastHSPPipeInfo));
    pipe_info->NewFnPtr = &s_BlastHSPBestHitPipeNew;
    pipe_info->params = params;
    pipe_info->next = NULL;
    return pipe_info;
}
