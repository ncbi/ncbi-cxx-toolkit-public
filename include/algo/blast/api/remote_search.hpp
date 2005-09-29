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
 * Author: Kevin Bealer
 *
 */

/** @file remote_search.hpp
 * Remote implementation of the uniform BLAST search interface.
 */

#ifndef ALGO_BLAST_API___REMOTE_SEARCH_HPP
#define ALGO_BLAST_API___REMOTE_SEARCH_HPP

#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/remote_blast.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Remote Sequence Search
/// 
/// The remote implementation of the uniform search interface for
/// sequence queries.

class NCBI_XBLAST_EXPORT CRemoteSeqSearch : public ISeqSearch {
public:
    /// Configure the search.
    virtual void SetOptions(CRef<CBlastOptionsHandle> options);
    
    /// Set the databases to search.
    virtual void SetSubject(CConstRef<CSearchDatabase> subject);
    
    /// Set the factory which will return the queries to search for.
    virtual void SetQueryFactory(CRef<IQueryFactory> query_factory);
    
    /// Run the search.
    virtual TResults Run();
    
private:
    /// Method to construct and run the remote blast search.
    CRemoteBlast & x_RemoteBlast();
    
    /// Search configuration.
    CRef<CBlastOptionsHandle> m_SearchOpts;
    
    /// Remote search management object.
    CRef<CRemoteBlast> m_RemoteBlast;
    
    /// Search queries
    CRef<IRemoteQueryData> m_Queries;
    
    /// Search subject.
    CConstRef<CSearchDatabase> m_Subject;
    
    /// Warnings produced by the search.
    vector<string> m_Warnings;
};

/// Remote Sequence Search
/// 
/// The remote implementation of the uniform search interface for PSSM
/// queries.

class NCBI_XBLAST_EXPORT CRemotePssmSearch : public IPssmSearch {
public:
    /// Configure the search.
    virtual void SetOptions(CRef<CBlastOptionsHandle> options);
    
    /// Set the databases to search.
    virtual void SetSubject(CConstRef<CSearchDatabase> subject);
    
    /// Set the query to search with.
    virtual void SetQuery(CRef<objects::CPssmWithParameters> query);
    
    /// Run the search.
    virtual TResults Run();
    
private:
    /// Method to construct and run the remote blast search.
    CRemoteBlast & x_RemoteBlast();
    
    /// Search configuration.
    CRef<CBlastOptionsHandle> m_SearchOpts;
    
    /// Remote search management object.
    CRef<CRemoteBlast> m_RemoteBlast;
    
    /// Search queries
    CRef<objects::CPssmWithParameters> m_Pssm;
    
    /// Search subject.
    CConstRef<CSearchDatabase> m_Subject;
    
    /// Warnings produced by the search.
    vector<string> m_Warnings;
};

/// Factory for CRemoteSearch.
/// 
/// This class is a concrete implementation of the ISearch factory.
/// Users desiring a remote search will normally create an object of
/// this type but limit their interaction with it to the capabilities
/// of the ISearch interface.

class NCBI_XBLAST_EXPORT CRemoteSearchFactory : public ISearchFactory {
public:
    /// Get an object to manage a remote sequence search.
    virtual CRef<ISeqSearch> GetSeqSearch();
    
    /// Get an object to manage a remote PSSM search.
    virtual CRef<IPssmSearch> GetPssmSearch();
    
    /// Get an options handle for a search of the specified type.
    virtual CRef<CBlastOptionsHandle> GetOptions(EProgram);
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___REMOTE_SEARCH__HPP */

