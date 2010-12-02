/* $Id$
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
 * Author: Christiam Camacho, Kevin Bealer
 *
 */

/** @file prelim_stage.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___PRELIM_STAGE_HPP
#define ALGO_BLAST_API___PRELIM_STAGE_HPP

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Search class to perform the preliminary stage of the BLAST search
class NCBI_XBLAST_EXPORT CBlastPrelimSearch : public CObject, public CThreadable
{
public:
    /// Constructor which creates and manages a BLAST database handle for 
    /// the caller
    /// @note we create a BlastSeqSrc using CSeqDB as its implementation
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       const CSearchDatabase& dbinfo);

    /// Constructor which creates BlastSeqSrc object from the already
    /// constructed BLAST database handle
    /// @note we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       CRef<CLocalDbAdapter> db);

    /// Constructor which takes a PSSM and an already initialized BlastSeqSrc
    /// object
    /// @note we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       BlastSeqSrc* seqsrc,
                       CConstRef<objects::CPssmWithParameters> pssm = null);

    /// Borrow the internal data and results results. 
    CRef<SInternalData> Run();

    /** @inheritDoc */
    virtual void SetNumberOfThreads(size_t nthreads);

    /// Set a function callback to be invoked by the CORE of BLAST to allow
    /// interrupting a BLAST search in progress
    TInterruptFnPtr SetInterruptCallback(TInterruptFnPtr fnptr,
                                         void* user_data = NULL);

    /// Checks that internal data is valid.  Used to know whether or not
    /// run should proceed or just print statistics for user.  This
    /// would most often be called if the problems in constructor are not bad enough to throw
    /// but there is no need to scan subject sequences.
    int CheckInternalData();


    /// Return HSPs in a structure other than the HSPStream? Provide
    /// conversion? How to combine this with CBlastTracebackStage?
    BlastHSPResults* ComputeBlastHSPResults(BlastHSPStream* stream,
                                            Uint4 max_num_hsps = 0,
                                            bool* rm_hsps = NULL) const;

    /// Retrieve any error/warning messages that occurred during the search
    TSearchMessages GetSearchMessages() const;

    /// Retrieve the filtered/masked query regions
    TSeqLocInfoVector GetFilteredQueryRegions() const;

private:
    /// Prohibit copy constructor
    CBlastPrelimSearch(const CBlastPrelimSearch& rhs);
    /// Prohibit assignment operator
    CBlastPrelimSearch& operator=(const CBlastPrelimSearch& rhs);

    /// Internal initialization function
    /// Initializes internal data structures except the BlastSeqSrc
    /// @param query_factory Contains query related data [in]
    /// @param options BLAST algorithm options [in]
    /// @param pssm PSSM to initialize PSI-BLAST
    /// @param seqsrc Wrapper for source of database sequences [in]
    void x_Init(CRef<IQueryFactory> query_factory,
                CRef<CBlastOptions> options,
                CConstRef<objects::CPssmWithParameters> pssm,
                BlastSeqSrc* seqsrc);

    /// Runs the preliminary search in multi-threaded mode
    /// @param internal_data internal preliminary data structures
    int x_LaunchMultiThreadedSearch(SInternalData& internal_data);

    /// Query factory is retained to ensure the lifetime of the data (queries)
    /// produced by it.
    CRef<IQueryFactory>             m_QueryFactory;
    CRef<SInternalData>             m_InternalData;
    CRef<CBlastOptions>             m_Options;

    /// Warnings and error messages
    TSearchMessages                 m_Messages;

    /// Query masking information
    TSeqLocInfoVector               m_MasksForAllQueries;
};

inline TSearchMessages
CBlastPrelimSearch::GetSearchMessages() const
{
    return m_Messages;
}

inline TSeqLocInfoVector
CBlastPrelimSearch::GetFilteredQueryRegions() const
{
    return m_MasksForAllQueries;
}

inline TInterruptFnPtr
CBlastPrelimSearch::SetInterruptCallback(TInterruptFnPtr fnptr, void* user_data)
{
    swap(m_InternalData->m_FnInterrupt, fnptr);
    m_InternalData->m_ProgressMonitor.Reset(new CSBlastProgress(SBlastProgressNew(user_data)));
    return fnptr;
}

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___PRELIM_STAGE__HPP */

