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
/// <pre>
/// Top-level API for doing a BLAST search with CDbBlast class:
/// 
/// Full search, returning Seq-align:
///     Run();
///
/// Full search, returning BlastHSPResults:
///     PartialRun(); 
///     GetResults(); 
///
/// Preliminary search only, returning BlastHSPResults:
///     SetupSearch();
///     RunPreliminarySearch();
///     GetResults();
///
/// Traceback only, returning Seq-align:
///     RunTraceback();
/// </pre>
///

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

/// Type definition for a vector of error messages from the BLAST engine
typedef vector<Blast_Message*> TBlastError;

/// Runs the BLAST algorithm between a set of sequences and BLAST database
class NCBI_XBLAST_EXPORT CDbBlast : public CObject
{
public:

    /// Contructor, creating default options for a given program
    /// @param queries Vector of query locations [in]
    /// @param seq_src Source of subject sequences [in]
    /// @param p Blast program (task) [in]
    /// @param hsp_stream Data structure for saving HSP lists. Passed in in case
    ///                   of on-the-fly formatting; otherwise 0. [in]
    /// @param num_threads How many threads to use in preliminary stage of the 
    ///                    search.
    CDbBlast(const TSeqLocVector& queries, 
             BlastSeqSrc* seq_src, EProgram p,
             BlastHSPStream* hsp_stream=0, int num_threads=1);
    /// Constructor using a prebuilt options handle
    /// @param queries Vector of query locations [in]
    /// @param seq_src Source of subject sequences [in]
    /// @param opts Options handle for the BLAST search. [in]
    /// @param hsp_stream Data structure for saving HSP lists. Passed in in case
    ///                   of on-the-fly formatting; otherwise 0. [in]
    /// @param num_threads How many threads to use in preliminary stage of the 
    ///                    search.
    CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* seq_src, 
             CBlastOptionsHandle& opts,
             BlastHSPStream* hsp_stream=0, int num_threads=1);
    /// Destructor
    virtual ~CDbBlast();
    /// Allocates and initializes internal data structures for the query 
    /// sequences.
    void SetQueries(const TSeqLocVector& queries);
    /// Returns the vector of query Seq-locs
    const TSeqLocVector& GetQueries() const;
    /// Returns the options handle for modification
    CBlastOptionsHandle& SetOptionsHandle();
    /// Returns the read only options handle
    const CBlastOptionsHandle& GetOptionsHandle() const;

    /// Perform BLAST search
    virtual TSeqAlignVector Run();
    /// Run BLAST search, without converting results to Seq-align form.
    virtual void PartialRun();
    /// Perform the main part of the search setup
    virtual void SetupSearch();
    /// Run the preliminary stage of the search engine; assumes that main
    /// setup has already been performed.
    virtual void RunPreliminarySearch();
    /// Run only the traceback stage of the BLAST search, returning results 
    /// in the Seq-align form. Assumes that preliminary search has already been
    /// done, and its results are available from the HSP stream.
    virtual TSeqAlignVector RunTraceback(); 

    /// Retrieves regions filtered on the query/queries
    // const TSeqLocVector& GetFilteredQueryRegions() const;
    const BlastMaskLoc* GetFilteredQueryRegions() const;

    /// Retrieve the data structure for the source of subject sequences
    const BlastSeqSrc* GetSeqSrc() const;
    /// Retrieve the data structure for saving and extracting HSP lists
    BlastHSPStream* GetHSPStream() const;
    /// Retrieve the results of the BLAST search in the internal form.
    BlastHSPResults* GetResults();
    /// Retrieve the diagnostics structure
    BlastDiagnostics* GetDiagnostics() const;
    /// Retrieve the scoring and statistical parameters structure
    BlastScoreBlk* GetScoreBlk() const;
    /// Retrieve the lookup table structure
    LookupTableWrap* GetLookupTable() const;
    /// Retrieve the query information structure
    const CBlastQueryInfo& GetQueryInfo() const;
    /// Retrieve the internal query sequence structure
    const CBLAST_SequenceBlk& GetQueryBlk() const;
    /// Get the error/warning message. FIXME: the DbBlast object owns the
    /// Blast_Messages in the vector and will delete them!
    TBlastError& GetErrorMessage();

protected:
    /// Perform traceback given preliminary results are already computed and 
    /// saved in an HSP stream. Fills the BlastHSPResults internal structure 
    /// with after-traceback results.
    virtual void x_RunTracebackSearch();
    /// Convert results in internal form to a Seq-align form
    virtual TSeqAlignVector x_Results2SeqAlign();
    /// Reset the query-related data structures for a new search
    virtual void x_ResetQueryDs();
    /// Reset results-related data structures for a new search
    virtual void x_ResetResultDs();
    /// Initialize the internal fields
    virtual void x_InitFields();
    /// Initialize the data structure for saving HSP lists
    virtual void x_InitHSPStream();
    /// Initialize the RPS database auxiliary structures
    virtual void x_InitRPSFields();
    /// Initialize the RPS database auxiliary structures
    virtual void x_ResetRPSFields();
    /// Internal data structures used in this and all derived classes 
    bool                m_ibQuerySetUpDone; ///< Has the query set-up been done?
    CBLAST_SequenceBlk  m_iclsQueries;    ///< one for all queries
    CBlastQueryInfo     m_iclsQueryInfo;  ///< one for all queries
    BlastScoreBlk*      m_ipScoreBlock;   ///< Karlin-Altschul parameters
    BlastDiagnostics*   m_ipDiagnostics; ///< Statistical return structures
    TBlastError         m_ivErrors;       ///< Error (info, warning) messages
    
    BlastHSPResults*    m_ipResults;      /**< Results structure - not private, 
                                             because derived class will need to
                                             set it. */

private:
    /// Data members received from client code
    TSeqLocVector        m_tQueries;      ///< query sequence(s)
    BlastSeqSrc*         m_pSeqSrc;       ///< Subject sequences sorce
    BlastHSPStream*      m_pHspStream;    /**< Placeholder for streaming HSP 
                                             lists out of the engine. */
    CRef<CBlastOptionsHandle>  m_OptsHandle; ///< Blast options

    /// Prohibit copy constructor
    CDbBlast(const CDbBlast& rhs);
    /// Prohibit assignment operator
    CDbBlast& operator=(const CDbBlast& rhs);

    /// Open and initialize RPS blast structures from disk.
    /// @param ppinfo Pointer to initialized RPS data [out]
    /// @param rps_mmap Pointer to memory-mapped lookup table file [out]
    /// @param rps_pssm_mmap Pointer to memory-mapped PSSM file [out]
    /// @param dbname Name of RPS database [in]
    static void         x_Blast_RPSInfoInit(BlastRPSInfo **ppinfo, 
                                              CMemoryFile **rps_mmap,
                                              CMemoryFile **rps_pssm_mmap,
                                              string dbname);

    /// Free RPS blast related structures.
    /// @param ppinfo Pointer to initialized RPS data [in/out]
    /// @param rps_mmap Pointer to memory-mapped lookup table file [in/out]
    /// @param rps_pssm_mmap Pointer to memory-mapped PSSM file [in/out]
    static void         x_Blast_RPSInfoFree(BlastRPSInfo **ppinfo,
                                            CMemoryFile **rps_mmap,
                                            CMemoryFile **rps_pssm_mmap);

    /************ Internal data structures (m_i = internal members)**********/
    LookupTableWrap*    m_ipLookupTable;   ///< Lookup table, one for all queries
    BlastSeqLoc*        m_ipLookupSegments; /**< Intervals for which lookup 
                                              table is created: complement of
                                              filtered regions */
    BlastMaskLoc*       m_ipFilteredRegions;///< Filtered regions
    bool                m_ibLocalResults;   /**< Are results saved locally or
                                             streamed out to the client of 
                                             this class? */
    int                 m_iNumThreads;      /**< How many threads are used in 
                                               the preliminary stage? */
    bool                m_ibTracebackOnly;/**< Should only traceback be 
                                             performed? Allows setup to recognize
                                             that lookup table need not be 
                                             created. */
    BlastRPSInfo*       m_ipRpsInfo;      ///< RPS BLAST database information
    CMemoryFile*        m_ipRpsMmap;      ///< Memory mapped RPS lookup table file
    CMemoryFile*        m_ipRpsPssmMmap;  ///< Memory mapped RPS PSSM file

    friend class ::CRPSTest;              ///< unit test class
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

inline const BlastSeqSrc* CDbBlast::GetSeqSrc() const
{
    return m_pSeqSrc;
}

inline BlastHSPStream* CDbBlast::GetHSPStream() const
{
    return m_pHspStream;
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

inline LookupTableWrap* CDbBlast::GetLookupTable() const
{
    return m_ipLookupTable;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.34  2005/03/31 13:43:49  camacho
* BLAST options API clean-up
*
* Revision 1.33  2005/01/21 15:38:44  papadopo
* changed x_Blast_FillRPSInfo to x_Blast_RPSInfo{Init|Free}
*
* Revision 1.32  2005/01/19 13:06:59  rsmith
* take out redundant and illegal class specification.
*
* Revision 1.31  2005/01/14 17:59:21  papadopo
* add FillRPSInfo as a private member, to remove some xblast dependencies on SeqDB
*
* Revision 1.30  2005/01/11 17:50:19  dondosha
* Removed TrimBlastHSPResults method: will be a static function in blastsrv4.REAL code
*
* Revision 1.29  2005/01/06 15:40:30  camacho
* Add comments to GetErrorMessage
*
* Revision 1.28  2004/12/21 17:15:56  dondosha
* GetResults method made non-const and moved to .cpp file - it can now retrieve results after preliminary stage
*
* Revision 1.27  2004/11/04 15:51:59  papadopo
* prepend 'Blast' to RPSInfo and related structures
*
* Revision 1.26  2004/10/26 15:30:26  dondosha
* Removed RPSInfo argument from constructors;
* RPSInfo is now initialized inside the CDbBlast class if RPS search is requested;
* multiple queries are now allowed for RPS search.
*
* Revision 1.25  2004/10/06 14:49:40  dondosha
* Added RunTraceback method to perform a traceback only search, given the precomputed preliminary results, and return Seq-align; removed unused SetSeqSrc method; added doxygen comments
*
* Revision 1.24  2004/09/13 12:44:11  madden
* Use BlastSeqLoc rather than ListNode
*
* Revision 1.23  2004/09/07 17:59:12  dondosha
* CDbBlast class changed to support multi-threaded search
*
* Revision 1.22  2004/07/06 15:47:11  dondosha
* Made changes in preparation for implementation of multi-threaded search; added doxygen comments
*
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
