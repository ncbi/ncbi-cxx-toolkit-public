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
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/pattern.h>

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
    // This functions return value is ignored; it would be void,
    // except that most existing deletion functions return "NULL".
    
    typedef TData* (TDelete)(TData*);
    
    CStructWrapper(TData * obj, TDelete * dfun)
        : m_Data(obj), m_DeleteFunction(dfun)
    {
    }
    
    ~CStructWrapper()
    {
        if (m_Data && m_DeleteFunction) {
            m_DeleteFunction(m_Data);
        }
    }
    
    TData * GetPointer()
    {
        return m_Data;
    }
    
    TData & operator*()
    {
        return *m_Data;
    }
    
    TData * operator->()
    {
        return m_Data;
    }
    
private:
    CStructWrapper(CStructWrapper<TData> & x);
    CStructWrapper & operator=(CStructWrapper<TData> & x);
    
    TData   * m_Data;
    TDelete * m_DeleteFunction;
};


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
    /// Never have less than 1 thread
    enum { kMinNumThreads = 1 };

public:
    CThreadable(void) : m_NumThreads(kMinNumThreads) {}
    virtual ~CThreadable(void) {}
    void SetNumberOfThreads(size_t nthreads);
    size_t GetNumberOfThreads(void) const;
    bool IsMultiThreaded(void) const;

protected:
    size_t m_NumThreads;
};


/// Auxiliary class to create the various C structures to set up the
/// preliminary and/or traceback stages of the search.
// Topological sort for calling these routines (after setting up queries):
// 1. RPS (if any)
// 2. ScoreBlk
// 3. LookupTable
// 4. diags, hspstream
class CSetupFactory {
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
    /// NULL) [in|out]
    /// @param rps_info RPS-BLAST data structures as obtained from
    /// CreateRpsStructures [in]
    /// @todo need to convert the lookup_segments to some kind of c++ object
    static BlastScoreBlk* 
    CreateScoreBlock(const CBlastOptionsMemento* opts_memento, 
                     CRef<ILocalQueryData> query_data, 
                     BlastSeqLoc** lookup_segments, 
                     const CBlastRPSInfo* rps_info = NULL);

    /// Initialize the lookup table. Caller owns the return value.
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
    static LookupTableWrap*
    CreateLookupTable(CRef<ILocalQueryData> query_data,
                      const CBlastOptionsMemento* opts_memento,
                      BlastScoreBlk* score_blk,
                      BlastSeqLoc* lookup_segments,
                      const CBlastRPSInfo* rps_info = NULL);

    /// Create and initialize the BlastDiagnostics structure for 
    /// single-threaded applications
    static BlastDiagnostics* CreateDiagnosticsStructure();

    /// Create and initialize the BlastDiagnostics structure for 
    /// multi-threaded applications
    static BlastDiagnostics* CreateDiagnosticsStructureMT();

    /// Create and initialize the BlastHSPStream structure for single-threaded
    /// applications
    /// @param opts_memento Memento options object [in]
    /// @param number_of_queries number of queries involved in the search [in]
    static BlastHSPStream* 
    CreateHspStream(const CBlastOptionsMemento* opts_memento, 
                    size_t number_of_queries);

    /// Create and initialize the BlastHSPStream structure for multi-threaded
    /// applications
    /// @param opts_memento Memento options object [in]
    /// @param number_of_queries number of queries involved in the search [in]
    static BlastHSPStream* 
    CreateHspStreamMT(const CBlastOptionsMemento* opts_memento, 
                      size_t number_of_queries);

    /// Create a BlastSeqSrc from a CSearchDatabase (uses CSeqDB)
    /// @param opts_memento Memento options object [in]
    /// @param number_of_queries number of queries involved in the search [in]
    /// @param is_multi_threaded true in case of multi-threaded search, else
    /// false [in]
    static BlastSeqSrc*
    CreateBlastSeqSrc(const CSearchDatabase& db);

private:
    /// Auxiliary function to create the BlastHSPStream structure
    /// @param 
    static BlastHSPStream*
    x_CreateHspStream(const CBlastOptionsMemento* opts_memento,
                      size_t number_of_queries,
                      bool is_multi_threaded);
};

typedef CStructWrapper<BlastScoreBlk>           TBlastScoreBlk;
typedef CStructWrapper<LookupTableWrap>         TLookupTableWrap;
typedef CStructWrapper<BlastDiagnostics>        TBlastDiagnostics;
typedef CStructWrapper<BlastHSPStream>          TBlastHSPStream;
typedef CStructWrapper<BlastSeqSrc>             TBlastSeqSrc;
typedef CStructWrapper<SPHIPatternSearchBlk>    TSPHIPatternSearchBlk;

/// Lightweight wrapper to enclose C structures needed for running the
/// preliminary and traceback stages of the BLAST search
struct SInternalData : public CObject
{
    SInternalData();

    // The query sequence data, these fields are "borrowed" from the query
    // factory (which owns them)
    BLAST_SequenceBlk* m_Queries;
    BlastQueryInfo* m_QueryInfo;

    CRef<TBlastScoreBlk> m_ScoreBlk;

    // in the traceback stage, only needed for PHI-BLAST needs this field to be
    // populated
    CRef<TLookupTableWrap> m_LookupTable;   

    // output from preliminary and traceback stages
    CRef<TBlastDiagnostics> m_Diagnostics;  

    // Output of the preliminary stage goes here
    CRef<TBlastHSPStream> m_HspStream;

    // The source of subject sequence data
    CRef<TBlastSeqSrc> m_SeqSrc;

    // The RPS-BLAST related data
    CRef<CBlastRPSInfo> m_RpsData;
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

