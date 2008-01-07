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

/** @addtogroup AlgoBlast
 *
 * @{
 */

class CBlastFilterTest;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Runs the BLAST algorithm between 2 sequences.

class NCBI_XBLAST_EXPORT CBl2Seq : public CObject
{
public:

    /// Constructor to compare 2 sequences with default options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p);

    /// Constructor to compare query against all subject sequences with 
    /// default options
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, EProgram p);

    /// Constructor to allow query concatenation with default options
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            EProgram p);

    /// Constructor to compare 2 sequences with specified options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, 
            CBlastOptionsHandle& opts);

    /// Constructor to compare query against all subject sequences with
    /// specified options
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

    /// Constructor to allow query concatenation with specified options
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

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
    CRef<CSearchResultSet> RunEx();

    /// Runs the search but does not produce seqalign output
    /// (useful if the raw search results are needed, rather
    /// than a set of complete Seq-aligns)
    /// @deprecated Please DO NOT use this method, use Run() or RunEx() instead.
    NCBI_DEPRECATED virtual void RunWithoutSeqalignGeneration();

    /// Retrieves the list of HSP results from the engine
    /// (to be used after RunWithoutSeqalignGeneration() method)
    /// @deprecated Please DO NOT use this method, use Run() or RunEx() 
    /// instead, as this is an internal data structure of the BLAST engine
    NCBI_DEPRECATED BlastHSPResults* GetResults() const;

    /// Retrieves regions filtered on the query/queries
    TSeqLocInfoVector GetFilteredQueryRegions() const;

    /// Retrieves the diagnostics information returned from the engine
    BlastDiagnostics* GetDiagnostics() const;

    /// Get the ancillary results for a BLAST search (to be used with the Run()
    /// method)
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

protected:
    /// Process the queries, do setup, and build the lookup table.
    virtual void SetupSearch();

    /// Creates a BlastHSPStream and calls the engine.
    virtual void RunFullSearch();

    /// Return a seqalign list for each query/subject pair, even if it is empty.
    virtual TSeqAlignVector x_Results2SeqAlign();

    /// Transpose the (linearly organized) seqalign set matrix from
    /// (q1 s1 q2 s1 ... qN s1, ..., q1 sM q2 sM ... qN sM) to
    /// (q1 s1 q1 s2 ... q1 sM, ..., qN s1 qN s2 ... qN sM)
    /// this method only reorganizes the seqalign sets, does not copy them.
    TSeqAlignVector x_TransposeSeqAlignVector(const TSeqAlignVector& alnvec);

    /// Convert the TSeqLocVector to a vector of Seq-ids
    /// @param slv TSeqLocVector used as source [in]
    /// @param query_ids output of this method [in|out]
    static void x_SimplifyTSeqLocVector(const TSeqLocVector& slv,
                           vector< CConstRef<objects::CSeq_id> >& query_ids);

    /// Populate the internal m_AncillaryData member
    /// @param alignments aligments to use
    void x_BuildAncillaryData(const TSeqAlignVector& alignments);

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         ///< query sequence(s)
    TSeqLocVector        m_tSubjects;        ///< sequence(s) to BLAST against
    CRef<CBlastOptionsHandle>  m_OptsHandle; ///< Blast options

    /// Common initialization code for all c-tors
    void x_Init(const TSeqLocVector& queries, const TSeqLocVector& subjs);

    /// Prohibit copy constructor
    CBl2Seq(const CBl2Seq& rhs);
    /// Prohibit assignment operator
    CBl2Seq& operator=(const CBl2Seq& rhs);

    /************ Internal data structures (m_i = internal members)***********/
    bool                                mi_bQuerySetUpDone;    ///< internal: query processing already done?
    CBLAST_SequenceBlk                  mi_clsQueries;         ///< internal: one for all queries
    CBlastQueryInfo                     mi_clsQueryInfo;       ///< internal: one for all queries

    BlastSeqSrc*                        mi_pSeqSrc;            ///< internal: Subject sequences source
    BlastScoreBlk*                      mi_pScoreBlock;        ///< internal: score block
    CLookupTableWrap                    mi_pLookupTable;       ///< internal: one for all queries
    BlastSeqLoc*                        mi_pLookupSegments;    ///< internal: regions of queries to scan during lookup table creation

    /// Stores any warnings emitted during query setup
    TSearchMessages                     m_Messages;

    /// Results for all queries and subjects together
    BlastHSPResults*                    mi_pResults;
    /// Return search statistics data
    BlastDiagnostics*                   mi_pDiagnostics;

    /// Regions filtered out from the query sequences
    BlastMaskLoc* m_ipFilteredRegions;

    /// User-provided interrupt callback
    TInterruptFnPtr                     m_fnpInterrupt;
    /// Structure to aid in progress monitoring/interruption
    CSBlastProgress                     m_ProgressMonitor;
    /// Ancillary BLAST data
    CSearchResultSet::TAncillaryVector  m_AncillaryData;

    /// Resets query data structures
    void x_ResetQueryDs();
    /// Resets subject data structures
    void x_ResetSubjectDs();

    friend class ::CBlastFilterTest;
};

inline void
CBl2Seq::SetQuery(const SSeqLoc& query)
{
    x_ResetQueryDs();
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
    x_ResetQueryDs();
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
    x_ResetSubjectDs();
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
    x_ResetSubjectDs();
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
    mi_bQuerySetUpDone = false;
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

inline BlastHSPResults* CBl2Seq::GetResults() const
{
    return mi_pResults;
}

inline void
CBl2Seq::GetMessages(TSearchMessages& messages) const
{
    messages = m_Messages;
}

inline TInterruptFnPtr
CBl2Seq::SetInterruptCallback(TInterruptFnPtr fnptr, void* user_data)
{
    swap(m_fnpInterrupt, fnptr);
    m_ProgressMonitor.Reset(SBlastProgressNew(user_data));
    return fnptr;
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
