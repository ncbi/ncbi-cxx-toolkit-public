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

/** @file blast_util.c
 * Various BLAST utilities
 */


static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_filter.h>


Int2
BlastSetUp_SeqBlkNew (const Uint1* buffer, Int4 length, Int4 context,
   BLAST_SequenceBlk* *seq_blk, Boolean buffer_allocated)
{
   /* Check if BLAST_SequenceBlk itself needs to be allocated here or not */
   if (*seq_blk == NULL) {
      *seq_blk = calloc(1, sizeof(BLAST_SequenceBlk));
   }

   if (buffer_allocated) {
      (*seq_blk)->sequence_start_allocated = TRUE;
      (*seq_blk)->sequence_start = (Uint1 *) buffer;
      /* The first byte is a sentinel byte. */
      (*seq_blk)->sequence = (*seq_blk)->sequence_start+1;
   } else {
      (*seq_blk)->sequence = (Uint1 *) buffer;
      (*seq_blk)->sequence_start = NULL;
   }
   
   (*seq_blk)->length = length;
   (*seq_blk)->context = context;
   
   return 0;
}

Int2 BlastSeqBlkNew(BLAST_SequenceBlk** retval)
{
    if ( !retval ) {
        return -1;
    } else {
        *retval = (BLAST_SequenceBlk*) calloc(1, sizeof(BLAST_SequenceBlk));
        if ( !*retval ) {
            return -1;
        }
    }

    return 0;
}

Int2 BlastSeqBlkSetSequence(BLAST_SequenceBlk* seq_blk, 
                            const Uint1* sequence,
                            Int4 seqlen)
{
    if ( !seq_blk ) {
        return -1;
    }

    seq_blk->sequence_start_allocated = TRUE;
    seq_blk->sequence_start = (Uint1*) sequence;
    seq_blk->sequence = (Uint1*) sequence + 1;
    seq_blk->length = seqlen;
    seq_blk->oof_sequence = NULL;

    return 0;
}

Int2 BlastSeqBlkSetCompressedSequence(BLAST_SequenceBlk* seq_blk, 
                                      const Uint1* sequence)
{
    if ( !seq_blk ) {
        return -1;
    }

    seq_blk->sequence_allocated = TRUE;
    seq_blk->sequence = (Uint1*) sequence;
    seq_blk->oof_sequence = NULL;

    return 0;
}

#if 0
/** Create the subject sequence block given an ordinal id in a database */
void
MakeBlastSequenceBlk(ReadDBFILEPtr db, BLAST_SequenceBlk** seq_blk,
                     Int4 oid, Uint1 encoding)
{
  Int4 length, buf_len = 0;
  Uint1* buffer = NULL;

  if (encoding == BLASTNA_ENCODING) {
     length = readdb_get_sequence_ex(db, oid, &buffer, &buf_len, TRUE);
  } else if (encoding == NCBI4NA_ENCODING) {
     length = readdb_get_sequence_ex(db, oid, &buffer, &buf_len, FALSE);
  } else {
     length=readdb_get_sequence(db, oid, &buffer);
  }

  BlastSetUp_SeqBlkNew(buffer, length, 0, seq_blk, 
                       (encoding != BLASTP_ENCODING));
  (*seq_blk)->oid = oid;
}
#endif

Int2 BlastSequenceBlkClean(BLAST_SequenceBlk* seq_blk)
{
   if (!seq_blk)
       return 1;

   if (seq_blk->sequence_allocated) {
       sfree(seq_blk->sequence);
       seq_blk->sequence_allocated = FALSE;
   }
   if (seq_blk->sequence_start_allocated) {
       sfree(seq_blk->sequence_start);
       seq_blk->sequence_start_allocated = FALSE;
   }
   if (seq_blk->oof_sequence_allocated) {
       sfree(seq_blk->oof_sequence);
       seq_blk->oof_sequence_allocated = FALSE;
   }

   return 0;
}

BLAST_SequenceBlk* BlastSequenceBlkFree(BLAST_SequenceBlk* seq_blk)
{
   if (!seq_blk)
      return NULL;

   BlastSequenceBlkClean(seq_blk);
   if (seq_blk->lcase_mask_allocated)
      BlastMaskLocFree(seq_blk->lcase_mask);
   sfree(seq_blk);
   return NULL;
}

void BlastSequenceBlkCopy(BLAST_SequenceBlk** copy, 
                          BLAST_SequenceBlk* src) 
{
   ASSERT(copy);
   ASSERT(src);
   
   if (*copy)
      memcpy(*copy, src, sizeof(BLAST_SequenceBlk));
   else 
      *copy = BlastMemDup(src, sizeof(BLAST_SequenceBlk));

   (*copy)->sequence_allocated = FALSE;
   (*copy)->sequence_start_allocated = FALSE;
   (*copy)->oof_sequence_allocated = FALSE;
   (*copy)->lcase_mask_allocated = FALSE;
}

Int2 BlastProgram2Number(const char *program, Uint1 *number)
{
	*number = eBlastTypeUndefined;
	if (program == NULL)
		return 1;

	if (strcasecmp("blastn", program) == 0)
		*number = eBlastTypeBlastn;
	else if (strcasecmp("blastp", program) == 0)
		*number = eBlastTypeBlastp;
	else if (strcasecmp("blastx", program) == 0)
		*number = eBlastTypeBlastx;
	else if (strcasecmp("tblastn", program) == 0)
		*number = eBlastTypeTblastn;
	else if (strcasecmp("tblastx", program) == 0)
		*number = eBlastTypeTblastx;
	else if (strcasecmp("rpsblast", program) == 0)
		*number = eBlastTypeRpsBlast;
	else if (strcasecmp("rpstblastn", program) == 0)
		*number = eBlastTypeRpsTblastn;

	return 0;
}

Int2 BlastNumber2Program(Uint1 number, char* *program)
{

	if (program == NULL)
		return 1;

	switch (number) {
		case eBlastTypeBlastn:
			*program = strdup("blastn");
			break;
		case eBlastTypeBlastp:
			*program = strdup("blastp");
			break;
		case eBlastTypeBlastx:
			*program = strdup("blastx");
			break;
		case eBlastTypeTblastn:
			*program = strdup("tblastn");
			break;
		case eBlastTypeTblastx:
			*program = strdup("tblastx");
			break;
		case eBlastTypeRpsBlast:
			*program = strdup("rpsblast");
			break;
		case eBlastTypeRpsTblastn:
			*program = strdup("rpstblastn");
			break;
		default:
			*program = strdup("unknown");
			break;
	}

	return 0;
}

#define X_STDAA 21
/** Translate 3 nucleotides into an amino acid
 * MUST have 'X' as unknown amino acid
 * @param codon 3 values in ncbi4na code
 * @param codes Geneic code string to use (must be in ncbistdaa encoding!)
 * @return Amino acid in ncbistdaa
 */
static Uint1 CodonToAA (Uint1* codon, const Uint1* codes)
{
   register Uint1 aa = 0, taa;
   register int i, j, k, index0, index1, index2;
   static Uint1 mapping[4] = { 8,     /* T in ncbi4na */
                               2,     /* C */
                               1,     /* A */
                               4 };   /* G */

   for (i = 0; i < 4; i++) {
      if (codon[0] & mapping[i]) {
         index0 = i * 16;
         for (j = 0; j < 4; j++) {
            if (codon[1] & mapping[j]) {
               index1 = index0 + (j * 4);
               for (k = 0; k < 4; k++) {
                  if (codon[2] & mapping[k]) {
                     index2 = index1 + k;
                     taa = codes[index2];
                     if (! aa)
                        aa = taa;
                     else {
                        if (taa != aa) {
                           aa = X_STDAA;
                           break;
                        }
                     }
                  }
                  if (aa == X_STDAA)
                     break;
               }
            }
            if (aa == X_STDAA)
               break;
         }
      }
      if (aa == X_STDAA)
         break;
   }
   return aa;
}

Int4
BLAST_GetTranslation(const Uint1* query_seq, const Uint1* query_seq_rev, 
   Int4 nt_length, Int2 frame, Uint1* prot_seq, const Uint1* genetic_code)
{
	Uint1 codon[CODON_LENGTH];
	Int4 index, index_prot;
	Uint1 residue;
   Uint1* nucl_seq;

   nucl_seq = (frame >= 0 ? (Uint1 *)query_seq : (Uint1 *)(query_seq_rev+1));

	/* The first character in the protein is the NULLB sentinel. */
	prot_seq[0] = NULLB;
	index_prot = 1;
	for (index=ABS(frame)-1; index<nt_length-2; index += CODON_LENGTH)
	{
		codon[0] = nucl_seq[index];
		codon[1] = nucl_seq[index+1];
		codon[2] = nucl_seq[index+2];
		residue = CodonToAA(codon, genetic_code);
		if (IS_residue(residue))
		{
			prot_seq[index_prot] = residue;
			index_prot++;
		}
	}
	prot_seq[index_prot] = NULLB;
	
	return index_prot - 1;
}


/*
  Translate a compressed nucleotide sequence without ambiguity codes.
*/
Int4
BLAST_TranslateCompressedSequence(Uint1* translation, Int4 length, 
   const Uint1* nt_seq, Int2 frame, Uint1* prot_seq)
{
   int state;
   Int2 total_remainder;
   Int4 prot_length;
   int byte_value, codon=-1;
   Uint1 last_remainder, last_byte, remainder;
   Uint1* nt_seq_end,* nt_seq_start;
   Uint1* prot_seq_start;
   int byte_value1,byte_value2,byte_value3,byte_value4,byte_value5;
   
   prot_length=0;
   if (nt_seq == NULL || prot_seq == NULL || 
       (length-ABS(frame)+1) < CODON_LENGTH)
      return prot_length;
   
   *prot_seq = NULLB;
   prot_seq++;
   
   /* record to determine protein length. */
   prot_seq_start = prot_seq;
  
   remainder = length%4;

   if (frame > 0) {
      nt_seq_end = (Uint1 *) (nt_seq + (length)/4 - 1);
      last_remainder = (4*(length/4) - frame + 1)%CODON_LENGTH;
      total_remainder = last_remainder+remainder;
      
      state = frame-1;
      byte_value = *nt_seq;
      
      /* If there's lots to do, advance to state 0, then enter fast loop */
      while (nt_seq < nt_seq_end) {
         switch (state)	{
         case 0:
            codon = (byte_value >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
            /* do state = 3 now, break is NOT missing. */
         case 3:
            codon = ((byte_value & 3) << 4);
            nt_seq++;
            byte_value = *nt_seq;	
            codon += (byte_value >> 4);
            *prot_seq = translation[codon];
            prot_seq++;
            if (nt_seq >= nt_seq_end) {
               state = 2;
               break;
            }
            /* Go on to state = 2 if not at end. */
         case 2:
            codon = ((byte_value & 15) << 2);
            nt_seq++;
            byte_value = *nt_seq;	
            codon += (byte_value >> 6);
            *prot_seq = translation[codon];
            prot_seq++;
            if (nt_seq >= nt_seq_end) {
               state = 1;
               break;
            }
            /* Go on to state = 1 if not at end. */
         case 1:
            codon = byte_value & 63;
            *prot_seq = translation[codon];
            prot_seq++;
            nt_seq++;
            byte_value = *nt_seq;	
            state = 0;
            break;
         } /* end switch */
         /* switch ends at state 0, except when at end */
         
         /********************************************/
         /* optimized loop: start in state 0. continue til near end */
         while (nt_seq < (nt_seq_end-10)) {
            byte_value1 = *(++nt_seq);
            byte_value2 = *(++nt_seq);
            byte_value3 = *(++nt_seq);
            /* case 0: */
            codon = (byte_value >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
            
            /* case 3: */
            codon = ((byte_value & 3) << 4);
            codon += (byte_value1 >> 4);
            *prot_seq = translation[codon];
            prot_seq++;
            
            byte_value4 = *(++nt_seq);
            /* case 2: */
            codon = ((byte_value1 & 15) << 2);
            
            codon += (byte_value2 >> 6);
            *prot_seq = translation[codon];
            prot_seq++;
            /* case 1: */
            codon = byte_value2 & 63;
            byte_value5 = *(++nt_seq);
            *prot_seq = translation[codon];
            prot_seq++;
            
            /* case 0: */
            codon = (byte_value3 >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
            /* case 3: */
            byte_value = *(++nt_seq);
            codon = ((byte_value3 & 3) << 4);
            codon += (byte_value4 >> 4);
            *prot_seq = translation[codon];
            prot_seq++;
            /* case 2: */
            codon = ((byte_value4 & 15) << 2);
            codon += (byte_value5 >> 6);
            *prot_seq = translation[codon];
            prot_seq++;
            /* case 1: */
            codon = byte_value5 & 63;
            *prot_seq = translation[codon];
            prot_seq++;
            state=0;
         } /* end optimized while */
         /********************************************/
      } /* end while */
      
      if (state == 1) { 
         /* This doesn't get done above, DON't do the state = 0
            below if this is done. */
         byte_value = *nt_seq;
         codon = byte_value & 63;
         state = 0;
         *prot_seq = translation[codon];
         prot_seq++;
      } else if (state == 0) { /* This one doesn't get done above. */
         byte_value = *nt_seq;
         codon = ((byte_value) >> 2);
         state = 3;
         *prot_seq = translation[codon];
         prot_seq++;
      }

      if (total_remainder >= CODON_LENGTH) {
         byte_value = *(nt_seq_end);
         last_byte = *(nt_seq_end+1);
         if (state == 0) {
            codon = (last_byte >> 2);
         } else if (state == 2) {
            codon = ((byte_value & 15) << 2);
            codon += (last_byte >> 6);
         } else if (state == 3)	{
            codon = ((byte_value & 3) << 4);
            codon += (last_byte >> 4);
         }
         *prot_seq = translation[codon];
         prot_seq++;
      }
   } else {
      nt_seq_start = (Uint1 *) nt_seq;
      nt_seq += length/4;
      state = remainder+frame;
      /* Do we start in the last byte?  This one has the lowest order
         bits set to represent the remainder, hence the odd coding here. */
      if (state >= 0) {
         last_byte = *nt_seq;
         nt_seq--;
         if (state == 0) {
            codon = (last_byte >> 6);
            byte_value = *nt_seq;
            codon += ((byte_value & 15) << 2);
            state = 1;
         } else if (state == 1) {
            codon = (last_byte >> 4);
            byte_value = *nt_seq;
            codon += ((byte_value & 3) << 4);
            state = 2;
         } else if (state == 2)	{
            codon = (last_byte >> 2);
            state = 3;
         }
         *prot_seq = translation[codon];
         prot_seq++;
      }	else {
         state = 3 + (remainder + frame + 1);
         nt_seq--;
      }
      
      byte_value = *nt_seq;	

      /* If there's lots to do, advance to state 3, then enter fast loop */
      while (nt_seq > nt_seq_start) {
         switch (state) {
         case 3:
            codon = (byte_value & 63);
            *prot_seq = translation[codon];
            prot_seq++;
            /* do state = 0 now, break is NOT missing. */
         case 0:
            codon = (byte_value >> 6);
            nt_seq--;
            byte_value = *nt_seq;	
            codon += ((byte_value & 15) << 2);
            *prot_seq = translation[codon];
            prot_seq++;
            if (nt_seq <= nt_seq_start)	{
               state = 1;
               break;
            }
            /* Go on to state = 2 if not at end. */
         case 1:
            codon = (byte_value >> 4);
            nt_seq--;
            byte_value = *nt_seq;
            codon += ((byte_value & 3) << 4);
            *prot_seq = translation[codon];
            prot_seq++;
            if (nt_seq <= nt_seq_start)	{
               state = 2;
               break;
            }
            /* Go on to state = 2 if not at end. */
         case 2:
            codon = (byte_value >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
            nt_seq--;
            byte_value = *nt_seq;	
            state = 3;
            break;
         } /* end switch */
         /* switch ends at state 3, except when at end */
         
         /********************************************/
         /* optimized area: start in state 0. continue til near end */
         while (nt_seq > (nt_seq_start+10)) {
            byte_value1 = *(--nt_seq);	
            byte_value2 = *(--nt_seq);
            byte_value3 = *(--nt_seq);
            
            codon = (byte_value & 63);
            *prot_seq = translation[codon];
            prot_seq++;
            codon = (byte_value >> 6);
            codon += ((byte_value1 & 15) << 2);
            *prot_seq = translation[codon];
            prot_seq++;
            byte_value4 = *(--nt_seq);
            codon = (byte_value1 >> 4);
            codon += ((byte_value2 & 3) << 4);
            *prot_seq = translation[codon];
            prot_seq++;
            codon = (byte_value2 >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
            byte_value5 = *(--nt_seq);
            
            codon = (byte_value3 & 63);
            *prot_seq = translation[codon];
            prot_seq++;
            byte_value = *(--nt_seq);
            codon = (byte_value3 >> 6);
            codon += ((byte_value4 & 15) << 2);
            *prot_seq = translation[codon];
            prot_seq++;
            codon = (byte_value4 >> 4);
            codon += ((byte_value5 & 3) << 4);
            *prot_seq = translation[codon];
            prot_seq++;
            codon = (byte_value5 >> 2);
            *prot_seq = translation[codon];
            prot_seq++;
         } /* end optimized while */
         /********************************************/
         
      } /* end while */
      
      byte_value = *nt_seq;
      if (state == 3) {
         codon = (byte_value & 63);
         *prot_seq = translation[codon];
         prot_seq++;
      } else if (state == 2) {
         codon = (byte_value >> 2);
         *prot_seq = translation[codon];
         prot_seq++;
      }
   }

   *prot_seq = NULLB;
   
   return (prot_seq - prot_seq_start);
} /* BlastTranslateUnambiguousSequence */


/* Reverse a nucleotide sequence in the ncbi4na encoding */
Int2 GetReverseNuclSequence(const Uint1* sequence, Int4 length, 
                            Uint1** rev_sequence_ptr)
{
   Uint1* rev_sequence;
   Int4 index;
   /* Conversion table from forward to reverse strand residue in the blastna 
      encoding */
   Uint1 conversion_table[17] = 
      { 0, 8, 4, 12, 2, 10, 9, 14, 1, 6, 5, 13, 3, 11, 7, 15 };

   if (!rev_sequence_ptr)
      return -1;

   rev_sequence = (Uint1*) malloc(length + 2);
   
   rev_sequence[0] = rev_sequence[length+1] = NULLB;

   for (index = 0; index < length; ++index) {
      rev_sequence[length-index] = conversion_table[sequence[index]];
   }

   *rev_sequence_ptr = rev_sequence;
   return 0;
}

Int2 BLAST_ContextToFrame(Uint1 prog_number, Int4 context_number)
{
   Int2 frame=255;

   if (prog_number == eBlastTypeBlastn) {
      if (context_number % 2 == 0)
         frame = 1;
      else
         frame = -1;
   } else if (prog_number == eBlastTypeBlastp ||
              prog_number == eBlastTypeRpsBlast ||
              prog_number == eBlastTypeTblastn ||
              prog_number == eBlastTypeRpsTblastn) { 
      /* Query and subject are protein, no frame. */
      frame = 0;
   } else if (prog_number == eBlastTypeBlastx || 
              prog_number == eBlastTypeTblastx) {
      context_number = context_number % 6;
      frame = (context_number < 3) ? context_number+1 : -context_number+2;
   }
   
   return frame;
}

Int4 
Blast_GetQueryIndexFromContext(Int4 context, Uint1 program)
{
   Int4 index = 0;
   switch (program) {
   case eBlastTypeBlastn:
      index = context/NUM_STRANDS; break;
   case eBlastTypeBlastp: case eBlastTypeTblastn: 
   case eBlastTypeRpsBlast: case eBlastTypeRpsTblastn:
      index = context; break;
   case eBlastTypeBlastx: case eBlastTypeTblastx:
      index = context/NUM_FRAMES; break;
   default:
      break;
   }
   return index;
}

Int4 BLAST_GetQueryLength(const BlastQueryInfo* query_info, Int4 context)
{
   return query_info->context_offsets[context+1] -
      query_info->context_offsets[context] - 1;
}

BlastQueryInfo* BlastQueryInfoFree(BlastQueryInfo* query_info)
{
   sfree(query_info->context_offsets);
   sfree(query_info->length_adjustments);
   sfree(query_info->eff_searchsp_array);
   sfree(query_info);
   return NULL;
}

BlastQueryInfo* BlastQueryInfoDup(BlastQueryInfo* query_info)
{
   BlastQueryInfo* retval = BlastMemDup(query_info, sizeof(BlastQueryInfo));
   Int4 num_contexts = query_info->last_context + 1;

   retval->context_offsets = 
      BlastMemDup(query_info->context_offsets, (num_contexts+1)*sizeof(Int4));
   retval->length_adjustments = 
      BlastMemDup(query_info->length_adjustments, num_contexts*sizeof(Int4));
   retval->eff_searchsp_array = 
      BlastMemDup(query_info->eff_searchsp_array, num_contexts*sizeof(Int8));
   return retval;
}

/** Convert a sequence in ncbi4na or blastna encoding into a packed sequence
 * in ncbi2na encoding. Needed for 2 sequences BLASTn comparison.
 */
Int2 BLAST_PackDNA(Uint1* buffer, Int4 length, Uint1 encoding, 
                          Uint1** packed_seq)
{
   Int4 new_length = length/COMPRESSION_RATIO + 1;
   Uint1* new_buffer = (Uint1*) malloc(new_length);
   Int4 index, new_index;
   Uint1 shift;     /* bit shift to pack bases */

   for (index=0, new_index=0; new_index < new_length-1; 
        ++new_index, index += COMPRESSION_RATIO) {
      if (encoding == BLASTNA_ENCODING)
         new_buffer[new_index] = 
            ((buffer[index]&NCBI2NA_MASK)<<6) | 
            ((buffer[index+1]&NCBI2NA_MASK)<<4) |
            ((buffer[index+2]&NCBI2NA_MASK)<<2) | 
             (buffer[index+3]&NCBI2NA_MASK);
      else
         new_buffer[new_index] = 
            ((NCBI4NA_TO_BLASTNA[buffer[index]]&NCBI2NA_MASK)<<6) | 
            ((NCBI4NA_TO_BLASTNA[buffer[index+1]]&NCBI2NA_MASK)<<4) |
            ((NCBI4NA_TO_BLASTNA[buffer[index+2]]&NCBI2NA_MASK)<<2) | 
            (NCBI4NA_TO_BLASTNA[buffer[index+3]]&NCBI2NA_MASK);
   }

   /* Handle the last byte of the compressed sequence.
      Last 2 bits of the last byte tell the number of valid 
      packed sequence bases in it. */
   new_buffer[new_index] = length % COMPRESSION_RATIO;

   for (; index < length; index++) {
      switch (index%COMPRESSION_RATIO) {
      case 0: shift = 6; break;
      case 1: shift = 4; break;
      case 2: shift = 2; break;
      default: abort();     /* should never happen */
      }
      if (encoding == BLASTNA_ENCODING)
         new_buffer[new_index] |= ((buffer[index]&NCBI2NA_MASK)<<shift);
      else
         new_buffer[new_index] |=
            ((NCBI4NA_TO_BLASTNA[buffer[index]]&NCBI2NA_MASK)<<shift);
   }

   *packed_seq = new_buffer;

   return 0;
}

Int2 BLAST_InitDNAPSequence(BLAST_SequenceBlk* query_blk, 
                            BlastQueryInfo* query_info)
{
   Uint1* buffer,* seq,* tmp_seq;
   Int4 total_length, index, offset, i, context;
   Int4 length[CODON_LENGTH];
   Int4* context_offsets = query_info->context_offsets;

   total_length = context_offsets[query_info->last_context+1] + 1;

   buffer = (Uint1*) malloc(total_length);

   for (index = 0; index <= query_info->last_context; index += CODON_LENGTH) {
      seq = &buffer[context_offsets[index]];

      for (i = 0; i < CODON_LENGTH; ++i) {
         *seq++ = NULLB;
         length[i] = BLAST_GetQueryLength(query_info, index + i);
      }

      for (i = 0; ; ++i) {
         context = i % 3;
         offset = i / 3;
         if (offset >= length[context]) {
            /* Once one frame is past its end, we are done */
            break;
         }
         tmp_seq = &query_blk->sequence[context_offsets[index+context]];
         *seq++ = tmp_seq[offset];
      }
   }

   /* The mixed-frame protein sequence buffer will be saved in 
      'sequence_start' */
   query_blk->oof_sequence = buffer;
   query_blk->oof_sequence_allocated = TRUE;

   return 0;
}

/*	Gets the translation array for a given genetic code.  
 *	This array is optimized for the NCBI2na alphabet.
 * The reverse complement can also be spcified.
 * @param genetic_code Genetic code string in ncbistdaa encoding [in]
 * @param reverse_complement Get translation table for the reverse strand? [in]
 * @return The translation table.
*/
static Uint1*
BLAST_GetTranslationTable(const Uint1* genetic_code, Boolean reverse_complement)

{
	Int2 index1, index2, index3, bp1, bp2, bp3;
	Int2 codon;
	Uint1* translation;
   /* The next array translate between the ncbi2na rep's and 
      the rep's used by the genetic_code tables.  The rep used by the 
      genetic code arrays is in mapping: T=0, C=1, A=2, G=3 */
  	static Uint1 mapping[4] = {2, /* A in ncbi2na */
                              1, /* C in ncbi2na. */
                              3, /* G in ncbi2na. */
                              0 /* T in ncbi2na. */ };

	if (genetic_code == NULL)
		return NULL;

	translation = calloc(64, sizeof(Uint1));
	if (translation == NULL)
		return NULL;

	for (index1=0; index1<4; index1++)
	{
		for (index2=0; index2<4; index2++)
		{
			for (index3=0; index3<4; index3++)
			{
            /* The reverse complement codon is saved in it's orginal 
               (non-complement) form AND with the high-order bits reversed 
               from the non-complement form, as this is how they appear in 
               the sequence. 
            */
			   if (reverse_complement)
            {
               bp1 = 3 - index1;
               bp2 = 3 - index2;
               bp3 = 3 - index3;
			   	codon = (mapping[bp1]<<4) + (mapping[bp2]<<2) + (mapping[bp3]);
			   	translation[(index3<<4) + (index2<<2) + index1] = 
                  genetic_code[codon];
			   }
			   else
			   {
			   	codon = (mapping[index1]<<4) + (mapping[index2]<<2) + 
                  (mapping[index3]);
			   	translation[(index1<<4) + (index2<<2) + index3] = 
                  genetic_code[codon];
			   }
			}
		}
	}
	return translation;
}


Int2 BLAST_GetAllTranslations(const Uint1* nucl_seq, Uint1 encoding,
        Int4 nucl_length, const Uint1* genetic_code,
        Uint1** translation_buffer_ptr, Int4** frame_offsets_ptr,
        Uint1** mixed_seq_ptr)
{
   Uint1* translation_buffer,* mixed_seq;
   Uint1* translation_table = NULL,* translation_table_rc;
   Uint1* nucl_seq_rev;
   Int4 offset = 0, length;
   Int4 context; 
   Int4* frame_offsets;
   Int2 frame;
   
   if (encoding != NCBI2NA_ENCODING && encoding != NCBI4NA_ENCODING)
      return -1;

   if ((translation_buffer = 
        (Uint1*) malloc(2*(nucl_length+1)+1)) == NULL)
      return -1;

   if (encoding == NCBI4NA_ENCODING) {
      /* First produce the reverse strand of the nucleotide sequence */
      GetReverseNuclSequence(nucl_seq, nucl_length, 
                             &nucl_seq_rev);
   } else {
      translation_table = BLAST_GetTranslationTable(genetic_code, FALSE);
      translation_table_rc = BLAST_GetTranslationTable(genetic_code, TRUE);
   } 

   frame_offsets = (Int4*) malloc((NUM_FRAMES+1)*sizeof(Int4));

   frame_offsets[0] = 0;
   
   for (context = 0; context < NUM_FRAMES; ++context) {
      frame = BLAST_ContextToFrame(eBlastTypeBlastx, context);
      if (encoding == NCBI2NA_ENCODING) {
         if (frame > 0) {
            length = 
               BLAST_TranslateCompressedSequence(translation_table,
                  nucl_length, nucl_seq, frame, translation_buffer+offset);
         } else {
            length = 
               BLAST_TranslateCompressedSequence(translation_table_rc,
                  nucl_length, nucl_seq, frame, translation_buffer+offset);
         }
      } else {
         length = 
            BLAST_GetTranslation(nucl_seq, nucl_seq_rev, 
               nucl_length, frame, translation_buffer+offset, genetic_code);
      }

      /* Increment offset by 1 extra byte for the sentinel NULLB 
         between frames. */
      offset += length + 1;
      frame_offsets[context+1] = offset;
   }

   if (encoding == NCBI4NA_ENCODING) {
      sfree(nucl_seq_rev);
   } else { 
      free(translation_table);
      sfree(translation_table_rc);
   }

   /* All frames are ready. For the out-of-frame gapping option, allocate 
      and fill buffer with the mixed frame sequence */
   if (mixed_seq_ptr) {
      Uint1* seq;
      Int4 index, i;

      *mixed_seq_ptr = mixed_seq = (Uint1*) malloc(2*(nucl_length+1));
      seq = mixed_seq;
      for (index = 0; index < NUM_FRAMES; index += CODON_LENGTH) {
         for (i = 0; i <= nucl_length; ++i) {
            context = i % CODON_LENGTH;
            offset = i / CODON_LENGTH;
            *seq++ = translation_buffer[frame_offsets[index+context]+offset];
         }
      }
   }
   if (translation_buffer_ptr)
      *translation_buffer_ptr = translation_buffer;
   else
      sfree(translation_buffer);

   if (frame_offsets_ptr)
      *frame_offsets_ptr = frame_offsets;
   else
      sfree(frame_offsets);

   return 0;
}

int GetPartialTranslation(const Uint1* nucl_seq,
        Int4 nucl_length, Int2 frame, const Uint1* genetic_code,
        Uint1** translation_buffer_ptr, Int4* protein_length, 
        Uint1** mixed_seq_ptr)
{
   Uint1* translation_buffer;
   Uint1* nucl_seq_rev = NULL;
   Int4 length;
   
   if ((translation_buffer = 
        (Uint1*) malloc(2*(nucl_length+1)+1)) == NULL)
      return -1;
   if (translation_buffer_ptr)
      *translation_buffer_ptr = translation_buffer;

   if (frame < 0) {
      /* First produce the reverse strand of the nucleotide sequence */
      GetReverseNuclSequence(nucl_seq, nucl_length, &nucl_seq_rev);
   } 

   if (!mixed_seq_ptr) {
      length = 
         BLAST_GetTranslation(nucl_seq, nucl_seq_rev, 
            nucl_length, frame, translation_buffer, genetic_code);
      if (protein_length)
         *protein_length = length;
   } else {
      Int2 index;
      Int2 frame_sign = ((frame < 0) ? -1 : 1);
      Int4 offset = 0;
      Int4 frame_offsets[3];
      Uint1* seq;

      for (index = 1; index <= 3; ++index) {
         length = 
            BLAST_GetTranslation(nucl_seq, nucl_seq_rev, 
               nucl_length, (short)(frame_sign*index), translation_buffer+offset, 
               genetic_code);
         frame_offsets[index-1] = offset;
         offset += length + 1;
      }

      *mixed_seq_ptr = (Uint1*) malloc(2*(nucl_length+1));
      if (protein_length)
         *protein_length = 2*nucl_length - 1;
      for (index = 0, seq = *mixed_seq_ptr; index <= nucl_length; 
           ++index, ++seq) {
         *seq = translation_buffer[frame_offsets[index%3]+(index/3)];
      }
   }

   sfree(nucl_seq_rev);
   if (!translation_buffer_ptr)
      sfree(translation_buffer);

   return 0;
}


Int4 FrameToContext(Int2 frame) 
{
   if (frame > 0)
      return frame - 1;
   else
      return 2 - frame;
}

Int4 BSearchInt4(Int4 n, Int4* A, Int4 size)
{
    Int4 m, b, e;

    b = 0;
    e = size;
    while (b < e - 1) {
	m = (b + e) / 2;
	if (A[m] > n)
	    e = m;
	else
	    b = m;
    }
    return b;
}
