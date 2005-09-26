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

/** @file internal_interfaces.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>                  // for CThread

#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase

#include "blast_memento_priv.hpp"
#include "blast_seqsrc_adapter_priv.hpp"

// CORE BLAST includes
#include <algo/blast/core/blast_engine.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Functor to run the preliminary stage of the BLAST search
class CPrelimSearchRunner : public CObject
{
public:
    CPrelimSearchRunner(SInternalData& internal_data,
                        const CBlastOptionsMemento* opts_memento)
        : m_InternalData(internal_data), m_OptsMemento(opts_memento)
    {}
    ~CPrelimSearchRunner() {}
    int operator()() {
        ASSERT(m_OptsMemento);
        ASSERT(m_InternalData.m_Queries);
        ASSERT(m_InternalData.m_QueryInfo);
        ASSERT(m_InternalData.m_SeqSrc);
        ASSERT(m_InternalData.m_ScoreBlk);
        ASSERT(m_InternalData.m_LookupTable);
        ASSERT(m_InternalData.m_HspStream);
        Int2 retval = Blast_RunPreliminarySearch(m_OptsMemento->m_ProgramType,
                                 m_InternalData.m_Queries,
                                 m_InternalData.m_QueryInfo,
                                 m_InternalData.m_SeqSrc->GetPointer(),
                                 m_OptsMemento->m_ScoringOpts,
                                 m_InternalData.m_ScoreBlk->GetPointer(),
                                 m_InternalData.m_LookupTable->GetPointer(),
                                 m_OptsMemento->m_InitWordOpts,
                                 m_OptsMemento->m_ExtnOpts,
                                 m_OptsMemento->m_HitSaveOpts,
                                 m_OptsMemento->m_EffLenOpts,
                                 m_OptsMemento->m_PSIBlastOpts,
                                 m_OptsMemento->m_DbOpts,
                                 m_InternalData.m_HspStream->GetPointer(),
                                 m_InternalData.m_Diagnostics->GetPointer());
        return static_cast<int>(retval);
    }

private:
    /// Data structure containing all the needed C structures for the
    /// preliminary stage of the BLAST search
    SInternalData& m_InternalData;

    /// Pointer to memento which this class doesn't own
    const CBlastOptionsMemento* m_OptsMemento;


    /// Prohibit copy constructor
    CPrelimSearchRunner(const CPrelimSearchRunner& rhs);
    /// Prohibit assignment operator
    CPrelimSearchRunner& operator=(const CPrelimSearchRunner& rhs);
};

/// Thread class to run the preliminary stage of the BLAST search
class CPrelimSearchThread : public CThread
{
public:
    CPrelimSearchThread(SInternalData& internal_data,
                        const CBlastOptionsMemento* opts_memento);
protected:
    virtual void* Main(void);
    ~CPrelimSearchThread(void);
private:
    SInternalData& m_InternalData;
    const CBlastOptionsMemento* m_OptsMemento;
};

CPrelimSearchThread::CPrelimSearchThread(SInternalData& internal_data,
                                         const CBlastOptionsMemento*
                                         opts_memento)
    : m_InternalData(internal_data), m_OptsMemento(opts_memento)
{
    // The following fields need to be copied to ensure MT-safety
    BlastSeqSrc* seqsrc = 
        BlastSeqSrcCopy(m_InternalData.m_SeqSrc->GetPointer());
    m_InternalData.m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));
}

CPrelimSearchThread::~CPrelimSearchThread()
{
}

void*
CPrelimSearchThread::Main(void)
{
    return (void*) CPrelimSearchRunner(m_InternalData, m_OptsMemento)();
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       CRef<CBlastOptions> options,
                                       const CSearchDatabase& dbinfo)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData)
{
    x_Init(query_factory, options, dbinfo.GetDatabaseName());

    BlastSeqSrc* seqsrc = CSetupFactory::CreateBlastSeqSrc(dbinfo);
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       CRef<CBlastOptions> options,
                                       //CConstRef<CBlastOptions> options,
                                       IBlastSeqSrcAdapter& ssa)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData)
{
    BlastSeqSrc* seqsrc = ssa.GetBlastSeqSrc();
    x_Init(query_factory, options, BlastSeqSrcGetName(seqsrc));
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
}

CBlastPrelimSearch::CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                                       //CConstRef<CBlastOptions> options,
                                       CRef<CBlastOptions> options,
                                       BlastSeqSrc* seqsrc)
    : m_QueryFactory(query_factory), m_InternalData(new SInternalData)
{
    x_Init(query_factory, options, BlastSeqSrcGetName(seqsrc));
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
}

void
CBlastPrelimSearch::x_Init(CRef<IQueryFactory> query_factory,
                           CRef<CBlastOptions> options,
                           const string& dbname)
{
    options->Validate();

    // 1. Initialize the query data (borrow it from the factory)
    CRef<ILocalQueryData>
        query_data(query_factory->MakeLocalQueryData(&*options));
    m_InternalData->m_Queries = query_data->GetSequenceBlk();
    m_InternalData->m_QueryInfo = query_data->GetQueryInfo();

    // 2. Take care of any rps information
    if (Blast_ProgramIsRpsBlast(options->GetProgramType())) {
        m_InternalData->m_RpsData =
            CSetupFactory::CreateRpsStructures(dbname, options);
    }

    // 3. Create the options memento
    m_OptsMemento = options->CreateSnapshot();

    // 4. Create the BlastScoreBlk
    BlastSeqLoc* lookup_segments(0);
    BlastScoreBlk* sbp =
        CSetupFactory::CreateScoreBlock(m_OptsMemento, query_data,
                                        &lookup_segments, 
                                        m_InternalData->m_RpsData);
    m_InternalData->m_ScoreBlk.Reset(new TBlastScoreBlk(sbp,
                                                       BlastScoreBlkFree));

    // 5. Create lookup table
    LookupTableWrap* lut =
        CSetupFactory::CreateLookupTable(query_data, m_OptsMemento,
                                     m_InternalData->m_ScoreBlk->GetPointer(),
                                     lookup_segments, 
                                     m_InternalData->m_RpsData);
    m_InternalData->m_LookupTable.Reset
        (new TLookupTableWrap(lut, LookupTableWrapFree));
    lookup_segments = BlastSeqLocFree(lookup_segments);
    ASSERT(lookup_segments == NULL);

    // 6. Create diagnostics
    BlastDiagnostics* diags = IsMultiThreaded()
        ? CSetupFactory::CreateDiagnosticsStructureMT()
        : CSetupFactory::CreateDiagnosticsStructure();
    m_InternalData->m_Diagnostics.Reset
        (new TBlastDiagnostics(diags, Blast_DiagnosticsFree));

    // 7. Create HSP stream
    BlastHSPStream* hsp_stream = IsMultiThreaded()
        ? CSetupFactory::CreateHspStreamMT(m_OptsMemento, 
                                           query_data->GetNumQueries())
        : CSetupFactory::CreateHspStream(m_OptsMemento, 
                                         query_data->GetNumQueries());
    m_InternalData->m_HspStream.Reset
        (new TBlastHSPStream(hsp_stream, BlastHSPStreamFree));
}

CBlastPrelimSearch::~CBlastPrelimSearch()
{
    delete m_OptsMemento;
}

int
CBlastPrelimSearch::x_LaunchMultiThreadedSearch()
{
    typedef vector< CRef<CPrelimSearchThread> > TBlastThreads;
    TBlastThreads the_threads(GetNumberOfThreads());

    // Create the threads ...
    NON_CONST_ITERATE(TBlastThreads, thread, the_threads) {
        thread->Reset(new CPrelimSearchThread(*m_InternalData, 
                                              m_OptsMemento));
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
        int result(0);
        (*thread)->Join(reinterpret_cast<void**>(&result));
        if (result != 0) {
            error_occurred = true;
        }
    }

    if (error_occurred) {
        NCBI_THROW(CBlastException, eCoreBlastError, 
                   "Preliminary search thread failure!");
    }
    return 0;
}

CRef<SInternalData>
CBlastPrelimSearch::Run()
{
    int retval =
        (IsMultiThreaded()
         ? x_LaunchMultiThreadedSearch()
         : CPrelimSearchRunner(*m_InternalData, m_OptsMemento)());
    
    ASSERT(retval == 0);
    retval += 0;        // dummy statement to avoid compiler warnings
    
    return m_InternalData;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

