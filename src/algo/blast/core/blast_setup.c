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
* Revision 1.23  2003/06/05 18:33:16  dondosha
* Compiler warning fixes
*
* Revision 1.22  2003/06/05 17:15:23  dondosha
* SeqLoc is no longer used for query masking/filtering
*
* Revision 1.21  2003/05/30 15:52:11  coulouri
* various lint-induced cleanups
*
* Revision 1.20  2003/05/21 22:29:04  dondosha
* Fix for two sequences tblastn traceback
*
* Revision 1.19  2003/05/20 15:47:16  dondosha
* Fix in BLAST_SetUpSubject for two sequences case
*
* Revision 1.18  2003/05/18 21:57:37  camacho
* Use Uint1 for program name whenever possible
*
* Revision 1.17  2003/05/15 22:01:22  coulouri
* add rcsid string to sources
*
* Revision 1.16  2003/05/08 18:20:47  dondosha
* Corrected order of query frames
*
* Revision 1.15  2003/05/06 21:27:31  dondosha
* Moved auxiliary functions from blast_driver.c to relevant modules
*
* Revision 1.14  2003/05/01 17:08:45  dondosha
* BLAST_SetUpAuxStructures moved from blast_setup to blast_engine module
*
* Revision 1.13  2003/05/01 16:56:30  dondosha
* Fixed strict compiler warnings
*
* Revision 1.12  2003/05/01 15:33:39  dondosha
* Reorganized the setup of BLAST search
*
* Revision 1.11  2003/04/21 18:19:52  dondosha
* UMR fixes
*
* Revision 1.10  2003/04/21 16:18:22  dondosha
* Minor correction in BlastSetUp_GetSequence
*
* Revision 1.9  2003/04/18 22:26:45  dondosha
* Removed structures generated from ASN.1 from the main engine of BLAST
*
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

static char const rcsid[] = "$Id$";

#include <blast_setup.h>
#include <seg.h>
#include <urkpcc.h>
#include <dust.h>
#include <seqport.h>
#include <sequtil.h>
#include <objloc.h>
#include <blast_util.h>
#include <blast_filter.h>

extern Uint1 FrameToDefine PROTO((Int2 frame));
extern void HackSeqLocId(SeqLocPtr slp, SeqIdPtr id);
extern Int4 SeqLocTotalLen (CharPtr prog_name, SeqLocPtr slp);

/* These should be defined elsewhere so other trans. units can use them. */
#define SEQLOC_MASKING_NOTSET 0
#define CODON_LENGTH 3


/* BlastScoreBlkGappedFill, fills the ScoreBlkPtr for a gapped search.  
 *	Should be moved to blastkar.c (or it's successor) in the future.
*/
Int2 
BlastScoreBlkGappedFill(BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr scoring_options, const Uint1 program)
{

	if (sbp == NULL || scoring_options == NULL)
		return 1;


	if (program == blast_type_blastn)
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

Int2 BLAST_GetTranslatedSeqLoc(SeqLocPtr query_slp, 
        SeqLocPtr PNTR protein_slp_head, Int4Ptr full_dna_length, 
        Int4Ptr PNTR frame_info_ptr)
{
   BioseqPtr protein_bsp;
   SeqLocPtr protein_slp = NULL, last_slp = NULL;
   CharPtr genetic_code=NULL;
   GeneticCodePtr gcp;
   Int4 protein_length;
   Int4 query_length;
   Int4 buffer_length;
   Int4 index=0, i;
   Uint1Ptr buffer, buffer_var, sequence;
   Int2 status;
   ValNodePtr vnp;
   Int4 frame_array[6] = { 1, 2, 3, -1, -2, -3 };
   Int4Ptr frame_info;
   SeqIdPtr seqid;
   
   if (!query_slp)
      return -1;

   *full_dna_length = SeqLocTotalLen("blastn", query_slp);
   *protein_slp_head = NULL;
   *frame_info_ptr = frame_info = 
      (Int4Ptr) Malloc(6*ValNodeLen(query_slp)*sizeof(Int4));

   gcp = GeneticCodeFind(1, NULL);
   for (vnp = (ValNodePtr)gcp->data.ptrvalue; vnp != NULL; 
        vnp = vnp->next) {
      if (vnp->choice == 3) {  /* ncbieaa */
         genetic_code = (CharPtr)vnp->data.ptrvalue;
         break;
      }
   }
   
   for ( ; query_slp; query_slp = query_slp->next) {
      query_length = SeqLocLen(query_slp);
      status = BlastSetUp_GetSequence(query_slp, FALSE, TRUE, &buffer, 
                                      &buffer_length, NULL);
      buffer_var = buffer + 1;
      
      for (i = 0; i < 6; i++) {
         if (i == 3) {
            /* Advance the query pointer to the beginning of the reverse
               strand */
            buffer_var += query_length + 1;
         }
         sequence = BLAST_GetTranslation(buffer_var, NULL, query_length, 
                       ABS(frame_array[i]), &protein_length, genetic_code);
         protein_bsp = BLAST_MakeTempProteinBioseq(sequence+1, 
                          protein_length, Seq_code_ncbistdaa);
         protein_slp = SeqLocIntNew(0, protein_length-1, Seq_strand_plus, 
                                    protein_bsp->id);
         /* Attach the underlying query id to the unique id of this 
            SeqLoc */
         seqid = SeqLocId(protein_slp);
         seqid->next = SeqLocId(query_slp);

         if (*protein_slp_head == NULL)
            *protein_slp_head = protein_slp;
         else
            ValNodeLink(&last_slp, protein_slp);
         last_slp = protein_slp;
         
         frame_info[index] = frame_array[i];
         index++;
      }

      buffer = MemFree(buffer);
   }
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
	ValNodePtr mask_loc, Boolean reverse, Int4 offset)
{
   DoubleIntPtr loc = NULL;
   Int2 status = 0;
   Int4 index, start, stop;
   Uint1 mask_letter;
   
   if (is_na)
      mask_letter = 14;
   else
      mask_letter = 21;
   
   for ( ; mask_loc; mask_loc = mask_loc->next) {
      loc = (DoubleIntPtr) mask_loc->data.ptrvalue;
      if (reverse)	{
         start = max_length - 1 - loc->i2;
         stop = max_length - 1 - loc->i1;
      } else {
         start = loc->i1;
         stop = loc->i2;
      }
      
      start -= offset;
      stop  -= offset;
      
      for (index = start; index <= stop; index++)
         buffer[index] = mask_letter;
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
ValNodePtr *selcys_pos	/* [out] positions where selenocysteine was replaced by
                           X. */
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
        Boolean double_length = FALSE;
 
	slp_var = slp;
	length=0;
	slp_count=0;
	while (slp_var)
	{
		length += SeqLocLen(slp_var);
		slp_count++;
                if (SeqLocStrand(slp_var) == Seq_strand_both)
                   double_length = TRUE;
		if (!concatenate)
			break;
		slp_var = slp_var->next;
	}

	is_na = ISA_na(mol);

	if (is_na && double_length)
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

Int2 BLAST_GetSubjectSequence(SeqLocPtr subject_slp, Uint1Ptr *buffer,
        Int4 *buffer_length, Uint1 encoding)
{
   Uint1Ptr buffer_var;
   SeqPortPtr spp = NULL;
   SPCompressPtr spc=NULL;
   Uint1 residue;
   BioseqPtr subject_bsp;

   *buffer_length = SeqLocLen(subject_slp);

   switch (encoding) {
   case BLASTP_ENCODING:
      *buffer = buffer_var = (Uint1Ptr) Malloc(*buffer_length + 2);
      *buffer_var = NULLB;
      spp = SeqPortNewByLoc(subject_slp, Seq_code_ncbistdaa);
      while ((residue=SeqPortGetResidue(spp)) != SEQPORT_EOF) {
         if (IS_residue(residue)) {
           *(++buffer_var) = residue;
         }
      }
      *(++buffer_var) = NULLB;
      break;
   case NCBI4NA_ENCODING:
      /* No extra sentinel bytes are allocated in this case, 
         mirroring the readdb API */
      *buffer = buffer_var = (Uint1Ptr) Malloc(*buffer_length);
      spp = SeqPortNewByLoc(subject_slp, Seq_code_ncbi4na);
      subject_bsp = BioseqFindCore(SeqLocId(subject_slp));
      if (subject_bsp != NULL && subject_bsp->repr == Seq_repr_delta)
         SeqPortSet_do_virtual(spp, TRUE);
      
      while ((residue=SeqPortGetResidue(spp)) != SEQPORT_EOF) {
         if (IS_residue(residue)) {
            *(buffer_var++) = residue;
         }
      }
      break;
   case BLASTNA_ENCODING:
      *buffer = buffer_var = (Uint1Ptr) Malloc(*buffer_length + 2);
      *buffer_var = NULLB;
      spp = SeqPortNewByLoc(subject_slp, Seq_code_ncbi4na);
      subject_bsp = BioseqFindCore(SeqLocId(subject_slp));
      if (subject_bsp != NULL && subject_bsp->repr == Seq_repr_delta)
         SeqPortSet_do_virtual(spp, TRUE);
      *buffer_var = ncbi4na_to_blastna[0];
      
      while ((residue=SeqPortGetResidue(spp)) != SEQPORT_EOF) {
         if (IS_residue(residue)) {
            *(++buffer_var) = ncbi4na_to_blastna[residue];
         }
      }
      /* Gap character in last space. */
      *(++buffer_var) = ncbi4na_to_blastna[0];
      break;
   case NCBI2NA_ENCODING:
      spp = SeqPortNewByLoc(subject_slp, Seq_code_ncbi4na);
      subject_bsp = BioseqFindCore(SeqLocId(subject_slp));
      if (subject_bsp != NULL && subject_bsp->repr == Seq_repr_delta)
         SeqPortSet_do_virtual(spp, TRUE);
      spc = SPCompressDNA(spp);
      if (spc == NULL) {
         *buffer = NULL;
         *buffer_length = 0;
      } else {
         *buffer = spc->buffer;
         spc->buffer = NULL;
         SPCompressFree(spc);
      }
      break;
   default:
      break;
   }
   spp = SeqPortFree(spp);
   return 0;
}

/** Create and fill the query information structure, including sequence ids, 
 * and array of offsets into a concatenated sequence, if needed.
 * @param prog_number Numeric value of the BLAST program [in]
 * @param query_slp List of query SeqLocs [in]
 * @param query_info_ptr The filled structure [out]
 */
static Int2
BLAST_SetUpQueryInfo(Uint1 prog_number, SeqLocPtr query_slp, 
   BlastQueryInfoPtr PNTR query_info_ptr)
{
   Int4 query_length;
   BlastQueryInfoPtr query_info;
   Int4 index, numseqs, num_contexts, context;
   SeqLocPtr slp;
   Int4 total_length = 0;

   if (!query_slp || !query_info_ptr)
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

   query_info->context_offsets =
      (Int4Ptr) Malloc((num_contexts+1)*sizeof(Int4));

   query_info->eff_searchsp_array = 
      (Int8Ptr) Malloc(num_contexts*sizeof(Int8));

   query_info->context_offsets[0] = 0;

   switch (prog_number) {
   case blast_type_blastn:
      for (index = 0, slp = query_slp; index < numseqs && slp;
           ++index, slp = slp->next) {
         context = 2*index + 1;
         query_length = SeqLocLen(slp);
         total_length += query_length;
         query_info->context_offsets[context] =
            query_info->context_offsets[context-1] + query_length + 1;
         query_info->context_offsets[context+1] =
            query_info->context_offsets[context] + query_length + 1;
      }
      break;
   case blast_type_blastx: case blast_type_tblastx:
      for (index = 0, slp = query_slp; index < num_contexts && slp;
           ++index, slp = slp->next) {
         query_length = SeqLocLen(slp);
         total_length += query_length;
         query_info->context_offsets[index+1] =
            query_info->context_offsets[index] + query_length + 1;
      }
      break;
   default: /* blastp, tblastn */
      for (index = 0, slp = query_slp; index < numseqs && slp;
           ++index, slp = slp->next) {
         query_length = SeqLocLen(slp);
         total_length += query_length;
         query_info->context_offsets[index+1] =
            query_info->context_offsets[index] + query_length + 1;
      }
      break;
   }
   query_info->total_length = total_length;
   *query_info_ptr = query_info;
   return 0;
}

Int2 BLAST_SetUpQuery(SeqLocPtr query_slp, const Uint1 program_number,
        BLAST_SequenceBlkPtr *query_blk, BlastQueryInfoPtr *query_info)
{
   Uint1Ptr buffer;	/* holds sequence for plus strand or protein. */
   Int4 buffer_length;
   Int2 status;

   if ((status=BlastSetUp_GetSequence(query_slp, TRUE, TRUE, &buffer, 
                                      &buffer_length, NULL)))
      return status; 
        
   /* Do not count the first and last sentinel bytes in the 
      query length */
   if ((status=BlastSetUp_SeqBlkNew(buffer, buffer_length-2, 
                                    0, NULL, query_blk, TRUE)))
      return status;

   /* Fill the query information structure, including the context 
      offsets for multiple queries */
   if (query_info && 
       (status = BLAST_SetUpQueryInfo(program_number, query_slp, 
                                      query_info)) != 0)
      return status;
   
   return 0;
}


Int2 BLAST_MainSetUp(const Uint1 program_number,
        const QuerySetUpOptionsPtr qsup_options,
        const BlastScoringOptionsPtr scoring_options,
        const LookupTableOptionsPtr lookup_options,	
        const BlastHitSavingOptionsPtr hit_options,
        BLAST_SequenceBlkPtr query_blk,
        BlastSeqLocPtr *lookup_segments,
        BlastQueryInfoPtr query_info, BlastMaskPtr *filter_out,
        BLAST_ScoreBlkPtr *sbpp, Blast_MessagePtr *blast_message)
{
   BLAST_ScoreBlkPtr sbp;
   Boolean mask_at_hash;/* mask only for making lookup table? */
   Boolean is_na;	/* Is this nucleotide? */
   Int2 context=0;	/* Loop variable. */
   Int2 total_num_contexts=0;/* number of different strands, sequences, etc. */
   Int2 status=0;	/* return value */
   Int4 query_length=0;	/* Length of query described by SeqLocPtr. */
   BlastSeqLocPtr filter_slp=NULL;/* SeqLocPtr computed for filtering. */
   BlastSeqLocPtr filter_slp_combined;/* Used to hold combined SeqLoc's */
   BlastSeqLocPtr loc; /* Iterator variable */
   BlastMaskPtr last_filter_out = NULL; 
   Uint1Ptr buffer;	/* holds sequence for plus strand or protein. */
   Boolean reverse; /* Indicates the strand when masking filtered locations */
   BlastMaskPtr mask_slp, next_mask_slp;/* Auxiliary locations for lower 
                                              case masks */
   Int4 context_offset;
   
   if ((status=
        BlastScoringOptionsValidate(scoring_options, blast_message)) != 0)
      return status;

   if ((status=
        LookupTableOptionsValidate(lookup_options, blast_message)) != 0)
      return status;
   
   if ((status =
        BlastHitSavingOptionsValidate(hit_options, program_number, 
                                      blast_message)) != 0)
      return status;
   
   /* At this stage query sequences are nucleotide only for blastn */
   is_na = (program_number == blast_type_blastn);
   total_num_contexts = query_info->last_context + 1;

   sbp = *sbpp;
   if (!sbp) {
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
      status = BlastScoreBlkGappedFill(sbp, scoring_options, program_number);
      if (status)
         return status;
      
      *sbpp = sbp;
   }
   
   next_mask_slp = qsup_options->lcase_mask;
   mask_slp = NULL;
   
   for (context = query_info->first_context; 
        context <= query_info->last_context; ++context) {
      context_offset = query_info->context_offsets[context];
      buffer = &query_blk->sequence[context_offset];
      query_length = query_info->context_offsets[context+1] 
         - context_offset - 1;

      reverse = (is_na && ((context & 1) != 0));

      /* It is not necessary to do filtering on the reverse strand - the 
         masking locations are the same as on the forward strand */
      if (!reverse) {
         if ((status = BlastSetUp_Filter(program_number, buffer, 
                         query_length, 0, qsup_options->filter_string, 
                         &mask_at_hash, &filter_slp)))
            return status;
      
         /* Extract the mask locations corresponding to this query 
            (frame, strand), detach it from other masks.
            NB: for translated search the mask locations are expected in 
            protein coordinates. The nucleotide locations must be converted
            to protein coordinates prior to the call to BLAST_MainSetUp.
         */
         if (next_mask_slp && (next_mask_slp->index == context)) {
            mask_slp = next_mask_slp;
            next_mask_slp = mask_slp->next;
            mask_slp->next = NULL;
         } else {
            mask_slp = NULL;
         }
         
         /* Attach the lower case mask locations to the filter locations 
            and combine them */
         if (filter_slp && mask_slp) {
            for (loc = filter_slp; loc->next; loc = loc->next);
            loc->next = mask_slp->loc_list;
         }
         
         filter_slp_combined = NULL;
         CombineMaskLocations(filter_slp, &filter_slp_combined);
         /*filter_slp = SeqLocSetFree(filter_slp);*/
         
         /* NB: for translated searches filter locations are returned in 
            protein coordinates, because the DNA lengths of sequences are 
            not available here. The caller must take care of converting 
            them back to nucleotide coordinates. */
         if (filter_slp_combined) {
            if (!last_filter_out) {
               last_filter_out = *filter_out = 
                  (BlastMaskPtr) MemNew(sizeof(BlastMask));
            } else {
               last_filter_out->next = 
                  (BlastMaskPtr) MemNew(sizeof(BlastMask));
               last_filter_out = last_filter_out->next;
            }
            if (is_na)
               last_filter_out->index = context / 2;
            else
               last_filter_out->index = context;
            last_filter_out->loc_list = filter_slp_combined;
         }
      }
      if (buffer) {	
         if (!mask_at_hash) {
            if((status = 
                BlastSetUp_MaskTheResidues(buffer, query_length, is_na, 
                   filter_slp_combined, reverse, 0)))
                  return status;
         }
            
         if ((status=BlastScoreBlkFill(sbp, (CharPtr) buffer, 
                                       query_length, context)))
            return status;
      }
   }
   
   *lookup_segments = NULL;
   BLAST_ComplementMaskLocations(program_number, query_info, *filter_out, 
                                 lookup_segments);

   /* Free the filtering locations if masking done for lookup table only */
   if (mask_at_hash) {
      *filter_out = BlastMaskFree(*filter_out);
   }

   /* Get "ideal" values if the calculated Karlin-Altschul params bad. */
   if (program_number == blast_type_blastx || 
       program_number == blast_type_tblastx) {
      sbp->kbp = sbp->kbp_std;
      BlastKarlinBlkStandardCalc(sbp, 0, total_num_contexts-1);
      /* Adjust the Karlin parameters for ungapped blastx/tblastx. */
      sbp->kbp = sbp->kbp_gap;
      BlastKarlinBlkStandardCalc(sbp, 0, sbp->number_of_contexts-1);
   }

   /* Why are there so many Karlin block names?? */
   sbp->kbp = sbp->kbp_std;
   sbp->kbp_gap = sbp->kbp_gap_std;

   return 0;
}

#define MAX_NUM_QUERIES 16383 /* == 1/2 INT2_MAX */
#define MAX_TOTAL_LENGTH 20000000

Boolean
BLAST_GetQuerySeqLoc(FILE *infp, Boolean query_is_na, 
   BlastMaskPtr PNTR lcase_mask, SeqLocPtr PNTR query_slp, Int4 ctr_start)
{
   Int4 num_bsps;
   Int8 total_length;
   BioseqPtr query_bsp;
   SeqLocPtr mask_slp;
   BlastMaskPtr last_mask;
   Char prefix[2];     /* for FastaToSeqEntryForDb */
   Int2 ctr = ctr_start + 1; /* Counter for FastaToSeqEntryForDb */
   Int4 query_index = 0;
   BlastMaskPtr new_mask;
   SeqEntryPtr sep;
   Boolean done = TRUE;
   
   if (!query_slp)
      return -1;

   num_bsps = 0;
   total_length = 0;
   *query_slp = NULL;

   SeqMgrHoldIndexing(TRUE);
   mask_slp = NULL;
   last_mask = NULL;
   
   StrCpy(prefix, "");
   
   while ((sep=FastaToSeqEntryForDb(infp, query_is_na, NULL, FALSE, prefix, 
                                    &ctr, (lcase_mask ? &mask_slp : NULL))) != NULL)
   {
      if (mask_slp) {
         new_mask = BlastMaskFromSeqLoc(mask_slp, query_index);
         
         if (!last_mask)
            *lcase_mask = last_mask = new_mask;
         else {
            last_mask->next = new_mask;
            last_mask = last_mask->next;
         }
         /* Masking locations in a SeqLoc form are no longer needed */
         mask_slp = SeqLocSetFree(mask_slp);
      }
      ++query_index;
      
      query_bsp = NULL;
      if (query_is_na) {
         SeqEntryExplore(sep, &query_bsp, FindNuc);
      } else {
         SeqEntryExplore(sep, &query_bsp, FindProt);
      }
      
      if (query_bsp == NULL) {
         ErrPostEx(SEV_FATAL, 0, 0, "Unable to obtain bioseq\n");
         return 2;
      }
      
      ValNodeAddPointer(query_slp, SEQLOC_WHOLE, 
                        SeqIdDup(SeqIdFindBest(query_bsp->id, SEQID_GI)));
      
      total_length += query_bsp->length;
      ++num_bsps;
      if (total_length > MAX_TOTAL_LENGTH || 
          num_bsps >= MAX_NUM_QUERIES) {
         done = FALSE;
         if (total_length > MAX_TOTAL_LENGTH) {
            ErrPostEx(SEV_INFO, 0, 0, 
               "Total length of queries has exceeded %ld\n",
               MAX_TOTAL_LENGTH);
         } else {
            ErrPostEx(SEV_INFO, 0, 0, 
               "Number of query sequences has exceeded the limit of %ld\n",
               MAX_NUM_QUERIES);
         }
         break;
      }
   }
   
   SeqMgrHoldIndexing(FALSE);
   
   return done;
}

Int2 BLAST_SetUpSubject(CharPtr file_name, CharPtr blast_program, 
        SeqLocPtr PNTR subject_slp, BLAST_SequenceBlkPtr PNTR subject)
{
   FILE *infp2;
   Int2 status = 0;
   Uint1Ptr subject_buffer = NULL; /* Buffer for the compressed subject 
                                      sequence in two sequences case */
   Int4 buffer_length=0; /* Length of subject sequence for two sequences 
                            case */
   Uint1 program_number;
   Boolean is_na;
   Uint1 encoding;

   if ((infp2 = FileOpen(file_name, "r")) == NULL) {
      ErrPostEx(SEV_FATAL, 0, 0, 
                "blast: Unable to open second input file %s\n", 
                file_name);
      return (1);
   }
      
   BlastProgram2Number(blast_program, &program_number);
   is_na = (program_number != blast_type_blastp && 
            program_number != blast_type_blastx);
 
   BLAST_GetQuerySeqLoc(infp2, is_na, NULL, subject_slp, 0);
   FileClose(infp2);

   /* Fill the sequence block for the subject sequence */

   encoding = (is_na ? NCBI2NA_ENCODING : BLASTP_ENCODING);
   if ((status=BLAST_GetSubjectSequence(*subject_slp, &subject_buffer, 
                                        &buffer_length, encoding)))
      return status;
   

   if (is_na) {
      /* The last argument in the following call is FALSE, to force 
         'sequence' to be initialized instead of 'sequence_start' */
      if ((status=BlastSetUp_SeqBlkNew(subject_buffer, buffer_length,
                                       0, NULL, subject, FALSE)))
         return status;
      /* Save uncompressed sequence for reevaluation with ambiguities 
         and traceback */
      encoding = ((program_number == blast_type_blastn) ?
                  BLASTNA_ENCODING : NCBI4NA_ENCODING);
      if ((status = 
           BLAST_GetSubjectSequence(*subject_slp, 
              &((*subject)->sequence_start), &((*subject)->length), encoding)))
         return status;
      (*subject)->sequence_start_allocated = TRUE;
   } else {
      /* Save allocated buffer in sequence_start */
      if ((status=BlastSetUp_SeqBlkNew(subject_buffer, buffer_length,
                                       0, NULL, subject, TRUE)))
         return status;
   }

   return 0;
}

