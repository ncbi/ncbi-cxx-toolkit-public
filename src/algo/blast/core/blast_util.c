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

Int2
BlastSetUp_SeqBlkNew (const Uint1Ptr buffer, Int4 length, Int2 context,
	const Int4Ptr frame, BLAST_SequenceBlkPtr *seq_blk, 
        Boolean buffer_allocated)
{
	*seq_blk = MemNew(sizeof(BLAST_SequenceBlk));

        if (buffer_allocated) {
           (*seq_blk)->sequence_start = buffer;
           (*seq_blk)->sequence_allocated = buffer_allocated;
           /* The first byte is a sentinel byte. */
           (*seq_blk)->sequence = (*seq_blk)->sequence_start+1;
        } else {
           (*seq_blk)->sequence = buffer;
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
                     Int4 oid, Boolean compressed)
{
  Int4 length, buf_len = 0;
  Uint1Ptr buffer = NULL;

  if (compressed) {
     length=readdb_get_sequence(db, oid, &buffer);
  } else {
     length = readdb_get_sequence_ex(db, oid, &buffer, &buf_len, TRUE);
  }

  BlastSetUp_SeqBlkNew(buffer, length, 0, NULL, seq_blk, !compressed);
  (*seq_blk)->oid = oid;
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


BLAST_SequenceBlkPtr BLAST_SequenceBlkDestruct(BLAST_SequenceBlkPtr seq_blk)
{
   if (seq_blk->sequence_allocated)
      MemFree(seq_blk->sequence_start);
   SeqIdFree(seq_blk->seqid);
   return (BLAST_SequenceBlkPtr) MemFree(seq_blk);
}

Uint1Ptr LIBCALL
BLAST_GetTranslation(Uint1Ptr query_seq, Int4 nt_length, Int2 frame, 
   Int4Ptr length, CharPtr genetic_code)
{
	Uint1 codon[CODON_LENGTH];
	Int4 index, index_prot;
	SeqMapTablePtr smtp;
	Uint1 residue, new_residue;
	Uint1Ptr prot_seq;

	smtp = SeqMapTableFind(Seq_code_ncbistdaa, Seq_code_ncbieaa);

	/* Allocate two extra spaces for NULLB's at beginning and end of seq. */
	prot_seq = (Uint1Ptr) MemNew((2+(nt_length+2)/CODON_LENGTH)*sizeof(Uint1));

	/* The first character in the protein is the NULLB sentinel. */
	prot_seq[0] = NULLB;
	index_prot = 1;
	for (index=ABS(frame)-1; index<nt_length-2; index += CODON_LENGTH)
	{
		codon[0] = query_seq[index];
		codon[1] = query_seq[index+1];
		codon[2] = query_seq[index+2];
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

