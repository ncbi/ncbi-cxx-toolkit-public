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

/** @file traceback_stage.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___TRACEBACK_STAGE_HPP
#define ALGO_BLAST_API___TRACEBACK_STAGE_HPP

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
class IBlastSeqInfoSrc;

class NCBI_XBLAST_EXPORT CBlastTracebackSearch : public CObject
{
public:
    // we create a BlastSeqSrc using CSeqDB as its implementation
    CBlastTracebackSearch(CRef<IQueryFactory>     qf,
                          CRef<CBlastOptions>     opts,
                          const CSearchDatabase & dbinfo,
                          CRef<TBlastHSPStream>   hsps);
    
    // we don't own the BlastSeqSrc
    CBlastTracebackSearch(CRef<IQueryFactory>   qf,
                          CRef<CBlastOptions>   opts,
                          IBlastSeqSrcAdapter & db,
                          CRef<TBlastHSPStream> hsps);
    
    // we don't own the BlastSeqSrc
    CBlastTracebackSearch(CRef<IQueryFactory>   qf,
                          CRef<CBlastOptions>   opts,
                          BlastSeqSrc         * seqsrc,
                          CRef<TBlastHSPStream> hsps);
    
    /// Use the internal data and return value of the preliminary search to
    /// proceed with the traceback.
    CBlastTracebackSearch(CRef<IQueryFactory> query_factory,
                          CRef<SInternalData> internal_data,
                          const CBlastOptions& opts,
                          const IBlastSeqInfoSrc& seqinfo_src);
    
    // is this enough? 
    virtual ~CBlastTracebackSearch();
    
    ISearch::TResults Run();
    
private:
    /// Common initialization performed when doing traceback only
    void x_Init(CRef<IQueryFactory> qf, 
                CRef<CBlastOptions> opts,
                const string& dbname);

    /// Initializes the IBlastSeqInfoSrc from data obtained from the
    /// BlastSeqSrc (works for database searches only)
    void x_InitSeqInfoSrc(const BlastSeqSrc* seqsrc);

    // Auto method generation prevention
    CBlastTracebackSearch(CBlastTracebackSearch &);
    CBlastTracebackSearch & operator =(CBlastTracebackSearch &);
    
    // C++ data
    
    CRef<IQueryFactory>             m_QueryFactory;
    CRef<CBlastOptions>             m_Options;
    CRef<SInternalData>             m_InternalData;
    const CBlastOptionsMemento*     m_OptsMemento; // the options memento...
    
    // Wrapped C objects
    
    // The output of traceback, I think...
    CRef< CStructWrapper<BlastHSPResults> > m_HspResults;

    /// Pointer to the IBlastSeqInfoSrc object to use to generate the
    /// Seq-aligns
    IBlastSeqInfoSrc* m_SeqInfoSrc;
    /// True if the field above must be deleted by this class, false otherwise
    bool m_OwnSeqInfoSrc;
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___TRACEBACK_STAGE__HPP */

