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
 * ===========================================================================
 */

/// @file bl2seq.cpp
/// Implementation of CBl2Seq class.

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/api/seqinfosrc_seqvec.hpp>
#include "dust_filter.hpp"
#include "repeats_filter.hpp"
#include "blast_seqalign.hpp"
#include "blast_objmgr_priv.hpp"
#include <algo/blast/api/objmgr_query_data.hpp>

// NewBlast includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/hspstream_collector.h>



/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBl2Seq::CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject,
                 CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    TSeqLocVector subjects;
    queries.push_back(query);
    subjects.push_back(subject);

    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 EProgram p)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 EProgram p)
    : mi_bQuerySetUpDone(false)
{
    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false)
{
    x_InitSeqs(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

void CBl2Seq::x_InitSeqs(const TSeqLocVector& queries, 
                         const TSeqLocVector& subjs)
{
    m_tQueries = queries;
    m_tSubjects = subjs;
    mi_pScoreBlock = NULL;
    mi_pLookupTable = NULL;
    mi_pLookupSegments = NULL;
    mi_pResults = NULL;
    mi_pDiagnostics = NULL;
    mi_pSeqSrc = NULL;
    m_ipFilteredRegions = NULL;
}

CBl2Seq::~CBl2Seq()
{ 
    x_ResetQueryDs();
    x_ResetSubjectDs();
}

void
CBl2Seq::x_ResetQueryDs()
{
    mi_bQuerySetUpDone = false;
    // should be changed if derived classes are created
    mi_clsQueries.Reset();
    mi_clsQueryInfo.Reset();
    mi_clsBlastMessage.Reset();
    m_Messages.clear();
    mi_pScoreBlock = BlastScoreBlkFree(mi_pScoreBlock);
    mi_pLookupTable = LookupTableWrapFree(mi_pLookupTable);
    mi_pLookupSegments = BlastSeqLocFree(mi_pLookupSegments);
    m_ipFilteredRegions = BlastMaskLocFree(m_ipFilteredRegions);
}

void
CBl2Seq::x_ResetSubjectDs()
{
    // Clean up structures and results from any previous search
    mi_pSeqSrc = BlastSeqSrcFree(mi_pSeqSrc);
    mi_pResults = Blast_HSPResultsFree(mi_pResults);
    mi_pDiagnostics = Blast_DiagnosticsFree(mi_pDiagnostics);
    // TODO: Should clear class wrappers for internal parameters structures?
    //      -> destructors will be called for them
    //m_OptsHandle->SetDbSeqNum(0);  // FIXME: Really needed?
    //m_OptsHandle->SetDbLength(0);  // FIXME: Really needed?
}

TSeqAlignVector
CBl2Seq::Run()
{
    //m_OptsHandle->GetOptions().DebugDumpText(cerr, "m_OptsHandle", 1);
    m_OptsHandle->GetOptions().Validate();  // throws an exception on failure
    SetupSearch();
    ScanDB();
    return x_Results2SeqAlign();
}

void
CBl2Seq::PartialRun()
{
    //m_OptsHandle->GetOptions().DebugDumpText(cerr, "m_OptsHandle", 1);
    m_OptsHandle->GetOptions().Validate();  // throws an exception on failure
    SetupSearch();
    ScanDB();
}

void 
CBl2Seq::SetupSearch()
{
    if ( !mi_bQuerySetUpDone ) {
        x_ResetQueryDs();
        const CBlastOptions& kOptions = m_OptsHandle->GetOptions();
        EBlastProgramType prog = kOptions.GetProgramType();
        ENa_strand strand_opt = kOptions.GetStrandOption();
        TAutoUint1ArrayPtr gc = FindGeneticCode(kOptions.GetQueryGeneticCode());

        if (CBlastNucleotideOptionsHandle *nucl_handle =
            dynamic_cast<CBlastNucleotideOptionsHandle*>(&*m_OptsHandle)) {
            Blast_FindDustFilterLoc(m_tQueries, nucl_handle);
            Blast_FindRepeatFilterLoc(m_tQueries, nucl_handle);
        }
        
        SetupQueryInfo(m_tQueries, prog, strand_opt, &mi_clsQueryInfo);
        SetupQueries(m_tQueries, mi_clsQueryInfo, &mi_clsQueries, 
                     prog, strand_opt, gc.get(), m_Messages);

        Blast_Message* blmsg = NULL;
        double scale_factor = 1.0;
        short st;
       
        st = BLAST_MainSetUp(m_OptsHandle->GetOptions().GetProgramType(), 
                             m_OptsHandle->GetOptions().GetQueryOpts(),
                             m_OptsHandle->GetOptions().GetScoringOpts(),
                             mi_clsQueries, mi_clsQueryInfo, scale_factor,
                             &mi_pLookupSegments, &m_ipFilteredRegions, &mi_pScoreBlock,
                             &blmsg);

        // TODO: Check that lookup_segments are not filtering the whole 
        // sequence (SSeqRange set to -1 -1)
        if (st != 0) {
            string msg = blmsg ? blmsg->message : "BLAST_MainSetUp failed";
            Blast_MessageFree(blmsg);
            NCBI_THROW(CBlastException, eCoreBlastError, msg);
        }
        Blast_MessageFree(blmsg);

        LookupTableWrapInit(mi_clsQueries, 
                            m_OptsHandle->GetOptions().GetLutOpts(),
                            mi_pLookupSegments, mi_pScoreBlock, 
                            &mi_pLookupTable, NULL);
        mi_bQuerySetUpDone = true;
    }

    x_ResetSubjectDs();

    mi_pSeqSrc = 
        MultiSeqBlastSeqSrcInit(m_tSubjects, 
                                m_OptsHandle->GetOptions().GetProgramType());
    char* error_str = BlastSeqSrcGetInitError(mi_pSeqSrc);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }

    // Set the hitlist size to the total number of subject sequences, to 
    // make sure that no hits are discarded.
    m_OptsHandle->SetHitlistSize((int) m_tSubjects.size());
}

void 
CBl2Seq::ScanDB()
{
    mi_pResults = NULL;
    mi_pDiagnostics = Blast_DiagnosticsInit();

    const CBlastOptions& kOptions = GetOptionsHandle().GetOptions();
    SBlastHitsParameters* blasthit_params=NULL;
    SBlastHitsParametersNew(kOptions.GetHitSaveOpts(), kOptions.GetExtnOpts(),
                            kOptions.GetScoringOpts(), &blasthit_params);
    /* Initialize an HSPList stream to collect hits; 
       results should not be sorted for reading from the stream. */
    BlastHSPStream* hsp_stream = 
        Blast_HSPListCollectorInit(kOptions.GetProgramType(), blasthit_params,
                                   mi_clsQueryInfo->num_queries, FALSE);
                  
    Int4 status = Blast_RunFullSearch(kOptions.GetProgramType(),
                                      mi_clsQueries, mi_clsQueryInfo, 
                                      mi_pSeqSrc, mi_pScoreBlock, 
                                      kOptions.GetScoringOpts(),
                                      mi_pLookupTable,
                                      kOptions.GetInitWordOpts(),
                                      kOptions.GetExtnOpts(),
                                      kOptions.GetHitSaveOpts(),
                                      kOptions.GetEffLenOpts(),
                                      NULL, kOptions.GetDbOpts(),
                                      hsp_stream, NULL, mi_pDiagnostics, 
                                      &mi_pResults);
    if (status) {
        string msg("Blast_RunFullSearch failed with status ");
        msg += NStr::IntToString(status);
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }
    hsp_stream = BlastHSPStreamFree(hsp_stream);
}


/* Unlike the database search, we want to make sure that a seqalign list is   
 * returned for each query/subject pair, even if it is empty. Also we don't 
 * want subjects to be sorted in seqalign results. Hence we retrieve results 
 * for each subject separately and append the resulting vectors of seqalign
 * lists. 
 */
TSeqAlignVector
CBl2Seq::x_Results2SeqAlign()
{
    EBlastProgramType program = m_OptsHandle->GetOptions().GetProgramType();
    bool gappedMode = m_OptsHandle->GetGappedMode();
    bool outOfFrameMode = m_OptsHandle->GetOptions().GetOutOfFrameMode();

    CSeqVecSeqInfoSrc seqinfo_src(m_tSubjects);
    CObjMgr_QueryFactory qf(m_tQueries);
    CRef<ILocalQueryData> query_data =
        qf.MakeLocalQueryData(&m_OptsHandle->GetOptions());

    return LocalBlastResults2SeqAlign(mi_pResults, *query_data, seqinfo_src,
                                      program, gappedMode, outOfFrameMode,
                                      eSequenceComparison);
}

TSeqLocInfoVector
CBl2Seq::GetFilteredQueryRegions() const
{
    CConstRef<CPacked_seqint> queries(TSeqLocVector2Packed_seqint(m_tQueries));
    EBlastProgramType program(GetOptionsHandle().GetOptions().GetProgramType());
    TSeqLocInfoVector retval;
    Blast_GetSeqLocInfoVector(program, *queries, m_ipFilteredRegions, retval);
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.87  2005/12/16 20:51:18  camacho
 * Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
 *
 * Revision 1.86  2005/11/09 20:56:26  camacho
 * Refactorings to allow CPsiBl2Seq to produce Seq-aligns in the same format
 * as CBl2Seq and reduce redundant code.
 *
 * Revision 1.85  2005/10/25 14:19:02  camacho
 * Perform repeats filtering as part of set up
 *
 * Revision 1.84  2005/09/22 14:49:58  madden
 * Validate options before calling SetupSearch
 *
 * Revision 1.83  2005/09/21 16:15:18  camacho
 * Throw exception if Blast_RunFullSearch fails
 *
 * Revision 1.82  2005/09/16 18:47:50  camacho
 * Remove dead code
 *
 * Revision 1.81  2005/09/16 17:03:09  camacho
 * Use new Blast_GetSeqLocInfoVector interface
 *
 * Revision 1.80  2005/08/29 14:38:48  camacho
 * From Ilya Dondoshansky:
 * GetFilteredQueryRegions now returns TSeqLocInfoVector
 *
 * Revision 1.79  2005/07/19 13:44:08  madden
 * Add call to Blast_FindDustFilterLoc
 *
 * Revision 1.78  2005/07/07 16:32:11  camacho
 * Revamping of BLAST exception classes and error codes
 *
 * Revision 1.77  2005/06/23 16:18:45  camacho
 * Doxygen fixes
 *
 * Revision 1.76  2005/06/09 20:35:29  camacho
 * Use new private header blast_objmgr_priv.hpp
 *
 * Revision 1.75  2005/06/08 19:13:37  camacho
 * Minor
 *
 * Revision 1.74  2005/05/24 20:02:42  camacho
 * Changed signature of SetupQueries and SetupQueryInfo
 *
 * Revision 1.73  2005/05/23 15:51:59  dondosha
 * Special case for preliminary hitlist size in RPS BLAST - hence no need for 2 extra parameters in SBlastHitsParametersNew
 *
 * Revision 1.72  2005/05/16 12:25:20  madden
 * Use SBlastHitsParameters in Blast_HSPListCollectorInit
 *
 * Revision 1.71  2005/05/12 15:35:35  camacho
 * Remove dead fields
 *
 * Revision 1.70  2005/04/06 21:06:18  dondosha
 * Use EBlastProgramType instead of EProgram in non-user-exposed functions
 *
 * Revision 1.69  2005/03/31 13:45:35  camacho
 * BLAST options API clean-up
 *
 * Revision 1.68  2005/03/29 17:15:51  dondosha
 * Use CBlastOptionsHandle instead of CBlastOptions to retrieve gapped mode
 *
 * Revision 1.67  2005/02/08 18:50:29  bealer
 * - Fix type truncation warnings.
 *
 * Revision 1.66  2005/01/06 15:42:01  camacho
 * Make use of modified signature to SetupQueries
 *
 * Revision 1.65  2004/12/21 17:18:04  dondosha
 * BLAST_SearchEngine renamed to Blast_RunFullSearch
 *
 * Revision 1.64  2004/11/17 20:26:33  camacho
 * Use new BlastSeqSrc initialization function names and check for initialization errors
 *
 * Revision 1.63  2004/10/06 14:53:36  dondosha
 * Remap subject coordinates in Seq-aligns separately after all Seq-aligns are filled; Use IBlastSeqInfoSrc interface in x_Results2CSeqAlign
 *
 * Revision 1.62  2004/09/13 12:46:07  madden
 * Replace call to ListNodeFreeData with BlastSeqLocFree
 *
 * Revision 1.61  2004/07/19 14:58:47  dondosha
 * Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
 *
 * Revision 1.60  2004/07/19 13:56:12  dondosha
 * Pass subject SSeqLoc directly to BLAST_OneSubjectResults2CSeqAlign instead of BlastSeqSrc
 *
 * Revision 1.59  2004/07/06 15:48:40  dondosha
 * Use EBlastProgramType enumeration type instead of EProgram when calling C code
 *
 * Revision 1.58  2004/06/29 14:17:24  papadopo
 * add PartialRun and GetResults methods to CBl2Seq
 *
 * Revision 1.57  2004/06/28 13:40:51  madden
 * Use BlastMaskInformation rather than BlastMaskLoc in BLAST_MainSetUp
 *
 * Revision 1.56  2004/06/08 15:20:44  dondosha
 * Use BlastHSPStream interface
 *
 * Revision 1.55  2004/06/07 21:34:55  dondosha
 * Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
 *
 * Revision 1.54  2004/06/07 18:26:29  dondosha
 * Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
 *
 * Revision 1.53  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.52  2004/05/14 17:16:12  dondosha
 * BlastReturnStat structure changed to BlastDiagnostics and refactored
 *
 * Revision 1.51  2004/05/07 15:28:05  papadopo
 * add scale factor of 1.0 to BlastMainSetup
 *
 * Revision 1.50  2004/05/05 15:28:56  dondosha
 * Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
 *
 * Revision 1.49  2004/04/30 16:53:06  dondosha
 * Changed a number of function names to have the same conventional Blast_ prefix
 *
 * Revision 1.48  2004/04/05 16:09:27  camacho
 * Rename DoubleInt -> SSeqRange
 *
 * Revision 1.47  2004/03/24 19:14:48  dondosha
 * Fixed memory leaks
 *
 * Revision 1.46  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 * Revision 1.45  2004/03/15 19:56:03  dondosha
 * Use sequence source instead of accessing subjects directly
 *
 * Revision 1.44  2004/03/10 17:37:36  papadopo
 * add (unused) RPS info pointer to LookupTableWrapInit()
 *
 * Revision 1.43  2004/02/24 18:16:29  dondosha
 * Removed lookup options argument from call to BLAST_MainSetUp
 *
 * Revision 1.42  2004/02/13 21:21:30  camacho
 * Add throws clause to Run method
 *
 * Revision 1.41  2004/02/13 19:32:55  camacho
 * Removed unnecessary #defines
 *
 * Revision 1.40  2004/01/16 21:52:59  bealer
 * - Changes for Blast4 API
 *
 * Revision 1.39  2004/01/02 18:53:27  camacho
 * Fix assertion
 *
 * Revision 1.38  2003/12/03 16:43:46  dondosha
 * Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
 *
 * Revision 1.37  2003/11/26 18:23:58  camacho
 * +Blast Option Handle classes
 *
 * Revision 1.36  2003/11/03 15:20:39  camacho
 * Make multiple query processing the default for Run().
 *
 * Revision 1.35  2003/10/31 00:05:15  camacho
 * Changes to return discontinuous seq-aligns for each query-subject pair
 *
 * Revision 1.34  2003/10/30 19:34:53  dondosha
 * Removed gapped_calculation from BlastHitSavingOptions structure
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
