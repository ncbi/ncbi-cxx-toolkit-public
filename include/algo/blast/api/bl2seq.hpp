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

#include <BlastOption.hpp>

USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

/// Runs the BLAST algorithm between 2 sequences.
class CBl2Seq : public CObject
{
public:
    typedef vector< CConstRef<CSeq_loc> >   TSeqLocVector;
    typedef CBlastOption::EProgram          TProgram;

    /// Constructor to compare 2 sequences
    CBl2Seq(CConstRef<CSeq_loc>& query, CConstRef<CSeq_loc>& subject, 
            TProgram p, CScope* scope);

    /// Constructor to compare query against all subject sequences
    CBl2Seq(CConstRef<CSeq_loc>& query, const TSeqLocVector& subjects, 
            TProgram p, CScope* scope);

    /// Contructor to allow query concatenation
    CBl2Seq(const TSeqLocVector& queries, const TSeqLocVector& subjects,
            TProgram p, CScope* scope);

    virtual ~CBl2Seq();

    void SetQuery(CConstRef<CSeq_loc>& query);
    const CConstRef<CSeq_loc>& GetQuery() const;

    void SetSubject(const TSeqLocVector& subjects);
    const TSeqLocVector& GetSubject() const;

    void SetProgram(TProgram p);
    TProgram GetProgram() const;

    CRef<CBlastOption>& SetOptions();
    void SetOptions(CRef<CBlastOption>& opts);
    CRef<CBlastOption> GetOptions() const;

    virtual CRef<CSeq_align_set> Run();

    /// Retrieves regions filtered on the query
    CRef<CSeq_loc> GetFilteredQueryRegions() const;

#if 0
    // Temporary: Accessors for internal structures
    const BLAST_ScoreBlkPtr GetScoreBlkPtr() const { return mi_Sbp; }
    const BlastQueryInfoPtr GetQueryInfoPtr() const { return &*mi_QueryInfo; }
    BlastReturnStat GetReturnStats() const 
    { 
        if (mi_vReturnStats.size())
            return mi_vReturnStats.front();
    }
#endif


protected:
    virtual void SetupSearch();
    virtual void ScanDB();
    virtual void Traceback();
    virtual CRef<CSeq_align_set> x_Results2SeqAlign();

private:
    bool                 m_QuerySetUpDone;  //< done once for every query
    CConstRef<CSeq_loc>  m_Query;           //< query sequence
    TSeqLocVector        m_Subjects;        //< sequence(s) to BLAST against
    CRef<CBlastOption>   m_Options;         //< Blast options
    TProgram             m_Program;         //< Blast program
    CScope*              m_Scope;           //< Needed to retrieve sequences

    void x_Init(void);

    // Prohibit copy constructor
    CBl2Seq(const CBl2Seq&);
    // Prohibit copying
    CBl2Seq& operator=(const CBl2Seq&);

    /********************* Internal data structures ***************************/
    CBLAST_SequenceBlkPtr               mi_Query;
    CBlastQueryInfoPtr                  mi_QueryInfo;
    /// Vector of sequence blk structures, one per subject
    vector<BLAST_SequenceBlkPtr>        mi_vSubjects;      
    unsigned int                        mi_MaxSubjLength;

    BLAST_ScoreBlkPtr                   mi_Sbp;
    LookupTableWrapPtr                  mi_LookupTable;
    ListNode *                          mi_LookupSegments;

    CBlastInitialWordParametersPtr      mi_InitWordParams;
    CBlastHitSavingParametersPtr        mi_HitSavingParams;
    CBLAST_ExtendWordPtr                mi_ExtnWord;
    CBlastExtensionParametersPtr        mi_ExtnParams;
    CBlastGapAlignStructPtr             mi_GapAlign;
    CBlastDatabaseOptionsPtr            mi_DbOptions;

    /// Vector of result structures, one per subject
    vector<BlastResultsPtr>             mi_vResults;
    /// Vector of statistical return structures, one per subject
    vector<BlastReturnStat>             mi_vReturnStats;

    /// Regions filtered out from the query sequence
    CRef<CSeq_loc>                      mi_FilteredRegions;

    void x_SetupQuery();
    void x_SetupQueryInfo();
    void x_SetupSubjects();
    void x_ResetQueryDs();
    void x_ResetSubjectDs();
};

inline void
CBl2Seq::SetProgram(TProgram p)
{
    m_Program = p;
    m_Options->SetProgram(p);
    m_QuerySetUpDone = false;
}

inline CBl2Seq::TProgram
CBl2Seq::GetProgram() const
{
    return m_Program;
}

inline void
CBl2Seq::SetQuery(CConstRef<CSeq_loc>& query)
{
    m_Query.Reset(query);
    m_QuerySetUpDone = false;
}

inline const CConstRef<CSeq_loc>&
CBl2Seq::GetQuery() const
{
    return m_Query;
}

inline void
CBl2Seq::SetSubject(const TSeqLocVector& subjects)
{
    m_Subjects = subjects;
}

inline const CBl2Seq::TSeqLocVector&
CBl2Seq::GetSubject() const
{
    return m_Subjects;
}

inline CRef<CBlastOption>&
CBl2Seq::SetOptions()
{
    m_QuerySetUpDone = false;
    return m_Options;
}

inline void
CBl2Seq::SetOptions(CRef<CBlastOption>& opts)
{
    m_Options.Reset(opts);
    m_Program = m_Options->GetProgram();
    m_QuerySetUpDone = false;
}

inline CRef<CBlastOption>
CBl2Seq::GetOptions() const
{
    return m_Options;
}

inline CRef<CSeq_loc> 
CBl2Seq::GetFilteredQueryRegions() const
{
    return mi_FilteredRegions;
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
