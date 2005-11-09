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
 * Author: Christiam Camacho
 *
 */

/** @file local_search.hpp
 * Implementation of the uniform BLAST search interface for searching locally
 * installed BLAST databases
 */

#ifndef ALGO_BLAST_API___LOCAL_SEARCH_HPP
#define ALGO_BLAST_API___LOCAL_SEARCH_HPP

#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/psiblast.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Local sequence search
/// 
/// The local implementation of the uniform search interface for
/// sequence queries.
class NCBI_XBLAST_EXPORT CLocalSeqSearch : public ISeqSearch {
public:
    virtual ~CLocalSeqSearch();
    
    /// Configure the search.
    virtual void SetOptions(CRef<CBlastOptionsHandle> options);
    
    /// Set the databases to search.
    virtual void SetSubject(CConstRef<CSearchDatabase> subject);
    
    /// Set the factory which will return the queries to search for.
    virtual void SetQueryFactory(CRef<IQueryFactory> query_factory);
    
    /// Run the search.
    virtual CSearchResultSet Run();
    
private:
    /// Search options
    CRef<CBlastOptionsHandle> m_SearchOpts;

    /// Local database search class
    CRef<CLocalBlast>         m_LocalBlast;

    /// Sequence source for the search class to use
    BlastSeqSrc*              m_SeqSrc;

    /// Factory which provides the query data to be populated in m_Queries
    CRef<IQueryFactory>       m_QueryFactory;

    /// Warnings produced by the search.
    vector<string> m_Warnings;
};

class NCBI_XBLAST_EXPORT CLocalPssmSearch : public IPssmSearch {
public:
    /// Configure this search
    virtual void SetOptions(CRef<CBlastOptionsHandle> options);

    /// Set the databases to search
    virtual void SetSubject(CConstRef<CSearchDatabase> subject);

    /// Set the PSSM with which to search the database
    virtual void SetQuery(CRef<objects::CPssmWithParameters> pssm);

    /// Run a single iteration of the search
    virtual CSearchResultSet Run();

private:
    /// Search configuration
    CRef<CBlastOptionsHandle> m_SearchOpts;

    /// Search class
    CRef<CPsiBlast> m_PsiBlast;

    /// Search queries
    CRef<objects::CPssmWithParameters> m_Pssm;

    /// Subject database
    CConstRef<CSearchDatabase> m_Subject;

};

/// Factory for CLocalSearch.
/// 
/// This class is a concrete implementation of the ISearch factory.
/// Users desiring a local search will normally create an object of
/// this type but limit their interaction with it to the capabilities
/// of the ISearch interface.

class NCBI_XBLAST_EXPORT CLocalSearchFactory : public ISearchFactory {
public:
    /// Get an object to manage a local sequence search.
    virtual CRef<ISeqSearch>          GetSeqSearch();

    /// Get an object to manage a remote PSSM search.
    virtual CRef<IPssmSearch>          GetPssmSearch();

    /// Get an options handle for a search of the specified type.
    virtual CRef<CBlastOptionsHandle> GetOptions(EProgram);
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___LOCAL_SEARCH__HPP */

