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
 *   Blast2Sequences class interface
 *
 * ===========================================================================
 */

#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/api/blast_setup.hpp>
#include <algo/blast/api/blast_seqalign.hpp>

// NewBlast includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/aa_lookup.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_traceback.h>

#ifndef NUM_FRAMES
#define NUM_FRAMES 6
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBl2Seq::CBl2Seq(SSeqLoc& query, SSeqLoc& subject, EProgram p)
    : m_pOptions(new CBlastOption(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_Init(queries, subjects);
}

CBl2Seq::CBl2Seq(SSeqLoc& query, const TSeqLocVector& subjects, EProgram p)
    : m_pOptions(new CBlastOption(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 EProgram p)
    : m_pOptions(new CBlastOption(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
{
    x_Init(queries, subjects);
}

void CBl2Seq::x_Init(const TSeqLocVector& queries, 
                     const TSeqLocVector& subjects)
{
    m_tQueries = queries;
    m_tSubjects = subjects;
    mi_iMaxSubjLength = 0;
    mi_pScoreBlock = NULL;
    mi_pLookupTable = NULL;
    mi_pLookupSegments = NULL;
}

CBl2Seq::~CBl2Seq()
{ 
    x_ResetQueryDs();
    x_ResetSubjectDs();
}

/// Resets query data structures
void
CBl2Seq::x_ResetQueryDs()
{
    mi_bQuerySetUpDone = false;
    // should be changed if derived classes are created
    mi_clsQueries.CBLAST_SequenceBlk::~CBLAST_SequenceBlk();
    mi_clsQueryInfo.CBlastQueryInfo::~CBlastQueryInfo();
    mi_pScoreBlock = BlastScoreBlkFree(mi_pScoreBlock);
    mi_pLookupTable = BlastLookupTableDestruct(mi_pLookupTable);
    mi_pLookupSegments = ListNodeFreeData(mi_pLookupSegments);
    // TODO: should clean filtered regions?
}

/// Resets subject data structures
void
CBl2Seq::x_ResetSubjectDs()
{
    // Clean up structures and results from any previous search
    NON_CONST_ITERATE(vector<BLAST_SequenceBlk*>, itr, mi_vSubjects) {
        *itr = BlastSequenceBlkFree(*itr);
    }
    mi_vSubjects.clear();
    NON_CONST_ITERATE(vector<BlastResults*>, itr, mi_vResults) {
        *itr = BLAST_ResultsFree(*itr);
    }
    mi_vResults.clear();
    mi_vReturnStats.clear();
    // TODO: Should clear class wrappers for internal parameters structures?
    //      -> destructors will be called for them
    mi_iMaxSubjLength = 0;
    //m_pOptions->SetDbSeqNum(0);  // FIXME: Really needed?
    //m_pOptions->SetDbLength(0);  // FIXME: Really needed?
}

CRef<CSeq_align_set>
CBl2Seq::Run()
{
    TSeqAlignVector seqalignv = MultiQRun();
    return seqalignv[0];
}

TSeqAlignVector
CBl2Seq::MultiQRun()
{
    SetupSearch();
    //m_pOptions->DebugDumpText(cerr, "m_pOptions", 1);
    m_pOptions->Validate();  // throws an exception on failure
    ScanDB();
    Traceback();
    return x_Results2SeqAlign();
}

unsigned int
x_GetNumberOfFrames(EProgram p)
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

/// Now allows query concatenation
void
CBl2Seq::x_SetupQueryInfo()
{
    unsigned int nframes = x_GetNumberOfFrames(m_eProgram);

    // Allocate and initialize the query info structure
    mi_clsQueryInfo.Reset((BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo)));
    if ( !(mi_clsQueryInfo.operator->()) ) { // FIXME!
        NCBI_THROW(CBlastException, eOutOfMemory, "Query info");
    }
    mi_clsQueryInfo->num_queries = static_cast<int>(m_tQueries.size());
    mi_clsQueryInfo->first_context = 0;
    mi_clsQueryInfo->last_context = mi_clsQueryInfo->num_queries * nframes - 1;

    bool is_na = (m_eProgram == eBlastn) ? true : false;
    bool translate = ((m_eProgram == eBlastx) ||
                      (m_eProgram == eTblastx)) ? true : false;

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
           malloc(sizeof(int) * (mi_clsQueryInfo->last_context + 2)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Context offsets array");
    }
    if ( !(mi_clsQueryInfo->eff_searchsp_array = 
           (Int8*) calloc(mi_clsQueryInfo->last_context + 1, sizeof(Int8)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Search space array");
    }
    if ( !(mi_clsQueryInfo->length_adjustments = 
           (int*) calloc(mi_clsQueryInfo->last_context + 1, sizeof(int)))) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Length adjustments array");
    }

    mi_clsQueryInfo->context_offsets = context_offsets;
    context_offsets[0] = 0;

    // Set up the context offsets into the sequence that will be added to the
    // sequence block structure.
    unsigned int ctx_index = 0;      // index into context_offsets array
    ITERATE(TSeqLocVector, itr, m_tQueries) {
        TSeqPos length = sequence::GetLength(*itr->seqloc, itr->scope);
        _ASSERT(length != numeric_limits<TSeqPos>::max());

        // Unless the strand option is set to single strand, the actual
        // CSeq_locs dictacte which strand to examine during the search
        ENa_strand strand_opt = m_pOptions->GetStrandOption();
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

    mi_clsQueryInfo->total_length = context_offsets[ctx_index];
}

void
CBl2Seq::x_SetupQueries()
{
    x_ResetQueryDs();

    // Determine sequence encoding
    Uint1 encoding;
    if (m_eProgram == eBlastn) {
        encoding = BLASTNA_ENCODING;
    } else if (m_eProgram == eBlastn ||
               m_eProgram == eBlastx ||
               m_eProgram == eTblastx) {
        encoding = NCBI4NA_ENCODING;
    } else {
        encoding = BLASTP_ENCODING;
    }

    x_SetupQueryInfo();

    int buflen = mi_clsQueryInfo->total_length;
    Uint1* buf = (Uint1*) calloc(buflen, sizeof(Uint1));
    if ( !buf ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query sequence buffer");
    }

    bool is_na = (m_eProgram == eBlastn) ? true : false;
    bool translate = ((m_eProgram == eBlastx) ||
                      (m_eProgram == eTblastx)) ? true : false;

    unsigned int ctx_index = 0;      // index into context_offsets array
    unsigned int nframes = x_GetNumberOfFrames(m_eProgram);
    ITERATE(TSeqLocVector, itr, m_tQueries) {
        if (translate) {
            Uint1* na_buffer = NULL;
            int na_length = 0;

            // Get both strands of the original nucleotide sequence with
            // sentinels
            na_buffer = BLASTGetSequence(*itr->seqloc, encoding, na_length, 
                                         itr->scope, eNa_strand_both, true);

            // Populate the sequence buffer
            Uint1* gc = 
                BLASTFindGeneticCode(m_pOptions->GetQueryGeneticCode());
            for (unsigned int i = 0; i < nframes; i++) {
                if (BLAST_GetQueryLength(mi_clsQueryInfo, i) == 0) {
                    continue;
                }

                int offset = mi_clsQueryInfo->context_offsets[ctx_index + i];
                short frame = (i < 3) ? (i + 1) : (-i + 2);
                BLAST_GetTranslation(na_buffer + 1, na_buffer + na_length + 2,
                                     na_length, frame, &buf[offset], gc);
            }
            sfree(na_buffer);
            delete [] gc;

        } else if (is_na) {

            // Unless the strand option is set to single strand, the actual
            // CSeq_locs dictacte which strand to examine during the search
            ENa_strand strand_opt = m_pOptions->GetStrandOption();
            ENa_strand strand = sequence::GetStrand(*itr->seqloc,
                                                    itr->scope);
            if (strand_opt == eNa_strand_minus || 
                strand_opt == eNa_strand_plus) {
                strand = strand_opt;
            }
            int sbuflen = 0;
            Uint1* seqbuf = BLASTGetSequence(*itr->seqloc, encoding, sbuflen,
                                           itr->scope, strand, true);
            int index = (strand == eNa_strand_minus) ? ctx_index + 1 :
                ctx_index;
            int offset = mi_clsQueryInfo->context_offsets[index];
            memcpy(&buf[offset], seqbuf, sbuflen);
            sfree(seqbuf);

        } else {

            int sbuflen = 0;
            Uint1* seqbuf = BLASTGetSequence(*itr->seqloc, encoding, sbuflen,
                                             itr->scope, eNa_strand_unknown, 
                                             true);
            int offset = mi_clsQueryInfo->context_offsets[ctx_index];
            memcpy(&buf[offset], seqbuf, sbuflen);
            sfree(seqbuf);

        }

        ctx_index += nframes;
    }

    mi_clsQueries.Reset((BLAST_SequenceBlk*) calloc(1,
                                                    sizeof(BLAST_SequenceBlk)));
    if ( !(mi_clsQueries.operator->()) ) { // FIXME!
        sfree(buf);
        NCBI_THROW(CBlastException, eOutOfMemory, "Query block structure");
    }
    // FIXME: is buflen calculated correctly here?
    BlastSetUp_SeqBlkNew(buf, buflen - 2, 0, &mi_clsQueries, true);


    BlastMask* filter_mask = NULL;
    Blast_Message* blmsg = NULL;
    short st;

    st = BLAST_MainSetUp(m_eProgram, m_pOptions->GetQueryOpts(),
                         m_pOptions->GetScoringOpts(),
                         m_pOptions->GetLookupTableOpts(),
                         m_pOptions->GetHitSavingOpts(), mi_clsQueries, 
                         mi_clsQueryInfo, &mi_pLookupSegments, &filter_mask, 
                         &mi_pScoreBlock, &blmsg);
    // TODO: Check that lookup_segments are not filtering the whole sequence
    // (DoubleInt set to -1 -1)
    if (st != 0) {
        string msg = blmsg ? blmsg->message : "BLAST_MainSetUp failed";
        NCBI_THROW(CBlastException, eInternal, msg.c_str());
    }

    // Convert the BlastMask* into a CSeq_loc
    // TODO: Implement this! 
    //mi_vFilteredRegions = BLASTBlastMask2SeqLoc(filter_mask);
}

void
CBl2Seq::x_SetupSubjects()
{
    x_ResetSubjectDs();

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
    bool subj_is_na = (m_eProgram == eBlastn  ||
                       m_eProgram == eTblastn ||
                       m_eProgram == eTblastx);

    Uint1 encoding = (subj_is_na ? NCBI2NA_ENCODING : BLASTP_ENCODING);

    // TODO: Should strand selection on the subject sequences be allowed?
    //ENa_strand strand = m_pOptions->GetStrandOption(); 
    ENa_strand strand = eNa_strand_unknown;
    Int8 dblength = 0;

    ITERATE(TSeqLocVector, itr, m_tSubjects) {
        Uint1* buf = NULL;  // stores compressed sequence
        int buflen = 0;     // length of the buffer above
        BLAST_SequenceBlk* subj = (BLAST_SequenceBlk*) 
            calloc(1, sizeof(BLAST_SequenceBlk));

        buf = BLASTGetSequence(*itr->seqloc, encoding, buflen, itr->scope, 
                strand, false);

        if (subj_is_na) {
            subj->sequence = buf;
            subj->sequence_allocated = TRUE;

            encoding = (m_eProgram == eBlastn) ? 
                BLASTNA_ENCODING : NCBI4NA_ENCODING;
            bool use_sentinels = (m_eProgram == eBlastn) ?
                true : false;

            // Retrieve the sequence with ambiguities
            buf = BLASTGetSequence(*itr->seqloc, encoding, buflen,
                                   itr->scope, strand, use_sentinels);
            subj->sequence_start = buf;
            subj->length = use_sentinels ? buflen - 2 : buflen;
            subj->sequence_start_allocated = TRUE;
        } else {
            subj->sequence_start_allocated = TRUE;
            subj->sequence_start = buf;
            subj->sequence = buf+1;      // skips the sentinel byte
            subj->length = buflen - 2;   // don't count the sentinel bytes
        }
        dblength += subj->length;
        mi_vSubjects.push_back(subj);
        mi_iMaxSubjLength = MAX(mi_iMaxSubjLength, 
                sequence::GetLength(*itr->seqloc, itr->scope));
    }
    m_pOptions->SetDbSeqNum(mi_vSubjects.size());
    m_pOptions->SetDbLength(dblength);
}

void 
CBl2Seq::SetupSearch()
{
    if ( !mi_bQuerySetUpDone ) {
        m_eProgram = m_pOptions->GetProgram();  // options might have changed
        x_SetupQueries();
        LookupTableWrapInit(mi_clsQueries, m_pOptions->GetLookupTableOpts(),
                            mi_pLookupSegments, mi_pScoreBlock, 
                            &mi_pLookupTable);
        mi_bQuerySetUpDone = true;
    }

    x_SetupSubjects();
}

void 
CBl2Seq::ScanDB()
{
    ITERATE(vector<BLAST_SequenceBlk*>, itr, mi_vSubjects) {

        BlastResults* result = NULL;
        BlastReturnStat return_stats;
        memset((void*) &return_stats, 0, sizeof(return_stats));

        BLAST_ResultsInit(mi_clsQueryInfo->num_queries, &result);

        /*int total_hits = BLAST_SearchEngineCore(mi_Query, mi_pLookupTable, 
          mi_QueryInfo, NULL, *itr,
          mi_clsExtnWord, mi_clsGapAlign, m_pOptions->GetScoringOpts(), 
          mi_clsInitWordParams, mi_clsExtnParams, mi_clsHitSavingParams, NULL,
          m_pOptions->GetDbOpts(), &result, &return_stats);
          _TRACE("*** BLAST_SearchEngineCore hits " << total_hits << " ***");*/

        BLAST_TwoSequencesEngine(m_eProgram, mi_clsQueries, mi_clsQueryInfo, 
                                 *itr, mi_pScoreBlock, 
                                 m_pOptions->GetScoringOpts(),
                                 mi_pLookupTable,
                                 m_pOptions->GetInitWordOpts(),
                                 m_pOptions->GetExtensionOpts(),
                                 m_pOptions->GetHitSavingOpts(),
                                 m_pOptions->GetEffLenOpts(),
                                 NULL, m_pOptions->GetDbOpts(),
                                 result, &return_stats);

        mi_vResults.push_back(result);
        mi_vReturnStats.push_back(return_stats);
    }
}

void
CBl2Seq::Traceback()
{
#if 0
    for (unsigned int i = 0; i < mi_vResults.size(); i++) {
        BLAST_TwoSequencesTraceback(m_eProgram, mi_vResults[i], mi_Query,
                                    mi_QueryInfo, mi_vSubjects[i], mi_clsGapAlign, 
                                    m_pOptions->GetScoringOpts(), mi_clsExtnParams, mi_clsHitSavingParams);
    }
#endif
}

TSeqAlignVector
CBl2Seq::x_Results2SeqAlign()
{
    _TRACE("*** Calling results2seqalign ***");

    vector< CConstRef<CSeq_id> > query_vector;
    ITERATE(TSeqLocVector, itr, m_tQueries) {
        CConstRef<CSeq_id> query_id(&sequence::GetId(*itr->seqloc,
                                                     itr->scope));
        query_vector.push_back(query_id);
    }

    _ASSERT(mi_vResults.size() == m_tSubjects.size());
    unsigned int index = 0;
    
    TSeqAlignVector retval;

    for (index = 0; index < m_tSubjects.size(); ++index)
    {
        TSeqAlignVector seqalign =
            BLAST_Results2CSeqAlign(mi_vResults[index], m_eProgram,
                m_tQueries, NULL, 
                &m_tSubjects[index],
                m_pOptions->GetScoringOpts(), mi_pScoreBlock,
                m_pOptions->GetGappedMode());

        if (seqalign.size() > 0) {
            /* Merge the new vector with the current. Assume that both vectors
               contain CSeq_align_sets for all queries, i.e. have the same 
               size. */
            if (retval.size() == 0) {
                // First time around, just fill the empty vector with the 
                // seqaligns from the first subject.
                retval.swap(seqalign);
            } else {
                int i;
                for (i=0; i<retval.size(); ++i) {
                    retval[i]->Set().splice(retval[i]->Set().end(), 
                                            seqalign[i]->Set());
                }
            }
        }
    }

    // Clean up structures
    mi_clsInitWordParams.CBlastInitialWordParameters::~CBlastInitialWordParameters();
    mi_clsHitSavingParams.CBlastHitSavingParameters::~CBlastHitSavingParameters();
    mi_clsExtnWord.CBLAST_ExtendWord::~CBLAST_ExtendWord();
    mi_clsExtnParams.CBlastExtensionParameters::~CBlastExtensionParameters();
    mi_clsGapAlign.CBlastGapAlignStruct::~CBlastGapAlignStruct();

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.24  2003/08/25 17:15:49  camacho
 * Removed redundant typedef
 *
 * Revision 1.23  2003/08/19 22:12:47  dondosha
 * Cosmetic changes
 *
 * Revision 1.22  2003/08/19 20:27:06  dondosha
 * EProgram enum type is no longer part of CBlastOption class
 *
 * Revision 1.21  2003/08/19 13:46:13  dicuccio
 * Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
 *
 * Revision 1.20  2003/08/18 22:17:36  camacho
 * Renaming of SSeqLoc members
 *
 * Revision 1.19  2003/08/18 20:58:57  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.18  2003/08/18 19:58:50  dicuccio
 * Fixed compilation errors after change from pair<> to SSeqLoc
 *
 * Revision 1.17  2003/08/18 17:07:41  camacho
 * Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
 * Change in function to read seqlocs from files.
 *
 * Revision 1.16  2003/08/15 15:59:13  dondosha
 * Changed call to BLAST_Results2CSeqAlign according to new prototype
 *
 * Revision 1.15  2003/08/12 19:21:08  dondosha
 * Subject id argument to BLAST_Results2CppSeqAlign is now a simple pointer, allowing it to be NULL
 *
 * Revision 1.14  2003/08/11 19:55:55  camacho
 * Early commit to support query concatenation and the use of multiple scopes.
 * Compiles, but still needs work.
 *
 * Revision 1.13  2003/08/11 15:23:59  dondosha
 * Renamed conversion functions between BlastMask and CSeqLoc; added algo/blast/core to headers from core BLAST library
 *
 * Revision 1.12  2003/08/11 14:00:41  dicuccio
 * Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
 * BEGIN_NCBI_SCOPE block)
 *
 * Revision 1.11  2003/08/08 19:43:07  dicuccio
 * Compilation fixes: #include file rearrangement; fixed use of 'list' and
 * 'vector' as variable names; fixed missing ostrea<< for __int64
 *
 * Revision 1.10  2003/08/01 17:40:56  dondosha
 * Use renamed functions and structures from local blastkar.h
 *
 * Revision 1.9  2003/07/31 19:45:33  camacho
 * Eliminate Ptr notation
 *
 * Revision 1.8  2003/07/30 19:58:02  coulouri
 * use ListNode
 *
 * Revision 1.7  2003/07/30 15:00:01  camacho
 * Do not use Malloc/MemNew/MemFree
 *
 * Revision 1.6  2003/07/29 14:15:12  camacho
 * Do not use MemFree/Malloc
 *
 * Revision 1.5  2003/07/28 22:20:17  camacho
 * Removed unused argument
 *
 * Revision 1.4  2003/07/23 21:30:40  camacho
 * Calls to options member functions
 *
 * Revision 1.3  2003/07/15 19:21:36  camacho
 * Use correct strands and sequence buffer length
 *
 * Revision 1.2  2003/07/14 22:16:37  camacho
 * Added interface to retrieve masked regions
 *
 * Revision 1.1  2003/07/10 18:34:19  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
