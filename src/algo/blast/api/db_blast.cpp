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

#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_seqalign.hpp"
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_message.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* bssp,
                   EProgram p)
    : mi_bQuerySetUpDone(false), m_pSeqSrc(bssp) 
{
    m_tQueries = queries;
    mi_pScoreBlock = NULL;
    mi_pLookupTable = NULL;
    mi_pLookupSegments = NULL;
    mi_pFilteredRegions = NULL;
    mi_pResults = NULL;
    mi_pReturnStats = NULL;
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
    // Set the database length and number of sequences here
    m_OptsHandle->SetDbLength(BLASTSeqSrcGetTotLen(bssp));
    m_OptsHandle->SetDbSeqNum(BLASTSeqSrcGetNumSeqs(bssp));
}

CDbBlast::CDbBlast(const TSeqLocVector& queries, 
                   BlastSeqSrc* bssp, CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false), m_pSeqSrc(bssp) 
{
    m_tQueries = queries;
    mi_pScoreBlock = NULL;
    mi_pLookupTable = NULL;
    mi_pLookupSegments = NULL;
    mi_pFilteredRegions = NULL;
    mi_pResults = NULL;
    mi_pReturnStats = NULL;
    m_OptsHandle.Reset(&opts);    
    // Set the database length and number of sequences here, since 
    // these are calculated, and might not be filled in the passed argument
    m_OptsHandle->SetDbLength(BLASTSeqSrcGetTotLen(bssp));
    m_OptsHandle->SetDbSeqNum(BLASTSeqSrcGetNumSeqs(bssp));
}

CDbBlast::~CDbBlast()
{ 
    x_ResetQueryDs();
}


/// Resets query data structures
void
CDbBlast::x_ResetQueryDs()
{
    mi_bQuerySetUpDone = false;
    // should be changed if derived classes are created
    mi_clsQueries.Reset(NULL);
    mi_clsQueryInfo.Reset(NULL);
    mi_pScoreBlock = BlastScoreBlkFree(mi_pScoreBlock);
    mi_pLookupTable = LookupTableWrapFree(mi_pLookupTable);
    mi_pLookupSegments = ListNodeFreeData(mi_pLookupSegments);

    mi_pFilteredRegions = BlastMaskLocFree(mi_pFilteredRegions);
    mi_pResults = BLAST_ResultsFree(mi_pResults);
    
    sfree(mi_pReturnStats);
    NON_CONST_ITERATE(TBlastError, itr, mi_vErrors) {
        *itr = Blast_MessageFree(*itr);
    }

}

int CDbBlast::SetupSearch()
{
    int status = 0;
    EProgram x_eProgram = m_OptsHandle->GetOptions().GetProgram();

    if ( !mi_bQuerySetUpDone ) {
        x_ResetQueryDs();
        bool translated_query = (x_eProgram == eBlastx || 
                                 x_eProgram == eTblastx);

        SetupQueryInfo(m_tQueries, m_OptsHandle->GetOptions(), 
                       &mi_clsQueryInfo);
        SetupQueries(m_tQueries, m_OptsHandle->GetOptions(), 
                     mi_clsQueryInfo, &mi_clsQueries);

        mi_pScoreBlock = 0;

        Blast_Message* blast_message = NULL;

        status = BLAST_MainSetUp(x_eProgram, 
                                 m_OptsHandle->GetOptions().GetQueryOpts(),
                                 m_OptsHandle->GetOptions().GetScoringOpts(),
                                 m_OptsHandle->GetOptions().GetHitSaveOpts(),
                                 mi_clsQueries, mi_clsQueryInfo,
                                 &mi_pLookupSegments, &mi_pFilteredRegions,
                                 &mi_pScoreBlock, &blast_message);
        if (status != 0) {
            string msg = blast_message ? blast_message->message : 
                "BLAST_MainSetUp failed";
            NCBI_THROW(CBlastException, eInternal, msg.c_str());
        } else if (blast_message) {
            mi_vErrors.push_back(blast_message);
        }
        
        if (translated_query) {
            /* Filter locations were returned in protein coordinates; 
               convert them back to nucleotide here */
            BlastMaskLocProteinToDNA(&mi_pFilteredRegions, m_tQueries);
        }

        BLAST_ResultsInit(mi_clsQueryInfo->num_queries, &mi_pResults);
        LookupTableWrapInit(mi_clsQueries, 
            m_OptsHandle->GetOptions().GetLutOpts(), 
            mi_pLookupSegments, mi_pScoreBlock, &mi_pLookupTable, NULL);
        
        mi_bQuerySetUpDone = true;
        mi_pReturnStats = (BlastReturnStat*) calloc(1, sizeof(BlastReturnStat));
    }
    return status;
}

void CDbBlast::PartialRun()
{
    SetupSearch();
    m_OptsHandle->GetOptions().Validate();
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
 * 
 * @param results All results from a BLAST run [in] [out]
 * @param total_hsp_limit Maximal number of HSPs to return per query 
 *                        sequence. No bound if 0. [in]
 * @return Has the HSP limit been exceeded? 
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
    
    for (query_index = 0; query_index < mi_pResults->num_queries; 
         ++query_index) {
        if (!(hit_list = mi_pResults->hitlist_array[query_index]))
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
                    BlastHSPFree(hsp_list->hsp_array[hsp_index]);
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
        mi_vErrors.push_back(blast_message);
    }
}

void 
CDbBlast::RunSearchEngine()
{
    BLAST_DatabaseSearchEngine(m_OptsHandle->GetOptions().GetProgram(),
         mi_clsQueries, mi_clsQueryInfo, 
         m_pSeqSrc, mi_pScoreBlock, 
         m_OptsHandle->GetOptions().GetScoringOpts(), 
         mi_pLookupTable, m_OptsHandle->GetOptions().GetInitWordOpts(), 
         m_OptsHandle->GetOptions().GetExtnOpts(), 
         m_OptsHandle->GetOptions().GetHitSaveOpts(), 
         m_OptsHandle->GetOptions().GetEffLenOpts(), NULL, 
         m_OptsHandle->GetOptions().GetDbOpts(),
         mi_pResults, mi_pReturnStats);

    mi_pLookupTable = LookupTableWrapFree(mi_pLookupTable);
    mi_clsQueries = BlastSequenceBlkFree(mi_clsQueries);

    /* The following works because the ListNodes' data point to simple
       double-integer structures */
    mi_pLookupSegments = ListNodeFreeData(mi_pLookupSegments);

    /* If a limit is provided for number of HSPs to return, trim the extra
       HSPs here */
    TrimBlastHSPResults();
}

TSeqAlignVector
CDbBlast::x_Results2SeqAlign()
{
    TSeqAlignVector retval;

    retval = BLAST_Results2CSeqAlign(mi_pResults, 
                 m_OptsHandle->GetOptions().GetProgram(),
                 m_tQueries, m_pSeqSrc, 0, 
                 m_OptsHandle->GetOptions().GetScoringOpts(), 
                 mi_pScoreBlock);

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
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
