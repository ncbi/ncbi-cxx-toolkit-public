
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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file link_hsps.c
 * Functions to link with use of sum statistics
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/blast_util.h>

/******************************************************************************
 * Structures and functions used only in even (small or large) gap linking    *
 * methods.                                                                   *
 ******************************************************************************/

/** Describes the method for ordering HSPs. Note that these values
 *  are used to index an array, so their values must linearly increase
 */
typedef enum ELinkOrderingMethod {
    eLinkSmallGaps = 0,   /**< favor small gaps when linking an HSP */
    eLinkLargeGaps = 1,   /**< favor large gaps when linking an HSP */
    eOrderingMethods = 2  /**< number of methods (last in list) */
} ELinkOrderingMethod;

/* Forward declaration */
struct LinkHSPStruct;

/** The following structure is used in "link_hsps" to decide between
 * two different "gapping" models.  Here link is used to hook up
 * a chain of HSP's, num is the number of links, and sum is the sum score.
 * Once the best gapping model has been found, this information is
 * transferred up to the LinkHSPStruct.  This structure should not be
 * used outside of the function Blast_EvenGapLinkHSPs.
 */
typedef struct BlastHSPLink {
   struct LinkHSPStruct* link[eOrderingMethods]; /**< Best 
                                               choice of HSP to link with */
   Int2 num[eOrderingMethods]; /**< number of HSP in the ordering. */
   Int4 sum[eOrderingMethods]; /**< Sum-Score of HSP. */
   double xsum[eOrderingMethods]; /**< Sum-Score of HSP,
                                     multiplied by the appropriate Lambda. */
   Int4 changed; /**< Has the link been changed since previous access? */
} BlastHSPLink;

/** Structure containing all information internal to the process of linking
 * HSPs.
 */
typedef struct LinkHSPStruct {
   BlastHSP* hsp;      /**< Specific HSP this structure corresponds to */
   struct LinkHSPStruct* prev;         /**< Previous HSP in a set, if any */
   struct LinkHSPStruct* next;         /**< Next HSP in a set, if any */ 
   BlastHSPLink  hsp_link; /**< Auxiliary structure for keeping track of sum
                              scores, etc. */
   Boolean linked_set;     /**< Is this HSp part of a linked set? */
   Boolean start_of_chain; /**< If TRUE, this HSP starts a chain along the
                              "link" pointer. */
   Int4 linked_to;         /**< Where this HSP is linked to? */
   double xsum;              /**< Normalized score of a set of HSPs */
   ELinkOrderingMethod ordering_method;   /**< Which method (max or 
                                            no max for gaps) was 
                                            used for linking HSPs? */
   Int4 q_offset_trim;     /**< Start of trimmed hsp in query */
   Int4 q_end_trim;        /**< End of trimmed HSP in query */
   Int4 s_offset_trim;     /**< Start of trimmed hsp in subject */
   Int4 s_end_trim;        /**< End of trimmed HSP in subject */
} LinkHSPStruct;

/** The helper array contains the info used frequently in the inner 
 * for loops of the HSP linking algorithm.
 * One array of helpers will be allocated for each thread.
 */
typedef struct LinkHelpStruct {
  LinkHSPStruct* ptr;         /**< The HSP to which the info belongs */
  Int4 q_off_trim;            /**< query start of trimmed HSP */
  Int4 s_off_trim;            /**< subject start of trimmed HSP */
  Int4 sum[eOrderingMethods]; /**< raw score of linked set containing HSP(?) */
  Int4 maxsum1;               /**< threshold for stopping link attempts (?) */
  Int4 next_larger;           /**< offset into array of HelpStructs containing
                                   HSP with higher score, used in bailout
                                   calculations */
} LinkHelpStruct;

/** Callback used by qsort to sort a list of HSPs, encapsulated in
 *  LinkHSPStruct structures, in order of increasing query start offset.
 *  The subject start offset of HSPs is used as a tiebreaker, and no HSPs
 *  may be NULL.
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_FwdCompareHSPs(const void* v1, const void* v2)
{
	LinkHSPStruct** hp1,** hp2;
    BlastHSP* h1,* h2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;

    h1 = (*hp1)->hsp;
    h2 = (*hp2)->hsp;

    if (h1->context < h2->context)
        return -1;
    else if (h1->context > h2->context)
        return 1;

	if (h1->query.offset < h2->query.offset) 
		return -1;
	if (h1->query.offset > h2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return -1;
	if (h1->subject.offset > h2->subject.offset) 
		return 1;

	return 0;
}

/** Like s_FwdCompareHSPs, except with additional logic to
 *  distinguish HSPs that lie within different strands of 
 *  a single translated query sequence
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_FwdCompareHSPsTransl(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   context1 = h1->context/(NUM_FRAMES / 2);
   context2 = h2->context/(NUM_FRAMES / 2);

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return -1;
	if (h1->query.offset > h2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return -1;
	if (h1->subject.offset > h2->subject.offset) 
		return 1;

	return 0;
}

/** Callback used by qsort to sort a list of HSPs (encapsulated in
 *  LinkHSPStruct structures) in order of decreasing query start offset.
 *  The subject start offset of HSPs is used as a tiebreaker, and no HSPs
 *  may be NULL
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_RevCompareHSPs(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;
	
   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return 1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

/** Like s_RevCompareHSPs, except with additional logic to
 *  distinguish HSPs that lie within different strands of 
 *  a single translated query sequence
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_RevCompareHSPsTransl(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;
	
   context1 = h1->context/(NUM_FRAMES / 2);
   context2 = h2->context/(NUM_FRAMES / 2);

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return 1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

/** Callback used by qsort to sort a list of HSPs (encapsulated in
 *  LinkHSPStruct structures) in order of decreasing query start offset
 *  (suitable for use with tblastn). HSPs are first separated by frame 
 *  of a translated subject sequence, and tiebreaking is by decreasing 
 *  query end offset, then subject start offset, then subject end offset. 
 *  HSPs may not be NULL
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_RevCompareHSPsTbn(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (SIGN(h1->subject.frame) != SIGN(h2->subject.frame))
	{
		if (h1->subject.frame > h2->subject.frame)
			return 1;
		else
			return -1;
	}

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	if (h1->query.end < h2->query.end) 
		return  1;
	if (h1->query.end > h2->query.end) 
		return -1;
	if (h1->subject.offset < h2->subject.offset) 
		return  1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	if (h1->subject.end < h2->subject.end) 
		return  1;
	if (h1->subject.end > h2->subject.end) 
		return -1;
	return 0;
}

/** Callback used by qsort to sort a list of HSPs (encapsulated in
 *  LinkHSPStruct structures) in order of decreasing query start offset
 *  (suitable for use with tblastx). HSPs are first separated by frame 
 *  of a translated query sequence and then by frame of a translated
 *  subject sequence. Tiebreaking is by decreasing query end offset, 
 *  then subject start offset, then subject end offset. HSPs may not be NULL
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_RevCompareHSPsTbx(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   context1 = h1->context/(NUM_FRAMES / 2);
   context2 = h2->context/(NUM_FRAMES / 2);

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;
   
	if (SIGN(h1->subject.frame) != SIGN(h2->subject.frame))
	{
		if (h1->subject.frame > h2->subject.frame)
			return 1;
		else
			return -1;
	}

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	if (h1->query.end < h2->query.end) 
		return  1;
	if (h1->query.end > h2->query.end) 
		return -1;
	if (h1->subject.offset < h2->subject.offset) 
		return  1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	if (h1->subject.end < h2->subject.end) 
		return  1;
	if (h1->subject.end > h2->subject.end) 
		return -1;
	return 0;
}

/** Initialize a LinkHSPStruct
 * @param lhsp Pointer to struct to initialize. If NULL, struct gets
 *              allocated and then initialized [in/modified]
 * @return Pointer to initialized struct
 */
static LinkHSPStruct* 
s_LinkHSPStructReset(LinkHSPStruct* lhsp)
{
   BlastHSP* hsp;

   if (!lhsp) {
      lhsp = (LinkHSPStruct*) calloc(1, sizeof(LinkHSPStruct));
      lhsp->hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
   } else {
      if (!lhsp->hsp) {
         hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
      } else {
         hsp = lhsp->hsp;
         memset(hsp, 0, sizeof(BlastHSP));
      }
      memset(lhsp, 0, sizeof(LinkHSPStruct));
      lhsp->hsp = hsp;
   }
   return lhsp;
}

/** Perform even gap linking on a list of HSPs 
 * @param program_number The blast program that generated the HSPs [in]
 * @param hsp_list List of HSPs to link [in/modified]
 * @param query_info List of structures describing all query sequences [in]
 * @param subject_length Number of letters in the subject sequence [in]
 * @param sbp Score block [in]
 * @param link_hsp_params Configuration information for the linking process [in]
 * @param gapped_calculation TRUE if the HSPs are from a gapped search [in]
 * @return 0 if linking succeeded, nonzero otherwise
 */
static Int2
s_BlastEvenGapLinkHSPs(EBlastProgramType program_number, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length,
   BlastScoreBlk* sbp, const BlastLinkHSPParameters* link_hsp_params,
   Boolean gapped_calculation)
{
	LinkHSPStruct* H,* H2,* best[2],* first_hsp,* last_hsp,** hp_frame_start;
	LinkHSPStruct* hp_start = NULL;
   BlastHSP* hsp;
   BlastHSP** hsp_array;
	Blast_KarlinBlk** kbp;
	Int4 maxscore, cutoff[2];
	Boolean linked_set, ignore_small_gaps;
	double gap_decay_rate, gap_prob, prob[2];
	Int4 index, index1, num_links, frame_index;
   ELinkOrderingMethod ordering_method;
   Int4 num_query_frames, num_subject_frames;
	Int4 *hp_frame_number;
	Int4 window_size, trim_size;
   Int4 number_of_hsps, total_number_of_hsps;
   Int4 query_length, length_adjustment;
   Int4 subject_length_orig = subject_length;
	LinkHSPStruct* link;
	Int4 H2_index,H_index;
	Int4 i;
 	Int4 path_changed;  /* will be set if an element is removed that may change an existing path */
 	Int4 first_pass, use_current_max; 
	LinkHelpStruct *lh_helper=0;
   Int4 lh_helper_size;
	Int4 query_context; /* AM: to support query concatenation. */
   Boolean translated_query;
   LinkHSPStruct** link_hsp_array;

	if (hsp_list == NULL)
		return -1;

   hsp_array = hsp_list->hsp_array;

   lh_helper_size = MAX(1024,hsp_list->hspcnt+5);
   lh_helper = (LinkHelpStruct *) 
      calloc(lh_helper_size, sizeof(LinkHelpStruct));

	if (gapped_calculation) 
		kbp = sbp->kbp_gap;
	else
		kbp = sbp->kbp;

	total_number_of_hsps = hsp_list->hspcnt;

	number_of_hsps = total_number_of_hsps;

   /* For convenience, include overlap size into the gap size */
	window_size = link_hsp_params->gap_size + link_hsp_params->overlap_size + 1;
   trim_size = (link_hsp_params->overlap_size + 1) / 2;
	gap_prob = link_hsp_params->gap_prob;
	gap_decay_rate = link_hsp_params->gap_decay_rate;

	translated_query = (program_number == eBlastTypeBlastx || 
                       program_number == eBlastTypeTblastx);
   if (program_number == eBlastTypeTblastn ||
       program_number == eBlastTypeTblastx)
      num_subject_frames = NUM_STRANDS;
   else
      num_subject_frames = 1;

   link_hsp_array = 
      (LinkHSPStruct**) malloc(total_number_of_hsps*sizeof(LinkHSPStruct*));
   for (index = 0; index < total_number_of_hsps; ++index) {
      link_hsp_array[index] = (LinkHSPStruct*) calloc(1, sizeof(LinkHSPStruct));
      link_hsp_array[index]->hsp = hsp_array[index];
   }

   /* Sort by (reverse) position. */
   if (translated_query) {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_RevCompareHSPsTbx);
   } else {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_RevCompareHSPsTbn);
   }

   cutoff[0] = link_hsp_params->cutoff_small_gap;
   cutoff[1] = link_hsp_params->cutoff_big_gap;
   
   ignore_small_gaps = (cutoff[0] == 0);
   
   /* If query is nucleotide, it has 2 strands that should be separated. */
   if (translated_query || program_number == eBlastTypeBlastn)
      num_query_frames = NUM_STRANDS*query_info->num_queries;
   else
      num_query_frames = query_info->num_queries;
   
   hp_frame_start = 
       calloc(num_subject_frames*num_query_frames, sizeof(LinkHSPStruct*));
   hp_frame_number = calloc(num_subject_frames*num_query_frames, sizeof(Int4));

   /* hook up the HSP's */
   hp_frame_start[0] = link_hsp_array[0];

   /* Put entries from different strands into separate 'query_frame's. */
   {
      Int4 cur_frame=0;
      Int4 strand_factor = (translated_query ? 3 : 1);
      for (index=0;index<number_of_hsps;index++) 
      {
        H=link_hsp_array[index];
        H->start_of_chain = FALSE;
        hp_frame_number[cur_frame]++;

        H->prev= index ? link_hsp_array[index-1] : NULL;
        H->next= index<(number_of_hsps-1) ? link_hsp_array[index+1] : NULL;
        if (H->prev != NULL && 
            ((H->hsp->context/strand_factor) != 
	     (H->prev->hsp->context/strand_factor) ||
             (SIGN(H->hsp->subject.frame) != SIGN(H->prev->hsp->subject.frame))))
        { /* If frame switches, then start new list. */
           hp_frame_number[cur_frame]--;
           hp_frame_number[++cur_frame]++;
           hp_frame_start[cur_frame] = H;
           H->prev->next = NULL;
           H->prev = NULL;
        }
      }
      num_query_frames = cur_frame+1;
   }

   /* trim_size is the maximum amount q.offset can differ from 
      q.offset_trim */
   /* This is used to break out of H2 loop early */
   for (index=0;index<number_of_hsps;index++) 
   {
       Int4 q_length, s_length;
       H = link_hsp_array[index];
       hsp = H->hsp;
       q_length = (hsp->query.end - hsp->query.offset) / 4;
       s_length = (hsp->subject.end - hsp->subject.offset) / 4;
       H->q_offset_trim = hsp->query.offset + MIN(q_length, trim_size);
       H->q_end_trim = hsp->query.end - MIN(q_length, trim_size);
       H->s_offset_trim = hsp->subject.offset + MIN(s_length, trim_size);
       H->s_end_trim = hsp->subject.end - MIN(s_length, trim_size);
   }	    
   
	for (frame_index=0; frame_index<num_query_frames; frame_index++)
	{
      hp_start = s_LinkHSPStructReset(hp_start);
      hp_start->next = hp_frame_start[frame_index];
      hp_frame_start[frame_index]->prev = hp_start;
      number_of_hsps = hp_frame_number[frame_index];
      query_context = hp_start->next->hsp->context;
      length_adjustment = query_info->contexts[query_context].length_adjustment;
      query_length = query_info->contexts[query_context].query_length;
      query_length = MAX(query_length - length_adjustment, 1);
      subject_length = subject_length_orig; /* in nucleotides even for tblast[nx] */
      /* If subject is translated, length adjustment is given in nucleotide
         scale. */
      if (program_number == eBlastTypeTblastn || 
          program_number == eBlastTypeTblastx)
      {
         length_adjustment /= CODON_LENGTH;
         subject_length /= CODON_LENGTH;
      }
      subject_length = MAX(subject_length - length_adjustment, 1);

      lh_helper[0].ptr = hp_start;
      lh_helper[0].q_off_trim = 0;
      lh_helper[0].s_off_trim = 0;
      lh_helper[0].maxsum1  = -10000;
      lh_helper[0].next_larger  = 0;
      
      /* lh_helper[0]  = empty     = additional end marker
       * lh_helper[1]  = hsp_start = empty entry used in original code
       * lh_helper[2]  = hsp_array->next = hsp_array[0]
       * lh_helper[i]  = ... = hsp_array[i-2] (for i>=2) 
       */
      first_pass=1;    /* do full search */
      path_changed=1;
      for (H=hp_start->next; H!=NULL; H=H->next) 
         H->hsp_link.changed=1;

      while (number_of_hsps > 0)
      {
         Int4 max[3];
         max[0]=max[1]=max[2]=-10000;
         /* Initialize the 'best' parameter */
         best[0] = best[1] = NULL;
         
         
         /* See if we can avoid recomputing all scores:
          *  - Find the max paths (based on old scores). 
          *  - If no paths were changed by removal of nodes (ie research==0) 
          *    then these max paths are still the best.
          *  - else if these max paths were unchanged, then they are still the best.
          */
         use_current_max=0;
         if (!first_pass){
            Int4 max0,max1;
            /* Find the current max sums */
            if(!ignore_small_gaps){
               max0 = -cutoff[0];
               max1 = -cutoff[1];
               for (H=hp_start->next; H!=NULL; H=H->next) {
                  Int4 sum0=H->hsp_link.sum[0];
                  Int4 sum1=H->hsp_link.sum[1];
                  if(sum0>=max0)
                  {
                     max0=sum0;
                     best[0]=H;
                  }
                  if(sum1>=max1)
                  {
                     max1=sum1;
                     best[1]=H;
                  }
               }
            } else {
               maxscore = -cutoff[1];
               for (H=hp_start->next; H!=NULL; H=H->next) {
                  Int4  sum=H->hsp_link.sum[1];
                  if(sum>=maxscore)
                  {
                     maxscore=sum;
                     best[1]=H;
                  }
               }
            }
            if(path_changed==0){
               /* No path was changed, use these max sums. */
               use_current_max=1;
            }
            else{
               /* If max path hasn't chaged, we can use it */
               /* Walk down best, give up if we find a removed item in path */
               use_current_max=1;
               if(!ignore_small_gaps){
                  for (H=best[0]; H!=NULL; H=H->hsp_link.link[0]) 
                     if (H->linked_to==-1000) {use_current_max=0; break;}
               }
               if(use_current_max)
                  for (H=best[1]; H!=NULL; H=H->hsp_link.link[1]) 
                     if (H->linked_to==-1000) {use_current_max=0; break;}
               
            }
         }

         /* reset helper_info */
         /* Inside this while loop, the linked list order never changes 
          * So here we initialize an array of commonly used info, 
          * and in this loop we access these arrays instead of the actual list 
          */
         if(!use_current_max){
            for (H=hp_start,H_index=1; H!=NULL; H=H->next,H_index++) {
               Int4 s_frame = H->hsp->subject.frame;
               Int4 s_off_t = H->s_offset_trim;
               Int4 q_off_t = H->q_offset_trim;
               lh_helper[H_index].ptr = H;
               lh_helper[H_index].q_off_trim = q_off_t;
               lh_helper[H_index].s_off_trim = s_off_t;
               for(i=0;i<eOrderingMethods;i++)
                  lh_helper[H_index].sum[i] = H->hsp_link.sum[i];
               max[SIGN(s_frame)+1]=
                  MAX(max[SIGN(s_frame)+1],H->hsp_link.sum[1]);
               lh_helper[H_index].maxsum1 =max[SIGN(s_frame)+1];					   
               
               /* set next_larger to link back to closest entry with a sum1 
                  larger than this */
               {
                  Int4 cur_sum=lh_helper[H_index].sum[1];
                  Int4 prev = H_index-1;
                  Int4 prev_sum = lh_helper[prev].sum[1];
                  while((cur_sum>=prev_sum) && (prev>0)){
                     prev=lh_helper[prev].next_larger;
                     prev_sum = lh_helper[prev].sum[1];
                  }
                  lh_helper[H_index].next_larger = prev;
               }
               H->linked_to = 0;
            }
            
            lh_helper[1].maxsum1 = -10000;
            
            /****** loop iter for index = 0  **************************/
            if(!ignore_small_gaps)
            {
               index=0;
               maxscore = -cutoff[index];
               H_index = 2;
               for (H=hp_start->next; H!=NULL; H=H->next,H_index++) 
               {
                  Int4 H_hsp_num=0;
                  Int4 H_hsp_sum=0;
                  double H_hsp_xsum=0.0;
                  LinkHSPStruct* H_hsp_link=NULL;
                  if (H->hsp->score > cutoff[index]) {
                     Int4 H_query_etrim = H->q_end_trim;
                     Int4 H_sub_etrim = H->s_end_trim;
                     Int4 H_q_et_gap = H_query_etrim+window_size;
                     Int4 H_s_et_gap = H_sub_etrim+window_size;
                     
                     /* We only walk down hits with the same frame sign */
                     /* for (H2=H->prev; H2!=NULL; H2=H2->prev,H2_index--) */
                     for (H2_index=H_index-1; H2_index>1; H2_index=H2_index-1) 
                     {
                        Int4 b1,b2,b4,b5;
                        Int4 q_off_t,s_off_t,sum;
                        
                        /* s_frame = lh_helper[H2_index].s_frame; */
                        q_off_t = lh_helper[H2_index].q_off_trim;
                        s_off_t = lh_helper[H2_index].s_off_trim;
                        
                        /* combine tests to reduce mispredicts -cfj */
                        b1 = q_off_t <= H_query_etrim;
                        b2 = s_off_t <= H_sub_etrim;
                        sum = lh_helper[H2_index].sum[index];
                        
                        
                        b4 = ( q_off_t > H_q_et_gap ) ;
                        b5 = ( s_off_t > H_s_et_gap ) ;
                        
                        /* list is sorted by q_off, so q_off should only increase.
                         * q_off_t can only differ from q_off by trim_size
                         * So once q_off_t is large enough (ie it exceeds limit 
                         * by trim_size), we can stop.  -cfj 
                         */
                        if(q_off_t > (H_q_et_gap+trim_size))
                           break;
                        
                        if (b1|b2|b5|b4) continue;
                        
                        if (sum>H_hsp_sum) 
                        {
                           H2=lh_helper[H2_index].ptr; 
                           H_hsp_num=H2->hsp_link.num[index];
                           H_hsp_sum=H2->hsp_link.sum[index];
                           H_hsp_xsum=H2->hsp_link.xsum[index];
                           H_hsp_link=H2;
                        }
                     } /* end for H2... */
                  }
                  { 
                     Int4 score=H->hsp->score;
                     double new_xsum =
                       H_hsp_xsum + score*kbp[H->hsp->context]->Lambda -
                       kbp[H->hsp->context]->logK;
                     Int4 new_sum = H_hsp_sum + (score - cutoff[index]);
                     
                     H->hsp_link.sum[index] = new_sum;
                     H->hsp_link.num[index] = H_hsp_num+1;
                     H->hsp_link.link[index] = H_hsp_link;
                     lh_helper[H_index].sum[index] = new_sum;
                     if (new_sum >= maxscore) 
                     {
                        maxscore=new_sum;
                        best[index]=H;
                     }
                     H->hsp_link.xsum[index] = new_xsum;
                     if(H_hsp_link)
                        ((LinkHSPStruct*)H_hsp_link)->linked_to++;
                  }
               } /* end for H=... */
            }
            /****** loop iter for index = 1  **************************/
            index=1;
            maxscore = -cutoff[index];
            H_index = 2;
            for (H=hp_start->next; H!=NULL; H=H->next,H_index++) 
            {
               Int4 H_hsp_num=0;
               Int4 H_hsp_sum=0;
               double H_hsp_xsum=0.0;
               LinkHSPStruct* H_hsp_link=NULL;
               
               H->hsp_link.changed=1;
               H2 = H->hsp_link.link[index];
               if ((!first_pass) && ((H2==0) || (H2->hsp_link.changed==0)))
               {
                  /* If the best choice last time has not been changed, then 
                     it is still the best choice, so no need to walk down list.
                  */
                  if(H2){
                     H_hsp_num=H2->hsp_link.num[index];
                     H_hsp_sum=H2->hsp_link.sum[index];
                     H_hsp_xsum=H2->hsp_link.xsum[index];
                  }
                  H_hsp_link=H2;
                  H->hsp_link.changed=0;
               } else if (H->hsp->score > cutoff[index]) {
                  Int4 H_query_etrim = H->q_end_trim;
                  Int4 H_sub_etrim = H->s_end_trim;
                  

                  /* Here we look at what was the best choice last time, if it's
                   * still around, and set this to the initial choice. By
                   * setting the best score to a (potentially) large value
                   * initially, we can reduce the number of hsps checked. 
                   */
                  
                  /* Currently we set the best score to a value just less than
                   * the real value. This is not really necessary, but doing
                   * this ensures that in the case of a tie, we make the same
                   * selection the original code did.
                   */
                  
                  if(!first_pass&&H2&&H2->linked_to>=0){
                     if(1){
                        /* We set this to less than the real value to keep the
                           original ordering in case of ties. */
                        H_hsp_sum=H2->hsp_link.sum[index]-1;
                     }else{
                        H_hsp_num=H2->hsp_link.num[index];
                        H_hsp_sum=H2->hsp_link.sum[index];
                        H_hsp_xsum=H2->hsp_link.xsum[index];
                        H_hsp_link=H2;
                     }
                  }
                  
                  /* We now only walk down hits with the same frame sign */
                  /* for (H2=H->prev; H2!=NULL; H2=H2->prev,H2_index--) */
                  for (H2_index=H_index-1; H2_index>1;)
                  {
                     Int4 b0,b1,b2;
                     Int4 q_off_t,s_off_t,sum,next_larger;
                     LinkHelpStruct * H2_helper=&lh_helper[H2_index];
                     sum = H2_helper->sum[index];
                     next_larger = H2_helper->next_larger;
                     
                     s_off_t = H2_helper->s_off_trim;
                     q_off_t = H2_helper->q_off_trim;
                     
                     b0 = sum <= H_hsp_sum;
                     
                     /* Compute the next H2_index */
                     H2_index--;
                     if(b0){	 /* If this sum is too small to beat H_hsp_sum, advance to a larger sum */
                        H2_index=next_larger;
                     }

                     /* combine tests to reduce mispredicts -cfj */
                     b1 = q_off_t <= H_query_etrim;
                     b2 = s_off_t <= H_sub_etrim;
                     
                     if(0) if(H2_helper->maxsum1<=H_hsp_sum)break;
                     
                     if (!(b0|b1|b2) )
                     {
                        H2 = H2_helper->ptr;
                        
                        H_hsp_num=H2->hsp_link.num[index];
                        H_hsp_sum=H2->hsp_link.sum[index];
                        H_hsp_xsum=H2->hsp_link.xsum[index];
                        H_hsp_link=H2;
                     }
                  } /* end for H2_index... */
               } /* end if(H->score>cuttof[]) */
               { 
                  Int4 score=H->hsp->score;
                  double new_xsum = 
                     H_hsp_xsum + score*kbp[H->hsp->context]->Lambda -
                     kbp[H->hsp->context]->logK;
                  Int4 new_sum = H_hsp_sum + (score - cutoff[index]);
                  
                  H->hsp_link.sum[index] = new_sum;
                  H->hsp_link.num[index] = H_hsp_num+1;
                  H->hsp_link.link[index] = H_hsp_link;
                  lh_helper[H_index].sum[index] = new_sum;
                  lh_helper[H_index].maxsum1 = MAX(lh_helper[H_index-1].maxsum1, new_sum);
                  /* Update this entry's 'next_larger' field */
                  {
                     Int4 cur_sum=lh_helper[H_index].sum[1];
                     Int4 prev = H_index-1;
                     Int4 prev_sum = lh_helper[prev].sum[1];
                     while((cur_sum>=prev_sum) && (prev>0)){
                        prev=lh_helper[prev].next_larger;
                        prev_sum = lh_helper[prev].sum[1];
                     }
                     lh_helper[H_index].next_larger = prev;
                  }
                  
                  if (new_sum >= maxscore) 
                  {
                     maxscore=new_sum;
                     best[index]=H;
                  }
                  H->hsp_link.xsum[index] = new_xsum;
                  if(H_hsp_link)
                     ((LinkHSPStruct*)H_hsp_link)->linked_to++;
               }
            }
            path_changed=0;
            first_pass=0;
         }
         /********************************/
         if (!ignore_small_gaps)
         {
            /* Select the best ordering method.
               First we add back in the value cutoff[index] * the number
               of links, as this was subtracted out for purposes of the
               comparison above. */
            best[0]->hsp_link.sum[0] +=
               (best[0]->hsp_link.num[0])*cutoff[0];

            prob[0] = BLAST_SmallGapSumE(window_size,
                         best[0]->hsp_link.num[0], best[0]->hsp_link.xsum[0],
                         query_length, subject_length,
                         query_info->contexts[query_context].eff_searchsp,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[0]->hsp_link.num[0]) );

            /* Adjust the e-value because we are performing multiple tests */
            if( best[0]->hsp_link.num[0] > 1 ) {
              if( gap_prob == 0 || (prob[0] /= gap_prob) > INT4_MAX ) {
                prob[0] = INT4_MAX;
              }
            }

            prob[1] = BLAST_LargeGapSumE(best[1]->hsp_link.num[1],
                         best[1]->hsp_link.xsum[1],
                         query_length, subject_length,
                         query_info->contexts[query_context].eff_searchsp,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[1]->hsp_link.num[1]));

            if( best[1]->hsp_link.num[1] > 1 ) {
              if( 1 - gap_prob == 0 || (prob[1] /= 1 - gap_prob) > INT4_MAX ) {
                prob[1] = INT4_MAX;
              }
            }
            ordering_method =
               prob[0]<=prob[1] ? eLinkSmallGaps : eLinkLargeGaps;
         }
         else
         {
            /* We only consider the case of big gaps. */
            best[1]->hsp_link.sum[1] +=
               (best[1]->hsp_link.num[1])*cutoff[1];

            prob[1] = BLAST_LargeGapSumE(
                         best[1]->hsp_link.num[1],
                         best[1]->hsp_link.xsum[1],
                         query_length, subject_length,
                         query_info->contexts[query_context].eff_searchsp,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[1]->hsp_link.num[1]));
            ordering_method = eLinkLargeGaps;
         }

         best[ordering_method]->start_of_chain = TRUE;
         best[ordering_method]->hsp->evalue    = prob[ordering_method];
         
         /* remove the links that have been ordered already. */
         if (best[ordering_method]->hsp_link.link[ordering_method])
            linked_set = TRUE;
         else
            linked_set = FALSE;

         if (best[ordering_method]->linked_to>0) path_changed=1;
         for (H=best[ordering_method]; H!=NULL;
              H=H->hsp_link.link[ordering_method]) 
         {
            if (H->linked_to>1) path_changed=1;
            H->linked_to=-1000;
            H->hsp_link.changed=1;
            /* record whether this is part of a linked set. */
            H->linked_set = linked_set;
            H->ordering_method = ordering_method;
            H->hsp->evalue = prob[ordering_method];
            if (H->next)
               (H->next)->prev=H->prev;
            if (H->prev)
               (H->prev)->next=H->next;
            number_of_hsps--;
         }
         
      } /* end while num_hsps... */
	} /* end for frame_index ... */

   sfree(hp_frame_start);
   sfree(hp_frame_number);
   sfree(hp_start->hsp);
   sfree(hp_start);

   if (translated_query) {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_RevCompareHSPsTransl);
      qsort(link_hsp_array, total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_FwdCompareHSPsTransl);
   } else {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_RevCompareHSPs);
      qsort(link_hsp_array, total_number_of_hsps,sizeof(LinkHSPStruct*), 
            s_FwdCompareHSPs);
   }

   /* Sort by starting position. */
   

	for (index=0, last_hsp=NULL;index<total_number_of_hsps; index++) 
	{
		H = link_hsp_array[index];
		H->prev = NULL;
		H->next = NULL;
	}

   /* hook up the HSP's. */
	first_hsp = NULL;
	for (index=0, last_hsp=NULL;index<total_number_of_hsps; index++) 
   {
		H = link_hsp_array[index];

      /* If this is not a single piece or the start of a chain, then Skip it. */
      if (H->linked_set == TRUE && H->start_of_chain == FALSE)
			continue;

      /* If the HSP has no "link" connect the "next", otherwise follow the "link"
         chain down, connecting them with "next" and "prev". */
		if (last_hsp == NULL)
			first_hsp = H;
		H->prev = last_hsp;
		ordering_method = H->ordering_method;
		if (H->hsp_link.link[ordering_method] == NULL)
		{
         /* Grab the next HSP that is not part of a chain or the start of a chain */
         /* The "next" pointers are not hooked up yet in HSP's further down array. */
         index1=index;
         H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         while (H2 && H2->linked_set == TRUE && 
                H2->start_of_chain == FALSE)
         {
            index1++;
		     	H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         }
         H->next= H2;
		}
		else
		{
			/* The first one has the number of links correct. */
			num_links = H->hsp_link.num[ordering_method];
			link = H->hsp_link.link[ordering_method];
			while (link)
			{
				H->hsp->num = num_links;
                H->xsum = H->hsp_link.xsum[ordering_method];
				H->next = (LinkHSPStruct*) link;
				H->prev = last_hsp;
				last_hsp = H;
				H = H->next;
				if (H != NULL)
				    link = H->hsp_link.link[ordering_method];
				else
				    break;
			}
			/* Set these for last link in chain. */
			H->hsp->num = num_links;
            H->xsum = H->hsp_link.xsum[ordering_method];
         /* Grab the next HSP that is not part of a chain or the start of a chain */
         index1=index;
         H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         while (H2 && H2->linked_set == TRUE && 
                H2->start_of_chain == FALSE)
		   {
            index1++;
            H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
			}
         H->next= H2;
			H->prev = last_hsp;
		}
		last_hsp = H;
	}
	
   /* The HSP's may be in a different order than they were before, 
      but first_hsp contains the first one. */
   for (index = 0, H = first_hsp; index < hsp_list->hspcnt; index++) {
      hsp_list->hsp_array[index] = H->hsp;
      /* Free the wrapper structure */
      H2 = H->next;
      sfree(H);
      H = H2;
   }
   sfree(link_hsp_array);
   sfree(lh_helper);

   return 0;
}

/******************************************************************************
 * Structures and functions used only in uneven gap linking method.           * 
 ******************************************************************************/

/** Simple doubly linked list of HSPs, used for calculating sum statistics. */ 
typedef struct BlastLinkedHSPSet {
    BlastHSP* hsp;                 /**< HSP for the current link in the chain. */
    struct BlastLinkedHSPSet* next;/**< Next link in the chain. */
    struct BlastLinkedHSPSet* prev;/**< Previous link in the chain. */
    double sum_score;              /**< Sum bit score for the linked set. */
} BlastLinkedHSPSet;

/** Calculates e-value of a set of HSPs with sum statistics.
 * @param program_number Type of BLAST program [in]
 * @param query_info Query information structure [in]
 * @param subject_length Subject sequence length [in]
 * @param link_hsp_params Parameters for linking HSPs [in]
 * @param head_hsp Set of HSPs with previously calculated sum score/evalue [in]
 * @param new_hsp New HSP candidate to join the set [in]
 * @param sum_score Normalized score for the collection if HSPs[out]
 * @return E-value of all the HSPs together
 */
static double 
s_SumHSPEvalue(EBlastProgramType program_number, 
   BlastQueryInfo* query_info, Int4 subject_length, 
   const BlastLinkHSPParameters* link_hsp_params, 
   BlastLinkedHSPSet* head_hsp, BlastLinkedHSPSet* new_hsp, double* sum_score)
{
   double gap_decay_rate, sum_evalue;
   Int2 num;
   Int4 subject_eff_length, query_eff_length, length_adjustment;
   Int4 context = head_hsp->hsp->context;
   Int4 query_window_size;
   Int4 subject_window_size;
   
   gap_decay_rate = link_hsp_params->gap_decay_rate;

   num = head_hsp->hsp->num + new_hsp->hsp->num;

   length_adjustment = query_info->contexts[context].length_adjustment;

   subject_eff_length = MAX((subject_length - length_adjustment), 1);
   if (program_number == eBlastTypeTblastn ||
       program_number == eBlastTypeBlastx) {
      subject_eff_length /= 3;
   }
   subject_eff_length = MAX(subject_eff_length, 1);
   
   query_eff_length = 
      MAX(query_info->contexts[context].query_length - length_adjustment, 1);
   
   *sum_score = new_hsp->sum_score + head_hsp->sum_score;

   query_window_size = 
      link_hsp_params->overlap_size + link_hsp_params->gap_size + 1;
   subject_window_size = 
      link_hsp_params->overlap_size + link_hsp_params->longest_intron + 1;

   sum_evalue = 
       BLAST_UnevenGapSumE(query_window_size, subject_window_size,
          num, *sum_score, query_eff_length, subject_eff_length,
          query_info->contexts[context].eff_searchsp,
          BLAST_GapDecayDivisor(gap_decay_rate, num));

   return sum_evalue;
}

/** Callback for sorting an array of HSPs, encapsulated in BlastLinkedHSPSet
 * structures, in order of increasing query starting offset.
 * The subject end offset of HSPs is used as a tiebreaker, and no HSPs may be
 * NULL. The comparison is applied only to HSPs from the same context. 
 * Otherwise, the sorting is in increasing order of contexts.
 * @param v1 first HSP in list [in]
 * @param v2 second HSP in list [in]
 * @return -1, 0, or 1 depending on HSPs
 */
static int
s_FwdCompareLinkedHSPSets(const void* v1, const void* v2)
{
	BlastLinkedHSPSet** hp1,** hp2;
    BlastHSP* hsp1,* hsp2;

	hp1 = (BlastLinkedHSPSet**) v1;
	hp2 = (BlastLinkedHSPSet**) v2;

    hsp1 = (*hp1)->hsp;
    hsp2 = (*hp2)->hsp;

    if (hsp1->context < hsp2->context)
        return -1;
    else if (hsp1->context > hsp2->context)
        return 1;

	if (hsp1->query.offset < hsp2->query.offset) 
		return -1;
	if (hsp1->query.offset > hsp2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (hsp1->subject.offset < hsp2->subject.offset) 
		return -1;
	if (hsp1->subject.offset > hsp2->subject.offset) 
		return 1;

	return 0;
}

/** Callback used by qsort to sort a list of BlastLinkedHSPSet structures
 *  in order of decreasing sum score. Entries in the list may be NULL
 *  @param v1 first HSP in list [in]
 *  @param v2 second HSP in list [in]
 *  @return -1, 0, or 1 depending on HSPs
*/
static int
s_SumScoreCompareLinkedHSPSets(const void* v1, const void* v2)
{
    BlastLinkedHSPSet* h1,* h2;
    BlastLinkedHSPSet** hp1,** hp2;

    hp1 = (BlastLinkedHSPSet**) v1;
    hp2 = (BlastLinkedHSPSet**) v2;
    h1 = *hp1;
    h2 = *hp2;

    if (!h1 && !h2)
        return 0;
    else if (!h1) 
        return 1;
    else if (!h2)
        return -1;

    if (h1->sum_score < h2->sum_score)
        return 1;
    if (h1->sum_score > h2->sum_score)
        return -1;

    return 0;
}

/** Find an HSP on the same context as the one given, with closest start offset
 * that is greater than a specified value. The list of HSPs to search must 
 * be sorted by query offset and in increasing order of contexts.
 * @param hsp_array List of pointers to HSPs, encapsulated within 
 *                  BlastLinkedHSPSet structures [in]
 * @param size Number of elements in the array [in]
 * @param context Context of the target HSP [in]
 * @param offset The target offset to search for [in]
 * @return The index in the array of the HSP whose start/end offset 
 *         is closest to but >= the value 'offset'
 */
static Int4 
s_HSPOffsetBinarySearch(BlastLinkedHSPSet** hsp_array, Int4 size, 
                        Int4 context, Int4 offset)
{
   Int4 index, begin, end;
   
   begin = 0;
   end = size;
   while (begin < end) {
      index = (begin + end) / 2;

      if (hsp_array[index]->hsp->context < context)
          begin = index + 1;
      else if (hsp_array[index]->hsp->context > context)
          end = index;
      else {
          if (hsp_array[index]->hsp->query.offset >= offset) 
              end = index;
          else
              begin = index + 1;
      }
   }

   return end;
}

/** Find an HSP in an array sorted in increasing order of query offsets and 
 * increasing order of contexts, with the smallest index such that its query end
 * is >= to a given offset.
 * @param hsp_array Array of pointers to HSPs, encapsulated within 
 *                  BlastLinkedHSPSet structures. Must be sorted by context and
 *                  query offsets. [in]
 * @param size Number of elements in the array [in]
 * @param qend_index_array Array indexing query ends in the hsp_array [in]
 * @param context Context of the target HSP [in]
 * @param offset The target offset to search for [in]
 * @return The found index in the hsp_array.
 */
static Int4 
s_HSPOffsetEndBinarySearch(BlastLinkedHSPSet** hsp_array, Int4 size, 
                           Int4* qend_index_array, Int4 context, Int4 offset)
{
   Int4 begin, end;
   
   begin = 0;
   end = size;
   while (begin < end) {
       Int4 right_index = (begin + end) / 2;
       Int4 left_index = qend_index_array[right_index];

       if (hsp_array[right_index]->hsp->context < context)
           begin = right_index + 1;
       else if (hsp_array[right_index]->hsp->context > context)
           end = left_index;
       else {
           if (hsp_array[left_index]->hsp->query.end >= offset) 
               end = left_index;
           else
               begin = right_index + 1;
       }
   }

   return end;
}

/** Merges HSPs from two linked HSP sets into an array of HSPs, sorted in 
 * increasing order of contexts and increasing order of query offsets. 
 * @param hsp_set1 First linked set. [in]
 * @param hsp_set2 Second linked set. [in]
 * @param merged_size The total number of HSPs in two sets. [out]
 * @return The array of pointers to HSPs representing a merged set.
 */
static BlastLinkedHSPSet**
s_MergeLinkedHSPSets(BlastLinkedHSPSet* hsp_set1, BlastLinkedHSPSet* hsp_set2,
                     Int4* merged_size)
{
    Int4 index;
    Int4 length;
    BlastLinkedHSPSet** merged_hsps;

    /* Find the first link of the old HSP chain. */
    while (hsp_set1->prev)
        hsp_set1 = hsp_set1->prev;
    /* Find first and last link in the new HSP chain. */
    while (hsp_set2->prev)
        hsp_set2 = hsp_set2->prev;
    
    *merged_size = length = hsp_set1->hsp->num + hsp_set2->hsp->num;
        
    merged_hsps = (BlastLinkedHSPSet**) 
        malloc(length*sizeof(BlastLinkedHSPSet*));
    
    index = 0;
    while (hsp_set1 || hsp_set2) {
        /* NB: HSP sets for which some HSPs have identical query offsets cannot 
           possibly be admissible, so it doesn't matter how to deal with equal
           offsets. */
        if (!hsp_set2 || (hsp_set1 && 
            hsp_set1->hsp->query.offset < hsp_set2->hsp->query.offset)) {
            merged_hsps[index] = hsp_set1;
            hsp_set1 = hsp_set1->next;
        } else { 
            merged_hsps[index] = hsp_set2;
            hsp_set2 = hsp_set2->next;
        }
        ++index;
    }
    return merged_hsps;
}

/** Combines two linked sets of HSPs into a single set. 
 * @param hsp_set1 First set of HSPs [in]
 * @param hsp_set2 Second set of HSPs [in]
 * @param sum_score The sum score of the combined linked set
 * @param evalue The E-value of the combined linked set
 * @return Combined linked set.
 */
static BlastLinkedHSPSet*
s_CombineLinkedHSPSets(BlastLinkedHSPSet* hsp_set1, BlastLinkedHSPSet* hsp_set2, 
                       double sum_score, double evalue)
{
    BlastLinkedHSPSet** merged_hsps; 
    BlastLinkedHSPSet* head_hsp;
    Int4 index, new_num;

    if (!hsp_set2)
        return hsp_set1;
    else if (!hsp_set1)
        return hsp_set2;

    merged_hsps = s_MergeLinkedHSPSets(hsp_set1, hsp_set2, &new_num);

    head_hsp = merged_hsps[0];
    head_hsp->prev = NULL;
    for (index = 0; index < new_num; ++index) {
        BlastLinkedHSPSet* link = merged_hsps[index];
        if (index < new_num - 1) {
            BlastLinkedHSPSet* next_link = merged_hsps[index+1];
            link->next = next_link;
            next_link->prev = link;
        } else {
            link->next = NULL;
        }
        link->sum_score = sum_score;
        link->hsp->evalue = evalue;
        link->hsp->num = new_num; 
    }
    
    sfree(merged_hsps);
    return head_hsp;
}

/** Checks if new candidate HSP is admissible to be linked to a set of HSPs on
 * the left. The new HSP must start strictly before the parent HSP in both query
 * and subject, and its end must lie within an interval from the parent HSP's 
 * start, determined by the allowed gap and overlap sizes in query and subject.
 * This function also indicates whether parent is already too far to the right
 * of the candidate HSP, via a boolean pointer.
 * @param hsp_set1 First linked set of HSPs. [in]
 * @param hsp_set2 Second linked set of HSPs. [in]
 * @param link_hsp_params Parameters for linking HSPs. [in]
 * @return Do the two sets satisfy the admissibility criteria to form a 
 *         combined set? 
 */
static Boolean
s_LinkedHSPSetsAdmissible(BlastLinkedHSPSet* hsp_set1, 
                          BlastLinkedHSPSet* hsp_set2, 
                          const BlastLinkHSPParameters* link_hsp_params)
{
    Int4 longest_intron;
    Int4 gap_size, overlap_size;
    BlastLinkedHSPSet** merged_hsps;   
    Int4 combined_size = 0;
    Int4 index;

    if (!hsp_set1 || !hsp_set2 || !link_hsp_params) 
        return FALSE;

    /* The first input HSP must be the head of its set. */
    if (hsp_set1->prev)
        return FALSE;

    /* The second input HSP may not be the head of its set. Hence follow the 
       previous pointers to get to the head. */
    for ( ; hsp_set2->prev; hsp_set2 = hsp_set2->prev);

    /* If left and right HSP are the same, return inadmissible status. */
    if (hsp_set1 == hsp_set2)
        return FALSE;

    /* Check if these HSPs are for the same protein sequence (same context) */
    if (hsp_set1->hsp->context != hsp_set2->hsp->context)
        return FALSE;

    /* Check if new HSP and hsp_set2 are on the same nucleotide sequence strand.
       (same sign of subject frame) */
    if (SIGN(hsp_set1->hsp->subject.frame) != 
        SIGN(hsp_set2->hsp->subject.frame))
        return FALSE;

    /* Merge the two sets into an array with increasing order of query 
       offsets. */
    merged_hsps = s_MergeLinkedHSPSets(hsp_set1, hsp_set2, &combined_size);

    longest_intron = link_hsp_params->longest_intron; /* Maximal gap size in
                                                         subject */
    gap_size = link_hsp_params->gap_size; /* Maximal gap size in query */
    overlap_size = link_hsp_params->overlap_size; /* Maximal overlap size in
                                                     query or subject */

    for (index = 0; index < combined_size - 1; ++index) {
        BlastLinkedHSPSet* left_hsp = merged_hsps[index];
        BlastLinkedHSPSet* right_hsp = merged_hsps[index+1];
        

        /* If the new HSP is too far to the left from the right_hsp, indicate this by 
           setting the boolean output value to TRUE. */
        if (left_hsp->hsp->query.end < right_hsp->hsp->query.offset - gap_size)
            break;
        
        /* Check if the left HSP's query offset is to the right of the right HSP's 
           offset, i.e. they came in wrong order. */
        if (left_hsp->hsp->query.offset >= right_hsp->hsp->query.offset)
            break;

        /* Check the remaining condition for query offsets: left HSP cannot end 
           further than the maximal allowed overlap from the right HSP's offset;
           and left HSP must end before the right HSP. */
        if (left_hsp->hsp->query.end > right_hsp->hsp->query.offset + overlap_size ||
            left_hsp->hsp->query.end >= right_hsp->hsp->query.end)
            break;
        
        /* Check the subject offsets conditions. */
        if (left_hsp->hsp->subject.end > 
            right_hsp->hsp->subject.offset + overlap_size || 
            left_hsp->hsp->subject.end < 
            right_hsp->hsp->subject.offset - longest_intron ||
            left_hsp->hsp->subject.offset >= right_hsp->hsp->subject.offset ||
            left_hsp->hsp->subject.end >= right_hsp->hsp->subject.end)
            break;
    }

    sfree(merged_hsps);

    if (index < combined_size - 1)
        return FALSE;
    
    return TRUE;
}

/** Sets up an array of wrapper structures for an array of BlastHSP's.
 * @param hsp_array Original array of HSP structures. [in]
 * @param hspcnt Size of hsp_array. [in]
 * @param kbp_array Array of Karlin blocks - structures containing 
 *                  Karlin-Altschul parameters. [in]
 * @param program BLAST program (tblastn or blastx) [in]
 * @return Array of wrapper structures, used for linking HSPs.
 */
static BlastLinkedHSPSet**
s_LinkedHSPSetArraySetUp(BlastHSP** hsp_array, Int4 hspcnt, 
                          Blast_KarlinBlk ** kbp_array,
                          EBlastProgramType program)
{
    Int4 index;
    BlastLinkedHSPSet** link_hsp_array = 
        (BlastLinkedHSPSet**) malloc(hspcnt*sizeof(BlastLinkedHSPSet*));

    for (index = 0; index < hspcnt; ++index) {
        BlastHSP * hsp = hsp_array[index];
        link_hsp_array[index] =
            (BlastLinkedHSPSet*) calloc(1, sizeof(BlastLinkedHSPSet));
        
        link_hsp_array[index]->hsp = hsp;
        link_hsp_array[index]->sum_score =
            kbp_array[hsp->context]->Lambda * hsp->score -
            kbp_array[hsp->context]->logK;
        
        hsp_array[index]->num = 1;
    }
    
    if (program == eBlastTypeBlastx) {
        BlastSeg seg;
        /* Create a temporary hsp_array with query and subject switched.
         * Also adjust context numbers so they distinguish HSPs from different
         * queries, but not different frames, since frame distinction is moved
         * to the subject side.
         */
        for (index = 0; index < hspcnt; ++index) {
            seg = hsp_array[index]->query;
            hsp_array[index]->query = hsp_array[index]->subject;
            hsp_array[index]->subject = seg;
            hsp_array[index]->context = 
                Blast_GetQueryIndexFromContext(hsp_array[index]->context, program);
        }
    }
    return link_hsp_array;
}

/** Frees the array of special structures, used for linking HSPs and restores 
 * the original contexts and subject/query order in BlastHSP structures, when
 * necessary.
 * @param link_hsp_array Array of wrapper HSP structures, used for linking. [in]
 * @param hspcnt Size of the array. [in]
 * @param program Type of BLAST program (blastx or tblastn) [in]
 * @return NULL.
 */ 
static BlastLinkedHSPSet**
s_LinkedHSPSetArrayCleanUp(BlastLinkedHSPSet** link_hsp_array, Int4 hspcnt,
                           EBlastProgramType program)
{
    Int4 index;

    /* Free the BlastLinkedHSPSet wrapper structures. For blastx, also switch back
       subject and query and restore the context values in all HSPs. */
    for (index = 0; index < hspcnt; ++index) {
        BlastLinkedHSPSet* lhsp = link_hsp_array[index];
        
        if (program == eBlastTypeBlastx) {
            BlastSeg seg;
            
            seg = lhsp->hsp->query;
            lhsp->hsp->query = lhsp->hsp->subject;
            lhsp->hsp->subject = seg;
            /* Also restore the correct context number. */
            lhsp->hsp->context = lhsp->hsp->context * NUM_FRAMES +
                FrameToContext(lhsp->hsp->query.frame);
        }
        sfree(lhsp);
    }
    sfree(link_hsp_array);
    return NULL;
}

/** Given an array of HSPs (H), sorted in increasing order of query offsets, 
 * fills an array of indices into array H such that for each i, the index is the
 * smallest HSP index, for which query ending offset is >= than query ending 
 * offset of H[i]. This indexing is performed before any of the HSPs in H are
 * linked.
 * @param hsp_array Array of wrapper HSP structures. [in]
 * @param hspcnt Size of the hsp_array. [in]
 * @param qend_index_ptr Pointer to an array of special structures indexing the
 *                       largest query ends in an HSP array sorting by query 
 *                       offset.
 */  
static Int2
s_LinkedHSPSetArrayIndexQueryEnds(BlastLinkedHSPSet** hsp_array, Int4 hspcnt, 
                                  Int4** qend_index_ptr)
{
    Int4 index;
    Int4* qend_index_array = NULL;
    BlastLinkedHSPSet* link;
    Int4 current_end = 0;
    Int4 current_index = 0;

    /* Allocate the array. */
    *qend_index_ptr = qend_index_array = (Int4*) calloc(hspcnt, sizeof(Int4));
    if (!qend_index_array)
        return -1;

    current_end = hsp_array[0]->hsp->query.end;

    for (index = 1; index < hspcnt; ++index) {
        link = hsp_array[index];
        if (link->hsp->context > hsp_array[current_index]->hsp->context ||
            link->hsp->query.end > current_end) {
            current_index = index;
            current_end = link->hsp->query.end;
        }
        qend_index_array[index] = current_index;
    }
    return 0;
}


/** Greedy algorithm to link HSPs with uneven gaps.
 * Sorts HSPs by score. Starting with the highest scoring HSP, finds
 * an HSP that produces the best sum e-value when added to the HSP set under 
 * consideration. The neighboring HSPs in a set must have endpoints within a
 * window of each other on the protein axis, and within the longest allowed 
 * intron length on the nucleotide axis. When no more HSPs can be added to the
 * highest scoring set, the next highest scoring HSP is considered that is not 
 * yet part of any set.
 * @param program Type of BLAST program (blastx or tblastn) [in]
 * @param hsp_list Structure containing all HSPs for a given subject [in] [out]
 * @param query_info Query information, including effective lengths [in]
 * @param subject_length Subject sequence length [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param link_hsp_params Parameters for linking HSPs [in]
 * @param gapped_calculation TRUE if input HSPs are from a gapped search [in]
 */
static Int2
s_BlastUnevenGapLinkHSPs(EBlastProgramType program, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length, BlastScoreBlk* sbp, 
   const BlastLinkHSPParameters* link_hsp_params, Boolean gapped_calculation)
{
   BlastHSP** hsp_array;
   BlastLinkedHSPSet** link_hsp_array;
   BlastLinkedHSPSet** score_hsp_array;  /* an array of HSPs sorted by decreasing 
                                        score */
   BlastLinkedHSPSet** offset_hsp_array; /* an array of HSPs sorted by increasing
                                        query offset */
   BlastLinkedHSPSet* head_hsp;
   Int4 hspcnt, index, index1;
   Int4 overlap_size, gap_size;
   Blast_KarlinBlk ** kbp_array;
   Int4* qend_index_array = NULL;

   /* Check input arguments. */
   if (!link_hsp_params || !sbp || !query_info)
       return -1;

   /* If HSP list is not available or has <= 1 HSPs, there is nothing to do. */
   if (!hsp_list || hsp_list->hspcnt <= 1)
       return 0;

   if(gapped_calculation) {
       kbp_array = sbp->kbp_gap;
   } else {
       kbp_array = sbp->kbp;
   }

   overlap_size = link_hsp_params->overlap_size; /* Maximal overlap size in
                                                    query or subject */
   gap_size = link_hsp_params->gap_size; /* Maximal gap size in query */

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   /* Set up an array of HSP structure wrappers. */
   link_hsp_array = 
       s_LinkedHSPSetArraySetUp(hsp_array, hspcnt, kbp_array, program);

   /* Allocate, fill and sort the auxiliary arrays. */
   score_hsp_array = 
       (BlastLinkedHSPSet**) malloc(hspcnt*sizeof(BlastLinkedHSPSet*));
   memcpy(score_hsp_array, link_hsp_array, hspcnt*sizeof(BlastLinkedHSPSet*));
   qsort(score_hsp_array, hspcnt, sizeof(BlastLinkedHSPSet*), 
         s_SumScoreCompareLinkedHSPSets);
   offset_hsp_array = 
       (BlastLinkedHSPSet**) malloc(hspcnt*sizeof(BlastLinkedHSPSet*));
   memcpy(offset_hsp_array, link_hsp_array, hspcnt*sizeof(BlastLinkedHSPSet*));
   qsort(offset_hsp_array, hspcnt, sizeof(BlastLinkedHSPSet*), 
         s_FwdCompareLinkedHSPSets);

   s_LinkedHSPSetArrayIndexQueryEnds(offset_hsp_array, hspcnt, &qend_index_array);

   /* head_hsp is set to NULL whenever there is no current linked set that is
      being worked on. */
   head_hsp = NULL;
   for (index = 0; index < hspcnt && score_hsp_array[index]; ) {
       double best_evalue, best_sum_score = 0;
       BlastLinkedHSPSet* best_hsp = NULL;
       BlastLinkedHSPSet* tail_hsp = NULL;
       Int4 hsp_index_left, hsp_index_right;
       Int4 left_offset;

       if (!head_hsp) {
           /* Find the highest scoring HSP that is not yet part of a linked set.
              An HSP is part of a linked set if and only if either prev or next
              pointer is not NULL. */
           while (index<hspcnt && score_hsp_array[index] && 
                  (score_hsp_array[index]->next ||
                   score_hsp_array[index]->prev))
               index++;
           if (index==hspcnt)
               break;
           head_hsp = score_hsp_array[index];
       }
       /* Find the last link in the current HSP set. */
       for (tail_hsp = head_hsp; tail_hsp->next; tail_hsp = tail_hsp->next);

       best_evalue = head_hsp->hsp->evalue;
       best_sum_score = head_hsp->sum_score;
       /* left_offset is the leftmost point where an HSP can end to be
          admissible for linking with head_hsp. */
       left_offset = head_hsp->hsp->query.offset - gap_size;

       /* Find the smallest index in the offset array, for which an HSP can 
          possibly be added to the set currently being explored. */
       hsp_index_left = 
           s_HSPOffsetEndBinarySearch(offset_hsp_array, hspcnt, qend_index_array,
                                       head_hsp->hsp->context, left_offset);

       /* Find the largest index in the offset array, for which an HSP can be
          possibly added to the currently explored set. */
       hsp_index_right = 
           s_HSPOffsetBinarySearch(offset_hsp_array, hspcnt, 
                                   tail_hsp->hsp->context, 
                                   tail_hsp->hsp->query.end + gap_size);
       
       for (index1 = hsp_index_left; index1 < hsp_index_right; ++index1) {
           BlastLinkedHSPSet* lhsp = offset_hsp_array[index1];

           /* From each previously linked HSP set consider only one 
              representative - the leftmost HSP whose query end is 
              >= left_offset. */
           if (lhsp->prev && lhsp->prev->hsp->query.end >= left_offset)
               continue;

           if (s_LinkedHSPSetsAdmissible(head_hsp, lhsp, link_hsp_params)) {
               double evalue, sum_score;
               /* Check if the e-value for the new combined HSP set is better
                  than for the previously obtained set. */
               if ((evalue = s_SumHSPEvalue(program, query_info, subject_length, 
                                            link_hsp_params, head_hsp, lhsp, 
                                            &sum_score)) < 
                   MIN(best_evalue, lhsp->hsp->evalue)) {
                   best_hsp = lhsp;
                   best_evalue = evalue;
                   best_sum_score = sum_score;
               }
           }
       }

      /* Link the new HSP to the set, if it qualified. */
      if (best_hsp) {
         head_hsp = s_CombineLinkedHSPSets(head_hsp, best_hsp, best_sum_score, 
                                           best_evalue);
      } else {
         head_hsp = NULL;
         ++index;
      }
   }
  
   /* Free the auxiliary arrays. */
   sfree(score_hsp_array);
   sfree(offset_hsp_array);
   sfree(qend_index_array);

   /* Do the final clean up. */
   s_LinkedHSPSetArrayCleanUp(link_hsp_array, hspcnt, program);

   return 0;
}

/* see description in link_hsps.h */
Int2 
BLAST_LinkHsps(EBlastProgramType program_number, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length,
   BlastScoreBlk* sbp, const BlastLinkHSPParameters* link_hsp_params,
   Boolean gapped_calculation)
{
    Int4 index;

    if (!hsp_list || hsp_list->hspcnt == 0)
        return 0;

    /* Remove any information on number of linked HSPs from previous
       linking. */
    for (index = 0; index < hsp_list->hspcnt; ++index)
        hsp_list->hsp_array[index]->num = 0;
    
    /* Link up the HSP's for this hsp_list. */
    if (link_hsp_params->longest_intron <= 0) {
        s_BlastEvenGapLinkHSPs(program_number, hsp_list, query_info, 
                              subject_length, sbp, link_hsp_params, 
                              gapped_calculation);
        /* The HSP's may be in a different order than they were before, 
           but hsp contains the first one. */
    } else {
        /* Calculate individual HSP e-values first - they'll be needed to
           compare with sum e-values. Use decay rate to compensate for 
           multiple tests. */
        Blast_HSPListGetEvalues(query_info, hsp_list, gapped_calculation, sbp, 
                                link_hsp_params->gap_decay_rate, 1.0);
        
        s_BlastUnevenGapLinkHSPs(program_number, hsp_list, query_info, 
                                subject_length, sbp, link_hsp_params,
                                gapped_calculation);
    }

    /* Sort the HSP array by score */
    Blast_HSPListSortByScore(hsp_list);

    /* Find and fill the best e-value */
    hsp_list->best_evalue = hsp_list->hsp_array[0]->evalue;
    for (index = 1; index < hsp_list->hspcnt; ++index) {
        if (hsp_list->hsp_array[index]->evalue < hsp_list->best_evalue)
            hsp_list->best_evalue = hsp_list->hsp_array[index]->evalue;
    }

    return 0;
}

