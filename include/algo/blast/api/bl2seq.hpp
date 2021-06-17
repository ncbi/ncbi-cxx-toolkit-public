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

/// @file bl2seq.hpp
/// Declares the CBl2Seq (BLAST 2 Sequences) class

#ifndef ALGO_BLAST_API___BL2SEQ__HPP
#define ALGO_BLAST_API___BL2SEQ__HPP

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/local_blast.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

class CBlastFilterTest;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Runs the BLAST algorithm between 2 sequences.
/// @note this is a single-BLAST search run object (i.e.: it caches the results
/// after a BLAST search is done). If multiple BLAST searches with different
/// queries, subjects, or options are required, please create a separate object
class NCBI_XBLAST_EXPORT CBl2Seq : public CObject
{
public:

    /// Constructor to compare 2 sequences with default options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p);

    /// Constructor to compare query against all subject sequences with 
    /// default options
    /// @param dbscan_mode Database search mode (as opposed to pairwise)
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, EProgram p,
	bool dbscan_mode=false);

    /// Constructor to allow query concatenation with default options
    /// @param dbscan_mode Database search mode (as opposed to pairwise)
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            EProgram p, bool dbscan_mode=false);

    /// Constructor to compare 2 sequences with specified options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, 
            CBlastOptionsHandle& opts);

    /// Constructor to compare query against all subject sequences with
    /// specified options
    /// @param dbscan_mode Database search mode (as opposed to pairwise)
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts, bool dbscan_mode=false);

    /// Constructor to allow query concatenation with specified options
    /// @param dbscan_mode Database search mode (as opposed to pairwise)
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts, bool dbscan_mode=false);

    /// Destructor
    virtual ~CBl2Seq();
    
    /// Set the query.
    void SetQuery(const SSeqLoc& query);

    /// Retrieve the query sequence.
    const SSeqLoc& GetQuery() const;

    /// Set a vector of query sequences for a concatenated search.
    void SetQueries(const TSeqLocVector& queries);

    /// Retrieve a vector of query sequences.
    const TSeqLocVector& GetQueries() const;

    /// Set the subject sequence.
    void SetSubject(const SSeqLoc& subject);

    /// Retrieve the subject sequence.
    const SSeqLoc& GetSubject() const;

    /// Set a vector of subject sequences.
    void SetSubjects(const TSeqLocVector& subjects);

    /// Retrieve a vector of subject sequences.
    const TSeqLocVector& GetSubjects() const;

    /// Set the options handle.
    CBlastOptionsHandle& SetOptionsHandle();

    /// Retrieve the options handle.
    const CBlastOptionsHandle& GetOptionsHandle() const;

    /// Perform BLAST search
    /// Assuming N queries and M subjects, the structure of the returned 
    /// vector is as follows, with types indicated in parenthesis:
    /// TSeqAlignVector = 
    ///     [ {Results for query 1 and subject 1 (Seq-align-set)},
    ///       {Results for query 1 and subject 2 (Seq-align-set)}, ...
    ///       {Results for query 1 and subject M (Seq-align-set)},
    ///       {Results for query 2 and subject 1 (Seq-align-set)},
    ///       {Results for query 2 and subject 2 (Seq-align-set)}, ...
    ///       {Results for query 2 and subject M (Seq-align-set)},
    ///       {Results for query 3 and subject 1 (Seq-align-set)}, ...
    ///       {Results for query N and subject M (Seq-align-set)} ]
    virtual TSeqAlignVector Run();

    /// Performs the same functionality as Run(), but it returns a different
    /// data type
    /// @note the number of CSearchResultSet::value_type objects in this
    /// function's return value will be (number of queries * number of
    /// subjects)
    CRef<CSearchResultSet> RunEx();

    /// Retrieves regions filtered on the query/queries
    TSeqLocInfoVector GetFilteredQueryRegions() const;

    /// Retrieves regions filtered on the subject sequence(s)
    /// @param retval the return value of this method [in|out]
    void GetFilteredSubjectRegions(vector<TSeqLocInfoVector>& retval) const;

    /// Retrieves the diagnostics information returned from the engine
    BlastDiagnostics* GetDiagnostics() const;

    /// Get the ancillary results for a BLAST search (to be used with the Run()
    /// method)
    /// @param retval the return value of this method [in|out]
    void GetAncillaryResults(CSearchResultSet::TAncillaryVector& retval) const;

    /// Returns error messages/warnings.
    void GetMessages(TSearchMessages& messages) const;

    /// Set a function callback to be invoked by the CORE of BLAST to allow
    /// interrupting a BLAST search in progress.
    /// @param fnptr pointer to callback function [in]
    /// @param user_data user data to be attached to SBlastProgress structure
    /// [in]
    /// @return the previously set TInterruptFnPtr (NULL if none was
    /// provided before)
    TInterruptFnPtr SetInterruptCallback(TInterruptFnPtr fnptr, 
                                         void* user_data = NULL);

    /// Converts m_Results data member to a TSeqAlignVector
    static TSeqAlignVector
        CSearchResultSet2TSeqAlignVector(CRef<CSearchResultSet> res);
protected:
    /// Populate the internal m_AncillaryData member
    void x_BuildAncillaryData();

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         ///< query sequence(s)
    TSeqLocVector        m_tSubjects;        ///< sequence(s) to BLAST against
    CRef<CBlastOptionsHandle>  m_OptsHandle; ///< Blast options
    CRef<CLocalBlast>    m_Blast;            ///< The actual BLAST instance
    bool		m_DbScanMode;        ///< Scan like a databsase (as opposed to pairwise)

    /// Common initialization code for all c-tors
    void x_Init(const TSeqLocVector& queries, const TSeqLocVector& subjs);
    /// Common initialization of the CLocalBlast object
    void x_InitCLocalBlast();

    /// Prohibit copy constructor
    CBl2Seq(const CBl2Seq& rhs);
    /// Prohibit assignment operator
    CBl2Seq& operator=(const CBl2Seq& rhs);

    /// Stores any warnings emitted during query setup
    TSearchMessages                     m_Messages;

    /************ Internal data structures (m_i = internal members)***********/
    /// Return search statistics data
    BlastDiagnostics*                   mi_pDiagnostics;

    /// Ancillary BLAST data
    CSearchResultSet::TAncillaryVector  m_AncillaryData;
    
    /// CLocalBlast results
    CRef<CSearchResultSet> m_Results;

    /// Interrupt callback
    TInterruptFnPtr m_InterruptFnx;
    /// Interrupt user datacallback
    void* m_InterruptUserData;

    /// Clean up structures and results from any previous search
    void x_ResetInternalDs();

    friend class ::CBlastFilterTest;
};

inline void
CBl2Seq::SetQuery(const SSeqLoc& query)
{
    x_ResetInternalDs();
    m_tQueries.clear();
    m_tQueries.push_back(query);
}

inline const SSeqLoc&
CBl2Seq::GetQuery() const
{
    return m_tQueries.front();
}

inline void
CBl2Seq::SetQueries(const TSeqLocVector& queries)
{
    x_ResetInternalDs();
    m_tQueries.clear();
    m_tQueries = queries;
}

inline const TSeqLocVector&
CBl2Seq::GetQueries() const
{
    return m_tQueries;
}

inline void
CBl2Seq::SetSubject(const SSeqLoc& subject)
{
    x_ResetInternalDs();
    m_tSubjects.clear();
    m_tSubjects.push_back(subject);
}

inline const SSeqLoc&
CBl2Seq::GetSubject() const
{
    return m_tSubjects.front();
}

inline void
CBl2Seq::SetSubjects(const TSeqLocVector& subjects)
{
    x_ResetInternalDs();
    m_tSubjects.clear();
    m_tSubjects = subjects;
}

inline const TSeqLocVector&
CBl2Seq::GetSubjects() const
{
    return m_tSubjects;
}

inline CBlastOptionsHandle&
CBl2Seq::SetOptionsHandle()
{
    x_ResetInternalDs();
    return *m_OptsHandle;
}

inline const CBlastOptionsHandle&
CBl2Seq::GetOptionsHandle() const
{
    return *m_OptsHandle;
}

inline BlastDiagnostics* CBl2Seq::GetDiagnostics() const
{
    return mi_pDiagnostics;
}

inline void
CBl2Seq::GetMessages(TSearchMessages& messages) const
{
    messages = m_Messages;
}

inline TInterruptFnPtr
CBl2Seq::SetInterruptCallback(TInterruptFnPtr fnptr, void* user_data)
{
    TInterruptFnPtr tmp = m_InterruptFnx;
    m_InterruptFnx = fnptr;
    m_InterruptUserData = user_data;
    return tmp;
}

inline void 
CBl2Seq::GetAncillaryResults(CSearchResultSet::TAncillaryVector& retval) const
{
    retval = m_AncillaryData;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BL2SEQ__HPP */
