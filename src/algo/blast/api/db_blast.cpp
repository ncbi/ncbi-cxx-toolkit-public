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
 * Author:  Ilya Dondoshansky
 *
 * File Description:
 *   Database BLAST class interface
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_seqalign.hpp"
#include "blast_setup.hpp"
#include <connect/ncbi_core.h>

// Core BLAST engine includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/core/hspstream_collector.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void CDbBlast::x_InitFields()
{
    m_ibQuerySetUpDone = false;
    m_ipScoreBlock = NULL;
    m_ipLookupTable = NULL;
    m_ipLookupSegments = NULL;
    m_ipFilteredRegions = NULL;
    m_ipResults = NULL;
    m_ipDiagnostics = NULL;
    m_OptsHandle->SetDbLength(BLASTSeqSrcGetTotLen(m_pSeqSrc));
    m_OptsHandle->SetDbSeqNum(BLASTSeqSrcGetNumSeqs(m_pSeqSrc));
}

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                   EProgram p, RPSInfo* rps_info, BlastHSPStream* hsp_stream,
                   MT_LOCK lock)
    : m_tQueries(queries), m_pSeqSrc(seq_src), m_pRpsInfo(rps_info), 
      m_pHspStream(hsp_stream), m_pLock(lock) 
{
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
    x_InitFields();
}

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* seq_src, 
                   CBlastOptionsHandle& opts, RPSInfo* rps_info, 
                   BlastHSPStream* hsp_stream, MT_LOCK lock)
    : m_tQueries(queries), m_pSeqSrc(seq_src), m_pRpsInfo(rps_info), 
      m_pHspStream(hsp_stream), m_pLock(lock) 
{
    m_OptsHandle.Reset(&opts);    
    x_InitFields();
}

CDbBlast::~CDbBlast()
{ 
    x_ResetQueryDs();
}

/// Resets results data structures
void
CDbBlast::x_ResetResultDs()
{
    m_ipResults = Blast_HSPResultsFree(m_ipResults);
    
    m_ipDiagnostics = Blast_DiagnosticsFree(m_ipDiagnostics);
    NON_CONST_ITERATE(TBlastError, itr, m_ivErrors) {
        *itr = Blast_MessageFree(*itr);
    }
}

/// Resets query data structures
void
CDbBlast::x_ResetQueryDs()
{
    m_ibQuerySetUpDone = false;
    // should be changed if derived classes are created
    m_iclsQueries.Reset(NULL);
    m_iclsQueryInfo.Reset(NULL);
    m_ipScoreBlock = BlastScoreBlkFree(m_ipScoreBlock);
    m_ipLookupTable = LookupTableWrapFree(m_ipLookupTable);
    m_ipLookupSegments = ListNodeFreeData(m_ipLookupSegments);

    m_ipFilteredRegions = BlastMaskLocFree(m_ipFilteredRegions);
    x_ResetResultDs();
}

int CDbBlast::SetupSearch()
{
    int status = 0;
    EProgram x_eProgram = m_OptsHandle->GetOptions().GetProgram();

    if ( !m_ibQuerySetUpDone ) {
        double scale_factor;

        x_ResetQueryDs();
        bool translated_query = (x_eProgram == eBlastx || 
                                 x_eProgram == eTblastx);

        SetupQueryInfo(m_tQueries, m_OptsHandle->GetOptions(), 
                       &m_iclsQueryInfo);
        SetupQueries(m_tQueries, m_OptsHandle->GetOptions(), 
                     m_iclsQueryInfo, &m_iclsQueries);

        m_ipScoreBlock = 0;

        if (x_eProgram == eRPSBlast || x_eProgram == eRPSTblastn)
            scale_factor = m_pRpsInfo->aux_info.scale_factor;
        else
            scale_factor = 1.0;

        Blast_Message* blast_message = NULL;

        status = BLAST_MainSetUp(x_eProgram, 
                                 m_OptsHandle->GetOptions().GetQueryOpts(),
                                 m_OptsHandle->GetOptions().GetScoringOpts(),
                                 m_OptsHandle->GetOptions().GetHitSaveOpts(),
                                 m_iclsQueries, m_iclsQueryInfo,
                                 scale_factor,
                                 &m_ipLookupSegments, &m_ipFilteredRegions,
                                 &m_ipScoreBlock, &blast_message);
        if (status != 0) {
            string msg = blast_message ? blast_message->message : 
                "BLAST_MainSetUp failed";
            Blast_MessageFree(blast_message);
            NCBI_THROW(CBlastException, eInternal, msg.c_str());
        } else if (blast_message) {
            // Non-fatal error message; just save it.
            m_ivErrors.push_back(blast_message);
        }
        
        if (translated_query) {
            /* Filter locations were returned in protein coordinates; 
               convert them back to nucleotide here */
            BlastMaskLocProteinToDNA(&m_ipFilteredRegions, m_tQueries);
        }

        LookupTableWrapInit(m_iclsQueries, 
            m_OptsHandle->GetOptions().GetLutOpts(), 
            m_ipLookupSegments, m_ipScoreBlock, &m_ipLookupTable, m_pRpsInfo);
        
        m_ibQuerySetUpDone = true;
        m_ipDiagnostics = Blast_DiagnosticsInit();
    }
    return status;
}

void CDbBlast::PartialRun()
{
    m_OptsHandle->GetOptions().Validate();
    SetupSearch();
    RunSearchEngine();
}

TSeqAlignVector
CDbBlast::Run()
{
    m_OptsHandle->GetOptions().Validate();// throws an exception on failure
    SetupSearch();
    RunSearchEngine();
    return x_Results2SeqAlign();
}

/* Comparison function for sorting HSP lists in increasing order of the 
   number of HSPs in a hit. Needed for TrimBlastHSPResults below. */
extern "C" int
compare_hsplist_hspcnt(const void* v1, const void* v2)
{
   BlastHSPList* r1 = *((BlastHSPList**) v1);
   BlastHSPList* r2 = *((BlastHSPList**) v2);

   if (r1->hspcnt < r2->hspcnt)
      return -1;
   else if (r1->hspcnt > r2->hspcnt)
      return 1;
   else
      return 0;
}


/** Removes extra results if a limit is imposed on the total number of HSPs
 * returned. Makes sure that at least 1 HSP is still returned for each
 * database sequence hit. 
 */
void 
CDbBlast::TrimBlastHSPResults()
{
    int total_hsp_limit = m_OptsHandle->GetOptions().GetTotalHspLimit();

    if (total_hsp_limit == 0)
        return;

    Int4 total_hsps = 0;
    Int4 allowed_hsp_num, hsplist_count;
    bool hsp_limit_exceeded = false;
    BlastHitList* hit_list;
    BlastHSPList* hsp_list;
    BlastHSPList** hsplist_array;
    int query_index, subject_index, hsp_index;
    
    for (query_index = 0; query_index < m_ipResults->num_queries; 
         ++query_index) {
        if (!(hit_list = m_ipResults->hitlist_array[query_index]))
            continue;
        /* The count of HSPs is separate for each query. */
        total_hsps = 0;
        hsplist_count = hit_list->hsplist_count;
        hsplist_array = (BlastHSPList**) 
            malloc(hsplist_count*sizeof(BlastHSPList*));
        
        for (subject_index = 0; subject_index < hsplist_count; ++subject_index)
            hsplist_array[subject_index] = 
                hit_list->hsplist_array[subject_index];
        
        qsort((void*)hsplist_array, hsplist_count,
              sizeof(BlastHSPList*), compare_hsplist_hspcnt);
        
        for (subject_index = 0; subject_index < hsplist_count; 
             ++subject_index) {
            allowed_hsp_num = 
                ((subject_index+1)*total_hsp_limit)/hsplist_count - total_hsps;
            hsp_list = hsplist_array[subject_index];
            if (hsp_list->hspcnt > allowed_hsp_num) {
                hsp_limit_exceeded = true;
                /* Free the extra HSPs */
                for (hsp_index = allowed_hsp_num; 
                     hsp_index < hsp_list->hspcnt; ++hsp_index)
                    Blast_HSPFree(hsp_list->hsp_array[hsp_index]);
                hsp_list->hspcnt = allowed_hsp_num;
            }
            total_hsps += hsp_list->hspcnt;
        }
        sfree(hsplist_array);
    }
    if (hsp_limit_exceeded) {
        Blast_Message* blast_message = NULL;
        Blast_MessageWrite(&blast_message, BLAST_SEV_INFO, 0, 0, 
                           "Too many HSPs to save all");
        m_ivErrors.push_back(blast_message);
    }
}

void 
CDbBlast::RunSearchEngine()
{
    BlastHSPStream* hsp_stream = m_pHspStream;
    BlastHSPResults** results_ptr = NULL;

    // If HSP stream has been passed from the user, it means that results are
    // handled outside of the BLAST engine, and hence we need to pass a NULL 
    // results pointer to the engine.
    if (!hsp_stream) {
        Int4 num_results;
        
        /* For RPS BLAST, number of "queries" in HSP stream is equal to 
           number of sequences in the RPS BLAST database, because queries 
           and subjects are switched in an RPS search. In all other cases,
           it is the real number of queries. */
        num_results = ((m_ipLookupTable->lut_type == RPS_LOOKUP_TABLE) ?
                       BLASTSeqSrcGetNumSeqs(m_pSeqSrc) : 
                       m_iclsQueryInfo->num_queries);
        hsp_stream = Blast_HSPListCollectorInitMT(
                        m_OptsHandle->GetOptions().GetProgram(), 
                        m_OptsHandle->GetOptions().GetHitSaveOpts(),
                        num_results, TRUE, m_pLock);
        results_ptr = &m_ipResults;
    }

    if (m_ipLookupTable->lut_type == RPS_LOOKUP_TABLE) {
        BLAST_RPSSearchEngine(m_OptsHandle->GetOptions().GetProgram(), 
            m_iclsQueries, m_iclsQueryInfo, m_pSeqSrc, m_ipScoreBlock,
            m_OptsHandle->GetOptions().GetScoringOpts(), 
            m_ipLookupTable, m_OptsHandle->GetOptions().GetInitWordOpts(), 
            m_OptsHandle->GetOptions().GetExtnOpts(), 
            m_OptsHandle->GetOptions().GetHitSaveOpts(),
            m_OptsHandle->GetOptions().GetEffLenOpts(),
            m_OptsHandle->GetOptions().GetPSIBlastOpts(), 
            m_OptsHandle->GetOptions().GetDbOpts(),
            hsp_stream, m_ipDiagnostics, results_ptr);
    } else {
        BLAST_SearchEngine(m_OptsHandle->GetOptions().GetProgram(),
            m_iclsQueries, m_iclsQueryInfo, m_pSeqSrc, m_ipScoreBlock, 
            m_OptsHandle->GetOptions().GetScoringOpts(), 
            m_ipLookupTable, m_OptsHandle->GetOptions().GetInitWordOpts(), 
            m_OptsHandle->GetOptions().GetExtnOpts(), 
            m_OptsHandle->GetOptions().GetHitSaveOpts(), 
            m_OptsHandle->GetOptions().GetEffLenOpts(), NULL, 
            m_OptsHandle->GetOptions().GetDbOpts(),
            hsp_stream, m_ipDiagnostics, results_ptr);
    }

    // Only free the HSP stream, if it was allocated locally
    if (!m_pHspStream)
        hsp_stream = BlastHSPStreamFree(hsp_stream);

    m_ipLookupTable = LookupTableWrapFree(m_ipLookupTable);

    /* The following works because the ListNodes' data point to simple
       double-integer structures */
    m_ipLookupSegments = ListNodeFreeData(m_ipLookupSegments);

    /* If a limit is provided for number of HSPs to return, trim the extra
       HSPs here */
    TrimBlastHSPResults();
}

TSeqAlignVector
CDbBlast::x_Results2SeqAlign()
{
    TSeqAlignVector retval;

    if (!m_ipResults)
        return retval;

    bool gappedMode = m_OptsHandle->GetOptions().GetGappedMode();
    bool outOfFrameMode = m_OptsHandle->GetOptions().GetOutOfFrameMode();

    retval = BLAST_Results2CSeqAlign(m_ipResults, 
                 m_OptsHandle->GetOptions().GetProgram(),
                 m_tQueries, m_pSeqSrc, 
                 gappedMode, outOfFrameMode);

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.34  2004/06/23 14:06:15  dondosha
 * Added MT_LOCK argument in constructors
 *
 * Revision 1.33  2004/06/15 18:49:07  dondosha
 * Added optional BlastHSPStream argument to constructors, to allow use of HSP
 * stream by a separate thread doing on-the-fly formatting
 *
 * Revision 1.32  2004/06/08 15:20:44  dondosha
 * Use BlastHSPStream interface
 *
 * Revision 1.31  2004/06/07 21:34:55  dondosha
 * Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
 *
 * Revision 1.30  2004/06/07 18:26:29  dondosha
 * Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
 *
 * Revision 1.29  2004/06/02 15:57:06  bealer
 * - Isolate object manager dependent code.
 *
 * Revision 1.28  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.27  2004/05/17 18:12:29  bealer
 * - Add PSI-Blast support.
 *
 * Revision 1.26  2004/05/14 17:16:12  dondosha
 * BlastReturnStat structure changed to BlastDiagnostics and refactored
 *
 * Revision 1.25  2004/05/07 15:28:41  papadopo
 * add scale factor to BlastMainSetup
 *
 * Revision 1.24  2004/05/05 15:28:56  dondosha
 * Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
 *
 * Revision 1.23  2004/04/30 16:53:06  dondosha
 * Changed a number of function names to have the same conventional Blast_ prefix
 *
 * Revision 1.22  2004/04/21 17:33:46  madden
 * Rename BlastHSPFree to Blast_HSPFree
 *
 * Revision 1.21  2004/03/24 22:12:46  dondosha
 * Fixed memory leaks
 *
 * Revision 1.20  2004/03/17 14:51:33  camacho
 * Fix compiler errors
 *
 * Revision 1.19  2004/03/16 23:32:28  dondosha
 * Added capability to run RPS BLAST seach; added function x_InitFields; changed mi_ to m_i in member field names
 *
 * Revision 1.18  2004/03/16 14:45:28  dondosha
 * Removed doxygen comments for nonexisting parameters
 *
 * Revision 1.17  2004/03/15 19:57:00  dondosha
 * Merged TwoSequences and Database engines
 *
 * Revision 1.16  2004/03/10 17:37:36  papadopo
 * add (unused) RPS info pointer to LookupTableWrapInit()
 *
 * Revision 1.15  2004/02/24 20:31:39  dondosha
 * Typo fix; removed irrelevant CVS log comments
 *
 * Revision 1.14  2004/02/24 18:19:35  dondosha
 * Removed lookup options argument from call to BLAST_MainSetUp
 *
 * Revision 1.13  2004/02/23 15:45:09  camacho
 * Eliminate compiler warning about qsort
 *
 * Revision 1.12  2004/02/19 21:12:02  dondosha
 * Added handling of error messages; fill info message in TrimBlastHSPResults
 *
 * Revision 1.11  2004/02/18 23:49:08  dondosha
 * Added TrimBlastHSPResults method to remove extra HSPs if limit on total number is provided
 *
 * Revision 1.10  2004/02/13 20:47:20  madden
 * Throw exception rather than ERR_POST if setup fails
 *
 * Revision 1.9  2004/02/13 19:32:55  camacho
 * Removed unnecessary #defines
 *
 * Revision 1.8  2004/01/16 21:51:34  bealer
 * - Changes for Blast4 API
 *
 * Revision 1.7  2003/12/15 23:42:46  dondosha
 * Set database length and number of sequences options in constructor
 *
 * Revision 1.6  2003/12/15 15:56:42  dondosha
 * Added constructor with options handle argument
 *
 * Revision 1.5  2003/12/03 17:41:19  camacho
 * Fix incorrect member initializer list
 *
 * Revision 1.4  2003/12/03 16:45:03  dondosha
 * Use CBlastOptionsHandle class
 *
 * Revision 1.3  2003/11/26 18:36:45  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.2  2003/10/30 21:41:12  dondosha
 * Removed unneeded extra argument from call to BLAST_Results2CSeqAlign
 *
 * Revision 1.1  2003/10/29 22:37:36  dondosha
 * Database BLAST search class methods
 * ===========================================================================
 */
