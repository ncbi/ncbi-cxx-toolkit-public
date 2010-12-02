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
 */

/** @file prelim_search_runner.hpp
 * Defines internal auxiliary functor object to run the preliminary stage of
 * the BLAST search.
 */

#ifndef ALGO_BLAST_API___PRELIM_SEARCH_RUNNER__HPP
#define ALGO_BLAST_API___PRELIM_SEARCH_RUNNER__HPP

/** @addtogroup AlgoBlast
 *
 * @{
 */

#include <corelib/ncbithr.hpp>                  // for CThread
#include <algo/blast/api/setup_factory.hpp>
#include "blast_memento_priv.hpp"

// CORE BLAST includes
#include <algo/blast/core/blast_engine.h>

BEGIN_NCBI_SCOPE
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
        _ASSERT(m_OptsMemento);
        _ASSERT(m_InternalData.m_Queries);
        _ASSERT(m_InternalData.m_QueryInfo);
        _ASSERT(m_InternalData.m_SeqSrc);
        _ASSERT(m_InternalData.m_ScoreBlk);
        _ASSERT(m_InternalData.m_LookupTable);
        _ASSERT(m_InternalData.m_HspStream);
        SBlastProgressReset(m_InternalData.m_ProgressMonitor->Get());
        Int2 retval = Blast_RunPreliminarySearchWithInterrupt(m_OptsMemento->m_ProgramType,
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
                                 m_InternalData.m_Diagnostics->GetPointer(),
                                 m_InternalData.m_FnInterrupt,
                                 m_InternalData.m_ProgressMonitor->Get());
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
                        const CBlastOptionsMemento* opts_memento)
        : m_InternalData(internal_data), m_OptsMemento(opts_memento)
    {
        // The following fields need to be copied to ensure MT-safety
        BlastSeqSrc* seqsrc = 
            BlastSeqSrcCopy(m_InternalData.m_SeqSrc->GetPointer());
        m_InternalData.m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 
                                                       BlastSeqSrcFree));
        // The progress field must be copied to ensure MT-safety
        if (m_InternalData.m_ProgressMonitor->Get()) {
            SBlastProgress* bp = 
                SBlastProgressNew(m_InternalData.m_ProgressMonitor->Get()->user_data);
            m_InternalData.m_ProgressMonitor.Reset(new CSBlastProgress(bp));
        }
    }

protected:
    virtual ~CPrelimSearchThread(void) {}

    virtual void* Main(void) {
        return (void*) 
            ((intptr_t) CPrelimSearchRunner(m_InternalData, m_OptsMemento)());
    }

private:
    SInternalData m_InternalData;
    const CBlastOptionsMemento* m_OptsMemento;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___PRELIM_SEARCH_RUNNER__HPP */
