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

File name: blast_filter.c

Author: Ilya Dondoshansky

Contents: All code related to query sequence masking/filtering for BLAST

******************************************************************************
 * $Revision$
 * */

#include <blast_def.h>
#include <blast_util.h>
#include <blast_filter.h>
#include <blast_dust.h>
#include <blast_seg.h>
#include <urkpcc.h>

static char const rcsid[] = "$Id$";

/* The following function will replace BlastSetUp_CreateDoubleInt */

BlastSeqLocPtr BlastSeqLocNew(Int4 from, Int4 to)
{
   BlastSeqLocPtr loc = (BlastSeqLocPtr) MemNew(sizeof(BlastSeqLoc));
   DoubleIntPtr di = (DoubleIntPtr) Malloc(sizeof(DoubleInt));

   di->i1 = from;
   di->i2 = to;
   loc->data.ptrvalue = di;
   return loc;
}

BlastSeqLocPtr BlastSeqLocFree(BlastSeqLocPtr loc)
{
   DoubleIntPtr dintp;
   BlastSeqLocPtr next_loc;

   while (loc) {
      next_loc = loc->next;
      dintp = (DoubleIntPtr) loc->data.ptrvalue;
      MemFree(dintp);
      MemFree(loc);
      loc = next_loc;
   }
   return NULL;
}

BlastMaskPtr BlastMaskFree(BlastMaskPtr mask_loc)
{
   BlastMaskPtr next_loc;
   while (mask_loc) {
      next_loc = mask_loc->next;
      BlastSeqLocFree(mask_loc->loc_list);
      MemFree(mask_loc);
      mask_loc = next_loc;
   }
   return NULL;
}

BlastMaskPtr BlastMaskFromSeqLoc(SeqLocPtr mask_slp, Int4 index)
{
   BlastMaskPtr new_mask = (BlastMaskPtr) MemNew(sizeof(BlastMask));
   BlastSeqLocPtr loc, last_loc = NULL, head_loc = NULL;
   SeqIntPtr si;

   new_mask->index = index;

   if (mask_slp->choice == SEQLOC_PACKED_INT)
      mask_slp = (SeqLocPtr) mask_slp->data.ptrvalue;

   for ( ; mask_slp; mask_slp = mask_slp->next) {
      si = (SeqIntPtr) mask_slp->data.ptrvalue;
      loc = BlastSeqLocNew(si->from, si->to);
      if (!last_loc) {
         last_loc = head_loc = loc;
      } else {
         last_loc->next = loc;
         last_loc = last_loc->next;
      }
   }
   new_mask->loc_list = head_loc;
   return new_mask;
}

SeqLocPtr BlastMaskToSeqLoc(Uint1 program_number, BlastMaskPtr mask_loc, 
                            SeqLocPtr slp)
{
   BlastSeqLocPtr loc;
   SeqIntPtr si;
   SeqLocPtr mask_head = NULL, last_mask = NULL;
   SeqLocPtr mask_slp_last, mask_slp_head, new_mask_slp;
   SeqIdPtr seqid;
   DoubleIntPtr di;
   Int4 index;
   Uint1 num_frames;
   
   if (program_number == blast_type_blastx || 
       program_number == blast_type_tblastx)
      num_frames = NUM_FRAMES;
   else
      num_frames = 1;

   for (index = 0; slp; ++index, slp = slp->next) {
      /* Find the mask locations for this query */
      for ( ; mask_loc && (mask_loc->index/NUM_FRAMES < index); 
            mask_loc = mask_loc->next);
      if (!mask_loc)
         /* No more masks */
         break;

      if (mask_loc->index != index)
         /* There is no mask for this sequence */
         continue;

      seqid = SeqLocId(slp);
      while (mask_loc && (mask_loc->index/num_frames == index)) {
      
         mask_slp_head = mask_slp_last = NULL;
         for (loc = mask_loc->loc_list; loc; loc = loc->next) {
            di = (DoubleIntPtr) loc->data.ptrvalue;
            si = SeqIntNew();
            si->from = di->i1;
            si->to = di->i2;
            si->id = SeqIdDup(seqid);
            if (!mask_slp_last)
               mask_slp_last = 
                  ValNodeAddPointer(&mask_slp_head, SEQLOC_INT, si);
            else 
               mask_slp_last = 
                  ValNodeAddPointer(&mask_slp_last, SEQLOC_INT, si);
         }

         if (mask_slp_head) {
            Uint1 frame_index = (mask_loc->index % num_frames) + 1;
            new_mask_slp = ValNodeAddPointer(NULL, SEQLOC_PACKED_INT, 
                                             mask_slp_head);
            /* The 'choice' of the SeqLoc in masks should show the frame,
               with values 1..6 when queries are translated; otherwise
               it does not matter. */
            if (!last_mask) {
               last_mask = ValNodeAddPointer(&mask_head, 
                              frame_index, new_mask_slp);
            } else {
               last_mask = ValNodeAddPointer(&last_mask, 
                              frame_index, new_mask_slp);
            }
         }
         mask_loc = mask_loc->next;
      }
   }
   return mask_head;
}

/** Used for HeapSort, compares two SeqLoc's by starting position. */
static int LIBCALLBACK DoubleIntSortByStartPosition(VoidPtr vp1, VoidPtr vp2)

{
   ValNodePtr v1 = *((ValNodePtr PNTR) vp1);
   ValNodePtr v2 = *((ValNodePtr PNTR) vp2);
   DoubleIntPtr loc1 = (DoubleIntPtr) v1->data.ptrvalue;
   DoubleIntPtr loc2 = (DoubleIntPtr) v2->data.ptrvalue;
   
   if (loc1->i1 < loc2->i1)
      return -1;
   else if (loc1->i1 > loc2->i1)
      return 1;
   else
      return 0;
}

/* This will go in place of CombineSeqLocs to combine filtered locations */
Int2
CombineMaskLocations(BlastSeqLocPtr mask_loc, BlastSeqLocPtr *mask_loc_out)
{
   Int2 status=0;		/* return value. */
   Int4 start, stop;	/* USed to merge overlapping SeqLoc's. */
   DoubleIntPtr di = NULL, di_next = NULL, di_tmp;
   BlastSeqLocPtr loc_head=NULL, last_loc, loc_var;
   BlastSeqLocPtr new_loc = NULL, new_loc_last = NULL;
   
   if (!mask_loc) {
      *mask_loc_out = NULL;
      return 0;
   }

   /* Put all the SeqLoc's into one big linked list. */
   if (loc_head == NULL) {
      loc_head = last_loc = 
         (BlastSeqLocPtr) MemDup(mask_loc, sizeof(BlastSeqLoc));
   } else {
      last_loc->next = (BlastSeqLocPtr) MemDup(mask_loc, sizeof(BlastSeqLoc));
      last_loc = last_loc->next;
   }
         
   /* Copy all locations, so loc points at the end of the chain */
   while (last_loc->next) {		
      last_loc->next = 
         (BlastSeqLocPtr)MemDup(last_loc->next, sizeof(BlastSeqLoc));
      last_loc = last_loc->next;
   }
   
   /* Sort them by starting position. */
   loc_head = (BlastSeqLocPtr) 
      ValNodeSort ((ValNodePtr) loc_head, 
                   DoubleIntSortByStartPosition);
   
   di = (DoubleIntPtr) loc_head->data.ptrvalue;
   start = di->i1;
   stop = di->i2;
   loc_var = loc_head;
   
   while (loc_var) {
      di = loc_var->data.ptrvalue;
      if (loc_var->next)
         di_next = loc_var->next->data.ptrvalue;
      if (di_next && stop+1 >= di_next->i1) {
         stop = MAX(stop, di_next->i2);
      } else {
         di_tmp = (DoubleIntPtr) Malloc(sizeof(DoubleInt));
         di_tmp->i1 = start;
         di_tmp->i2 = stop;
         if (!new_loc)
            new_loc_last = ValNodeAddPointer(&new_loc, 0, di_tmp);
         else
            new_loc_last = ValNodeAddPointer(&new_loc_last, 0, di_tmp);
         if (loc_var->next) {
               start = di_next->i1;
               stop = di_next->i2;
         }
      }
      loc_var = loc_var->next;
      di_next = NULL;
   }

   *mask_loc_out = new_loc;
      
   /* Free memory allocated for the temporary list of SeqLocs */
   while (loc_head) {
      loc_var = loc_head->next;
      MemFree(loc_head);
      loc_head = loc_var;
   }
   return status;
}

Int2 
BLAST_ComplementMaskLocations(Uint1 program_number, 
   BlastQueryInfoPtr query_info, 
   BlastMaskPtr mask_loc, BlastSeqLocPtr *complement_mask) 
{
   Int4 start_offset, end_offset, filter_start, filter_end;
   Int4 context;
   BlastSeqLocPtr loc, last_loc = NULL;
   DoubleIntPtr double_int = NULL, di;
   Boolean first;	/* Specifies beginning of query. */
   Boolean last_interval_open=TRUE; /* if TRUE last interval needs to be closed. */
   Boolean is_na, reverse = FALSE;
   
   is_na = (program_number == blast_type_blastn);
   *complement_mask = NULL;

   for (context = query_info->first_context; 
        context <= query_info->last_context; ++context) {
      start_offset = query_info->context_offsets[context];
      end_offset = query_info->context_offsets[context+1] - 2;
      reverse = (is_na && ((context & 1) != 0));
      first = TRUE;

      if (!reverse) {
         for ( ; mask_loc && mask_loc->index < context; 
               mask_loc = mask_loc->next);
      }

      if (!mask_loc || (mask_loc->index > context) ||
          !mask_loc->loc_list) {
         /* No masks for this context */
         double_int = MemNew(sizeof(DoubleInt));
         double_int->i1 = start_offset;
         double_int->i2 = end_offset;
         if (!last_loc)
            last_loc = ValNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ValNodeAddPointer(&last_loc, 0, double_int);
         continue;
      }
      
      if (reverse) {
         BlastSeqLocPtr prev_loc = NULL, tmp_loc;
         /* Reverse the order of the locations */
         for (tmp_loc = mask_loc->loc_list; tmp_loc; 
              tmp_loc = tmp_loc->next) {
            loc = MemDup(tmp_loc, sizeof(BlastSeqLoc));
            loc->next = prev_loc;
            prev_loc = loc;
         }
      } else {
         loc = mask_loc->loc_list;
      }

      for ( ; loc; loc = loc->next) {
         di = loc->data.ptrvalue;
         if (reverse) {
            filter_start = end_offset - di->i2;
            filter_end = end_offset - di->i1;
         } else {
            filter_start = start_offset + di->i1;
            filter_end = start_offset + di->i2;
         }
         /* The canonical "state" at the top of this 
            while loop is that a DoubleInt has been 
            created and one field was filled in on the 
            last iteration. The first time this loop is 
            entered in a call to the funciton this is not
            true and the following "if" statement moves 
            everything to the canonical state. */
         if (first) {
            first = FALSE;
            double_int = MemNew(sizeof(DoubleInt));
            
            if (filter_start > start_offset) {
               /* beginning of sequence not filtered */
               double_int->i1 = start_offset;
            } else {
               /* beginning of sequence filtered */
               double_int->i1 = filter_end + 1;
               continue;
            }
         }
         double_int->i2 = filter_start - 1;
         if (!last_loc)
            last_loc = ValNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ValNodeAddPointer(&last_loc, 0, double_int);
         if (filter_end >= end_offset) {
            /* last masked region at end of sequence */
            last_interval_open = FALSE;
            break;
         }	else {
            double_int = MemNew(sizeof(DoubleInt));
            double_int->i1 = filter_end + 1;
         }
      }

      if (reverse) {
         loc = BlastSeqLocFree(loc);
      }
      
      if (last_interval_open) {
         /* Need to finish DoubleIntPtr for last interval. */
         if (reverse) {
            double_int->i1 = start_offset;
         } else {
            double_int->i2 = end_offset;
         }
         if (!last_loc)
            last_loc = ValNodeAddPointer(complement_mask, 0, double_int);
         else 
            last_loc = ValNodeAddPointer(&last_loc, 0, double_int);
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
parse_dust_options(const Char *ptr, Int4Ptr level, Int4Ptr window,
	Int4Ptr cutoff, Int4Ptr linker)

{
	Char buffer[BLASTOPTIONS_BUFFER_SIZE];
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
parse_seg_options(const Char *ptr, Int4Ptr window, FloatHiPtr locut, FloatHiPtr hicut)

{
	Char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1; 
	long	tmplong;
	FloatHi	tmpdouble;

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

/** Coiled-coiled algorithm parameters. 
 * @param ptr buffer containing instructions. [in]
 * @param window returns window for coil-coiled algorithm. [out]
 * @param cutoff cutoff for coil-coiled algorithm [out]
 * @param linker returns linker [out]
*/
static Int2
parse_cc_options(const Char *ptr, Int4Ptr window, FloatHiPtr cutoff, Int4Ptr linker)

{
	Char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1;
	long	tmplong;
	FloatHi	tmpdouble;

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

/** Copies filtering commands for one filtering algorithm from "instructions" to
 * "buffer". 
 * ";" is a delimiter for the commands for different algorithms, so it stops
 * copying when a ";" is found. 
 * Example filtering string: "m L; R -d rodents.lib"
 * @param instructions filtering commands [in] 
 * @param buffer filled with filtering commands for one algorithm. [out]
*/
static const Char *
BlastSetUp_load_options_to_buffer(const Char *instructions, CharPtr buffer)
{
	Boolean not_started=TRUE;
	CharPtr buffer_ptr;
	const Char *ptr;
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
BlastSetUp_Filter(Uint1 program_number, Uint1Ptr sequence, Int4 length, 
   Int4 offset, CharPtr instructions, BoolPtr mask_at_hash, 
   BlastSeqLocPtr *seqloc_retval)
{
	Boolean do_default=FALSE, do_seg=FALSE, do_coil_coil=FALSE, do_dust=FALSE; 
	CharPtr buffer=NULL;
   const Char *ptr;
	Int2 seqloc_num;
	Int2 status=0;		/* return value. */
	Int4 window_cc, linker_cc, window_dust, level_dust, minwin_dust, linker_dust;
   BlastSeqLocPtr dust_loc = NULL, seg_loc = NULL;
   BlastSeqLocPtr cc_loc = NULL, vs_loc = NULL, repeat_loc = NULL;
	PccDatPtr pccp;
	Nlm_FloatHiPtr scores;
	Nlm_FloatHi cutoff_cc;
	SegParametersPtr sparamsp=NULL;
#ifdef TEMP_BLAST_OPTIONS
   /* TEMP_BLAST_OPTIONS is set to zero until these are implemented. */
	BLAST_OptionsBlkPtr repeat_options, vs_options;
	Boolean do_repeats=FALSE; 	/* screen for orgn. specific repeats. */
	Boolean do_vecscreen=FALSE;	/* screen for vector contamination. */
	Boolean myslp_allocated;
	CharPtr repeat_database=NULL, vs_database=NULL, error_msg;
	SeqLocPtr repeat_slp=NULL, vs_slp=NULL;
	SeqAlignPtr seqalign;
	SeqLocPtr myslp, seqloc_var, seqloc_tmp;
	ValNodePtr vnp=NULL, vnp_var;
#endif

	cutoff_cc = CC_CUTOFF;

	/* FALSE is the default right now. */
	if (mask_at_hash)
		*mask_at_hash = FALSE;

	if (instructions == NULL || StringICmp(instructions, "F") == 0)
		return status;

	/* parameters for dust. */
	/* -1 indicates defaults. */
	level_dust = -1;
	window_dust = -1;
	minwin_dust = -1;
	linker_dust = -1;
	if (StringICmp(instructions, "T") == 0)
	{ /* do_default actually means seg for proteins and dust for nt. */
		do_default = TRUE;
	}
	else
	{
		buffer = MemNew(StringLen(instructions)*sizeof(Char));
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
				ptr = BlastSetUp_load_options_to_buffer(ptr+1, buffer);
				window_cc = CC_WINDOW;
				cutoff_cc = CC_CUTOFF;
				linker_cc = CC_LINKER;
				if (buffer[0] != NULLB)
					parse_cc_options(buffer, &window_cc, &cutoff_cc, &linker_cc);
				do_coil_coil = TRUE;
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
                                   repeat_database = StringSave("humlines.lib humsines.lib retrovir.lib");
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
                                   vs_database = StringSave("UniVec_Core");
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
		buffer = MemFree(buffer);
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
		if (do_coil_coil)
		{
			pccp = PccDatNew ();
			pccp->window = window_cc;
			ReadPccData (pccp);
#if CC_FILTER_ALLOWED
			scores = PredictCCSeqLoc(slp, pccp);
			cc_slp = FilterCC(scores, cutoff_cc, length, linker_cc,
                                          SeqIdDup(sip), FALSE);
			MemFree(scores);
#endif
			PccDatFree (pccp);
			seqloc_num++;
		}
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
			repeat_database = MemFree(repeat_database);
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
				seqloc_tmp = vnp_var->data.ptrvalue;
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
				vnp_var->data.ptrvalue = NULL;
				vnp_var = vnp_var->next;
			}
			vnp = ValNodeFree(vnp);
			seqalign = SeqAlignSetFree(seqalign);
			vs_database = MemFree(vs_database);
			if (myslp_allocated)
				SeqLocFree(myslp);
			seqloc_num++;
		}
#endif
	}

	if (seqloc_num)
	{ 
		BlastSeqLocPtr seqloc_list=NULL;  /* Holds all SeqLoc's for
                                                      return. */
#if 0
		if (seg_slp)
			ValNodeAddPointer(&seqloc_list, SEQLOC_MIX, seg_slp);
		if (cc_slp)
			ValNodeAddPointer(&seqloc_list, SEQLOC_MIX, cc_slp);
		if (dust_slp)
			ValNodeAddPointer(&seqloc_list, SEQLOC_MIX, dust_slp);
		if (repeat_slp)
			ValNodeAddPointer(&seqloc_list, SEQLOC_MIX, repeat_slp);
		if (vs_slp)
			ValNodeAddPointer(&seqloc_list, SEQLOC_MIX, vs_slp);
#endif
      if (dust_loc)
         seqloc_list = dust_loc;
      if (seg_loc)
         seqloc_list = seg_loc;

		*seqloc_retval = seqloc_list;
	}

	return status;
}

Int2 BlastMaskDNAToProtein(BlastMaskPtr PNTR mask_loc_ptr, SeqLocPtr slp)
{
   Int2 status = 0;
   BlastMaskPtr last_mask = NULL, head_mask = NULL, mask_loc; 
   Int4 dna_length;
   BlastSeqLocPtr dna_loc, prot_loc_head, prot_loc_last;
   DoubleIntPtr dip;
   Int4 index, context;
   Int2 frame;
   Int4 from, to;

   if (!mask_loc_ptr)
      return -1;

   mask_loc = *mask_loc_ptr;

   for (index = 0; slp; ++index, slp = slp->next) {
      if (index < mask_loc->index)
         /* Advance to the next nucleotide sequence */
         continue;

      dna_length = SeqLocLen(slp);
      /* Reproduce this mask for all 6 frames, with translated 
         coordinates */
      for (context = 0; context < NUM_FRAMES; ++context) {
         if (!last_mask) {
            head_mask = last_mask = MemNew(sizeof(BlastMask));
         } else {
            last_mask->next = MemNew(sizeof(BlastMask));
            last_mask = last_mask->next;
         }
         
         last_mask->index = NUM_FRAMES * index + context;
         prot_loc_last = prot_loc_head = NULL;
         
         frame = BLAST_ContextToFrame(blast_type_blastx, context);

         for (dna_loc = mask_loc->loc_list; dna_loc; 
              dna_loc = dna_loc->next) {
            dip = (DoubleIntPtr) dna_loc->data.ptrvalue;
            if (frame < 0) {
               from = (dna_length + frame - dip->i2)/CODON_LENGTH;
               to = (dna_length + frame - dip->i1)/CODON_LENGTH;
            } else {
               from = (dip->i1 - frame + 1)/CODON_LENGTH;
               to = (dip->i2 - frame + 1)/CODON_LENGTH;
            }
            if (!prot_loc_last) {
               prot_loc_head = prot_loc_last = BlastSeqLocNew(from, to);
            } else { 
               prot_loc_last->next = BlastSeqLocNew(from, to);
               prot_loc_last = prot_loc_last->next; 
            }
         }
         last_mask->loc_list = prot_loc_head;
      }
      /* Go to the next nucleotide mask */
      mask_loc = mask_loc->next;
   }

   /* Free the mask with nucleotide coordinates */
   BlastMaskFree(*mask_loc_ptr);
   /* Return the new mask with protein coordinates */
   *mask_loc_ptr = head_mask;

   return status;
}


Int2 BlastMaskProteinToDNA(BlastMaskPtr PNTR mask_loc_ptr, SeqLocPtr slp)
{
   Int2 status = 0;
   Int4 index;
   BlastMaskPtr mask_loc;
   BlastSeqLocPtr loc;
   DoubleIntPtr dip;
   Int4 dna_length;
   Int2 frame;
   Int4 from, to;

   if (!mask_loc_ptr) 
      return -1;

   mask_loc = *mask_loc_ptr;

   index = NUM_FRAMES;

   while (slp && mask_loc) {
      if (index <= mask_loc->index) {
         /* Advance to the next nucleotide sequence */
         index += NUM_FRAMES;
         slp = slp->next;
      } else {
         dna_length = SeqLocLen(slp);
         frame = BLAST_ContextToFrame(blast_type_blastx, 
                                mask_loc->index % NUM_FRAMES);

         for (loc = mask_loc->loc_list; loc; loc = loc->next) {
            dip = (DoubleIntPtr) loc->data.ptrvalue;
            if (frame < 0)	{
               to = dna_length - CODON_LENGTH*dip->i1 + frame;
               from = dna_length - CODON_LENGTH*dip->i2 + frame + 1;
            } else {
               from = CODON_LENGTH*dip->i1 + frame - 1;
               to = CODON_LENGTH*dip->i2 + frame - 1;
            }
            dip->i1 = from;
            dip->i2 = to;
         }
         /* Advance to the next mask */
         mask_loc = mask_loc->next;
      }
   }
   return status;
}
