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
 */

/** @file traceback_stage.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/traceback_stage.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/seqinfosrc_seqdb.hpp>  // for CSeqDbSeqInfoSrc
#include <objtools/blast/seqdb_reader/seqdb.hpp>     // for CSeqDb
#include <algo/blast/api/subj_ranges_set.hpp>

#include "blast_memento_priv.hpp"
#include "blast_seqalign.hpp"
#include "blast_aux_priv.hpp"
#include "psiblast_aux_priv.hpp"

// CORE BLAST includes
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_hits.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory>   qf,
                                             CRef<CBlastOptions>   opts,
                                             BlastSeqSrc         * seqsrc,
                                             CRef<IBlastSeqInfoSrc> seqinfosrc,
                                             CRef<TBlastHSPStream> hsps,
                                             CConstRef<objects::CPssmWithParameters> pssm)
    : m_QueryFactory (qf),
      m_Options      (opts),
      m_InternalData (new SInternalData),
      m_OptsMemento  (0),
      m_SeqInfoSrc   (seqinfosrc),
      m_ResultType(eDatabaseSearch),
      m_DBscanInfo(0)
{
    x_Init(qf, opts, pssm, BlastSeqSrcGetName(seqsrc), hsps);
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
    m_InternalData->m_FnInterrupt = NULL;
    m_InternalData->m_ProgressMonitor.Reset(new CSBlastProgress(NULL));
}

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory> qf,
                                             CRef<SInternalData> internal_data, 
                                             CRef<CBlastOptions>   opts,
                                             CRef<IBlastSeqInfoSrc> seqinfosrc,
                                             TSearchMessages& search_msgs)
    : m_QueryFactory (qf),
      m_Options      (opts),
      m_InternalData (internal_data),
      m_OptsMemento  (opts->CreateSnapshot()),
      m_Messages     (search_msgs),
      m_SeqInfoSrc   (seqinfosrc),
      m_ResultType(eDatabaseSearch),
      m_DBscanInfo(0)
{
      if (Blast_ProgramIsPhiBlast(opts->GetProgramType())) {
           if (m_InternalData)
           {
              BlastDiagnostics* diag = m_InternalData->m_Diagnostics->GetPointer();
              if (diag && diag->ungapped_stat)
              {
                 CRef<SDatabaseScanData> dbscan_info(new SDatabaseScanData);;
                 dbscan_info->m_NumPatOccurInDB = (int) diag->ungapped_stat->lookup_hits;
                 SetDBScanInfo(dbscan_info);
              }
           }
     }
}

CBlastTracebackSearch::~CBlastTracebackSearch()
{
    delete m_OptsMemento;
}

void
CBlastTracebackSearch::SetResultType(EResultType type)
{
    m_ResultType = type;
}

void
CBlastTracebackSearch::SetDBScanInfo(CRef<SDatabaseScanData> dbscan_info)
{
    m_DBscanInfo = dbscan_info;
}

void
CBlastTracebackSearch::x_Init(CRef<IQueryFactory>   qf,
                              CRef<CBlastOptions>   opts,
                              CConstRef<objects::CPssmWithParameters> pssm,
                              const string        & dbname,
                              CRef<TBlastHSPStream> hsps)
{
    opts->Validate();
    
    // 1. Initialize the query data (borrow it from the factory)
    CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));
    m_InternalData->m_Queries = query_data->GetSequenceBlk();
    m_InternalData->m_QueryInfo = query_data->GetQueryInfo();
    
    query_data->GetMessages(m_Messages);
    
    // 2. Take care of any rps information
    if (Blast_ProgramIsRpsBlast(opts->GetProgramType())) {
        m_InternalData->m_RpsData =
            CSetupFactory::CreateRpsStructures(dbname, opts);
    }

    // 3. Create the options memento
    m_OptsMemento = opts->CreateSnapshot();

    const bool kIsPhiBlast =
		Blast_ProgramIsPhiBlast(m_OptsMemento->m_ProgramType) ? true : false;

    // 4. Create the BlastScoreBlk
    // Note: we don't need masked query regions, as these are generally created
    // in the preliminary stage of the BLAST search
    BlastSeqLoc* lookup_segments(0);
    BlastScoreBlk* sbp =
        CSetupFactory::CreateScoreBlock(m_OptsMemento, query_data, 
                                        kIsPhiBlast ? &lookup_segments : 0, 
                                        m_Messages, 0, 
                                        m_InternalData->m_RpsData);
    m_InternalData->m_ScoreBlk.Reset
        (new TBlastScoreBlk(sbp, BlastScoreBlkFree));
    if (pssm.NotEmpty()) {
        PsiBlastSetupScoreBlock(sbp, pssm, m_Messages, opts);
    }

    // N.B.: Only PHI BLAST pseudo lookup table is necessary
    if (kIsPhiBlast) {
        _ASSERT(lookup_segments);
        _ASSERT(m_InternalData->m_RpsData == NULL);
        CRef< CBlastSeqLocWrap > lookup_segments_wrap( 
                new CBlastSeqLocWrap( lookup_segments ) );
        int num_pat_occurrences_in_db = 0;
        if (m_DBscanInfo && m_DBscanInfo->m_NumPatOccurInDB != m_DBscanInfo->kNoPhiBlastPattern)
           num_pat_occurrences_in_db = m_DBscanInfo->m_NumPatOccurInDB;
        LookupTableWrap* lut =
            CSetupFactory::CreateLookupTable(query_data, m_OptsMemento,
                 m_InternalData->m_ScoreBlk->GetPointer(), lookup_segments_wrap);
        m_InternalData->m_LookupTable.Reset
            (new TLookupTableWrap(lut, LookupTableWrapFree));
    }

    // 5. Create diagnostics
    BlastDiagnostics* diags = CSetupFactory::CreateDiagnosticsStructure();
    m_InternalData->m_Diagnostics.Reset
        (new TBlastDiagnostics(diags, Blast_DiagnosticsFree));

    // 6. Attach HSP stream
    m_InternalData->m_HspStream.Reset(hsps);
}

CRef<CSearchResultSet>
CBlastTracebackSearch::Run()
{
    _ASSERT(m_OptsMemento);
    SPHIPatternSearchBlk* phi_lookup_table(0);

    // For PHI BLAST we need to pass the pattern search items structure to the
    // traceback code
    bool is_phi = !! Blast_ProgramIsPhiBlast(m_OptsMemento->m_ProgramType);
    
    if (is_phi) {
        _ASSERT(m_InternalData->m_LookupTable);
        _ASSERT(m_DBscanInfo && m_DBscanInfo->m_NumPatOccurInDB !=
                m_DBscanInfo->kNoPhiBlastPattern);
        phi_lookup_table = (SPHIPatternSearchBlk*) 
            m_InternalData->m_LookupTable->GetPointer()->lut;
        phi_lookup_table->num_patterns_db = m_DBscanInfo->m_NumPatOccurInDB;
    }
    else
    {
        m_InternalData->m_LookupTable.Reset(NULL);
    }

    // When dealing with PSI-BLAST iterations, we need to keep a larger
    // alignment for the PSSM engine as to replicate blastpgp's behavior
    int hitlist_size_backup = m_OptsMemento->m_HitSaveOpts->hitlist_size;
    if (m_OptsMemento->m_ProgramType == eBlastTypePsiBlast ) {
        SBlastHitsParameters* bhp = NULL;
        SBlastHitsParametersNew(m_OptsMemento->m_HitSaveOpts, 
                                m_OptsMemento->m_ExtnOpts,
                                m_OptsMemento->m_ScoringOpts,
                                &bhp);
        m_OptsMemento->m_HitSaveOpts->hitlist_size = bhp->prelim_hitlist_size;
        SBlastHitsParametersFree(bhp);
    }
    
    BlastHSPResults * hsp_results(0);
    int status =
        Blast_RunTracebackSearchWithInterrupt(m_OptsMemento->m_ProgramType,
                                 m_InternalData->m_Queries,
                                 m_InternalData->m_QueryInfo,
                                 m_InternalData->m_SeqSrc->GetPointer(),
                                 m_OptsMemento->m_ScoringOpts,
                                 m_OptsMemento->m_ExtnOpts,
                                 m_OptsMemento->m_HitSaveOpts,
                                 m_OptsMemento->m_EffLenOpts,
                                 m_OptsMemento->m_DbOpts,
                                 m_OptsMemento->m_PSIBlastOpts,
                                 m_InternalData->m_ScoreBlk->GetPointer(),
                                 m_InternalData->m_HspStream->GetPointer(),
                                 m_InternalData->m_RpsData ?
                                 (*m_InternalData->m_RpsData)() : 0,
                                 phi_lookup_table,
                                 & hsp_results,
                                 m_InternalData->m_FnInterrupt,
                                 m_InternalData->m_ProgressMonitor->Get());
    if (status) {
        NCBI_THROW(CBlastException, eCoreBlastError, "Traceback failed"); 
    }
    
    // This is the data resulting from the traceback phase (before it is converted to ASN.1).
    // We wrap it this way so it is released even if an exception is thrown below.
    CRef< CStructWrapper<BlastHSPResults> > HspResults;
    HspResults.Reset(WrapStruct(hsp_results, Blast_HSPResultsFree));
    
    _ASSERT(m_SeqInfoSrc);
    _ASSERT(m_QueryFactory);
    m_OptsMemento->m_HitSaveOpts->hitlist_size = hitlist_size_backup;
    
    CRef<ILocalQueryData> qdata = m_QueryFactory->MakeLocalQueryData(m_Options);
    
    m_SeqInfoSrc->GarbageCollect();
    vector<TSeqLocInfoVector> subj_masks;
    TSeqAlignVector aligns =
        LocalBlastResults2SeqAlign(hsp_results,
                                   *qdata,
                                   *m_SeqInfoSrc,
                                   m_OptsMemento->m_ProgramType,
                                   m_Options->GetGappedMode(),
                                   m_Options->GetOutOfFrameMode(),
                                   subj_masks,
                                   m_ResultType);

    vector< CConstRef<CSeq_id> > query_ids;
    query_ids.reserve(aligns.size());
    for (size_t i = 0; i < qdata->GetNumQueries(); i++) {
        query_ids.push_back(CConstRef<CSeq_id>(qdata->GetSeq_loc(i)->GetId()));
    }
    
    return BlastBuildSearchResultSet(query_ids,
                                     m_InternalData->m_ScoreBlk->GetPointer(),
                                     m_InternalData->m_QueryInfo,
                                     m_OptsMemento->m_ProgramType, 
                                     aligns, 
                                     m_Messages,
                                     subj_masks,
                                     NULL,
                                     m_ResultType);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

