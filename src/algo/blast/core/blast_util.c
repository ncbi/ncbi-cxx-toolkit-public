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

File name: blast_util.c

Author: Ilya Dondoshansky

Contents: Various BLAST utilities

******************************************************************************
 * $Revision$
 * */

#include <blast_def.h>
#include <blast_util.h>

static char const rcsid[] = "$Id$";

extern OIDListPtr LIBCALL 
BlastGetVirtualOIDList PROTO((ReadDBFILEPtr rdfp_chain));

Int2
BlastSetUp_SeqBlkNew (const Uint1Ptr buffer, Int4 length, Int2 context,
	const Int4Ptr frame, BLAST_SequenceBlkPtr *seq_blk, 
        Boolean buffer_allocated)
{
   /* Check if BLAST_SequenceBlk itself needs to be allocated here or not */
   if (*seq_blk == NULL) {
      *seq_blk = MemNew(sizeof(BLAST_SequenceBlk));
   }

   if (buffer_allocated) {
      (*seq_blk)->sequence_start_allocated = TRUE;
      (*seq_blk)->sequence_start = buffer;
      /* The first byte is a sentinel byte. */
      (*seq_blk)->sequence = (*seq_blk)->sequence_start+1;
   } else {
      (*seq_blk)->sequence = buffer;
      (*seq_blk)->sequence_start = NULL;
   }
   
   (*seq_blk)->length = length;
   (*seq_blk)->context = context;
   if (frame)
      (*seq_blk)->frame = frame[context];
   
   return 0;
}

/** Create the subject sequence block given an ordinal id in a database */
void
MakeBlastSequenceBlk(ReadDBFILEPtr db, BLAST_SequenceBlkPtr PNTR seq_blk,
                     Int4 oid, Uint1 encoding)
{
  Int4 length, buf_len = 0;
  Uint1Ptr buffer = NULL;

  if (encoding == BLASTNA_ENCODING) {
     length = readdb_get_sequence_ex(db, oid, &buffer, &buf_len, TRUE);
  } else if (encoding == NCBI4NA_ENCODING) {
     length = readdb_get_sequence_ex(db, oid, &buffer, &buf_len, FALSE);
  } else {
     length=readdb_get_sequence(db, oid, &buffer);
  }

  BlastSetUp_SeqBlkNew(buffer, length, 0, NULL, seq_blk, 
                       (encoding != BLASTP_ENCODING));
  (*seq_blk)->oid = oid;
}

Int2 BlastSequenceBlkClean(BLAST_SequenceBlkPtr seq_blk)
{
   if (!seq_blk)
      return 1;

   if (seq_blk->sequence_allocated) 
      seq_blk->sequence = MemFree(seq_blk->sequence);
   if (seq_blk->sequence_start_allocated)
      seq_blk->sequence_start = MemFree(seq_blk->sequence_start);

   return 0;
}

BLAST_SequenceBlkPtr BlastSequenceBlkFree(BLAST_SequenceBlkPtr seq_blk)
{
   if (!seq_blk)
      return NULL;

   BlastSequenceBlkClean(seq_blk);

   return (BLAST_SequenceBlkPtr) MemFree(seq_blk);
}

Int2 BlastProgram2Number(const Char *program, Uint1 *number)
{
	*number = blast_type_undefined;
	if (program == NULL)
		return 1;

	if (StringICmp("blastn", program) == 0)
		*number = blast_type_blastn;
	else if (StringICmp("blastp", program) == 0)
		*number = blast_type_blastp;
	else if (StringICmp("blastx", program) == 0)
		*number = blast_type_blastx;
	else if (StringICmp("tblastn", program) == 0)
		*number = blast_type_tblastn;
	else if (StringICmp("tblastx", program) == 0)
		*number = blast_type_tblastx;
	else if (StringICmp("psitblastn", program) == 0)
		*number = blast_type_psitblastn;

	return 0;
}

Int2 BlastNumber2Program(Uint1 number, CharPtr *program)
{

	if (program == NULL)
		return 1;

	switch (number) {
		case blast_type_blastn:
			*program = StringSave("blastn");
			break;
		case blast_type_blastp:
			*program = StringSave("blastp");
			break;
		case blast_type_blastx:
			*program = StringSave("blastx");
			break;
		case blast_type_tblastn:
			*program = StringSave("tblastn");
			break;
		case blast_type_tblastx:
			*program = StringSave("tblastx");
			break;
		case blast_type_psitblastn:
			*program = StringSave("psitblastn");
			break;
		default:
			*program = StringSave("unknown");
			break;
	}

	return 0;
}

Uint1Ptr LIBCALL
BLAST_GetTranslation(Uint1Ptr query_seq, Uint1Ptr query_seq_rev, 
   Int4 nt_length, Int2 frame, Int4Ptr length, CharPtr genetic_code)
{
	Uint1 codon[CODON_LENGTH];
	Int4 index, index_prot;
	SeqMapTablePtr smtp;
	Uint1 residue, new_residue;
        Uint1Ptr nucl_seq;
	Uint1Ptr prot_seq;

	smtp = SeqMapTableFind(Seq_code_ncbistdaa, Seq_code_ncbieaa);

        nucl_seq = (frame >= 0 ? query_seq : query_seq_rev);

	/* Allocate two extra spaces for NULLB's at beginning and end of 
           sequence */
	prot_seq = (Uint1Ptr) 
           MemNew((2+(nt_length+2)/CODON_LENGTH)*sizeof(Uint1));

	/* The first character in the protein is the NULLB sentinel. */
	prot_seq[0] = NULLB;
	index_prot = 1;
	for (index=ABS(frame)-1; index<nt_length-2; index += CODON_LENGTH)
	{
		codon[0] = nucl_seq[index];
		codon[1] = nucl_seq[index+1];
		codon[2] = nucl_seq[index+2];
		residue = AAForCodon(codon, genetic_code);
		new_residue = SeqMapTableConvert(smtp, residue);
		if (IS_residue(new_residue))
		{
			prot_seq[index_prot] = new_residue;
			index_prot++;
		}
	}
	prot_seq[index_prot] = NULLB;
	*length = index_prot-1;
	
	return prot_seq;
}

BioseqPtr
BLAST_MakeTempProteinBioseq (Uint1Ptr sequence, Int4 length, Uint1 alphabet)
{
    BioseqPtr bsp;
    Int4 byte_store_length;
    Nlm_ByteStorePtr byte_store;
    ObjectIdPtr oip;

    if (sequence == NULL || length == 0)
        return NULL;
    
    byte_store = Nlm_BSNew(length);
    
    byte_store_length = Nlm_BSWrite(byte_store, (VoidPtr) sequence, length);
    if (length != byte_store_length) {
        Nlm_BSDelete(byte_store, length);
        return NULL;
    }
    
    bsp = BioseqNew();
    bsp->seq_data = byte_store;
    bsp->length = length;
    bsp->seq_data_type = alphabet;
    bsp->mol = Seq_mol_aa;
    bsp->repr = Seq_repr_raw;
    
    oip = UniqueLocalId();
    ValNodeAddPointer(&(bsp->id), SEQID_LOCAL, oip);
    SeqMgrAddToBioseqIndex(bsp);
    
    return bsp;
}


Int4 MakeBlastSequenceBlkFromGI(ReadDBFILEPtr db, Int4 gi, BLAST_SequenceBlkPtr seq)
{
  Int4 oid;
  Int4 length;
  
  oid=readdb_gi2seq(db, gi, NULL);
  
  length=readdb_get_sequence(db, oid, &(seq->sequence) );
  
  seq->sequence_start = NULL;
  seq->length = length;
  seq->frame = 0;
  return 0;
}

Int4 MakeBlastSequenceBlkFromOID(ReadDBFILEPtr db, Int4 oid, BLAST_SequenceBlkPtr seq)
{
  Int4 length;
  
  length=readdb_get_sequence(db, oid, &(seq->sequence) );
  
  seq->sequence_start = NULL;
  seq->length = length;
  seq->frame = 0;
  seq->oid = oid;
  return 0;
}

Int4 MakeBlastSequenceBlkFromFasta(FILE *fasta_fp, BLAST_SequenceBlkPtr seq)
{
   BioseqPtr query_bsp;
   SeqEntryPtr query_sep;
   Boolean is_na = FALSE;
   Boolean believe_defline = TRUE;
   SeqLocPtr mask_slp = NULL;
   Int2 ctr=1;
   
   Uint1Ptr sequence = NULL;
   
   query_sep=FastaToSeqEntryForDb(fasta_fp,
                               is_na, /* query is nucleotide? */
                               NULL, /* error message */
                               believe_defline, /* believe query defline? */
                               "", /* prefix for localid if not parseable */
                               &ctr, /* starting point for constructing a unique id */
                               &mask_slp);

   if (query_sep == NULL)
      return 1;
   
   SeqEntryExplore(query_sep, &query_bsp, FindProt);
   
   if (query_bsp == NULL)
      return 1;
   
   /* allocate contiguous space for the sequence */
   sequence = Malloc(query_bsp->length);
   
   if (sequence == NULL)
      return 1;
   
   /* convert to ncbistdaa encoding */
   BioseqRawConvert(query_bsp, Seq_code_ncbistdaa);
   
   /* read the sequence */
   BSSeek(query_bsp->seq_data, 0, SEEK_SET);
   BSRead(query_bsp->seq_data, sequence, query_bsp->length);
   
   seq->length = query_bsp->length;
   seq->sequence = sequence;
   return 0;
}

/*
  Translate a compressed nucleotide sequence without ambiguity codes.
*/
Int4 LIBCALL
BLAST_TranslateCompressedSequence(Uint1Ptr translation, Int4 length, 
   Uint1Ptr nt_seq, Int2 frame, Uint1Ptr prot_seq)
{
   int state;
   Int2 total_remainder;
   Int4 prot_length;
   int byte_value, codon=-1;
   Uint1 last_remainder, last_byte, remainder;
   Uint1Ptr nt_seq_end, nt_seq_start;
   Uint1Ptr prot_seq_start;
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
      nt_seq_end = nt_seq + (length)/4 - 1;
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
      *prot_seq = NULLB;
   } else {
      nt_seq_start = nt_seq;
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


/** Reverse a nucleotide sequence in the ncbi4na encoding */
Int2 GetReverseNuclSequence(Uint1Ptr sequence, Int4 length, 
                            Uint1Ptr PNTR rev_sequence_ptr)
{
   Uint1Ptr rev_sequence;
   Int4 index;
   /* Conversion table from forward to reverse strand residue in the blastna 
      encoding */
   Uint1 conversion_table[17] = 
      { 0, 8, 4, 12, 2, 10, 9, 14, 1, 6, 5, 13, 3, 11, 7, 15 };

   if (!rev_sequence_ptr)
      return -1;

   rev_sequence = (Uint1Ptr) Malloc(length + 2);
   
   rev_sequence[0] = rev_sequence[length+1] = NULLB;

   for (index = 0; index < length; ++index) {
      rev_sequence[length-index] = conversion_table[sequence[index]];
   }

   *rev_sequence_ptr = rev_sequence + 1;
   return 0;
}

Boolean 
BLAST_GetDbChunk(ReadDBFILEPtr rdfp, Int4Ptr start, Int4Ptr stop, 
   Int4Ptr id_list, Int4Ptr id_list_number, BlastThrInfoPtr thr_info)
{
    Boolean done=FALSE;
    OIDListPtr virtual_oidlist = NULL;
    *id_list_number = 0;
    
    NlmMutexLockEx(&thr_info->db_mutex);
    if (thr_info->realdb_done) {
        if ((virtual_oidlist = BlastGetVirtualOIDList(rdfp))) {
           /* Virtual database.   Create id_list using mask file */
           Int4 gi_end       = 0;
	    
           thr_info->final_db_seq = 
              MIN(thr_info->final_db_seq, virtual_oidlist->total);
	    
           gi_end = thr_info->final_db_seq;

           if (thr_info->oid_current < gi_end) {
              Int4 oidindex  = 0;
              Int4 gi_start  = thr_info->oid_current;
              Int4 bit_start = gi_start % MASK_WORD_SIZE;
              Int4 gi;
              
              for(gi = gi_start; (gi < gi_end) && 
                     (oidindex < thr_info->db_chunk_size);) {
                 Int4 bit_end = ((gi_end - gi) < MASK_WORD_SIZE) ? 
                    (gi_end - gi) : MASK_WORD_SIZE;
                 Int4 bit;
                 
                 Uint4 mask_index = gi / MASK_WORD_SIZE;
                 Uint4 mask_word  = 
                    Nlm_SwapUint4(virtual_oidlist->list[mask_index]);
                 
                 if ( mask_word ) {
                    for(bit = bit_start; bit<bit_end; bit++) {
                       Uint4 bitshift = (MASK_WORD_SIZE-1)-bit;
                       
                       if ((mask_word >> bitshift) & 1) {
                          id_list[ oidindex++ ] = (gi - bit_start) + bit;
                       }
                    }
                 }
                 
                 gi += bit_end - bit_start;
                 bit_start = 0;
              }
              
              thr_info->oid_current = gi;
              *id_list_number = oidindex;
           } else {
              done = TRUE;
           }
        } else {
           done = TRUE;
        }
    } else {
       int real_readdb_entries;
       int total_readdb_entries;
       int final_real_seq;
       
       real_readdb_entries  = readdb_get_num_entries_total_real(rdfp);
       total_readdb_entries = readdb_get_num_entries_total(rdfp);
       final_real_seq = MIN( real_readdb_entries, thr_info->final_db_seq );
       
       /* we have real database with start/stop specified */
       if (thr_info->db_mutex) {
          *start = thr_info->db_chunk_last;
          if (thr_info->db_chunk_last < final_real_seq) {
             *stop = MIN((thr_info->db_chunk_last + 
                          thr_info->db_chunk_size), final_real_seq);
          } else {/* Already finished. */
             *stop = thr_info->db_chunk_last;
             
             /* Change parameters for oidlist processing. */
             thr_info->realdb_done  = TRUE;
          }
          thr_info->db_chunk_last = *stop;
       } else {
          if (*stop != final_real_seq) {
             done = FALSE;
             *start = thr_info->last_db_seq;
             *stop  = final_real_seq;
          } else {
             thr_info->realdb_done = TRUE;
             
             if (total_readdb_entries == real_readdb_entries) {
                done = TRUE;
             } else {
                thr_info->oid_current = final_real_seq;
             }
          }
       }
    }
    
    NlmMutexUnlock(thr_info->db_mutex);
    return done;
}

#define BLAST_DB_CHUNK_SIZE 1024
BlastThrInfoPtr BLAST_ThrInfoNew(ReadDBFILEPtr rdfp)
{
   BlastThrInfoPtr thr_info;
   
   thr_info = MemNew(sizeof(BlastThrInfo));
   thr_info->db_chunk_size = BLAST_DB_CHUNK_SIZE;
   thr_info->final_db_seq = readdb_get_num_entries_total(rdfp);
   
   return thr_info;
}

void BLAST_ThrInfoFree(BlastThrInfoPtr thr_info)
{
    if (thr_info == NULL)
	return;

    NlmMutexDestroy(thr_info->db_mutex);
    NlmMutexDestroy(thr_info->results_mutex);
    NlmMutexDestroy(thr_info->callback_mutex);

    MemFree(thr_info);
    
    return;
}

Int2 BLAST_ContextToFrame(Uint1 prog_number, Int2 context_number)
{
   Int2 frame=255;

   if (prog_number == blast_type_blastn) {
      if (context_number % 2 == 0)
         frame = 1;
      else
         frame = -1;
   } else if (prog_number == blast_type_blastp ||
              prog_number == blast_type_tblastn ||
              prog_number == blast_type_psitblastn) {
      /* Query and subject are protein, no frame. */
      frame = 0;
   } else if (prog_number == blast_type_blastx || 
              prog_number == blast_type_tblastx) {
      context_number = context_number % 6;
      frame = (context_number < 3) ? context_number+1 : -context_number+2;
   }
   
   return frame;
}

Int4 BLAST_GetQueryLength(BlastQueryInfoPtr query_info, Int4 context)
{
   return query_info->context_offsets[context+1] -
      query_info->context_offsets[context] - 1;
}
