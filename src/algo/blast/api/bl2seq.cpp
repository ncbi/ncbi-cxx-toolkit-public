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
#include "blast_seqalign.hpp"
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_traceback.h>

#ifndef NUM_FRAMES
#define NUM_FRAMES 6
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBl2Seq::CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p)
    : m_pOptions(new CBlastOptions(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_Init(queries, subjects);
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 EProgram p)
    : m_pOptions(new CBlastOptions(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 EProgram p)
    : m_pOptions(new CBlastOptions(p)), m_eProgram(p), mi_bQuerySetUpDone(false)
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
    delete m_pOptions;
}

/// Resets query data structures
void
CBl2Seq::x_ResetQueryDs()
{
    mi_bQuerySetUpDone = false;
    // should be changed if derived classes are created
    mi_clsQueries.Reset(NULL);
    mi_clsQueryInfo.Reset(NULL);
    mi_pScoreBlock = BlastScoreBlkFree(mi_pScoreBlock);
    mi_pLookupTable = LookupTableWrapFree(mi_pLookupTable);
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

    if (seqalignv.size()) {
        return seqalignv[0];
    } else {
        return null;
    }
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


void 
CBl2Seq::SetupSearch()
{
    if ( !mi_bQuerySetUpDone ) {
        x_ResetQueryDs();
        m_eProgram = m_pOptions->GetProgram();  // options might have changed
        SetupQueryInfo(m_tQueries, *m_pOptions, &mi_clsQueryInfo);
        SetupQueries(m_tQueries, *m_pOptions, mi_clsQueryInfo, &mi_clsQueries);

        // FIXME
        BlastMask* filter_mask = NULL;
        Blast_Message* blmsg = NULL;
        short st;

        st = BLAST_MainSetUp(m_eProgram, m_pOptions->GetQueryOpts(),
                             m_pOptions->GetScoringOpts(),
                             m_pOptions->GetLookupTableOpts(),
                             m_pOptions->GetHitSavingOpts(), mi_clsQueries, 
                             mi_clsQueryInfo, &mi_pLookupSegments, 
                             &filter_mask, &mi_pScoreBlock, &blmsg);

        // TODO: Check that lookup_segments are not filtering the whole 
        // sequence (DoubleInt set to -1 -1)
        if (st != 0) {
            string msg = blmsg ? blmsg->message : "BLAST_MainSetUp failed";
            NCBI_THROW(CBlastException, eInternal, msg.c_str());
        }

        // Convert the BlastMask* into a CSeq_loc
        // TODO: Implement this! 
        //mi_vFilteredRegions = BLASTBlastMask2SeqLoc(filter_mask);
        BlastMaskFree(filter_mask); // FIXME, return seqlocs for formatter

        LookupTableWrapInit(mi_clsQueries, m_pOptions->GetLookupTableOpts(),
                            mi_pLookupSegments, mi_pScoreBlock, 
                            &mi_pLookupTable);
        mi_bQuerySetUpDone = true;
    }

    x_ResetSubjectDs();
    SetupSubjects(m_tSubjects, m_pOptions, &mi_vSubjects, &mi_iMaxSubjLength);
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
    ASSERT(mi_vResults.size() == m_tSubjects.size());
    TSeqAlignVector retval;

    for (unsigned int index = 0; index < m_tSubjects.size(); ++index)
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
                for (unsigned int i = 0; i < retval.size(); ++i) {
                    retval[i]->Set().splice(retval[i]->Set().end(), 
                                            seqalign[i]->Set());
                }
            }
        }
    }

    // Clean up structures
    mi_clsInitWordParams.Reset(NULL);
    mi_clsHitSavingParams.Reset(NULL);
    mi_clsExtnWord.Reset(NULL);
    mi_clsExtnParams.Reset(NULL);
    mi_clsGapAlign.Reset(NULL);

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.33  2003/09/11 17:45:03  camacho
 * Changed CBlastOption -> CBlastOptions
 *
 * Revision 1.32  2003/09/10 20:01:30  dondosha
 * Use lookup_wrap.h
 *
 * Revision 1.31  2003/09/09 20:31:13  camacho
 * Add const type qualifier
 *
 * Revision 1.30  2003/09/09 15:18:02  camacho
 * Fix includes
 *
 * Revision 1.29  2003/09/09 12:55:09  camacho
 * Moved setup member functions to blast_setup_cxx.cpp
 *
 * Revision 1.28  2003/09/03 19:36:58  camacho
 * Fix include path for blast_setup.hpp, code clean up
 *
 * Revision 1.27  2003/08/28 22:42:54  camacho
 * Change BLASTGetSequence signature
 *
 * Revision 1.26  2003/08/28 17:37:06  camacho
 * Fix memory leaks, properly reset structure wrappers
 *
 * Revision 1.25  2003/08/28 15:48:23  madden
 * Make buf one longer for sentinel byte
 *
 * Revision 1.24  2003/08/25 17:15:49  camacho
 * Removed redundant typedef
 *
 * Revision 1.23  2003/08/19 22:12:47  dondosha
 * Cosmetic changes
 *
 * Revision 1.22  2003/08/19 20:27:06  dondosha
 * EProgram enum type is no longer part of CBlastOptions class
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
