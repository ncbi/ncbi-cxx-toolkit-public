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
#include <algo/blast/core/blast_engine.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// Type definition for a vector of error messages from the BLAST engine
typedef vector<Blast_Message*> TBlastError;

/// Runs the BLAST algorithm between a set of sequences and BLAST database
class NCBI_XBLAST_EXPORT CDbBlast : public CObject
{
public:

    /// Contructor, creating default options for a given program
    CDbBlast(const TSeqLocVector& queries, 
             BlastSeqSrc* bssp, EProgram p);
    // Constructor using a prebuilt options handle
    CDbBlast(const TSeqLocVector& queries, 
             BlastSeqSrc* bssp, CBlastOptionsHandle& opts);

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
    
    // Remove extra results if a limit is provided on total number of HSPs
    void TrimBlastHSPResults();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const BlastMaskLoc* GetFilteredQueryRegions() const;

    BlastSeqSrc* GetSeqSrc() const;
    BlastHSPResults* GetResults() const;
    BlastReturnStat* GetReturnStats() const;
    BlastScoreBlk* GetScoreBlk() const;
    const CBlastQueryInfo& GetQueryInfo() const;
    TBlastError& GetErrorMessage();

protected:
    virtual int SetupSearch();
    virtual void RunSearchEngine();
    virtual TSeqAlignVector x_Results2SeqAlign();
    virtual void x_ResetQueryDs();

    BlastScoringOptions* GetScoringOpts() const;
    BlastEffectiveLengthsOptions* GetEffLenOpts() const;
    BlastExtensionOptions * GetExtnOpts() const;
    BlastHitSavingOptions * GetHitSaveOpts() const;
    QuerySetUpOptions * GetQueryOpts() const;
    BlastDatabaseOptions * GetDbOpts() const;
    PSIBlastOptions * CDbBlast::GetProtOpts() const;    

    /// Internal data structures used in this and all derived classes 
    bool                mi_bQuerySetUpDone;
    CBLAST_SequenceBlk  mi_clsQueries;  // one for all queries
    CBlastQueryInfo     mi_clsQueryInfo; // one for all queries
    BlastScoreBlk*      mi_pScoreBlock; // Karlin-Altschul parameters
    /// Statistical return structures
    BlastReturnStat*    mi_pReturnStats;
    /// Error (info, warning) messages
    TBlastError         mi_vErrors;
   
    /// Results structure - not private, because derived class will need to
    /// set it
    BlastHSPResults*    mi_pResults;

private:
    // Data members received from client code
    TSeqLocVector        m_tQueries;         //< query sequence(s)
    BlastSeqSrc*         m_pSeqSrc;          //< Subject sequences sorce
    CRef<CBlastOptionsHandle>  m_OptsHandle; //< Blast options

    /// Prohibit copy constructor
    CDbBlast(const CDbBlast& rhs);
    /// Prohibit assignment operator
    CDbBlast& operator=(const CDbBlast& rhs);

    /************ Internal data structures (m_i = internal members)**********/
    LookupTableWrap*    mi_pLookupTable; // one for all queries
    ListNode*           mi_pLookupSegments; /* Intervals for which lookup 
                                               table is created: complement of
                                               filtered regions */
    BlastMaskLoc*       mi_pFilteredRegions; // Filtered regions

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

inline const CBlastQueryInfo& CDbBlast::GetQueryInfo() const
{
    return mi_clsQueryInfo;

}

inline TBlastError& CDbBlast::GetErrorMessage()
{
    return mi_vErrors;
}

inline BlastScoringOptions* CDbBlast::GetScoringOpts() const
{
    return m_OptsHandle->GetOptions().GetScoringOpts();
}
inline BlastEffectiveLengthsOptions* CDbBlast::GetEffLenOpts() const
{
    return m_OptsHandle->GetOptions().GetEffLenOpts();
}
inline BlastExtensionOptions * CDbBlast::GetExtnOpts() const
{
    return m_OptsHandle->GetOptions().GetExtnOpts();
}
inline BlastHitSavingOptions * CDbBlast::GetHitSaveOpts() const
{
    return m_OptsHandle->GetOptions().GetHitSaveOpts();
}
inline QuerySetUpOptions * CDbBlast::GetQueryOpts() const
{
    return m_OptsHandle->GetOptions().GetQueryOpts();
}
inline BlastDatabaseOptions * CDbBlast::GetDbOpts() const
{
    return m_OptsHandle->GetOptions().GetDbOpts();
}
inline PSIBlastOptions * CDbBlast::GetProtOpts() const
{
    return m_OptsHandle->GetOptions().GetProtOpts();
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2004/02/25 15:44:47  dondosha
* Corrected prototype for GetErrorMessage to eliminate warning on Sun compiler
*
* Revision 1.10  2004/02/24 20:38:20  dondosha
* Removed irrelevant CVS log comments
*
* Revision 1.9  2004/02/24 18:18:54  dondosha
* Made some private variables and methods protected - needed for derived class; added getters for various options
*
* Revision 1.8  2004/02/19 21:10:25  dondosha
* Added vector of error messages to the CDbBlast class
*
* Revision 1.7  2004/02/18 23:48:45  dondosha
* Added TrimBlastHSPResults method to remove extra HSPs if limit on total number is provided
*
* Revision 1.6  2003/12/15 15:52:29  dondosha
* Added constructor with options handle argument
*
* Revision 1.5  2003/12/10 20:08:59  dondosha
* Added function to retrieve the query info structure
*
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
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___DBBLAST__HPP */
