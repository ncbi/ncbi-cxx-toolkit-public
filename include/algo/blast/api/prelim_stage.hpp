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

/** @file internal_interfaces.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___PRELIM_STAGE_HPP
#define ALGO_BLAST_API___PRELIM_STAGE_HPP

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/uniform_search.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declaration
class IBlastSeqSrcAdapter;

/// NOTE: Non-const CBlastOptions is only needed for RPS-BLAST!!!
class NCBI_XBLAST_EXPORT CBlastPrelimSearch : public CObject, public CThreadable
{
public:
    // we create a BlastSeqSrc using CSeqDB as its implementation
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       const CSearchDatabase& dbinfo);
    // we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       //CConstRef<CBlastOptions> options,
                       CRef<CBlastOptions> options,
                       IBlastSeqSrcAdapter& db);
    // we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       //CConstRef<CBlastOptions> options,
                       CRef<CBlastOptions> options,
                       BlastSeqSrc* seqsrc);
    ~CBlastPrelimSearch();

    /// Borrow the internal data and results results. 
    CRef<SInternalData> Run();

private:
    /// Prohibit copy constructor
    CBlastPrelimSearch(const CBlastPrelimSearch& rhs);
    /// Prohibit assignment operator
    CBlastPrelimSearch& operator=(const CBlastPrelimSearch& rhs);

    void x_Init(CRef<IQueryFactory> query_factory,
                CRef<CBlastOptions> options,
                const string& dbname);

    /// Runs the preliminary search in multi-threaded mode
    int x_LaunchMultiThreadedSearch();

    /// Query factory is retained to ensure the lifetime of the data (queries)
    /// produced by it.
    CRef<IQueryFactory>             m_QueryFactory;
    CRef<SInternalData>             m_InternalData;
    const CBlastOptionsMemento*     m_OptsMemento;
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___PRELIM_STAGE__HPP */

