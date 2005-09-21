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

/** @file uniform_search.hpp
 * Uniform BLAST Search Interface.
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___UNIFORM_SEARCH_HPP
#define ALGO_BLAST_API___UNIFORM_SEARCH_HPP

#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


// Errors & Warnings:
// 
// An error is defined as a condition that halts or severely affects
// processing of one or more queries, and is represented as a string.
// A warning is defined as a detected condition or event that is
// reported by the search code, and may influence interpretation of
// the output, but does not stop the search or invalidate the
// results.

/// Exception class
///
/// Searches throw this when an error condition is detected in the
/// usage or execution of a search.  An example of a case where an
/// exception is appropriate is when a database cannot be found for a
/// local search, or if a memory allocation fails.  An example of a
/// non-exception error is if a search is completely masked.

class CSearchException : public CException {
public:
    /// Errors are classified into one of two types.
    enum EErrCode {
        /// Argument validation failed.
        eConfigErr,
        
        /// Memory allocation failed.
        eMemErr,
        
        /// Internal error (e.g. unimplemented methods).
        eInternal
    };
    
    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eConfigErr: return "eConfigErr";
        case eMemErr:    return "eMemErr";
        case eInternal:  return "eInternal";
        default:         return CException::GetErrCodeString();
        }
    }
    
    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CSearchException, CException);
};


/// Blast Search Subject
class CSearchDatabase : public CObject {
public:
    typedef vector<int> TGiList;
    enum EMoleculeType {
        eBlastDbIsProtein,
        eBlastDbIsNucleotide
    };

    CSearchDatabase(const string& dbname, EMoleculeType mol_type);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const string& entrez_query);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const TGiList& gilist);

    CSearchDatabase(const string& dbname, EMoleculeType mol_type,
                    const string& entrez_query, const TGiList& gilist);

    void SetDatabaseName(const string& dbname);
    string GetDatabaseName() const;

    void SetMoleculeType(EMoleculeType mol_type);
    EMoleculeType GetMoleculeType() const;

    void SetEntrezQueryLimitation(const string& entrez_query);
    string GetEntrezQueryLimitation() const;

    void SetGiListLimitation(const TGiList& gilist);
    const TGiList& GetGiListLimitation() const;

private:
    string          m_DbName;
    EMoleculeType   m_MolType;
    string          m_EntrezQueryLimitation;
    TGiList         m_GiListLimitation;
};


/// Error or Warning Message from search.
/// 
/// This class encapsulates a single error or warning message returned
/// from a search.  These include conditions detected by the algorithm
/// where no exception is thrown, but which impact the completeness or
/// accuracy of search results.  One example might be a completely
/// masked query.

class CSearchMessage : public CObject {
public:
    CSearchMessage(EBlastSeverity   severity,
                   int              error_id,
                   const string   & message)
        : m_Severity(severity), m_ErrorId(error_id), m_Message(message)
    {
    }
    
    CSearchMessage()
        : m_Severity(EBlastSeverity(0)), m_ErrorId(0)
    {
    }
    
    EBlastSeverity GetSeverity() const
    {
        return m_Severity;
    }
    
    string GetSeverityString() const
    {
        return GetSeverityString(m_Severity);
    }
    
    static string GetSeverityString(EBlastSeverity severity)
    {
        switch(severity) {
        case eBlastSevInfo:    return "Informational Message";
        case eBlastSevWarning: return "Warning";
        case eBlastSevError:   return "Error";
        case eBlastSevFatal:   return "Fatal Error";
        }
        return "Message";
    }
    
    int GetErrorId() const
    {
        return m_ErrorId;
    }
    
    string GetMessage() const
    {
        return GetSeverityString() + ": " + m_Message;
    }
    
private:
    CSearchMessage & operator =(const CSearchMessage &);
    CSearchMessage(CSearchMessage&);
    
    EBlastSeverity m_Severity;
    int            m_ErrorId;
    string         m_Message;
};


/// Search Results for One Query.
/// 
/// This class encapsulates all the search results and related data
/// corresponding to one of the input queries.

class CSearchResults : public CObject {
public:
    typedef vector< CRef<CSearchMessage> > TErrors;
    
    CSearchResults(CRef<CSeq_align_set> align, const TErrors & errs)
        : m_Alignment(align), m_Errors(errs)
    {
    }
    
    CConstRef<CSeq_align_set> GetSeqAlign() const
    {
        return m_Alignment;
    }
    
    CConstRef<CSeq_id> GetSeqId() const;
    
    TErrors GetErrors(int min_severity = eBlastSevError) const;

    // FIXME: add list of query masked regions with frame information for
    // translated searches. To do this one might need to create (or move) some
    // type similar to CDisplaySeqalign::SeqlocInfo as well as some function
    // like Blast_GetSeqLocInfoVector
    // (CVSROOT/individual/dondosha/blast_demo/format_util.cpp).
    
private:
    CRef<CSeq_align_set> m_Alignment;
    
    TErrors m_Errors;
};


/// Search Results for All Queries.
/// 
/// This class encapsulates all of the search results and related data
/// from a search.

class CSearchResultSet {
public:
    typedef vector< vector< CRef< CSearchMessage > > > TMessages;
    
    CSearchResultSet()
    {
    }
    
    CSearchResultSet(TSeqAlignVector aligns, TMessages msg_vec);
    
    CSearchResults & operator[](int i)
    {
        return *m_Results[i];
    }
    
    const CSearchResults & operator[](int i) const
    {
        return *m_Results[i];
    }
    
    CRef<CSearchResults> operator[](const objects::CSeq_id & ident);
    
    CConstRef<CSearchResults> operator[](const objects::CSeq_id & ident) const;
    
    int GetNumResults() const
    {
        return (int) m_Results.size();
    }
    
    void AddResult(CRef<CSearchResults> result)
    {
        m_Results.push_back(result);
    }
    
private:
    typedef vector< CRef<CSearchResults> > TResultsVector;
    
    /// Vector of results.
    TResultsVector m_Results;
};


/// Single Iteration Blast Database Search
/// 
/// FIXME: update comment
///
/// This class is the top-level Uniform Search Interface for blast
/// searches.  Concrete subclasses of this class will create and
/// return concrete subclasses of the IBlastDbSearch class.  Use this
/// class when you need to write code that decribes an algorithm over
/// the abstract IBlastDbSearch API, and is ignorant of the concrete
/// type of search it is performing.

class ISearch : public CObject {
public:
    // Configuration
    
    /// Configure the search
    virtual void SetOptions(CRef<CBlastOptionsHandle> options) = 0;
    
    /// Set the subject database(s) to search
    virtual void SetSubject(CConstRef<CSearchDatabase> subject) = 0;
    
    
    // Processing
    typedef CSearchResultSet TResults;
    
    /// Run the search to completion.
    virtual TResults Run() = 0;
};

class ISeqSearch : public ISearch {
public:

    virtual ~ISeqSearch() {}
    
    // Inputs
    
    /// Set the queries to search
    virtual void SetQueryFactory(CRef<IQueryFactory> query_factory) = 0;
};


/// Experimental inteface (since this does not provide a full interface to
/// PSI-BLAST)
class IPssmSearch : public ISearch {
public:
    virtual void SetQuery(CRef<objects::CPssmWithParameters> pssm) = 0;
};


/// Factory for ISearch.
/// 
/// This class is an abstract factory class for the ISearch class.
/// Concrete subclasses of this class will create and return concrete
/// subclasses of the ISearch class.  Use this class when you need to
/// write code that decribes an algorithm over the abstract ISearch
/// API, and is ignorant of the concrete type of search it is
/// performing (i.e.: local vs. remote search).

class ISearchFactory : public CObject {
public:
    virtual CRef<ISeqSearch>          GetSeqSearch()  = 0;
    virtual CRef<IPssmSearch>         GetPssmSearch()  = 0;
    virtual CRef<CBlastOptionsHandle> GetOptions(EProgram) = 0;
};


END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___UNIFORM_SEARCH__HPP */

