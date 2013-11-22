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
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// Forward declaration
class IBlastSeqInfoSrc;

class NCBI_XBLAST_EXPORT CBlastTracebackSearch : public CObject, public CThreadable
{
public:
    /// Create a BlastSeqSrc re-using an already created BlastSeqSrc
    /// @note We don't own the BlastSeqSrc
    CBlastTracebackSearch(CRef<IQueryFactory>   qf,
                          CRef<CBlastOptions>   opts,
                          BlastSeqSrc         * seqsrc,
                          CRef<IBlastSeqInfoSrc> seqinfosrc,
                          CRef<TBlastHSPStream> hsps,
                          CConstRef<objects::CPssmWithParameters> pssm = null);
    
    /// Use the internal data and return value of the preliminary
    /// search to proceed with the traceback.
    CBlastTracebackSearch(CRef<IQueryFactory> query_factory,
                          CRef<SInternalData> internal_data,
                          CRef<CBlastOptions>   opts,
                          CRef<IBlastSeqInfoSrc> seqinfosrc,
                          TSearchMessages& search_msgs);
    
    /// Destructor.
    virtual ~CBlastTracebackSearch();
    
    /// Run the traceback search.
    CRef<CSearchResultSet> Run();
    
    /// Specifies how the Seq-align-set returned as part of the
    /// results is formatted.
    void SetResultType(EResultType type);

    /// Sets the m_DBscanInfo field.
    void SetDBScanInfo(CRef<SDatabaseScanData> dbscan_info);

    /// Retrieve any error/warning messages that occurred during the search
    TSearchMessages GetSearchMessages() const;

private:
    /// Common initialization performed when doing traceback only
    void x_Init(CRef<IQueryFactory>   qf, 
                CRef<CBlastOptions>   opts,
                CConstRef<objects::CPssmWithParameters> pssm,
                const string        & dbname,
                CRef<TBlastHSPStream> hsps);
    
    /// Prohibit copy constructor
    CBlastTracebackSearch(CBlastTracebackSearch &);
    /// Prohibit assignment operator
    CBlastTracebackSearch & operator =(CBlastTracebackSearch &);
    
    // C++ data
    
    /// The query to search for.
    CRef<IQueryFactory>             m_QueryFactory;

    /// The options with which this search is configured.
    CRef<CBlastOptions>             m_Options;
    
    /// Fields and data from the preliminary search.
    CRef<SInternalData>             m_InternalData;
    
    /// Options from the preliminary search.
    const CBlastOptionsMemento*     m_OptsMemento;
    
    /// Warnings and Errors
    TSearchMessages m_Messages;
    
    /// Pointer to the IBlastSeqInfoSrc object to use to generate the
    /// Seq-aligns
    CRef<IBlastSeqInfoSrc> m_SeqInfoSrc;
    
    /// Determines if BLAST database search or BLAST 2 sequences style of
    /// results should be produced
    EResultType m_ResultType;
    
    /// Tracks information from database scanning phase.  Right now only used
    /// for the number of occurrences of a pattern in phiblast run.
    CRef<SDatabaseScanData> m_DBscanInfo;
};


inline TSearchMessages
CBlastTracebackSearch::GetSearchMessages() const
{
    return m_Messages;
}

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___TRACEBACK_STAGE__HPP */

