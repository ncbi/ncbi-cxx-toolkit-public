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
 * Authors:  Ilya Dondoshansky
 *
 * File Description:
 *   Set up before call to BLAST engine
 *
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Seq_data_.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/api/blast_setup.hpp>
#include <algo/blast/api/blast_seq.hpp>
/* From core BLAST library: for encodings definitions */
#include <algo/blast/core/blast_util.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

static int
BLAST_SetUpQueryInfo(TSeqLocVector &slp,
    CBlastOption::EProgram program, BlastQueryInfo** query_info_ptr)
{
   Int4 length, protein_length;
   bool translate = 
      (program == blast_type_blastx || program == blast_type_tblastx);
   bool is_na = (program == blast_type_blastn);
   Int2 num_frames, frame;
   ENa_strand strand;
   BlastQueryInfo* query_info;
   Int4* context_offsets;
   Int4 index;

   if (translate)
      num_frames = 6;
   else if (is_na)
      num_frames = 2;
   else
      num_frames = 1;

   if ((query_info = (BlastQueryInfo*) malloc(sizeof(BlastQueryInfo)))
       == NULL)
      return -1;

   query_info->first_context = 0;
   query_info->num_queries = slp.size();
   query_info->last_context = query_info->num_queries*num_frames - 1;

   if ((strand = sequence::GetStrand(*slp[0].first, slp[0].second))
       == eNa_strand_minus) {
       if (translate)
           query_info->first_context = 3;
       else
           query_info->first_context = 1;
   } else if (strand == eNa_strand_plus) {
       if (translate)
           query_info->last_context -= 3;
       else 
           query_info->last_context -= 1;
   }

   if ((context_offsets = (Int4*) 
      malloc((query_info->last_context+2)*sizeof(Int4))) == NULL)
      return -1;

   if ((query_info->eff_searchsp_array = 
      (Int8*) malloc((query_info->last_context+1)*sizeof(Int8))) == NULL)
      return -1;
   if ((query_info->length_adjustments =
        (Int4*) malloc((query_info->last_context+1)*sizeof(Int4))) == NULL)
       return -1;

   context_offsets[0] = 0;

   query_info->context_offsets = context_offsets;
   
   /* Fill the context offsets */
   for (index = 0; index <= query_info->last_context; index += num_frames) {
      length = sequence::GetLength(*slp[index/num_frames].first, slp[index/num_frames].second);
      strand = sequence::GetStrand(*slp[index/num_frames].first, 
                   slp[index/num_frames].second);
      if (translate) {
         Int2 first_frame, last_frame;
         if (strand == eNa_strand_plus) {
            first_frame = 0;
            last_frame = 2;
         } else if (strand == eNa_strand_minus) {
            first_frame = 3;
            last_frame = 5;
         } else {
            first_frame = 0;
            last_frame = 5;
         }
         for (frame = 0; frame < first_frame; ++frame)
            context_offsets[index+frame+1] = context_offsets[index+frame];
         for (frame = first_frame; frame <= last_frame; ++frame) {
            protein_length = (length - frame%CODON_LENGTH)/CODON_LENGTH;
            context_offsets[index+frame+1] = 
               context_offsets[index+frame] + protein_length + 1;
         }
         for ( ; frame < num_frames; ++frame)
            context_offsets[index+frame+1] = context_offsets[index+frame];
      } else if (is_na) {
         if (strand == eNa_strand_plus) {
            context_offsets[index+1] = context_offsets[index] + length + 1;
            context_offsets[index+2] = context_offsets[index+1];
         } else if (strand == eNa_strand_minus) {
            context_offsets[index+1] = context_offsets[index];
            context_offsets[index+2] = context_offsets[index+1] + length + 1;
         } else {
            context_offsets[index+1] = context_offsets[index] + length + 1;
            context_offsets[index+2] = context_offsets[index+1] + length + 1;
         }
      } else {
         context_offsets[index+1] = context_offsets[index] + length + 1;
      }
   }
   query_info->total_length = context_offsets[index];

   *query_info_ptr = query_info;
   return 0;
}

/** Given a SeqPort, fills a preallocated sequence buffer 
 * in the correct encoding.
 */
static int SeqVectorToSequenceBuffer(CSeqVector &sv, Uint1 encoding, 
                                      Uint1** buffer)
{
   Uint1* buffer_var = *buffer;
   int i;

   if (!buffer_var)
      return -1;

   switch (encoding) {
   case BLASTP_ENCODING: case NCBI4NA_ENCODING:
       for (i = 0; i < sv.size(); i++)
           *(buffer_var++) = sv[i];
       break;
   case BLASTNA_ENCODING:
       for (i = 0; i < sv.size(); i++)
           *(buffer_var++) = NCBI4NA_TO_BLASTNA[sv[i]];
       break;
   default:
      break;
   }

   *buffer = buffer_var;
   return 0;
}

/** Fills sequence buffer for a single CSeq_loc; 
 * fills both strands if necessary.
 */
static int 
BLASTFillSequenceBuffer(const CSeq_loc &sl, CScope* scope, 
    Uint1 encoding, Boolean add_sentinel_bytes, Boolean both_strands, 
    Uint1* buffer)
{
   Uint1* buffer_var;
   Uint1 sentinel = 
      (encoding == BLASTNA_ENCODING ? NCBI4NA_TO_BLASTNA[NULLB] : NULLB);
   CSeq_data::E_Choice seq_code;
   ENa_strand strand;
   int status = 0;
   CBioseq_Handle handle = scope->GetBioseqHandle(sl);
   
   if (!handle) {
       ERR_POST(Error << "Could not retrieve bioseq_handle");
       return NULL;
   }

   buffer_var = buffer;

   if (add_sentinel_bytes) {
      *buffer_var = sentinel;
      ++buffer_var;
   }

   if (encoding == BLASTP_ENCODING) {
      seq_code = CSeq_data::e_Ncbistdaa;
      strand = eNa_strand_unknown;
   } else {
      seq_code = CSeq_data::e_Ncbi4na;
      strand = sequence::GetStrand(sl, scope);
   }

   // Retrieves the correct strand (plus or minus), but not both
   CSeqVector sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi, strand);
   sv.SetCoding(seq_code);

   status = SeqVectorToSequenceBuffer(sv, encoding, &buffer_var);

   if (add_sentinel_bytes)
      *buffer_var = sentinel;

   if (both_strands && strand == eNa_strand_both) {

      ++buffer_var;

      sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                               eNa_strand_minus);
      sv.SetCoding(seq_code);
      status = SeqVectorToSequenceBuffer(sv, encoding, &buffer_var);
      if (add_sentinel_bytes)
         *buffer_var = sentinel;

   }

   return status;
}

/** BLAST_GetSequence
 * Purpose:     Get the sequence for the BLAST engine, put in a Uint1 buffer
 * @param slp SeqLoc to extract sequence for [in]
 * @param query_info The query information structure, pre-initialized,
 *                   but filled here [in]
 * @param query_options Query setup options, containing the genetic code for
 *                      translation [in]
 * @param num_frames How many frames to get for this sequence? [in]
 * @param encoding In what encoding to retrieve the sequence? [in]
 * @param buffer_out Buffer to hold plus strand or protein [out]
 * @param buffer_length Length of buffer allocated [out]
 */
static Int2 
BLAST_GetSequence(TSeqLocVector & slp, BlastQueryInfo* query_info, 
   const QuerySetUpOptions* query_options, Uint1 num_frames, Uint1 encoding, 
   Uint1* *buffer_out, Int4 *buffer_length)
{
   Int2		status=0; /* return value. */
   Int4 total_length; /* Total length of all queries/frames/strands */
   Int4		index; /* Loop counter */
   Uint1*	buffer,* buffer_var; /* buffer offset to be worked on. */
   bool add_sentinel_bytes = TRUE;
   Uint1* genetic_code=NULL;
   bool translate = FALSE;
   Int4 offset = 0;

   if (!query_info)
       return -1;

   *buffer_length = total_length = 
       query_info->context_offsets[query_info->last_context+1] + 1;

   if (num_frames == 6) {
      /* Sequence must be translated in 6 frames. This can only happen
         for query - subject sequences are translated later. */
      Int4 gc;
      
      translate = TRUE;
      gc = (query_options ? query_options->genetic_code : 1);

      if ((genetic_code = BLASTFindGeneticCode(gc)) == NULL)
          return -1;
   }

   *buffer_out = buffer = (Uint1 *) malloc((total_length)*sizeof(Uint1));
   
   for (index = 0; index <= query_info->last_context; index += num_frames)
   {
       int i = index/num_frames;

       if (translate) {
           Uint1* na_buffer;
           Int2 frame, frame_start, frame_end;
           Int4 na_length;
           Uint1 strand;
           
           na_length = sequence::GetLength(*slp[i].first, slp[i].second);
           strand = sequence::GetStrand(*slp[i].first, slp[i].second);
           /* Retrieve nucleotide sequence in an auxiliary buffer; 
              then translate into the appropriate place in the 
              preallocated buffer */
           if (strand == eNa_strand_plus) {
               na_buffer = (Uint1 *) malloc(na_length + 2);
               frame_start = 0;
               frame_end = 2;
           } else if (strand == eNa_strand_minus) {
               na_buffer = (Uint1 *) malloc(na_length + 2);
               frame_start = 3;
               frame_end = 5;
           } else {
               na_buffer = (Uint1*) malloc(2*na_length + 3);
               frame_start = 0;
               frame_end = 5;
           }
           BLASTFillSequenceBuffer(*slp[i].first, slp[i].second, encoding, TRUE, TRUE, na_buffer);
           buffer_var = na_buffer + 1;
      
           for (frame = frame_start; frame <= frame_end; frame++) {
               offset = query_info->context_offsets[index+frame];
               if (strand == eNa_strand_both && frame == 3) {
                   /* Advance the nucleotide sequence pointer to the 
                      beginning of the reverse strand */
                   buffer_var += na_length + 1;
               }
               BLAST_GetTranslation(buffer_var, NULL, na_length, 
                                    frame%3+1, &buffer[offset], genetic_code);
           }
           sfree(na_buffer);
       } else {
           /* This can happen both for query and subject, so query_info 
              might not be initialized here. */
           if (query_info)
               offset = query_info->context_offsets[index];
           BLASTFillSequenceBuffer(*slp[i].first, slp[i].second, encoding, 
               add_sentinel_bytes, (num_frames == 2), &buffer[offset]);
       }
   }

   return status;
}

int
BLAST_SetUpQuery(CBlastOption::EProgram program_number, 
    TSeqLocVector &query_slp, const QuerySetUpOptions* query_options, 
    BlastQueryInfo** query_info, BLAST_SequenceBlk* *query_blk)
{
   Uint1* buffer;	/* holds sequence for plus strand or protein. */
   Int4 buffer_length;
   Int2 status;
   Uint1 num_frames;
   Uint1 encoding;

   if ((status = BLAST_SetUpQueryInfo(query_slp, program_number, 
                                      query_info)))
      return status;

   if (program_number == CBlastOption::eBlastn) {
      encoding = BLASTNA_ENCODING;
      num_frames = 2;
   } else if (program_number == CBlastOption::eBlastp || 
              program_number == CBlastOption::eTblastn) {
      encoding = BLASTP_ENCODING;
      num_frames = 1;
   } else { 
      encoding = NCBI4NA_ENCODING;
      num_frames = 6;
   }

   if ((status=BLAST_GetSequence(query_slp, *query_info, query_options,
                  num_frames, encoding, &buffer, &buffer_length)))
      return status; 
        
   /* Do not count the first and last sentinel bytes in the 
      query length */
   if ((status=BlastSetUp_SeqBlkNew(buffer, buffer_length-2, 
                                    0, query_blk, TRUE)))
      return status;

   return 0;
}

END_NCBI_SCOPE
