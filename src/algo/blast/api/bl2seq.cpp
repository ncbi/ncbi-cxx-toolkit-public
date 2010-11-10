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
#include <algo/blast/api/repeats_filter.hpp>
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
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>
#include "blast_aux_priv.hpp"


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

    x_Init(queries, subjects);
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

    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 EProgram p)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false)
{
    TSeqLocVector queries;
    queries.push_back(query);

    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 EProgram p)
    : mi_bQuerySetUpDone(false)
{
    x_Init(queries, subjects);
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
}

CBl2Seq::CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
                 CBlastOptionsHandle& opts)
    : mi_bQuerySetUpDone(false)
{
    x_Init(queries, subjects);
    m_OptsHandle.Reset(&opts);
}

void CBl2Seq::x_Init(const TSeqLocVector& queries, const TSeqLocVector& subjs)
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
    m_fnpInterrupt = NULL;
    m_ProgressMonitor = NULL;
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
    m_AncillaryData.clear();
    m_SubjectMasks.clear();
}

TSeqAlignVector
CBl2Seq::Run()
{
    //m_OptsHandle->GetOptions().DebugDumpText(cerr, "m_OptsHandle", 1);
    m_OptsHandle->GetOptions().Validate();  // throws an exception on failure
    SetupSearch();
    RunFullSearch();
    TSeqAlignVector retval = x_Results2SeqAlign();
    x_BuildAncillaryData(retval);
    return retval;
}

void
CBl2Seq::x_BuildAncillaryData(const TSeqAlignVector& alignments)
{
    vector< CConstRef<CSeq_id> > query_ids;
    x_SimplifyTSeqLocVector(m_tQueries, query_ids);
    BuildBlastAncillaryData(m_OptsHandle->GetOptions().GetProgramType(),
                            query_ids, mi_pScoreBlock, mi_clsQueryInfo, 
                            alignments, 
                            ncbi::blast::eSequenceComparison,
                            m_AncillaryData);
}

void
CBl2Seq::x_SimplifyTSeqLocVector(const TSeqLocVector& slv,
                             vector< CConstRef<objects::CSeq_id> >& query_ids)
{
    query_ids.clear();
    for (size_t i = 0; i < slv.size(); i++) {
        query_ids.push_back(CConstRef<CSeq_id>(slv[i].seqloc->GetId()));
    }
}

CRef<CSearchResultSet>
CBl2Seq::RunEx()
{
    TSeqAlignVector alignments = Run();
    TSeqLocInfoVector query_masks = GetFilteredQueryRegions();

    vector< CConstRef<CSeq_id> > query_ids;
    x_SimplifyTSeqLocVector(m_tQueries, query_ids);

    CRef<CSearchResultSet> retval = 
        BlastBuildSearchResultSet(query_ids, mi_pScoreBlock,
                                  mi_clsQueryInfo,
                                  m_OptsHandle->GetOptions().GetProgramType(),
                                  alignments, m_Messages, m_SubjectMasks,
                                  &query_masks,
                                  ncbi::blast::eSequenceComparison);

    m_AncillaryData.reserve(retval->size());
    ITERATE(CSearchResultSet, result, *retval) {
        m_AncillaryData.push_back((*result)->GetAncillaryData());
    }

    return retval;
}

void
CBl2Seq::RunWithoutSeqalignGeneration()
{
    //m_OptsHandle->GetOptions().DebugDumpText(cerr, "m_OptsHandle", 1);
    m_OptsHandle->GetOptions().Validate();  // throws an exception on failure
    SetupSearch();
    RunFullSearch();
}

void 
CBl2Seq::SetupSearch()
{
    if ( !mi_bQuerySetUpDone ) {
        x_ResetQueryDs();
        const CBlastOptions& kOptions = m_OptsHandle->GetOptions();
        EBlastProgramType prog = kOptions.GetProgramType();
        ENa_strand strand_opt = kOptions.GetStrandOption();

        if (CBlastNucleotideOptionsHandle *nucl_handle =
            dynamic_cast<CBlastNucleotideOptionsHandle*>(&*m_OptsHandle)) {
            Blast_FindDustFilterLoc(m_tQueries, nucl_handle);
            Blast_FindRepeatFilterLoc(m_tQueries, nucl_handle);
        }
        
        SetupQueryInfo(m_tQueries, prog, strand_opt, &mi_clsQueryInfo);
        m_Messages.resize(mi_clsQueryInfo->num_queries);
        SetupQueries(m_tQueries, mi_clsQueryInfo, &mi_clsQueries, 
                     prog, strand_opt, m_Messages);

        Blast_Message* blmsg = NULL;
        double scale_factor = 1.0;
        short st;
       
        st = BLAST_MainSetUp(m_OptsHandle->GetOptions().GetProgramType(), 
                             m_OptsHandle->GetOptions().GetQueryOpts(),
                             m_OptsHandle->GetOptions().GetScoringOpts(),
                             mi_clsQueries, mi_clsQueryInfo, scale_factor,
                             &mi_pLookupSegments, &m_ipFilteredRegions, &mi_pScoreBlock,
                             &blmsg, &BlastFindMatrixPath);

        // TODO: Check that lookup_segments are not filtering the whole 
        // sequence (SSeqRange set to -1 -1)
        const string error_msg1(blmsg
                               ? blmsg->message
                               : "BLAST_MainSetUp failed");
        Blast_Message2TSearchMessages(blmsg, mi_clsQueryInfo, m_Messages);
        blmsg = Blast_MessageFree(blmsg);
        if (st != 0) {
            NCBI_THROW(CBlastException, eCoreBlastError, error_msg1);
        }

        st = LookupTableWrapInit(mi_clsQueries, 
                            m_OptsHandle->GetOptions().GetLutOpts(),
                            m_OptsHandle->GetOptions().GetQueryOpts(),
                            mi_pLookupSegments, mi_pScoreBlock, 
                            &mi_pLookupTable, NULL, &blmsg);
        const string error_msg2(blmsg 
                      ? blmsg->message
                       : "LookupTableWrapInit failed");
        Blast_Message2TSearchMessages(blmsg, mi_clsQueryInfo, m_Messages);
        blmsg = Blast_MessageFree(blmsg);
        if (st != 0) {
            NCBI_THROW(CBlastException, eCoreBlastError, error_msg2);
        }

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
CBl2Seq::RunFullSearch()
{
    mi_pResults = NULL;
    mi_pDiagnostics = Blast_DiagnosticsInit();

    const CBlastOptions& kOptions = GetOptionsHandle().GetOptions();
    auto_ptr<const CBlastOptionsMemento> opts_mem(kOptions.CreateSnapshot());
    
    BlastHSPStream* hsp_stream = 
        CSetupFactory::CreateHspStream(opts_mem.get(),
                                       mi_clsQueryInfo->num_queries,
        CSetupFactory::CreateHspWriter(opts_mem.get(), mi_clsQueryInfo));

    BlastHSPStreamRegisterPipe(hsp_stream,
        CSetupFactory::CreateHspPipe(opts_mem.get(), mi_clsQueryInfo),
                               eTracebackSearch);

    TBlastHSPStream thsp_stream(hsp_stream, BlastHSPStreamFree);
    
    SBlastProgressReset(m_ProgressMonitor);
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
                                      thsp_stream.GetPointer(), NULL, mi_pDiagnostics, 
                                      &mi_pResults, m_fnpInterrupt,
                                      m_ProgressMonitor);
    if (status) {
        NCBI_THROW(CBlastException, eCoreBlastError,
                   BlastErrorCode2String(status));
    }
}

/* Unlike the database search, we want to make sure that a seqalign list is   
 * returned for each query/subject pair, even if it is empty. Also we don't 
 * want subjects to be sorted in seqalign results.
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
                                      m_SubjectMasks, eSequenceComparison);
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
