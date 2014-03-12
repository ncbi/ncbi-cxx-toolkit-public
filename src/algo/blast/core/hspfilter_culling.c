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
 * Author: Tom Madden 
 *
 */

/** @file hspfilter_culling.c
 * Implementation of the BlastHSPWriter interface to perform 
 * culling.  The implementation is based upon the algorithm
 * described in [1], though the original implementation only
 * applied to the preliminary stage and was later rewritten 
 * to use interval trees by Jason Papadopoulos.
 *
 * [1] Berman P, Zhang Z, Wolf YI, Koonin EV, Miller W. Winnowing sequences from a
 * database search. J Comput Biol. 2000 Feb-Apr;7(1-2):293-302. PubMed PMID:
 * 10890403.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */


#include <algo/blast/core/hspfilter_culling.h>
#include <algo/blast/core/hspfilter_collector.h>
#include <algo/blast/core/blast_util.h>
//#include "blast_hits_priv.h"

/*************************************************************/
/** linked list of HSPs
 *  used to store hsps in culling tree.
 */
typedef struct LinkedHSP {
    BlastHSP * hsp;
    Int4 cid;    /* context id for hsp */
    Int4 sid;    /* OID for hsp*/
    Int4 begin;  /* query offset in plus strand */
    Int4 end;    /* query end in plus strand */
    Int4 merit;  /* how many other hsps in the tree dominates me? */
    struct LinkedHSP *next;
} LinkedHSP;

/** functions to manipulate LinkedHSPs */

/** copy x to y */
static LinkedHSP * s_HSPCopy(LinkedHSP *x) {
    LinkedHSP * y = malloc(sizeof(LinkedHSP));
    memcpy(y,x, sizeof(LinkedHSP));
    return y;
}

/** free x */
static LinkedHSP * s_HSPFree(LinkedHSP *x) {
    Blast_HSPFree(x->hsp);
    sfree(x);
    return NULL;
}

/** return true if p dominates y  */
static Boolean s_DominateTest(LinkedHSP *p, LinkedHSP *y, Boolean drop_y_if_tie) {
    Int8 b1 = p->begin;
    Int8 b2 = y->begin;
    Int8 e1 = p->end;
    Int8 e2 = y->end;
    Int8 s1 = p->hsp->score;
    Int8 s2 = y->hsp->score;
    Int8 l1 = e1 - b1;
    Int8 l2 = e2 - b2;

    /* the main criterion:
       2 * (%diff in score) + 1 * (%diff in length) */
    Int8 d  = 3*s1*l1 + s1*l2 - s2*l1 - 3*s2*l2;

    if (d < 0 ||  
         /* the following is the 50% overlap condition */
        ((e1+b1-2*b2) * (e1+b1-2*e2) > 0
      && (e2+b2-2*b1) * (e2+b2-2*e1) > 0)) return FALSE;

    if (d > 0 || 
         /* when two hsps are identical, drop the 2nd one */
         (drop_y_if_tie && s1 == s2 && l1 == l2)) return TRUE;

    /* non-identical case, use score as tie_break
       note: when two hsps are identical, drop the 1st one */
    return (s1 > s2);
}

/** test hsp y to see if it is dominated by lower merit hsps in list */
static Boolean s_LowMeritPass(LinkedHSP *list, LinkedHSP *y) {
    LinkedHSP *p = list;
    while (p) {
       if (p->merit == 1 && s_DominateTest(p, y, TRUE))  return FALSE;
       p = p->next;
    }
    return TRUE;
}

/** check how many hsps in list dominates y, and update merit of y accordingly */
static Boolean s_FullPass(LinkedHSP *list, LinkedHSP *y) {
    LinkedHSP *p = list;
    while (p) {
       if (s_DominateTest(p, y, TRUE)) { 
          (y->merit)--;
          if (y->merit <= 0) return FALSE;
       }
       p = p->next;
    }
    return TRUE;
}

/** update merit for hsps in list; also returns the number of hsps in list */
static Int4 s_ProcessHSPList(LinkedHSP **list, LinkedHSP *y) {
    Int4 num = 0;
    LinkedHSP *p = *list, *q, *r;
    q = p;
    while (p) {
       ++num;
       r = p;
       p = p->next;
       if (r != y && s_DominateTest(y, r, FALSE)) {
          (r->merit)--;
          if (r->merit <= 0) {
             if (r == *list) {
                 *list = p;
                 q = p;
             } else {
                 q->next = p;
             }
             --num;
             s_HSPFree(r);
          } else {
             q = r;
          }
       } else {
          q = r;
       }
    }
    return num;
}

/** decrease merit for all hsps in list; also returns the number of hsps in list */
static Int4 s_MarkDownHSPList(LinkedHSP **list) {
    Int4 num = 0;
    LinkedHSP *p = *list, *q, *r;
    q = p;
    while (p) {
       ++num;
       r = p;
       p = p->next;
       (r->merit)--;
       if (r->merit <= 0) {
          if (r == *list) {
              *list = p;
              q = p;
          } else {
              q->next = p;
          }
          --num;
          s_HSPFree(r);
       } else {
          q = r;
       }
    }
    return num;
}
          
                
/** add an hsp to the front of hsp list */
static void s_AddHSPtoList(LinkedHSP **list, LinkedHSP *y) {
    y->next = *list;
    *list = y;
    return;
}

/*************************************************************/
/** definition of a Culling tree */
typedef struct CTreeNode {
    Int4 begin;  /* left endpoint */
    Int4 end;    /* right endpoint */
    struct CTreeNode *left;    /* left child */
    struct CTreeNode *right;   /* right child */
    LinkedHSP *hsplist; /* hsps belong to this node, start with low merits */
} CTreeNode;

/*******Memory management layer (may be changed later)********/
static CTreeNode * s_GetNode() {
    return( (CTreeNode *) malloc(sizeof(CTreeNode)));
}

static CTreeNode * s_RetNode(CTreeNode * node) {
    sfree(node);
    return NULL;
}
    
/*************************************************************/
/**  functions to manipulate Culling Tree (private)*/

typedef enum ECTreeChild {
    eLeft,
    eRight,
} ECTreeChild;

/** Allocate and return a new node for use */
static CTreeNode * s_CTreeNodeNew(CTreeNode * parent, ECTreeChild dir) {
    Int4 midpt;
    CTreeNode * node = s_GetNode();

    node->left    = NULL;
    node->right   = NULL;
    node->hsplist = NULL;

    if (!parent) return node;

    midpt = (parent->begin + parent->end) / 2;
    if (dir == eLeft) {
        node->begin = parent->begin;
        node->end   = midpt;
    } else {
        node->begin = midpt;
        node->end   = parent->end;
    }
    return node;
}

/** Free an individual node */
static CTreeNode * s_CTreeNodeFree(CTreeNode * node) {
    ASSERT(node->left  == NULL);
    ASSERT(node->right == NULL);
    ASSERT(node->hsplist == NULL);
    return(s_RetNode(node));
}

/** Fork children from a node */
static void s_ForkChildren(CTreeNode * node) {
    CTreeNode * child;
    LinkedHSP *p, *q, *r;
    Int4 midpt;

    ASSERT(node != NULL);
    ASSERT(node->left ==NULL);
    ASSERT(node->right ==NULL);

    p = node->hsplist;
    q = p;  /* q is predecessor of p */
    midpt = (node->begin + node->end) /2;
    while(p) {
      child = NULL;
      r = p;
      if (p->end < midpt) {
         if (!node->left) {
            node->left = s_CTreeNodeNew(node, eLeft);
         }
         child = node->left;
      } else if (p->begin > midpt) {
         if (!node->right) {
            node->right = s_CTreeNodeNew(node, eRight);
         }
         child = node->right;
      }
      p = p->next;
      if (child) {
         /* remove r from parent list */
         if (r == node->hsplist) {
             node->hsplist = p;
             q = p;
         } else {   
             q->next = p;
         }
         /* and put it on the child */
         s_AddHSPtoList(&(child->hsplist), r);
      } else {
         q = r;
      }      
   }
}

#if 0
static void s_Debug(CTreeNode *node) {
   LinkedHSP *p;
   if(!node) return;
   p=node->hsplist;
   while(p) {
      printf(" (%d %d %d %d)",p->begin, p->end, p->hsp->score,p->merit);
      p=p->next;
   }
   printf("\n");
   s_Debug(node->left);
   s_Debug(node->right);
   return;
}
#endif

/** recursively decrease the merit of all hsps within a subtree, 
    return TRUE if whole node is empty and should be deleted */
static void s_MarkDownCTree(CTreeNode ** node) {
   if (! (*node)) return;

   s_MarkDownCTree(&((*node)->left));
   s_MarkDownCTree(&((*node)->right));
   if ( s_MarkDownHSPList(&((*node)->hsplist)) <= 0
     && !(*node)->left && !(*node)->right) {
        s_CTreeNodeFree(*node);
        *node = NULL;
   }
   return;
}

/** recursively search and update merit hsps in culling tree 
    due to addition of hsp x */
static void s_ProcessCTree(CTreeNode ** node, LinkedHSP *x) {
   Int4 midpt;

   if (! (*node)) return;

   /* first test if x includes the full range covered by node */
   if (x->begin <= (*node)->begin && x->end >= (*node)->end) {
      s_MarkDownCTree(node);
      return;
   }

   /* if node reaches the leaves*/
   if (!(*node)->left && !(*node)->right) {
      if (s_ProcessHSPList(&((*node)->hsplist), x) <= 0) {
          s_CTreeNodeFree(*node);
          *node = NULL;
      } 
      return;
   }

   /* recursive case */
   midpt = ((*node)->begin + (*node)->end) / 2;
   if (x->end < midpt) {
      s_ProcessCTree(&((*node)->left), x);
   } else if (x->begin > midpt) {
      s_ProcessCTree(&((*node)->right), x);
   } else {
      s_ProcessCTree(&((*node)->left), x);
      s_ProcessCTree(&((*node)->right), x);
      if (s_ProcessHSPList(&((*node)->hsplist), x) <= 0
       && !(*node)->left && !(*node)->right) {
          s_CTreeNodeFree(*node);
          *node = NULL;
      }
   }
   return;
}

/*************************************************************/
/**  functions to manipulate Culling Tree (public)*/

/** Allocate a tree */
static CTreeNode * s_CTreeNew(Int4 qlen) {
    CTreeNode * tree = s_CTreeNodeNew(NULL, eLeft);
    tree->begin = 0;
    tree->end   = qlen;
    return tree;
}

/** Recursively deallocate a tree, assuming all hsps are already ripped off */
static CTreeNode * s_CTreeFree(CTreeNode *tree) {
    if (!tree) return NULL;

    ASSERT(tree->hsplist == NULL);

    tree->left  = s_CTreeFree(tree->left);
    tree->right = s_CTreeFree(tree->right);
    s_CTreeNodeFree(tree);
    return NULL;
}

/** Recursively rip off hsps into a link list */
static LinkedHSP * s_RipHSPOffCTree(CTreeNode *tree) {
    LinkedHSP *q, *p;

    if (!tree) return NULL;

    q = tree->hsplist;
    tree->hsplist = NULL;

    /* grab left child */
    if (!q) {
       q = s_RipHSPOffCTree(tree->left);
       p = q;
    } else {
       p = q;
       while(p->next) p=p->next;
       p->next = s_RipHSPOffCTree(tree->left);
    }

    /* grab right child */
    if (!q) {
       q = s_RipHSPOffCTree(tree->right);
    } else {
       while(p->next) p=p->next;
       p->next = s_RipHSPOffCTree(tree->right);
    }

    return q;
}

/** First pass to see if hsp A can be insert into the tree, will
    only compare A to the low merit ones  */
static Boolean s_FirstPass(CTreeNode *tree, LinkedHSP *A) {
    Int4 midpt;

    ASSERT(tree != NULL);
    /* Descend the tree */
    while (tree) {
       ASSERT(tree->begin <= A->begin);
       ASSERT(tree->end   >= A->end);
       
       if (! s_LowMeritPass(tree->hsplist, A)) return FALSE;
       midpt = (tree->begin + tree->end) /2;
       if      (A->end   < midpt) tree = tree->left;
       else if (A->begin > midpt) tree = tree->right;
       else return TRUE;
    }
    return TRUE;
}

/** Second pass, a full traverse to determine the merit of A,
    in addition, insert A to the proper place if A is valid,
    or return FALSE if A's merit decreases to zero */
static Boolean s_SecondPass(CTreeNode *tree, LinkedHSP *A) {
   Int4 midpt;

   LinkedHSP *x;
   CTreeNode *node;
   Int4 kNumHSPtoFork = 20;  /** number of HSP to trig forking children */
    
   ASSERT(tree != NULL);
   /* Descend the tree */
   while (tree) {
      ASSERT(tree->begin <= A->begin);
      ASSERT(tree->end   >= A->end);
 
      if (! s_FullPass(tree->hsplist, A)) return FALSE;
      midpt = (tree->begin + tree->end) /2;
      node = tree;  /* record the last valid position */
      if      (A->end   < midpt) tree = tree->left;
      else if (A->begin > midpt) tree = tree->right;
      else break;
   }

   /* if we get here, A is valid. copy and insert A at node */
   x = s_HSPCopy(A);
   s_AddHSPtoList(&(node->hsplist), x);

   /* if this is the leaf, calculate update hsp number */
   if (!node->left && !node->right) {
       /* check for domination */
       if (s_ProcessHSPList(&(node->hsplist), x) >= kNumHSPtoFork) {
           /* fork this node into sub trees */
           s_ForkChildren(node);
       }
       return TRUE;
   }

   /* check domination */
   s_ProcessCTree(&node, x);
   return TRUE;
}

/*************************************************************/
/** The following are implementations for BlastHSPWriter ADT */

/** Auxillary data structure used by culling */
typedef struct BlastHSPCullingData {
    BlastHSPCullingParams* params; /**< parameters for culling. */
    BlastQueryInfo* query_info;    /**< information about queries */
    Int4 num_contexts;             /**< number of contexts */
    CTreeNode** c_tree;            /**< forest of culling trees */
} BlastHSPCullingData;

/** Perform pre-run stage-specific initialization 
 * @param data The internal data structure [in][out]
 * @param results The HSP results to operate on  [in]
 */ 
static int 
s_BlastHSPCullingInit(void* data, BlastHSPResults* results)
{
    BlastHSPCullingData * cull_data = data;
    cull_data->c_tree = calloc(cull_data->num_contexts, sizeof(CTreeNode *));
    return 0;
}

/** Perform post-run clean-ups
 * @param data The buffered data structure [in]
 * @param results The HSP results to propagate [in][out]
 */ 
static int 
s_BlastHSPCullingFinal(void* data, BlastHSPResults* results)
{
   int cid, qid, sid, id, new_allocated;
   BlastHSPCullingData* cull_data = data;
   BlastHSPCullingParams* params = cull_data->params;
   CTreeNode **c_tree = cull_data->c_tree;
   LinkedHSP *cull_list, *p;
   BlastHitList * hitlist;
   BlastHSPList * list;
   Boolean allocated;
   double best_evalue, worst_evalue;
   Int4 low_score;
   const int kStartValue = 100;

   /* rip best hits off the best_list and put them to results */
   for (cid=0; cid < cull_data->num_contexts; ++cid) {
      if (c_tree[cid]) {
         qid = Blast_GetQueryIndexFromContext(cid, params->program);
         if (!results->hitlist_array[qid]) {
            results->hitlist_array[qid] = Blast_HitListNew(params->prelim_hitlist_size);
         }
         hitlist = results->hitlist_array[qid];

         /* collapse the linked hsps tree into one list and free the tree */
         cull_list = s_RipHSPOffCTree(c_tree[cid]);
         c_tree[cid] = s_CTreeFree(c_tree[cid]);

         /* insert hsp list into results */
         while (cull_list) {
            p = cull_list;
            /* test to see if new hsplist has already been allocated */
            allocated = FALSE;
            for (sid=0; sid<hitlist->hsplist_count; ++sid) {
                list = hitlist->hsplist_array[sid];
                if (p->sid == list->oid) {
                   allocated = TRUE;
                   break;
                }
            }
            if (!allocated) {                                            
                /* we must allocate a new hsplist*/                      
                list = Blast_HSPListNew(0);                              
                list->oid = p->sid;                                      
                list->query_index = qid;                                 
                if (sid >= hitlist->hsplist_current) {                   
                   /* we must increase the pool size as well */          
                   new_allocated = MAX(kStartValue, 2*sid);              
                   hitlist->hsplist_array = (BlastHSPList **)            
                      realloc(hitlist->hsplist_array, new_allocated*sizeof(BlastHSPList*));
                   hitlist->hsplist_current = new_allocated;             
                }                                                        
                hitlist->hsplist_array[sid] = list;                      
                hitlist->hsplist_count++;          
            }                                                            
            /* put the new hsp into the array */                         
            id = list->hspcnt;                                           
            if (id >= list->allocated) {                                 
                /* we must increase the list size */                     
                new_allocated = 2*id;                                    
                list->hsp_array = (BlastHSP**)                           
                      realloc(list->hsp_array, new_allocated*sizeof(BlastHSP*));       
                list->allocated = new_allocated;                         
            }                                                            
            p = cull_list;
            list->hsp_array[id] = p->hsp;                                
            list->hspcnt++;                                              
            cull_list = p->next;
            free(p);
         }                                                               
                                                                         
         /* sort hsplist */                                              
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
   sfree(cull_data->c_tree);
   cull_data->c_tree = NULL;
   return 0;
}

/** Perform writing task
 * ownership of the HSP list and sets the dereferenced pointer to NULL.
 * This is bascially a copy of s_BlastHSPCollectorRun with calls to
 * the culling function.
 * @param data To store results to [in][out]
 * @param hsplist Pointer to the HSP list to save in the collector. [in]
 */
static int 
s_BlastHSPCullingRun(void* data, BlastHSPList* hsp_list)
{
   Int4 i, qlen;
   LinkedHSP A;

   BlastHSPCullingData * cull_data = data;
   BlastHSPCullingParams* params = cull_data->params;
   CTreeNode **c_tree = cull_data->c_tree;

   if (!hsp_list) return 0;

   for (i=0; i<hsp_list->hspcnt; ++i) {

      /* wrap the hsp with a LinkedHSP structure */
      A.hsp   = hsp_list->hsp_array[i];                                  
      A.cid   = A.hsp->context;
      A.sid   = hsp_list->oid;
      A.merit = params->culling_max;
      A.begin = A.hsp->query.offset;
      A.end   = A.hsp->query.end;
      A.next  = NULL;
      qlen    = cull_data->query_info->contexts[A.cid].query_length;

      if (! c_tree[A.cid]) {
         c_tree[A.cid] = s_CTreeNew(qlen);
      }

      if ( s_FirstPass(c_tree[A.cid], &A) && s_SecondPass(c_tree[A.cid], &A)) {
         hsp_list->hsp_array[i] = NULL;
      } 

   }

   /* now all good hits have moved to tree, we can remove hsp_list */
   Blast_HSPListFree(hsp_list);
         
   return 0; 
}

/** Free the writer 
 * @param writer The writer to free [in]
 * @return NULL.
 */
static 
BlastHSPWriter*
s_BlastHSPCullingFree(BlastHSPWriter* writer) 
{
   BlastHSPCullingData *data = writer->data;
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
s_BlastHSPCullingNew(void* params, BlastQueryInfo* query_info)
{
   BlastHSPWriter * writer = NULL;
   BlastHSPCullingData data;

   /* culling algo needs query_info */
   if (! query_info) return NULL;

   /* allocate space for writer */
   writer = malloc(sizeof(BlastHSPWriter));

   /* fill up the function pointers */
   writer->InitFnPtr   = &s_BlastHSPCullingInit;
   writer->FinalFnPtr  = &s_BlastHSPCullingFinal;
   writer->FreeFnPtr   = &s_BlastHSPCullingFree;
   writer->RunFnPtr    = &s_BlastHSPCullingRun;

   /* allocate for data structure */
   writer->data = malloc(sizeof(BlastHSPCullingData));
   data.params = params;
   data.query_info = query_info;
   data.num_contexts = query_info->last_context + 1;
   memcpy(writer->data, &data, sizeof(data));
   return writer;
}

/** The pipe version of best-hit writer.  
 * @param data To store results to [in][out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 */
static int 
s_BlastHSPCullingPipeRun(void* data, BlastHSPResults* results)
{
   int qid, sid, num_list;

   s_BlastHSPCullingInit(data, results);
   for (qid = 0; qid < results->num_queries; ++qid) {
      if (!(results->hitlist_array[qid])) continue;
      num_list = results->hitlist_array[qid]->hsplist_count;
      for (sid = 0; sid < num_list; ++sid) {
         s_BlastHSPCullingRun(data,
                   results->hitlist_array[qid]->hsplist_array[sid]);
         results->hitlist_array[qid]->hsplist_array[sid] = NULL;
      }
      results->hitlist_array[qid]->hsplist_count = 0;
      Blast_HitListFree(results->hitlist_array[qid]);
      results->hitlist_array[qid] = NULL;
   }
   s_BlastHSPCullingFinal(data, results);
   return 0;
}

/** Free the pipe
 * @param pipe The pipe to free [in]
 * @return NULL.
 */
static
BlastHSPPipe*
s_BlastHSPCullingPipeFree(BlastHSPPipe* pipe) 
{
   BlastHSPCullingData *data = pipe->data;
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
s_BlastHSPCullingPipeNew(void* params, BlastQueryInfo* query_info)
{
   BlastHSPPipe * pipe = NULL;
   BlastHSPCullingData data;

   /* culling algo needs query_info */
   if (! query_info) return NULL;

   /* allocate space for writer */
   pipe = malloc(sizeof(BlastHSPPipe));

   /* fill up the function pointers */
   pipe->RunFnPtr = &s_BlastHSPCullingPipeRun;
   pipe->FreeFnPtr= &s_BlastHSPCullingPipeFree;

   /* allocate for data structure */
   pipe->data = malloc(sizeof(BlastHSPCullingData));
   data.params = params;
   data.query_info = query_info;
   memcpy(pipe->data, &data, sizeof(data));
   pipe->next = NULL;
    
   return pipe;
}

/**************************************************************/
/** The following are exported functions to be used by APP    */

BlastHSPCullingParams*
BlastHSPCullingParamsNew(const BlastHitSavingOptions* hit_options,
                         const BlastHSPCullingOptions* culling_opts,
                         Int4 compositionBasedStats,
                         Boolean gapped_calculation)
{
    BlastHSPCollectorParams* collector_params = 
        BlastHSPCollectorParamsNew(hit_options, compositionBasedStats, gapped_calculation);

    BlastHSPCullingParams* retval = NULL;
    retval = (BlastHSPCullingParams*) malloc(sizeof(BlastHSPCullingParams));
    retval->culling_max = culling_opts->max_hits;

    retval->prelim_hitlist_size = collector_params->prelim_hitlist_size;
    retval->hsp_num_max = collector_params->hsp_num_max;
    retval->program = collector_params->program;
    collector_params = BlastHSPCollectorParamsFree(collector_params);

    return retval;
}

BlastHSPCullingParams*
BlastHSPCullingParamsFree(BlastHSPCullingParams* opts)
{
    if ( !opts )
        return NULL;
    sfree(opts);
    return NULL;
}

BlastHSPWriterInfo*
BlastHSPCullingInfoNew(BlastHSPCullingParams* params) {
    BlastHSPWriterInfo * writer_info =
                         malloc(sizeof(BlastHSPWriterInfo));
    writer_info->NewFnPtr = &s_BlastHSPCullingNew;
    writer_info->params = params;
    return writer_info;
}

BlastHSPPipeInfo*
BlastHSPCullingPipeInfoNew(BlastHSPCullingParams* params) {
    BlastHSPPipeInfo * pipe_info =
                         malloc(sizeof(BlastHSPPipeInfo));
    pipe_info->NewFnPtr = &s_BlastHSPCullingPipeNew;
    pipe_info->params = params;
    pipe_info->next = NULL;
    return pipe_info;
}
