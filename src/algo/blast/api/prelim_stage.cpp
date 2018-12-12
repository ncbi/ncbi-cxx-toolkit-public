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
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_stat.h>

#include "prelim_search_runner.hpp"
#include "blast_aux_priv.hpp"
#include "psiblast_aux_priv.hpp"
#include "split_query_aux_priv.hpp"
#include "blast_seqalign.hpp"
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
    m_Options(options), m_DbAdapter(NULL), m_DbInfo(&dbinfo)
{
    BlastSeqSrc* seqsrc = CSetupFactory::CreateBlastSeqSrc(dbinfo);
    x_Init(query_factory, options, CRef<CPssmWithParameters>(), seqsrc);

    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       CRef<CBlastOptions> options,
                                       CRef<CLocalDbAdapter> db,
                                       size_t num_threads)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData),
    m_Options(options), m_DbAdapter(db), m_DbInfo(NULL)
{
    BlastSeqSrc* seqsrc = db->MakeSeqSrc();
    x_Init(query_factory, options, CRef<CPssmWithParameters>(), seqsrc,
           num_threads);
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
    if (num_threads > 1) {
        SetNumberOfThreads(num_threads);
    }
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                               CRef<CBlastOptions> options,
                               BlastSeqSrc* seqsrc,
                               CConstRef<objects::CPssmWithParameters> pssm)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData),
    m_Options(options),  m_DbAdapter(NULL), m_DbInfo(NULL)
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
                           BlastSeqSrc* seqsrc,
                           size_t num_threads)
{
    CRef<SBlastSetupData> setup_data =
        BlastSetupPreliminarySearchEx(query_factory, options, pssm, seqsrc,
                                      num_threads);
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

    // Inform indexing library about the number of concurrent
    // search threads.
    //
    GetDbIndexSetNumThreadsFn()( GetNumberOfThreads() );

    // ... launch the threads ...
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        (*thread)->Run();
    }

    // ... and wait for the threads to finish
    Uint8 retv(0);
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        void * result(0);
        (*thread)->Join(&result);
        if (result) {
        	// Thread is not really returning a pointer, it's actually
        	//  retruning an int
            retv = reinterpret_cast<Uint8> (result);
        }
    }

    BlastSeqSrcSetNumberOfThreads(m_InternalData->m_SeqSrc->GetPointer(), 0);

    if (retv) {
          NCBI_THROW(CBlastException, eCoreBlastError,
                                   BlastErrorCode2String((Int2)retv));
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
    BLAST_SequenceBlk* queries = m_InternalData->m_Queries;
    LookupTableOptions * lut_options = opts_memento->m_LutOpts;
    BlastInitialWordOptions * word_options = opts_memento->m_InitWordOpts;

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
                                               GetNumberOfThreads());

                CRef<ILocalQueryData> query_data(
                        chunk_qf->MakeLocalQueryData( &*m_Options ) );
                BLAST_SequenceBlk * chunk_queries =
                    query_data->GetSequenceBlk();
                GetDbIndexSetUsingThreadsFn()( IsMultiThreaded() );
                GetDbIndexRunSearchFn()(
                        chunk_queries, lut_options, word_options );

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
                const string err_msg2(kBlastErrMsg_CantCalculateUngappedKAParams);
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

        GetDbIndexSetUsingThreadsFn()( IsMultiThreaded() );
        GetDbIndexRunSearchFn()( queries, lut_options, word_options );

        if (IsMultiThreaded()) {
             x_LaunchMultiThreadedSearch(*m_InternalData);
        } else {
            retval = CPrelimSearchRunner(*m_InternalData, opts_memento.get())();
            if (retval) {
                NCBI_THROW(CBlastException, eCoreBlastError,
                           BlastErrorCode2String(retval));
            }
        }
        CRef<TBlastHSPStream> s(m_InternalData->m_HspStream);
        if (*((*s)->results->hitlist_array) == NULL) {
            CNcbiDiag diag;
            diag << Warning << "No seeds produced for query" << Endm;
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
					   bool* rm_hsps,
					   vector<bool> *rm_hsps_info) const
{
    bool any_query_hsp_limited = false;
    auto_ptr<const CBlastOptionsMemento> opts_memento
        (m_Options->CreateSnapshot());

    _ASSERT(m_InternalData->m_QueryInfo->num_queries > 0);
    Boolean *removed_hsps = new Boolean [ m_InternalData->m_QueryInfo->num_queries ];
    SBlastHitsParameters* hit_param = NULL;
    SBlastHitsParametersNew(opts_memento->m_HitSaveOpts,
                            opts_memento->m_ExtnOpts,
                            opts_memento->m_ScoringOpts,
                            &hit_param);
    BlastHSPResults* retval =
        Blast_HSPResultsFromHSPStreamWithLimitEx(stream,
           (Uint4) m_InternalData->m_QueryInfo->num_queries,
           hit_param,
           max_num_hsps,
           removed_hsps);
    if( rm_hsps_info){
	rm_hsps_info->reserve(m_InternalData->m_QueryInfo->num_queries );
        for( int query_index = 0 ; query_index < m_InternalData->m_QueryInfo->num_queries ; query_index ++ ){
	  (*rm_hsps_info)[ query_index ] = removed_hsps[query_index] == FALSE ? false : true;
	  if( (*rm_hsps_info)[ query_index ] ) any_query_hsp_limited = true;
        }
    }
    delete [] removed_hsps;
    if( rm_hsps ) *rm_hsps = any_query_hsp_limited ;
    // applications assume the HSPLists in the HSPResults are
    // sorted in order of worsening best e-value
    Blast_HSPResultsSortByEvalue(retval);
    return retval;
}

bool CBlastPrelimSearch::Run( vector<list<CRef<CStd_seg> > >  & l )
{
	Run();
	return x_BuildStdSegList(l);
}

void s_FixNumIdent(BlastHSPList *hsp_list, bool gapped_calculation)
{
	   BlastHSP* hsp;
	   int i;

	   for (i=0; i < hsp_list->hspcnt; i++)
	   {
	      hsp = hsp_list->hsp_array[i];
	      if (gapped_calculation)
	    	  hsp->num_ident = -1;
	   }
}

void s_GetBitScores(BlastHitList * hit_list, bool gapped_calculation, const BlastScoreBlk * sbp)
{

	 for (int i = 0; i < hit_list->hsplist_count; i++)
	 {
	    	BlastHSPList* hsp_list = hit_list->hsplist_array[i];
		    if (!hsp_list)
		    	continue;

		    Blast_HSPListGetBitScores(hsp_list, gapped_calculation, sbp);
		    s_FixNumIdent(hsp_list, gapped_calculation);
	 }
}

// Results is trimmed by Blast Hits Save Options if set
bool CBlastPrelimSearch::x_BuildStdSegList( vector<list<CRef<CStd_seg> > >  & l )
{
	if(m_InternalData->m_HspStream.Empty())
	{
		_TRACE("HSP Stream is empty");
		return false;
	}

	if(NULL != m_DbInfo)
	{
		m_DbAdapter.Reset(new CLocalDbAdapter(*m_DbInfo));
	}

	if(m_DbAdapter.Empty())
	{
		_TRACE("This method does not support CBlastPrelimSearch constructed with BlastSeqSrc");
		return false;
	}

	BlastHSPStream * hsp_stream  = m_InternalData->m_HspStream->GetPointer();
	if(NULL == hsp_stream)
	{
		_TRACE("NULL HSP Stream Pointer");
		return false;
	}

	IBlastSeqInfoSrc * s_seqInfoSrc = m_DbAdapter->MakeSeqInfoSrc();
	EBlastProgramType program = hsp_stream->program;

	BlastHSPResults * results = ComputeBlastHSPResults(hsp_stream );

	if(NULL == results)
		return false;

	int num_queries = results->num_queries;

	BlastHitList ** q_list_ptr = results->hitlist_array;
	CRef<ILocalQueryData> local_query_data = m_QueryFactory->MakeLocalQueryData(m_Options.GetPointer());
	l.resize(num_queries);
	const BlastScoreBlk * sbp = m_InternalData->m_ScoreBlk->GetPointer();
	bool gapped_cal = m_Options->GetGappedMode();
	for(int i=0; i < num_queries; i++)
	{
		CConstRef<CSeq_loc> query_loc = local_query_data->GetSeq_loc(i);
		TSeqPos	query_length = local_query_data->GetSeqLength(i);
		BlastHitList * hit_list = q_list_ptr[i];
		if(NULL != hit_list)
		{

			s_GetBitScores(hit_list, gapped_cal, sbp);
			BLASTPrelminSearchHitListToStdSeg(program, hit_list, *query_loc, query_length, s_seqInfoSrc, l[i]);
		}
	}

	return true;
}
END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

