/* ===========================================================================
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_setup.c

Author: Tom Madden

Contents: Utilities initialize/setup BLAST.

$Revision$

******************************************************************************/

/*
* $Log$
* Revision 1.8  2003/04/17 21:14:41  dondosha
* Added cutoff score hit parameters that is calculated from e-value
*
* Revision 1.7  2003/04/11 16:41:16  dondosha
* Added an extern declaration to eliminate compiler warning
*
* Revision 1.6  2003/04/09 18:44:14  dondosha
* Advance buffer pointer for masking residues for all programs
*
* Revision 1.5  2003/04/09 18:01:52  dondosha
* Fixed creation of double integer location list for reverse strands for mask-at-hash case.
*
* Revision 1.4  2003/04/03 14:17:45  coulouri
* fix warnings, remove unused parameter
*
* Revision 1.3  2003/04/02 17:20:41  dondosha
* Added calculation of ungapped cutoff score in correct place
*
* Revision 1.2  2003/04/01 17:45:54  dondosha
* Corrections for filtering with translated queries
*
* Revision 1.1  2003/03/31 18:22:30  camacho
* Moved from parent directory
*
* Revision 1.54  2003/03/28 23:16:02  dondosha
* Correction for multiple query case
*
* Revision 1.53  2003/03/28 18:12:14  coulouri
* replumb to remove matrix from lookup structure, pass in directly to wordfinders
*
* Revision 1.52  2003/03/28 15:55:45  dondosha
* Doxygen warnings fix
*
* Revision 1.51  2003/03/27 20:58:30  dondosha
* Implementing query multiplexing for all types of BLAST programs
*
* Revision 1.50  2003/03/25 16:30:25  dondosha
* Strict compiler warning fixes
*
* Revision 1.49  2003/03/25 16:22:44  camacho
* Correction to setting ambiguous character in score block
*
* Revision 1.48  2003/03/24 17:27:42  dondosha
* Improvements and additions for doxygen
*
* Revision 1.47  2003/03/21 21:12:09  madden
* Minor fix for doxygen
*
* Revision 1.46  2003/03/14 16:55:09  dondosha
* Minor corrections to eliminate strict compiler warnings
*
* Revision 1.45  2003/03/12 17:04:35  dondosha
* Fix for multiple freeing of filter SeqLoc
*
* Revision 1.44  2003/03/10 19:32:16  dondosha
* Fixed wrong order of structure initializations
*
* Revision 1.43  2003/03/07 20:42:23  dondosha
* Added ewp argument to be initialized in BlastSetup_Main
*
* Revision 1.42  2003/03/07 15:30:47  coulouri
* correct include
*
* Revision 1.41  2003/03/06 17:05:14  dondosha
* Comments fixes for doxygen
*
* Revision 1.40  2003/03/05 21:38:21  dondosha
* Cast argument to CharPtr in call to BlastScoreBlkFill, to get rid of compiler warning
*
* Revision 1.39  2003/03/05 21:18:28  coulouri
* refactor and comment lookup table creation
*
* Revision 1.38  2003/03/05 20:58:50  dondosha
* Corrections for handling effective search space for multiple queries
*
* Revision 1.37  2003/02/28 22:56:49  dondosha
* BlastSetUp_SeqBlkNew moved to blast_util.c; compiler warning for types mismatch fixed
*
* Revision 1.36  2003/02/26 15:42:19  madden
* const CharPtr becomes const Char *
*
* Revision 1.35  2003/02/25 20:03:02  madden
* Remove BlastSetUp_Concatenate and BlastSetUp_Standard
*
* Revision 1.34  2003/02/14 17:37:24  madden
* Doxygen compliant comments
*
* Revision 1.33  2003/02/13 21:40:40  madden
* Validate options, send back message if problem
*
* Revision 1.32  2003/02/13 20:46:36  dondosha
* Set the kbp and kbp_gap to kbp_std and kbp_gap_std respectively at the end of set up
*
* Revision 1.31  2003/02/07 23:44:35  dondosha
* Corrections for multiple query sequences, in particular for lower case masking
*
* Revision 1.30  2003/01/28 15:14:21  madden
* BlastSetUp_Main gets additional args for parameters
*
* Revision 1.29  2003/01/22 20:49:19  dondosha
* Set the ambiguous residue before the ScoreBlk is filled
*
* Revision 1.28  2003/01/10 21:25:42  dondosha
* Corrected buffer length for sequence block in case of multiple queries/strands
*
* Revision 1.27  2003/01/10 18:50:10  madden
* Version of BlastSetUp_Main that does not require num_seqs or dblength
*
* Revision 1.26  2003/01/06 17:44:40  madden
* First revision of BlastSetUp_Main
*
* Revision 1.25  2003/01/02 17:11:19  dondosha
* Call different functions to fill protein/nucleotide lookup table
*
* Revision 1.24  2002/12/27 18:03:52  madden
* Blastx fixes, added function BlastSetUp_ConvertProteinSeqLoc
*
* Revision 1.23  2002/12/24 16:21:40  madden
* BlastSetUp_Mega renamed to BlastSetUp_Concatenate, unused arg frame removed
*
* Revision 1.22  2002/12/24 16:09:53  madden
* Cleanup from strict compiler options
*
* Revision 1.21  2002/12/24 14:48:23  madden
* Create lookup table for proteins
*
* Revision 1.20  2002/12/23 19:57:32  madden
* Remove reverse buffer from BlastSetUp_GetSequence and other parts of code
*
* Revision 1.19  2002/12/20 20:54:35  dondosha
* 1. Use value 14 (corresponding to N) for masked bases in blastn
* 2. Corrected buffer offsets for masking filtered locations;
* 3. Corrected how reverse strand buffer is masked.
* 4. BlastScoreBlkGappedFill made external (probably temporarily).
*
* Revision 1.18  2002/12/20 15:15:24  madden
* remove unused variables etc.
*
* Revision 1.17  2002/12/20 15:07:02  madden
* Fixes to get both strands for (megablast) lookup table creation
*
* Revision 1.16  2002/12/19 21:23:09  madden
* Add LookupTableWrapPtr to setup functions
*
* Revision 1.15  2002/12/19 19:58:02  madden
* Add function BlastSetUp_Mega for megablast setup, set DoubleIntPtrs for lookup table
*
* Revision 1.14  2002/12/13 14:38:04  madden
* Add function BlastSetUp_FilterSeqLoc2DoubleInt to convert SeqLocs to array of offsets for looup creation
*
* Revision 1.13  2002/12/12 21:21:12  dondosha
* Changed blast_lookup.h to mb_lookup.h
*
* Revision 1.12  2002/12/12 16:05:21  madden
* Remove include/need for blastdef.h
*
* Revision 1.11  2002/10/24 14:07:29  madden
* Handle lower-case masking for blastx/tblastx
*
* Revision 1.9  2002/10/24 12:24:50  madden
* Fixes for filtering, combine filtered SeqLocs
*
* Revision 1.8  2002/10/23 22:42:34  madden
* Save context and frame information
*
* Revision 1.7  2002/10/22 15:50:39  madden
* fix translating searches
*
* Revision 1.6  2002/10/17 15:45:33  madden
* fixes for effective lengths
*
* Revision 1.5  2002/10/08 21:00:01  madden
* Merge filtered locs and lower-case loc
*
* Revision 1.4  2002/10/07 21:04:15  madden
* Fixes for filtering
*
* Revision 1.3  2002/10/02 20:46:04  madden
* Added comments for parsing dust options
*
* Revision 1.2  2002/10/02 20:05:56  madden
* Add filtering functions
*
* Revision 1.1  2002/09/20 20:36:33  madden
* First version of setup functions
*
*/

#include <blast_setup.h>
#include <seg.h>
#include <urkpcc.h>
#include <dust.h>
#include <seqport.h>
#include <sequtil.h>
#include <objloc.h>
#include <mb_lookup.h>
#include <aa_lookup.h>
#include <blast_util.h>

extern Uint1 FrameToDefine PROTO((Int2 frame));
extern void HackSeqLocId(SeqLocPtr slp, SeqIdPtr id);

/* These should be defined elsewhere so other trans. units can use them. */
#define SEQLOC_MASKING_NOTSET 0
#define CODON_LENGTH 3


/* BlastScoreBlkGappedFill, fills the ScoreBlkPtr for a gapped search.  
 *	Should be moved to blastkar.c (or it's successor) in the future.
*/

Int2 
BlastScoreBlkGappedFill(BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr scoring_options, const Char *program_name)
{

	if (sbp == NULL || scoring_options == NULL || program_name == NULL)
		return 1;


	if (StringCmp(program_name, "blastn") == 0)
	{
		sbp->penalty = scoring_options->penalty;
		sbp->reward = scoring_options->reward;
		BlastScoreBlkMatFill(sbp, "BLOSUM62"); /* does this neeeed to be done? */
	}
	else
	{
		Int2 tmp_index;	/* loop variable. */

		sbp->read_in_matrix = TRUE;
		if (scoring_options->matrix)
			BlastScoreBlkMatFill(sbp, scoring_options->matrix);
		else
			BlastScoreBlkMatFill(sbp, "BLOSUM62");

		BlastScoreBlkMaxScoreSet(sbp);
		for (tmp_index=0; tmp_index<sbp->number_of_contexts; tmp_index++)
		{
			sbp->kbp_gap_std[tmp_index] = BlastKarlinBlkCreate();
			BlastKarlinBlkGappedCalcEx(sbp->kbp_gap_std[tmp_index], 
				scoring_options->gap_open, scoring_options->gap_extend, 
				scoring_options->decline_align, sbp->name, NULL);
		}

		sbp->kbp_gap = sbp->kbp_gap_std;
	}

	return 0;
}


/** Converts a protein (translated) SeqLocPtr from the protein
 *	coordinates to the nucl. coordinates.  Only works on a 
 *	SeqLocPtr of type SeqIntPtr right now.  
 *
 * @param slp SeqLoc in protein coordinates [in]
 * @param frame Frame of translated DNA [in] 
 * @param full_length full length of DNA [in]
 * @param slp_out back-translated SeqLoc (DNA coordinates). [out]
*/
static Int2
BlastSetUp_ConvertProteinSeqLoc(SeqLocPtr slp, Int2 frame, Int4 full_length, SeqLocPtr *slp_out)

{
	Int4 from, to;
	SeqIntPtr seq_int;
    	SeqLocPtr seqloc_new=NULL;

	if (slp == NULL)
		return 0;

	if (slp->choice == SEQLOC_PACKED_INT)
		slp = slp->data.ptrvalue;

	while (slp)
	{
		if (slp->choice != SEQLOC_INT)
			return -1;

		seq_int = slp->data.ptrvalue;

		if (frame < 0)
		{
			to = full_length - CODON_LENGTH*seq_int->from + frame;
			from = full_length - CODON_LENGTH*seq_int->to + frame + 1;
			ValNodeLink(&seqloc_new, 
				SeqLocIntNew(from, to, Seq_strand_minus, SeqIdDup(seq_int->id)));
		}
		else
		{
			from = CODON_LENGTH*seq_int->from + frame - 1;
			to = CODON_LENGTH*seq_int->to + frame - 1;
			ValNodeLink(&seqloc_new, 
				SeqLocIntNew(from, to, Seq_strand_plus, SeqIdDup(seq_int->id)));
		}
		slp = slp->next;
	}

	if (seqloc_new)
		ValNodeAddPointer(slp_out, SEQLOC_PACKED_INT, seqloc_new);
    
	return 0;
}

/** Converts a DNA SeqLocPtr from the nucl. coordinates to 
 * the protein (translated) coordinates.  Only works on a SeqLocPtr of type 
 * SEQLOC_INT or SEQLOC_PACKED_INT right now.
 * @param slp back-translated SeqLoc (DNA coordinates). [in]
 * @param frame frame to use for translation. [in]
 * @param full_length length of nucleotide sequence. [in]
 * @param seqid SeqId to be used in SeqLoc. [in]
 * @param seqloc_translated SeqLoc in protein coordinates. [out]
*/
static Int2
BlastSetUp_TranslateDNASeqLoc(SeqLocPtr slp, Int2 frame, Int4 full_length,
	const SeqIdPtr seqid, SeqLocPtr *seqloc_translated) 
{
    SeqIntPtr seq_int;
    Int4 from, to;
    SeqLocPtr seqloc_new=NULL;
    
    if (slp == NULL)
        return 0;
    
    if (slp->choice == SEQLOC_PACKED_INT)
        slp = slp->data.ptrvalue;
    
    while (slp) 
    {
        if (slp->choice != SEQLOC_INT)
            return 1;
        
        seq_int = slp->data.ptrvalue;
        
        if (frame < 0) 
	{
            	from = (full_length + frame - seq_int->to)/CODON_LENGTH;
            	to = (full_length + frame - seq_int->from)/CODON_LENGTH;
		ValNodeLink(&seqloc_new, 
                   SeqLocIntNew(from, to, Seq_strand_minus, SeqIdDup(seqid)));
        } 
	else 
	{
            	from = (seq_int->from - frame + 1)/CODON_LENGTH;
            	to = (seq_int->to - frame + 1)/CODON_LENGTH;
		ValNodeLink(&seqloc_new, 
                   SeqLocIntNew(from, to, Seq_strand_plus, SeqIdDup(seqid)));
        }
        slp = slp->next;
    }

    if (seqloc_new)
	ValNodeAddPointer(seqloc_translated, SEQLOC_PACKED_INT, seqloc_new);
    
    return 0;
}

/** Masks the letters in buffer.
 * @param buffer the sequence to be masked (will be modified). [out]
 * @param max_length the sequence to be masked (will be modified). [in]
 * @param is_na nucleotide if TRUE [in]
 * @param mask_slp the SeqLoc to use for masking [in] 
 * @param reverse minus strand if TRUE [in]
 * @param offset how far along sequence is 1st residuse in buffer [in]
 *
*/
static Int2
BlastSetUp_MaskTheResidues(Uint1Ptr buffer, Int4 max_length, Boolean is_na,
	SeqLocPtr mask_slp, Boolean reverse, Int4 offset)
{
	SeqLocPtr slp=NULL;
	Int2 status=0;
        Int4 index, start, stop;
	Uint1 mask_letter;

	if (is_na)
		mask_letter = 14;
	else
		mask_letter = 21;

       
	while (mask_slp)
	{
		slp=NULL;
        	while((slp = SeqLocFindNext(mask_slp, slp))!=NULL)
        	{
			if (reverse)
			{
				start = max_length - 1 - SeqLocStop(slp);
				stop = max_length - 1 - SeqLocStart(slp);
			}
			else
			{
              			start = SeqLocStart(slp);
              			stop = SeqLocStop(slp);
			}

			start -= offset;
			stop  -= offset;

			for (index=start; index<=stop; index++)
			{
				buffer[index] = mask_letter;
			}
        	}
		mask_slp = mask_slp->next;
	}

	return status;
}

/** Used for HeapSort, compares two SeqLoc's by starting position. */
static int LIBCALLBACK MySeqLocSortByStartPosition(VoidPtr vp1, VoidPtr vp2)

{
	SeqLocPtr slp1, slp2;

	slp1 = *((SeqLocPtr PNTR) vp1);
	slp2 = *((SeqLocPtr PNTR) vp2);

	if (SeqLocStart(slp1) < SeqLocStart(slp2))
		return -1;
	else if (SeqLocStart(slp1) > SeqLocStart(slp2))
		return 1;
	else
		return 0;
}

/** Go through all SeqLoc's on the ValNodePtr seqloc_list, combine any that
 * overlap. Deallocate the memory for the SeqLoc's that were on the list,
 * produce a new (merged) SeqLoc. 
 * @param seqloc_list The SeqLoc's to be merged, SEQLOC_MIX expected of
 *                    PACKED_INTS expected [in] 
 * @param seqloc_out The new (merged) seqloc. [out]
*/
static Int2
CombineSeqLocs(SeqLocPtr seqloc_list, SeqLocPtr *seqloc_out)

{
	Int2 status=0;		/* retrun value. */
	Int4 start, stop;	/* USed to merge overlapping SeqLoc's. */
	SeqLocPtr seqloc_var, new_seqloc=NULL;
	SeqLocPtr my_seqloc=NULL, tmp_seqloc;
	SeqLocPtr my_seqloc_head=NULL;
	SeqIntPtr seq_int;

	/* Put all the SeqLoc's into one big linked list. */
	while (seqloc_list)
	{
		tmp_seqloc = seqloc_list->data.ptrvalue;
		if (tmp_seqloc->choice == SEQLOC_PACKED_INT)
			tmp_seqloc = tmp_seqloc->data.ptrvalue;

		seqloc_list = seqloc_list->next;

		if (my_seqloc_head == NULL) {
                   my_seqloc_head = my_seqloc = 
                      (SeqLocPtr)MemDup(tmp_seqloc, sizeof(SeqLoc));
		} else {
                   my_seqloc->next = 
                      (SeqLocPtr)MemDup(tmp_seqloc, sizeof(SeqLoc));
                   my_seqloc = my_seqloc->next;
                }
                
                /* Copy all SeqLocs, so my_seqloc points at the end of the 
                   chain */
		while (my_seqloc->next) {		
                   my_seqloc->next = 
                      (SeqLocPtr)MemDup(my_seqloc->next, sizeof(SeqLoc));
                   my_seqloc = my_seqloc->next;
		}
	}

	/* Sort them by starting position. */
	my_seqloc_head = (SeqLocPtr) ValNodeSort ((ValNodePtr) my_seqloc_head, MySeqLocSortByStartPosition);

	start = SeqLocStart(my_seqloc_head);
	stop = SeqLocStop(my_seqloc_head);
	seqloc_var = my_seqloc_head;


	while (seqloc_var)
	{
		if (seqloc_var->next && stop+1 >= SeqLocStart(seqloc_var->next))
		{
			stop = MAX(stop, SeqLocStop(seqloc_var->next));
		}
		else
		{
			seq_int = SeqIntNew();
			seq_int->from = start;
			seq_int->to = stop;
			seq_int->strand = Seq_strand_plus;
			seq_int->id = SeqIdDup(SeqIdFindBest(SeqLocId(seqloc_var), SEQID_GI));
                	ValNodeAddPointer(&new_seqloc, SEQLOC_INT, seq_int);
			if (seqloc_var->next)
			{
				start = SeqLocStart(seqloc_var->next);
				stop = SeqLocStop(seqloc_var->next);
			}
		}
		seqloc_var = seqloc_var->next;
	}

	if (new_seqloc)
		ValNodeAddPointer(seqloc_out, SEQLOC_PACKED_INT, new_seqloc);

        /* Free memory allocated for the temporary list of SeqLocs */
        while (my_seqloc_head) {
           my_seqloc = my_seqloc_head->next;
           MemFree(my_seqloc_head);
           my_seqloc_head = my_seqloc;
        }

	return status;
}

#define CC_WINDOW 22
#define CC_CUTOFF 40.0
#define CC_LINKER 32

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

/** This function takes the list of filtered SeqLoc's (i.e., regions that 
 * should not be searches or not added to lookup table) and makes up a set 
 * of DoubleIntPtr's that should be searched (that is, takes the 
 * complement). If the entire sequence is filtered, then a DoubleInt is 
 * created and both of its elements (i1 and i2) are set to -1 to indicate 
 * this. 
 * If filter_slp is NULL, a DoubleInt for full span of the slp is created.
 * @param slp SeqLoc of sequence. [in]
 * @param filter_slp SeqLoc of filtered region. [in]
 * @param reverse Should locations from filter_slp be reversed? [in]
 * @param vnp_out Linked list of DoubleIntPtrs with offsets. [out]
 * @param offset Offset to be added to any start/stop [out]
*/
static Int2
BlastSetUp_CreateDoubleInt(SeqLocPtr slp, SeqLocPtr filter_slp, 
   Boolean reverse, ValNodePtr *vnp_out, Int4 *offset)

{
	DoubleIntPtr double_int=NULL; /* stores start/stop for each non-filtered region.*/
	Int2 status=0;		/* return value. */
	Int2 overlap;		/* stores return value of SeqLocCompare. */
	Int4 seqloc_start=0;  /* Start of this SeqLoc, with added offset */
        Int4 seqloc_end = 0;    /* End of this SeqLoc, with added offset */
        Int4 filter_start, filter_end; /* Start and end of the SeqLoc for
                                          the filtered segment */
        
	if (offset)
	{
		seqloc_start = *offset;
                seqloc_end = *offset;
                /* adjust offset so it's ready when function returns. */
		*offset += SeqLocStop(slp) + 1;  
	}
	seqloc_start += SeqLocStart(slp);
        seqloc_end += SeqLocStop(slp);

	/* Check if filter_slp covers entire slp. */
	overlap = SeqLocCompare(slp, filter_slp);
	if (overlap == SLC_A_IN_B || overlap == SLC_A_EQ_B)
	{
		double_int = MemNew(sizeof(DoubleInt));
		double_int->i1 = -1;
		double_int->i2 = -1;
		ValNodeAddPointer(vnp_out, 0, double_int);
		filter_slp = NULL; /* Entire query is filtered. */
		return 0;
	}

	if (filter_slp)
	{
		Boolean first=TRUE;	/* Specifies beginning of query. */
		Boolean last_interval_open=TRUE; /* if TRUE last interval needs to be closed. */

		while (filter_slp)
		{
			SeqLocPtr tmp_slp=NULL;

			while((tmp_slp = SeqLocFindNext(filter_slp, tmp_slp))!=NULL)
			{
                           if (reverse) {
                              filter_start = 
                                 seqloc_end - SeqLocStop(tmp_slp);
                              filter_end = 
                                 seqloc_end - SeqLocStart(tmp_slp);
                           } else {
                              filter_start = 
                                 seqloc_start + SeqLocStart(tmp_slp);
                              filter_end = 
                                 seqloc_start + SeqLocStop(tmp_slp);
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
                              
                              if (reverse) {
                                 if (filter_end < seqloc_end) {
                                    /* end of sequence not filtered */
                                    double_int->i2 = seqloc_end;
                                 } else {
                                    /* end of sequence filtered */
                                    double_int->i2 = filter_start - 1;
                                    continue;
                                 }
                              } else {
                                 if (filter_start > seqloc_start) {
                                    /* beginning of sequence not filtered */
                                    double_int->i1 = seqloc_start;
                                 } else {
                                    /* beginning of sequence filtered */
                                    double_int->i1 = filter_end + 1;
                                    continue;
                                 }
                              }
                           }
                           if (reverse) {
                              double_int->i1 = filter_end + 1;
                              ValNodeAddPointer(vnp_out, 0, double_int);
                              if (filter_start <= seqloc_start) {
                                 /* last masked region at start of 
                                    sequence */
                                 last_interval_open = FALSE;
                                 break;
                              }	else {
                                 double_int = MemNew(sizeof(DoubleInt));
                                 double_int->i2 = filter_start - 1;
                              }
                           } else {
                              double_int->i2 = filter_start - 1;
                              ValNodeAddPointer(vnp_out, 0, double_int);
                              if (filter_end >= seqloc_end) {
                                 /* last masked region at end of sequence */
                                 last_interval_open = FALSE;
                                 break;
                              }	else {
                                 double_int = MemNew(sizeof(DoubleInt));
                                 double_int->i1 = filter_end + 1;
                              }
                           }
                           

			}
			filter_slp = filter_slp->next;
		}

		if (last_interval_open) {
                   /* Need to finish DoubleIntPtr for last interval. */
                   if (reverse) {
                      double_int->i1 = seqloc_start;
                   } else {
                      double_int->i2 = seqloc_end;
                   }
                   ValNodeAddPointer(vnp_out, 0, double_int);
		}
	}
	else
	{
		double_int = MemNew(sizeof(DoubleInt));
		double_int->i1 = seqloc_start;
		double_int->i2 = seqloc_end;
		ValNodeAddPointer(vnp_out, 0, double_int);
	}

	return status;
}


/** Runs filtering functions, according to the string "instructions", on the
 * SeqLocPtr. Should combine all SeqLocs so they are non-redundant.
 * @param slp SeqLoc of sequence to be filtered. [in]
 * @param instructions String of instructions to filtering functions. [in]
 * @param mask_at_hash If TRUE masking is done while making the lookup table
 *                     only. [out] 
 * @param seqloc_retval Resutling seqloc for filtered region. [out]
*/
static Int2
BlastSetUp_Filter( SeqLocPtr slp, CharPtr instructions, BoolPtr mask_at_hash, SeqLocPtr *seqloc_retval)
{
/* TEMP_BLAST_OPTIONS is set to zero until these are implemented. */
#ifdef TEMP_BLAST_OPTIONS
	BLAST_OptionsBlkPtr repeat_options, vs_options;
#endif
	Boolean do_default=FALSE, do_seg=FALSE, do_coil_coil=FALSE, do_dust=FALSE; 
#ifdef TEMP_BLAST_OPTIONS
	Boolean do_repeats=FALSE; 	/* screen for orgn. specific repeats. */
	Boolean do_vecscreen=FALSE;	/* screen for vector contamination. */
	Boolean myslp_allocated;
	CharPtr repeat_database=NULL, vs_database=NULL, error_msg;
#endif
	CharPtr buffer=NULL;
        const Char *ptr;
	Int2 seqloc_num;
	Int2 status=0;		/* return value. */
	Int4 window_cc, linker_cc, window_dust, level_dust, minwin_dust, linker_dust;
	SeqLocPtr cc_slp=NULL, dust_slp=NULL, seg_slp=NULL, seqloc_head=NULL, repeat_slp=NULL, vs_slp=NULL;
	PccDatPtr pccp;
	Nlm_FloatHiPtr scores;
	Nlm_FloatHi cutoff_cc;
	SegParamsPtr sparamsp=NULL;
#ifdef TEMP_BLAST_OPTIONS
	SeqAlignPtr seqalign;
	SeqLocPtr myslp, seqloc_var, seqloc_tmp;
	ValNodePtr vnp=NULL, vnp_var;
#endif
	SeqIdPtr sip;

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
				sparamsp = SegParamsNewAa();
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
	seqloc_head = NULL;
	sip = SeqLocId(slp);
	if (ISA_aa(SeqLocMol(slp)))
	{
		if (do_default || do_seg)
		{
			seg_slp = SeqlocSegAa(slp, sparamsp);
			SegParamsFree(sparamsp);
			sparamsp = NULL;
			seqloc_num++;
		}
		if (do_coil_coil)
		{
			pccp = PccDatNew ();
			pccp->window = window_cc;
			ReadPccData (pccp);
			scores = PredictCCSeqLoc(slp, pccp);
			cc_slp = FilterCC(scores, cutoff_cc, SeqLocLen(slp), linker_cc, SeqIdDup(sip), FALSE);
			MemFree(scores);
			PccDatFree (pccp);
			seqloc_num++;
		}
	}
	else
	{
		if (do_default || do_dust)
		{
			dust_slp = SeqLocDust(slp, level_dust, window_dust, minwin_dust, linker_dust);
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
		SeqLocPtr seqloc_list=NULL;  /* Holds all SeqLoc's for return. */

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

		*seqloc_retval = seqloc_list;
	}

	return status;
}


/** Fills buffer with sequence, keeps list of Selenocysteines replaced by X.
 * Note: this function expects to have a buffer already allocated of  the proper
 * size.   
 * Reallocation is not done, as for the case of concatenating sequences the
 * start of the allocated memory will not be passed in, so it's not possible to
 * deallocate and reallocate that. 
 * @param spp SeqPortPtr to use to extract sequence [in]
 * @param is_na TRUE if nucleotide [in]
 * @param use_blastna use blastna alphabet if TRUE (ignored if protein). [in]
 * @param buffer buffer to hold plus strand or protein. [in]
 * @param selcys_pos positions where selenocysteine was replaced by X. [out]
*/
static Int2 
BlastSetUp_FillSequenceBuffer(SeqPortPtr spp, Boolean is_na, Boolean use_blastna,
	Uint1Ptr buffer, ValNodePtr *selcys_pos)
{
	Int4 index;			/* loop index. */
	Uint1 residue;			/* letter from query. */
	Uint1Ptr query_seq;		/* start of sequence. */

	if (is_na)
        	buffer[0] = 0x0f;
	else
        	buffer[0] = NULLB;

        query_seq = buffer+1;

        index=0;
        while ((residue=SeqPortGetResidue(spp)) != SEQPORT_EOF)
        {
        	if (IS_residue(residue))
               	{
			if (is_na)
			{
				if (use_blastna)
                       			query_seq[index] = ncbi4na_to_blastna[residue];
				else
                       			query_seq[index] = residue;
			}
               		else 
                       	{
				if (residue == 24) /* 24 is Selenocysteine. */
				{
                       			residue = 21; /* change Selenocysteine to X. */
					if (selcys_pos)	
						ValNodeAddInt(selcys_pos, 0, index);
				}
                       		query_seq[index] = residue;
                       	}
                       	index++;
                }
	}
	if (is_na)
        	query_seq[index] = 0x0f;
	else
        	query_seq[index] = NULLB;

	return 0;
}


/* ----------------------  BlastSetUp_GetSequence --------------------------
   Purpose:     Get the sequence for the BLAST engine, put in a Uint1 buffer

   Parameters:  bsp - BioseqPtr for query, if non-NULL use to get sequence
		slp - SeqLocPtr for query, use to get sequence if bsp is NULL.
		buffer - return Uint1Ptr (by value) containing the sequence.
			Sequence is ncbistdaa if protein, blastna if nucleotide
			(one letter per byte).

   Returns:     length of sequence (Int4), -1 if error.
   NOTE:        it is the caller's responsibility to deallocate memory 
		for the buffer.
  ------------------------------------------------------------------*/

Int2 LIBCALL 
BlastSetUp_GetSequence(
SeqLocPtr slp, 		/* [in] SeqLoc to extract sequence for. */
Boolean use_blastna,	/* [in] if TRUE use blastna alphabet (ignored for proteins). */
Boolean concatenate,	/* [in] if TRUE do all SeqLoc's, otherwise only first. */
Uint1Ptr *buffer, 	/* [out] buffer to hold plus strand or protein. */
Int4 *buffer_length,	/* [out] length of buffer allocated. */
ValNodePtr *selcys_pos	/* [out] positions where selenocysteine was replaced by X. */
)
{
	Boolean 	is_na; /* nucl. if TRUE, otherwise protein. */
	Int2		status=0; /* return value. */
	Int4		length; /* length of query. */
	Int4		slp_count; /* number of SeqLoc's present. */
	SeqLocPtr	slp_var; /* loop variable */
	SeqPortPtr	spp; /* Used to get sequence with SeqPortGetResidue. */
	Uint1Ptr	buffer_var; /* buffer offset to be worked on. */
	Int2 mol = SeqLocMol(slp);	 /* Get molecule type. */

	slp_var = slp;
	length=0;
	slp_count=0;
	while (slp_var)
	{
		length += SeqLocLen(slp_var);
		slp_count++;
		if (!concatenate)
			break;
		slp_var = slp_var->next;
	}

	is_na = ISA_na(mol);

	if (is_na)
	{	/* twice as long for two strands. */
		*buffer_length = 2*(length + slp_count) + 1;
	}
	else
	{
		*buffer_length = length + slp_count + 1;
	}
	*buffer = Malloc((*buffer_length)*sizeof(Uint1));
	buffer_var = *buffer;

	slp_var = slp;
	while (slp_var)
	{
	    if (is_na)
	    {
		Uint1 strand = SeqLocStrand(slp_var);
		
		if (strand == Seq_strand_plus || strand == Seq_strand_minus)	
		{	/* One strand. */
			spp = SeqPortNewByLoc(slp_var, Seq_code_ncbi4na);
			if (spp)
			{
				SeqPortSet_do_virtual(spp, TRUE);
				status=BlastSetUp_FillSequenceBuffer(spp, TRUE, use_blastna, buffer_var, NULL);
                		spp = SeqPortFree(spp);
			}
		}
		else if (strand == Seq_strand_both)
		{
			SeqLocPtr tmp_slp=NULL;

			spp = SeqPortNewByLoc(slp_var, Seq_code_ncbi4na);
			if (spp)
			{
				SeqPortSet_do_virtual(spp, TRUE);
				status=BlastSetUp_FillSequenceBuffer(spp, TRUE, use_blastna, buffer_var, NULL);
                		spp = SeqPortFree(spp);
			}

			tmp_slp = SeqLocIntNew(SeqLocStart(slp_var), SeqLocStop(slp_var), Seq_strand_minus, SeqLocId(slp_var));

			spp = SeqPortNewByLoc(tmp_slp, Seq_code_ncbi4na);
			if (spp)
			{
	    			buffer_var += SeqLocLen(slp_var) + 1;
				SeqPortSet_do_virtual(spp, TRUE);
				status=BlastSetUp_FillSequenceBuffer(spp, TRUE, use_blastna, buffer_var, NULL);
                		spp = SeqPortFree(spp);
			}
			tmp_slp = SeqLocFree(tmp_slp);
		}
	    }
	    else
	    {
		spp = SeqPortNewByLoc(slp_var, Seq_code_ncbistdaa);
		if (spp)
		{
			SeqPortSet_do_virtual(spp, TRUE);
			status=BlastSetUp_FillSequenceBuffer(spp, FALSE, FALSE, buffer_var, selcys_pos);
                	spp = SeqPortFree(spp);
		}
	    }

	    buffer_var += SeqLocLen(slp_var) + 1;

	    if (!concatenate)
		break;

	    slp_var = slp_var->next;
	}

	return status;
}

/** Create and fill the query information structure, including sequence ids, 
 * and array of offsets into a concatenated sequence, if needed.
 * @param prog_number Numeric value of the BLAST program [in]
 * @param query_slp List of query SeqLocs [in]
 * @param sbp Scoring and statistical information block [in]
 * @param query_info_ptr The filled structure [out]
 */
static Int2
BLAST_SetUpQueryInfo(Uint1 prog_number, SeqLocPtr query_slp, 
   BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr PNTR query_info_ptr)
{
   Int4 query_length;
   BlastQueryInfoPtr query_info;
   Int4 index, numseqs, num_contexts, context;
   SeqLocPtr slp;

   if (!query_slp || !sbp || !query_info_ptr)
      return -1;

   /* Count the number of queries by parsing the SeqLoc list */
   numseqs = num_contexts = ValNodeLen(query_slp);

   /* For blastx and tblastx there are 6 SeqLocs per query. */
   if (prog_number == blast_type_blastx || 
       prog_number == blast_type_tblastx) {
      numseqs /= 6;
   } else if (prog_number == blast_type_blastn) {
      num_contexts *= 2;
   }

   query_info = (BlastQueryInfoPtr) Malloc(sizeof(BlastQueryInfo));

   query_info->first_context = 0;
   query_info->last_context = num_contexts - 1;
   query_info->num_queries = numseqs;
   query_info->query_slp = query_slp;

   query_info->context_offsets =
      (Int4Ptr) Malloc((num_contexts+1)*sizeof(Int4));

   query_info->eff_searchsp_array = 
      (Int8Ptr) Malloc(num_contexts*sizeof(Int8));
   query_info->qid_array = (SeqIdPtr PNTR) Malloc(numseqs*sizeof(SeqIdPtr));

   query_info->context_offsets[0] = 0;

   switch (prog_number) {
   case blast_type_blastn:
      for (index = 0, slp = query_slp; index < numseqs && slp;
           ++index, slp = slp->next) {
         context = 2*index + 1;
         query_length = SeqLocLen(slp);
         query_info->context_offsets[context] =
            query_info->context_offsets[context-1] + query_length + 1;
         query_info->context_offsets[context+1] =
            query_info->context_offsets[context] + query_length + 1;
         query_info->qid_array[index] = SeqLocId(slp);
      }
      break;
   case blast_type_blastx: case blast_type_tblastx:
      for (index = 0, slp = query_slp; index < num_contexts && slp;
           ++index, slp = slp->next) {
         query_length = SeqLocLen(slp);
         query_info->context_offsets[index+1] =
            query_info->context_offsets[index] + query_length + 1;
         if ((index % 6) == 0) {
            query_info->qid_array[index/6] = (SeqLocId(slp))->next;
         }
      }
      break;
   default: /* blastp, tblastn */
      for (index = 0, slp = query_slp; index < numseqs && slp;
           ++index, slp = slp->next) {
         query_length = SeqLocLen(slp);
         query_info->context_offsets[index+1] =
            query_info->context_offsets[index] + query_length + 1;
         query_info->qid_array[index] = SeqLocId(slp);
      }
      break;
   }
   *query_info_ptr = query_info;
   return 0;
}

/** Function to calculate effective query length and db length as well as
 * effective search space. 
 * @param program blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options used to calc. effective lengths [in]
 * @param sbp Karlin-Altschul parameters [out]
 * @param query_info The query information block, which stores the effective
 *                   search spaces for all queries [in] [out]
 * @param query_slp List of query SeqLoc's [in]
*/
static Int2 BlastSetUp_CalcEffLengths (const Char *program, 
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options, 
   const BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info, 
   SeqLocPtr query_slp)
{
   Nlm_FloatHi alpha, beta; /*alpha and beta for new scoring system */
   Int4 min_query_length;   /* lower bound on query length. */
   Int4 length_adjustment;  /* length adjustment for current iteration. */
   Int4 last_length_adjustment;/* length adjustment in previous iteration.*/
   Int4 index;		/* loop index. */
   Int4	db_num_seqs;	/* number of sequences in database. */
   Int8	db_length;	/* total length of database. */
   BLAST_KarlinBlkPtr *kbp_ptr; /* Array of Karlin block pointers */
   BLAST_KarlinBlkPtr kbp; /* Karlin-Blk pointer from ScoreBlk. */
   Int4 query_length;   /* length of an individual query sequence */
   Int8 effective_length, effective_db_length; /* effective lengths of 
                                                  query and database */
   Int8 effective_search_space; /* Effective search space for a given 
                                   sequence/strand/frame */
   SeqLocPtr slp;       /* Iterator for the SeqLoc list */
   Int2 i; /* Iteration index for calculating length adjustment */
   Uint1 num_strands;

   if (sbp == NULL || eff_len_options == NULL)
      return 1;
   
   /* use values in BlastEffectiveLengthsOptionsPtr */
   db_length = eff_len_options->db_length;
   if (StringCmp(program, "tblastn") == 0 || 
       StringCmp(program, "tblastx") == 0)
      db_length = db_length/3;	
   
   db_num_seqs = eff_len_options->dbseq_num;
   
   if (StringCmp(program, "blastn") != 0) {
      if (scoring_options->gapped_calculation) {
         getAlphaBeta(sbp->name,&alpha,&beta,TRUE, 
            scoring_options->gap_open, scoring_options->gap_extend);
      }
   }
   
   if (scoring_options->gapped_calculation && 
       StringCmp(program, "blastn")) 
      kbp_ptr = sbp->kbp_gap_std;
   else
      kbp_ptr = sbp->kbp_std; 
   
   if (!StringCmp(program, "blastn"))
      num_strands = 2;
   else
      num_strands = 1;

   for (index = 0, slp = query_slp; slp; slp = slp->next) {
      if (eff_len_options->searchsp_eff) {
         effective_search_space = eff_len_options->searchsp_eff;
      } else {
         query_length = SeqLocLen(slp);
         /* Use the correct Karlin block. For blastn, two identical Karlin 
            blocks are allocated for each sequence (one per strand), but we
            only need one of them.
         */
         kbp = kbp_ptr[index];
         length_adjustment = 0;
         last_length_adjustment = 0;
         
         min_query_length = (Int4) 1/(kbp->K);

         for (i=0; i<5; i++) {
            if (StringCmp(program, "blastn") != 0 && 
                scoring_options->gapped_calculation) {
               length_adjustment = Nlm_Nint((((kbp->logK)+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(db_num_seqs, db_length-db_num_seqs*last_length_adjustment)))*alpha/kbp->Lambda) + beta);
            } else {
               length_adjustment = (Int4) (kbp->logK+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(1, db_length-db_num_seqs*last_length_adjustment)))/(kbp->H);
            }

            if (length_adjustment >= query_length-min_query_length) {
               length_adjustment = query_length-min_query_length;
               break;
            }
            
            if (ABS(last_length_adjustment-length_adjustment) <= 1)
               break;
            last_length_adjustment = length_adjustment;
         }
         
         effective_length = 
            MAX(query_length - length_adjustment, min_query_length);
         effective_db_length = MAX(1, db_length - db_num_seqs*length_adjustment);
         
         effective_search_space = effective_length * effective_db_length;
      }
      for (i = 0; i < num_strands; ++i) {
         query_info->eff_searchsp_array[index] = effective_search_space;
         ++index;
      }
   }

   return 0;
}


/********************************************************************************

	"Main" setup routine for BLAST.

	All the query sequences to be searched should have a SeqLocPtr in slp,
	if "concatenate" is TRUE they will be concatenated and the megablast lookup table created.

********************************************************************************/

Int2 LIBCALL
BlastSetUp_Main
(SeqLocPtr query_slp,
const Char *program,
const QuerySetUpOptionsPtr qsup_options,
const BlastScoringOptionsPtr scoring_options,
const BlastEffectiveLengthsOptionsPtr eff_len_options,
const LookupTableOptionsPtr	lookup_options,	
const BlastInitialWordOptionsPtr word_options,
const BlastHitSavingOptionsPtr hit_options,
Boolean concatenate,
const Int4Ptr frame,
ValNodePtr *seq_blocks,
BlastQueryInfoPtr *query_info,
SeqLocPtr *filter_slp_out,
BLAST_ExtendWordPtr *ewp,
BLAST_ScoreBlkPtr *sbpp,
LookupTableWrapPtr *lookup_wrap,
BlastHitSavingParametersPtr *hit_parameters,
Blast_MessagePtr *blast_message	
)
{
	BLAST_ScoreBlkPtr sbp;
	BLAST_SequenceBlkPtr sequence_block;	/* Stores sequence as Uint1Ptr and length. */
	BLAST_SequenceBlkPtr seq_block_array;	/* array of BLAST_SequenceBlk. */
	Boolean mask_at_hash;			/* mask only for making lookup table? */
	Boolean is_na;				/* Is this nucleotide? */
	Int2 context=0;				/* Loop variable. */
	Int2 total_num_contexts=0;		/* number of different strands, sequences, etc. */
	Int2 status=0;				/* return value */
	Int2 total_iterations;			/* loop variable for different strands. */
	Int4 query_length=0;			/* Length of query described by
                                                   SeqLocPtr. */
        Int4 dna_length=0;     /* Length of the underlying nucleotide sequence if
                                queries are translated */
                                                   
	Int4 double_int_offset=0;		/* passed to BlastSetUp_CreateDoubleInt */
	SeqLocPtr filter_slp=NULL;		/* SeqLocPtr computed for filtering. */
	SeqLocPtr filter_slp_combined;		/* Used to hold combined SeqLoc's */
	Uint1Ptr buffer;			/* holds sequence for plus strand or protein. */
	Uint1Ptr buffer_var = NULL;		/* holds offset of buffer to be worked on. */
	ValNodePtr vnp_di=NULL;			/* Holds the DoubleIntPtr's for location. */
	ValNodePtr vnp_di_array=NULL;		/* Holds the DoubleIntPtr's for location. */
	SeqLocPtr tmp_slp=query_slp;		/* loop variable */
        SeqLocPtr slp = query_slp;              /* variable pointer */ 
	Boolean reverse;                        /* Indicates the strand when masking filtered locations */
        SeqLocPtr mask_slp, next_mask_slp;      /* Auxiliary locations for lower case masks */
        Int4 counter;
        SeqIdPtr seqid = NULL, mask_seqid = NULL, next_mask_seqid = NULL;
        Uint1 program_number;

	if ((status=BlastScoringOptionsValidate(scoring_options, blast_message)) != 0)
	{
		return status;
	}

	if ((status=LookupTableOptionsValidate(lookup_options, blast_message)) != 0)
	{
		return status;
	}

	if ((status=BlastHitSavingOptionsValidate(hit_options, blast_message)) != 0)
	{
		return status;
	}

        BlastProgram2Number(program, &program_number);

	is_na = ISA_na(SeqLocMol(slp));
	while (tmp_slp)
	{	/* Only nucleotide can have two strands. */
		if (is_na && SeqLocStrand(tmp_slp) == Seq_strand_both)	
			total_num_contexts += 2;
		else
			total_num_contexts++;

		tmp_slp = tmp_slp->next;
	}

	vnp_di_array = MemNew(total_num_contexts*sizeof(ValNode));
	seq_block_array = MemNew(total_num_contexts*sizeof(BLAST_SequenceBlk));

	sbp = *sbpp;
	if (!sbp)
	{
		if (is_na)
			sbp = BLAST_ScoreBlkNew(BLASTNA_SEQ_CODE, total_num_contexts);
		else
			sbp = BLAST_ScoreBlkNew(Seq_code_ncbistdaa, total_num_contexts);
	
        /* Set the ambiguous residue before the ScoreBlk is filled. */
        if (is_na) {
           if (!StringHasNoText(scoring_options->matrix)) {
              sbp->read_in_matrix = TRUE;
           } else {
              sbp->read_in_matrix = FALSE;
           }
           BlastScoreSetAmbigRes(sbp, 'N');
        } else {
           sbp->read_in_matrix = TRUE;
           BlastScoreSetAmbigRes(sbp, 'X');
        }

		/* Fills in block for gapped blast. */
		status = BlastScoreBlkGappedFill(sbp, scoring_options, program);
		if (status)
			return status;

        *sbpp = sbp;
	}

	

	buffer = NULL;
	if (concatenate)
	{
		Int4 buffer_length=0;

		if ((status=BlastSetUp_GetSequence(slp, TRUE, concatenate, &buffer, &buffer_length, NULL)))
			return status; 

                /* Do not count the first and last sentinel bytes in the 
                   query length */
		if ((status=BlastSetUp_SeqBlkNew(buffer, buffer_length-2, 
                     context, NULL, &sequence_block, TRUE)))
			return status;

		ValNodeAddPointer(seq_blocks, 0, sequence_block);
		MemCpy(&(seq_block_array[context]), sequence_block, sizeof(BLAST_SequenceBlk));

		buffer_var = buffer;
	
                /* First byte is sentinel */
                ++buffer_var;
	}

        next_mask_slp = qsup_options->lcase_mask;
        mask_slp = NULL;

        counter = 0;
	while (slp)
	{
           ++counter;
		if (!concatenate)
		{
			Int4 buffer_length=0;

			buffer = NULL;
			if ((status=BlastSetUp_GetSequence(slp, TRUE, concatenate, &buffer, &buffer_length, NULL)))
				return status; 

			if ((status=BlastSetUp_SeqBlkNew(buffer, buffer_length,
                             context, NULL, &sequence_block, TRUE)))
				return status;

			ValNodeAddPointer(seq_blocks, 0, sequence_block);
			MemCpy(&(seq_block_array[context]), sequence_block, sizeof(BLAST_SequenceBlk));

			buffer_var = buffer;
	
			/* For blastn the first byte is sentinel */
			if (!StrCmp(program, "blastn"))
	   			++buffer_var;
		}

		if((status=BlastSetUp_Filter(slp, qsup_options->filter_string, 
                                             &mask_at_hash, &filter_slp)))
			return status;

                query_length = SeqLocLen(slp);

                /* Retrieve the id of the query sequence. If this id is for a 
                   translated sequence, get the underlying query id in the next 
                   pointer, and correct the ids in the filter SeqLoc */ 
                seqid = SeqLocId(slp);
                if (seqid->next) {
                   SeqLocPtr dna_seqloc = NULL;
                   seqid = seqid->next;
                   ValNodeAddPointer(&dna_seqloc, SEQLOC_WHOLE, seqid);
                   dna_length = SeqLocLen(dna_seqloc);
                }

                /* Extract the mask location corresponding to this 
                   query, detach it from other queries' masks */
                if (mask_slp || next_mask_slp) {
                   /* If previous mask is from this query, which can only 
                      happen when it's a different frame of the same query,
                      use it again, else go to the next mask */
                   if (!mask_seqid || 
                       SeqIdComp(mask_seqid, seqid) != SIC_YES) {
                      next_mask_seqid = SeqLocId(next_mask_slp);
                      if (SeqIdComp(next_mask_seqid, seqid) == SIC_YES) { 
                         mask_slp = next_mask_slp;
                         if (mask_slp) {
                            mask_seqid = next_mask_seqid;
                            next_mask_slp = mask_slp->next;
                            mask_slp->next = NULL;
                         }
                      } else {
                         mask_slp = NULL;
                      }
                   }
                   if (mask_slp) {
                      if (frame) {
                         tmp_slp = NULL;
                         BlastSetUp_TranslateDNASeqLoc(mask_slp, 
                            frame[context], dna_length, seqid, 
                            &tmp_slp);
                         ValNodeAddPointer(&filter_slp, SEQLOC_MIX, 
                                           tmp_slp);
                      } else {
                         ValNodeAddPointer(&filter_slp, SEQLOC_MIX, 
                                           mask_slp);
                         mask_seqid = NULL;
                      }
                   }
                }

		filter_slp_combined = NULL;
		CombineSeqLocs(filter_slp, &filter_slp_combined);
		filter_slp = SeqLocSetFree(filter_slp);

		if (filter_slp_combined && !mask_at_hash)
		{
			if ((frame && StringCmp("blastx", program) == 0) || StringCmp("tblastx", program) == 0)
			{
				SeqLocPtr filter_slp_converted=NULL;
				BlastSetUp_ConvertProteinSeqLoc(filter_slp_combined, frame[context], dna_length, &filter_slp_converted);
                                HackSeqLocId(filter_slp_converted, seqid);
				ValNodeAddPointer(filter_slp_out, FrameToDefine(frame[context]), filter_slp_converted);
			}
			else
			{
				ValNodeAddPointer(filter_slp_out, SEQLOC_MASKING_NOTSET, filter_slp_combined);
			}
		}

		/* For plus strand or protein. */
		total_iterations=1;
		if (is_na && SeqLocStrand(slp) == Seq_strand_both)
			total_iterations++; /* Two iteration for two strands. */
		reverse = FALSE;
		while (total_iterations > 0)
		{
		    if (buffer)
		    {	
			Int4Ptr double_int_offset_ptr = concatenate ? &double_int_offset : NULL;

			if (!mask_at_hash)
			{
                           if((status = BlastSetUp_MaskTheResidues(buffer_var, query_length, is_na, filter_slp_combined, reverse, 0)))
                              return status;
                           /* Create vnp_di spanning entire sequence. */
                           BlastSetUp_CreateDoubleInt(slp, NULL, FALSE, 
                              &vnp_di, double_int_offset_ptr);
			}
			else
			{
                           BlastSetUp_CreateDoubleInt(slp, 
                              filter_slp_combined, reverse, &vnp_di,
                              double_int_offset_ptr);
			}
			MemCpy(&(vnp_di_array[context]), vnp_di, sizeof(ValNode));
			double_int_offset++; /*add One for sentinel byte between sequences. */

			if ((status=BlastScoreBlkFill(sbp, (CharPtr)(buffer_var+1), query_length, context)))
				return status;

			context++;
			buffer_var += query_length+1;
		    }
		    total_iterations--;
		    reverse = !reverse;
		}

		if (filter_slp_combined)
		{
			if ((frame && StringCmp("blastx", program) == 0) || StringCmp("tblastx", program) == 0)
			{	/* Translated SeqLoc saved for blastx/tblastx. */
				filter_slp_combined = SeqLocSetFree(filter_slp_combined);
			}
		}
		
		slp = slp->next;
	}

	/* Get "ideal" values if the calculated Karlin-Altschul params bad. */
	if (StringCmp("blastx", program) == 0 || StringCmp("tblastx", program) == 0)
	{
		sbp->kbp = sbp->kbp_std;
		BlastKarlinBlkStandardCalc(sbp, 0, total_num_contexts-1);
	}


	/* Construct the lookup table. */
  	*lookup_wrap = MemNew(sizeof(LookupTableWrap));

	switch ( lookup_options->lut_type )
	  {
	  case AA_LOOKUP_TABLE:
	    
	    (*lookup_wrap)->lut_type = AA_LOOKUP_TABLE;
	    BlastAaLookupNew(lookup_options, (LookupTablePtr *)
                             &((*lookup_wrap)->lut));
            if (!concatenate)
	      {
		BlastAaLookupIndexQueries( (LookupTablePtr) (*lookup_wrap)->lut,
					   sbp->matrix,
					   seq_block_array,
					   vnp_di_array, 
					   total_num_contexts);
	      }
	    else
	      {
		BlastAaLookupIndexQueries( (LookupTablePtr) (*lookup_wrap)->lut,
					   sbp->matrix,
					   sequence_block,
					   vnp_di,
					   1);
	      }
	    _BlastAaLookupFinalize((LookupTablePtr) (*lookup_wrap)->lut);
	    
	    break;
		
	  case MB_LOOKUP_TABLE:
	    (*lookup_wrap)->lut_type = MB_LOOKUP_TABLE;
	    
	    MB_LookupTableNew(sequence_block, vnp_di, (MBLookupTablePtr *) &((*lookup_wrap)->lut), lookup_options);
	    break;
	  
	  case NA_LOOKUP_TABLE:
	    (*lookup_wrap)->lut_type = NA_LOOKUP_TABLE;
	    
	    LookupTableNew(lookup_options, (LookupTablePtr *) &((*lookup_wrap)->lut), FALSE);
	    
	    BlastNaLookupIndexQuery((LookupTablePtr) (*lookup_wrap)->lut, sequence_block, vnp_di);
	    _BlastAaLookupFinalize((LookupTablePtr) (*lookup_wrap)->lut);
	    break;

	  default:
	    {
	      /* FIXME - emit error condition here */
	    }
	  } /* end switch */

	 /* Adjust the Karlin parameters for ungapped blastx/tblastx. */
        if (StringCmp(program, "blastx") == 0  || StringCmp(program, "tblastx") == 0)
        {
		sbp->kbp = sbp->kbp_gap;
               	BlastKarlinBlkStandardCalc(sbp, 0, sbp->number_of_contexts-1);
        }

        if (ewp && (status = BLAST_ExtendWordInit(sequence_block, word_options, 
                                eff_len_options->db_length, 
                                eff_len_options->dbseq_num, ewp)) != 0)
           return status;

        /* Why are there so many Karlin block names?? */
        sbp->kbp = sbp->kbp_std;
        sbp->kbp_gap = sbp->kbp_gap_std;

        /* Fill the query information structure, including the context 
           offsets for multiple queries */
        if (query_info && 
            (status = BLAST_SetUpQueryInfo(program_number, query_slp, sbp, 
                                           query_info)) != 0)
           return status;

        if ((status = BlastSetUp_CalcEffLengths(program, scoring_options, 
                         eff_len_options, sbp, *query_info, query_slp)) != 0)
           return status;

        if (hit_parameters)
           BlastHitSavingParametersNew(hit_options, NULL, sbp, *query_info,
                                       hit_parameters);

	return 0;
}

