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
 * Author:  Christiam Camacho
 *
 * ===========================================================================
 */

/// @file blast_setup_cxx.cpp
/// Auxiliary setup functions for Blast objects interface.

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/metareg.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/gencode_singleton.h>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seqloc/Seq_point.hpp> // needed in s_SeqLoc2MaskedSubjRanges

#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/** Set field values for one element of the context array of a
 * concatenated query.  All previous contexts should have already been
 * assigned correct values.
 * @param qinfo  Query info structure containing contexts. [in/out]
 * @param index  Index of the context to fill. [in]
 * @param length Length of this context. [in]
 */
static void
s_QueryInfo_SetContext(BlastQueryInfo*   qinfo,
                       Uint4             index,
                       Uint4             length)
{
    _ASSERT(index <= static_cast<Uint4>(qinfo->last_context));
    
    if (index) {
        Uint4 prev_loc = qinfo->contexts[index-1].query_offset;
        Uint4 prev_len = qinfo->contexts[index-1].query_length;
        
        Uint4 shift = prev_len ? prev_len + 1 : 0;
        
        qinfo->contexts[index].query_offset = prev_loc + shift;
        qinfo->contexts[index].query_length = length;
        if (length == 0)
           qinfo->contexts[index].is_valid = false;
    } else {
        // First context
        qinfo->contexts[0].query_offset = 0;
        qinfo->contexts[0].query_length = length;
        if (length == 0)
           qinfo->contexts[0].is_valid = false;
    }
}

/// Internal function to choose between the strand specified in a Seq-loc 
/// (which specified the query strand) and the strand obtained
/// from the CBlastOptions
/// @param seqloc_strand strand extracted from the query Seq-loc [in]
/// @param program program type from the CORE's point of view [in]
/// @param strand_opt strand as specified by the BLAST options [in]
static objects::ENa_strand 
s_BlastSetup_GetStrand(objects::ENa_strand seqloc_strand, 
                     EBlastProgramType program,
                     objects::ENa_strand strand_opt)
{
    if (Blast_QueryIsProtein(program)) {
        return eNa_strand_unknown;
    }

    // Only if the strand specified by the options is NOT both or unknown,
    // it takes precedence over what is specified by the query's strand
    ENa_strand retval = (strand_opt == eNa_strand_both || 
                         strand_opt == eNa_strand_unknown) 
        ? seqloc_strand : strand_opt;
    if (Blast_QueryIsNucleotide(program) && retval == eNa_strand_unknown) {
        retval = eNa_strand_both;
    }
    return retval;
}

objects::ENa_strand 
BlastSetup_GetStrand(const objects::CSeq_loc& query_seqloc, 
                     EBlastProgramType program, 
                     objects::ENa_strand strand_opt)
{
    return s_BlastSetup_GetStrand(query_seqloc.GetStrand(), program, 
                                  strand_opt);
}

/// Adjust first context depending on the first query strand
static void
s_AdjustFirstContext(BlastQueryInfo* query_info, 
                     EBlastProgramType prog,
                     ENa_strand strand_opt,
                     const IBlastQuerySource& queries)
{
    _ASSERT(query_info);

#if _DEBUG      /* to eliminate compiler warning in release mode */
    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
#endif
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    _ASSERT(is_na || translate);

    ENa_strand strand = s_BlastSetup_GetStrand(queries.GetStrand(0), prog,
                                             strand_opt);
    _ASSERT(strand != eNa_strand_unknown);

    // Adjust the first context if the requested strand is the minus strand
    if (strand == eNa_strand_minus) {
        query_info->first_context = translate ? 3 : 1;
    }
}

void
SetupQueryInfo_OMF(const IBlastQuerySource& queries,
                   EBlastProgramType prog,
                   objects::ENa_strand strand_opt,
                   BlastQueryInfo** qinfo)
{
    _ASSERT(qinfo);
    CBlastQueryInfo query_info(BlastQueryInfoNew(prog, queries.Size()));
    if (query_info.Get() == NULL) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "Query info");
    }

    const unsigned int kNumContexts = GetNumberOfContexts(prog);
    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    if (is_na || translate) {
        s_AdjustFirstContext(query_info, prog, strand_opt, queries);
    }

    // Set up the context offsets into the sequence that will be added
    // to the sequence block structure.
    unsigned int ctx_index = 0; // index into BlastQueryInfo::contexts array
    // Longest query length, to be saved in the query info structure
    Uint4 max_length = 0;

    for(TSeqPos j = 0; j < queries.Size(); j++) {
        TSeqPos length = 0;
        try { length = queries.GetLength(j); }
        catch (const CException&) { 
            // Ignore exceptions in this function as they will be caught in
            // SetupQueries
        }

        ENa_strand strand = s_BlastSetup_GetStrand(queries.GetStrand(j), prog,
                                                 strand_opt);
        
        if (translate) {
            for (unsigned int i = 0; i < kNumContexts; i++) {
                unsigned int prot_length = 
                    BLAST_GetTranslatedProteinLength(length, i);
                max_length = MAX(max_length, prot_length);
                
                Uint4 ctx_len(0);
                
                switch (strand) {
                case eNa_strand_plus:
                    ctx_len = (i<3) ? prot_length : 0;
                    s_QueryInfo_SetContext(query_info, ctx_index + i, ctx_len);
                    break;

                case eNa_strand_minus:
                    ctx_len = (i<3) ? 0 : prot_length;
                    s_QueryInfo_SetContext(query_info, ctx_index + i, ctx_len);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    s_QueryInfo_SetContext(query_info, ctx_index + i, 
                                           prot_length);
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
                    s_QueryInfo_SetContext(query_info, ctx_index, length);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, 0);
                    break;

                case eNa_strand_minus:
                    s_QueryInfo_SetContext(query_info, ctx_index, 0);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, length);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    s_QueryInfo_SetContext(query_info, ctx_index, length);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, length);
                    break;

                default:
                    abort();
                }
            } else {    // protein
                s_QueryInfo_SetContext(query_info, ctx_index, length);
            }
        }
        ctx_index += kNumContexts;
    }
    query_info->max_length = max_length;
    *qinfo = query_info.Release();
}

/** 
 * @brief Calculate the starting and ending contexts for a given strand
 * 
 * @param strand strand to compute contexts for [in]
 * @param num_contexts number of contexts [in]
 * @param start starting context [out]
 * @param end ending context [out]
 */
static void
s_ComputeStartEndContexts(ENa_strand   strand,
                          int          num_contexts,
                          int        & start,
                          int        & end)
{
    start = end = num_contexts;
    
    switch (strand) {
    case eNa_strand_minus: 
        start = num_contexts/2; 
        end = num_contexts;
        break;
    case eNa_strand_plus: 
        start = 0; 
        end = num_contexts/2;
        break;
    case eNa_strand_both:
        start = 0;
        end = num_contexts;
        break;
    default:
        abort();
    }
}

/** 
 * @brief Adds seqloc_frames to mask. 
 * 
 * @param prog BLAST program [in]
 * @param mask data structure to add the mask to [in|out]
 * @param query_index index of the query for which to add the mask [in]
 * @param seqloc_frames mask to add [in]
 * @param strand strand on which the mask is being added [in]
 * @param query_length length of the query [in]
 */
static void
s_AddMask(EBlastProgramType           prog,
          BlastMaskLoc              * mask,
          int                         query_index,
          CBlastQueryFilteredFrames & seqloc_frames,
          ENa_strand                  strand,
          TSeqPos                     query_length)
{
    _ASSERT(query_index < mask->total_size);
    unsigned num_contexts = GetNumberOfContexts(prog);
    
    if (Blast_QueryIsTranslated(prog)) {
        assert(seqloc_frames.QueryHasMultipleFrames());
        
        int starting_context(0), ending_context(0);
        
        s_ComputeStartEndContexts(strand,
                                  num_contexts,
                                  starting_context,
                                  ending_context);
        
        const TSeqPos dna_length = query_length;
        
        BlastSeqLoc** frames_seqloc = 
            & (mask->seqloc_array[query_index*num_contexts]);
        
        seqloc_frames.UseProteinCoords(dna_length);
            
        for (int i = starting_context; i < ending_context; i++) {
            short frame = BLAST_ContextToFrame(eBlastTypeBlastx, i);
            frames_seqloc[i] = *seqloc_frames[frame];
            seqloc_frames.Release(frame);
        }
    } else if (Blast_QueryIsNucleotide(prog) && 
               !Blast_ProgramIsPhiBlast(prog)) {
        
        int posframe = CSeqLocInfo::eFramePlus1;
        int negframe = CSeqLocInfo::eFrameMinus1;
        
        switch (strand) {
        case eNa_strand_plus:
            mask->seqloc_array[query_index*num_contexts] =
                *seqloc_frames[posframe];
            seqloc_frames.Release(posframe);
            break;
            
        case eNa_strand_minus:
            mask->seqloc_array[query_index*num_contexts+1] =
                *seqloc_frames[negframe];
            seqloc_frames.Release(negframe);
            break;
            
        case eNa_strand_both:
            mask->seqloc_array[query_index*num_contexts] =
                *seqloc_frames[posframe];
            
            mask->seqloc_array[query_index*num_contexts+1] =
                *seqloc_frames[negframe];
            
            seqloc_frames.Release(posframe);
            seqloc_frames.Release(negframe);
            break;
            
        default:
            abort();
        }
        
    } else {
        mask->seqloc_array[query_index] = *seqloc_frames[0];
        seqloc_frames.Release(0);
    }
}

/// Restricts the masked locations in frame_to_bsl for the case when the
/// BLAST program requires the query to be translated into multiple frames.
/// @param frame_to_bsl query filtered frames to adjust [out]
/// @param queries all query sequences [in]
/// @param query_index index of the query of interest in queries [in]
/// @param qinfo BlastQueryInfo structure for the queries above [in]
static void
s_RestrictSeqLocs_Multiframe(CBlastQueryFilteredFrames & frame_to_bsl,
                             const IBlastQuerySource   & queries,
                             int                         query_index,
                             const BlastQueryInfo      * qinfo)
{
    typedef set<CSeqLocInfo::ETranslationFrame> TFrameSet;
    const TFrameSet& frames = frame_to_bsl.ListFrames();
    const size_t kNumFrames = frame_to_bsl.GetNumFrames();
    _ASSERT(kNumFrames != 0);
    const int first_ctx = kNumFrames * query_index;
    const int last_ctx = kNumFrames * (query_index + 1);
    
    ITERATE(TFrameSet, iter, frames) {
        int seqloc_frame = *iter;
        BlastSeqLoc ** bsl = frame_to_bsl[seqloc_frame];
        
        for(int ci = first_ctx; ci <= last_ctx; ci++) {
            _ASSERT(qinfo->contexts[ci].query_index == query_index);
            int context_frame = qinfo->contexts[ci].frame;
            
            if (context_frame == seqloc_frame) {
                CConstRef<CSeq_loc> qseqloc = queries.GetSeqLoc(query_index);
                    
                BlastSeqLoc_RestrictToInterval(bsl,
                                               qseqloc->GetStart(eExtreme_Positional),
                                               qseqloc->GetStop (eExtreme_Positional));
                    
                break;
            }
        }
    }
}

/// Extract the masking locations for a single query into a
/// CBlastQueryFilteredFrames object and adjust the masks so that they
/// correspond to the range specified by the Seq-loc in queries.
/// @param queries all query sequences [in]
/// @param query_index index of the query of interest in queries [in]
/// @param qinfo BlastQueryInfo structure for the queries above [in]
/// @param program BLAST program being executed [in]
static CRef<CBlastQueryFilteredFrames>
s_GetRestrictedBlastSeqLocs(IBlastQuerySource & queries,
                            int                       query_index,
                            const BlastQueryInfo    * qinfo,
                            EBlastProgramType         program)
{
    TMaskedQueryRegions mqr =
        queries.GetMaskedRegions(query_index);
    
    CRef<CBlastQueryFilteredFrames> frame_to_bsl
        (new CBlastQueryFilteredFrames(program, mqr));
    
    if (! frame_to_bsl->Empty()) {
        if (frame_to_bsl->QueryHasMultipleFrames()) {
            s_RestrictSeqLocs_Multiframe(*frame_to_bsl,
                                         queries,
                                         query_index,
                                         qinfo);
        } else {
            CConstRef<CSeq_loc> qseqloc = queries.GetSeqLoc(query_index);
            BlastSeqLoc_RestrictToInterval((*frame_to_bsl)[0],
                                       qseqloc->GetStart(eExtreme_Positional),
                                       qseqloc->GetStop (eExtreme_Positional));
        }
    }
    
    return frame_to_bsl;
}

/// Mark the contexts corresponding to the query identified by query_index as
/// invalid
/// @param qinfo BlastQueryInfo structure to modify [in]
/// @param query_index index of the query, assumes it's in the BlastQueryInfo
/// structure above [in]
static void
s_InvalidateQueryContexts(BlastQueryInfo* qinfo, int query_index)
{
    _ASSERT(qinfo);
    for (int i = qinfo->first_context; i <= qinfo->last_context; i++) {
        if (qinfo->contexts[i].query_index == query_index) {
            qinfo->contexts[i].is_valid = FALSE;
        }
    }
}

void
SetupQueries_OMF(IBlastQuerySource& queries, 
                 BlastQueryInfo* qinfo, 
                 BLAST_SequenceBlk** seqblk,
                 EBlastProgramType prog,
                 objects::ENa_strand strand_opt,
                 TSearchMessages& messages)
{
    _ASSERT(seqblk);
    _ASSERT( !queries.Empty() );
    if (messages.size() != queries.Size()) {
        messages.resize(queries.Size());
    }

    EBlastEncoding encoding = GetQueryEncoding(prog);
    
    int buflen = QueryInfo_GetSeqBufLen(qinfo);
    TAutoUint1Ptr buf((Uint1*) calloc(buflen+1, sizeof(Uint1)));
    
    if ( !buf ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Query sequence buffer");
    }

    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    unsigned int ctx_index = 0;      // index into context_offsets array
    const unsigned int kNumContexts = GetNumberOfContexts(prog);

    CBlastMaskLoc mask(BlastMaskLocNew(qinfo->num_queries*kNumContexts));

    for(TSeqPos index = 0; index < queries.Size(); index++) {
        ENa_strand strand = eNa_strand_unknown;
        
        try {

            strand = s_BlastSetup_GetStrand(queries.GetStrand(index), prog,
                                          strand_opt);
            if ((is_na || translate) && strand == eNa_strand_unknown) {
                strand = eNa_strand_both;
            }

            CRef<CBlastQueryFilteredFrames> frame_to_bsl = 
                s_GetRestrictedBlastSeqLocs(queries, index, qinfo, prog);
            
            // Set the id if this is possible
            if (const CSeq_id* id = queries.GetSeqId(index)) {
                const string kTitle = queries.GetTitle(index);
                string query_id = id->AsFastaString();
                if (kTitle != kEmptyStr) {
                    query_id += " " + kTitle;
                }
                messages[index].SetQueryId(query_id);
            }

            SBlastSequence sequence;
            
            if (translate) {
                _ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                _ASSERT(Blast_QueryIsTranslated(prog));

                const Uint4 genetic_code_id = queries.GetGeneticCodeId(index);
                Uint1* gc = GenCodeSingletonFind(genetic_code_id);
                if (gc == NULL) {
                    TAutoUint1ArrayPtr gc_str = 
                        FindGeneticCode(genetic_code_id);
                    GenCodeSingletonAdd(genetic_code_id, gc_str.get());
                    gc = GenCodeSingletonFind(genetic_code_id);
                    _ASSERT(gc);
                }
            
                // Get both strands of the original nucleotide sequence with
                // sentinels
                sequence = queries.GetBlastSequence(index, encoding, strand, 
                                                    eSentinels);
                
                int na_length = queries.GetLength(index);
                Uint1* seqbuf_rev = NULL;  // negative strand
                if (strand == eNa_strand_both)
                   seqbuf_rev = sequence.data.get() + na_length + 1;
                else if (strand == eNa_strand_minus)
                   seqbuf_rev = sequence.data.get();

                // Populate the sequence buffer
                for (unsigned int i = 0; i < kNumContexts; i++) {
                    if (qinfo->contexts[ctx_index + i].query_length <= 0) {
                        continue;
                    }
                    
                    int offset = qinfo->contexts[ctx_index + i].query_offset;
                    BLAST_GetTranslation(sequence.data.get() + 1,
                                         seqbuf_rev,
                                         na_length,
                                         qinfo->contexts[ctx_index + i].frame,
                                         & buf.get()[offset], gc);
                }

            } else if (is_na) {

                _ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                
                sequence = queries.GetBlastSequence(index, encoding, strand, 
                                                    eSentinels);
                
                int idx = (strand == eNa_strand_minus) ? 
                    ctx_index + 1 : ctx_index;

                int offset = qinfo->contexts[idx].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);

            } else {

                string warnings;
                sequence = queries.GetBlastSequence(index,
                                                    encoding,
                                                    eNa_strand_unknown,
                                                    eSentinels,
                                                    &warnings);
                
                int offset = qinfo->contexts[ctx_index].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);
                if ( !warnings.empty() ) {
                    // FIXME: is index this the right value for the 2nd arg?
                    CRef<CSearchMessage> m
                        (new CSearchMessage(eBlastSevWarning, index, warnings));
                    messages[index].push_back(m);
                }
            }

            TSeqPos qlen = BlastQueryInfoGetQueryLength(qinfo, prog, index);
            
            // s_AddMask releases the elements of frame_to_bsl that it uses;
            // the rest are freed by frame_to_bsl in the destructor.
            s_AddMask(prog, mask, index, *frame_to_bsl, strand, qlen);
        
        } catch (const CException& e) {
            // FIXME: is index this the right value for the 2nd arg? Also, how
            // to determine whether the message should contain a warning or
            // error?
            CRef<CSearchMessage> m
                (new CSearchMessage(eBlastSevWarning, index, e.GetMsg()));
            messages[index].push_back(m);
            s_InvalidateQueryContexts(qinfo, index);
        }
        
        ctx_index += kNumContexts;
    }
    
    if (BlastSeqBlkNew(seqblk) < 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "Query sequence block");
    }

    // Validate that at least one query context is valid 
    if (BlastSetup_Validate(qinfo, NULL) != 0 && messages.HasMessages()) {
        NCBI_THROW(CBlastException, eSetup, messages.ToString());
    }
    
    BlastSeqBlkSetSequence(*seqblk, buf.release(), buflen - 2);
    
    (*seqblk)->lcase_mask = mask.Release();
    (*seqblk)->lcase_mask_allocated = TRUE;
}

static void
s_SeqLoc2MaskedSubjRanges(const CSeq_loc* slp, CSeqDB::TSequenceRanges& output)
{
    output.clear();
    if (!slp || 
        slp->Which() == CSeq_loc::e_not_set || 
        slp->IsEmpty() || 
        slp->IsNull() ) {
        return;
    }

    _ASSERT(slp->IsInt() || slp->IsPacked_int() || slp->IsMix());

    if (slp->IsInt()) {
        output.reserve(1);
		CSeqDB::TOffsetPair p;
		p.first=slp->GetInt().GetFrom();
		p.second=slp->GetInt().GetTo();
        output.push_back(p);
    } else if (slp->IsPacked_int()) {
        output.reserve(slp->GetPacked_int().Get().size());
        ITERATE(CPacked_seqint::Tdata, itr, slp->GetPacked_int().Get()) {
    		CSeqDB::TOffsetPair p;
		    p.first=(*itr)->GetFrom();
		    p.second=(*itr)->GetTo();
            output.push_back(p);
        }
    } else if (slp->IsMix()) {
        output.reserve(slp->GetMix().Get().size());
        ITERATE(CSeq_loc_mix::Tdata, itr, slp->GetMix().Get()) {
            if ((*itr)->IsInt()) {
				CSeqDB::TOffsetPair p;
				p.first=(*itr)->GetInt().GetFrom();
				p.second=(*itr)->GetInt().GetTo();
                output.push_back(p);
            } else if ((*itr)->IsPnt()) {
				CSeqDB::TOffsetPair p;
				p.first=(*itr)->GetPnt().GetPoint();
				p.second=(*itr)->GetPnt().GetPoint();
                output.push_back(p);
            }
        }
    } else {
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported CSeq_loc type");
    }
}

void
SetupSubjects_OMF(IBlastQuerySource& subjects,
                  EBlastProgramType prog,
                  vector<BLAST_SequenceBlk*>* seqblk_vec,
                  unsigned int* max_subjlen)
{
    _ASSERT(seqblk_vec);
    _ASSERT(max_subjlen);
    _ASSERT(!subjects.Empty());

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
	bool subj_is_na = Blast_SubjectIsNucleotide(prog) ? true : false;

    ESentinelType sentinels = eSentinels;
    if (prog == eBlastTypeTblastn 
     || prog == eBlastTypePsiTblastn
     || prog == eBlastTypeTblastx) {
        sentinels = eNoSentinels;
    }

    EBlastEncoding encoding = GetSubjectEncoding(prog);
       
    // N.B.: strand selection is only allowed for translated subjects, this is
    // done in the engine. For non-translated nucleotide subjects, the
    // alignment is "fixed" in s_AdjustNegativeSubjFrameInBlastn

    *max_subjlen = 0;

    for (TSeqPos i = 0; i < subjects.Size(); i++) {
        BLAST_SequenceBlk* subj = NULL;

        SBlastSequence sequence =
            subjects.GetBlastSequence(i, encoding, 
                                      eNa_strand_plus, sentinels);

        if (BlastSeqBlkNew(&subj) < 0) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                       "Subject sequence block");
        }

        if (Blast_SubjectIsTranslated(prog)) {
            const Uint4 genetic_code_id = subjects.GetGeneticCodeId(i);
            Uint1* gc = GenCodeSingletonFind(genetic_code_id);
            if (gc != NULL) {
                TAutoUint1ArrayPtr gc_str = FindGeneticCode(genetic_code_id);
                GenCodeSingletonAdd(genetic_code_id, gc_str.get());
                gc = GenCodeSingletonFind(genetic_code_id);
                _ASSERT(gc);
                subj->gen_code_string = gc; /* N.B.: not copied! */
            }
        }

        /* Set the lower case mask, if it exists */
        if (subjects.GetMask(i).NotEmpty()) {
            const CSeq_loc* masks = subjects.GetMask(i);
            CSeqDB::TSequenceRanges masked_ranges;
            _ASSERT(masks);
            s_SeqLoc2MaskedSubjRanges(masks, masked_ranges);
            _ASSERT( !masked_ranges.empty() );
            /// @todo: FIXME: this is inefficient, ideally, the masks shouldn't
            /// be copied for performance reasons...
            if (BlastSeqBlkSetSeqRanges(subj, (SSeqRange*)& masked_ranges[0],
                                    masked_ranges.size(), true) != 0) {
            }
        }
        subj->lcase_mask = NULL;                // unused for subjects
        subj->lcase_mask_allocated = FALSE;     // unused for subjects

        if (subj_is_na) {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
               ((sentinels == eSentinels) ? sequence.length - 2 :
                sequence.length));

            try {
                // Get the compressed sequence
                SBlastSequence compressed_seq =
                    subjects.GetBlastSequence(i, eBlastEncodingNcbi2na, 
                                              eNa_strand_plus, eNoSentinels);
                BlastSeqBlkSetCompressedSequence(subj, 
                                          compressed_seq.data.release());
            } catch (CException& e) {
                BlastSequenceBlkFree(subj);
                NCBI_RETHROW_SAME(e, 
                      "Failed to get compressed nucleotide sequence");
            }
        } else {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
                                   sequence.length - 2);
        }

        seqblk_vec->push_back(subj);
        (*max_subjlen) = MAX((*max_subjlen), subjects.GetLength(i));

    }
}

/// Tests if a number represents a valid residue
/// @param res Value to test [in]
/// @return TRUE if value is a valid residue
static inline bool s_IsValidResidue(Uint1 res) { return res < BLASTAA_SIZE; }

/// Protein sequences are always encoded in eBlastEncodingProtein and always 
/// have sentinel bytes around sequence data
static SBlastSequence 
GetSequenceProtein(IBlastSeqVector& sv, string* warnings = 0)
{
    Uint1* buf = NULL;          // buffer to write sequence
    Uint1* buf_var = NULL;      // temporary pointer to buffer
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    TAutoUint1Ptr safe_buf;     // contains buf to ensure exception safety
    vector<TSeqPos> replaced_residues; // Substituted residue positions
    vector<TSeqPos> invalid_residues;        // Invalid residue positions
    // This is the maximum number of residues we'll write a warning about
    static const size_t kMaxResiduesToWarnAbout = 20;

    sv.SetCoding(CSeq_data::e_Ncbistdaa);
    buflen = CalculateSeqBufferLength(sv.size(), eBlastEncodingProtein);
    _ASSERT(buflen != 0);
    buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
    if ( !buf ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Failed to allocate " + NStr::IntToString(buflen) + "bytes");
    }
    safe_buf.reset(buf);
    *buf_var++ = GetSentinelByte(eBlastEncodingProtein);
    for (i = 0; i < sv.size(); i++) {
        // Change unsupported residues to X
        if (sv[i] == AMINOACID_TO_NCBISTDAA[(int)'U'] ||
            sv[i] == AMINOACID_TO_NCBISTDAA[(int)'O']) {
            replaced_residues.push_back(i);
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
        for (i = 1; i < min(kMaxResiduesToWarnAbout, invalid_residues.size()); 
             i++) {
            error += ", " + NStr::IntToString(invalid_residues[i]);
        }
        if (invalid_residues.size() > kMaxResiduesToWarnAbout) {
            error += ",... (only first ";
            error += NStr::IntToString(kMaxResiduesToWarnAbout) + " shown)";
        }
        NCBI_THROW(CBlastException, eInvalidCharacter, error);
    }

    *buf_var++ = GetSentinelByte(eBlastEncodingProtein);
    if (warnings && replaced_residues.size() > 0) {
        *warnings += "One or more U or O characters replaced by X for ";
        *warnings += "alignment score calculations at positions ";
        *warnings += NStr::IntToString(replaced_residues[0]);
        for (i = 1; i < min(kMaxResiduesToWarnAbout, replaced_residues.size()); 
             i++) {
            *warnings += ", " + NStr::IntToString(replaced_residues[i]);
        }
        if (replaced_residues.size() > kMaxResiduesToWarnAbout) {
            *warnings += ",... (only first ";
            *warnings += NStr::IntToString(kMaxResiduesToWarnAbout);
            *warnings += " shown)";
        }
    }
    return SBlastSequence(safe_buf.release(), buflen);
}

/** 
 * @brief Auxiliary function to retrieve plus strand in compressed (ncbi4na)
 * format
 * 
 * @param sv abstraction to get sequence data [in]
 * 
 * @return requested data in compressed format
 */
static SBlastSequence
GetSequenceCompressedNucleotide(IBlastSeqVector& sv)
{
    sv.SetCoding(CSeq_data::e_Ncbi4na);
    return CompressNcbi2na(sv.GetCompressedPlusStrand());
}

/** 
 * @brief Auxiliary function to retrieve a single strand of a nucleotide
 * sequence.
 * 
 * @param sv abstraction to get sequence data [in]
 * @param encoding desired encoding for the data above [in]
 * @param strand desired strand [in]
 * @param sentinel use or do not use sentinel bytes [in]
 * 
 * @return Requested strand in desired encoding with/without sentinels
 */
static SBlastSequence
GetSequenceSingleNucleotideStrand(IBlastSeqVector& sv,
                                  EBlastEncoding encoding,
                                  objects::ENa_strand strand, 
                                  ESentinelType sentinel)
{
    _ASSERT(strand == eNa_strand_plus || strand == eNa_strand_minus);
    
    Uint1* buffer = NULL;          // buffer to write sequence
    TSeqPos buflen;             // length of buffer allocated
    const TSeqPos size = sv.size();   // size of original sequence
    TAutoUint1Ptr safe_buf;     // contains buffer to ensure exception safety
    
    // We assume that this packs one base per byte in the requested encoding
    sv.SetCoding(CSeq_data::e_Ncbi4na);
    buflen = CalculateSeqBufferLength(size, encoding, strand, sentinel);
    _ASSERT(buflen != 0);
    buffer = (Uint1*) malloc(sizeof(Uint1)*buflen);
    if ( !buffer ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
               "Failed to allocate " + NStr::IntToString(buflen) + " bytes");
    }
    safe_buf.reset(buffer);
    if (sentinel == eSentinels)
        *buffer++ = GetSentinelByte(encoding);

    sv.GetStrandData(strand, buffer);
    if (encoding == eBlastEncodingNucleotide) {
        for (TSeqPos i = 0; i < size; i++) {
            _ASSERT(sv[i] < BLASTNA_SIZE);
            buffer[i] = NCBI4NA_TO_BLASTNA[buffer[i]];
        }
    }
    buffer += size;
    
    if (sentinel == eSentinels)
        *buffer++ = GetSentinelByte(encoding);
    
    return SBlastSequence(safe_buf.release(), buflen);
}

/** 
 * @brief Auxiliary function to retrieve both strands of a nucleotide sequence.
 * 
 * @param sv abstraction to get sequence data [in]
 * @param encoding desired encoding for the data above [in]
 * @param sentinel use or do not use sentinel bytes [in]
 * 
 * @return concatenated strands in requested encoding with sentinels as
 * requested
 */
static SBlastSequence
GetSequenceNucleotideBothStrands(IBlastSeqVector& sv, 
                                 EBlastEncoding encoding, 
                                 ESentinelType sentinel)
{
    SBlastSequence plus =
        GetSequenceSingleNucleotideStrand(sv,
                                          encoding,
                                          eNa_strand_plus,
                                          eNoSentinels);
    
    SBlastSequence minus =
        GetSequenceSingleNucleotideStrand(sv,
                                          encoding,
                                          eNa_strand_minus,
                                          eNoSentinels);
    
    // Stitch the two together
    TSeqPos buflen = CalculateSeqBufferLength(sv.size(), encoding, 
                                              eNa_strand_both, sentinel);
    Uint1* buf_ptr = (Uint1*) malloc(sizeof(Uint1) * buflen);
    if ( !buf_ptr ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Failed to allocate " + NStr::IntToString(buflen) + "bytes");
    }
    SBlastSequence retval(buf_ptr, buflen);

    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }
    memcpy(buf_ptr, plus.data.get(), plus.length);
    buf_ptr += plus.length;
    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }
    memcpy(buf_ptr, minus.data.get(), minus.length);
    buf_ptr += minus.length;
    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }

    return retval;
}


SBlastSequence
GetSequence_OMF(IBlastSeqVector& sv, EBlastEncoding encoding, 
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings) 
{
    switch (encoding) {
    case eBlastEncodingProtein:
        return GetSequenceProtein(sv, warnings);

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide: // Used for nucleotide blastn queries
        if (strand == eNa_strand_both) {
            return GetSequenceNucleotideBothStrands(sv, encoding, sentinel);
        } else {
            return GetSequenceSingleNucleotideStrand(sv,
                                                     encoding,
                                                     strand,
                                                     sentinel);
        }

    case eBlastEncodingNcbi2na:
        _ASSERT(sentinel == eNoSentinels);
        return GetSequenceCompressedNucleotide(sv);

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }
}

EBlastEncoding
GetQueryEncoding(EBlastProgramType program)
{
    EBlastEncoding retval = eBlastEncodingError;

    switch (program) {
    case eBlastTypeBlastn:
    case eBlastTypePhiBlastn: 
        retval = eBlastEncodingNucleotide; 
        break;

    case eBlastTypeBlastp: 
    case eBlastTypeTblastn:
    case eBlastTypePsiTblastn:
    case eBlastTypeRpsBlast: 
    case eBlastTypePsiBlast:
    case eBlastTypePhiBlastp:
        retval = eBlastEncodingProtein; 
        break;

    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        retval = eBlastEncodingNcbi4na;
        break;

    default:
        abort();    // should never happen
    }

    return retval;
}

EBlastEncoding
GetSubjectEncoding(EBlastProgramType program)
{
    EBlastEncoding retval = eBlastEncodingError;

    switch (program) {
    case eBlastTypeBlastn: 
        retval = eBlastEncodingNucleotide; 
        break;

    case eBlastTypeBlastp: 
    case eBlastTypeBlastx:
    case eBlastTypePsiBlast:
        retval = eBlastEncodingProtein; 
        break;

    case eBlastTypeTblastn:
    case eBlastTypePsiTblastn:
    case eBlastTypeTblastx:
        retval = eBlastEncodingNcbi4na;
        break;

    default:
        abort();        // should never happen
    }

    return retval;
}

SBlastSequence CompressNcbi2na(const SBlastSequence& source)
{
    _ASSERT(source.data.get());

    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence

    // Allocate the return value
    SBlastSequence retval(CalculateSeqBufferLength(source.length,
                                                   eBlastEncodingNcbi2na,
                                                   eNa_strand_plus,
                                                   eNoSentinels));
    Uint1* source_ptr = source.data.get();

    // Populate the compressed sequence up to the last byte
    for (ci = 0, i = 0; ci < retval.length-1; ci++, i+= COMPRESSION_RATIO) {
        Uint1 a, b, c, d;
        a = ((*source_ptr & NCBI2NA_MASK)<<6); ++source_ptr;
        b = ((*source_ptr & NCBI2NA_MASK)<<4); ++source_ptr;
        c = ((*source_ptr & NCBI2NA_MASK)<<2); ++source_ptr;
        d = ((*source_ptr & NCBI2NA_MASK)<<0); ++source_ptr;
        retval.data.get()[ci] = a | b | c | d;
    }

    // Set the last byte in the compressed sequence
    retval.data.get()[ci] = 0;
    for (; i < source.length; i++) {
            Uint1 bit_shift = 0;
            switch (i%COMPRESSION_RATIO) {
               case 0: bit_shift = 6; break;
               case 1: bit_shift = 4; break;
               case 2: bit_shift = 2; break;
               default: abort();   // should never happen
            }
            retval.data.get()[ci] |= ((*source_ptr & NCBI2NA_MASK)<<bit_shift);
            ++source_ptr;
    }
    // Set the number of bases in the last 2 bits of the last byte in the
    // compressed sequence
    retval.data.get()[ci] |= source.length%COMPRESSION_RATIO;
    return retval;
}

TSeqPos CalculateSeqBufferLength(TSeqPos sequence_length, 
                                 EBlastEncoding encoding,
                                 objects::ENa_strand strand, 
                                 ESentinelType sentinel)
                                 THROWS((CBlastException))
{
    TSeqPos retval = 0;

    if (sequence_length == 0) {
        return retval;
    }

    switch (encoding) {
    // Strand and sentinels are irrelevant in this encoding.
    // Strand is always plus and sentinels cannot be represented
    case eBlastEncodingNcbi2na:
        _ASSERT(sentinel == eNoSentinels);
        _ASSERT(strand == eNa_strand_plus);
        retval = sequence_length / COMPRESSION_RATIO + 1;
        break;

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide: // Used for nucleotide blastn queries
        if (sentinel == eSentinels) {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
                retval += 3;
            } else {
                retval = sequence_length + 2;
            }
        } else {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
            } else {
                retval = sequence_length;
            }
        }
        break;

    case eBlastEncodingProtein:
        _ASSERT(sentinel == eSentinels);
        _ASSERT(strand == eNa_strand_unknown);
        retval = sequence_length + 2;
        break;

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }

    return retval;
}

Uint1 GetSentinelByte(EBlastEncoding encoding) THROWS((CBlastException))
{
    switch (encoding) {
    case eBlastEncodingProtein:
        return kProtSentinel;

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide:
        return kNuclSentinel;

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }
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

/** Get the path to the matrix, without the actual matrix name.
 * @param full_path including the matrix name, this string will be modified [in]
 * @param matrix_name name of matrix (e.g., BLOSUM62) [in]
 * @return char* to matrix path
 */
char* s_GetCStringOfMatrixPath(string& full_path, const string& matrix_name)
{
        // The following line erases the actual name of the matrix from the string.
        full_path.erase(full_path.size() - matrix_name.size());
        char* matrix_path = strdup(full_path.c_str());
        return matrix_path;
}

char* BlastFindMatrixPath(const char* matrix_name, Boolean is_prot)
{
    if (!matrix_name)
        return NULL;

    try {
       string mtx(matrix_name);
       mtx = NStr::ToUpper(mtx);

       // Look for matrix file in local directory
       string full_path = mtx;       // full path to matrix file
       if (CFile(full_path).Exists()) {
           string cwd = CDir::GetCwd();
           cwd += CFile::GetPathSeparator();
           return strdup(cwd.c_str());
       }
    
       string path("");

       // Obtain the matrix path from the ncbi configuration file
       try {
           CMetaRegistry::SEntry sentry;
           sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
           path = sentry.registry ? sentry.registry->Get("NCBI", "Data") : "";
       } catch (const CRegistryException&) { /* ignore */ }
    
       full_path = CFile::MakePath(path, mtx);
       if (CFile(full_path).Exists()) {
           return s_GetCStringOfMatrixPath(full_path, mtx);
       }
   
       // Try appending "aa" or "nt" 
       full_path = path;
       full_path += CFile::GetPathSeparator();
       full_path += is_prot ? "aa" : "nt";
       full_path += CFile::GetPathSeparator();
       full_path += mtx;
       if (CFile(full_path).Exists()) {
           return s_GetCStringOfMatrixPath(full_path, mtx);
       }
   
       // Try using local "data" directory
       full_path = "data";
       full_path += CFile::GetPathSeparator();
       full_path += mtx;
       if (CFile(full_path).Exists()) {
           return s_GetCStringOfMatrixPath(full_path, mtx);
       }
   
       CNcbiApplication* app = CNcbiApplication::Instance();
       if (!app)
       {
           return NULL;
       }
   
       const string& blastmat_env = app->GetEnvironment().Get("BLASTMAT");
       if (CDir(blastmat_env).Exists()) {
           full_path = blastmat_env;
           full_path += CFile::GetPathSeparator();
           full_path += is_prot ? "aa" : "nt";
           full_path += CFile::GetPathSeparator();
           full_path += mtx;
           if (CFile(full_path).Exists()) {
               return s_GetCStringOfMatrixPath(full_path, mtx);
           }
       }
   
#ifdef OS_UNIX
       full_path = BLASTMAT_DIR;
       full_path += CFile::GetPathSeparator();
       full_path += is_prot ? "aa" : "nt";
       full_path += CFile::GetPathSeparator();
       full_path += mtx;
       if (CFile(full_path).Exists()) {
           return s_GetCStringOfMatrixPath(full_path, mtx);
       }
#endif

       // Try again without the "aa" or "nt"
       if (CDir(blastmat_env).Exists()) {
           full_path = blastmat_env;
           full_path += CFile::GetPathSeparator();
           full_path += mtx;
           if (CFile(full_path).Exists()) {
               return s_GetCStringOfMatrixPath(full_path, mtx);
           }
       }

#ifdef OS_UNIX
       full_path = BLASTMAT_DIR;
       full_path += CFile::GetPathSeparator();
       full_path += mtx;
       if (CFile(full_path).Exists()) {
           return s_GetCStringOfMatrixPath(full_path, mtx);
       }
#endif
    } catch (...)  { } // Ignore all exceptions and return NULL.

    return NULL;
}


/// Checks if a BLAST database exists at a given file path: looks for 
/// an alias file first, then for an index file
static bool BlastDbFileExists(string& path, bool is_prot)
{
    // Check for alias file first
    string full_path = path + (is_prot ? ".pal" : ".nal");
    if (CFile(full_path).Exists())
        return true;
    // Check for an index file
    full_path = path + (is_prot ? ".pin" : ".nin");
    if (CFile(full_path).Exists())
        return true;
    return false;
}

string
FindBlastDbPath(const char* dbname, bool is_prot)
{
    string retval;
    string full_path;       // full path to matrix file

    if (!dbname)
        return retval;

    string database(dbname);

    // Look for matrix file in local directory
    full_path = database;
    if (BlastDbFileExists(full_path, is_prot)) {
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const string& blastdb_env = app->GetEnvironment().Get("BLASTDB");
        if (CFile(blastdb_env).Exists()) {
            full_path = blastdb_env;
            full_path += CFile::GetPathSeparator();
            full_path += database;
            if (BlastDbFileExists(full_path, is_prot)) {
                retval = full_path;
                retval.erase(retval.size() - database.size());
                return retval;
            }
        }
    }

    // Obtain the matrix path from the ncbi configuration file
    CMetaRegistry::SEntry sentry;
    sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    string path = 
        sentry.registry ? sentry.registry->Get("BLAST", "BLASTDB") : "";

    full_path = CFile::MakePath(path, database);
    if (BlastDbFileExists(full_path, is_prot)) {
        retval = full_path;
        retval.erase(retval.size() - database.size());
        return retval;
    }

    return retval;
}

unsigned int
GetNumberOfContexts(EBlastProgramType p)
{
    unsigned int retval = 0;
    if ( (retval = BLAST_GetNumberOfContexts(p)) == 0) {
        int debug_value = static_cast<int>(p);
        string prog_name(Blast_ProgramNameFromType(p));
        string msg = "Cannot get number of contexts for invalid program ";
        msg += "type: " + prog_name + " (" + NStr::IntToString(debug_value);
        msg += ")";
        NCBI_THROW(CBlastException, eNotSupported, msg);
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////

BLAST_SequenceBlk*
SafeSetupQueries(IBlastQuerySource& queries,
                 const CBlastOptions* options,
                 BlastQueryInfo* query_info,
                 TSearchMessages& messages)
{
    _ASSERT(options);
    _ASSERT(query_info);
    _ASSERT( !queries.Empty() );

    CBLAST_SequenceBlk retval;
    SetupQueries_OMF(queries, query_info, &retval, options->GetProgramType(), 
                     options->GetStrandOption(), messages);

    return retval.Release();
}

BlastQueryInfo*
SafeSetupQueryInfo(const IBlastQuerySource& queries,
                   const CBlastOptions* options)
{
    _ASSERT(!queries.Empty());
    _ASSERT(options);

    CBlastQueryInfo retval;
    SetupQueryInfo_OMF(queries, options->GetProgramType(),
                       options->GetStrandOption(), &retval);

    if (retval.Get() == NULL) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "blast::SetupQueryInfo failed");
    }
    return retval.Release();
}


bool CBlastQueryFilteredFrames::x_NeedsTrans()
{
    bool retval;
    switch(m_Program) {
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        retval = true;
        break;
            
    default:
        retval = false;
        break;
    }
    return retval;
}

CBlastQueryFilteredFrames::
CBlastQueryFilteredFrames(EBlastProgramType program)
    : m_Program(program)
{
    m_TranslateCoords = x_NeedsTrans();
}

CBlastQueryFilteredFrames::
CBlastQueryFilteredFrames(EBlastProgramType           program,
                          const TMaskedQueryRegions & mqr)
    : m_Program(program)
{
    m_TranslateCoords = x_NeedsTrans();
    
    if (mqr.empty()) {
        return;
    }
    
    set<ETranslationFrame> frames;
    ITERATE(TMaskedQueryRegions, itr, mqr) {
        const CSeq_interval & intv = (**itr).GetInterval();
        
        ETranslationFrame frame =
            (ETranslationFrame) (**itr).GetFrame();
        
        AddSeqLoc(intv, frame);
        frames.insert(frame);
    }
}

CBlastQueryFilteredFrames::~CBlastQueryFilteredFrames()
{
    ITERATE(TFrameSet, iter, m_Seqlocs) {
        if ((*iter).second != 0) {
            BlastSeqLocFree((*iter).second);
        }
    }
}

void CBlastQueryFilteredFrames::Release(int frame)
{
    m_Seqlocs.erase((ETranslationFrame)frame);
    m_SeqlocTails.erase((ETranslationFrame)frame);
}

// some of the logic in this function is shamelessly copied from
// BlastMaskLocDNAToProtein, which should have been used instead of creating
// this class (which I presume was added ignoring the former function)
void CBlastQueryFilteredFrames::UseProteinCoords(TSeqPos dna_length)
{
    if (m_TranslateCoords) {
        m_TranslateCoords = false;
        map<ETranslationFrame, int> frame_lengths;
        frame_lengths[CSeqLocInfo::eFramePlus1] = 
            frame_lengths[CSeqLocInfo::eFrameMinus1] = dna_length /
            CODON_LENGTH;
        frame_lengths[CSeqLocInfo::eFramePlus2] = 
            frame_lengths[CSeqLocInfo::eFrameMinus2] = (dna_length-1) /
            CODON_LENGTH;
        frame_lengths[CSeqLocInfo::eFramePlus3] = 
            frame_lengths[CSeqLocInfo::eFrameMinus3] = (dna_length-2) /
            CODON_LENGTH;
        
        ITERATE(TFrameSet, iter, m_Seqlocs) {
            short frame = iter->first;
            BlastSeqLoc * bsl = iter->second;

            for (BlastSeqLoc* itr = bsl; itr; itr = itr->next) {
                int to(0), from(0);
                
                if (frame < 0) {
                    from = (dna_length + frame - itr->ssr->right) / CODON_LENGTH;
                    to = (dna_length + frame - itr->ssr->left) / CODON_LENGTH;
                } else {
                    from = (itr->ssr->left - frame + 1) / CODON_LENGTH;
                    to = (itr->ssr->right - frame + 1) / CODON_LENGTH;
                }
                if (from < 0)
                    from = 0;
                if (to < 0)
                    to = 0;
                const int kFrameLength = frame_lengths[(CSeqLocInfo::ETranslationFrame)frame];
                if (from >= kFrameLength)
                    from = kFrameLength - 1;
                if (to >= kFrameLength)
                    to = kFrameLength - 1;
                
                _ASSERT(from >= 0 && to >= 0);
                _ASSERT(from < kFrameLength && to < kFrameLength);
                itr->ssr->left  = from;
                itr->ssr->right = to;
            }
        }
    }
}

const set<CBlastQueryFilteredFrames::ETranslationFrame>&
CBlastQueryFilteredFrames::ListFrames()
{
    if (m_Frames.empty()) {
        ITERATE(TFrameSet, iter, m_Seqlocs) {
            if ((*iter).second != 0) {
                m_Frames.insert((*iter).first);
            }
        }
    }
    return m_Frames;
}

bool CBlastQueryFilteredFrames::Empty()
{
    return ListFrames().empty();
}

void CBlastQueryFilteredFrames::x_VerifyFrame(int frame)
{
    bool okay = true;
    
    switch(m_Program) {
    case eBlastTypeBlastp:
    case eBlastTypeTblastn:
    case eBlastTypePsiTblastn:
    case eBlastTypeRpsBlast:
    case eBlastTypePsiBlast:
    case eBlastTypePhiBlastp:
        if (frame != 0) {
            okay = false;
        }
        break;
        
    case eBlastTypeBlastn:
        if ((frame != CSeqLocInfo::eFramePlus1) &&
            (frame != CSeqLocInfo::eFrameMinus1)) {
            okay = false;
        }
        break;
        
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        switch(frame) {
        case 1:
        case 2:
        case 3:
        case -1:
        case -2:
        case -3:
            break;
            
        default:
            okay = false;
        }
        break;
        
    default:
        okay = false;
    }
    
    if (! okay) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Frame and program values are incompatible.");
    }
}

bool CBlastQueryFilteredFrames::QueryHasMultipleFrames() const
{
    switch(m_Program) {
    case eBlastTypeBlastp:
    case eBlastTypeTblastn:
    case eBlastTypePsiTblastn:
    case eBlastTypeRpsBlast:
    case eBlastTypePhiBlastp:
    case eBlastTypePsiBlast:
        return false;
        
    case eBlastTypeBlastn:
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        return true;
        
    default:
        NCBI_THROW(CBlastException, eNotSupported, 
                   "IsMulti: unsupported program");
    }
    
    return false;
}

void CBlastQueryFilteredFrames::AddSeqLoc(const objects::CSeq_interval & intv, 
                                          int frame)
{
    _ASSERT( m_Frames.empty() );
    if ((frame == 0) && (m_Program == eBlastTypeBlastn)) {
        x_VerifyFrame(CSeqLocInfo::eFramePlus1);
        x_VerifyFrame(CSeqLocInfo::eFrameMinus1);
        static const CSeqLocInfo::ETranslationFrame kFrames[] = {
            CSeqLocInfo::eFramePlus1, CSeqLocInfo::eFrameMinus1 };
        
        for (size_t i = 0; i < sizeof(kFrames)/sizeof(*kFrames); i++) {
            m_SeqlocTails[ kFrames[i] ] = 
                BlastSeqLocNew( (m_SeqlocTails[ kFrames[i] ] 
                                ? & m_SeqlocTails[ kFrames[i] ] 
                                : & m_Seqlocs[ kFrames[i] ]),
                               intv.GetFrom(), intv.GetTo());
        }

    } else {
        x_VerifyFrame(frame);
        
        m_SeqlocTails[(ETranslationFrame) frame] = 
            BlastSeqLocNew( (m_SeqlocTails[(ETranslationFrame) frame] 
                            ? & m_SeqlocTails[(ETranslationFrame) frame] 
                            : & m_Seqlocs[(ETranslationFrame) frame]),
                           intv.GetFrom(), intv.GetTo());
    }
}

BlastSeqLoc ** CBlastQueryFilteredFrames::operator[](int frame)
{
    // Asking for a frame verifies that it is a valid value for the
    // type of search you are running.
    
    x_VerifyFrame(frame);
    return & m_Seqlocs[(ETranslationFrame) frame];
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
