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
*/

/// @file db_blast.hpp
/// Declaration of the database BLAST class CDbBlast

#ifndef ALGO_BLAST_API___DBBLAST__HPP
#define ALGO_BLAST_API___DBBLAST__HPP

#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_engine.h>
#include <connect/ncbi_core.h>

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

// Type definition for a vector of error messages from the BLAST engine
typedef vector<Blast_Message*> TBlastError;

/// Runs the BLAST algorithm between a set of sequences and BLAST database
class NCBI_XBLAST_EXPORT CDbBlast : public CObject
{
public:

    /// Contructor, creating default options for a given program
    CDbBlast(const TSeqLocVector& queries, 
             BlastSeqSrc* bssp, EProgram p, RPSInfo* rps_info=0,
             BlastHSPStream* hsp_stream=0, MT_LOCK lock=0);
    // Constructor using a prebuilt options handle
    CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* bssp, 
             CBlastOptionsHandle& opts, RPSInfo* rps_info=0,
             BlastHSPStream* hsp_stream=0, MT_LOCK lock=0);

    virtual ~CDbBlast();

    void SetQueries(const TSeqLocVector& queries);
    const TSeqLocVector& GetQueries() const;

    CBlastOptions& SetOptions();
    const CBlastOptions& GetOptions() const;

    CBlastOptionsHandle& SetOptionsHandle();
    const CBlastOptionsHandle& GetOptionsHandle() const;

    // Perform BLAST search
    virtual TSeqAlignVector Run();
    // Run BLAST search without traceback
    virtual void PartialRun();
    // Just perform the search setup
    virtual int SetupSearch();
    // Run the search engine; assumes that setup has already been performed
    virtual void RunSearchEngine();
    
    // Remove extra results if a limit is provided on total number of HSPs
    void TrimBlastHSPResults();

    /// Retrieves regions filtered on the query/queries
    //const TSeqLocVector& GetFilteredQueryRegions() const;
    const BlastMaskLoc* GetFilteredQueryRegions() const;

    void SetSeqSrc(BlastSeqSrc* seq_src, bool free_old_src=false);
    BlastSeqSrc* GetSeqSrc() const;
    BlastHSPStream* GetHSPStream() const;
    BlastHSPResults* GetResults() const;
    BlastDiagnostics* GetDiagnostics() const;
    BlastScoreBlk* GetScoreBlk() const;
    const CBlastQueryInfo& GetQueryInfo() const;
    const CBLAST_SequenceBlk& GetQueryBlk() const;
    TBlastError& GetErrorMessage();

protected:
    virtual TSeqAlignVector x_Results2SeqAlign();
    virtual void x_ResetQueryDs();
    virtual void x_ResetResultDs();
    virtual void x_InitFields();
    RPSInfo * GetRPSInfo() const;    

    /// Internal data structures used in this and all derived classes 
    bool                m_ibQuerySetUpDone; ///< Has the query set-up been done?
    CBLAST_SequenceBlk  m_iclsQueries;    ///< one for all queries
    CBlastQueryInfo     m_iclsQueryInfo;  ///< one for all queries
    BlastScoreBlk*      m_ipScoreBlock;   ///< Karlin-Altschul parameters
    BlastDiagnostics*    m_ipDiagnostics; ///< Statistical return structures
    TBlastError         m_ivErrors;       ///< Error (info, warning) messages
   
    BlastHSPResults*    m_ipResults;      /**< Results structure - not private, 
                                             because derived class will need to
                                             set it. */
private:
    /// Data members received from client code
    TSeqLocVector        m_tQueries;      ///< query sequence(s)
    BlastSeqSrc*         m_pSeqSrc;       ///< Subject sequences sorce
    RPSInfo*             m_pRpsInfo;      ///< RPS BLAST database information
    BlastHSPStream*      m_pHspStream;    /**< Placeholder for streaming HSP 
                                             lists out of the engine. */
    MT_LOCK              m_pLock;         /**< Locking mechanism for writing
                                             results. */

    CRef<CBlastOptionsHandle>  m_OptsHandle; ///< Blast options

    /// Prohibit copy constructor
    CDbBlast(const CDbBlast& rhs);
    /// Prohibit assignment operator
    CDbBlast& operator=(const CDbBlast& rhs);

    /************ Internal data structures (m_i = internal members)**********/
    LookupTableWrap*    m_ipLookupTable;   ///< Lookup table, one for all queries
    ListNode*           m_ipLookupSegments;/**< Intervals for which lookup 
                                              table is created: complement of
                                              filtered regions */
    BlastMaskLoc*       m_ipFilteredRegions;///< Filtered regions
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
    m_ibQuerySetUpDone = false;
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
    m_ibQuerySetUpDone = false;
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
    return m_ipFilteredRegions;
}

inline void CDbBlast::SetSeqSrc(BlastSeqSrc* seq_src, bool free_old_src)
{
    x_ResetResultDs();
    if (free_old_src)
        BlastSeqSrcFree(m_pSeqSrc);
    m_pSeqSrc = seq_src;
}

inline BlastSeqSrc* CDbBlast::GetSeqSrc() const
{
    return m_pSeqSrc;
}

inline BlastHSPStream* CDbBlast::GetHSPStream() const
{
    return m_pHspStream;
}

inline BlastHSPResults* CDbBlast::GetResults() const
{
    return m_ipResults;
}

inline BlastDiagnostics* CDbBlast::GetDiagnostics() const
{
    return m_ipDiagnostics;
}

inline BlastScoreBlk* CDbBlast::GetScoreBlk() const
{
    return m_ipScoreBlock;
}

inline const CBlastQueryInfo& CDbBlast::GetQueryInfo() const
{
    return m_iclsQueryInfo;

}

inline const CBLAST_SequenceBlk& CDbBlast::GetQueryBlk() const
{
    return m_iclsQueries;

}

inline TBlastError& CDbBlast::GetErrorMessage()
{
    return m_ivErrors;
}

inline RPSInfo * CDbBlast::GetRPSInfo() const
{
    return m_pRpsInfo;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.21  2004/06/24 15:55:16  dondosha
* Added doxygen file description
*
* Revision 1.20  2004/06/23 14:02:36  dondosha
* Added MT_LOCK argument with default 0 value in constructors
*
* Revision 1.19  2004/06/15 18:45:51  dondosha
* 1. Added optional BlastHSPStream argument to constructors;
* 2. Made SetupSearch and RunSearchEngine methods public;
* 3. Added getter methods for HSP stream and results;
* 4. Removed getter methods for individual options.
*
* Revision 1.18  2004/05/17 18:07:19  bealer
* - Add PSI Blast support.
*
* Revision 1.17  2004/05/14 17:15:59  dondosha
* BlastReturnStat structure changed to BlastDiagnostics and refactored
*
* Revision 1.16  2004/05/07 15:39:23  papadopo
* add getter for the RPSInfo private member, since the scale factor is now explicitly needed in the implementation of this class
*
* Revision 1.15  2004/05/05 15:28:31  dondosha
* Added SetSeqSrc method
*
* Revision 1.14  2004/05/04 14:05:30  dondosha
* Removed extra unimplemented SetOptions method
*
* Revision 1.13  2004/03/16 23:29:55  dondosha
* Added optional RPSInfo* argument to constructors; added function x_InitFields; changed mi_ to m_i in member field names
*
* Revision 1.12  2004/02/27 15:42:18  rsmith
* No class specifiers inside that class's declaration
*
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
