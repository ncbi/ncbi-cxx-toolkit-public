static char const rcsid[] = "$Id$";
/*
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

File name: blast_filter.c

Author: Ilya Dondoshansky

Contents: All code related to query sequence masking/filtering for BLAST

******************************************************************************
 * $Revision$
 * */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_dust.h>
#include <algo/blast/core/blast_seg.h>
#ifdef CC_FILTER_ALLOWED
#include <algo/blast/core/urkpcc.h>
#endif

/* The following function will replace BlastSetUp_CreateSSeqRange */

BlastSeqLoc* BlastSeqLocNew(Int4 from, Int4 to)
{
   BlastSeqLoc* loc = (BlastSeqLoc*) calloc(1, sizeof(BlastSeqLoc));
   SSeqRange* di = (SSeqRange*) malloc(sizeof(SSeqRange));

   di->left = from;
   di->right = to;
   loc->ptr = di;
   return loc;
}

BlastSeqLoc* BlastSeqLocFree(BlastSeqLoc* loc)
{
   SSeqRange* dintp;
   BlastSeqLoc* next_loc;

   while (loc) {
      next_loc = loc->next;
      dintp = (SSeqRange*) loc->ptr;
      sfree(dintp);
      sfree(loc);
      loc = next_loc;
   }
   return NULL;
}

BlastMaskLoc* BlastMaskLocNew(Int4 index, BlastSeqLoc *loc_list)
{
      BlastMaskLoc* retval = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
      retval->index = index;
      retval->loc_list = loc_list;
      return retval;
}

BlastMaskLoc* BlastMaskLocFree(BlastMaskLoc* mask_loc)
{
   BlastMaskLoc* next_loc;
   while (mask_loc) {
      next_loc = mask_loc->next;
      BlastSeqLocFree(mask_loc->loc_list);
      sfree(mask_loc);
      mask_loc = next_loc;
   }
   return NULL;
}

/** Used for qsort, compares two SeqLoc's by starting position. */
static int SSeqRangeSortByStartPosition(const void *vp1, const void *vp2)
{
   ListNode* v1 = *((ListNode**) vp1);
   ListNode* v2 = *((ListNode**) vp2);
   SSeqRange* loc1 = (SSeqRange*) v1->ptr;
   SSeqRange* loc2 = (SSeqRange*) v2->ptr;
   
   if (loc1->left < loc2->left)
      return -1;
   else if (loc1->left > loc2->left)
      return 1;
   else
      return 0;
}

/* This will go in place of CombineSeqLocs to combine filtered locations */
Int2
CombineMaskLocations(BlastSeqLoc* mask_loc, BlastSeqLoc* *mask_loc_out,
                     Int4 link_value)
{
   Int2 status=0;		/* return value. */
   Int4 start, stop;	/* USed to merge overlapping SeqLoc's. */
   SSeqRange* di = NULL,* di_next = NULL,* di_tmp=NULL;
   BlastSeqLoc* loc_head=NULL,* last_loc=NULL,* loc_var=NULL;
   BlastSeqLoc* new_loc = NULL,* new_loc_last = NULL;
   
   if (!mask_loc) {
      *mask_loc_out = NULL;
      return 0;
   }

   /* Put all the SeqLoc's into one big linked list. */
   loc_head = last_loc = 
      (BlastSeqLoc*) BlastMemDup(mask_loc, sizeof(BlastSeqLoc));
         
   /* Copy all locations, so loc points at the end of the chain */
   while (last_loc->next) {		
      last_loc->next = 
         (BlastSeqLoc*) BlastMemDup(last_loc->next, sizeof(BlastSeqLoc));
      last_loc = last_loc->next;
   }
   
   /* Sort them by starting position. */
   loc_head = (BlastSeqLoc*) 
      ListNodeSort ((ListNode*) loc_head, 
                   SSeqRangeSortByStartPosition);
   
   di = (SSeqRange*) loc_head->ptr;
   start = di->left;
   stop = di->right;
   loc_var = loc_head;
   
   while (loc_var) {
      di = loc_var->ptr;
      if (loc_var->next)
         di_next = loc_var->next->ptr;
      if (di_next && ((stop + link_value) > di_next->left)) {
         stop = MAX(stop, di_next->right);
      } else {
         di_tmp = (SSeqRange*) malloc(sizeof(SSeqRange));
         di_tmp->left = start;
         di_tmp->right = stop;
         if (!new_loc)
            new_loc_last = ListNodeAddPointer(&new_loc, 0, di_tmp);
         else
            new_loc_last = ListNodeAddPointer(&new_loc_last, 0, di_tmp);
         if (loc_var->next) {
             start = di_next->left;
             stop = di_next->right;
         }
      }
      loc_var = loc_var->next;
      di_next = NULL;
   }

   *mask_loc_out = new_loc;
      
   /* Free memory allocated for the temporary list of SeqLocs */
   while (loc_head) {
      loc_var = loc_head->next;
      sfree(loc_head);
      loc_head = loc_var;
   }
   return status;
}

Int2 
BLAST_ComplementMaskLocations(Uint1 program_number, 
   BlastQueryInfo* query_info, 
   BlastMaskLoc* mask_loc, BlastSeqLoc* *complement_mask) 
{
   Int4 start_offset, end_offset, filter_start, filter_end;
   Int4 context, index;
   BlastSeqLoc* loc,* last_loc = NULL,* start_loc = NULL;
   SSeqRange* double_int = NULL,* di;
   Boolean first;	/* Specifies beginning of query. */
   Boolean last_interval_open=TRUE; /* if TRUE last interval needs to be closed. */
   Boolean reverse = FALSE;
   const Boolean k_is_na = (program_number == blast_type_blastn);

   if (complement_mask == NULL)
	return -1;

   *complement_mask = NULL;

   for (context = query_info->first_context; 
        context <= query_info->last_context; ++context) {
      start_offset = query_info->context_offsets[context];
      end_offset = query_info->context_offsets[context+1] - 2;
      /* For blastn: check if this strand is not searched at all */
      if (end_offset < start_offset)
          continue;
      index = (k_is_na ? context / 2 : context);
      reverse = (k_is_na && ((context & 1) != 0));
      first = TRUE;

      if (!reverse) {
         for ( ; mask_loc && mask_loc->index < index; 
               mask_loc = mask_loc->next);
      }

      if (!mask_loc || (mask_loc->index > index) ||
          !mask_loc->loc_list) {
         /* No masks for this context */
         double_int = (SSeqRange*) calloc(1, sizeof(SSeqRange));
         double_int->left = start_offset;
         double_int->right = end_offset;
         if (!last_loc)
            last_loc = ListNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ListNodeAddPointer(&last_loc, 0, double_int);
         continue;
      }
      
      if (reverse) {
         BlastSeqLoc* prev_loc = NULL;
         /* Reverse the order of the locations */
         for (start_loc = mask_loc->loc_list; start_loc; 
              start_loc = start_loc->next) {
            loc = (BlastSeqLoc*) BlastMemDup(start_loc, sizeof(BlastSeqLoc));
            loc->next = prev_loc;
            prev_loc = loc;
         }
         /* Save where this list starts, so it can be freed later */
         start_loc = loc;
      } else {
         loc = mask_loc->loc_list;
      }

      for ( ; loc; loc = loc->next) {
         di = loc->ptr;
         if (reverse) {
            filter_start = end_offset - di->right;
            filter_end = end_offset - di->left;
         } else {
            filter_start = start_offset + di->left;
            filter_end = start_offset + di->right;
         }
         /* The canonical "state" at the top of this 
            while loop is that a SSeqRange has been 
            created and one field was filled in on the 
            last iteration. The first time this loop is 
            entered in a call to the funciton this is not
            true and the following "if" statement moves 
            everything to the canonical state. */
         if (first) {
            last_interval_open = TRUE;
            first = FALSE;
            double_int = (SSeqRange*) calloc(1, sizeof(SSeqRange));
            
            if (filter_start > start_offset) {
               /* beginning of sequence not filtered */
               double_int->left = start_offset;
            } else {
               /* beginning of sequence filtered */
               double_int->left = filter_end + 1;
               continue;
            }
         }

         double_int->right = filter_start - 1;

         if (!last_loc)
            last_loc = ListNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ListNodeAddPointer(&last_loc, 0, double_int);
         if (filter_end >= end_offset) {
            /* last masked region at end of sequence */
            last_interval_open = FALSE;
            break;
         } else {
            double_int = (SSeqRange*) calloc(1, sizeof(SSeqRange));
               double_int->left = filter_end + 1;
         }
      }

      if (reverse) {
         start_loc = ListNodeFree(start_loc);
      }
      
      if (last_interval_open) {
         /* Need to finish SSeqRange* for last interval. */
         double_int->right = end_offset;
         if (!last_loc)
            last_loc = ListNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ListNodeAddPointer(&last_loc, 0, double_int);
         last_interval_open = FALSE;
      }
   }
   return 0;
}

#define BLASTOPTIONS_BUFFER_SIZE 128

/** Parses options used for dust.
 * @param ptr buffer containing instructions. [in]
 * @param level sets level for dust. [out]
 * @param window sets window for dust [out]
 * @param cutoff sets cutoff for dust. [out] 
 * @param linker sets linker for dust. [out]
*/
static Int2
parse_dust_options(const char *ptr, Int2* level, Int2* window,
	Int2* cutoff, Int2* linker)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1, window_pri=-1, linker_pri=-1, level_pri=-1, cutoff_pri=-1;
	long	tmplong;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					level_pri = tmplong;
					break;
				case 1:
					sscanf(buffer, "%ld", &tmplong);
					window_pri = tmplong;
					break;
				case 2:
					sscanf(buffer, "%ld", &tmplong);
					cutoff_pri = tmplong;
					break;
				case 3:
					sscanf(buffer, "%ld", &tmplong);
					linker_pri = tmplong;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	*level = level_pri; 
	*window = window_pri; 
	*cutoff = cutoff_pri; 
	*linker = linker_pri; 

	return 0;
}

/** parses a string to set three seg options. 
 * @param ptr buffer containing instructions [in]
 * @param window returns "window" for seg algorithm. [out]
 * @param locut returns "locut" for seg. [out]
 * @param hicut returns "hicut" for seg. [out]
*/
static Int2
parse_seg_options(const char *ptr, Int4* window, double* locut, double* hicut)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1; 
	long	tmplong;
	double	tmpdouble;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					*window = tmplong;
					break;
				case 1:
					sscanf(buffer, "%le", &tmpdouble);
					*locut = tmpdouble;
					break;
				case 2:
					sscanf(buffer, "%le", &tmpdouble);
					*hicut = tmpdouble;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	return 0;
}

#ifdef CC_FILTER_ALLOWED
/** Coiled-coiled algorithm parameters. 
 * @param ptr buffer containing instructions. [in]
 * @param window returns window for coil-coiled algorithm. [out]
 * @param cutoff cutoff for coil-coiled algorithm [out]
 * @param linker returns linker [out]
*/
static Int2
parse_cc_options(const char *ptr, Int4* window, double* cutoff, Int4* linker)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1;
	long	tmplong;
	double	tmpdouble;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					*window = tmplong;
					break;
				case 1:
					sscanf(buffer, "%le", &tmpdouble);
					*cutoff = tmpdouble;
					break;
				case 2:
					sscanf(buffer, "%ld", &tmplong);
					*linker = tmplong;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	return 0;
}
#endif

/** Copies filtering commands for one filtering algorithm from "instructions" to
 * "buffer". 
 * ";" is a delimiter for the commands for different algorithms, so it stops
 * copying when a ";" is found. 
 * Example filtering string: "m L; R -d rodents.lib"
 * @param instructions filtering commands [in] 
 * @param buffer filled with filtering commands for one algorithm. [out]
*/
static const char *
BlastSetUp_load_options_to_buffer(const char *instructions, char* buffer)
{
	Boolean not_started=TRUE;
	char* buffer_ptr;
	const char *ptr;
	Int4 index;

	ptr = instructions;
	buffer_ptr = buffer;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE && *ptr != NULLB; index++)
	{
		if (*ptr == ';')
		{	/* ";" is a delimiter for different filtering algorithms. */
			ptr++;
			break;
		}
		/* Remove blanks at the beginning. */
		if (not_started && *ptr == ' ')
		{
			ptr++;
		}
		else
		{
			not_started = FALSE;
			*buffer_ptr = *ptr;
			buffer_ptr++; ptr++;
		}
	}

	*buffer_ptr = NULLB;

	if (not_started == FALSE)
	{	/* Remove trailing blanks. */
		buffer_ptr--;
		while (*buffer_ptr == ' ' && buffer_ptr > buffer)
		{
			*buffer_ptr = NULLB;
			buffer_ptr--;
		}
	}

	return ptr;
}

#define CC_WINDOW 22
#define CC_CUTOFF 40.0
#define CC_LINKER 32

Int2
BlastSetUp_Filter(Uint1 program_number, Uint1* sequence, Int4 length, 
   Int4 offset, char* instructions, Boolean *mask_at_hash, 
   BlastSeqLoc* *seqloc_retval)
{
	Boolean do_default=FALSE, do_seg=FALSE, do_dust=FALSE; 
	char* buffer=NULL;
   const char *ptr;
	Int2 seqloc_num;
	Int2 status=0;		/* return value. */
	Int2 window_dust, level_dust, minwin_dust, linker_dust;
   BlastSeqLoc* dust_loc = NULL,* seg_loc = NULL;
	SegParameters* sparamsp=NULL;
#ifdef CC_FILTER_ALLOWED
   Boolean do_coil_coil = FALSE;
   BlastSeqLoc* cc_loc = NULL;
	PccDatPtr pccp;
   Int4 window_cc, linker_cc;
	double cutoff_cc;
#endif
#ifdef TEMP_BLAST_OPTIONS
   /* TEMP_BLAST_OPTIONS is set to zero until these are implemented. */
   BlastSeqLoc* vs_loc = NULL,* repeat_loc = NULL;
	BLAST_OptionsBlkPtr repeat_options, vs_options;
	Boolean do_repeats=FALSE; 	/* screen for orgn. specific repeats. */
	Boolean do_vecscreen=FALSE;	/* screen for vector contamination. */
	Boolean myslp_allocated;
	char* repeat_database=NULL,* vs_database=NULL,* error_msg;
	SeqLocPtr repeat_slp=NULL, vs_slp=NULL;
	SeqAlignPtr seqalign;
	SeqLocPtr myslp, seqloc_var, seqloc_tmp;
	ListNode* vnp=NULL,* vnp_var;
#endif

#ifdef CC_FILTER_ALLOWED
	cutoff_cc = CC_CUTOFF;
#endif

   if (!seqloc_retval) 
      return -1;

	/* FALSE is the default right now. */
	if (mask_at_hash)
      *mask_at_hash = FALSE;
   
   *seqloc_retval = NULL;

	if (instructions == NULL || strcasecmp(instructions, "F") == 0)
		return status;

	/* parameters for dust. */
	/* -1 indicates defaults. */
	level_dust = -1;
	window_dust = -1;
	minwin_dust = -1;
	linker_dust = -1;
	if (strcasecmp(instructions, "T") == 0)
	{ /* do_default actually means seg for proteins and dust for nt. */
		do_default = TRUE;
	}
	else
	{
		buffer = (char*) calloc(strlen(instructions), sizeof(char));
		ptr = instructions;
		/* allow old-style filters when m cannot be followed by the ';' */
		if (*ptr == 'm' && ptr[1] == ' ')
		{
			if (mask_at_hash)
				*mask_at_hash = TRUE;
			ptr += 2;
		}
		while (*ptr != NULLB)
		{
			if (*ptr == 'S')
			{
				sparamsp = SegParametersNewAa();
				sparamsp->overlaps = TRUE;	/* merge overlapping segments. */
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
				{
					parse_seg_options(buffer, &sparamsp->window, &sparamsp->locut, &sparamsp->hicut);
				}
				do_seg = TRUE;
			}
			else if (*ptr == 'C')
			{
#ifdef CC_FILTER_ALLOWED
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				window_cc = CC_WINDOW;
				cutoff_cc = CC_CUTOFF;
				linker_cc = CC_LINKER;
				if (buffer[0] != NULLB)
					parse_cc_options(buffer, &window_cc, &cutoff_cc, &linker_cc);
				do_coil_coil = TRUE;
#endif
			}
			else if (*ptr == 'D')
			{
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
					parse_dust_options(buffer, &level_dust, &window_dust, &minwin_dust, &linker_dust);
				do_dust = TRUE;
			}
#ifdef TEMP_BLAST_OPTIONS
			else if (*ptr == 'R')
			{
				repeat_options = BLASTOptionNew("blastn", TRUE);
				repeat_options->expect_value = 0.1;
				repeat_options->penalty = -1;
				repeat_options->wordsize = 11;
				repeat_options->gap_x_dropoff_final = 90;
				repeat_options->dropoff_2nd_pass = 40;
				repeat_options->gap_open = 2;
				repeat_options->gap_extend = 1;
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
                                   parse_blast_options(repeat_options,
                                      buffer, &error_msg, &repeat_database,
                                      NULL, NULL);
				if (repeat_database == NULL)
                                   repeat_database = strdup("humlines.lib humsines.lib retrovir.lib");
				do_repeats = TRUE;
			}
			else if (*ptr == 'V')
			{
				vs_options = VSBlastOptionNew();
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
                                   parse_blast_options(vs_options, buffer,
                                      &error_msg, &vs_database, NULL, NULL); 
				vs_options = BLASTOptionDelete(vs_options);
				if (vs_database == NULL)
                                   vs_database = strdup("UniVec_Core");
				do_vecscreen = TRUE;
			}
#endif
			else if (*ptr == 'L')
			{ /* do low-complexity filtering; dust for blastn, otherwise seg.*/
				do_default = TRUE;
				ptr++;
			}
			else if (*ptr == 'm')
			{
				if (mask_at_hash)
					*mask_at_hash = TRUE;
				ptr++;
			}
			else
			{	/* Nothing applied. */
				ptr++;
			}
		}
	        sfree(buffer);
	}

	seqloc_num = 0;
	if (program_number != blast_type_blastn)
	{
		if (do_default || do_seg)
		{
			SeqBufferSeg(sequence, length, offset, sparamsp, &seg_loc);
			SegParametersFree(sparamsp);
			sparamsp = NULL;
			seqloc_num++;
		}
#ifdef CC_FILTER_ALLOWED
		if (do_coil_coil)
		{
			pccp = PccDatNew ();
			pccp->window = window_cc;
			ReadPccData (pccp);
			scores = PredictCCSeqLoc(slp, pccp);
			cc_slp = FilterCC(scores, cutoff_cc, length, linker_cc,
                                          SeqIdDup(sip), FALSE);
			sfree(scores);
			PccDatFree (pccp);
			seqloc_num++;
		}
#endif
	}
	else
	{
		if (do_default || do_dust)
		{
         SeqBufferDust(sequence, length, offset, level_dust, window_dust,
                       minwin_dust, linker_dust, &dust_loc);
			seqloc_num++;
		}
#ifdef TEMP_BLAST_OPTIONS
		if (do_repeats)
		{
		/* Either the SeqLocPtr is SEQLOC_WHOLE (both strands) or SEQLOC_INT (probably 
one strand).  In that case we make up a double-stranded one as we wish to look at both strands. */
			myslp_allocated = FALSE;
			if (slp->choice == SEQLOC_INT)
			{
				myslp = SeqLocIntNew(SeqLocStart(slp), SeqLocStop(slp), Seq_strand_both, SeqLocId(slp));
				myslp_allocated = TRUE;
			}
			else
			{
				myslp = slp;
			}
start_timer;
			repeat_slp = BioseqHitRangeEngineByLoc(myslp, "blastn", repeat_database, repeat_options, NULL, NULL, NULL, NULL, NULL, 0);
stop_timer("after repeat filtering");
			repeat_options = BLASTOptionDelete(repeat_options);
			sfree(repeat_database);
			if (myslp_allocated)
				SeqLocFree(myslp);
			seqloc_num++;
		}
		if (do_vecscreen)
		{
		/* Either the SeqLocPtr is SEQLOC_WHOLE (both strands) or SEQLOC_INT (probably 
one strand).  In that case we make up a double-stranded one as we wish to look at both strands. */
			myslp_allocated = FALSE;
			if (slp->choice == SEQLOC_INT)
			{
				myslp = SeqLocIntNew(SeqLocStart(slp), SeqLocStop(slp), Seq_strand_both, SeqLocId(slp));
				myslp_allocated = TRUE;
			}
			else
			{
				myslp = slp;
			}
			VSScreenSequenceByLoc(myslp, NULL, vs_database, &seqalign, &vnp, NULL, NULL);
			vnp_var = vnp;
			while (vnp_var)
			{
				seqloc_tmp = vnp_var->ptr;
				if (vs_slp == NULL)
				{
					vs_slp = seqloc_tmp;
				}
				else
				{
					seqloc_var = vs_slp;
					while (seqloc_var->next)
						seqloc_var = seqloc_var->next;
					seqloc_var->next = seqloc_tmp;
				}
				vnp_var->ptr = NULL;
				vnp_var = vnp_var->next;
			}
			vnp = ListNodeFree(vnp);
			seqalign = SeqAlignSetFree(seqalign);
			sfree(vs_database);
			if (myslp_allocated)
				SeqLocFree(myslp);
			seqloc_num++;
		}
#endif
	}

	if (seqloc_num)
	{ 
		BlastSeqLoc* seqloc_list=NULL;  /* Holds all SeqLoc's for
                                                      return. */
#if 0
		if (seg_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, seg_slp);
		if (cc_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, cc_slp);
		if (dust_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, dust_slp);
		if (repeat_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, repeat_slp);
		if (vs_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, vs_slp);
#endif
      if (dust_loc)
         seqloc_list = dust_loc;
      if (seg_loc)
         seqloc_list = seg_loc;

		*seqloc_retval = seqloc_list;
	}

	return status;
}

static Int2
GetFilteringLocationsForOneContext(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, Int2 context, Uint1 program_number, char* filter_string, BlastSeqLoc* *filter_out, Boolean* mask_at_hash)
{
        Int2 status = 0;
        Int4 query_length = 0;      /* Length of query described by SeqLocPtr. */
        Int4 context_offset;
        BlastMaskLoc *mask_slp, *mask_slp_var; /* Auxiliary locations for lower-case masking  */
        BlastSeqLoc *filter_slp = NULL;     /* SeqLocPtr computed for filtering. */
        BlastSeqLoc *filter_slp_combined;   /* Used to hold combined SeqLoc's */
        Uint1 *buffer;              /* holds sequence for plus strand or protein. */

        Boolean is_na = (program_number == blast_type_blastn);
        Int2 index = (is_na ? context / 2 : context);

        context_offset = query_info->context_offsets[context];
        buffer = &query_blk->sequence[context_offset];

        if ((query_length = BLAST_GetQueryLength(query_info, context)) <= 0)
           return 0;


        if ((status = BlastSetUp_Filter(program_number, buffer,
                       query_length, 0, filter_string,
                             mask_at_hash, &filter_slp))) 
             return status;

        /* Extract the mask locations corresponding to this query 
               (frame, strand), detach it from other masks.
               NB: for translated search the mask locations are expected in 
               protein coordinates. The nucleotide locations must be converted
               to protein coordinates prior to the call to BLAST_MainSetUp.
        */
        mask_slp = NULL;
        for (mask_slp_var=query_blk->lcase_mask; mask_slp_var; mask_slp_var=mask_slp_var->next)
        {
                if (mask_slp_var->index == index)
                {
                   mask_slp = mask_slp_var;
                   break;
                }
        }

        /* Attach the lower case mask locations to the filter locations and combine them */
        if (mask_slp) {
             if (filter_slp) {
                  BlastSeqLoc *loc;           /* Iterator variable */
                  for (loc = filter_slp; loc->next; loc = loc->next);
                    loc->next = mask_slp->loc_list;
             } else {
                   filter_slp = mask_slp->loc_list;
             }
                /* Set location list to NULL, to allow safe memory deallocation */
                mask_slp->loc_list = NULL;
        }

        filter_slp_combined = NULL;
        CombineMaskLocations(filter_slp, &filter_slp_combined, 0);
        *filter_out = filter_slp_combined;

        filter_slp = BlastSeqLocFree(filter_slp);

	return 0;
}


Int2
BlastSetUp_GetFilteringLocations(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, Uint1 program_number, char* filter_string, BlastMaskLoc* *filter_out, Boolean* mask_at_hash, Blast_Message * *blast_message)
{

    Int2 status = 0;
    Int4 context = 0; /* loop variable. */
    const Boolean k_is_na = (program_number == blast_type_blastn);
    BlastMaskLoc *last_maskloc = NULL;
    BlastMaskLoc *filter_maskloc = NULL;   /* Local variable for mask locs. */
    Boolean no_forward_strand = (query_info->first_context > 0);  /* filtering needed on reverse strand. */

    for (context = query_info->first_context;
         context <= query_info->last_context; ++context) {
      
        BlastSeqLoc *filter_per_context = NULL;   /* Used to hold combined SeqLoc's */
        Boolean reverse = (k_is_na && ((context & 1) != 0));
        Int4 query_length;

        /* For each query, check if forward strand is present */
        if ((query_length = BLAST_GetQueryLength(query_info, context)) < 0)
        {
            if ((context & 1) == 0)
               no_forward_strand = TRUE;
            continue;
        }

      if (!reverse || no_forward_strand)
      {
        if ((status=GetFilteringLocationsForOneContext(query_blk, query_info, context, program_number, filter_string, &filter_per_context, mask_at_hash)))
        {
               Blast_MessageWrite(blast_message, BLAST_SEV_ERROR, 2, 1, 
                  "Failure at filtering");
               return status;
        }

        /* NB: for translated searches filter locations are returned in 
               protein coordinates, because the DNA lengths of sequences are 
               not available here. The caller must take care of converting 
               them back to nucleotide coordinates. */
       if (filter_per_context)
       {
        if (!last_maskloc) {
                last_maskloc = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
                filter_maskloc = last_maskloc;
        } else {
                last_maskloc->next =
                        (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
                last_maskloc = last_maskloc->next;
        }

        last_maskloc->index = (k_is_na ? context / 2 : context);
        last_maskloc->loc_list = filter_per_context;
       }
      }
    }

    if (filter_out && filter_maskloc)
	*filter_out = filter_maskloc;

    return 0;
}

Int2
Blast_MaskTheResidues(Uint1 * buffer, Int4 length, Boolean is_na,
                           ListNode * mask_loc, Boolean reverse, Int4 offset)
{
    SSeqRange *loc = NULL;
    Int2 status = 0;
    Int4 index, start, stop;
    Uint1 mask_letter;

    if (is_na)
        mask_letter = 14;
    else
        mask_letter = 21;

    for (; mask_loc; mask_loc = mask_loc->next) {
        loc = (SSeqRange *) mask_loc->ptr;
        if (reverse) {
            start = length - 1 - loc->right;
            stop = length - 1 - loc->left;
        } else {
            start = loc->left;
            stop = loc->right;
        }

        start -= offset;
        stop -= offset;

        for (index = start; index <= stop; index++)
            buffer[index] = mask_letter;
    }

    return status;
}

Int2 
BlastSetUp_MaskQuery(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, BlastMaskLoc *filter_maskloc, Uint1 program_number)
{
    const Boolean k_is_na = (program_number == blast_type_blastn);
    Int4 context; /* loop variable. */
    Int2 status=0;

    for (context = query_info->first_context;
         context <= query_info->last_context; ++context) {
      
        BlastMaskLoc* filter_maskloc_var = NULL;
        BlastSeqLoc *filter_per_context = NULL;   /* Used to hold combined SeqLoc's */
        Boolean reverse = (k_is_na && ((context & 1) != 0));
        Int4 query_length;
        Int4 context_offset;
        Uint1 *buffer;              /* holds sequence */

        /* For each query, check if forward strand is present */
        if ((query_length = BLAST_GetQueryLength(query_info, context)) < 0)
            continue;

        context_offset = query_info->context_offsets[context];
        buffer = &query_blk->sequence[context_offset];

	filter_maskloc_var = filter_maskloc;
        while (filter_maskloc_var)
        {
             if (filter_maskloc_var->index == (k_is_na ? context / 2 : context))
             {
		filter_per_context = filter_maskloc_var->loc_list;
                break;
             }
             filter_maskloc_var = filter_maskloc_var->next;
        }

        if (buffer) {

            if ((status =
                     Blast_MaskTheResidues(buffer, query_length, k_is_na,
                                                filter_per_context, reverse, 0)))
            {
                    return status;
            }
        }
    }

    return 0;
}
