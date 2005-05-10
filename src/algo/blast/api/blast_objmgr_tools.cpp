#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

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
 * ===========================================================================
 *
 * Author:  Christiam Camacho / Kevin Bealer
 *
 */

/// @file blast_objmgr_tools.cpp
/// Functions in xblast API code that interact with object manager.

#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/core/blast_setup.h>

#include <serial/iterator.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)


CSeq_align_set*
x_CreateEmptySeq_align_set(CSeq_align_set* sas);

CRef<CSeq_align>
BLASTUngappedHspListToSeqAlign(EBlastProgramType program, BlastHSPList* hsp_list, 
                               const CSeq_id *query_id, 
                               const CSeq_id *subject_id, Int4 query_length, 
                               Int4 subject_length);

CRef<CSeq_align>
BLASTHspListToSeqAlign(EBlastProgramType program, BlastHSPList* hsp_list, 
                       const CSeq_id *query_id, const CSeq_id *subject_id,
                       Int4 query_length, Int4 subject_length,
                       bool is_ooframe);

/** Set field values for one element of the context array of a
 * concatenated query.  All previous contexts should have already been
 * assigned correct values.
 * @param qinfo  Query info structure containing contexts. [in/out]
 * @param index  Index of the context to fill. [in]
 * @param length Length of this context. [in]
 * @param prog   Program type of this search. [in]
 */
static void
QueryInfo_SetContext(BlastQueryInfo*   qinfo,
                     Uint4             index,
                     Uint4             length,
                     EBlastProgramType prog)
{
    qinfo->contexts[index].frame = BLAST_ContextToFrame(prog, index);
    ASSERT(qinfo->contexts[index].frame != 127);
    
    qinfo->contexts[index].query_index =
        Blast_GetQueryIndexFromContext(index, prog);
    ASSERT(qinfo->contexts[index].query_index != -1);
    
    if (index) {
        Uint4 prev_loc = qinfo->contexts[index-1].query_offset;
        Uint4 prev_len = qinfo->contexts[index-1].query_length;
        
        Uint4 shift = prev_len ? prev_len + 1 : 0;
        
        qinfo->contexts[index].query_offset = prev_loc + shift;
        qinfo->contexts[index].query_length = length;
    } else {
        // First context
        qinfo->contexts[0].query_offset = 0;
        qinfo->contexts[0].query_length = length;
    }
}

void
SetupQueryInfo(const TSeqLocVector& queries, const CBlastOptions& options, 
               BlastQueryInfo** qinfo)
{
    ASSERT(qinfo);
    BlastQueryInfo* query_info = *qinfo = NULL;

    // Allocate and initialize the query info structure
    if ( !(query_info = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query info");
    }

    EBlastProgramType prog = options.GetProgramType();
    unsigned int nframes = GetNumberOfFrames(prog);
    query_info->num_queries = static_cast<int>(queries.size());
    query_info->first_context = 0;
    query_info->last_context = query_info->num_queries * nframes - 1;

    query_info->contexts =
        (BlastContextInfo*) calloc(query_info->last_context + 1, 
                                   sizeof(BlastContextInfo));
    
    if ( !query_info->contexts ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Context offsets array");
    }
    
    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
    bool translate = 
        ((prog == eBlastTypeBlastx) || (prog == eBlastTypeTblastx) || 
         (prog == eBlastTypeRpsTblastn));

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
                query_info->first_context = 3;
            } else {
                query_info->first_context = 1;
            }
        }
    }

    // Set up the context offsets into the sequence that will be added
    // to the sequence block structure.
    unsigned int ctx_index = 0;      // index into context_offsets array
    // Longest query length, to be saved in the query info structure
    Uint4 max_length = 0;

    ITERATE(TSeqLocVector, itr, queries) {
        TSeqPos length = 0;
        try { length = sequence::GetLength(*itr->seqloc, itr->scope); }
        catch (const CException&) { 
            // Ignore exceptions in this function as they will be caught in
            // SetupQueries
        }

        strand = sequence::GetStrand(*itr->seqloc, itr->scope);
        if (strand_opt == eNa_strand_minus || strand_opt == eNa_strand_plus) {
            strand = strand_opt;
        }

        if (translate) {
            for (unsigned int i = 0; i < nframes; i++) {
                unsigned int prot_length = 
                    (length == 0 ? 0 : 
                     (length - i % CODON_LENGTH) / CODON_LENGTH);
                max_length = MAX(max_length, prot_length);
                
                Uint4 ctx_len(0);
                
                switch (strand) {
                case eNa_strand_plus:
                    ctx_len = (i<3) ? prot_length : 0;
                    QueryInfo_SetContext(query_info, ctx_index + i, ctx_len, 
                                         prog);
                    break;

                case eNa_strand_minus:
                    ctx_len = (i<3) ? 0 : prot_length;
                    QueryInfo_SetContext(query_info, ctx_index + i, ctx_len, 
                                         prog);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    QueryInfo_SetContext(query_info, ctx_index + i, 
                                         prot_length, prog);
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
                    QueryInfo_SetContext(query_info, ctx_index, length,
                                         prog);
                    QueryInfo_SetContext(query_info, ctx_index+1, 0, prog);
                    break;

                case eNa_strand_minus:
                    QueryInfo_SetContext(query_info, ctx_index, 0, prog);
                    QueryInfo_SetContext(query_info, ctx_index+1, length,
                                         prog);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    QueryInfo_SetContext(query_info, ctx_index, length,
                                         prog);
                    QueryInfo_SetContext(query_info, ctx_index+1, length,
                                         prog);
                    break;

                default:
                    abort();
                }
            } else {    // protein
                QueryInfo_SetContext(query_info, ctx_index, length, prog);
            }
        }
        ctx_index += nframes;
    }
    query_info->max_length = max_length;
    *qinfo = query_info;
}

/// Compresses sequence data on vector to buffer, which should have been
/// allocated and have the right size.
void CompressDNA(const CSeqVector& vec, Uint1* buffer, const int buflen)
{
    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence
    CSeqVector_CI iter(vec);

    iter.SetRandomizeAmbiguities();
    iter.SetCoding(CSeq_data::e_Ncbi2na);


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
             const CBlastQueryInfo& qinfo, BLAST_SequenceBlk** seqblk,
             Blast_Message** blast_msg)
{
    ASSERT(seqblk);
    ASSERT(blast_msg);
    ASSERT(queries.size() != 0);

    EBlastProgramType prog = options.GetProgramType();

    // Determine sequence encoding
    Uint1 encoding = GetQueryEncoding(prog);
    
    int buflen = QueryInfo_GetSeqBufLen(qinfo);
    TAutoUint1Ptr buf((Uint1*) calloc(buflen+1, sizeof(Uint1)));
    
    if ( !buf ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence buffer");
    }

    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
    bool translate = 
       ((prog == eBlastTypeBlastx) || (prog == eBlastTypeTblastx) || 
        (prog == eBlastTypeRpsTblastn));

    unsigned int ctx_index = 0;      // index into context_offsets array
    unsigned int nframes = GetNumberOfFrames(prog);

    CBlastMaskLoc mask(BlastMaskLocNew(qinfo->num_queries));

    // Unless the strand option is set to single strand, the actual
    // CSeq_locs dictacte which strand to examine during the search
    ENa_strand strand_opt = options.GetStrandOption();
    int index = 0;
    string error_string;

    // to keep track of the query position in its vector for error reporting
    int query_num = 0;  
    ITERATE(TSeqLocVector, itr, queries) {

        try {
            query_num++;
            ENa_strand strand;
            BlastSeqLoc* bsl_tmp=NULL;

            if ((is_na || translate) &&
                (strand_opt == eNa_strand_unknown || 
                 strand_opt == eNa_strand_both)) 
            {
                strand = sequence::GetStrand(*itr->seqloc, itr->scope);
            } else {
                strand = strand_opt;
            }

            bsl_tmp = CSeqLoc2BlastSeqLoc(itr->mask);

            BlastSeqLoc_RestrictToInterval(&bsl_tmp, itr->seqloc->GetStart(eExtreme_Positional), 
                                           itr->seqloc->GetStop(eExtreme_Positional));

            SBlastSequence sequence;

            if (translate) {
                ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                // Get both strands of the original nucleotide sequence with
                // sentinels
                sequence = GetSequence(*itr->seqloc, encoding, itr->scope, 
                                       strand, eSentinels);

                TAutoUint1ArrayPtr gc = 
                    FindGeneticCode(options.GetQueryGeneticCode());
                int na_length = sequence::GetLength(*itr->seqloc, itr->scope);
                Uint1* seqbuf_rev = NULL;  // negative strand
                if (strand == eNa_strand_both)
                   seqbuf_rev = sequence.data.get() + na_length + 1;
                else if (strand == eNa_strand_minus)
                   seqbuf_rev = sequence.data.get();

                // Populate the sequence buffer
                for (unsigned int i = 0; i < nframes; i++) {
                    if (qinfo->contexts[i].query_length <= 0) {
                        continue;
                    }
                    
                    int offset = qinfo->contexts[ctx_index + i].query_offset;

                    // The BlastContextInfo structure has a "frame" field, but
                    // that field is set from the program type, and the value
                    // we want here is the value for blastx, not the actual
                    // program type (why?).  Perhaps there ought to be two
                    // frame values...  Further investigation and discussion
                    // indicates that BLAST_ContextToFrame should do this
                    // internally (this change will be made soon).

                    //short frame = qinfo->contexts[ctx_index + i].frame;
                    short frame = BLAST_ContextToFrame(eBlastTypeBlastx, i);

                    BLAST_GetTranslation(sequence.data.get() + 1,
                                         seqbuf_rev,
                                         na_length,
                                         frame,
                                         & buf.get()[offset],
                                         gc.get());
                }

            } else if (is_na) {

                ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                sequence = GetSequence(*itr->seqloc, encoding, itr->scope, 
                                       strand, eSentinels);
                int idx = (strand == eNa_strand_minus) ? 
                    ctx_index + 1 : ctx_index;

                int offset = qinfo->contexts[idx].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);

            } else {

                string warnings;
                sequence = GetSequence(*itr->seqloc, encoding, itr->scope,
                                     eNa_strand_unknown, eSentinels, &warnings);
                int offset = qinfo->contexts[ctx_index].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);
                if ( !warnings.empty() ) {
                    error_string += warnings + " ";
                }
            }

            mask->seqloc_array[index] = bsl_tmp;
            ++index;
            ctx_index += nframes;
        } catch (const CException& e) {
            error_string += 
                "Query number " + NStr::IntToString(query_num) + ": ";
            error_string += e.ReportThis(eDPF_ErrCodeExplanation) + " ";
        }
    }

    if (error_string.size() != 0) {
        Blast_MessageWrite(blast_msg, BLAST_SEV_WARNING, 0, 0,
                           error_string.c_str());
    }

    // Translate the lower case mask coordinates, if it is a translated search
    if (translate)
        BlastMaskLocDNAToProtein(mask, qinfo);

    if (BlastSeqBlkNew(seqblk) < 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence block");
    }

    BlastSeqBlkSetSequence(*seqblk, buf.release(), buflen - 2);

    (*seqblk)->lcase_mask = mask.Release();
    (*seqblk)->lcase_mask_allocated = TRUE;
}

void
SetupSubjects(const TSeqLocVector& subjects, 
              EBlastProgramType prog,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    ASSERT(seqblk_vec);
    ASSERT(max_subjlen);
    ASSERT(subjects.size() != 0);

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
    bool subj_is_na = (prog == eBlastTypeBlastn  ||
                       prog == eBlastTypeTblastn ||
                       prog == eBlastTypeTblastx);

    ESentinelType sentinels = eSentinels;
    if (prog == eBlastTypeTblastn || prog == eBlastTypeTblastx) {
        sentinels = eNoSentinels;
    }

    Uint1 encoding = GetSubjectEncoding(prog);
       
    // TODO: Should strand selection on the subject sequences be allowed?
    //ENa_strand strand = options->GetStrandOption(); 
    int index = 0; // Needed for lower case masks only.

    *max_subjlen = 0;

    ITERATE(TSeqLocVector, itr, subjects) {
        BLAST_SequenceBlk* subj = NULL;

        SBlastSequence sequence = GetSequence(*itr->seqloc, encoding, 
                                              itr->scope, eNa_strand_plus, 
                                              sentinels);

        if (BlastSeqBlkNew(&subj) < 0) {
            NCBI_THROW(CBlastException, eOutOfMemory, "Subject sequence block");
        }

        /* Set the lower case mask, if it exists */
        if (subj->lcase_mask)  /*FIXME?? */
            subj->lcase_mask->seqloc_array[index] = CSeqLoc2BlastSeqLoc(itr->mask);
        ++index;

        if (subj_is_na) {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
               ((sentinels == eSentinels) ? sequence.length - 2 :
                sequence.length));

            try {
                // Get the compressed sequence
                SBlastSequence compressed_seq = 
                    GetSequence(*itr->seqloc, eBlastEncodingNcbi2na, itr->scope,
                                 eNa_strand_plus, eNoSentinels);
                BlastSeqBlkSetCompressedSequence(subj, 
                                                 compressed_seq.data.release());
            } catch (const CSeqVectorException& sve) {
                BlastSequenceBlkFree(subj);
                NCBI_THROW(CBlastException, eInternal, sve.what());
            }
        } else {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
                                   sequence.length - 2);
        }

        seqblk_vec->push_back(subj);
        (*max_subjlen) = MAX((*max_subjlen),
                sequence::GetLength(*itr->seqloc, itr->scope));

    }
}

/// Tests if a number represents a valid residue
/// @param res Value to test [in]
/// @return TRUE if value is a valid residue ( < 26)
static bool s_IsValidResidue(Uint1 res) { return res < 26; }

SBlastSequence
GetSequence(const objects::CSeq_loc& sl, Uint1 encoding, 
            objects::CScope* scope,
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings) 
{
    Uint1* buf = NULL;          // buffer to write sequence
    Uint1* buf_var = NULL;      // temporary pointer to buffer
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    TAutoUint1Ptr safe_buf;     // contains buf to ensure exception safety

    // Retrieves the correct strand (plus or minus), but not both
    CSeqVector sv(sl, *scope);

    switch (encoding) {
    // Protein sequences (query & subject) always have sentinels around sequence
    case eBlastEncodingProtein:
    {
        vector<TSeqPos> replaced_selenocysteins; // Selenocystein residue positions
        vector<TSeqPos> invalid_residues;        // Invalid residue positions
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        buflen = CalculateSeqBufferLength(sv.size(), eBlastEncodingProtein);
        if (buflen == 0) {
            break;
        }
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        *buf_var++ = GetSentinelByte(encoding);
        for (i = 0; i < sv.size(); i++) {
            // Change Selenocysteine to X
            if (sv[i] == AMINOACID_TO_NCBISTDAA[(int)'U']) {
                replaced_selenocysteins.push_back(i);
                *buf_var++ = AMINOACID_TO_NCBISTDAA[(int)'X'];
            } else if (!s_IsValidResidue(sv[i])) {
                invalid_residues.push_back(i);
            } else {
                *buf_var++ = sv[i];
            }
        }
        if (invalid_residues.size() > 0) {
            string error("Invalid residues found at positions ");
            error += NStr::IntToString(invalid_residues[0]);
            for (i = 1; i < invalid_residues.size(); i++) {
                error += ", " + NStr::IntToString(invalid_residues[i]);
            }
            NCBI_THROW(CBlastException, eInvalidCharacter, error);
        }

        *buf_var++ = GetSentinelByte(encoding);
        if (warnings && replaced_selenocysteins.size() > 0) {
            *warnings += "Selenocysteine (U) replaced by X at positions ";
            *warnings += NStr::IntToString(replaced_selenocysteins[0]);
            for (i = 1; i < replaced_selenocysteins.size(); i++) {
                *warnings += ", " + NStr::IntToString(replaced_selenocysteins[i]);
            }
        }
        break;
    }

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide: // Used for nucleotide blastn queries
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        buflen = CalculateSeqBufferLength(sv.size(), eBlastEncodingNcbi4na,
                                          strand, sentinel);
        if (buflen == 0) {
            break;
        }
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        if (sentinel == eSentinels)
            *buf_var++ = GetSentinelByte(encoding);

        if (encoding == eBlastEncodingNucleotide) {
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
            sv = CSeqVector(sl, *scope,
                            CBioseq_Handle::eCoding_Ncbi, 
                            eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi4na);
            if (encoding == eBlastEncodingNucleotide) {
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
    case eBlastEncodingNcbi2na:
        ASSERT(sentinel == eNoSentinels);
        sv.SetCoding(CSeq_data::e_Ncbi2na);
        buflen = CalculateSeqBufferLength(sv.size(), sv.GetCoding(),
                                          eNa_strand_plus, eNoSentinels);
        if (buflen == 0) {
            break;
        }
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        buf = (Uint1*) malloc(sizeof(Uint1)*buflen);
        safe_buf.reset(buf);
        CompressDNA(sv, buf, buflen);
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Invalid encoding");
    }

    return SBlastSequence(safe_buf.release(), buflen);
}

/** Retrieves subject sequence Seq-id and length.
 * @param seqinfo_src Source of subject sequences information [in]
 * @param oid Ordinal id (index) of the subject sequence [in]
 * @param seqid Subject sequence identifier to fill [out]
 * @param length Subject sequence length [out]
 */
static void
x_GetSequenceLengthAndId(const IBlastSeqInfoSrc* seqinfo_src, // [in]
                         int oid,                    // [in] 
                         CConstRef<CSeq_id>& seqid,  // [out]
                         TSeqPos* length)            // [out]
{
    ASSERT(length);
    list<CRef<CSeq_id> > seqid_list = seqinfo_src->GetId(oid);

    seqid.Reset(seqid_list.front());
    *length = seqinfo_src->GetLength(oid);

    return;
}

/// Remaps Seq-align offsets relative to the query Seq-loc. 
/// Since the query strands were already taken into account when CSeq_align 
/// was created, only start position shifts in the CSeq_loc's are relevant in 
/// this function. 
/// @param sar Seq-align for a given query [in] [out]
/// @param query The query Seq-loc [in]
static void
x_RemapToQueryLoc(CRef<CSeq_align> sar, const SSeqLoc* query)
{
    _ASSERT(sar);
    ASSERT(query);
    const int query_dimension = 0;

    TSeqPos q_shift = 0;

    if (query->seqloc->IsInt()) {
        q_shift = query->seqloc->GetInt().GetFrom();
    }
    if (q_shift > 0) {
        for (CTypeIterator<CDense_seg> itr(Begin(*sar)); itr; ++itr) {
            const vector<ENa_strand> strands = itr->GetStrands();
            // Create temporary CSeq_locs with strands either matching 
            // (for query and for subject if it is not on a minus strand),
            // or opposite to those in the segment, to force RemapToLoc to 
            // behave in the correct way.
            CSeq_loc q_seqloc;
            ENa_strand q_strand = strands[0];
            q_seqloc.SetInt().SetFrom(q_shift);
            q_seqloc.SetInt().SetTo(query->seqloc->GetInt().GetTo());
            q_seqloc.SetInt().SetStrand(q_strand);
            q_seqloc.SetInt().SetId().Assign(sequence::GetId(*query->seqloc, 
                                                             query->scope));
            itr->RemapToLoc(query_dimension, q_seqloc, true);
        }
    }
}

void
Blast_RemapToSubjectLoc(TSeqAlignVector& seqalignv, 
                        const TSeqLocVector& subjectv)
{
    const int subject_dimension = 1;

    // Elements of the TSeqAlignVector correspond to different queries.
    ITERATE(TSeqAlignVector, q_itr, seqalignv) {
        Uint4 index = 0;
        // Iterate over subjects. It is expected that Seq-aligns for all
        // subjects must be present in the list, albeit some of them might
        // be empty. Check that the list size is the same as the subjects 
        // vector size.
        if ((*q_itr)->Get().size() != subjectv.size()) {
            NCBI_THROW(CBlastException, eInternal, 
                "List of Seq-aligns does not correspond to subjects vector");
        }
        ITERATE(list< CRef<CSeq_align> >, s_itr, (*q_itr)->Get()) { 
            // If subject is on a minus strand, we'll need to flip subject 
            // strands and remap subject coordinates on all segments.
            // Otherwise we only need to shift subject coordinates, 
            // if the respective location starts not from 0.
            bool reverse =
                (subjectv[index].seqloc->IsInt() &&
                 subjectv[index].seqloc->GetInt().CanGetStrand() &&
                 subjectv[index].seqloc->GetInt().GetStrand() == eNa_strand_minus);
            
            TSeqPos s_shift = 0;
            
            if (subjectv[index].seqloc->IsInt())
                s_shift = subjectv[index].seqloc->GetInt().GetFrom();
            
            // Nothing needs to be done if subject location is on forward strand 
            // and starts from the beginning of sequence.
            if (!reverse && s_shift == 0) {
                ++index;
                continue;
            }

            CRef<CSeq_align> sar(*s_itr);
            for (CTypeIterator<CDense_seg> seg_itr(Begin(*sar)); seg_itr; 
                 ++seg_itr) {

                const vector<ENa_strand> strands = seg_itr->GetStrands();
                // Create temporary CSeq_loc with strand matching the segment 
                // strand if subject location is on forward strand,
                // or opposite if subject is on reverse strand, to force 
                // RemapToLoc to behave correctly.
                CSeq_loc s_seqloc;
                ENa_strand s_strand;
                if (reverse) {
                    s_strand = ((strands[1] == eNa_strand_plus) ?
                                eNa_strand_minus : eNa_strand_plus);
                } else {
                    s_strand = strands[1];
                }
                s_seqloc.SetInt().SetFrom(s_shift);
                s_seqloc.SetInt().SetTo(subjectv[index].seqloc->GetInt().GetTo());
                s_seqloc.SetInt().SetStrand(s_strand);
                s_seqloc.SetInt().SetId().Assign(
                    sequence::GetId(*subjectv[index].seqloc, 
                                    subjectv[index].scope));
                seg_itr->RemapToLoc(subject_dimension, s_seqloc, !reverse);
            } // Loop on Dense-segs
        } // One-iteration loop over subjects
    } // Loop over queries
}


/// Converts BlastHitList containing alignments for a single query to database
/// sequences into a CSeq_align_set
/// @param hit_list List of alignments [in]
/// @param prog Program type [in]
/// @param query Query sequence blast::SSeqLoc [in]
/// @param seqinfo_src abstraction to obtain sequence information [in]
/// @param is_gapped true if this is a gapped alignment [in]
/// @param is_ooframe true if this is an out-of-frame alignment [in]
/// @return empty CSeq_align_set if hit_list is NULL, otherwise conversion from
/// BlastHitList to CSeq_align_set
CSeq_align_set*
BLAST_HitList2CSeqAlign(const BlastHitList* hit_list,
    EBlastProgramType prog, SSeqLoc &query,
    const IBlastSeqInfoSrc* seqinfo_src, bool is_gapped, bool is_ooframe)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();

    if (!hit_list) {
        return x_CreateEmptySeq_align_set(seq_aligns);
    }

    TSeqPos query_length = sequence::GetLength(*query.seqloc, query.scope);
    CConstRef<CSeq_id> query_id(&sequence::GetId(*query.seqloc, query.scope));

    TSeqPos subj_length = 0;
    CConstRef<CSeq_id> subject_id;

    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;

        // Sort HSPs with e-values as first priority and scores as 
        // tie-breakers, since that is the order we want to see them in 
        // in Seq-aligns.
        Blast_HSPListSortByEvalue(hsp_list);

        x_GetSequenceLengthAndId(seqinfo_src, hsp_list->oid,
                                 subject_id, &subj_length);

        // Create a CSeq_align for each matching sequence
        CRef<CSeq_align> hit_align;
        if (is_gapped) {
            hit_align =
                BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                       subject_id, query_length, subj_length,
                                       is_ooframe);
        } else {
            hit_align =
                BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                    subject_id, query_length, subj_length);
        }
        x_RemapToQueryLoc(hit_align, &query);
        seq_aligns->Set().push_back(hit_align);
    }
    return seq_aligns;
}

TSeqAlignVector
BLAST_Results2CSeqAlign(const BlastHSPResults* results,
        EBlastProgramType prog,
        TSeqLocVector &query,
        const IBlastSeqInfoSrc* seqinfo_src,
        bool is_gapped, bool is_ooframe)
{
    ASSERT(results->num_queries == (int)query.size());

    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;

    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set> seq_aligns(BLAST_HitList2CSeqAlign(hit_list, prog,
                                           query[index], seqinfo_src,
                                           is_gapped, is_ooframe));

       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size()
              << " seqaligns");

    }

    return retval;
}

TSeqAlignVector
BLAST_OneSubjectResults2CSeqAlign(const BlastHSPResults* results,
                                  EBlastProgramType prog, TSeqLocVector &query, 
                                  const IBlastSeqInfoSrc* seqinfo_src,
                                  Uint4 subject_index, bool is_gapped, 
                                  bool is_ooframe)
{
    ASSERT(results->num_queries == (int)query.size());

    TSeqAlignVector retval;
    CConstRef<CSeq_id> subject_id;
    TSeqPos subj_length = 0;

    // Subject is the same for all queries, so retrieve its id right away
    x_GetSequenceLengthAndId(seqinfo_src, subject_index, subject_id, &subj_length);

    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
        CRef<CSeq_align_set> seq_aligns;
        BlastHitList* hit_list = results->hitlist_array[index];
        BlastHSPList* hsp_list = NULL;

        // Find the HSP list corresponding to this subject, if it exists
        if (hit_list) {
            int result_index;
            for (result_index = 0; result_index < hit_list->hsplist_count;
                 ++result_index) {
                hsp_list = hit_list->hsplist_array[result_index];
                if (hsp_list->oid == (Int4)subject_index)
                    break;
            }
            /* If hsp_list for this subject is not found, set it to NULL */
            if (result_index == hit_list->hsplist_count)
                hsp_list = NULL;
        }

        if (hsp_list) {
            // Sort HSPs with e-values as first priority and scores as 
            // tie-breakers, since that is the order we want to see them in 
            // in Seq-aligns.
            Blast_HSPListSortByEvalue(hsp_list);

            CRef<CSeq_align> hit_align;
            CConstRef<CSeq_id> 
                query_id(&sequence::GetId(*query[index].seqloc, 
                                          query[index].scope));

            TSeqPos query_length = 
                sequence::GetLength(*query[index].seqloc, 
                                    query[index].scope);
            if (is_gapped) {
                hit_align =
                    BLASTHspListToSeqAlign(prog, hsp_list, query_id,
                                           subject_id, query_length, 
                                           subj_length, is_ooframe);
            } else {
                hit_align =
                    BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id,
                        subject_id, query_length, subj_length);
            }
            x_RemapToQueryLoc(hit_align, &query[index]);
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
* Revision 1.43  2005/05/10 16:08:39  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.42  2005/04/11 19:26:32  madden
* Remove extra EBlastProgramType variable
*
* Revision 1.41  2005/04/06 21:04:55  dondosha
* GapEditBlock structure removed, use BlastHSP which contains GapEditScript
*
* Revision 1.40  2005/03/31 16:15:03  dondosha
* Some doxygen fixes
*
* Revision 1.39  2005/03/07 20:55:23  dondosha
* Detect invalid residues in GetSequence and throw exception: this should never happen in normal circumstances because invalid residues are skipped by ReadFasta API
*
* Revision 1.38  2005/03/04 16:53:27  camacho
* more doxygen fixes
*
* Revision 1.37  2005/03/04 16:07:05  camacho
* doxygen fixes
*
* Revision 1.36  2005/03/01 20:52:42  camacho
* Doxygen fixes
*
* Revision 1.35  2005/02/18 15:06:02  shomrat
* CSeq_loc interface changes
*
* Revision 1.34  2005/01/27 18:30:24  camacho
* Remove unused variable
*
* Revision 1.33  2005/01/21 17:38:57  camacho
* Handle zero-length sequences
*
* Revision 1.32  2005/01/10 18:35:07  dondosha
* BlastMaskLocDNAToProtein and BlastMaskLocProteinToDNA moved to core with changed signatures
*
* Revision 1.31  2005/01/06 16:08:09  camacho
* + warnings output parameter to blast::GetSequence
*
* Revision 1.30  2005/01/06 15:41:35  camacho
* Add Blast_Message output parameter to SetupQueries
*
* Revision 1.29  2004/12/28 17:08:26  camacho
* fix compiler warning
*
* Revision 1.28  2004/12/28 16:47:43  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Remove exception specification from blast::SetupQueries
* 3. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.27  2004/12/06 17:54:09  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.26  2004/12/02 16:01:24  bealer
* - Change multiple-arrays to array-of-struct in BlastQueryInfo
*
* Revision 1.25  2004/11/30 16:59:16  dondosha
* Call BlastSeqLoc_RestrictToInterval to adjust lower case mask offsets when query restricted to interval
*
* Revision 1.24  2004/11/29 18:47:22  madden
* Fix for SetupQueryInfo for zero-length translated nucl. sequence
*
* Revision 1.23  2004/10/19 16:09:39  dondosha
* Sort HSPs by e-value before converting results to Seq-align, since they are sorted by score coming in.
*
* Revision 1.22  2004/10/06 18:42:08  dondosha
* Cosmetic fix for SunOS compiler warning
*
* Revision 1.21  2004/10/06 14:55:49  dondosha
* Use IBlastSeqInfoSrc interface to get ids and lengths of subject sequences
*
* Revision 1.20  2004/09/21 13:50:19  dondosha
* Translate query in 6 frames for RPS tblastn
*
* Revision 1.19  2004/09/13 15:55:04  madden
* Remove unused parameter from CSeqLoc2BlastSeqLoc
*
* Revision 1.18  2004/09/13 12:47:06  madden
* Changes for redefinition of BlastSeqLoc and BlastMaskLoc
*
* Revision 1.17  2004/09/08 20:10:35  dondosha
* Fix for searches against multiple subjects when some query-subject pairs have no hits
*
* Revision 1.16  2004/08/17 21:49:36  dondosha
* Removed unused variable
*
* Revision 1.15  2004/08/16 19:49:25  dondosha
* Small memory leak fix; catch CSeqVectorException and throw CBlastException out of setup functions
*
* Revision 1.14  2004/07/20 21:11:04  dondosha
* Fixed a memory leak in x_GetSequenceLengthAndId
*
* Revision 1.13  2004/07/19 21:04:20  dondosha
* Added back case when BlastSeqSrc returns a wrong type Seq-id - then retrieve a seqid string and construct CSeq_id from it
*
* Revision 1.12  2004/07/19 14:58:47  dondosha
* Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
*
* Revision 1.11  2004/07/19 13:56:02  dondosha
* Pass subject SSeqLoc directly to BLAST_OneSubjectResults2CSeqAlign instead of BlastSeqSrc
*
* Revision 1.10  2004/07/15 14:50:09  madden
* removed commented out ASSERT
*
* Revision 1.9  2004/07/06 15:48:40  dondosha
* Use EBlastProgramType enumeration type instead of EProgram when calling C code
*
* Revision 1.8  2004/06/23 14:05:34  dondosha
* Changed CSeq_loc argument in CSeqLoc2BlastMaskLoc to pointer; fixed a memory leak in x_GetSequenceLengthAndId
*
* Revision 1.7  2004/06/22 16:45:14  camacho
* Changed the blast_type_* definitions for the TBlastProgramType enumeration.
*
* Revision 1.6  2004/06/21 15:28:16  jcherry
* Guard against trying to access unset strand of a Seq-interval
*
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
