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
#include <algo/blast/core/blast_traceback.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* bssp,
                   EProgram p)
    : m_pSeqSrc(bssp), mi_bQuerySetUpDone(false) 
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
    : m_pSeqSrc(bssp), mi_bQuerySetUpDone(false) 
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


    // TODO: should clean filtered regions?
}

int CDbBlast::SetupSearch()
{
    Blast_Message* blast_message = NULL;
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

        status = BLAST_MainSetUp(x_eProgram, 
                                 m_OptsHandle->GetOptions().GetQueryOpts(),
                                 m_OptsHandle->GetOptions().GetScoringOpts(),
                                 m_OptsHandle->GetOptions().GetLutOpts(),
                                 m_OptsHandle->GetOptions().GetHitSaveOpts(),
                                 mi_clsQueries, mi_clsQueryInfo,
                                 &mi_pLookupSegments, &mi_pFilteredRegions,
                                 &mi_pScoreBlock, &blast_message);
        if (status != 0) {
            string msg = blast_message ? blast_message->message : "BLAST_MainSetUp failed";
            NCBI_THROW(CBlastException, eInternal, msg.c_str());
        }
        
        if (translated_query) {
            /* Filter locations were returned in protein coordinates; 
               convert them back to nucleotide here */
            BlastMaskLocProteinToDNA(&mi_pFilteredRegions, m_tQueries);
        }

        BLAST_ResultsInit(mi_clsQueryInfo->num_queries, &mi_pResults);
        LookupTableWrapInit(mi_clsQueries, 
            m_OptsHandle->GetOptions().GetLutOpts(), 
            mi_pLookupSegments, mi_pScoreBlock, &mi_pLookupTable);
        
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
    SetupSearch();
    //m_OptsHandle->GetOptions()->DebugDumpText(cerr, "m_pOptions", 1);
    m_OptsHandle->GetOptions().Validate();// throws an exception on failure
    RunSearchEngine();
    return x_Results2SeqAlign();
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
 *
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
 * Renamed conversion functions between BlastMaskLoc and CSeqLoc; added algo/blast/core to headers from core BLAST library
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
