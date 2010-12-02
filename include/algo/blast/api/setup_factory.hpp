/* $Id$
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
 * Author: Christiam Camacho, Kevin Bealer
 *
 */

/** @file setup_factory.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___SETUP_FACTORY_HPP
#define ALGO_BLAST_API___SETUP_FACTORY_HPP

#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/rps_aux.hpp>           // for CBlastRPSInfo
#include <algo/blast/api/blast_dbindex.hpp>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/pattern.h>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Forward declations
class CBlastOptionsMemento;
class CSearchDatabase;

// -- RATIONALE: --
//
// This is a wrapper for a (C language) struct pointer, providing
// optional-at-runtime deletion / ownership semantics plus sharing.
// The simplest way to explain what it is for is to explain why the
// other smart pointer classes were not used or not used directly.
//
// CObject/CRef: These require the base object to be a CObject.
// Because of our requirement of continuing to work with a mixture of
// C and C++, we cannot make these particular structs into CObjects.
//
// auto_ptr and AutoPtr: One of the requirements is simultaneous
// ownership -- these classes cannot do this.
//
// CObjectFor: This does not provide configurable deletion, cannot
// control deletion at runtime, and copies data by value.
//
// DECLARE_AUTO_CLASS_WRAPPER: This lacks sharing semantics.  It is
// also a macro, and requires more work to use than CStructWrapper.
//
// Combining two of these versions: .... would probably work.  For
// example, something like CObjectFor< AutoPtr<T> > is almost good
// enough, but wrapping it to provide the optional deletion semantics
// would result in code the same size as that below.


// CStructWrapper
//
// This template wraps a C or C++ object in a CObject.  A deletion
// function can be provided to the constructor, and if so, will be
// used to delete the object.  The signature must be "T* D(T *)".
// 
// CStructWrapper<T>(T *, TDelete * d)  -> Uses "d(x)"
// CStructWrapper<T>(T *, 0)            -> Non-deleting version
//

template<class TData>
class CStructWrapper : public CObject {
public:
    /// type definition for a function that deallocated memory associated with
    /// an object.
    /// This functions return value is ignored; it would be void,
    /// except that most existing deletion functions return "NULL".
    typedef TData* (TDelete)(TData*);
    
    /// Constructor
    /// @param obj object to wrap [in]
    /// @param dfun deallocation function for object above [in]
    CStructWrapper(TData * obj, TDelete * dfun)
        : m_Data(obj), m_DeleteFunction(dfun)
    {
    }
    
    /// Destructor
    ~CStructWrapper()
    {
        if (m_Data && m_DeleteFunction) {
            m_DeleteFunction(m_Data);
        }
        m_Data = NULL;
    }
    
    /// The a pointer to the wrapped object
    TData * GetPointer()
    {
        return m_Data;
    }
    
    /// The a reference to the wrapped object
    TData & operator*()
    {
        return *m_Data;
    }
    
    /// The a pointer to the wrapped object
    TData * operator->()
    {
        return m_Data;
    }
    
private:
    /// Prohibit copy constructor
    CStructWrapper(CStructWrapper<TData> & x);
    /// Prohibit assignment operator
    CStructWrapper & operator=(CStructWrapper<TData> & x);
    
    /// the pointer managed by this object
    TData   * m_Data;
    /// deallocation function for the pointer above
    TDelete * m_DeleteFunction;
};


/// Auxiliary function to create a CStructWrapper for a pointer to an object
/// @param obj pointer to wrap [in]
/// @param del deallocation function [in]
template<class TData>
CStructWrapper<TData> *
WrapStruct(TData * obj, TData* (*del)(TData*))
{
    return new CStructWrapper<TData>(obj, del);
}

/// Class that supports setting the number of threads to use with a given
/// algorithm. Ensures that this number is greater than or equal to 1.
class NCBI_XBLAST_EXPORT CThreadable
{
public:
    /// Never have less than 1 thread
    enum { kMinNumThreads = 1 };

    /// Default ctor
    CThreadable(void) : m_NumThreads(kMinNumThreads) {}
    /// Our virtual destructor
    virtual ~CThreadable(void) {}
    /// Mutator for the number of threads
    /// @param nthreads number of threads to use
    virtual void SetNumberOfThreads(size_t nthreads);
    /// Accessor for the number of threads to use
    size_t GetNumberOfThreads(void) const;
    /// Returns true if more than 1 thread is specified
    bool IsMultiThreaded(void) const;

protected:
    size_t m_NumThreads;    ///< Keep track of how many threads should be used
};


/// Auxiliary class to create the various C structures to set up the
/// preliminary and/or traceback stages of the search.
// Topological sort for calling these routines (after setting up queries):
// 1. RPS (if any)
// 2. ScoreBlk
// 3. LookupTable
// 4. diags, hspstream
class NCBI_XBLAST_EXPORT CSetupFactory {
public:
    /// Initializes RPS-BLAST data structures
    /// @param rps_dbname Name of the RPS-BLAST database [in]
    /// @param options BLAST options (matrix name and gap costs will be
    /// modified with data read from the RPS-BLAST auxiliary file) [in|out]
    static CRef<CBlastRPSInfo> 
    CreateRpsStructures(const string& rps_dbname, CRef<CBlastOptions> options);

    /// Initializes the BlastScoreBlk. Caller owns the return value.
    /// @param opts_memento Memento options object [in]
    /// @param query_data source of query sequence data [in]
    /// @param lookup_segments query segments to be searched because they were
    /// not filtered, needed for the lookup table creation (otherwise pass
    /// NULL). If this is passed to this function it should also be passed to
    /// CreateLookupTable [in|out]
    /// @param search_messages Error/warning messages [in|out]
    /// @param masked_query_regions Regions of the query which were masked
    /// including those masked outside the CORE. If non-NULL they will be
    /// populated and caller assumes ownership of the object [in|out]
    /// @param rps_info RPS-BLAST data structures as obtained from
    /// CreateRpsStructures [in]
    /// @todo need to convert the lookup_segments to some kind of c++ object
    static BlastScoreBlk* 
    CreateScoreBlock(const CBlastOptionsMemento* opts_memento, 
                     CRef<ILocalQueryData> query_data, 
                     BlastSeqLoc** lookup_segments, 
                     TSearchMessages& search_messages,
                     TSeqLocInfoVector* masked_query_regions = NULL,
                     const CBlastRPSInfo* rps_info = NULL);

    /// Initialize the lookup table. Note that for the case of PSI-BLAST the
    /// PSSM must be initialized in the BlastScoreBlk for it to be recognized
    /// properly by the lookup table code. Caller owns the return value.
    /// @param query_data source of query sequence data [in]
    /// @param opts_memento Memento options object [in]
    /// @param score_blk BlastScoreBlk structure, as obtained in
    /// CreateScoreBlock [in]
    /// @param lookup_segments query segments to be searched because they were
    /// not filtered, needed for the lookup table creation (otherwise pass
    /// NULL) [in|out]
    /// @todo need to convert the lookup_segments to some kind of c++ object
    /// @param rps_info RPS-BLAST data structures as obtained from
    /// CreateRpsStructures [in]
    /// @param seqsrc BlastSeqSrc structure, only needed when performing
    /// megablast indexed-database searches [in]
    static LookupTableWrap*
    CreateLookupTable(CRef<ILocalQueryData> query_data,
                      const CBlastOptionsMemento* opts_memento,
                      BlastScoreBlk* score_blk,
                      CRef< CBlastSeqLocWrap > lookup_segments,
                      const CBlastRPSInfo* rps_info = NULL,
                      BlastSeqSrc* seqsrc = NULL);

    /// Create and initialize the BlastDiagnostics structure for 
    /// single-threaded applications
    static BlastDiagnostics* CreateDiagnosticsStructure();

    /// Create and initialize the BlastDiagnostics structure for 
    /// multi-threaded applications
    static BlastDiagnostics* CreateDiagnosticsStructureMT();

    /// Create and initialize the BlastHSPStream structure 
    /// @param opts_memento Memento options object [in]
    /// @param number_of_queries number of queries involved in the search [in]
    /// @param writer writer to be used within this stream [in]
    static BlastHSPStream* 
    CreateHspStream(const CBlastOptionsMemento* opts_memento, 
                    size_t number_of_queries,
                    BlastHSPWriter *writer);

    /// Create a writer to be registered for use by stream
    /// @param opts_memento Memento options object [in]
    /// @param query_info Information about queries [in]
    static BlastHSPWriter* 
    CreateHspWriter(const CBlastOptionsMemento* opts_memento,
                    BlastQueryInfo* query_info);

    /// Create a pipe to be registered for use by stream
    /// @param opts_memento Memento options object [in]
    /// @param query_info Information about queries [in]
    static BlastHSPPipe* 
    CreateHspPipe(const CBlastOptionsMemento* opts_memento,
                    BlastQueryInfo* query_info);

    /// Create a BlastSeqSrc from a CSearchDatabase (uses CSeqDB)
    /// @param db description of BLAST database to search [in]
    static BlastSeqSrc*
    CreateBlastSeqSrc(const CSearchDatabase& db);
    
    /// Create a BlastSeqSrc from an existing CSeqDB object
    /// @param db Existing CSeqDB object for the searched BLAST database [in]
    static BlastSeqSrc*
    CreateBlastSeqSrc(CSeqDB * db, int filt_algo = -1, int mask_type = DB_MASK_NONE);

    /// Initialize a megablast BLAST database index
    /// @param seqsrc The source of subject sequence data [in]
    /// @param options BLAST options (will be modified to record the fact that
    /// the database index has been initialized [in|out]
    static void
    InitializeMegablastDbIndex(BlastSeqSrc* seqsrc,
                               CRef<CBlastOptions> options);
    
};

#ifndef SKIP_DOXYGEN_PROCESSING
typedef CStructWrapper<BlastScoreBlk>           TBlastScoreBlk;
typedef CStructWrapper<LookupTableWrap>         TLookupTableWrap;
typedef CStructWrapper<BlastDiagnostics>        TBlastDiagnostics;
typedef CStructWrapper<BlastHSPStream>          TBlastHSPStream;
typedef CStructWrapper<BlastSeqSrc>             TBlastSeqSrc;
typedef CStructWrapper<SPHIPatternSearchBlk>    TSPHIPatternSearchBlk;

#endif /* SKIP_DOXYGEN_PROCESSING */

/// Lightweight wrapper to enclose C structures needed for running the
/// preliminary and traceback stages of the BLAST search
struct NCBI_XBLAST_EXPORT SInternalData : public CObject
{
    /// Default ctor
    SInternalData();

    /// The query sequence data, these fields are "borrowed" from the query
    /// factory (which owns them)
    BLAST_SequenceBlk* m_Queries;
    /// The query information structure
    BlastQueryInfo* m_QueryInfo;

    /// BLAST score block structure
    CRef<TBlastScoreBlk> m_ScoreBlk;

    /// Lookup table, usually only needed in the preliminary stage of the
    /// search, but for PHI-BLAST it's also needed in the traceback stage.
    CRef<TLookupTableWrap> m_LookupTable;   

    /// Diagnostic output from preliminary and traceback stages
    CRef<TBlastDiagnostics> m_Diagnostics;  

    /// HSP output of the preliminary stage goes here
    CRef<TBlastHSPStream> m_HspStream;

    /// The source of subject sequence data
    CRef<TBlastSeqSrc> m_SeqSrc;

    /// The RPS-BLAST related data
    CRef<CBlastRPSInfo> m_RpsData;

    /// The interrupt callback
    TInterruptFnPtr m_FnInterrupt;

    /// The user data structure to aid in progress monitoring
    CRef<CSBlastProgress> m_ProgressMonitor;
};

/// Structure to hold results of the preliminary (databases scanning phase)
/// part of the search that are needed for the traceback.
/// Generally this structure will be used if the preliminary and traceback parts
/// are done as separate processes (or even machines).
struct NCBI_XBLAST_EXPORT SDatabaseScanData : public CObject
{
    /// Default ctor
    SDatabaseScanData();

    /// set to -1 in ctor, indicate that m_NumPatOccurInDB is unset or not applicable.
    const int kNoPhiBlastPattern;

    /// Number of times pattern found to occur in database (for phi-blast only).
    int m_NumPatOccurInDB; 
};

inline void
CThreadable::SetNumberOfThreads(size_t nthreads)
{
    m_NumThreads = nthreads == 0 ? kMinNumThreads : nthreads;
}

inline size_t
CThreadable::GetNumberOfThreads(void) const
{
    ASSERT(m_NumThreads >= kMinNumThreads);
    return m_NumThreads;
}

inline bool
CThreadable::IsMultiThreaded(void) const
{
    return m_NumThreads > kMinNumThreads;
}

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___SETUP_FACTORY__HPP */

