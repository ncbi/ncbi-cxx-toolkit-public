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
* Author:  Christiam Camacho
*
* File Description:
*   Auxiliary setup functions for Blast objects interface
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbifile.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIstdaa.hpp>

#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_stat.h>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(ncbi::objects);

/// Now allows query concatenation
void
SetupQueryInfo(const TSeqLocVector& queries, const CBlastOptions& options, 
               BlastQueryInfo** qinfo)
{
    ASSERT(qinfo);

    // Allocate and initialize the query info structure
    // implement assignment operator for wrappers?
#if 0
    qinfo->Reset((BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo)));
    if ( !(qinfo->operator->()) ) { // FIXME!
        NCBI_THROW(CBlastException, eOutOfMemory, "Query info");
    }
#endif
    if ( !((*qinfo) = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query info");
    }

    EProgram prog = options.GetProgram();
    unsigned int nframes = GetNumberOfFrames(prog);
    (*qinfo)->num_queries = static_cast<int>(queries.size());
    (*qinfo)->first_context = 0;
    (*qinfo)->last_context = (*qinfo)->num_queries * nframes - 1;

    bool is_na = (prog == eBlastn) ? true : false;
    bool translate = ((prog == eBlastx) || (prog == eTblastx)) ? true : false;

#if 0
    // Adjust first/last context depending on (first?) query strand
    // is this really needed? (for kbp assignment in getting dropoff params)
    // This is inconsistent, as the contexts in the middle of the
    // context_offsets array are ignored
    if (m_tQueries.front().IsInt()) {
        if (m_tQueries.front().GetInt().GetStrand() == eNa_strand_minus) {
            if (translate) {
                mi_QueryInfo->first_context = 3;
            } else {
                mi_QueryInfo->last_context = 1;
            }
        } else if (m_tQueries.front().GetInt().GetStrand() = eNa_strand_plus) {
            if (translate) {
                mi_QueryInfo->last_context -= 3;
            } else {
                mi_QueryInfo->last_context -= 1;
            }
        }
    }
#endif

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

    // Set up the context offsets into the sequence that will be added to the
    // sequence block structure.
    unsigned int ctx_index = 0;      // index into context_offsets array
    ITERATE(TSeqLocVector, itr, queries) {
        TSeqPos length = sequence::GetLength(*itr->seqloc, itr->scope);
        ASSERT(length != numeric_limits<TSeqPos>::max());

        // Unless the strand option is set to single strand, the actual
        // CSeq_locs dictacte which strand to examine during the search
        ENa_strand strand_opt = options.GetStrandOption();
        ENa_strand strand = sequence::GetStrand(*itr->seqloc, itr->scope);
        if (strand_opt == eNa_strand_minus || strand_opt == eNa_strand_plus) {
            strand = strand_opt;
        }

        if (translate) {
            for (unsigned int i = 0; i < nframes; i++) {
                unsigned int prot_length = 
                    (length - i % CODON_LENGTH) / CODON_LENGTH;
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
        } else if (is_na) {
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
            context_offsets[ctx_index + 1] = length + 1;
        }

        ctx_index += nframes;
    }

    (*qinfo)->total_length = context_offsets[ctx_index];
}

void
SetupQueries(const TSeqLocVector& queries, const CBlastOptions& options,
             const CBlastQueryInfo& qinfo, BLAST_SequenceBlk** seqblk)
{
    // Determine sequence encoding
    Uint1 encoding;
    EProgram prog = options.GetProgram();
    if (prog == eBlastn) {
        encoding = BLASTNA_ENCODING;
    } else if (prog == eBlastn ||
               prog == eBlastx ||
               prog == eTblastx) {
        encoding = NCBI4NA_ENCODING;
    } else {
        encoding = BLASTP_ENCODING;
    }

    int buflen = qinfo->total_length;
    Uint1* buf = (Uint1*) calloc((buflen+1), sizeof(Uint1));
    if ( !buf ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence buffer");
    }

    bool is_na = (prog == eBlastn) ? true : false;
    bool translate = ((prog == eBlastx) || (prog == eTblastx)) ? true : false;

    unsigned int ctx_index = 0;      // index into context_offsets array
    unsigned int nframes = GetNumberOfFrames(prog);
    ITERATE(TSeqLocVector, itr, queries) {

        Uint1* seqbuf = NULL;
        TSeqPos seqbuflen = 0;

        if (translate) {

            // Get both strands of the original nucleotide sequence with
            // sentinels
            //pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
            seqbuf =
                GetSequence(*itr->seqloc, encoding, itr->scope, &seqbuflen,
                            eNa_strand_both, true);

            AutoPtr<Uint1, ArrayDeleter<Uint1> > gc = 
                FindGeneticCode(options.GetQueryGeneticCode());
            int na_length = sequence::GetLength(*itr->seqloc, itr->scope);

            // Populate the sequence buffer
            for (unsigned int i = 0; i < nframes; i++) {
                if (BLAST_GetQueryLength(qinfo, i) == 0) {
                    continue;
                }

                int offset = qinfo->context_offsets[ctx_index + i];
                short frame = BLAST_ContextToFrame(prog, i);
                BLAST_GetTranslation(seqbuf + 1, 
                                     seqbuf + na_length + 1,
                                     na_length, frame, &buf[offset], gc.get());
            }
            sfree(seqbuf);

        } else if (is_na) {

            // Unless the strand option is set to single strand, the actual
            // CSeq_locs dictacte which strand to examine during the search
            ENa_strand strand_opt = options.GetStrandOption();
            ENa_strand strand = sequence::GetStrand(*itr->seqloc,
                                                    itr->scope);
            if (strand_opt == eNa_strand_minus || 
                strand_opt == eNa_strand_plus) {
                strand = strand_opt;
            }
            //pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
            seqbuf =
                GetSequence(*itr->seqloc, encoding, itr->scope, &seqbuflen,
                            strand, true);
            int index = (strand == eNa_strand_minus) ? 
                ctx_index + 1 : ctx_index;
            int offset = qinfo->context_offsets[index];
            memcpy(&buf[offset], seqbuf, seqbuflen);
            sfree(seqbuf);

        } else {

            //pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf(
            seqbuf =
                GetSequence(*itr->seqloc, encoding, itr->scope, &seqbuflen,
                            eNa_strand_unknown, true);
            int offset = qinfo->context_offsets[ctx_index];
            memcpy(&buf[offset], seqbuf, seqbuflen);
            sfree(seqbuf);

        }

        ctx_index += nframes;
    }

    (*seqblk) = (BLAST_SequenceBlk*) calloc(1, sizeof(BLAST_SequenceBlk));
    if ( !*seqblk ) {
        sfree(buf);
        NCBI_THROW(CBlastException, eOutOfMemory, "Query block structure");
    }
    // FIXME: is buflen calculated correctly here?
    BlastSetUp_SeqBlkNew(buf, buflen - 2, 0, seqblk, true);

    return;
}


void
SetupSubjects(const TSeqLocVector& subjects, 
              CBlastOptions* options,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen)
{
    ASSERT(options);
    ASSERT(seqblk_vec);
    ASSERT(max_subjlen);

    EProgram prog = options->GetProgram();
    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
    bool subj_is_na = (prog == eBlastn  ||
                       prog == eTblastn ||
                       prog == eTblastx);

    Uint1 encoding = (subj_is_na ? NCBI2NA_ENCODING : BLASTP_ENCODING);

    // TODO: Should strand selection on the subject sequences be allowed?
    //ENa_strand strand = options->GetStrandOption(); 
    Int8 dblength = 0;

    ITERATE(TSeqLocVector, itr, subjects) {
        BLAST_SequenceBlk* subj = (BLAST_SequenceBlk*) 
            calloc(1, sizeof(BLAST_SequenceBlk));

        Uint1* seqbuf = NULL;
        TSeqPos seqbuflen = 0;

        ENa_strand strand = sequence::GetStrand(*itr->seqloc, itr->scope);
        //pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> seqbuf( 
        seqbuf =
            GetSequence(*itr->seqloc, encoding, itr->scope, &seqbuflen, 
                        strand, false);

        if (subj_is_na) {
            subj->sequence = seqbuf;
            subj->sequence_allocated = TRUE;

            encoding = (prog == eBlastn) ? BLASTNA_ENCODING : NCBI4NA_ENCODING;
            bool use_sentinels = (prog == eBlastn) ?  true : false;

            // Retrieve the sequence with ambiguities
            //pair<AutoPtr<Uint1, CDeleter<Uint1> >, TSeqPos> sbuf2(
            seqbuf = GetSequence(*itr->seqloc, encoding, itr->scope,
                                 &seqbuflen, strand, use_sentinels);
            subj->sequence_start = seqbuf;
            subj->length = use_sentinels ? seqbuflen - 2 : seqbuflen;
            subj->sequence_start_allocated = TRUE;
        } else {
            subj->sequence_start_allocated = TRUE;
            subj->sequence_start = seqbuf;
            subj->sequence = seqbuf + 1;// skips the sentinel byte
            subj->length = seqbuflen - 2; // don't count the sentinel bytes
        }
        dblength += subj->length;
        seqblk_vec->push_back(subj);
        (*max_subjlen) = MAX((*max_subjlen),
                sequence::GetLength(*itr->seqloc, itr->scope));
    }
    options->SetDbSeqNum(seqblk_vec->size());
    options->SetDbLength(dblength);
}

#define LAST2BITS 0x03

// Compresses sequence data on vector to buffer, which should have been
// allocated and have the right size.
static void PackDNA(const CSeqVector& vec, Uint1* buffer, const int buflen)
{
    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence

    ASSERT(vec.GetCoding() == CSeq_data::e_Ncbi2na);

    for (ci = 0, i = 0; ci < (TSeqPos) buflen-1; ci++, i += COMPRESSION_RATIO) {
        buffer[ci] = ((vec[i+0] & LAST2BITS)<<6) |
                     ((vec[i+1] & LAST2BITS)<<4) |
                     ((vec[i+2] & LAST2BITS)<<2) |
                     ((vec[i+3] & LAST2BITS)<<0);
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
            buffer[ci] |= ((vec[i] & LAST2BITS)<<bit_shift);
    }
    buffer[ci] |= vec.size()%COMPRESSION_RATIO;    // Number of bases in the last byte.
   
}

//pair<Uint1*, TSeqPos>
Uint1*
GetSequence(const CSeq_loc& sl, Uint1 encoding, CScope* scope, TSeqPos* len,
            ENa_strand strand, bool add_nucl_sentinel)
{
    Uint1* buf,* buf_var;       // buffers to write sequence
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    Uint1 sentinel;             // sentinel byte

    CBioseq_Handle handle = scope->GetBioseqHandle(sl); // might throw exception

    // Retrieves the correct strand (plus or minus), but not both
    CSeqVector sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi, strand);

    switch (encoding) {
    // Protein sequences (query & subject) always have sentinels around sequence
    case BLASTP_ENCODING:
        sentinel = NULLB;
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        buflen = sv.size()+2;
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++)
            *buf_var++ = sv[i];
        *buf_var++ = sentinel;
        break;

    case NCBI4NA_ENCODING:
    case BLASTNA_ENCODING: // Used for nucleotide blastn queries
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        sentinel = 0xF;
        buflen = add_nucl_sentinel ? sv.size() + 2 : sv.size();
        if (strand == eNa_strand_both)
            buflen = add_nucl_sentinel ? buflen * 2 - 1 : buflen * 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++) {
            if (encoding == BLASTNA_ENCODING)
                *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
            else
                *buf_var++ = sv[i];
        }
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi4na);
            for (i = 0; i < sv.size(); i++) {
                if (encoding == BLASTNA_ENCODING)
                    *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
                else
                    *buf_var++ = sv[i];
            }
            if (add_nucl_sentinel)
                *buf_var++ = sentinel;
        }
        break;

    // Used only in Blast2Sequences for the subject sequence. No sentinels are
    // required. As in readdb, remainder (sv.size()%4 != 0) goes in the last 
    // 2 bits of the last byte.
    case NCBI2NA_ENCODING:
        ASSERT(add_nucl_sentinel == false);
        sv.SetCoding(CSeq_data::e_Ncbi2na);
        buflen = (sv.size()/COMPRESSION_RATIO) + 1;
        if (strand == eNa_strand_both)
            buflen *= 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        PackDNA(sv, buf_var, (strand == eNa_strand_both) ? buflen/2 : buflen);

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi2na);
            buf_var += buflen/2;
            PackDNA(sv, buf_var, buflen/2);
        }
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Invalid encoding");
    }

    if (len) {
        *len = buflen;
    }

    return buf;
}

#if 0
// Not used right now, need to complete implementation
void
BLASTGetTranslation(const Uint1* seq, const Uint1* seq_rev,
        const int nucl_length, const short frame, Uint1* translation)
{
    TSeqPos ni = 0;     // index into nucleotide sequence
    TSeqPos pi = 0;     // index into protein sequence

    const Uint1* nucl_seq = frame >= 0 ? seq : seq_rev;
    translation[0] = NULLB;
    for (ni = ABS(frame)-1; ni < (TSeqPos) nucl_length-2; ni += CODON_LENGTH) {
        Uint1 residue = CGen_code_table::CodonToIndex(nucl_seq[ni+0], 
                                                      nucl_seq[ni+1],
                                                      nucl_seq[ni+2]);
        if (IS_residue(residue))
            translation[pi++] = residue;
    }
    translation[pi++] = NULLB;

    return;
}
#endif

AutoPtr<Uint1, ArrayDeleter<Uint1> >
FindGeneticCode(int genetic_code)
{
    Uint1* retval = NULL;
    CSeq_data gc_ncbieaa(CGen_code_table::GetNcbieaa(genetic_code),
            CSeq_data::e_Ncbieaa);
    CSeq_data gc_ncbistdaa;

    TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
            CSeq_data::e_Ncbistdaa);

    ASSERT(gc_ncbistdaa.IsNcbistdaa());
    ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

    try {
        retval = new Uint1[nconv];
    } catch (bad_alloc& ba) {
        return NULL;
    }

    for (unsigned int i = 0; i < nconv; i++)
        retval[i] = gc_ncbistdaa.GetNcbistdaa().Get()[i];

    return retval;
}

string
FindMatrixPath(const char* matrix_name, bool is_prot)
{
    //char* retval = NULL, *p = NULL;
    string retval;
    string full_path;       // full path to matrix file

    if (!matrix_name)
        return retval;

    string mtx(matrix_name);
    transform(mtx.begin(), mtx.end(), mtx.begin(), (int (*)(int))toupper);

    // Look for matrix file in local directory
    full_path = mtx;
    if (CFile(full_path).Exists()) {
        return retval;
    }

    // Obtain the matrix path from the ncbi configuration file
    CMetaRegistry::SEntry sentry;
    sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    string path = sentry.registry ? sentry.registry->Get("NCBI", "Data") : "";

    full_path = retval = CFile::MakePath(path, mtx);
    if (CFile(full_path).Exists()) {
        retval[retval.find(mtx)] = NULLB;
        return retval;
    }

    // Try appending "aa" or "nt" 
    full_path = path;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval[retval.find(mtx)] = NULLB;
        return retval;
    }

    // Try using local "data" directory
    full_path = "data";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval[retval.find(mtx)] = NULLB;
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return NULL;

    const string& blastmat_env = app->GetEnvironment().Get("BLASTMAT");
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += is_prot ? "aa" : "nt";
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval[retval.find(mtx)] = NULLB;
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval[retval.find(mtx)] = NULLB;
        return retval;
    }
#endif

    // Try again without the "aa" or "nt"
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval[retval.find(mtx)] = NULLB;
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval[retval.find(mtx)] = NULLB;
        return retval;
    }
#endif

    return retval;
}

unsigned int
GetNumberOfFrames(EProgram p)
{
    unsigned int retval = 0;

    switch (p) {
    case eBlastn:
        retval = 2;
        break;
    case eBlastp:
    case eTblastn: 
        retval = 1;
        break;
    case eBlastx:
    case eTblastx:
        retval = 6;
        break;
    default:
        abort();
    }

    return retval;
}
END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.28  2003/09/12 17:52:42  camacho
* Stop using pair<> as return value from GetSequence
*
* Revision 1.27  2003/09/11 20:55:01  camacho
* Temporary fix for AutoPtr return value
*
* Revision 1.26  2003/09/11 17:45:03  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.25  2003/09/10 04:27:43  camacho
* 1) Minor change to return type of GetSequence
* 2) Fix to previous revision
*
* Revision 1.24  2003/09/09 22:15:02  dondosha
* Added cast in return statement in GetSequence method, fixing compiler error
*
* Revision 1.23  2003/09/09 15:57:23  camacho
* Fix indentation
*
* Revision 1.22  2003/09/09 14:21:39  coulouri
* change blastkar.h to blast_stat.h
*
* Revision 1.21  2003/09/09 12:57:15  camacho
* + internal setup functions, use smart pointers to handle memory mgmt
*
* Revision 1.20  2003/09/05 19:06:31  camacho
* Use regular new to allocate genetic code string
*
* Revision 1.19  2003/09/03 19:36:27  camacho
* Fix include path for blast_setup.hpp
*
* Revision 1.18  2003/08/28 22:42:54  camacho
* Change BLASTGetSequence signature
*
* Revision 1.17  2003/08/28 15:49:02  madden
* Fix for packing DNA as well as correct buflen
*
* Revision 1.16  2003/08/25 16:24:14  camacho
* Updated BLASTGetMatrixPath
*
* Revision 1.15  2003/08/19 17:39:07  camacho
* Minor fix to use of metaregistry class
*
* Revision 1.14  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.13  2003/08/14 13:51:24  camacho
* Use CMetaRegistry class to load the ncbi config file
*
* Revision 1.12  2003/08/11 15:17:39  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.11  2003/08/11 14:00:41  dicuccio
* Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
* BEGIN_NCBI_SCOPE block)
*
* Revision 1.10  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.9  2003/08/04 15:18:23  camacho
* Minor fixes
*
* Revision 1.8  2003/08/01 22:35:02  camacho
* Added function to get matrix path (fixme)
*
* Revision 1.7  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/25 13:55:58  camacho
* Removed unnecessary #includes
*
* Revision 1.3  2003/07/24 18:22:50  camacho
* #include blastkar.h
*
* Revision 1.2  2003/07/23 21:29:06  camacho
* Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
