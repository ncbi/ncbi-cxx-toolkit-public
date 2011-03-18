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

/** @file prelim_stage.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>

#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/blast_mtlock.hpp>
#include <algo/blast/core/gencode_singleton.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_stat.h>

#include "prelim_search_runner.hpp"
#include "blast_aux_priv.hpp"
#include "psiblast_aux_priv.hpp"
#include "split_query_aux_priv.hpp"
#include <sstream>

#include <algo/blast/api/blast_dbindex.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       CRef<CBlastOptions> options,
                                       const CSearchDatabase& dbinfo)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData),
    m_Options(options)
{
    BlastSeqSrc* seqsrc = CSetupFactory::CreateBlastSeqSrc(dbinfo);
    x_Init(query_factory, options, CRef<CPssmWithParameters>(), seqsrc);

    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       CRef<CBlastOptions> options,
                                       CRef<CLocalDbAdapter> db)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData),
    m_Options(options)
{
    BlastSeqSrc* seqsrc = db->MakeSeqSrc();
    x_Init(query_factory, options, CRef<CPssmWithParameters>(), seqsrc);
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                               CRef<CBlastOptions> options,
                               BlastSeqSrc* seqsrc,
                               CConstRef<objects::CPssmWithParameters> pssm)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData),
    m_Options(options)
{
    x_Init(query_factory, options, pssm, seqsrc);
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
}

void
CBlastPrelimSearch::SetNumberOfThreads(size_t nthreads)
{
    const bool was_multithreaded = IsMultiThreaded();

    CThreadable::SetNumberOfThreads(nthreads);
    if (was_multithreaded != IsMultiThreaded()) {
        BlastDiagnostics* diags = IsMultiThreaded()
            ? CSetupFactory::CreateDiagnosticsStructureMT()
            : CSetupFactory::CreateDiagnosticsStructure();
        m_InternalData->m_Diagnostics.Reset
            (new TBlastDiagnostics(diags, Blast_DiagnosticsFree));

        CRef<ILocalQueryData> query_data
            (m_QueryFactory->MakeLocalQueryData(&*m_Options));
        auto_ptr<const CBlastOptionsMemento> opts_memento
            (m_Options->CreateSnapshot());
        if (IsMultiThreaded())
            BlastHSPStreamRegisterMTLock(m_InternalData->m_HspStream->GetPointer(),
                                         Blast_CMT_LOCKInit());
    }
}

void
CBlastPrelimSearch::x_Init(CRef<IQueryFactory> query_factory,
                           CRef<CBlastOptions> options,
                           CConstRef<objects::CPssmWithParameters> pssm,
                           BlastSeqSrc* seqsrc )
{
    CRef<SBlastSetupData> setup_data =
        BlastSetupPreliminarySearchEx(query_factory, options, pssm, seqsrc,
                                      IsMultiThreaded());
    m_InternalData = setup_data->m_InternalData;
    copy(setup_data->m_Masks.begin(), setup_data->m_Masks.end(),
         back_inserter(m_MasksForAllQueries));
    m_Messages = setup_data->m_Messages;
}

int
CBlastPrelimSearch::x_LaunchMultiThreadedSearch(SInternalData& internal_data)
{
    typedef vector< CRef<CPrelimSearchThread> > TBlastThreads;
    TBlastThreads the_threads(GetNumberOfThreads());

    auto_ptr<const CBlastOptionsMemento> opts_memento
        (m_Options->CreateSnapshot());
    _TRACE("Launching BLAST with " << GetNumberOfThreads() << " threads");

    // -RMH- This appears to be a problem right now.  When used...this
    // can cause all the work to go to a single thread!  (-MN- This is fixed in SB-768)
    BlastSeqSrcSetNumberOfThreads(m_InternalData->m_SeqSrc->GetPointer(), 
                                  GetNumberOfThreads());

    // Create the threads ...
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        thread->Reset(new CPrelimSearchThread(internal_data,
                                              opts_memento.get()));
        if (thread->Empty()) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory,
                       "Failed to create preliminary search thread");
        }
    }

    // ... launch the threads ...
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        (*thread)->Run();
    }

    // ... and wait for the threads to finish
    bool error_occurred(false);
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        long result(0);
        (*thread)->Join(reinterpret_cast<void**>(&result));
        if (result != 0) {
            error_occurred = true;
        }
    }

    BlastSeqSrcSetNumberOfThreads(m_InternalData->m_SeqSrc->GetPointer(), 0);

    if (error_occurred) {
        NCBI_THROW(CBlastException, eCoreBlastError, 
                   "Preliminary search thread failure!");
    }
    return 0;
}

CRef<SInternalData>
CBlastPrelimSearch::Run()
{
    if (! BlastSeqSrcGetNumSeqs(m_InternalData->m_SeqSrc->GetPointer())) {
        string msg =
            "GI or TI list filtering resulted in an empty database.";
        
        m_Messages.AddMessageAllQueries(eBlastSevWarning,
                                        kBlastMessageNoContext, 
                                        msg);
    }
    
    BlastSeqSrcResetChunkIterator(m_InternalData->m_SeqSrc->GetPointer());

    CEffectiveSearchSpacesMemento eff_memento(m_Options);
    SplitQuery_SetEffectiveSearchSpace(m_Options, m_QueryFactory,
                                       m_InternalData);
    int retval = 0;

    auto_ptr<const CBlastOptionsMemento> opts_memento
        (m_Options->CreateSnapshot());
    BlastSeqSrc * seqsrc = m_InternalData->m_SeqSrc->GetPointer();
    BLAST_SequenceBlk* queries = m_InternalData->m_Queries;
    LookupTableOptions * lut_options = opts_memento->m_LutOpts;
    BlastInitialWordOptions * word_options = opts_memento->m_InitWordOpts;

    GetDbIndexSetNumThreadsFn()( seqsrc, GetNumberOfThreads() );

    int db_genetic_code = 0;
    if ( (db_genetic_code = m_Options->GetDbGeneticCode()) > 0) {
        CFastMutex mutex;
        if (GenCodeSingletonFind(db_genetic_code) == NULL) {
            TAutoUint1ArrayPtr gc = FindGeneticCode(db_genetic_code);
            GenCodeSingletonAdd(db_genetic_code, gc.get());
        }
    }

    // Query splitting data structure (used only if applicable)
    CRef<SBlastSetupData> setup_data(new SBlastSetupData(m_QueryFactory, m_Options));
    CRef<CQuerySplitter> query_splitter = setup_data->m_QuerySplitter;
    if (query_splitter->IsQuerySplit()) {

        CRef<CSplitQueryBlk> split_query_blk = query_splitter->Split();

        for (Uint4 i = 0; i < query_splitter->GetNumberOfChunks(); i++) {
            try {
                CRef<IQueryFactory> chunk_qf = 
                    query_splitter->GetQueryFactoryForChunk(i);
                _TRACE("Query chunk " << i << "/" << 
                       query_splitter->GetNumberOfChunks());
                CRef<SInternalData> chunk_data =
                    SplitQuery_CreateChunkData(chunk_qf, m_Options,
                                               m_InternalData,
                                               IsMultiThreaded());

                CRef<ILocalQueryData> query_data(
                        chunk_qf->MakeLocalQueryData( &*m_Options ) );
                BLAST_SequenceBlk * chunk_queries = 
                    query_data->GetSequenceBlk();
                GetDbIndexRunSearchFn()( 
                        seqsrc, chunk_queries, lut_options, word_options );

                if (IsMultiThreaded()) {
                     x_LaunchMultiThreadedSearch(*chunk_data);
                } else {
                    retval = 
                        CPrelimSearchRunner(*chunk_data, opts_memento.get())();
                    if (retval) {
                        NCBI_THROW(CBlastException, eCoreBlastError,
                                   BlastErrorCode2String(retval));
                    }
                }


                _ASSERT(chunk_data->m_HspStream->GetPointer());
                BlastHSPStreamMerge(split_query_blk->GetCStruct(), i,
                                chunk_data->m_HspStream->GetPointer(),
                                m_InternalData->m_HspStream->GetPointer());
                _ASSERT(m_InternalData->m_HspStream->GetPointer());
                // free this as the query_splitter keeps a reference to the
                // chunk factories, which in turn keep a reference to the local
                // query data.
                query_data->FlushSequenceData();        
            } catch (const CBlastException& e) {
                // This error message is safe to ignore for a given chunk,
                // because the chunks might end up producing a region of
                // the query for which ungapped Karlin-Altschul blocks
                // cannot be calculated
                const string err_msg1("search cannot proceed due to errors "
                                     "in all contexts/frames of query "
                                     "sequences");
                const string err_msg2("Warning: Could not calculate ungapped Karlin-Altschul "
                                      "parameters due to an invalid query sequence or its translation. "
                                      "Please verify the query sequence(s) and/or filtering options");
                if (e.GetMsg().find(err_msg1) == NPOS && e.GetMsg().find(err_msg2) == NPOS) {
                    throw;
                }
            }
        }

        // Restore the full query sequence for the traceback stage!
        if (m_InternalData->m_Queries == NULL) {
            CRef<ILocalQueryData> query_data
                (m_QueryFactory->MakeLocalQueryData(&*m_Options));
            // Query masking info is calculated as a side-effect
            CBlastScoreBlk sbp
                (CSetupFactory::CreateScoreBlock(opts_memento.get(), query_data,
                                        NULL, m_Messages, NULL, NULL));
            m_InternalData->m_Queries = query_data->GetSequenceBlk();
        }
    } else {

        GetDbIndexRunSearchFn()( 
                seqsrc, queries, lut_options, word_options );

        if (IsMultiThreaded()) {
             x_LaunchMultiThreadedSearch(*m_InternalData);
        } else {
            retval = CPrelimSearchRunner(*m_InternalData, opts_memento.get())();
            if (retval) {
                NCBI_THROW(CBlastException, eCoreBlastError,
                           BlastErrorCode2String(retval));
            }
        }
    }

    return m_InternalData;
}

int
CBlastPrelimSearch::CheckInternalData()
{
    int retval = 0;
    retval = BlastScoreBlkCheck(m_InternalData->m_ScoreBlk->GetPointer()); 
    return retval;
}

BlastHSPResults*
CBlastPrelimSearch::ComputeBlastHSPResults(BlastHSPStream* stream,
                                           Uint4 max_num_hsps,
                                           bool* rm_hsps) const
{
    auto_ptr<const CBlastOptionsMemento> opts_memento
        (m_Options->CreateSnapshot());

    _ASSERT(m_InternalData->m_QueryInfo->num_queries > 0);
    Boolean removed_hsps = FALSE;
    SBlastHitsParameters* hit_param = NULL;
    SBlastHitsParametersNew(opts_memento->m_HitSaveOpts,
                            opts_memento->m_ExtnOpts,
                            opts_memento->m_ScoringOpts,
                            &hit_param);
    BlastHSPResults* retval =
        Blast_HSPResultsFromHSPStreamWithLimit(stream,
           (Uint4) m_InternalData->m_QueryInfo->num_queries,
           hit_param,
           max_num_hsps,
           &removed_hsps);
    if (rm_hsps) {
        *rm_hsps = removed_hsps == FALSE ? false : true;
    }
    // applications assume the HSPLists in the HSPResults are
    // sorted in order of worsening best e-value
    Blast_HSPResultsSortByEvalue(retval);
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

