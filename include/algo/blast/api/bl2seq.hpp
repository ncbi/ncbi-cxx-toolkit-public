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

#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Runs the BLAST algorithm between 2 sequences.

class NCBI_XBLAST_EXPORT CBl2Seq : public CObject
{
public:

    /// Constructor to compare 2 sequences with default options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p);

    /// Constructor to compare query against all subject sequences with default options
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, EProgram p);

    /// Constructor to allow query concatenation with default options
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            EProgram p);

    /// Constructor to compare 2 sequences with specified options
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, 
            CBlastOptionsHandle& opts);

    /// Constructor to compare query against all subject sequences with specified options
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

    /// Constructor to allow query concatenation with specified options
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

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
    /// TSeqAlignVector = [ {Results for query 1 (Seq-align-set)}, 
    ///                     {Results for query 2 (Seq-align-set)}, ...
    ///                     {Results for query N (Seq-align-set)} ]
    /// 
    /// The individual query-subject alignments are returned in the
    /// CSeq_align_set for that query:
    /// {Results for query i} = 
    ///     [ {Results for query i and subject 1 (discontinuous Seq-align)}, 
    ///       {Results for query i and subject 2 (discontinuous Seq-align)}, ...
    ///       {Results for query i and subject M (discontinuous Seq-align)} ]
    /// Discontinuous Seq-aligns are used to allow grouping of multiple HSPs
    /// that correspond to that query-subject alignment.
    virtual TSeqAlignVector Run();

    /// Runs the search but does not produce seqalign output
    /// (useful if the raw search results are needed, rather
    /// than a set of complete Seq-aligns)
    virtual void PartialRun();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const vector< CConstRef<objects::CSeq_loc> >& GetFilteredQueryRegions() const;
    /// Retrieves the diagnostics information returned from the engine
    BlastDiagnostics* GetDiagnostics() const;

    /// Retrieves the list of HSP results from the engine
    /// (to be used after PartialRun() method)
    BlastHSPResults* GetResults() const;

    /// Returns error messages/warnings. Caller is responsible for deallocating
    /// return value if any
    Blast_Message* GetErrorMessage() const;

protected:
    /// Process the queries, do setup, and build the lookup table.
    virtual void SetupSearch();

    /// Creates a BlastHSPStream and calls the engine.
    virtual void ScanDB();

    /// Return a seqalign list for each query/subject pair, even if it is empty.
    virtual TSeqAlignVector x_Results2SeqAlign();

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         ///< query sequence(s)
    TSeqLocVector        m_tSubjects;        ///< sequence(s) to BLAST against
    CRef<CBlastOptionsHandle>  m_OptsHandle; ///< Blast options

    /// Common initialization code for all c-tors
    void x_InitSeqs(const TSeqLocVector& queries, const TSeqLocVector& subjs);

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
    CBlast_Message                      mi_clsBlastMessage;

    /// Results for all queries and subjects together
    BlastHSPResults*                    mi_pResults;
    /// Return search statistics data
    BlastDiagnostics*                   mi_pDiagnostics;

    /// Regions filtered out from the query sequence, one per query
    vector< CConstRef<objects::CSeq_loc> >       mi_vFilteredRegions;

    /// Resets query data structures
    void x_ResetQueryDs();
    /// Resets subject data structures
    void x_ResetSubjectDs();
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

inline const vector< CConstRef<objects::CSeq_loc> >&
CBl2Seq::GetFilteredQueryRegions() const
{
    return mi_vFilteredRegions;
}

inline BlastDiagnostics* CBl2Seq::GetDiagnostics() const
{
    return mi_pDiagnostics;
}

inline BlastHSPResults* CBl2Seq::GetResults() const
{
    return mi_pResults;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.42  2005/06/23 16:18:45  camacho
* Doxygen fixes
*
* Revision 1.41  2005/05/12 15:35:35  camacho
* Remove dead fields
*
* Revision 1.40  2005/03/31 13:43:49  camacho
* BLAST options API clean-up
*
* Revision 1.39  2005/01/07 15:17:33  papadopo
* additional clarification on using PartialRun() and GetResults()
*
* Revision 1.38  2005/01/06 15:40:19  camacho
* Add methods to retrieve error messages/warnings
*
* Revision 1.37  2004/09/13 12:44:11  madden
* Use BlastSeqLoc rather than ListNode
*
* Revision 1.36  2004/06/29 14:17:24  papadopo
* add PartialRun and GetResults methods to CBl2Seq
*
* Revision 1.35  2004/06/27 18:49:17  coulouri
* doxygen fixes
*
* Revision 1.34  2004/06/14 15:41:44  dondosha
* "Doxygenized" some comments.
*
* Revision 1.33  2004/05/14 17:15:59  dondosha
* BlastReturnStat structure changed to BlastDiagnostics and refactored
*
* Revision 1.32  2004/05/14 16:02:56  madden
* Rename BLAST_ExtendWord to Blast_ExtendWord in order to fix conflicts with C toolkit
*
* Revision 1.31  2004/03/24 19:12:48  dondosha
* Use auto class wrapper for lookup tabl wrap field
*
* Revision 1.30  2004/03/19 18:56:04  camacho
* Move to doxygen AlgoBlast group
*
* Revision 1.29  2004/03/15 19:55:28  dondosha
* Use sequence source instead of accessing subjects directly
*
* Revision 1.28  2004/02/13 21:21:44  camacho
* Add throws clause to Run method
*
* Revision 1.27  2003/12/09 13:41:22  camacho
* Added comment to Run method
*
* Revision 1.26  2003/12/03 16:36:07  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.25  2003/11/27 04:24:39  camacho
* Remove unneeded setters for options
*
* Revision 1.24  2003/11/26 18:36:44  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.23  2003/11/26 18:22:13  camacho
* +Blast Option Handle classes
*
* Revision 1.22  2003/11/03 15:20:20  camacho
* Make multiple query processing the default for Run().
*
* Revision 1.21  2003/10/16 03:16:39  camacho
* Fix to setting queries/subjects
*
* Revision 1.20  2003/09/11 17:44:39  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.19  2003/09/09 20:31:21  camacho
* Add const type qualifier
*
* Revision 1.18  2003/09/09 12:53:31  camacho
* Moved setup member functions to blast_setup_cxx.cpp
*
* Revision 1.17  2003/08/28 17:36:21  camacho
* Delete options before reassignment
*
* Revision 1.16  2003/08/25 17:15:33  camacho
* Removed redundant typedef
*
* Revision 1.15  2003/08/19 22:11:16  dondosha
* Cosmetic changes
*
* Revision 1.14  2003/08/19 20:24:17  dondosha
* Added TSeqAlignVector type as a return type for results-to-seqalign functions and input for formatting
*
* Revision 1.13  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.12  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.11  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.10  2003/08/15 16:01:02  dondosha
* TSeqLoc and TSeqLocVector types definitions moved to blast_aux.hpp, so all applications can use them
*
* Revision 1.9  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.8  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.7  2003/08/08 19:42:14  dicuccio
* Compilation fixes: #include file relocation; fixed use of 'list' and 'vector'
* as variable names
*
* Revision 1.6  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.5  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.4  2003/07/30 19:58:02  coulouri
* use ListNode
*
* Revision 1.3  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.2  2003/07/14 22:16:37  camacho
* Added interface to retrieve masked regions
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BL2SEQ__HPP */
