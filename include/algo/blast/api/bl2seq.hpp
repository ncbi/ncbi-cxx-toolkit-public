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

#ifndef BL2SEQ__HPP
#define BL2SEQ__HPP

#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



/// Runs the BLAST algorithm between 2 sequences.
class NCBI_XBLAST_EXPORT CBl2Seq : public CObject
{
public:
    typedef CBlastOption::EProgram          TProgram;

    /// Constructor to compare 2 sequences
    CBl2Seq(SSeqLoc& query, SSeqLoc& subject, TProgram p);

    /// Constructor to compare query against all subject sequences
    CBl2Seq(SSeqLoc& query, const TSeqLocVector& subjects, TProgram p);

    /// Contructor to allow query concatenation
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects, 
            TProgram p);

    virtual ~CBl2Seq();

    void SetQuery(SSeqLoc& query);
    const SSeqLoc& GetQuery() const;

    void SetQueries(const TSeqLocVector& queries);
    const TSeqLocVector& GetQueries() const;

    void SetSubject(SSeqLoc& subject);
    const SSeqLoc& GetSubject() const;

    void SetSubjects(const TSeqLocVector& subjects);
    const TSeqLocVector& GetSubjects() const;

    void SetProgram(TProgram p);
    TProgram GetProgram() const;

    CBlastOption& SetOptions();
    void SetOptions(const CBlastOption& opts);
    const CBlastOption& GetOptions() const;

    virtual CRef<CSeq_align_set> Run();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const vector< CConstRef<CSeq_loc> >& GetFilteredQueryRegions() const;

protected:
    virtual void SetupSearch();
    virtual void ScanDB();
    virtual void Traceback();
    virtual CRef<CSeq_align_set> x_Results2SeqAlign();

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         //< query sequence(s)
    TSeqLocVector        m_tSubjects;        //< sequence(s) to BLAST against
    CBlastOption*        m_pOptions;         //< Blast options
    TProgram             m_eProgram;         //< Blast program FIXME ?needed?

    ///< Common initialization code for all c-tors
    void x_Init(const TSeqLocVector& queries, const TSeqLocVector& subjects);

    /// Prohibit copy constructor
    CBl2Seq(const CBl2Seq& rhs);
    /// Prohibit assignment operator
    CBl2Seq& operator=(const CBl2Seq& rhs);

    /************ Internal data structures (m_i = internal members)***********/
    //< done once for every query
    bool                                mi_bQuerySetUpDone;
    CBLAST_SequenceBlkPtr               mi_clsQueries;  // one for all queries
    CBlastQueryInfoPtr                  mi_clsQueryInfo; // one for all queries
    vector<BLAST_SequenceBlk*>          mi_vSubjects; // should use structures?
    unsigned int                        mi_iMaxSubjLength; // should use int8?

    BlastScoreBlk*                      mi_pScoreBlock;
    LookupTableWrap*                    mi_pLookupTable; // one for all queries
    ListNode*                           mi_pLookupSegments;

    CBlastInitialWordParametersPtr      mi_clsInitWordParams;
    CBlastHitSavingParametersPtr        mi_clsHitSavingParams;
    CBLAST_ExtendWordPtr                mi_clsExtnWord;
    CBlastExtensionParametersPtr        mi_clsExtnParams;
    CBlastGapAlignStructPtr             mi_clsGapAlign;
    CBlastDatabaseOptionsPtr            mi_clsDbOptions;

    /// Vector of result structures, one per subject
    vector<BlastResults*>               mi_vResults;//should use structs?
    /// Vector of statistical return structures, should have one per query
    vector<BlastReturnStat>             mi_vReturnStats;

    /// Regions filtered out from the query sequence
    //TSeqLocVector                       mi_vFilteredRegions;
    vector< CConstRef<CSeq_loc> >       mi_vFilteredRegions;

    //void x_SetupQuery();    // FIXME: should be setup_queries
    void x_SetupQueries();
    void x_SetupQueryInfo();// FIXME: Allow query concatenation
    void x_SetupSubjects();
    void x_ResetQueryDs();
    void x_ResetSubjectDs();
};

inline void
CBl2Seq::SetProgram(TProgram p)
{
    m_eProgram = p;  // FIXME: we could just store the program in options obj
    m_pOptions->SetProgram(p);
    mi_bQuerySetUpDone = false;
}

inline CBl2Seq::TProgram
CBl2Seq::GetProgram() const
{
    return m_eProgram;   // FIXME: return m_pOptions->GetProgram();
}

inline void
CBl2Seq::SetQuery(SSeqLoc& query)
{
    x_ResetQueryDs();
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
    m_tQueries = queries;
}

inline const TSeqLocVector&
CBl2Seq::GetQueries() const
{
    return m_tQueries;
}

inline void
CBl2Seq::SetSubject(SSeqLoc& subject)
{
    x_ResetSubjectDs();
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
    m_tSubjects = subjects;
}

inline const TSeqLocVector&
CBl2Seq::GetSubjects() const
{
    return m_tSubjects;
}

inline CBlastOption&
CBl2Seq::SetOptions()
{
    mi_bQuerySetUpDone = false;
    return *m_pOptions;
}

inline void
CBl2Seq::SetOptions(const CBlastOption& opts)
{
    m_pOptions = const_cast<CBlastOption*>(&opts);
    m_eProgram = m_pOptions->GetProgram();
    mi_bQuerySetUpDone = false;
}

inline const CBlastOption&
CBl2Seq::GetOptions() const
{
    return *m_pOptions;
}

inline const vector< CConstRef<CSeq_loc> >&
CBl2Seq::GetFilteredQueryRegions() const
{
    return mi_vFilteredRegions;
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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

#endif  /* BL2SEQ__HPP */
