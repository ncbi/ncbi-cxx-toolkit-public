/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_gapalign.c

Author: Ilya Dondoshansky

Contents: Functions to perform gapped alignment

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_util.h> /* for READDB_UNPACK_BASE macros */
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/greedy_align.h>

static Int2 BLAST_DynProgNtGappedAlignment(BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastGapAlignStruct* gap_align, 
   const BlastScoringOptions* score_options, BlastInitHSP* init_hsp);
static Int4 BLAST_AlignPackedNucl(Uint1* B, Uint1* A, Int4 N, Int4 M, 
   Int4* pej, Int4* pei, BlastGapAlignStruct* gap_align,
   const BlastScoringOptions* score_options, Boolean reverse_sequence);
static Int2
BLAST_GapAlignStructFill(BlastGapAlignStruct* gap_align, Int4 q_start, 
   Int4 s_start, Int4 q_end, Int4 s_end, Int4 score, GapEditScript* esp);
static Int2 
BLAST_SaveHsp(BlastGapAlignStruct* gap_align, BlastInitHSP* init_hsp, 
   BlastHSPList* hsp_list, const BlastHitSavingOptions* hit_options, 
   Int2 frame);

static Int2 BLAST_ProtGappedAlignment(Uint1 program, 
   BLAST_SequenceBlk* query_in, BLAST_SequenceBlk* subject_in,
   BlastGapAlignStruct* gap_align,
   const BlastScoringOptions* score_options, BlastInitHSP* init_hsp);

typedef struct GapData {
  BlastGapDP* CD;
  Int4** v;
  Int4* sapp;                 /* Current script append ptr */
  Int4  last;
  Int4 h,  g;
} GapData;

/** Append "Delete k" op */
#define DEL_(k) \
data.last = (data.last < 0) ? (data.sapp[-1] -= (k)) : (*data.sapp++ = -(k));

/** Append "Insert k" op */
#define INS_(k) \
data.last = (data.last > 0) ? (data.sapp[-1] += (k)) : (*data.sapp++ = (k));

/** Append "Replace" op */
#define REP_ \
{data.last = *data.sapp++ = 0;}

/* Divide by two to prevent underflows. */
#define MININT INT4_MIN/2
#define REPP_ \
{*data.sapp++ = MININT; data.last = 0;}

/** Are the two HSPs within a given number of diagonals from each other? */
#define MB_HSP_CLOSE(q1, q2, s1, s2, c) \
(ABS((q1-s1) - (q2-s2)) < c)

/** Is one HSP contained in a diagonal strip around another? */
#define MB_HSP_CONTAINED(qo1,qo2,qe2,so1,so2,se2,c) \
(qo1>=qo2 && qo1<=qe2 && so1>=so2 && so1<=se2 && \
MB_HSP_CLOSE(qo1,qo2,so1,so2,c))

/** TRUE if c is between a and b; f between d and f. Determines if the
 * coordinates are already in an HSP that has been evaluated. 
 */
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Callback for sorting HSPs by starting offset in query */ 
static int
query_offset_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

	if (h1->query.offset < h2->query.offset)
		return -1;
	if (h1->query.offset > h2->query.offset)
		return 1;

	return 0;
}

/** Callback for sorting HSPs by ending offset in query */
static int
query_end_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

	if (h1->query.end < h2->query.end)
		return -1;
	if (h1->query.end > h2->query.end)
		return 1;

	return 0;
}

/** Callback for sorting HSPs by score */
static int
score_compare_hsp(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;
        
        /* NULL HSPs are moved to the end */
	if (h1 == NULL)
           return 1;
        if (h2 == NULL)
           return -1;

	if (h1->score < h2->score) 
		return 1;
	if (h1->score > h2->score)
		return -1;

	return 0;
}

/** Check the gapped alignments for an overlap of two different alignments.
 * A sufficient overlap is when two alignments have the same start values
 * of have the same final values. 
 * @param hsp_array Pointer to an array of BlastHSP structures [in]
 * @param hsp_count The size of the hsp_array [in]
 * @param frame What frame these HSPs are from? [in]
 * @return The number of valid alignments remaining. 
*/
static Int4
CheckGappedAlignmentsForOverlap(BlastHSP* *hsp_array, Int4 hsp_count, Int2 frame)

{
   Int4 index1, index, increment;

   if (*hsp_array == NULL || hsp_count == 0)
      return 0;
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), query_offset_compare_hsps);
   index=0;
   increment=1;
   while (index < hsp_count-increment) {
      /* Check if both HSP's start on or end on the same digonal. */
      if (hsp_array[index+increment] == NULL) {
	 increment++;
	 continue;
      }
      
      if (frame != 0 && hsp_array[index+increment]->subject.frame != frame)
	 break;
      
      if (hsp_array[index] && hsp_array[index]->query.offset == hsp_array[index+increment]->query.offset &&
	  hsp_array[index]->subject.offset == hsp_array[index+increment]->subject.offset &&
	  SIGN(hsp_array[index]->query.frame) == SIGN(hsp_array[index+increment]->query.frame))
      {
	 if (hsp_array[index]->score > hsp_array[index+increment]->score) {
	   sfree(hsp_array[index+increment]);
	    increment++;
	 } else {
	    sfree(hsp_array[index]);
	    index++;
	    increment = 1;
	 }
      } else {
	 index++;
	 increment = 1;
      }
   }
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), query_end_compare_hsps);
   index=0;
   increment=1;
   while (index < hsp_count-increment)
   { /* Check if both HSP's start on or end on the same digonal. */
      if (hsp_array[index+increment] == NULL)
      {
	 increment++;
	 continue;
      }
      
      if (frame != 0 && hsp_array[index+increment]->subject.frame != frame)
	 break;
      
      if (hsp_array[index] &&
	  hsp_array[index]->query.end == hsp_array[index+increment]->query.end &&
	  hsp_array[index]->subject.end == hsp_array[index+increment]->subject.end &&
	  SIGN(hsp_array[index]->query.frame) == SIGN(hsp_array[index+increment]->query.frame))
      {
	 if (hsp_array[index]->score > hsp_array[index+increment]->score)
	 {
          sfree(hsp_array[index+increment]);
	    increment++;
	 } else	{
	    sfree(hsp_array[index]);
	    index++;
	    increment = 1;
	 }
      } else {
	 index++;
	 increment = 1;
      }
   }

   qsort(hsp_array,hsp_count,sizeof(BlastHSP*), score_compare_hsp);
   
   index1 = 0;
   for (index=0; index<hsp_count; index++)
   {
      if (hsp_array[index] != NULL)
	 index1++;
   }

   return index1;
}

/** Retrieve the state structure corresponding to a given length
 * @param head Pointer to the first element of the state structures 
 *        array [in]
 * @param length The length for which the state structure has to be 
 *        found [in]
 * @return The found or created state structure
 */
#define	CHUNKSIZE	2097152
static GapStateArrayStruct*
GapGetState(GapStateArrayStruct** head, Int4 length)

{
   GapStateArrayStruct*	retval,* var,* last;
   Int4	chunksize = MAX(CHUNKSIZE, length + length/3);

   length += length/3;	/* Add on about 30% so the end will get reused. */
   retval = NULL;
   if (*head == NULL) {
      retval = (GapStateArrayStruct*) 
         malloc(sizeof(GapStateArrayStruct));
      retval->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
      retval->length = chunksize;
      retval->used = 0;
      retval->next = NULL;
      *head = retval;
   } else {
      var = *head;
      last = *head;
      while (var) {
         if (length < (var->length - var->used)) {
            retval = var;
            break;
         } else if (var->used == 0) { 
            /* If it's empty and too small, replace. */
            sfree(var->state_array);
            var->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
            var->length = chunksize;
            retval = var;
            break;
         }
         last = var;
         var = var->next;
      }
      
      if (var == NULL)
      {
         retval = (GapStateArrayStruct*) malloc(sizeof(GapStateArrayStruct));
         retval->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
         retval->length = chunksize;
         retval->used = 0;
         retval->next = NULL;
         last->next = retval;
      }
   }

#ifdef ERR_POST_EX_DEFINED
   if (retval->state_array == NULL)
      ErrPostEx(SEV_ERROR, 0, 0, "state array is NULL");
#endif
		
   return retval;

}

/** Remove a state from a state structure */
static Boolean
GapPurgeState(GapStateArrayStruct* state_struct)
{
   while (state_struct)
   {
      /*
	memset(state_struct->state_array, 0, state_struct->used);
      */
      state_struct->used = 0;
      state_struct = state_struct->next;
   }
   
   return TRUE;
}

/** Greatest common divisor */
static Int4 gcd(Int4 a, Int4 b)
{
    Int4 c;
    if (a < b) {
        c = a;
        a = b; b = c;
    }
    while ((c = a % b) != 0) {
        a = b; b = c;
    }

    return b;
}

/** Divide 3 numbers by their greatest common divisor
 * @return The GCD
 */
static Int4 gdb3(Int4* a, Int4* b, Int4* c)
{
    Int4 g;
    if (*b == 0) g = gcd(*a, *c);
    else g = gcd(*a, gcd(*b, *c));
    if (g > 1) {
        *a /= g;
        *b /= g;
        *c /= g;
    }
    return g;
}

/** Deallocate the memory for greedy gapped alignment */
static GreedyAlignMem* BLAST_GreedyAlignsfree(GreedyAlignMem* gamp)
{
   if (gamp->flast_d) {
      sfree(gamp->flast_d[0]);
      sfree(gamp->flast_d);
   } else {
      if (gamp->flast_d_affine) {
         sfree(gamp->flast_d_affine[0]);
         sfree(gamp->flast_d_affine);
      }
      sfree(gamp->uplow_free);
   }
   sfree(gamp->max_row_free);
   if (gamp->space)
      MBSpaceFree(gamp->space);
   sfree(gamp);
   return NULL;
}

/** Allocate memory for the greedy gapped alignment algorithm
 * @param score_options Options related to scoring [in]
 * @param ext_params Options and parameters related to the extension [in]
 * @param max_dbseq_length The length of the longest sequence in the 
 *        database [in]
 * @return The allocated GreedyAlignMem structure
 */
static GreedyAlignMem* 
BLAST_GreedyAlignMemAlloc(const BlastScoringOptions* score_options,
		       BlastExtensionParameters* ext_params,
		       Int4 max_dbseq_length)
{
   GreedyAlignMem* gamp;
   Int4 max_d, max_d_1, Xdrop, d_diff, max_cost, gd, i;
   Int4 reward, penalty, gap_open, gap_extend;
   Int4 Mis_cost, GE_cost;
   Boolean do_traceback;
   
   if (score_options == NULL || ext_params == NULL) 
      return NULL;
   
   do_traceback = 
      (ext_params->options->algorithm_type != EXTEND_GREEDY_NO_TRACEBACK);

   if (score_options->reward % 2 == 1) {
      reward = 2*score_options->reward;
      penalty = -2*score_options->penalty;
      Xdrop = 2*ext_params->gap_x_dropoff;
      gap_open = 2*score_options->gap_open;
      gap_extend = 2*score_options->gap_extend;
   } else {
      reward = score_options->reward;
      penalty = -score_options->penalty;
      Xdrop = ext_params->gap_x_dropoff;
      gap_open = score_options->gap_open;
      gap_extend = score_options->gap_extend;
   }

   if (gap_open == 0 && gap_extend == 0)
      gap_extend = reward / 2 + penalty;

   max_d = (Int4) (max_dbseq_length / ERROR_FRACTION + 1);

   gamp = (GreedyAlignMem*) calloc(1, sizeof(GreedyAlignMem));

   if (score_options->gap_open==0 && score_options->gap_extend==0) {
      d_diff = ICEIL(Xdrop+reward/2, penalty+reward);
   
      gamp->flast_d = (Int4**) malloc((max_d + 2) * sizeof(Int4*));
      if (gamp->flast_d == NULL) {
         sfree(gamp);
         return NULL;
      }
      gamp->flast_d[0] = 
         (Int4*) malloc((max_d + max_d + 6) * sizeof(Int4) * 2);
      if (gamp->flast_d[0] == NULL) {
#ifdef ERR_POST_EX_DEFINED
	 ErrPostEx(SEV_WARNING, 0, 0, 
              "Failed to allocate %ld bytes for greedy alignment", 
              (max_d + max_d + 6) * sizeof(Int4) * 2);
#endif
         BLAST_GreedyAlignsfree(gamp);
         return NULL;
      }

      gamp->flast_d[1] = gamp->flast_d[0] + max_d + max_d + 6;
      gamp->flast_d_affine = NULL;
      gamp->uplow_free = NULL;
   } else {
      gamp->flast_d = NULL;
      Mis_cost = reward + penalty;
      GE_cost = gap_extend+reward/2;
      max_d_1 = max_d;
      max_d *= GE_cost;
      max_cost = MAX(Mis_cost, gap_open+GE_cost);
      gd = gdb3(&Mis_cost, &gap_open, &GE_cost);
      d_diff = ICEIL(Xdrop+reward/2, gd);
      gamp->uplow_free = (Int4*) calloc(2*(max_d+1+max_cost), sizeof(Int4));
      gamp->flast_d_affine = (ThreeVal**) 
	 malloc((MAX(max_d, max_cost) + 2) * sizeof(ThreeVal*));
      if (!gamp->uplow_free || !gamp->flast_d_affine) {
         BLAST_GreedyAlignsfree(gamp);
         return NULL;
      }
      gamp->flast_d_affine[0] = (ThreeVal*)
	 calloc((2*max_d_1 + 6) , sizeof(ThreeVal) * (max_cost+1));
      for (i = 1; i <= max_cost; i++)
	 gamp->flast_d_affine[i] = 
	    gamp->flast_d_affine[i-1] + 2*max_d_1 + 6;
      if (!gamp->flast_d_affine || !gamp->flast_d_affine[0])
         BLAST_GreedyAlignsfree(gamp);
   }
   gamp->max_row_free = (Int4*) malloc(sizeof(Int4) * (max_d + 1 + d_diff));

   if (do_traceback)
      gamp->space = MBSpaceNew();
   if (!gamp->max_row_free || (do_traceback && !gamp->space))
      /* Failure in one of the memory allocations */
      BLAST_GreedyAlignsfree(gamp);

   return gamp;
}

/** Deallocate the BlastGapAlignStruct structure */
BlastGapAlignStruct* 
BLAST_GapAlignStructFree(BlastGapAlignStruct* gap_align)
{
   GapEditBlockDelete(gap_align->edit_block);
   sfree(gap_align->dyn_prog);
   if (gap_align->greedy_align_mem)
      BLAST_GreedyAlignsfree(gap_align->greedy_align_mem);
   GapStateFree(gap_align->state_struct);

   sfree(gap_align);
   return NULL;
}

/** Documented in blast_gapalign.h */
Int2
BLAST_GapAlignStructNew(const BlastScoringOptions* score_options, 
   BlastExtensionParameters* ext_params, 
   Uint4 max_subject_length, Int4 query_length, 
   BlastScoreBlk* sbp, BlastGapAlignStruct** gap_align_ptr)
{
   Int2 status = 0;
   BlastGapAlignStruct* gap_align;

   if (!gap_align_ptr || !sbp || !score_options || !ext_params)
      return -1;

   gap_align = (BlastGapAlignStruct*) calloc(1, sizeof(BlastGapAlignStruct));

   *gap_align_ptr = gap_align;

   gap_align->sbp = sbp;

   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff;

   if (ext_params->options->algorithm_type == EXTEND_DYN_PROG) {
      gap_align->dyn_prog = (BlastGapDP*) 
         malloc((query_length+2)*sizeof(BlastGapDP));
      if (!gap_align->dyn_prog)
         gap_align = BLAST_GapAlignStructFree(gap_align);
   } else {
      max_subject_length = MIN(max_subject_length, MAX_DBSEQ_LEN);
      gap_align->greedy_align_mem = 
         BLAST_GreedyAlignMemAlloc(score_options, ext_params, 
                                max_subject_length);
      if (!gap_align->greedy_align_mem)
         gap_align = BLAST_GapAlignStructFree(gap_align);
   }

   gap_align->positionBased = (sbp->posMatrix != NULL);

   return status;
}

/** Low level function to perform dynamic programming gapped extension 
 * with traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param pei Resulting starting offset in query [out]
 * @param pej Resulting starting offset in subject [out]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @return The best alignment score found.
*/
static Int4
ALIGN_EX(Uint1* A, Uint1* B, Int4 M, Int4 N, Int4* S, Int4* pei, 
	Int4* pej, Int4** sapp, BlastGapAlignStruct* gap_align, 
	const BlastScoringOptions* score_options, Int4 query_offset, 
        Boolean reversed, Boolean reverse_sequence)
	
{ 
  GapData data;
  Int4 i, j, cb,  j_r, s, k;
  Uint1 st, std, ste;
  Int4 gap_open, gap_extend, decline_penalty;
  Int4 c, d, e, m,t, tt, f, tt_start;
  Int4 best_score = 0;
  Int4* wa;
  BlastGapDP* dp,* dyn_prog;
  Uint1** state,* stp,* tmp;
  Uint1* state_array;
  Int4* *matrix;
  Int4 X;
  GapStateArrayStruct* state_struct;
  Int4 next_c;
  Uint1* Bptr;
  Int4 B_increment=1;
  Int4 align_len;
  
  matrix = gap_align->sbp->matrix;
  *pei = *pej = 0;
  data.sapp = *sapp = S;
  data.last= 0;
  m = score_options->gap_open + score_options->gap_extend;
  decline_penalty = score_options->decline_align;

  gap_open = score_options->gap_open;
  gap_extend = score_options->gap_extend;
  X = gap_align->gap_x_dropoff;

  if (X < m)
	X = m;

  if(N <= 0 || M <= 0) { 
    *pei = *pej;
    return 0;
  }

  GapPurgeState(gap_align->state_struct);

  j = (N + 2) * sizeof(BlastGapDP);
  if (gap_align->dyn_prog)
     dyn_prog = gap_align->dyn_prog;
  else
     dyn_prog = (BlastGapDP*)malloc(j);

  state = (Uint1**) malloc(sizeof(Uint1*)*(M+1));
  dyn_prog[0].CC = 0;
  dyn_prog[0].FF = -m - decline_penalty;
  c = dyn_prog[0].DD = -m;

  /* Protection against divide by zero. */
  if (gap_extend > 0)
  	state_struct = GapGetState(&gap_align->state_struct, X/gap_extend+5);
  else
	state_struct = GapGetState(&gap_align->state_struct, N+3);

  state_array = state_struct->state_array;
  state[0] = state_array;
  stp  = state[0];
  for(i = 1; i <= N; i++) {
    if(c < -X) break;
    dyn_prog[i].CC = c;
    dyn_prog[i].DD = c - m; 
    dyn_prog[i].FF = c - m - decline_penalty;
    c -= gap_extend;
    stp[i] = 1;
  }
  state_struct->used = i+1;
  
  if(reverse_sequence)
    B_increment=-1;
  else
    B_increment=1;
 
  tt = 0;  j = i;
  for(j_r = 1; j_r <= M; j_r++) {
     /* Protection against divide by zero. */
    if (gap_extend > 0)
    	state_struct = GapGetState(&gap_align->state_struct, j-tt+5+X/gap_extend);
    else
	state_struct = GapGetState(&gap_align->state_struct, N-tt+3);
    state_array = state_struct->state_array + state_struct->used + 1;
    state[j_r] = state_array - tt + 1;
    stp = state[j_r];
    tt_start = tt; 
    if (!(gap_align->positionBased)){ /*AAS*/
      if(reverse_sequence)
        wa = matrix[A[M-j_r]];
      else
        wa = matrix[A[j_r]];
      }
    else {
      if(reversed || reverse_sequence)
        wa = gap_align->sbp->posMatrix[M - j_r];
      else
        wa = gap_align->sbp->posMatrix[j_r + query_offset];
    }
    e = c = f= MININT;
    Bptr = &B[tt];
    if(reverse_sequence)
      Bptr = &B[N-tt];

    for (cb = i = tt, dp = &dyn_prog[i]; i < j; i++) {
        Int4 next_f;
        d = (dp)->DD;
        Bptr += B_increment;
        next_c = dp->CC+wa[*Bptr];   /* Bptr is & B[i+1]; */
        next_f = dp->FF;
	st = 0;
	if (c < f) {c = f; st = 3;}
	if (f > d) {d = f; std = 60;} 
	else {
	  std = 30;
	  if (c < d) { c= d;st = 2;}
	}
	if (f > e) {e = f; ste = 20;} 
	else {
	  ste = 10;
	  if (c < e) {c=e; st=1;}
	}
	if (best_score - c > X){
	  if (tt == i) tt++;
	  else { dp->CC =  MININT; }
	} else {
	  cb = i;
	  if (c > best_score) {
	    best_score = c;
	    *pei = j_r; *pej = i;
	  }
	  if ((c-=m) > (d-=gap_extend)) {
	    dp->DD = c; 
	  } else {
	    dp->DD = d;
	    st+=std;
	  } 
	  if (c > (e-=gap_extend)) {
	    e = c; 
	  }  else {
	    st+=ste;
	  }
	  c+=m; 
	  if (f < c-gap_open) { 
	    dp->FF = c-gap_open-decline_penalty; 
	  } else {
	    dp->FF = f-decline_penalty; st+= 5; 
	  }
	  dp->CC = c;
	}
	stp[i] = st;
	c = next_c;
	f = next_f;
	dp++;
    }
    if (tt == j) { j_r++; break;}
    if (cb < j-1) { j = cb+1;}
    else {
	while (e >= best_score-X && j <= N) {
	    dyn_prog[j].CC = e; dyn_prog[j].DD = e-m; dyn_prog[j].FF = e-gap_open-decline_penalty;
	    e -= gap_extend; stp[j] = 1;
	    j++;
	}
    }
    if (j <= N) {
	dyn_prog[j].DD = dyn_prog[j].CC= dyn_prog[j].FF = MININT;
	j++; 
    }
    state_struct->used += (MAX(i, j) - tt_start + 1);
  }
  i = *pei; j = *pej;
  tmp = (Uint1*) malloc(i+j);
  for (s=0, c = 0; i> 0 || j > 0; c++) {
      t = state[i][j];
      k  = t %5;
      if (s == 1) {
	  if ((t/10)%3 == 1) k = 1;
	  else if ((t/10)%3 == 2) k = 3;
      }
      if (s == 2) {
	  if ((t/30) == 1) k = 2;
	  else if ((t/30) == 2) k = 3;
      }
      if (s == 3 && ((t/5)%2) == 1) k = 3;
      if (k == 1) { j--;}
      else if (k == 2) {i--;}
      else {j--; i--;}
      tmp[c] = s = k;
  }

  align_len = c;
  c--;
  while (c >= 0) {
      if (tmp[c] == 0) REP_
      else if (tmp[c] == 1) INS_(1)
      else if (tmp[c] == 3) REPP_			  
      else DEL_(1)     
      c--;
  }

  sfree(tmp);

  sfree(state);

  if (!gap_align->dyn_prog)
     sfree(dyn_prog);

  if ((align_len -= data.sapp - S) > 0)
     memset(data.sapp, 0, align_len);
  *sapp = data.sapp;

  return best_score;
}

/** Low level function to perform gapped extension in one direction with 
 * or without traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param pei Resulting starting offset in query [out]
 * @param pej Resulting starting offset in subject [out]
 * @param score_only Only find the score, without saving traceback [in]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @return The best alignment score found.
 */
static Int4 SEMI_G_ALIGN_EX(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* pei, Int4* pej, Boolean score_only, Int4** sapp,
   BlastGapAlignStruct* gap_align, const BlastScoringOptions* score_options, 
   Int4 query_offset, Boolean reversed, Boolean reverse_sequence)
{
  BlastGapDP* dyn_prog;
  Int4 i, j, cb, j_r, g, decline_penalty;
  Int4 c, d, e, m, tt, h, X, f;
  Int4 best_score = 0;
  Int4* *matrix;
  Int4* wa;
  BlastGapDP* dp;
  Uint1* Bptr;
  Int4 B_increment=1;
  Int4 next_c, next_f;

  if(!score_only) {
    return ALIGN_EX(A, B, M, N, S, pei, pej, sapp, gap_align, score_options, 
                    query_offset, reversed, reverse_sequence);
  }
  
  matrix = gap_align->sbp->matrix;
  *pei = *pej = 0;
  m = (g=score_options->gap_open) + score_options->gap_extend;
  h = score_options->gap_extend;
  decline_penalty = score_options->decline_align;

  X = gap_align->gap_x_dropoff;

  if (X < m)
	X = m;

  if(N <= 0 || M <= 0) return 0;

  j = (N + 2) * sizeof(BlastGapDP);

  dyn_prog = (BlastGapDP*)malloc(j);

  dyn_prog[0].CC = 0; c = dyn_prog[0].DD = -m;
  dyn_prog[0].FF = -m - decline_penalty;
  for(i = 1; i <= N; i++) {
    if(c < -X) break;
    dyn_prog[i].CC = c;
    dyn_prog[i].DD = c - m; 
    dyn_prog[i].FF = c - m - decline_penalty;
    c -= h;
  }

  if(reverse_sequence)
    B_increment=-1;
  else
    B_increment=1;

  tt = 0;  j = i;
  for (j_r = 1; j_r <= M; j_r++) {
     if ((reverse_sequence && (A[M-j_r] == NULLB)) ||
         (!reverse_sequence && (A[j_r] == NULLB)))
        break;

     if (!(gap_align->positionBased)){ /*AAS*/
        if(reverse_sequence)
           wa = matrix[A[M-j_r]];
        else
           wa = matrix[A[j_r]];
     }
     else {
        if(reversed || reverse_sequence)
           wa = gap_align->sbp->posMatrix[M - j_r];
        else
           wa = gap_align->sbp->posMatrix[j_r + query_offset];
     }
      e = c =f = MININT;
      Bptr = &B[tt];
      if(reverse_sequence)
         Bptr = &B[N-tt];
      for (cb = i = tt, dp = &dyn_prog[i]; i < j; i++) {
         d = dp->DD;
         Bptr += B_increment;
         next_c = dp->CC+wa[*Bptr];   /* Bptr is & B[i+1]; */
         next_f = dp->FF;
         if (c < f) c = f;
         if (f > d) d = f;
         else if (c < d) c= d;

         if (f > e) e = f;
         else if (c < e) c=e;

         if (best_score - c > X){
            if (tt == i) tt++;
            else { dp->CC =  MININT; }
         } else {
            cb = i;
            if (c > best_score) {
               best_score = c;
               *pei = j_r; *pej = i;
            }
            if ((c-=m) > (d-=h)) {
               dp->DD = c; 
            } else {
               dp->DD = d;
            } 
            if (c > (e-=h)) {
               e = c; 
            }
            c+=m; 
            if (f < c-g) { 
               dp->FF = c-g-decline_penalty; 
            } else {
               dp->FF = f-decline_penalty;
            }
            dp->CC = c;
         }
         c = next_c;
         f = next_f;
         dp++;
      }

      if (tt == j) break;
      if (cb < j-1) { j = cb+1;}
      else while (e >= best_score-X && j <= N) {
	  dyn_prog[j].CC = e; dyn_prog[j].DD = e-m;dyn_prog[j].FF = e-g-decline_penalty;
	  e -= h; j++;
      }
      if (j <= N) {
	  dyn_prog[j].DD = dyn_prog[j].CC = dyn_prog[j].FF = MININT; j++;
      }
  }
  

  sfree(dyn_prog);

  return best_score;
}

/** Low level function to perform gapped extension with out-of-frame
 * gapping with traceback 
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param pei Resulting starting offset in query [out]
 * @param pej Resulting starting offset in subject [out]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @return The best alignment score found.
 */
static Int4 OOF_ALIGN(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* pei, Int4* pej, Int4** sapp, 
   BlastGapAlignStruct* gap_align, const BlastScoringOptions* score_options,
   Int4 query_offset, Boolean reversed)
{
  GapData data;
  Int4 i, j, cb,  j_r, s, k, sc, s1, s2, s3, st1, e1, e2, e3, shift;
  Int4 c, d, m,t, tt, tt_start, f1, f2;
  Int4 best_score = 0;
  Int4* wa;
  Int4 count = 0;
  BlastGapDP* dp;
  Uint1** state,* stp,* tmp;
  Uint1* state_array;
  Int4* *matrix;
  Int4 X;
  GapStateArrayStruct* state_struct;
  Int4 factor = 1;
  
  matrix = gap_align->sbp->matrix;
  *pei =0; *pej = 0;
  data.sapp = *sapp = S;
  data.last= 0;
  m = score_options->gap_open + score_options->gap_extend;
  data.g = score_options->gap_open;
  data.h = score_options->gap_extend;
  data.v = matrix;
  X = gap_align->gap_x_dropoff;
  shift = score_options->shift_pen;

  if (X < m)
	X = m;

  if(N <= 0 || M <= 0) { 
    return 0;
  }

  N+=2;
  GapPurgeState(gap_align->state_struct);

  j = (N + 2) * sizeof(BlastGapDP);
  data.CD = (BlastGapDP*)calloc(1, j);

  state = (Uint1**) calloc((M+1), sizeof(Uint1*));
  data.CD[0].CC = 0;
  c = data.CD[0].DD = -m;
  state_struct = GapGetState(&gap_align->state_struct, N+3);
  state_array = state_struct->state_array;
  state[0] = state_array;
  stp  = state[0];
  data.CD[0].CC = 0; c = data.CD[0].DD = -m;
  for(i = 3; i <= N; i+=3) {
    data.CD[i].CC = c;
    data.CD[i].DD = c - m; 
    data.CD[i-1].CC = data.CD[i-2].CC = data.CD[i-1].DD = 
	data.CD[i-2].DD = MININT;
    if(c < -X) break;
    c -= data.h;
    stp[i] = stp[i-1] = stp[i-2] = 6;
  }
  i -= 2;
  data.CD[i].CC = data.CD[i].DD = MININT;
  tt = 0;  j = i;
  state_struct->used = i+1;

  if (!reversed) {
     /* Shift sequence to allow for backwards frame shift */
     B -= 2;
  } else {
     /* Set direction of the coordinates in B */
     factor = -1;
  }

  tt = 0;  j = i;
  for(j_r = 1; j_r <= M; j_r++) {
    count += j - tt; 
    state_struct = GapGetState(&gap_align->state_struct, N-tt+3);
    state_array = state_struct->state_array + state_struct->used + 1;
    state[j_r] = state_array - tt + 1;
    stp = state[j_r];
    tt_start = tt; 
    if (!(gap_align->positionBased)) {
       wa = matrix[A[factor*j_r]]; 
    } else {
      if(reversed)
        wa = gap_align->sbp->posMatrix[M - j_r];
      else
        wa = gap_align->sbp->posMatrix[j_r + query_offset];
    }
    c = MININT; sc =f1=f2=e1 =e2=e3=s1=s2=s3=MININT;
    for(cb = i = tt, dp = &data.CD[i-1]; 1;) {
	if (i >= j) break;
	sc = MAX(MAX(f1, f2)-shift, s3);
	if (sc == s3) st1=3;
	else if (sc+shift == f1) {
	  if (f1 == s2) st1=2; else st1 = 5;
	} else if (f2 == s1) st1 = 1; else st1 = 4;
	sc += wa[B[factor*i]];
   ++i;
	f1 = s3; 
	s3 = (++dp)->CC; f1 = MAX(f1, s3);
	d = dp->DD;
	if (sc < MAX(d, e1)) {
	    if (d > e1) { sc = d; st1 = 30;}
	    else {sc = e1; st1 = 36;}
	    if (best_score -sc > X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		dp->DD = d-data.h;
		e1-=data.h;
	    }
	} else {
	    if (best_score -sc > X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc; *pei = j_r;*pej=i-1;}
		if ((sc-=m) > (e1-=data.h)) e1 = sc; else st1+=10;
		if (sc < (d-=data.h)) { dp->DD = d; st1 += 20;} 
		else dp->DD = sc;
	    }
	}
	stp[i-1] = st1;
	if (i >= j) {c = e1; e1 = e2; e2 = e3; e3 = c; break;}
	sc = MAX(MAX(f1,f2)-shift, s2);
	if (sc == s2) st1=3;
	else if (sc+shift == f1) {
	  if (f1 == s3) st1=1; else st1 = 4;
	} else if (f2 == s1) st1 = 2; else st1 = 5;
	sc += wa[B[factor*i]];
   ++i;
	f2 = s2; s2 = (++dp)->CC; f2 = MAX(f2, s2);
	d = dp->DD;
	if (sc < MAX(d, e2)) {
	    if ((sc=MAX(d,e2)) < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		if (sc == d)  st1= 30; else st1=36;
		cb = i;
		dp->CC = sc;
		dp->DD = d-data.h;
		e2-=data.h;
	    }
	} else {
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc;*pei= j_r; *pej=i-1;}
		if ((sc-=m) > (e2-=data.h)) e2 = sc; else st1+=10;
		if (sc < (d-=data.h)) {dp->DD = d; st1+=20;} 
		else  dp->DD = sc;
	    }
	}
	stp[i-1] = st1;
	if (i >= j) { c = e2; e2 = e1; e1 = e3; e3 = c; break; }
	sc = MAX(MAX(f1, f2)-shift, s1);
	if (sc == s1) st1=3;
	else if (sc+shift == f1) {
	  if (f1 == s3) st1=2; else st1 = 5;
	} else if (f2 == s2) st1 = 1; else st1 = 4;
	sc += wa[B[factor*i]];
   ++i;
	f1 = f2;
	f2 = s1; s1 = (++dp)->CC; f2 = MAX(f2, s1);
	d = dp->DD;
	if (sc < MAX(d, e3)) {
	    sc = MAX(d, e3);
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		if (sc == d) st1 = 30; else st1 = 36;
		cb = i;
		dp->CC = sc;
		dp->DD = d-data.h;
		e3-=data.h;
	    }
	} else {
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc;*pei = j_r; *pej=i-1;}
		if ((sc-=m) > (e3-=data.h)) e3 = sc; else st1 += 10;
		if (sc < (d-=data.h)) {dp->DD = d; st1 += 20;}
		else dp->DD = sc;
	    }
	}
	stp[i-1] = st1;
	sc = c; 
    }
    if(tt == j) break;
    if(cb < j) { j = cb;}
    else {
	c = (MAX(e1, MAX(e2, e3))+X-best_score)/data.h+j;
	if (c > N) c = N;
	if (c > j)
	while (1) {
	    data.CD[j].CC = e1;
	    stp[j] = 36;
	    data.CD[j++].DD = e1 - m; e1 -=data.h;
	    if (j > c) break;      
	    data.CD[j].CC = e2; stp[j] = 36;
	    data.CD[j++].DD = e2- m; e2-=data.h;
	    if (j > c) break;      
	    data.CD[j].CC = e3; stp[j] = 36;
	    data.CD[j++].DD = e3- m; e3-=data.h;
	    if (j > c) break;
	}
    }
    c = j+4;
    if (c > N+1) c = N+1;
    while (j < c) {
	data.CD[j].DD = data.CD[j].CC = MININT;
	j++;
    }

    state_struct->used += (MAX(i, j) - tt_start + 1);
  }

  i = *pei; j = *pej;
  /* printf("best = %d i,j=%d %d\n", best_score, i, j); */
  tmp = (Uint1*) calloc(1, i + j);        
  for (s= 1, c= 0; i > 0 || j > 0; c++, i--) {
      k  = (t=state[i][j])%10;
      if (s == 6 && (t/10)%2 == 1) k = 6;
      if (s == 0 && (t/20)== 1) k = 0;
      if (k == 6) { j -= 3; i++;}
      else {j -= k;}
      s = tmp[c] = k;
  }
  c--; 
  while(c >= 0) {
      *data.sapp++ = tmp[c--];
  }

  if (!reversed)
     /* Sequence was shifted backwards, so length must be adjusted */
     *pej -= 2;

  sfree(tmp);

  sfree(state);

  sfree(data.CD);
  *sapp = data.sapp;

  return best_score;
}

/** Low level function to perform gapped extension with out-of-frame
 * gapping with or without traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param pei Resulting starting offset in query [out]
 * @param pej Resulting starting offset in subject [out]
 * @param score_only Only find the score, without saving traceback [in]
 * @param sapp the traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @return The best alignment score found.
 */
static Int4 OOF_SEMI_G_ALIGN(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* pei, Int4* pej, Boolean score_only, Int4** sapp, 
   BlastGapAlignStruct* gap_align, const BlastScoringOptions* score_options,
   Int4 query_offset, Boolean reversed)
{
  BlastGapDP* CD;
  Int4 i, j, cb, j_r;
  Int4 e1, e2, e3, s1, s2, s3, shift;
  Int4 c, d, sc, m, tt, h, X, f1, f2;
  Int4 best_score = 0, count = 0;
  Int4* *matrix;
  Int4* wa;
  BlastGapDP* dp;
  Int4 factor = 1;
  Int4 NN;
  
  if(!score_only)
      return OOF_ALIGN(A, B, M, N, S, pei, pej, sapp, gap_align, score_options,
                       query_offset, reversed);
  
  matrix = gap_align->sbp->matrix;
  *pei = 0; *pej = -2;
  m = score_options->gap_open + score_options->gap_extend;
  h = score_options->gap_extend;
  X = gap_align->gap_x_dropoff;
  shift = score_options->shift_pen;

  if(!reversed) {
     /* Allow for a backwards frame shift */
     B -= 2;
  } else {
     /* Set the direction for the coordinates in B */
     factor = -1;
  }

  if (X < m)
	X = m;

  if(N <= 0 || M <= 0) return 0;
  NN = N + 2;

  j = (N + 5) * sizeof(BlastGapDP);
  CD = (BlastGapDP*)calloc(1, j);
  CD[0].CC = 0; c = CD[0].DD = -m;
  for(i = 3; i <= NN; i+=3) {
    CD[i].CC = c;
    CD[i].DD = c - m; 
    CD[i-1].CC = CD[i-2].CC = CD[i-1].DD = CD[i-2].DD = MININT;
    if(c < -X) break;
    c -= h;
  }
  i -= 2;
  CD[i].CC = CD[i].DD = MININT;
  tt = 0;  j = i;
  for (j_r = 1; j_r <= M; j_r++) {
    count += j - tt; CD[2].CC = CD[2].DD= MININT;
    if (!(gap_align->positionBased)) {
       wa = matrix[A[factor*j_r]];
    } else {
      if(reversed)
        wa = gap_align->sbp->posMatrix[M-j_r];
      else
        wa = gap_align->sbp->posMatrix[j_r + query_offset];
    }
    s1 = s2 = s3 = f1= f2 = MININT; f1=f2=e1 = e2 = e3 = MININT; sc = MININT;
    for(cb = i = tt, dp = &CD[i-1]; 1;) {
	if (i >= j || i > N) break;
        sc = MAX(MAX(f1, f2)-shift, s3)+wa[B[factor*i]];
        ++i;

	f1 = s3; 
	s3 = (++dp)->CC; f1 = MAX(f1, s3);
	d = dp->DD;
	if (sc < MAX(d, e1)) {
	    sc = MAX(d, e1);
	    if (best_score -sc > X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		dp->DD = d-h;
		e1-=h;
	    }
	} else {
	    if (best_score -sc > X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc; *pei = j_r;*pej=i-1;}
		if ((sc-=m) > (e1-=h)) e1 = sc;
		dp->DD = MAX(sc, d-h);
	    }
	}
	if (i >= j) {c = e1; e1 = e2; e2 = e3; e3 = c; break;}
   sc = MAX(MAX(f1, f2)-shift, s2)+wa[B[factor*i]];
   ++i;

	f2 = s2; s2 = (++dp)->CC; f2 = MAX(f2, s2);
	d = dp->DD;
	if (sc < MAX(d, e2)) {
	    if ((c=MAX(d,e2)) < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = c;
		dp->DD = d-h;
		e2-=h;
	    }
	} else {
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc;*pei= j_r; *pej=i-1;}
		if ((sc-=m) > (e2-=h)) e2 = sc;
		dp->DD = MAX(sc, d-h);
	    }
	}
	if (i >= j || i > N) { c = e2; e2 = e1; e1 = e3; e3 = c; break; }

   sc = MAX(MAX(f1, f2)-shift, s1)+wa[B[factor*i]];
   ++i;

	f1 = f2;
	f2 = s1; s1 = (++dp)->CC; f2 = MAX(f2, s1);
	d = dp->DD;
	if (sc < MAX(d, e3)) {
	    sc = MAX(d, e3);
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		dp->DD = d-h;
		e3-=h;
	    }
	} else {
	    if (sc < best_score-X) {
		if (tt == i-1) tt = i;
		else dp->CC = MININT;
	    } else {
		cb = i;
		dp->CC = sc;
		if (sc > best_score) {best_score = sc;*pei = j_r; *pej=i-1;}
		if ((sc-=m) > (e3-=h)) e3 = sc;
		dp->DD = MAX(sc, d-h);
	    }
	}
	sc = c;
    }
    if(tt == j) break;
    if(cb < j) { j = cb;}
    else {
	c = (MAX(e1, MAX(e2, e3))+X-best_score)/h+j;
	if (c > NN) c = NN;
	if (c > j)
	while (1) {
	    CD[j].CC = e1;
	    CD[j++].DD = e1 - m; e1 -=h;
	    if (j > c) break;      
	    CD[j].CC = e2;
	    CD[j++].DD = e2- m; e2-=h;
	    if (j > c) break;      
	    CD[j].CC = e3;
	    CD[j++].DD = e3- m; e3-=h;
	    if (j > c) break;
	}
    }
    c = j+4;
    if (c > NN+1) c = NN+1;
    while (j < c) {
	CD[j].DD = CD[j].CC = MININT;
	j++;
    }
  }
  
  if (!reversed)
     /* The sequence was shifted, so length should be adjusted as well */
     *pej -= 2;

  sfree(CD);

  return best_score;
}


/** Callback function for a sorting of initial HSPs by diagonal */
static int
diag_compare_match(const void* v1, const void* v2)
{
   BlastInitHSP* h1,* h2;

   h1 = *((BlastInitHSP**) v1);
   h2 = *((BlastInitHSP**) v2);
   
   if (!h1 || !h2)
      return 0;

   return (h1->q_off - h1->s_off) - (h2->q_off - h2->s_off);
}

Int2 BLAST_MbGetGappedScore(Uint1 program_number, 
        BLAST_SequenceBlk* query, 
			    BLAST_SequenceBlk* subject,
			    BlastGapAlignStruct* gap_align,
			    const BlastScoringOptions* score_options, 
			    BlastExtensionParameters* ext_params,
			    BlastHitSavingParameters* hit_params,
			    BlastInitHitList* init_hitlist,
			    BlastHSPList** hsp_list_ptr)
{
   const BlastExtensionOptions* ext_options = ext_params->options;
   Int4 index, i;
   Boolean delete_hsp;
   BlastInitHSP* init_hsp;
   BlastInitHSP** init_hsp_array;
   BlastHSPList* hsp_list;
   const BlastHitSavingOptions* hit_options = hit_params->options;

   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = BlastHSPListNew();
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
     malloc(init_hitlist->total*sizeof(BlastInitHSP*));
   for (index=0; index<init_hitlist->total; ++index)
     init_hsp_array[index] = &init_hitlist->init_hsp_array[index];

   qsort(init_hsp_array, init_hitlist->total, 
            sizeof(BlastInitHSP*), diag_compare_match);

   for (index=0; index<init_hitlist->total; index++) {
      init_hsp = init_hsp_array[index];
      delete_hsp = FALSE;
      for (i = hsp_list->hspcnt - 1; 
           i >= 0 && MB_HSP_CLOSE(init_hsp->q_off, 
              hsp_list->hsp_array[i]->query.offset, init_hsp->s_off, 
              hsp_list->hsp_array[i]->subject.offset, MB_DIAG_NEAR);
           i--) {
         /* Do not extend an HSP already contained in another HSP, unless
            its ungapped score is higher than that HSP's gapped score,
            which indicates wrong starting offset for previously extended HSP.
         */
         if (MB_HSP_CONTAINED(init_hsp->q_off, 
                hsp_list->hsp_array[i]->query.offset, 
                hsp_list->hsp_array[i]->query.end, init_hsp->s_off, 
                hsp_list->hsp_array[i]->subject.offset, 
                hsp_list->hsp_array[i]->subject.end, MB_DIAG_CLOSE) &&
             (!init_hsp->ungapped_data || 
             init_hsp->ungapped_data->score < hsp_list->hsp_array[i]->score)) 
         {
               delete_hsp = TRUE;
               break;
         }
      }
      if (!delete_hsp) {
         Boolean good_hit = TRUE;
            
         BLAST_GreedyGappedAlignment(query->sequence, subject->sequence,
            query->length, subject->length, gap_align, 
            score_options, init_hsp->q_off, init_hsp->s_off, 
            TRUE, (ext_options->algorithm_type == EXTEND_GREEDY ? 
                   TRUE : FALSE));
         /* For neighboring we have a stricter criterion to keep an HSP */
         if (hit_options->is_neighboring) {
            Int4 hsp_length;
               
            hsp_length = 
               MIN(gap_align->query_stop-gap_align->query_start, 
                   gap_align->subject_stop-gap_align->subject_start) + 1;
            if (hsp_length < MIN_NEIGHBOR_HSP_LENGTH || 
                gap_align->percent_identity < MIN_NEIGHBOR_PERC_IDENTITY)
               good_hit = FALSE;
         }
               
         if (good_hit && gap_align->score >= hit_options->cutoff_score) {
            /* gap_align contains alignment endpoints; init_hsp contains 
               the offsets to start the alignment from, if traceback is to 
               be performed later */
            BLAST_SaveHsp(gap_align, init_hsp, hsp_list, hit_options, 1); 
         }
      }
   }

   if (ext_options->algorithm_type != EXTEND_GREEDY_NO_TRACEBACK)
      /* Set the flag that traceback is already done for this HSP list */
      hsp_list->traceback_done = TRUE;

   sfree(init_hsp_array);

   return 0;
}

/** Auxiliary function to transform one style of edit script into 
 *  another. 
 */
static GapEditScript*
MBToGapEditScript (MBGapEditScript* ed_script)
{
   GapEditScript* esp_start = NULL,* esp,* esp_prev = NULL;
   Uint4 i;

   if (ed_script==NULL || ed_script->num == 0)
      return NULL;

   for (i=0; i<ed_script->num; i++) {
      esp = (GapEditScript*) calloc(1, sizeof(GapEditScript));
      esp->num = EDIT_VAL(ed_script->op[i]);
      esp->op_type = 3 - EDIT_OPC(ed_script->op[i]);
      if (esp->op_type == 3)
         fprintf(stderr, "op_type = 3\n");
      if (i==0)
         esp_start = esp_prev = esp;
      else {
         esp_prev->next = esp;
         esp_prev = esp;
      }
   }

   return esp_start;

}

Int2 
BLAST_GreedyGappedAlignment(Uint1* query, Uint1* subject, 
   Int4 query_length, Int4 subject_length, BlastGapAlignStruct* gap_align,
   const BlastScoringOptions* score_options, 
   Int4 q_off, Int4 s_off, Boolean compressed_subject, Boolean do_traceback)
{
   Uint1* q;
   Uint1* s;
   Int4 score;
   Int4 X;
   Int4 q_avail, s_avail;
   /* Variables for the call to MegaBlastGreedyAlign */
   Int4 q_ext_l, q_ext_r, s_ext_l, s_ext_r;
   MBGapEditScript *ed_script_fwd=NULL, *ed_script_rev=NULL;
   Uint1 rem;
   GapEditScript* esp = NULL;
   
   q_avail = query_length - q_off;
   s_avail = subject_length - s_off;
   
   q = query + q_off;
   if (!compressed_subject) {
      s = subject + s_off;
      rem = 4; /* Special value to indicate that sequence is already
                  uncompressed */
   } else {
      s = subject + s_off/4;
      rem = s_off % 4;
   }

   /* Find where positive scoring starts & ends within the word hit */
   score = 0;
   
   X = gap_align->gap_x_dropoff;
   
   if (do_traceback) {
      ed_script_fwd = MBGapEditScriptNew();
      ed_script_rev = MBGapEditScriptNew();
   }
   
   /* extend to the right */
   score = BLAST_AffineGreedyAlign(s, s_avail, q, q_avail, FALSE, X,
              score_options->reward, -score_options->penalty, 
              score_options->gap_open, score_options->gap_extend,
              &s_ext_r, &q_ext_r, gap_align->greedy_align_mem, 
              ed_script_fwd, rem);

   if (compressed_subject)
      rem = 0;

   /* extend to the left */
   score += BLAST_AffineGreedyAlign(subject, s_off, 
               query, q_off, TRUE, X, 
               score_options->reward, -score_options->penalty, 
               score_options->gap_open, score_options->gap_extend, 
               &s_ext_l, &q_ext_l, gap_align->greedy_align_mem, 
               ed_script_rev, rem);

   /* In basic case the greedy algorithm returns number of 
      differences, hence we need to convert it to score */
   if (score_options->gap_open==0 && score_options->gap_extend==0) {
      /* Take advantage of an opportunity to easily calculate percent 
         identity, to avoid parsing the traceback later */
      gap_align->percent_identity = 
         100*(1 - ((double)score) / MIN(q_ext_l+q_ext_r, s_ext_l+s_ext_r));
      score = 
         (q_ext_r + s_ext_r + q_ext_l + s_ext_l)*score_options->reward/2 - 
         score*(score_options->reward - score_options->penalty);
   } else if (score_options->reward % 2 == 1) {
      score /= 2;
   }

   if (do_traceback) {
      MBGapEditScriptAppend(ed_script_rev, ed_script_fwd);
      esp = MBToGapEditScript(ed_script_rev);
   }
   
   MBGapEditScriptFree(ed_script_fwd);
   MBGapEditScriptFree(ed_script_rev);
   
   BLAST_GapAlignStructFill(gap_align, q_off-q_ext_l, 
      s_off-s_ext_l, q_off+q_ext_r, 
      s_off+s_ext_r, score, esp);
   return 0;
}

/** Fills the BlastGapAlignStruct structure with the results of a gapped 
 * extension.
 * @param gap_align the initialized gapped alignment structure [in] [out]
 * @param q_start The starting offset in query [in]
 * @param s_start The starting offset in subject [in]
 * @param q_end The ending offset in query [in]
 * @param s_end The ending offset in subject [in]
 * @param score The alignment score [in]
 * @param esp The edit script containing the traceback information [in]
 */
static Int2
BLAST_GapAlignStructFill(BlastGapAlignStruct* gap_align, Int4 q_start, 
   Int4 s_start, Int4 q_end, Int4 s_end, Int4 score, GapEditScript* esp)
{

   gap_align->query_start = q_start;
   gap_align->query_stop = q_end;
   gap_align->subject_start = s_start;
   gap_align->subject_stop = s_end;
   gap_align->score = score;

   if (esp) {
      gap_align->edit_block = GapEditBlockNew(q_start, s_start);
      gap_align->edit_block->start1 = q_start;
      gap_align->edit_block->start2 = s_start;
      gap_align->edit_block->length1 = q_end - q_start + 1;
      gap_align->edit_block->length2 = s_end - s_start + 1;
      gap_align->edit_block->frame1 = gap_align->edit_block->frame2 = 1;
      gap_align->edit_block->reverse = 0;
      gap_align->edit_block->esp = esp;
   }
   return 0;
}

/** Performs dynamic programming style gapped extension, given an initial 
 * HSP (offset pair), two sequence blocks and scoring and extension options.
 * Note: traceback is not done in this function.
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param init_hsp The initial HSP that needs to be extended [in]
*/
static Int2 BLAST_DynProgNtGappedAlignment(BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastGapAlignStruct* gap_align, 
   const BlastScoringOptions* score_options, BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left, 
      private_q_start, private_s_start;
   Uint1 offset_adjustment;
   Uint1* query,* subject;
   
   found_start = FALSE;
   found_end = FALSE;

   query = query_blk->sequence;
   subject = subject_blk->sequence;
   score_left = 0;
   /* If subject offset is not at the start of a full byte, 
      BLAST_AlignPackedNucl won't work, so shift the alignment start
      to the left */
   offset_adjustment = 
      COMPRESSION_RATIO - (init_hsp->s_off % COMPRESSION_RATIO);
   q_length = init_hsp->q_off + offset_adjustment;
   s_length = init_hsp->s_off + offset_adjustment;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      score_left = BLAST_AlignPackedNucl(query, subject, q_length, s_length, 
                      &private_q_start, &private_s_start, gap_align, 
                      score_options, TRUE);
      if (score_left < 0) 
         return -1;
      gap_align->query_start = q_length - private_q_start;
      gap_align->subject_start = s_length - private_s_start;
   }

   score_right = 0;
   if (q_length < query_blk->length && 
       s_length < subject_blk->length)
   {
      found_end = TRUE;
      score_right = BLAST_AlignPackedNucl(query+q_length-1, 
         subject+(s_length+3)/COMPRESSION_RATIO - 1, 
         query_blk->length-q_length, 
         subject_blk->length-s_length, &(gap_align->query_stop),
         &(gap_align->subject_stop), gap_align, score_options, FALSE);
      if (score_right < 0) 
         return -1;
      gap_align->query_stop += q_length;
      gap_align->subject_stop += s_length;
   }

   if (found_start == FALSE) {
      /* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }

   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
   }

   gap_align->score = score_right+score_left;

   return 0;
}

/* Aligns two nucleotide sequences, one (A) should be packed in the
 * same way as the BLAST databases, the other (B) should contain one
 * basepair/byte. Traceback is not done in this function.
 * @param B The query sequence [in]
 * @param A The subject sequence [in]
 * @param N Maximal extension length in query [in]
 * @param M Maximal extension length in subject [in]
 * @param pej Resulting starting offset in query [out]
 * @param pei Resulting starting offset in subject [out]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param reverse_sequence Reverse the sequence.
 * @return The best alignment score found.
*/
static Int4 BLAST_AlignPackedNucl(Uint1* B, Uint1* A, Int4 N, Int4 M, 
		         Int4* pej, Int4* pei, BlastGapAlignStruct* gap_align,
               const BlastScoringOptions* score_options, 
               Boolean reverse_sequence)
{ 
  BlastGapDP* dyn_prog;
  Int4 i, j, cb, j_r, g, decline_penalty;
  Int4 c, d, e, m, tt, h, X, f;
  Int4 best_score = 0, score;
  Int4* *matrix;
  Int4* wa;
  BlastGapDP* dp;
  Uint1* Bptr;
  Uint1 base_pair;
  Int4 B_increment=1;
  
  matrix = gap_align->sbp->matrix;
  *pei = *pej = 0;
  m = (g=score_options->gap_open) + score_options->gap_extend;
  h = score_options->gap_extend;
  decline_penalty = score_options->decline_align;
  X = gap_align->gap_x_dropoff;

  if (X < m)
     X = m;
  
  if(N <= 0 || M <= 0) return 0;
  
  j = (N + 2) * sizeof(BlastGapDP);
  if (gap_align->dyn_prog)
     dyn_prog = gap_align->dyn_prog;
  else
     dyn_prog = (BlastGapDP*)malloc(j);
  if (!dyn_prog) {
#ifdef ERR_POST_EX_DEFINED
     ErrPostEx(SEV_ERROR, 0, 0, 
               "Cannot allocate %ld bytes for dynamic programming", j);
#endif
     return -1;
  }
  dyn_prog[0].CC = 0; c = dyn_prog[0].DD = -m;
  dyn_prog[0].FF = -m;
  for(i = 1; i <= N; i++) {
     if(c < -X) break;
     dyn_prog[i].CC = c;
     dyn_prog[i].DD = c - m; 
     dyn_prog[i].FF = c - m - decline_penalty;
     c -= h;
  }
  
  if(reverse_sequence)
     B_increment = -1;
  else
     B_increment = 1;
  
  tt = 0;  j = i;
  for (j_r = 1; j_r <= M; j_r++) {
     if(reverse_sequence) {
        base_pair = READDB_UNPACK_BASE_N(A[(M-j_r)/4], ((j_r-1)%4));
        wa = matrix[base_pair];
     } else {
        base_pair = READDB_UNPACK_BASE_N(A[1+((j_r-1)/4)], (3-((j_r-1)%4)));
        wa = matrix[base_pair];
     }
     e = c = f = MININT;
     Bptr = &B[tt];
     if(reverse_sequence)
	Bptr = &B[N-tt];

     for (cb = i = tt, dp = &dyn_prog[i]; i < j; i++) {
        Bptr += B_increment;
        score = wa[*Bptr];
        d = dp->DD;
        if (e < f) e = f;
        if (d < f) d = f;
        if (c < d || c < e) {
           if (d < e) {
              c = e;
           } else {
              c = d; 
           }
           if (best_score - c > X) {
              c = dp->CC + score; f = dp->FF;
              if (tt == i) tt++;
              else { dp->CC = dp->FF = MININT;}
           } else {
              cb = i;
              if ((c-=m) > (d-=h)) {
                 dp->DD = c;
              } else {
                 dp->DD = d;
              }
              if (c > (e-=h)) {
                 e = c;
              }
              c+=m;
              d = dp->CC + score; dp->CC = c; c=d;
              d = dp->FF; dp->FF = f - decline_penalty; f = d;
           }
        } else {
           if (best_score - c > X){
              c = dp->CC + score; f = dp->FF;
              if (tt == i) tt++;
              else { dp->CC = dp->FF = MININT;}
           } else {
              cb = i; 
              if (c > best_score) {
                 best_score = c;
                 *pei = j_r; *pej = i;
              } 
              if ((c-=m) > (d-=h)) {
                 dp->DD = c; 
              } else {
                 dp->DD = d;
              } 
              if (c > (e-=h)) {
                 e = c;
              } 
              c+=m;
              d = dp->FF;
              if (c-g>f) dp->FF = c-g-decline_penalty; 
              else dp->FF = f-decline_penalty;
              f = d;
              d = dp->CC+score; dp->CC = c; c = d;
           }
        }
        if (score == MININT)
           break;
        dp++;
     }
     if (tt == j) break;
     if (cb < j-1) { j = cb+1;}
     else while (e >= best_score-X && j <= N) {
        dyn_prog[j].CC = e; 
        dyn_prog[j].DD = e-m;
        dyn_prog[j].FF = e-g-decline_penalty;
        e -= h; j++;
     }
     if (j <= N) {
        dyn_prog[j].DD = dyn_prog[j].CC = dyn_prog[j].FF = MININT; 
        j++;
     }
  }
  
  if (!gap_align->dyn_prog)
     sfree(dyn_prog);

  return best_score;
}

static void SavePatternLengthInBlastHSP(BlastInitHSP* init_hsp, 
                                        BlastHSP* hsp)
{
   /* Kludge: reuse the variables in the BlastHSP structure that are 
      otherwise unused in PHI BLAST, but have a completely different
      meaning for other program(s). Here we need to save information
   */
   hsp->query.end_trim = init_hsp->ungapped_data->length;
   return;
}

#define BLAST_SAVE_ITER_MAX 20

/** Saves full BLAST HSP information into a BlastHSPList 
 * structure
 * @param gap_align The structure holding gapped alignment information [in]
 * @param init_hsp The initial HSP information [in]
 * @param hsp_list Structure holding all HSPs with full gapped alignment 
 *        information [in] [out]
 * @param hit_options Options related to saving hits [in]
 * @param frame Subject frame: -3..3 for translated sequence, 1 for blastn, 
 *              0 for blastp [in]
 */
static Int2 BLAST_SaveHsp(BlastGapAlignStruct* gap_align, 
   BlastInitHSP* init_hsp, BlastHSPList* hsp_list, 
   const BlastHitSavingOptions* hit_options, Int2 frame)
{
   BlastHSP** hsp_array,* new_hsp;
   Int4 highscore, lowscore, score = 0;
   Int4 hspcnt, hspmax, index, new_index, high_index, old_index, low_index;
   Int4 new_hspmax;

   hspcnt = hsp_list->hspcnt;
   hspmax = hsp_list->allocated;
   
   /* Check if list is already full, then reallocate. */
   if (hspcnt >= hspmax-1 && hsp_list->do_not_reallocate == FALSE)
   {
      new_hspmax = 2*hsp_list->allocated;
      if (hit_options->hsp_num_max)
         new_hspmax = MIN(new_hspmax, hit_options->hsp_num_max);
      if (new_hspmax > hsp_list->allocated) {
         hsp_array = (BlastHSP**)
            realloc(hsp_list->hsp_array, hsp_list->allocated*2*sizeof(BlastHSP*));
         if (hsp_array == NULL)
         {
#ifdef ERR_POST_EX_DEFINED
            ErrPostEx(SEV_WARNING, 0, 0, 
               "UNABLE to reallocate in BLAST_SaveHsp,"
               " continuing with fixed array of %ld HSP's", 
                      (long) hspmax);
#endif
            hsp_list->do_not_reallocate = TRUE; 
         } else {
            hsp_list->hsp_array = hsp_array;
            hsp_list->allocated = new_hspmax;
            hspmax = new_hspmax;
         }
      } else {
         hsp_list->do_not_reallocate = TRUE; 
      }
   }

   hsp_array = hsp_list->hsp_array;

   new_hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));

   if (gap_align) {
      score = gap_align->score;
      new_hsp->query.offset = gap_align->query_start;
      new_hsp->subject.offset = gap_align->subject_start;
      new_hsp->query.length = 
         gap_align->query_stop - gap_align->query_start;
      new_hsp->subject.length = 
         gap_align->subject_stop - gap_align->subject_start;

      new_hsp->query.end = gap_align->query_stop;
      new_hsp->subject.end = gap_align->subject_stop;
      new_hsp->gap_info = gap_align->edit_block;
      gap_align->edit_block = NULL;
   } else if (init_hsp->ungapped_data) {
      score = init_hsp->ungapped_data->score;
      new_hsp->query.offset = init_hsp->ungapped_data->q_start;
      new_hsp->query.end = 
         init_hsp->ungapped_data->q_start + init_hsp->ungapped_data->length;
      new_hsp->subject.offset = init_hsp->ungapped_data->s_start;
      new_hsp->subject.end = 
         init_hsp->ungapped_data->s_start + init_hsp->ungapped_data->length;
      new_hsp->query.length = init_hsp->ungapped_data->length;
      new_hsp->subject.length = init_hsp->ungapped_data->length;
   }

   new_hsp->score = score;
   new_hsp->subject.frame = frame;
   new_hsp->query.gapped_start = MIN(init_hsp->q_off, new_hsp->query.end-1);
   new_hsp->subject.gapped_start = 
      MIN(init_hsp->s_off, new_hsp->subject.end-1);

   if (hit_options->phi_align) {
      SavePatternLengthInBlastHSP(init_hsp, new_hsp);
   }

   /* If we are saving ALL HSP's, simply save and sort later. */
   if (hsp_list->do_not_reallocate == FALSE)
   {
      hsp_array[hsp_list->hspcnt] = new_hsp;
      (hsp_list->hspcnt)++;
      return 0;
   }

   /* Use a binary search to insert the HSP. */

   if (hspcnt != 0) {
      highscore = hsp_array[0]->score;
      lowscore = hsp_array[hspcnt-1]->score;
   } else {
      highscore = 0;
      lowscore = 0;
   }

   if (score >= highscore) {
      new_index = 0;
   } else if (score <= lowscore) {
      new_index = hspcnt;
   } else {
      low_index = 0;
      high_index = hspcnt-1;
      new_index = (low_index+high_index)/2;
      old_index = new_index;
      
      for (index=0; index<BLAST_SAVE_ITER_MAX; index++)
      {
         if (score > hsp_array[new_index]->score)
            high_index = new_index;
         else
            low_index = new_index;
         new_index = (low_index+high_index)/2;
         if (new_index == old_index) { 
            /* Perform this check as new_index get rounded DOWN above.*/
            if (score < hsp_array[new_index]->score)
               new_index++;
            break;
         }
         old_index = new_index;
      }
   }

   if (hspcnt >= hspmax-1) {
      if (new_index >= hspcnt) { 
         /* this HSP is less significant than others on a full list.*/
         sfree(new_hsp);
         return 0;
      } else { /* Delete the last HSP on the list. */
         hspcnt = hsp_list->hspcnt--;
         sfree(hsp_array[hspcnt-1]);
      }
   }
   hsp_list->hspcnt++;
   memmove((hsp_array+new_index+1), (hsp_array+new_index), (hspcnt-new_index)*sizeof(hsp_array[0]));
   hsp_array[new_index] = new_hsp;
   
   return 0;
}

/** Callback for sorting initial HSPs by score. */
static int
score_compare_match(const void* v1, const void* v2)

{
	BlastInitHSP* h1,* h2;

	h1 = *(BlastInitHSP**) v1;
	h2 = *(BlastInitHSP**) v2;

	if (h1 == NULL || h2 == NULL || 
            !h1->ungapped_data || !h2->ungapped_data)
		return 0;

	if (h1->ungapped_data->score < h2->ungapped_data->score) 
		return 1;
	if (h1->ungapped_data->score > h2->ungapped_data->score)
		return -1;

	return 0;
}

#define HSP_MAX_WINDOW 11

/** Function to look for the highest scoring window (of size HSP_MAX_WINDOW)
 * in an HSP and return the middle of this.  Used by the gapped-alignment
 * functions to start the gapped alignments.
 * @param gap_align The structure holding gapped alignment information [in]
 * @param init_hsp The initial HSP information [in]
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @return The offset at which alignment should be started [out]
*/
static Int4 BLAST_GetStartForGappedAlignment (BlastGapAlignStruct* gap_align,
        BlastInitHSP* init_hsp, Uint1* query, Uint1* subject)
{
    Int4 index1, max_offset, score, max_score, hsp_end;
    Uint1* query_var,* subject_var;
    BlastUngappedData* uhsp = init_hsp->ungapped_data;
    
    if (uhsp->length <= HSP_MAX_WINDOW) {
        max_offset = uhsp->q_start + uhsp->length/2;
        return max_offset;
    }

    hsp_end = uhsp->q_start + HSP_MAX_WINDOW;
    query_var = query + uhsp->q_start;
    subject_var = subject + uhsp->s_start;
    score=0;
    for (index1=uhsp->q_start; index1<hsp_end; index1++) {
        if (!(gap_align->positionBased))
            score += gap_align->sbp->matrix[*query_var][*subject_var];
        else
            score += gap_align->sbp->posMatrix[index1][*subject_var];
        query_var++; subject_var++;
    }
    max_score = score;
    max_offset = hsp_end - 1;
    hsp_end = uhsp->q_start + uhsp->length - 1;
    for (index1=uhsp->q_start + HSP_MAX_WINDOW; index1<hsp_end; index1++) {
        if (!(gap_align->positionBased)) {
            score -= gap_align->sbp->matrix[*(query_var-HSP_MAX_WINDOW)][*(subject_var-HSP_MAX_WINDOW)];
            score += gap_align->sbp->matrix[*query_var][*subject_var];
        } else {
            score -= gap_align->sbp->posMatrix[index1-HSP_MAX_WINDOW][*(subject_var-HSP_MAX_WINDOW)];
            score += gap_align->sbp->posMatrix[index1][*subject_var];
        }
        if (score > max_score) {
            max_score = score;
            max_offset = index1;
        }
        query_var++; subject_var++;
    }
    if (max_score > 0)
       max_offset -= HSP_MAX_WINDOW/2;
    else 
       max_offset = uhsp->q_start;

    return max_offset;
}

Int2 BLAST_GetGappedScore (Uint1 program_number, 
        BLAST_SequenceBlk* query, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr)

{
   DoubleInt* helper = NULL;
   Boolean hsp_start_is_contained, hsp_end_is_contained;
   Int4 index, index1, next_offset;
   BlastInitHSP* init_hsp = NULL;
   BlastHSP* hsp1 = NULL;
   Int4 q_start, s_start, q_end, s_end;
   Boolean is_prot;
   Int4 max_offset;
   Int2 status = 0;
   Int2 frame = 0; /* CHANGE!!!!!!!!!!!!!!!!! */
   BlastInitHSP** init_hsp_array = NULL;
   BlastHSPList* hsp_list = NULL;
   double gap_trigger;
   double cutoff_score;
   Boolean further_process = FALSE;
   const BlastHitSavingOptions* hit_options = hit_params->options;

   if (!query || !subject || !gap_align || !score_options || !ext_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return 1;

   if (init_hitlist->total == 0)
      return 0;

   gap_trigger = ext_params->gap_trigger;
   cutoff_score = hit_params->cutoff_score;

   is_prot = (program_number != blast_type_blastn);

   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = BlastHSPListNew();
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
      malloc(init_hitlist->total*sizeof(BlastInitHSP*));

   for (index = 0; index < init_hitlist->total; ++index)
      init_hsp_array[index] = &init_hitlist->init_hsp_array[index];

   qsort(init_hsp_array, init_hitlist->total,
            sizeof(BlastInitHSP*), score_compare_match);

   /* If no initial HSPs passes the e-value threshold so far, check if any 
      would do after gapped alignment, and exit if none are found. 
      Only attempt to extend initial HSPs whose scores are already above 
      gap trigger */
   if (init_hsp_array[0]->ungapped_data && 
       init_hsp_array[0]->ungapped_data->score < cutoff_score) {
      for (index=0; index<init_hitlist->total; index++) {
         init_hsp = init_hsp_array[index];
         if (init_hsp->ungapped_data && 
             init_hsp->ungapped_data->score < gap_trigger)
            break;
         if (is_prot) {
            status =  BLAST_ProtGappedAlignment(program_number, query, 
                         subject, gap_align, score_options, init_hsp);
         } else {
            status = BLAST_DynProgNtGappedAlignment(query, subject, gap_align,
                         score_options, init_hsp);
         }
         if (gap_align->score >= cutoff_score) {
            further_process = TRUE;
            break;
         }
      }
   } else {
      index = 0;
      further_process = TRUE;
   }

   if (!further_process) {
      /* Free the ungapped data */
      for (index = 0; index < init_hitlist->total; ++index) {
            sfree(init_hsp_array[index]->ungapped_data);
      }
      sfree(init_hsp_array);
      return 0;
   }
   
   /* Sort again, if necessary */
   if (index > 0) {
      qsort(init_hsp_array, init_hitlist->total,
               sizeof(BlastInitHSP*), score_compare_match);
   }

   /* helper contains most frequently used information to speed up access. */
   helper = (DoubleInt*) malloc((init_hitlist->total)*sizeof(DoubleInt));

   for (index=0; index<init_hitlist->total; index++)
   {
      hsp_start_is_contained = FALSE;
      hsp_end_is_contained = FALSE;
      init_hsp = init_hsp_array[index];

      /* This prefetches this value for the test below. */
      next_offset = init_hsp->q_off;

      if (!init_hsp->ungapped_data) {
         q_start = q_end = init_hsp->q_off;
         s_start = s_end = init_hsp->s_off;
      } else {
         q_start = init_hsp->ungapped_data->q_start;
         q_end = q_start + init_hsp->ungapped_data->length - 1;
         s_start = init_hsp->ungapped_data->s_start;
         s_end = s_start + init_hsp->ungapped_data->length - 1;
      }

      for (index1=0; index1<hsp_list->hspcnt; index1++)
      {
         hsp_start_is_contained = FALSE;
         hsp_end_is_contained = FALSE;
         
         hsp1 = hsp_list->hsp_array[index1];

         /* Check with the helper array whether further
            tests are warranted.  Having only two ints
            in the helper array speeds up access. */
         if (helper[index1].i1 <= next_offset &&
             helper[index1].i2 >= next_offset)
         {
            if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, q_start,
                hsp1->subject.offset, hsp1->subject.end, s_start) &&
                (!init_hsp->ungapped_data || (SIGN(hsp1->query.frame) == 
                    SIGN(init_hsp->ungapped_data->frame))))
            {
                  hsp_start_is_contained = TRUE;
            }

            if (hsp_start_is_contained && (CONTAINED_IN_HSP(hsp1->query.offset,
                hsp1->query.end, q_end, hsp1->subject.offset, 
                hsp1->subject.end, s_end)))
            {
               hsp_end_is_contained = TRUE;
            }

            if (hsp_start_is_contained && hsp_end_is_contained && 
                (!init_hsp->ungapped_data || 
                 init_hsp->ungapped_data->score <= hsp1->score))
               break;
         }
      }
      
      if (!hsp_start_is_contained || !hsp_end_is_contained || 
          (init_hsp->ungapped_data && hsp1 && 
           init_hsp->ungapped_data->score > hsp1->score)) {
#ifdef NEWBLAST_COLLECT_STATS
         real_gap_number_of_hsps++;
#endif
 
         if(is_prot && !score_options->is_ooframe) {
            max_offset = BLAST_GetStartForGappedAlignment(gap_align, init_hsp,
                            query->sequence, subject->sequence);
            init_hsp->s_off += max_offset - init_hsp->q_off;
            init_hsp->q_off = max_offset;
         }

         if (is_prot) {
            status =  BLAST_ProtGappedAlignment(program_number, query, 
                         subject, gap_align, score_options, init_hsp);
         } else {
            status = BLAST_DynProgNtGappedAlignment(query, subject, gap_align,
                         score_options, init_hsp);
         }

         if (status) {
            sfree(init_hsp_array);
            return status;
         }

         if (!is_prot) {
            /* Check if the alignment has crossed the strand boundary */
            /* THIS SHOULD NEVER HAPPEN SINCE THE MATRIX VALUES ARE SET
               SO THIS BOUNDARIES CAN NEVER BE CROSSED */
            if (gap_align->query_start / query->length != 
                (gap_align->query_stop - 1) / query->length) {
               if (query->length - gap_align->query_start > 
                   gap_align->query_stop - query->length) {
                  gap_align->subject_stop -= 
                     (gap_align->query_stop - query->length + 1);
                  gap_align->query_stop = query->length - 1;
               } else {
                  gap_align->subject_start += 
                     (query->length + 1 - gap_align->query_start);
                  gap_align->query_start = query->length + 1;
               }
            }

         }
         BLAST_SaveHsp(gap_align, init_hsp, hsp_list, hit_options, 
                       subject->frame);

         /* Fill in the helper structure. */
         helper[hsp_list->hspcnt - 1].i1 = gap_align->query_start;
         helper[hsp_list->hspcnt - 1].i2 = gap_align->query_stop;
      }
      /* Free ungapped data here - it's no longer needed */
      sfree(init_hsp->ungapped_data);
   }   

   sfree(init_hsp_array);

   sfree(helper);

   if (is_prot) {
      hsp_list->hspcnt = 
         CheckGappedAlignmentsForOverlap(hsp_list->hsp_array, 
            hsp_list->hspcnt, frame);
   }
   *hsp_list_ptr = hsp_list;
   return status;
}

/** Out-of-frame gapped alignment.
 * @param query Query sequence [in]
 * @param subject Subject sequence [in]
 * @param q_off Offset in query [in]
 * @param s_off Offset in subject [in]
 * @param S Start of the traceback buffer [in]
 * @param private_q_start Extent of alignment in query [out]
 * @param private_s_start Extent of alignment in subject [out]
 * @param score_only Return score only, without traceback [in]
 * @param sapp End of the traceback buffer [out]
 * @param gap_align Gapped alignment information and preallocated 
 *                  memory [in] [out]
 * @param score_options Scoring options [in]
 * @param psi_offset Starting position in PSI-BLAST matrix [in]
 * @param reversed Direction of the extension [in]
 * @param switch_seq Sequences need to be switched for blastx, 
 *                   but not for tblastn [in]
 */
static Int4 
OOF_SEMI_G_ALIGN_EX(Uint1* query, Uint1* subject, Int4 q_off, 
   Int4 s_off, Int4* S, Int4* private_q_start, Int4* private_s_start, 
   Boolean score_only, Int4** sapp, BlastGapAlignStruct* gap_align, 
   const BlastScoringOptions* score_options, Int4 psi_offset, 
   Boolean reversed, Boolean switch_seq)
{
   if (switch_seq) {
      return OOF_SEMI_G_ALIGN(subject, query, s_off, q_off, S, 
                private_s_start, private_q_start, score_only, sapp, 
                gap_align, score_options, psi_offset, reversed);
   } else {
      return OOF_SEMI_G_ALIGN(query, subject, q_off, s_off, S,
                private_q_start, private_s_start, score_only, sapp, 
                gap_align, score_options, psi_offset, reversed);
   }
}

#define MAX_SUBJECT_OFFSET 90000
#define MAX_TOTAL_GAPS 3000

static void 
AdjustSubjectRange(Int4* subject_offset_ptr, Int4* subject_length_ptr, 
		   Int4 query_offset, Int4 query_length, Int4* start_shift)
{
   Int4 s_offset;
   Int4 subject_length = *subject_length_ptr;
   Int4 max_extension_left, max_extension_right;
   
   /* If subject sequence is not too long, leave everything as is */
   if (subject_length < MAX_SUBJECT_OFFSET) {
      *start_shift = 0;
      return;
   }

   s_offset = *subject_offset_ptr;
   /* Maximal extension length is the remaining length in the query, plus 
      an estimate of a maximal total number of gaps. */
   max_extension_left = query_offset + MAX_TOTAL_GAPS;
   max_extension_right = query_length - query_offset + MAX_TOTAL_GAPS;

   if (s_offset <= max_extension_left) {
      *start_shift = 0;
   } else {
      *start_shift = s_offset - max_extension_left;;
      *subject_offset_ptr = max_extension_left;
   }

   if (subject_length - s_offset > max_extension_right) {
      *subject_length_ptr = s_offset + max_extension_right - *start_shift;
   }
}

/** Performs gapped extension for protein sequences, given two
 * sequence blocks, scoring and extension options, and an initial HSP 
 * with information from the previously performed ungapped extension
 * @param program BLAST program [in]
 * @param query_blk The query sequence block [in]
 * @param subject_blk The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param init_hsp The initial HSP information [in]
 */
static Int2 BLAST_ProtGappedAlignment(Uint1 program, 
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   BlastGapAlignStruct* gap_align,
   const BlastScoringOptions* score_options, BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left;
   Int4 private_q_start, private_s_start;
   Uint1* query=NULL,* subject=NULL;
   Boolean switch_seq = FALSE;
   Int4 query_length, subject_length;
   Uint4 subject_shift = 0;
    
   if (gap_align == NULL)
      return FALSE;
   
   if (score_options->is_ooframe) {
      q_length = init_hsp->q_off;
      s_length = init_hsp->s_off;

      if (program == blast_type_blastx) {
         subject = subject_blk->sequence + s_length;
         query = query_blk->oof_sequence + CODON_LENGTH + q_length;
         query_length = query_blk->length - CODON_LENGTH + 1;
         subject_length = subject_blk->length;
         switch_seq = TRUE;
      } else if (program == blast_type_tblastn) {
         subject = subject_blk->oof_sequence + CODON_LENGTH + s_length;
         query = query_blk->sequence + q_length;
         query_length = query_blk->length;
         subject_length = subject_blk->length - CODON_LENGTH + 1;
      }
   } else {
      q_length = init_hsp->q_off + 1;
      s_length = init_hsp->s_off + 1;
      query = query_blk->sequence;
      subject = subject_blk->sequence;
      query_length = query_blk->length;
      subject_length = subject_blk->length;
   }

   AdjustSubjectRange(&s_length, &subject_length, q_length, query_length, 
                      &subject_shift);

   found_start = FALSE;
   found_end = FALSE;
    
   /* Looking for "left" score */
   score_left = 0;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      if(score_options->is_ooframe) {
         score_left = OOF_SEMI_G_ALIGN_EX(query, subject, q_length, s_length,
               NULL, &private_q_start, &private_s_start, TRUE, NULL, 
               gap_align, score_options, q_length, TRUE, switch_seq);
      } else {
         score_left = SEMI_G_ALIGN_EX(query, subject+subject_shift, q_length, 
            s_length, NULL,
            &private_q_start, &private_s_start, TRUE, NULL, gap_align, 
            score_options, init_hsp->q_off, FALSE, TRUE);
      }
        
      gap_align->query_start = q_length - private_q_start;
      gap_align->subject_start = s_length - private_s_start + subject_shift;
      
   }

   score_right = 0;
   if (q_length < query_length && s_length < subject_length) {
      found_end = TRUE;
      if(score_options->is_ooframe) {
         score_right = OOF_SEMI_G_ALIGN_EX(query-1, subject-1, 
            query_length-q_length+1, subject_length-s_length+1,
            NULL, &(gap_align->query_stop), &(gap_align->subject_stop), 
            TRUE, NULL, gap_align, 
            score_options, q_length, FALSE, switch_seq);
         gap_align->query_stop += q_length;
         gap_align->subject_stop += s_length + subject_shift;
      } else {
         score_right = SEMI_G_ALIGN_EX(query+init_hsp->q_off,
            subject+init_hsp->s_off, query_length-q_length, 
            subject_length-s_length, NULL, &(gap_align->query_stop), 
            &(gap_align->subject_stop), TRUE, NULL, gap_align, 
            score_options, init_hsp->q_off, FALSE, FALSE);
         /* Make end offsets point to the byte after the end of the 
            alignment */
         gap_align->query_stop += init_hsp->q_off + 1;
         gap_align->subject_stop += init_hsp->s_off + 1;
      }
   }
   
   if (found_start == FALSE) {	/* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }
   
   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
      
      if(score_options->is_ooframe) {  /* Do we really need this ??? */
         gap_align->query_stop++;
         gap_align->subject_stop++;
      }
   }
   
   gap_align->score = score_right+score_left;

   return 0;
}

/** Copy of the TracebackToGapXEditBlock function from gapxdrop.c, only 
 * without 2 unused arguments 
 * @param S The traceback obtained from ALIGN [in]
 * @param M Length of alignment in query [in]
 * @param N Length of alignment in subject [in]
 * @param start1 Starting query offset [in]
 * @param start2 Starting subject offset [in]
 * @param edit_block The constructed edit block [out]
 */
static Int2 
BLAST_TracebackToGapEditBlock(Int4* S, Int4 M, Int4 N, Int4 start1, 
                               Int4 start2, GapEditBlock** edit_block)
{

  Int4 i, j, op, number_of_subs, number_of_decline;
  GapEditScript* e_script;

  if (S == NULL)
     return -1;

  i = j = op = number_of_subs = number_of_decline = 0;

  *edit_block = GapEditBlockNew(start1, start2);
  (*edit_block)->esp = e_script = GapEditScriptNew(NULL);

  while(i < M || j < N) 
  {
	op = *S;
	if (op != MININT && number_of_decline > 0) 
	{
               e_script->op_type = GAPALIGN_DECLINE;
               e_script->num = number_of_decline;
               e_script = GapEditScriptNew(e_script);
		number_of_decline = 0;
	}
        if (op != 0 && number_of_subs > 0) 
	{
                        e_script->op_type = GAPALIGN_SUB;
                        e_script->num = number_of_subs;
                        e_script = GapEditScriptNew(e_script);
                        number_of_subs = 0;
        }      
	if (op == 0) {
		i++; j++; number_of_subs++;
	} else if (op == MININT) {
		i++; j++;
		number_of_decline++; 
	}	
	else
	{
		if(op > 0) 
		{
			e_script->op_type = GAPALIGN_DEL;
			e_script->num = op;
			j += op;
			if (i < M || j < N)
                                e_script = GapEditScriptNew(e_script);
		}
		else
		{
			e_script->op_type = GAPALIGN_INS;
			e_script->num = ABS(op);
			i += ABS(op);
			if (i < M || j < N)
                                e_script = GapEditScriptNew(e_script);
		}
    	}
	S++;
  }

  if (number_of_subs > 0)
  {
	e_script->op_type = GAPALIGN_SUB;
	e_script->num = number_of_subs;
  } else if (number_of_decline > 0) {
        e_script->op_type = GAPALIGN_DECLINE;
        e_script->num = number_of_decline;
  }

  return 0;
}

/** Converts a OOF traceback from the gapped alignment to a GapEditBlock.
 * This function is for out-of-frame gapping:
 * index1 is for the protein, index2 is for the nucleotides.
 * 0: deletion of a dna codon, i.e dash aligned with a protein letter.
 * 1: one nucleotide vs a protein, deletion of 2 nuc.
 * 2: 2 nucleotides aligned with a protein, i.e deletion of one nuc.
 * 3: substitution, 3 nucleotides vs an amino acid.
 * 4: 4 nuc vs an amino acid.
 * 5: 5 nuc vs an amino acid.
 * 6: a codon aligned with a dash. opposite of 0.
 */

static Int2
BLAST_OOFTracebackToGapEditBlock(Int4* S, Int4 q_length, 
   Int4 s_length, Int4 q_start, Int4 s_start, Uint1 program, 
   GapEditBlock** edit_block_ptr)
{
    register Int4 current_val, last_val, number, index1, index2;
    Int4 M, N;
    GapEditScript* e_script;
    GapEditBlock* edit_block;
    
    if (S == NULL)
        return -1;
    
    number = 0;
    index1 = 0;
    index2 = 0;
    
    *edit_block_ptr = edit_block = GapEditBlockNew(q_start, s_start);
    edit_block->is_ooframe = TRUE;
    if (program == blast_type_blastx) {
        M = s_length;
        N = q_length;
    } else {
        M = q_length;
        N = s_length;
    }

    edit_block->esp = e_script = GapEditScriptNew(NULL);
    
    last_val = *S;
    
    /* index1, M - index and length of protein, 
       index2, N - length of the nucleotide */
    
    for(index1 = 0, index2 = 0; index1 < M && index2 < N; S++, number++) {
        
        current_val = *S;
        /* New script element will be created for any new frameshift
           region  0,3,6 will be collected in a single segment */
        if (current_val != last_val || (current_val%3 != 0 && 
                                        edit_block->esp != e_script)) {
            e_script->num = number;
            e_script = GapEditScriptNew(e_script);
            
            /* if(last_val%3 != 0 && current_val%3 == 0) */
            if(last_val%3 != 0 && current_val == 3) 
                /* 1, 2, 4, 5 vs. 0, 3, 6*/                
                number = 1;
            else
                number = 0;
        }
        
        last_val = current_val;
        
        /* for out_of_frame == TRUE - we have op_type == S parameter */
        e_script->op_type = current_val;
        
        if(current_val != 6) {
            index1++;
            index2 += current_val;
        } else {
            index2 += 3;
        }
    }
    /* Get the last one. */    
    e_script->num = number;

    return 0;
}

Int2 BLAST_GappedAlignmentWithTraceback(Uint1 program, Uint1* query, 
        Uint1* subject, BlastGapAlignStruct* gap_align, 
        const BlastScoringOptions* score_options,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length)
{
    Boolean found_start, found_end;
    Int4 score_right, score_left, private_q_length, private_s_length, tmp;
    Int4 q_length, s_length;
    Int4 prev;
    Int4* tback,* tback1,* p = NULL,* q;
    Boolean is_ooframe = score_options->is_ooframe;
    Int2 status = 0;
    Boolean switch_seq = FALSE;
    
    if (gap_align == NULL)
        return -1;
    
    found_start = FALSE;
    found_end = FALSE;
    
    q_length = query_length;
    s_length = subject_length;
    if (is_ooframe) {
       /* The mixed frame sequence is shifted to the 3rd position, so its 
          maximal available length for extension is less by 2 than its 
          total length. */
       switch_seq = (program == blast_type_blastx);
       if (switch_seq) {
          q_length -= CODON_LENGTH - 1;
       } else {
          s_length -= CODON_LENGTH - 1;
       }
    }

    tback = tback1 = (Int4*)
       malloc((s_length + q_length)*sizeof(Int4));

    score_left = 0; prev = 3;
    found_start = TRUE;
        
    if(is_ooframe) {
       /* NB: Left extension does not include starting point corresponding
          to the offset pair; the right extension does. */
       score_left =
          OOF_SEMI_G_ALIGN_EX(query+q_start, subject+s_start, q_start, 
             s_start, tback, &private_q_length, &private_s_length, FALSE,
             &tback1, gap_align, score_options, q_start, TRUE, switch_seq);
       gap_align->query_start = q_start - private_q_length;
       gap_align->subject_start = s_start - private_s_length;
    } else {        
       /* NB: The left extension includes the starting point 
          [q_start,s_start]; the right extension does not. */
       score_left = 
          SEMI_G_ALIGN_EX(query, subject, q_start+1, s_start+1, tback, 
             &private_q_length, &private_s_length, FALSE, &tback1, 
             gap_align, score_options, q_start, FALSE, TRUE);
       gap_align->query_start = q_start - private_q_length + 1;
       gap_align->subject_start = s_start - private_s_length + 1;
    }
    
    for(p = tback, q = tback1-1; p < q; p++, q--)  {
       tmp = *p;
       *p = *q;
       *q = tmp;
    }
        
    if(is_ooframe){
       for (prev = 3, p = tback; p < tback1; p++) {
          if (*p == 0 || *p ==  6) continue;
          tmp = *p; *p = prev; prev = tmp;
       }
    }
    
    score_right = 0;

    if ((q_start < q_length) && (s_start < s_length)) {
       found_end = TRUE;
       if(is_ooframe) {
          score_right = 
             OOF_SEMI_G_ALIGN_EX(query+q_start-1, subject+s_start-1, 
                q_length-q_start, s_length-s_start, 
                tback1, &private_q_length, &private_s_length, FALSE,
                &tback1, gap_align, score_options, q_start, FALSE, switch_seq);
            if (prev != 3 && p) {
                while (*p == 0 || *p == 6) p++;
                *p = prev+*p-3;
            }
        } else {
            score_right = 
               SEMI_G_ALIGN_EX(query+q_start, subject+s_start, 
                  q_length-q_start-1, s_length-s_start-1, 
                  tback1, &private_q_length, &private_s_length, FALSE, 
                  &tback1, gap_align, score_options, q_start, FALSE, FALSE);
        }

        gap_align->query_stop = q_start + private_q_length + 1;
        gap_align->subject_stop = s_start + private_s_length + 1;
    }
    
    if (found_start == FALSE) {	/* Start never found */
        gap_align->query_start = q_start;
        gap_align->subject_start = s_start;
    }
    
    if (found_end == FALSE) {
        gap_align->query_stop = q_start - 1;
        gap_align->subject_stop = s_start - 1;
    }

    if(is_ooframe) {
        status = BLAST_OOFTracebackToGapEditBlock(tback, 
           gap_align->query_stop-gap_align->query_start, 
           gap_align->subject_stop-gap_align->subject_start, 
           gap_align->query_start, gap_align->subject_start, 
           program, &gap_align->edit_block);
    } else {
        status = BLAST_TracebackToGapEditBlock(tback, 
           gap_align->query_stop-gap_align->query_start, 
           gap_align->subject_stop-gap_align->subject_start, 
           gap_align->query_start, gap_align->subject_start,
           &gap_align->edit_block);
    }

    gap_align->edit_block->length1 = query_length;
    gap_align->edit_block->length2 = subject_length;
#if 0 /** How can these be assigned???? This data is now not in gap_align. */
    gap_align->edit_block->translate1 = gap_align->translate1;
    gap_align->edit_block->translate2 = gap_align->translate2;
    gap_align->edit_block->discontinuous = gap_align->discontinuous;
#endif

    sfree(tback);

    gap_align->score = score_right+score_left;
    
    return status;
}

static Int4 
GetPatternLengthFromGapAlignStruct(BlastGapAlignStruct* gap_align)
{
   /* Kludge: pattern lengths are saved in the output structure member 
      query_stop. Probably should be changed??? */
   return gap_align->query_stop;
}

Int2 PHIGappedAlignmentWithTraceback(Uint1 program, 
        Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringOptions* score_options,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length)
{
    Boolean found_end;
    Int4 score_right, score_left, private_q_length, private_s_length, tmp;
    Int4* tback,* tback1,* p = NULL,* q;
    Int2 status = 0;
    Int4 pat_length;
    
    if (gap_align == NULL)
        return -1;
    
    found_end = FALSE;
    
    tback = tback1 = (Int4*)
       malloc((subject_length + query_length)*sizeof(Int4));

    score_left = 0;
        
    score_left = 
       SEMI_G_ALIGN_EX(query, subject, q_start, s_start, tback, 
           &private_q_length, &private_s_length, FALSE, &tback1, 
          gap_align, score_options, q_start, FALSE, TRUE);
    gap_align->query_start = q_start - private_q_length;
    gap_align->subject_start = s_start - private_s_length;
    
    for(p = tback, q = tback1-1; p < q; p++, q--)  {
       tmp = *p;
       *p = *q;
       *q = tmp;
    }
        
    pat_length = GetPatternLengthFromGapAlignStruct(gap_align);
    memset(tback1, 0, pat_length*sizeof(Int4));
    tback1 += pat_length;

    score_right = 0;

    q_start += pat_length - 1;
    s_start += pat_length - 1;

    if ((q_start < query_length) && (s_start < subject_length)) {
       found_end = TRUE;
       score_right = 
          SEMI_G_ALIGN_EX(query+q_start, subject+s_start, 
             query_length-q_start-1, subject_length-s_start-1, 
             tback1, &private_q_length, &private_s_length, FALSE, 
             &tback1, gap_align, score_options, q_start, FALSE, FALSE);

       gap_align->query_stop = q_start + private_q_length + 1;
       gap_align->subject_stop = s_start + private_s_length + 1; 
    }
    
    if (found_end == FALSE) {
        gap_align->query_stop = q_start;
        gap_align->subject_stop = s_start;
    }

    status = BLAST_TracebackToGapEditBlock(tback, 
                gap_align->query_stop-gap_align->query_start, 
                gap_align->subject_stop-gap_align->subject_start, 
                gap_align->query_start, gap_align->subject_start,
                &gap_align->edit_block);

    gap_align->edit_block->length1 = query_length;
    gap_align->edit_block->length2 = subject_length;

    sfree(tback);

    gap_align->score = score_right+score_left;
    
    return status;
}

Int2 BLAST_GetUngappedHSPList(BlastInitHitList* init_hitlist, 
        BLAST_SequenceBlk* subject, 
        const BlastHitSavingOptions* hit_options, 
        BlastHSPList** hsp_list_ptr)
{
   BlastHSPList* hsp_list = NULL;
   Int4 index;
   BlastInitHSP* init_hsp;

   if (!init_hitlist) {
      *hsp_list_ptr = NULL;
      return 0;
   }

   for (index = 0; index < init_hitlist->total; ++index) {
      init_hsp = &init_hitlist->init_hsp_array[index];
      if (!init_hsp->ungapped_data) 
         continue;
      if (!hsp_list)
         hsp_list = BlastHSPListNew();
      BLAST_SaveHsp(NULL, init_hsp, hsp_list, hit_options, 
                    subject->frame);
   }

   *hsp_list_ptr = hsp_list;
   return 0;
}

/** Performs gapped extension for PHI BLAST, given two
 * sequence blocks, scoring and extension options, and an initial HSP 
 * with information from the previously performed ungapped extension
 * @param program BLAST program [in]
 * @param query_blk The query sequence block [in]
 * @param subject_blk The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param init_hsp The initial HSP information [in]
 */
static Int2 PHIGappedAlignment(Uint1 program, 
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   BlastGapAlignStruct* gap_align,
   const BlastScoringOptions* score_options, BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left;
   Int4 private_q_start, private_s_start;
   Uint1* query,* subject;
    
   if (gap_align == NULL)
      return FALSE;
   
   q_length = init_hsp->q_off;
   s_length = init_hsp->s_off;
   query = query_blk->sequence;
   subject = subject_blk->sequence;

   found_start = FALSE;
   found_end = FALSE;
    
   /* Looking for "left" score */
   score_left = 0;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      score_left = 
         SEMI_G_ALIGN_EX(query, subject, q_length, s_length, NULL,
            &private_q_start, &private_s_start, TRUE, NULL, gap_align, 
            score_options, init_hsp->q_off, FALSE, TRUE);
        
      gap_align->query_start = q_length - private_q_start + 1;
      gap_align->subject_start = s_length - private_s_start + 1;
      
   } else {
      q_length = init_hsp->q_off;
      s_length = init_hsp->s_off;
   }

   /* Pattern itself is not included in the gapped alignment */
   q_length += init_hsp->ungapped_data->length - 1;
   s_length += init_hsp->ungapped_data->length - 1;

   score_right = 0;
   if (init_hsp->q_off < query_blk->length && 
       init_hsp->s_off < subject_blk->length) {
      found_end = TRUE;
      score_right = SEMI_G_ALIGN_EX(query+q_length,
         subject+s_length, query_blk->length-q_length, 
         subject_blk->length-s_length, NULL, &(gap_align->query_stop), 
         &(gap_align->subject_stop), TRUE, NULL, gap_align, 
         score_options, q_length, FALSE, FALSE);
      gap_align->query_stop += q_length;
      gap_align->subject_stop += s_length;
   }
   
   if (found_start == FALSE) {	/* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }
   
   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
   }
   
   gap_align->score = score_right+score_left;

   return 0;
}

Int2 PHIGetGappedScore (Uint1 program_number, 
        BLAST_SequenceBlk* query, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr)

{
   BlastHSPList* hsp_list;
   BlastInitHSP** init_hsp_array;
   BlastInitHSP* init_hsp;
   Int4 index;
   Int2 status = 0;
   BlastHitSavingOptions* hit_options = hit_params->options;

   if (!query || !subject || !gap_align || !score_options || !ext_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return 1;

   if (init_hitlist->total == 0)
      return 0;


   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = BlastHSPListNew();
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
      malloc(init_hitlist->total*sizeof(BlastInitHSP*));

   for (index = 0; index < init_hitlist->total; ++index)
      init_hsp_array[index] = &init_hitlist->init_hsp_array[index];

   for (index=0; index<init_hitlist->total; index++)
   {
      init_hsp = init_hsp_array[index];

      status =  PHIGappedAlignment(program_number, query, 
                   subject, gap_align, score_options, init_hsp);

      if (status) {
         sfree(init_hsp_array);
         return status;
      }

      BLAST_SaveHsp(gap_align, init_hsp, hsp_list, hit_options, 
                    subject->frame);

      /* Free ungapped data here - it's no longer needed */
      sfree(init_hsp->ungapped_data);
   }   

   sfree(init_hsp_array);

   *hsp_list_ptr = hsp_list;
   return status;
}
