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
#include <objects/seqfeat/seqfeat__.hpp>

#include <BlastOption.hpp>
#include <BlastSetup.hpp>
#include <Bl2Seq.hpp>
#include <BlastSeqalign.hpp>

// NewBlast includes
#include <blast_def.h>
#include <blast_util.h>
#include <blast_setup.h>
#include <aa_lookup.h>
#include <blast_engine.h>
#include <blast_traceback.h>

#ifndef NUM_FRAMES
#define NUM_FRAMES 6
#endif

BEGIN_NCBI_SCOPE

CBl2Seq::CBl2Seq(CConstRef<CSeq_loc>& query, CConstRef<CSeq_loc>& subject, 
        TProgram p, CScope* scope)
: m_Query(query), m_Program(p), m_Scope(scope)
{
    // call constructor for mult seqlocs vs mult seqlocs
    TSeqLocVector subjects;
    subjects.push_back(subject);
    m_Subjects = subjects;
    x_Init();
}

CBl2Seq::CBl2Seq(CConstRef<CSeq_loc>& query, const TSeqLocVector& subject,
        TProgram p, CScope* scope)
: m_Query(query), m_Subjects(subject), m_Program(p), m_Scope(scope)
{
    x_Init();
}

void CBl2Seq::x_Init(void)
{
    m_QuerySetUpDone = false;
    m_Options.Reset(new CBlastOption(m_Program));

    mi_vSubjects.clear();
    mi_MaxSubjLength = 0;

    mi_Sbp = NULL;
    mi_LookupTable = NULL;
    mi_LookupSegments = NULL;

    mi_vResults.clear();
    mi_vReturnStats.clear();
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects,
        TProgram p, CScope* scope)
{
    // real constructor should live here!
    abort();
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
    // Clean up structures if needed
    mi_Sbp = BLAST_ScoreBlkDestruct(mi_Sbp);
    mi_Query.CBLAST_SequenceBlkPtr::~CBLAST_SequenceBlkPtr();
    mi_QueryInfo.CBlastQueryInfoPtr::~CBlastQueryInfoPtr();
    if (mi_LookupTable)
        mi_LookupTable = BlastLookupTableDestruct(mi_LookupTable);
    mi_LookupSegments = ListNodeFreeData(mi_LookupSegments);
    m_QuerySetUpDone = false;
}

/// Resets subject data structures
void
CBl2Seq::x_ResetSubjectDs()
{
    // Clean up structures and results from any previous search
    for (unsigned int i = 0; i < mi_vSubjects.size(); i++) {
        if (mi_vSubjects[i])
            mi_vSubjects[i] = BlastSequenceBlkFree(mi_vSubjects[i]);
        if (mi_vResults[i])
            mi_vResults[i] = BLAST_ResultsFree(mi_vResults[i]);
    }
    mi_vSubjects.clear();
    mi_vResults.clear();
    mi_vReturnStats.clear();
    m_Options->SetDbSeqNum(0);
    m_Options->SetDbLength(0);
}

CRef<CSeq_align_set> 
CBl2Seq::Run()
{
    SetupSearch();
    //m_Options->DebugDumpText(cerr, "m_Options", 1);
    m_Options->Validate();  // throws an exception on failure
    ScanDB();
    Traceback();
    return x_Results2SeqAlign();
}

void
CBl2Seq::x_SetupQueryInfo()
{
    int* context_offsets = NULL;
    int nframes;
    bool translate = false, is_na = false;
    ENa_strand strand = m_Options->GetStrandOption();
    TSeqPos length = sequence::GetLength(*m_Query, m_Scope);
    _ASSERT(length != numeric_limits<TSeqPos>::max());

    mi_QueryInfo.Reset((BlastQueryInfoPtr) calloc(1, sizeof(BlastQueryInfo)));
    _ASSERT(mi_QueryInfo.operator->() != NULL);

    if (m_Program == CBlastOption::eBlastn) {
        is_na = true;
        nframes = 2;
    } else if (m_Program == CBlastOption::eBlastp ||
               m_Program == CBlastOption::eTblastn) {
        nframes = 1;
    } else if (m_Program == CBlastOption::eBlastx ||
               m_Program == CBlastOption::eTblastx) {
        translate = true;
        nframes = NUM_FRAMES;
    } else {
        NCBI_THROW(CBlastException, eInternal, "Invalid program");
    }

    mi_QueryInfo->num_queries = 1;      // Only 1 query allowed for bl2seq
    mi_QueryInfo->first_context = 0;
    mi_QueryInfo->last_context = nframes - 1;

    if ( !(context_offsets = (int*) malloc(sizeof(int)*(nframes+1))))
        NCBI_THROW(CBlastException, eOutOfMemory, "Context offsets array");

    // Only one element is needed as query concatenation is not allowed for
    // bl2seq
    if ( !(mi_QueryInfo->eff_searchsp_array = 
                (Int8*) calloc(nframes, sizeof(Int8))))
        NCBI_THROW(CBlastException, eOutOfMemory, "Search space array");
    if ( !(mi_QueryInfo->length_adjustments = 
                (int*) calloc(nframes, sizeof(int))))
        NCBI_THROW(CBlastException, eOutOfMemory, "Length adjustments array");

    mi_QueryInfo->context_offsets = context_offsets;
    context_offsets[0] = 0;

    // Set up the context offsets into the sequence that will be added to the
    // sequence block structure. All queries have sentinel bytes, so add 1 to 
    // the context offsets
    if (translate) {
        for (int i = 0; i < nframes; i++) {
            int prot_length = (length - i%CODON_LENGTH)/CODON_LENGTH;
            switch (strand) {
            case eNa_strand_plus: 
                if (i < 3)
                    context_offsets[i+1] = context_offsets[i] + prot_length + 1;
                else
                    context_offsets[i+1] = context_offsets[2];
                break;

            case eNa_strand_minus:
                if (i < 3)
                    context_offsets[i+1] = context_offsets[0];
                else
                    context_offsets[i+1] = context_offsets[i] + prot_length + 1;
                break;

            case eNa_strand_both:
            case eNa_strand_unknown:
                context_offsets[i+1] = context_offsets[i] + prot_length + 1;
                break;
            }
        }
    } else {
        if (is_na) {
            for (int i = 0; i < nframes; i += nframes) {
                switch (strand) {
                case eNa_strand_plus:
                    context_offsets[i+1] = context_offsets[i] + length + 1;
                    context_offsets[i+2] = context_offsets[i+1];
                    break;

                case eNa_strand_minus:
                    context_offsets[i+1] = context_offsets[i];
                    context_offsets[i+2] = context_offsets[i+1] + length + 1;
                    break;

                case eNa_strand_both:
                    context_offsets[i+1] = context_offsets[i] + length + 1;
                    context_offsets[i+2] = context_offsets[i+1] + length + 1;
                    break;

                default:
                    break;
                }
            }
        } else {
            context_offsets[1] = length + 1;
        }
    }
    
    mi_QueryInfo->total_length = context_offsets[nframes];
}

void
CBl2Seq::x_SetupQuery()
{
    Uint1* buf = NULL;
    int buflen = 0;
    bool query_is_na, query_is_translated;
    Uint1 encoding;
    ENa_strand strand = m_Options->GetStrandOption();

    x_ResetQueryDs();

    query_is_na = (m_Program == CBlastOption::eBlastn ||
                   m_Program == CBlastOption::eBlastx ||
                   m_Program == CBlastOption::eTblastx);

    query_is_translated = (m_Program == CBlastOption::eBlastx ||
                           m_Program == CBlastOption::eTblastx);

    encoding = (m_Program == CBlastOption::eBlastn) ?  BLASTNA_ENCODING : 
                    (query_is_na ? NCBI4NA_ENCODING : BLASTP_ENCODING);

    x_SetupQueryInfo();

    if (query_is_translated) {

        Uint1* translation = NULL, *buf_var = NULL;
        TSeqPos orig_length = sequence::GetLength(*m_Query, m_Scope);
        TSeqPos trans_len = 
            mi_QueryInfo->context_offsets[mi_QueryInfo->last_context] + 1;
        Uint1Ptr gc = BLASTFindGeneticCode(m_Options->GetQueryGeneticCode());

        if ( !(translation = (Uint1*) malloc(sizeof(Uint1)*trans_len)))
            NCBI_THROW(CBlastException, eOutOfMemory, "Translation buffer");

        // Get both strands of the original nucleotide sequence
        buf = buf_var = BLASTGetSequence(*m_Query, encoding, buflen, m_Scope,
                eNa_strand_both, true);
        buf_var++;

        for (int i = 0; i < NUM_FRAMES; i++) {

            if (BLAST_GetQueryLength(mi_QueryInfo, i) == 0)
                continue;

            int offset = mi_QueryInfo->context_offsets[i];
            short frame = i < 3 ? i+1 : -i+2;

            BLAST_GetTranslation(buf_var, buf_var+orig_length+1, orig_length,
                    frame, &translation[offset], gc);
        }

        sfree(buf);
        if (gc) delete [] gc;

        // don't count the sentinel bytes
        BlastSetUp_SeqBlkNew(translation, trans_len, 0, &mi_Query, true);

    } else {

        buf = BLASTGetSequence(*m_Query, encoding, buflen, m_Scope, strand, 
                true);
        // Sentinel bytes are not counted in the sequence block
        //buflen = (strand == eNa_strand_both) ? buflen - 3 : buflen - 2;
        BlastSetUp_SeqBlkNew(buf, buflen - 2, 0, &mi_Query, true);

    }

    BlastMaskPtr filter_mask = NULL;
    Blast_MessagePtr blmsg = NULL;
    short st;

    st = BLAST_MainSetUp(m_Program, m_Options->GetQueryOpts(),
            m_Options->GetScoringOpts(), m_Options->GetLookupTableOpts(),
            m_Options->GetHitSavingOpts(), mi_Query, mi_QueryInfo,
            &mi_LookupSegments, &filter_mask, &mi_Sbp, &blmsg);
    // TODO: Check that lookup_segments are not filtering the whole sequence
    // (DoubleInt set to -1 -1)
    if (st != 0) {
        string msg = blmsg ? blmsg->message : "BLAST_MainSetUp failed";
        NCBI_THROW(CBlastException, eInternal, msg.c_str());
    }

    // Convert the BlastMaskPtr into a CSeq_loc
    mi_FilteredRegions = BLASTBlastMask2SeqLoc(filter_mask);
}

void
CBl2Seq::x_SetupSubjects()
{
    x_ResetSubjectDs();

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
    bool subj_is_na = (m_Program == CBlastOption::eBlastn  ||
                       m_Program == CBlastOption::eTblastn ||
                       m_Program == CBlastOption::eTblastx);

    Uint1 encoding = (subj_is_na ? NCBI2NA_ENCODING : BLASTP_ENCODING);

    //ENa_strand strand = m_Options->GetStrandOption(); // allow subject strand
    // selection?
    ENa_strand strand = eNa_strand_unknown;
    Int8 dblength = 0;

    ITERATE(TSeqLocVector, itr, m_Subjects) {

        Uint1* buf = NULL;  // stores compressed sequence
        int buflen = 0;     // length of the buffer above
        BLAST_SequenceBlkPtr subj = (BLAST_SequenceBlkPtr) 
                calloc(1, sizeof(BLAST_SequenceBlk));

        buf = BLASTGetSequence(**itr, encoding, buflen, m_Scope, strand, false);

        if (subj_is_na) {
            subj->sequence = buf;
            subj->sequence_allocated = TRUE;

            encoding = (m_Program == CBlastOption::eBlastn) ? 
                BLASTNA_ENCODING : NCBI4NA_ENCODING;
            bool use_sentinels = (m_Program == CBlastOption::eBlastn) ?
                true : false;

            // Retrieve the sequence with ambiguities
            buf = BLASTGetSequence(**itr, encoding, buflen, m_Scope, strand, 
                    use_sentinels);
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
        mi_MaxSubjLength = MAX(mi_MaxSubjLength, sequence::GetLength(**itr,
                    m_Scope));
        _TRACE("*** Subject sequence of " << buflen << " bytes");
    }
    m_Options->SetDbSeqNum(mi_vSubjects.size());
    m_Options->SetDbLength(dblength);
}

void 
CBl2Seq::SetupSearch()
{
    if (!m_QuerySetUpDone) {
        m_Program = m_Options->GetProgram();  // options might have changed
        x_SetupQuery();
        LookupTableWrapInit(mi_Query, m_Options->GetLookupTableOpts(),
                mi_LookupSegments, mi_Sbp, &mi_LookupTable);
        m_QuerySetUpDone = true;
    }

    x_SetupSubjects();
}

void 
CBl2Seq::ScanDB()
{
    ITERATE(vector<BLAST_SequenceBlkPtr>, itr, mi_vSubjects) {

        BlastResultsPtr result = NULL;
        BlastReturnStat return_stats = { 0 };

        BLAST_ResultsInit(mi_QueryInfo->num_queries, &result);

        /*int total_hits = BLAST_SearchEngineCore(mi_Query, mi_LookupTable, 
            mi_QueryInfo, NULL, *itr,
            mi_ExtnWord, mi_GapAlign, m_Options->GetScoringOpts(), 
            mi_InitWordParams, mi_ExtnParams, mi_HitSavingParams, NULL,
            m_Options->GetDbOpts(), &result, &return_stats);
        _TRACE("*** BLAST_SearchEngineCore hits " << total_hits << " ***");*/

        BLAST_TwoSequencesEngine(m_Program, mi_Query, mi_QueryInfo, *itr,
                mi_Sbp, m_Options->GetScoringOpts(), mi_LookupTable,
                m_Options->GetInitWordOpts(), m_Options->GetExtensionOpts(),
                m_Options->GetHitSavingOpts(), m_Options->GetEffLenOpts(),
                NULL, m_Options->GetDbOpts(), result, &return_stats);
                
        mi_vResults.push_back(result);
        mi_vReturnStats.push_back(return_stats);
    }
}

void
CBl2Seq::Traceback()
{
#if 0
    for (unsigned int i = 0; i < mi_vResults.size(); i++) {
        BLAST_TwoSequencesTraceback(m_Program, mi_vResults[i], mi_Query,
                mi_QueryInfo, mi_vSubjects[i], mi_GapAlign, 
                m_Options->GetScoringOpts(), mi_ExtnParams, mi_HitSavingParams);
    }
#endif
}

CRef<CSeq_align_set>
CBl2Seq::x_Results2SeqAlign()
{
    _TRACE("*** Calling results2seqalign ***");
    CRef<CSeq_align_set> retval(new CSeq_align_set());
    CConstRef<CSeq_id> query_id(&sequence::GetId(*m_Query, m_Scope));
    vector< CConstRef<CSeq_id> > query_vector;
    query_vector.push_back(query_id);

    for (unsigned int i = 0; i < mi_vResults.size(); i++) {

        CConstRef<CSeq_id> subj_id(&sequence::GetId(*m_Subjects[i]));
        CRef<CSeq_align_set> seqalign = BLAST_Results2CppSeqAlign(
                mi_vResults[i], m_Program, query_vector, NULL,
                subj_id, m_Options->GetScoringOpts(), mi_Sbp);
        if (seqalign)
            retval->Set().merge(seqalign->Set());
    }

    // Clean up structures
    mi_InitWordParams.CBlastInitialWordParametersPtr::~CBlastInitialWordParametersPtr();
    mi_HitSavingParams.CBlastHitSavingParametersPtr::~CBlastHitSavingParametersPtr();
    mi_ExtnWord.CBLAST_ExtendWordPtr::~CBLAST_ExtendWordPtr();
    mi_ExtnParams.CBlastExtensionParametersPtr::~CBlastExtensionParametersPtr();
    mi_GapAlign.CBlastGapAlignStructPtr::~CBlastGapAlignStructPtr();

    return retval;
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
