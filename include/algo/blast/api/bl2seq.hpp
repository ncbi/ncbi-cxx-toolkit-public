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
* File Description:
*   Blast2Sequences class interface
*
*/

#ifndef ALGO_BLAST_API___BL2SEQ__HPP
#define ALGO_BLAST_API___BL2SEQ__HPP

#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

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

    /// Constructor to compare 2 sequences
    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, EProgram p);

    /// Constructor to compare query against all subject sequences
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, EProgram p);

    /// Contructor to allow query concatenation
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            EProgram p);

    CBl2Seq(const SSeqLoc& query, const SSeqLoc& subject, 
            CBlastOptionsHandle& opts);

    /// Constructor to compare query against all subject sequences
    CBl2Seq(const SSeqLoc& query, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

    /// Contructor to allow query concatenation
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            CBlastOptionsHandle& opts);

    virtual ~CBl2Seq();

    void SetQuery(const SSeqLoc& query);
    const SSeqLoc& GetQuery() const;

    void SetQueries(const TSeqLocVector& queries);
    const TSeqLocVector& GetQueries() const;

    void SetSubject(const SSeqLoc& subject);
    const SSeqLoc& GetSubject() const;

    void SetSubjects(const TSeqLocVector& subjects);
    const TSeqLocVector& GetSubjects() const;

    void SetOptions(CBlastOptions& opts);
    CBlastOptions& SetOptions();
    const CBlastOptions& GetOptions() const;

    void SetOptionsHandle(CBlastOptionsHandle& opts);
    CBlastOptionsHandle& SetOptionsHandle();
    const CBlastOptionsHandle& GetOptionsHandle() const;

    // Perform BLAST search with multiple query sequences
    virtual TSeqAlignVector Run();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const vector< CConstRef<objects::CSeq_loc> >& GetFilteredQueryRegions() const;

protected:
    virtual void SetupSearch();
    virtual void ScanDB();
    virtual TSeqAlignVector x_Results2SeqAlign();

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         //< query sequence(s)
    TSeqLocVector        m_tSubjects;        //< sequence(s) to BLAST against
    CRef<CBlastOptionsHandle>  m_OptsHandle;         //< Blast options

    ///< Common initialization code for all c-tors
    void x_InitSeqs(const TSeqLocVector& queries, const TSeqLocVector& subjs);

    /// Prohibit copy constructor
    CBl2Seq(const CBl2Seq& rhs);
    /// Prohibit assignment operator
    CBl2Seq& operator=(const CBl2Seq& rhs);

    /************ Internal data structures (m_i = internal members)***********/
    bool                                mi_bQuerySetUpDone;
    CBLAST_SequenceBlk                  mi_clsQueries;  // one for all queries
    CBlastQueryInfo                     mi_clsQueryInfo; // one for all queries
    vector<BLAST_SequenceBlk*>          mi_vSubjects; // should use structures?
    unsigned int                        mi_iMaxSubjLength; // should use int8?

    BlastScoreBlk*                      mi_pScoreBlock;
    LookupTableWrap*                    mi_pLookupTable; // one for all queries
    ListNode*                           mi_pLookupSegments;

    CBlastInitialWordParameters         mi_clsInitWordParams;
    CBlastHitSavingParameters           mi_clsHitSavingParams;
    CBLAST_ExtendWord                   mi_clsExtnWord;
    CBlastExtensionParameters           mi_clsExtnParams;
    CBlastGapAlignStruct                mi_clsGapAlign;
    CBlastDatabaseOptions               mi_clsDbOptions;

    /// Vector of result structures, one per subject
    vector<BlastResults*>               mi_vResults;//should use structs?
    /// Vector of statistical return structures, should have one per query
    vector<BlastReturnStat>             mi_vReturnStats;

    /// Regions filtered out from the query sequence
    vector< CConstRef<objects::CSeq_loc> >       mi_vFilteredRegions;

    void x_ResetQueryDs();
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

inline CBlastOptions&
CBl2Seq::SetOptions()
{
    mi_bQuerySetUpDone = false;
    return m_OptsHandle->SetOptions();
}

inline void
CBl2Seq::SetOptions(CBlastOptions& opts)
{
    m_OptsHandle->SetOptions() = opts;
    mi_bQuerySetUpDone = false;
}

inline const CBlastOptions&
CBl2Seq::GetOptions() const
{
    return m_OptsHandle->GetOptions();
}

inline CBlastOptionsHandle&
CBl2Seq::SetOptionsHandle()
{
    mi_bQuerySetUpDone = false;
    return *m_OptsHandle;
}

inline void
CBl2Seq::SetOptionsHandle(CBlastOptionsHandle& opts)
{
    m_OptsHandle.Reset(&opts);
    mi_bQuerySetUpDone = false;
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

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
