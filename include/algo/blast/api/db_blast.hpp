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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Database BLAST class interface
*
*/

#ifndef ALGO_BLAST_API___DBBLAST__HPP
#define ALGO_BLAST_API___DBBLAST__HPP

#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/core/blast_seqsrc.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Runs the BLAST algorithm between 2 sequences.
class NCBI_XBLAST_EXPORT CDbBlast : public CObject
{
public:

    /// Contructor to allow query concatenation
    CDbBlast(const TSeqLocVector& queries, 
             BlastSeqSrc* bssp, EProgram p);

    virtual ~CDbBlast();

    void SetQueries(const TSeqLocVector& queries);
    const TSeqLocVector& GetQueries() const;

    CBlastOptions& SetOptions();
    void SetOptions(const CBlastOptions& opts);
    const CBlastOptions& GetOptions() const;

    CBlastOptionsHandle& SetOptionsHandle();
    const CBlastOptionsHandle& GetOptionsHandle() const;

    // Perform BLAST search
    virtual TSeqAlignVector Run();
    // Run BLAST search without traceback
    virtual void PartialRun();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const BlastMaskLoc* GetFilteredQueryRegions() const;

    BlastSeqSrc* GetSeqSrc() const;
    BlastHSPResults* GetResults() const;
    BlastReturnStat* GetReturnStats() const;
    BlastScoreBlk* GetScoreBlk() const;

protected:
    virtual int SetupSearch();
    virtual void RunSearchEngine();
    virtual TSeqAlignVector x_Results2SeqAlign();

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         //< query sequence(s)
    BlastSeqSrc*         m_pSeqSrc;          //< Subject sequences sorce
    CRef<CBlastOptionsHandle>  m_OptsHandle; //< Blast options

    /// Prohibit copy constructor
    CDbBlast(const CDbBlast& rhs);
    /// Prohibit assignment operator
    CDbBlast& operator=(const CDbBlast& rhs);

    /************ Internal data structures (m_i = internal members)***********/
    bool                mi_bQuerySetUpDone;
    CBLAST_SequenceBlk  mi_clsQueries;  // one for all queries
    CBlastQueryInfo     mi_clsQueryInfo; // one for all queries
    BlastScoreBlk*      mi_pScoreBlock; // Karlin-Altschul parameters
    LookupTableWrap*    mi_pLookupTable; // one for all queries
    ListNode*           mi_pLookupSegments; /* Intervals for which lookup 
                                               table is created: complement of
                                               filtered regions */
    BlastMaskLoc*          mi_pFilteredRegions; // Filtered regions

    /// Results structure
    BlastHSPResults*                       mi_pResults;
    /// Statistical return structures
    BlastReturnStat*                    mi_pReturnStats;


   
    void x_ResetQueryDs();
};

inline void
CDbBlast::SetQueries(const TSeqLocVector& queries)
{
    x_ResetQueryDs();
    m_tQueries.clear();
    m_tQueries = queries;
}

inline const TSeqLocVector&
CDbBlast::GetQueries() const
{
    return m_tQueries;
}

inline CBlastOptions&
CDbBlast::SetOptions()
{
    mi_bQuerySetUpDone = false;
    return m_OptsHandle->SetOptions();
}

inline const CBlastOptions&
CDbBlast::GetOptions() const
{
    return m_OptsHandle->GetOptions();
}

inline CBlastOptionsHandle&
CDbBlast::SetOptionsHandle()
{
    mi_bQuerySetUpDone = false;
    return *m_OptsHandle;
}

inline const CBlastOptionsHandle&
CDbBlast::GetOptionsHandle() const
{
    return *m_OptsHandle;
}

inline const BlastMaskLoc*
CDbBlast::GetFilteredQueryRegions() const
{
    return mi_pFilteredRegions;
}

inline BlastSeqSrc* CDbBlast::GetSeqSrc() const
{
    return m_pSeqSrc;
}

inline BlastHSPResults* CDbBlast::GetResults() const
{
    return mi_pResults;
}

inline BlastReturnStat* CDbBlast::GetReturnStats() const
{
    return mi_pReturnStats;
}

inline BlastScoreBlk* CDbBlast::GetScoreBlk() const
{
    return mi_pScoreBlock;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2003/12/08 22:43:05  dondosha
* Added getters for score block and return stats structures
*
* Revision 1.3  2003/12/03 16:36:07  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.2  2003/11/26 18:36:44  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.1  2003/10/29 22:37:21  dondosha
* Database BLAST search class
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

#endif  /* ALGO_BLAST_API___DBBLAST__HPP */
