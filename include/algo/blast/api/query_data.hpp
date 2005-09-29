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

/** @file query_data.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___BLAST_QUERY_DATA_HPP
#define ALGO_BLAST_API___BLAST_QUERY_DATA_HPP

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_def.h>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)

/// Provides access (not ownership) to the C structures used to configure
/// local BLAST search class implementations
class NCBI_XBLAST_EXPORT ILocalQueryData : public CObject
{
public:
    virtual ~ILocalQueryData() {}

    /// Accessor for the BLAST_SequenceBlk structure
    virtual BLAST_SequenceBlk* GetSequenceBlk() = 0;
    
    /// Accessor for the BlastQueryInfo structure
    virtual BlastQueryInfo* GetQueryInfo() = 0;
    
    
    /// Get the number of queries.
    virtual int GetNumQueries() = 0;
    
    /// Get the Seq_loc for the sequence indicated by index.
    virtual CConstRef<objects::CSeq_loc> GetSeq_loc(int index) = 0;
    
    /// Get the length of the sequence indicated by index.
    virtual int GetSeqLength(int index) = 0;
    
protected:
    /// Data member to cache the BLAST_SequenceBlk
    CBLAST_SequenceBlk m_SeqBlk;
    /// Data member to cache the BlastQueryInfo
    CBlastQueryInfo m_QueryInfo;
};

class IRemoteQueryData : public CObject
{
public:
    virtual ~IRemoteQueryData() {}

    /// Accessor for the CBioseq_set
    virtual CRef<objects::CBioseq_set> GetBioseqSet() = 0;
    /// Type definition for CSeq_loc set used as queries in the BLAST remote 
    /// search class
    typedef list< CRef<objects::CSeq_loc> > TSeqLocs;
    /// Accessor for the TSeqLocs
    virtual TSeqLocs GetSeqLocs() = 0;

protected:
    /// Data member to cache the CBioseq_set
    CRef<objects::CBioseq_set> m_Bioseqs;
    /// Data member to cache the TSeqLocs
    TSeqLocs                   m_SeqLocs;
};

// forward declaration needed by IQueryFactory
class CBlastOptions;

/// Source of query sequence data for BLAST
/// Provides an interface for search classes to retrieve sequence data to be
/// used in local/remote searches without coupling them to the actual means 
/// of retrieving the data.
///
/// Its subclasses define which types of inputs can be converted into ILocalQ
/// @note Because this class caches the result of the Make*QueryData methods,
/// the products created by this factory will have the same lifespan as 
/// the factory.
class NCBI_XBLAST_EXPORT IQueryFactory : public CObject
{
public:
    virtual ~IQueryFactory() {}

    /// Creates and caches an ILocalQueryData
    CRef<ILocalQueryData> MakeLocalQueryData(const CBlastOptions* opts);

    /// Creates and caches an IRemoteQueryData
    CRef<IRemoteQueryData> MakeRemoteQueryData();

protected:
    CRef<ILocalQueryData> m_LocalQueryData;
    CRef<IRemoteQueryData> m_RemoteQueryData;

    /// factory method to create an ILocalQueryData, only called if the data
    /// members above are not set
    virtual CRef<ILocalQueryData> 
    x_MakeLocalQueryData(const CBlastOptions* opts) = 0;

    /// factory method to create an IRemoteQueryData, only called if the data
    /// members above are not set
    virtual CRef<IRemoteQueryData> x_MakeRemoteQueryData() = 0;
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___BLAST_QUERY_DATA__HPP */
