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
* Author:  Christiam Camacho / Kevin Bealer
*
* ===========================================================================
*/

/// @file blast_objmgr_tools.cpp
/// Functions in xblast API code that interact with object manager.

#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/multiseq_src.hpp>
#include "blast_setup.hpp"
#include <algo/blast/core/blast_encoding.h>

#include <serial/iterator.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(ncbi::objects);


Uint1
GetQueryEncoding(EProgram program);

Uint1
GetSubjectEncoding(EProgram program);

CSeq_align_set*
x_CreateEmptySeq_align_set(CSeq_align_set* sas);

CRef<CSeq_align>
BLASTUngappedHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id, Int4 query_length, Int4 subject_length);

CRef<CSeq_align>
BLASTHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id, bool is_ooframe);


/// Now allows query concatenation
void
SetupQueryInfo(const TSeqLocVector& queries, const CBlastOptions& options, 
               BlastQueryInfo** qinfo)
{
    ASSERT(qinfo);

    // Allocate and initialize the query info structure
    if ( !((*qinfo) = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query info");
    }

    EProgram prog = options.GetProgram();
    unsigned int nframes = GetNumberOfFrames(prog);
    (*qinfo)->num_queries = static_cast<int>(queries.size());
    (*qinfo)->first_context = 0;
    (*qinfo)->last_context = (*qinfo)->num_queries * nframes - 1;

    // Allocate the various arrays of the query info structure
    int* context_offsets = NULL;
    if ( !(context_offsets = (int*)
           malloc(sizeof(int) * ((*qinfo)->last_context + 2)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Context offsets array");
    }
    if ( !((*qinfo)->eff_searchsp_array = 
           (Int8*) calloc((*qinfo)->last_context + 1, sizeof(Int8)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Search space array");
    }
    if ( !((*qinfo)->length_adjustments = 
           (int*) calloc((*qinfo)->last_context + 1, sizeof(int)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Length adjustments array");
    }

    (*qinfo)->context_offsets = context_offsets;
    context_offsets[0] = 0;

    bool is_na = (prog == eBlastn) ? true : false;
    bool translate = 
        ((prog == eBlastx) || (prog == eTblastx)) ? true : false;

    // Adjust first context depending on the first query strand
    // Unless the strand option is set to single strand, the actual
    // CSeq_locs dictate which strand to examine during the search.
    ENa_strand strand_opt = options.GetStrandOption();
    ENa_strand strand;

    if (is_na || translate) {
        if (strand_opt == eNa_strand_both || 
            strand_opt == eNa_strand_unknown) {
            strand = sequence::GetStrand(*queries.front().seqloc, 
                                         queries.front().scope);
        } else {
            strand = strand_opt;
        }

        if (strand == eNa_strand_minus) {
            if (translate) {
                (*qinfo)->first_context = 3;
            } else {
                (*qinfo)->first_context = 1;
            }
        }
    }

    // Set up the context offsets into the sequence that will be added
    // to the sequence block structure.
    unsigned int ctx_index = 0;      // index into context_offsets array
    // Longest query length, to be saved in the query info structure
    Uint4 max_length = 0;

    ITERATE(TSeqLocVector, itr, queries) {
        TSeqPos length = sequence::GetLength(*itr->seqloc, itr->scope);
        ASSERT(length != numeric_limits<TSeqPos>::max());

        strand = sequence::GetStrand(*itr->seqloc, itr->scope);
        if (strand_opt == eNa_strand_minus || strand_opt == eNa_strand_plus) {
            strand = strand_opt;
        }

        if (translate) {
            for (unsigned int i = 0; i < nframes; i++) {
                unsigned int prot_length = 
                    (length - i % CODON_LENGTH) / CODON_LENGTH;
                max_length = MAX(max_length, prot_length);

                switch (strand) {
                case eNa_strand_plus:
                    if (i < 3) {
                        context_offsets[ctx_index + i + 1] = 
                            context_offsets[ctx_index + i] + prot_length + 1;
                    } else {
                        context_offsets[ctx_index + i + 1] = 
                            context_offsets[ctx_index + i];
                    }
                    break;

                case eNa_strand_minus:
                    if (i < 3) {
                        context_offsets[ctx_index + i + 1] = 
                            context_offsets[ctx_index + i];
                    } else {
                        context_offsets[ctx_index + i + 1] =
                            context_offsets[ctx_index + i] + prot_length + 1;
                    }
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    context_offsets[ctx_index + i + 1] = 
                        context_offsets[ctx_index + i] + prot_length + 1;
                    break;

                default:
                    abort();
                }
            }
        } else {
            max_length = MAX(max_length, length);
            if (is_na) {
                switch (strand) {
                case eNa_strand_plus:
                    context_offsets[ctx_index + 1] =
                        context_offsets[ctx_index] + length + 1;
                    context_offsets[ctx_index + 2] =
                        context_offsets[ctx_index + 1];
                    break;

                case eNa_strand_minus:
                    context_offsets[ctx_index + 1] =
                        context_offsets[ctx_index];
                    context_offsets[ctx_index + 2] =
                        context_offsets[ctx_index + 1] + length + 1;
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    context_offsets[ctx_index + 1] =
                        context_offsets[ctx_index] + length + 1;
                    context_offsets[ctx_index + 2] =
                        context_offsets[ctx_index + 1] + length + 1;
                    break;

                default:
                    abort();
                }
            } else {    // protein
                context_offsets[ctx_index + 1] = 
                    context_offsets[ctx_index] + length + 1;
            }
        }
        ctx_index += nframes;
    }
    (*qinfo)->max_length = max_length;
}

// Compresses sequence data on vector to buffer, which should have been
// allocated and have the right size.
void CompressDNA(const CSeqVector& vec, Uint1* buffer, const int buflen)
{
    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence
    CSeqVector_CI iter(vec);

    iter.SetRandomizeAmbiguities();
    iter.SetCoding(CSeq_data::e_Ncbi2na);

    // ASSERT(vec.GetCoding() == CSeq_data::e_Ncbi2na);


    for (ci = 0, i = 0; ci < (TSeqPos) buflen-1; ci++, i += COMPRESSION_RATIO) {
        Uint1 a, b, c, d;
        a = ((*iter & NCBI2NA_MASK)<<6); ++iter;
        b = ((*iter & NCBI2NA_MASK)<<4); ++iter;
        c = ((*iter & NCBI2NA_MASK)<<2); ++iter;
        d = ((*iter & NCBI2NA_MASK)<<0); ++iter;
        buffer[ci] = a | b | c | d;
    }

    buffer[ci] = 0;
    for (; i < vec.size(); i++) {
            Uint1 bit_shift = 0;
            switch (i%COMPRESSION_RATIO) {
               case 0: bit_shift = 6; break;
               case 1: bit_shift = 4; break;
               case 2: bit_shift = 2; break;
               default: abort();   // should never happen
            }
            buffer[ci] |= ((*iter & NCBI2NA_MASK)<<bit_shift);
            ++iter;
    }
    // Set the number of bases in the last byte.
    buffer[ci] |= vec.size()%COMPRESSION_RATIO;
}

void
SetupQueries(const TSeqLocVector& queries, const CBlastOptions& options,
             const CBlastQueryInfo& qinfo, BLAST_SequenceBlk** seqblk)
{
    ASSERT(seqblk);
    ASSERT(queries.size() != 0);

    EProgram prog = options.GetProgram();

    // Determine sequence encoding
    Uint1 encoding = GetQueryEncoding(prog);

    int buflen = qinfo->context_offsets[qinfo->last_context+1] + 1;
    Uint1* buf = (Uint1*) calloc((buflen+1), sizeof(Uint1));
    if ( !buf ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence buffer");
    }

    bool is_na = (prog == eBlastn) ? true : false;
    bool translate = ((prog == eBlastx) || (prog == eTblastx)) ? true : false;

    unsigned int ctx_index = 0;      // index into context_offsets array
    unsigned int nframes = GetNumberOfFrames(prog);

    BlastMaskLoc* mask = NULL, *head_mask = NULL, *last_mask = NULL;

    // Unless the strand option is set to single strand, the actual
    // CSeq_locs dictacte which strand to examine during the search
    ENa_strand strand_opt = options.GetStrandOption();
    int index = 0;

    ITERATE(TSeqLocVector, itr, queries) {

        ENa_strand strand;

        if ((is_na || translate) &&
            (strand_opt == eNa_strand_unknown || 
             strand_opt == eNa_strand_both)) 
        {
            strand = sequence::GetStrand(*itr->seqloc, itr->scope);
        } else {
            strand = strand_opt;
        }

        if (itr->mask)
            mask = CSeqLoc2BlastMaskLoc(*itr->mask, index);

        ++index;

        if (translate) {
            ASSERT(strand == eNa_strand_both ||
                   strand == eNa_strand_plus ||
                   strand == eNa_strand_minus);

            // Get both strands of the original nucleotide sequence with
            // sentinels
            pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
                GetSequence(*itr->seqloc, encoding, itr->scope, strand,
                            eSentinels));
            

            AutoPtr<Uint1, ArrayDeleter<Uint1> > gc = 
                FindGeneticCode(options.GetQueryGeneticCode());
            int na_length = sequence::GetLength(*itr->seqloc, itr->scope);
            Uint1* seqbuf_rev = NULL;  // negative strand
            if (strand == eNa_strand_both)
               seqbuf_rev = seqbuf.first.get() + na_length + 1;
            else if (strand == eNa_strand_minus)
               seqbuf_rev = seqbuf.first.get() + 1;

            // Populate the sequence buffer
            for (unsigned int i = 0; i < nframes; i++) {
                if (BLAST_GetQueryLength(qinfo, i) <= 0) {
                    continue;
                }

                int offset = qinfo->context_offsets[ctx_index + i];
                short frame = BLAST_ContextToFrame(prog, i);
                BLAST_GetTranslation(seqbuf.first.get() + 1, seqbuf_rev,
                   na_length, frame, &buf[offset], gc.get());
            }
            // Translate the lower case mask coordinates;
            BlastMaskLocDNAToProtein(&mask, *itr->seqloc, itr->scope);

        } else if (is_na) {

            ASSERT(strand == eNa_strand_both ||
                   strand == eNa_strand_plus ||
                   strand == eNa_strand_minus);

            pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
                GetSequence(*itr->seqloc, encoding, itr->scope, strand,
                            eSentinels));
            int idx = (strand == eNa_strand_minus) ? 
                ctx_index + 1 : ctx_index;
            int offset = qinfo->context_offsets[idx];
            memcpy(&buf[offset], seqbuf.first.get(), seqbuf.second);

        } else {

            pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
                GetSequence(*itr->seqloc, encoding, itr->scope,
                            eNa_strand_unknown, eSentinels));
            int offset = qinfo->context_offsets[ctx_index];
            memcpy(&buf[offset], seqbuf.first.get(), seqbuf.second);

        }

        ctx_index += nframes;
        
        if (mask) {
            if ( !last_mask )
                head_mask = last_mask = mask;
            else {
                last_mask->next = mask;
                last_mask = last_mask->next;
            }
        }
    }

    if (BlastSeqBlkNew(seqblk) < 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence block");
    }

    BlastSeqBlkSetSequence(*seqblk, buf, buflen - 2);

    (*seqblk)->lcase_mask = head_mask;
    (*seqblk)->lcase_mask_allocated = TRUE;

    return;
}

void
SetupSubjects(const TSeqLocVector& subjects, 
              EProgram prog,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    ASSERT(seqblk_vec);
    ASSERT(max_subjlen);
    ASSERT(subjects.size() != 0);

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
    bool subj_is_na = (prog == eBlastn  ||
                       prog == eTblastn ||
                       prog == eTblastx);

    ESentinelType sentinels = eSentinels;
    if (prog == eTblastn || prog == eTblastx) {
        sentinels = eNoSentinels;
    }

    Uint1 encoding = GetSubjectEncoding(prog);
       
    // TODO: Should strand selection on the subject sequences be allowed?
    //ENa_strand strand = options->GetStrandOption(); 
    int index = 0; // Needed for lower case masks only.

    *max_subjlen = 0;

    ITERATE(TSeqLocVector, itr, subjects) {
        BLAST_SequenceBlk* subj = NULL;

        pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
            GetSequence(*itr->seqloc, encoding, itr->scope,
                        eNa_strand_plus, sentinels));

        if (BlastSeqBlkNew(&subj) < 0) {
            NCBI_THROW(CBlastException, eOutOfMemory, "Subject sequence block");
        }

        /* Set the lower case mask, if it exists */
        if (itr->mask)
            subj->lcase_mask = CSeqLoc2BlastMaskLoc(*itr->mask, index);
        ++index;

        if (subj_is_na) {
            BlastSeqBlkSetSequence(subj, seqbuf.first.release(), 
               (sentinels == eSentinels) ? seqbuf.second - 2 : seqbuf.second);

            try {
                // Get the compressed sequence
                pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> comp_seqbuf(
                    GetSequence(*itr->seqloc, NCBI2NA_ENCODING, itr->scope,
                                 eNa_strand_plus, eNoSentinels));
                BlastSeqBlkSetCompressedSequence(subj, 
                                                 comp_seqbuf.first.release());
            } catch (const CSeqVectorException& sve) {
                BlastSequenceBlkFree(subj);
                NCBI_THROW(CBlastException, eInternal, sve.what());
            }
        } else {
            BlastSeqBlkSetSequence(subj, seqbuf.first.release(), 
                                   seqbuf.second - 2);
        }

        seqblk_vec->push_back(subj);
        (*max_subjlen) = MAX((*max_subjlen),
                sequence::GetLength(*itr->seqloc, itr->scope));

    }
}

pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos>
GetSequence(const CSeq_loc& sl, Uint1 encoding, CScope* scope,
            ENa_strand strand, ESentinelType sentinel) 
            THROWS((CBlastException, CException))
{
    Uint1* buf = NULL;          // buffer to write sequence
    Uint1* buf_var = NULL;      // temporary pointer to buffer
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    AutoPtr<Uint1, CDeleter<Uint1> > safe_buf; // contains buf to ensure 
                                               // exception safety

    CBioseq_Handle handle = scope->GetBioseqHandle(sl); // might throw exception

    // Retrieves the correct strand (plus or minus), but not both
    CSeqVector sv =
        handle.GetSequenceView(sl, CBioseq_Handle::eViewConstructed,
                               CBioseq_Handle::eCoding_Ncbi);

    switch (encoding) {
    // Protein sequences (query & subject) always have sentinels around sequence
    case BLASTP_ENCODING:
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        buflen = CalculateSeqBufferLength(sv.size(), BLASTP_ENCODING);
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        *buf_var++ = GetSentinelByte(encoding);
        for (i = 0; i < sv.size(); i++)
            *buf_var++ = sv[i];
        *buf_var++ = GetSentinelByte(encoding);
        break;

    case NCBI4NA_ENCODING:
    case BLASTNA_ENCODING: // Used for nucleotide blastn queries
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        buflen = CalculateSeqBufferLength(sv.size(), NCBI4NA_ENCODING,
                                          strand, sentinel);
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        if (sentinel == eSentinels)
            *buf_var++ = GetSentinelByte(encoding);

        if (encoding == BLASTNA_ENCODING) {
            for (i = 0; i < sv.size(); i++) {
                *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
            }
        } else {
            for (i = 0; i < sv.size(); i++) {
                *buf_var++ = sv[i];
            }
        }
        if (sentinel == eSentinels)
            *buf_var++ = GetSentinelByte(encoding);

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSequenceView(sl, CBioseq_Handle::eViewConstructed,
                                        CBioseq_Handle::eCoding_Ncbi, 
                                        eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi4na);
            if (encoding == BLASTNA_ENCODING) {
                for (i = 0; i < sv.size(); i++) {
                    *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
                }
            } else {
                for (i = 0; i < sv.size(); i++) {
                    *buf_var++ = sv[i];
                }
            }
            if (sentinel == eSentinels) {
                *buf_var++ = GetSentinelByte(encoding);
            }
        }

        break;

    /* Used only in Blast2Sequences for the subject sequence. 
     * No sentinels can be used. As in readdb, remainder 
     * (sv.size()%COMPRESSION_RATIO != 0) goes in the last 2 bits of the 
     * last byte.
     */
    case NCBI2NA_ENCODING:
        ASSERT(sentinel == eNoSentinels);
        sv.SetCoding(CSeq_data::e_Ncbi2na);
        buflen = CalculateSeqBufferLength(sv.size(), sv.GetCoding(),
                                          eNa_strand_plus, eNoSentinels);
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        buf = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        CompressDNA(sv, buf, buflen);
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Invalid encoding");
    }

    return make_pair(safe_buf, buflen);
}

void BlastMaskLocDNAToProtein(BlastMaskLoc** mask_ptr, const CSeq_loc &seqloc, 
                           CScope* scope)
{
   BlastMaskLoc* last_mask = NULL,* head_mask = NULL,* mask_loc; 
   Int4 dna_length;
   BlastSeqLoc* dna_loc,* prot_loc_head,* prot_loc_last;
   SSeqRange* dip;
   Int4 context;
   Int2 frame;
   Int4 from, to;

   if (!mask_ptr)
      return;

   mask_loc = *mask_ptr;

   if (!mask_loc) 
      return;

   dna_length = sequence::GetLength(seqloc, scope);
   /* Reproduce this mask for all 6 frames, with translated 
      coordinates */
   for (context = 0; context < NUM_FRAMES; ++context) {
       if (!last_mask) {
           head_mask = last_mask = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
       } else {
           last_mask->next = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
           last_mask = last_mask->next;
       }
         
       last_mask->index = NUM_FRAMES * mask_loc->index + context;
       prot_loc_last = prot_loc_head = NULL;
       
       frame = BLAST_ContextToFrame(blast_type_blastx, context);
       
       for (dna_loc = mask_loc->loc_list; dna_loc; 
            dna_loc = dna_loc->next) {
           dip = (SSeqRange*) dna_loc->ptr;
           if (frame < 0) {
               from = (dna_length + frame - dip->right)/CODON_LENGTH;
               to = (dna_length + frame - dip->left)/CODON_LENGTH;
           } else {
               from = (dip->left - frame + 1)/CODON_LENGTH;
               to = (dip->right - frame + 1)/CODON_LENGTH;
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

   /* Free the mask with nucleotide coordinates */
   BlastMaskLocFree(mask_loc);
   /* Return the new mask with protein coordinates */
   *mask_ptr = head_mask;
}

void BlastMaskLocProteinToDNA(BlastMaskLoc** mask_ptr, TSeqLocVector &slp)
{
   BlastMaskLoc* mask_loc;
   BlastSeqLoc* loc;
   SSeqRange* dip;
   Int4 dna_length;
   Int2 frame;
   Int4 from, to;

   if (!mask_ptr) 
      // Nothing to do - just return
      return;

   for (mask_loc = *mask_ptr; mask_loc; mask_loc = mask_loc->next) {
      dna_length = 
         sequence::GetLength(*slp[mask_loc->index/NUM_FRAMES].seqloc, 
                             slp[mask_loc->index/NUM_FRAMES].scope);
      frame = BLAST_ContextToFrame(blast_type_blastx, 
                                   mask_loc->index % NUM_FRAMES);
      
      for (loc = mask_loc->loc_list; loc; loc = loc->next) {
         dip = (SSeqRange*) loc->ptr;
         if (frame < 0)	{
            to = dna_length - CODON_LENGTH*dip->left + frame;
            from = dna_length - CODON_LENGTH*dip->right + frame + 1;
         } else {
            from = CODON_LENGTH*dip->left + frame - 1;
            to = CODON_LENGTH*dip->right + frame - 1;
         }
         dip->left = from;
         dip->right = to;
      }
   }
}

void* CMultiSeqInfo::GetSeqId(int index)
{
    CSeq_id* seqid = 
        const_cast<CSeq_id*>(&sequence::GetId(*m_vSeqVector[index].seqloc,
                                              m_vSeqVector[index].scope));

    return (void*) seqid;
}

void
x_GetSequenceLengthAndId(const SSeqLoc* ss,          // [in]
                         const BlastSeqSrc* seq_src, // [in]
                         int oid,                    // [in] 
                         CSeq_id** seqid,            // [out]
                         TSeqPos* length)            // [out]
{
    ASSERT(ss || seq_src);
    ASSERT(seqid);
    ASSERT(length);

    if ( !seq_src ) {
        *seqid = const_cast<CSeq_id*>(&sequence::GetId(*ss->seqloc,
                                                       ss->scope));
        *length = sequence::GetLength(*ss->seqloc, ss->scope);
    } else {
        ListNode* seqid_wrap;
        seqid_wrap = BLASTSeqSrcGetSeqId(seq_src, (void*) &oid);
        ASSERT(seqid_wrap);
        if (seqid_wrap->choice == BLAST_SEQSRC_CPP_SEQID) {
            *seqid = (CSeq_id*)seqid_wrap->ptr;
            ListNodeFree(seqid_wrap);
        } else if (seqid_wrap->choice == BLAST_SEQSRC_CPP_SEQID_REF) {
            *seqid = ((CRef<CSeq_id>*)seqid_wrap->ptr)->GetPointer();
        } else {
            /** FIXME!!! This is wrong, because the id created here will 
                not be registered! However if sequence source returns a 
                C object, we cannot handle it here. */
            char* id = BLASTSeqSrcGetSeqIdStr(seq_src, (void*) &oid);
            string id_str(id);
            *seqid = new CSeq_id(id_str);
            sfree(id);
        }
        *length = BLASTSeqSrcGetSeqLen(seq_src, (void*) &oid);
    }
    return;
}

/// Always remap the query, the subject is remapped if it's given (assumes
/// alignment created by BLAST 2 Sequences API).
/// Since the query strands were already taken into account when CSeq_align 
/// was created, only start position shifts in the CSeq_loc's are relevant in 
/// this function. However full remapping is necessary for the subject sequence
/// if it is on a negative strand.

static void
x_RemapAlignmentCoordinates(CRef<CSeq_align> sar,
                            const SSeqLoc* query,
                            const SSeqLoc* subject = NULL)
{
    _ASSERT(sar);
    ASSERT(query);
    const int query_dimension = 0;
    const int subject_dimension = 1;

    // If subject is on a minus strand, we'll need to flip subject strands 
    // and remap subject coordinates on all segments.
    // Otherwise we only need to shift query and/or subject coordinates, 
    // if the respective location starts not from 0.
    bool remap_subject =
        (subject && subject->seqloc->IsInt() &&
         subject->seqloc->GetInt().GetStrand() == eNa_strand_minus);

    TSeqPos q_shift = 0, s_shift = 0;

    if (query->seqloc->IsInt()) {
        q_shift = query->seqloc->GetInt().GetFrom();
    }
    if (subject && subject->seqloc->IsInt()) {
        s_shift = subject->seqloc->GetInt().GetFrom();
    }

    if (remap_subject || q_shift > 0 || s_shift > 0) {
        for (CTypeIterator<CDense_seg> itr(Begin(*sar)); itr; ++itr) {
            const vector<ENa_strand> strands = itr->GetStrands();
            // Create temporary CSeq_locs with strands either matching 
            // (for query and for subject if it is not on a minus strand),
            // or opposite to those in the segment, to force RemapToLoc to 
            // behave in the correct way.
            if (q_shift > 0) {
                CSeq_loc q_seqloc;
                ENa_strand q_strand = strands[0];
                q_seqloc.SetInt().SetFrom(q_shift);
                q_seqloc.SetInt().SetTo(query->seqloc->GetInt().GetTo());
                q_seqloc.SetInt().SetStrand(q_strand);
                q_seqloc.SetInt().SetId().Assign(sequence::GetId(*query->seqloc, query->scope));
                itr->RemapToLoc(query_dimension, q_seqloc, true);
            }
            if (remap_subject || s_shift > 0) {
                CSeq_loc s_seqloc;
                ENa_strand s_strand;
                if (remap_subject) {
                    s_strand = ((strands[1] == eNa_strand_plus) ?
                                eNa_strand_minus : eNa_strand_plus);
                } else {
                    s_strand = strands[1];
                }
                s_seqloc.SetInt().SetFrom(s_shift);
                s_seqloc.SetInt().SetTo(subject->seqloc->GetInt().GetTo());
                s_seqloc.SetInt().SetStrand(s_strand);
                s_seqloc.SetInt().SetId().Assign(sequence::GetId(*subject->seqloc, subject->scope));
                itr->RemapToLoc(subject_dimension, s_seqloc, !remap_subject);
            }
        }
    }
}

CSeq_align_set*
BLAST_HitList2CSeqAlign(const BlastHitList* hit_list,
    EProgram prog, SSeqLoc &query,
    const BlastSeqSrc* seq_src, bool is_gapped, bool is_ooframe)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();

    ASSERT(seq_src);

    if (!hit_list) {
        return x_CreateEmptySeq_align_set(seq_aligns);
    }

    TSeqPos query_length = 0;
    CSeq_id* qid = NULL;
    CConstRef<CSeq_id> query_id;
    x_GetSequenceLengthAndId(&query, NULL, 0, &qid, &query_length);
    query_id.Reset(qid);

    TSeqPos subj_length = 0;
    CSeq_id* sid = NULL;
    CConstRef<CSeq_id> subject_id;

    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;

        x_GetSequenceLengthAndId(NULL, seq_src, hsp_list->oid,
                                 &sid, &subj_length);
        subject_id.Reset(sid);

        // Create a CSeq_align for each matching sequence
        CRef<CSeq_align> hit_align;
        if (is_gapped) {
            hit_align =
                BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                       subject_id, is_ooframe);
        } else {
            hit_align =
                BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                    subject_id, query_length, subj_length);
        }
        ListNode* subject_loc_wrap =
            BLASTSeqSrcGetSeqLoc(seq_src, (void*)&hsp_list->oid);
        SSeqLoc* subject_loc = NULL;
        if (subject_loc_wrap &&
            subject_loc_wrap->choice == BLAST_SEQSRC_CPP_SEQLOC)
            subject_loc = (SSeqLoc*) subject_loc_wrap->ptr;
        x_RemapAlignmentCoordinates(hit_align, &query, subject_loc);
        seq_aligns->Set().push_back(hit_align);
    }
    return seq_aligns;
}

TSeqAlignVector
BLAST_Results2CSeqAlign(const BlastHSPResults* results,
        EProgram prog,
        TSeqLocVector &query,
        const BlastSeqSrc* seq_src,
        bool is_gapped, bool is_ooframe)
{
    ASSERT(results->num_queries == (int)query.size());
    ASSERT(seq_src);

    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;

    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set> seq_aligns(BLAST_HitList2CSeqAlign(hit_list, prog,
                                           query[index], seq_src,
                                           is_gapped, is_ooframe));

       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size()
              << " seqaligns");

    }

    return retval;
}

TSeqAlignVector
BLAST_OneSubjectResults2CSeqAlign(const BlastHSPResults* results,
        EProgram prog,
        TSeqLocVector &query,
        const BlastSeqSrc* seq_src,
        Int4 subject_index,
        bool is_gapped, bool is_ooframe)
{
    ASSERT(results->num_queries == (int)query.size());
    ASSERT(seq_src);

    TSeqAlignVector retval;
    CConstRef<CSeq_id> subject_id;
    CConstRef<CSeq_id> query_id;
    CSeq_id* sid = NULL;
    CSeq_id* qid = NULL;
    TSeqPos subj_length = 0;
    TSeqPos query_length = 0;

    // Subject is the same for all queries, so retrieve its id right away
    x_GetSequenceLengthAndId(NULL, seq_src, subject_index,
                             &sid, &subj_length);
    subject_id.Reset(sid);


    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
        x_GetSequenceLengthAndId(&query[index], NULL, 0, &qid, &query_length);
        query_id.Reset(qid);
        CRef<CSeq_align_set> seq_aligns;
        BlastHitList* hit_list = results->hitlist_array[index];
        BlastHSPList* hsp_list = NULL;
        // Find the HSP list corresponding to this subject, if it exists
        if (hit_list) {
            int result_index;
            for (result_index = 0; result_index < hit_list->hsplist_count;
                 ++result_index) {
                hsp_list = hit_list->hsplist_array[result_index];
                if (hsp_list->oid == subject_index)
                    break;
            }
        }

        if (hsp_list) {
            CRef<CSeq_align> hit_align;
            if (is_gapped) {
                hit_align =
                    BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                           subject_id, is_ooframe);
            } else {
                hit_align =
                    BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                        subject_id, query_length, subj_length);
            }
            ListNode* subject_loc_wrap =
                BLASTSeqSrcGetSeqLoc(seq_src, (void*)&hsp_list->oid);
            SSeqLoc* subject_loc = NULL;
            if (subject_loc_wrap &&
                subject_loc_wrap->choice == BLAST_SEQSRC_CPP_SEQLOC)
                subject_loc = (SSeqLoc*) subject_loc_wrap->ptr;
            x_RemapAlignmentCoordinates(hit_align, &query[index],
                                        subject_loc);
            ListNodeFree(subject_loc_wrap);
            seq_aligns.Reset(new CSeq_align_set());
            seq_aligns->Set().push_back(hit_align);
        } else {
            seq_aligns.Reset(x_CreateEmptySeq_align_set(NULL));
        }
        retval.push_back(seq_aligns);
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/06/15 22:14:23  dondosha
* Correction in RemapToLoc call for subject Seq-loc on a minus strand
*
* Revision 1.4  2004/06/14 17:46:39  madden
* Use CSeqVector_CI and call SetRandomizeAmbiguities to properly handle gaps in subject sequence
*
* Revision 1.3  2004/06/07 21:34:55  dondosha
* Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
*
* Revision 1.2  2004/06/07 18:26:29  dondosha
* Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
*
* Revision 1.1  2004/06/02 16:00:59  bealer
* - New file for objmgr dependent code.
*
* ===========================================================================
*/
