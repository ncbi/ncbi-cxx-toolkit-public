
/* ===========================================================================
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
 * Author:  Christiam Camacho, Kevin Bealer
 *
 */

/** @file objmgr_query_data.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <objmgr/util/sequence.hpp>
#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"

#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include "blast_seqalign.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/////////////////////////////////////////////////////////////////////////////

/// Produces a BioseqSet from a CBlastQueryVector
/// @param queries queries as a CBlastQueryVector
/// @retval CRef to BioseqSet
static CRef<CBioseq_set>
s_QueryVectorToBioseqSet(const CBlastQueryVector & queries)
{
    list< CRef<CSeq_entry> > se_list;
    
    for(size_t i = 0; i < queries.Size(); i++) {
        CScope & scope = *queries.GetScope(i);

        const CBioseq * cbs = 
            scope.GetBioseqHandle(*queries.GetQuerySeqLoc(i)).GetBioseqCore();
        
        CRef<CBioseq> bs(const_cast<CBioseq*>(cbs));
        
        CRef<CSeq_entry> se(new CSeq_entry);
        se->SetSeq(*bs);
        
        se_list.push_back(se);
    }
    
    CRef<CBioseq_set> rv;
    if ( !se_list.empty() ) {
        rv.Reset(new CBioseq_set);
        rv->SetSeq_set().swap(se_list);
    }
    
    return rv;
}

/// Produces a BioseqSet from a TSeqLocVector
/// @param queries queries as a TSeqLocVector
/// @retval Cref to BioseqSet
static CRef<CBioseq_set>
s_TSeqLocVectorToBioseqSet(const TSeqLocVector* queries)
{
    list< CRef<CSeq_entry> > se_list;
    
    ITERATE(TSeqLocVector, query, *queries) {
        if ( !query->seqloc->GetId() ) {
            continue;
        }
        const CBioseq * cbs = 
            query->scope->GetBioseqHandle(*query->seqloc->GetId()).GetBioseqCore();
        
        CRef<CSeq_entry> se(new CSeq_entry);
        se->SetSeq(*const_cast<CBioseq*>(cbs));
        
        se_list.push_back(se);
    }
    
    CRef<CBioseq_set> rv(new CBioseq_set);
    rv->SetSeq_set().swap(se_list);
    
    return rv;
}

/// Produces a vector of SeqLocs from a TSeqLocVector
/// @param queries queries as a TSeqLocVector
/// @retval vector of SeqLocs.
static IRemoteQueryData::TSeqLocs
s_TSeqLocVectorToTSeqLocs(const TSeqLocVector* queries)
{
    IRemoteQueryData::TSeqLocs retval;
    
    ITERATE(TSeqLocVector, query, *queries) {
        CRef<CSeq_loc> sl(const_cast<CSeq_loc *>(&* query->seqloc));
        retval.push_back(sl);
    }
    
    return retval;
}

/// Produces a vector of SeqLocs from a CBlastQueryVector
/// @param queries queries as a CBlastQueryVector
/// @retval vector of SeqLocs.
static IRemoteQueryData::TSeqLocs
s_QueryVectorToTSeqLocs(const CBlastQueryVector & queries)
{
    IRemoteQueryData::TSeqLocs retval;
    
    for(size_t i = 0; i < queries.Size(); i++) {
        CSeq_loc * slp =
            const_cast<CSeq_loc *>(&* queries.GetQuerySeqLoc(i));
        
        retval.push_back(CRef<CSeq_loc>(slp));
    }
    
    return retval;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgr_LocalQueryData
//
/////////////////////////////////////////////////////////////////////////////

/// Provides access (not ownership) to the C structures used to configure local 
/// BLAST search class implementations. 
class CObjMgr_LocalQueryData : public ILocalQueryData
{
public:
    /// Ctor that takes a vector of SSeqLocs
    /// @param queries queries as a vector of SSeqLoc [in]
    /// @param options Blast options [in]
    CObjMgr_LocalQueryData(TSeqLocVector* queries,
                           const CBlastOptions* options);
    /// Ctor that takes a CBlastQueryVector (preferred over TSeqLocVector).
    /// @param queries queries as a CBlastQueryVector [in]
    /// @param options Blast options [in]
    CObjMgr_LocalQueryData(CBlastQueryVector & queries,
                           const CBlastOptions* options);
    
    virtual BLAST_SequenceBlk* GetSequenceBlk();
    virtual BlastQueryInfo* GetQueryInfo();
    
    
    /// Get the number of queries.
    virtual size_t GetNumQueries();
    
    /// Get the Seq_loc for the sequence indicated by index.
    virtual CConstRef<CSeq_loc> GetSeq_loc(size_t index);
    
    /// Get the length of the sequence indicated by index.
    virtual size_t GetSeqLength(size_t index);
    
private:
    const TSeqLocVector* m_Queries;     ///< Adaptee in adapter design pattern
    CRef<CBlastQueryVector> m_QueryVector;
    const CBlastOptions* m_Options;
    CRef<IBlastQuerySource> m_QuerySource;
};

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData(TSeqLocVector * queries,
                                               const CBlastOptions * opts)
    : m_Queries(queries), m_Options(opts)
{
    m_QuerySource.Reset(new CBlastQuerySourceOM(*queries, opts));
}

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData(CBlastQueryVector   & qv,
                                               const CBlastOptions * opts)
    : m_Queries(NULL), m_QueryVector(& qv), m_Options(opts)
{
    m_QuerySource.Reset(new CBlastQuerySourceOM(qv, opts));
}

BLAST_SequenceBlk*
CObjMgr_LocalQueryData::GetSequenceBlk()
{
    if (m_SeqBlk.Get() == NULL) {
        if (m_Queries || m_QueryVector.NotEmpty()) {
            m_SeqBlk.Reset(SafeSetupQueries(*m_QuerySource, 
                                            m_Options, 
                                            GetQueryInfo(),
                                            m_Messages));
        } else {
            abort();
        }
    }
    return m_SeqBlk.Get();
}

BlastQueryInfo*
CObjMgr_LocalQueryData::GetQueryInfo()
{
    if (m_QueryInfo.Get() == NULL) {
        if (m_QuerySource) {
            m_QueryInfo.Reset(SafeSetupQueryInfo(*m_QuerySource, m_Options));
        } else {
            abort();
        }
    }
    return m_QueryInfo.Get();
}

size_t
CObjMgr_LocalQueryData::GetNumQueries()
{
    size_t retval = m_QuerySource->Size();
    _ASSERT(retval == (size_t)GetQueryInfo()->num_queries);
    return retval;
}

CConstRef<CSeq_loc> 
CObjMgr_LocalQueryData::GetSeq_loc(size_t index)
{
    return m_QuerySource->GetSeqLoc(index);
}

size_t 
CObjMgr_LocalQueryData::GetSeqLength(size_t index)
{
    return m_QuerySource->GetLength(index);
}


/////////////////////////////////////////////////////////////////////////////
//
// CObjMgr_RemoteQueryData
//
/////////////////////////////////////////////////////////////////////////////

class CObjMgr_RemoteQueryData : public IRemoteQueryData
{
public:
    /// Construct query data from a TSeqLocVector.
    /// @param queries Queries expressed as a TSeqLocVector.
    CObjMgr_RemoteQueryData(const TSeqLocVector* queries);
    
    /// Construct query data from a CBlastQueryVector.
    /// @param queries Queries expressed as a CBlastQueryVector.
    CObjMgr_RemoteQueryData(CBlastQueryVector & queries);
    
    /// Accessor for the CBioseq_set.
    virtual CRef<objects::CBioseq_set> GetBioseqSet();

    /// Accessor for the TSeqLocs.
    virtual TSeqLocs GetSeqLocs();

private:
    /// Queries, if input representation is TSeqLocVector, or NULL.
    const TSeqLocVector* m_Queries;

    /// Queries, if input representation is a CBlastQueryVector, or NULL.
    const CRef<CBlastQueryVector> m_QueryVector;
};

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData(const TSeqLocVector* queries)
    : m_Queries(queries)
{}

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData(CBlastQueryVector & qv)
    : m_QueryVector(& qv)
{}

CRef<CBioseq_set>
CObjMgr_RemoteQueryData::GetBioseqSet()
{
    if (m_Bioseqs.Empty()) {
        if (m_QueryVector.NotEmpty()) {
            m_Bioseqs.Reset(s_QueryVectorToBioseqSet(*m_QueryVector));
        } else if (m_Queries) {
            m_Bioseqs.Reset(s_TSeqLocVectorToBioseqSet(m_Queries));
        } else {
            abort();
        }
    }
    return m_Bioseqs;
}

IRemoteQueryData::TSeqLocs
CObjMgr_RemoteQueryData::GetSeqLocs()
{
    if (m_SeqLocs.empty()) {
        if (m_QueryVector.NotEmpty()) {
            m_SeqLocs = s_QueryVectorToTSeqLocs(*m_QueryVector);
        } else if (m_Queries) {
            m_SeqLocs = s_TSeqLocVectorToTSeqLocs(m_Queries);
        } else {
            abort();
        }
    }
    return m_SeqLocs;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgr_QueryFactory
//
/////////////////////////////////////////////////////////////////////////////

CObjMgr_QueryFactory::CObjMgr_QueryFactory(TSeqLocVector& queries)
{
    if (queries.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty TSeqLocVector");
    }

    bool found_packedint = false;
    ITERATE(TSeqLocVector, itr, queries)
    {
        if (((*itr).seqloc)->IsPacked_int())
        {
           found_packedint = true;
           break;
        }
    }

    if (found_packedint)
    {
        NON_CONST_ITERATE(TSeqLocVector, itr, queries)
        {
           if (((*itr).seqloc)->IsPacked_int())
           {
               CSeq_loc* mix = const_cast<CSeq_loc *> (&* (*itr).seqloc);
               NON_CONST_ITERATE(CPacked_seqint::Tdata, it, mix->SetPacked_int().Set())
               {
                     CRef<CSeq_loc> ival(new CSeq_loc);
                     ival->SetInt(**it);
                     m_SSeqLocVector.push_back(SSeqLoc(ival, (*itr).scope, (*itr).mask)); 
               }
           }
           else
           {
               m_SSeqLocVector.push_back(*itr);
           }
        }
    }
    else
    {
        NON_CONST_ITERATE(TSeqLocVector, itr, queries)
        {
               m_SSeqLocVector.push_back(*itr);
        }
    }
}

CObjMgr_QueryFactory::CObjMgr_QueryFactory(CBlastQueryVector & queries)
    : m_QueryVector(& queries)
{
    if (queries.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty CBlastQueryVector");
    }
}

vector< CRef<CScope> >
CObjMgr_QueryFactory::ExtractScopes()
{
    vector< CRef<CScope> > retval;
    if ( !m_SSeqLocVector.empty() ) {
        NON_CONST_ITERATE(TSeqLocVector, itr, m_SSeqLocVector)
            retval.push_back(itr->scope);
    } else if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; i < m_QueryVector->Size(); i++)
            retval.push_back(m_QueryVector->GetScope(i));
    } else {
        abort();
    }
    return retval;
}

TSeqLocVector
CObjMgr_QueryFactory::GetTSeqLocVector()
{
    TSeqLocVector retval;
    if ( !m_SSeqLocVector.empty() ) {
        retval = m_SSeqLocVector;
    } else if (m_QueryVector.NotEmpty()) {
        // FIXME: this is inefficient as it might be copying the masks too many
        // times
        for (CBlastQueryVector::size_type i = 0; 
             i < m_QueryVector->Size(); i++) {
            TMaskedQueryRegions mqr = m_QueryVector->GetMaskedRegions(i);
            CRef<CSeq_loc> masks;
            CRef<CPacked_seqint> conv_masks = mqr.ConvertToCPacked_seqint();
            if (conv_masks.NotEmpty()) {
                masks.Reset(new CSeq_loc);
                masks->SetPacked_int(*conv_masks);
            }
            SSeqLoc sl(m_QueryVector->GetQuerySeqLoc(i), 
                       m_QueryVector->GetScope(i), masks);
            retval.push_back(sl);
        }
    } else {
        abort();
    }
    return retval;
}

/// Auxiliary function to help guess the program type from a CSeq-loc. This
/// should only be used in the context of 
/// CObjMgr_QueryFactory::ExtractUserSpecifiedMasks
static EBlastProgramType
s_GuessProgram(CConstRef<CSeq_loc> mask)
{
    // if we cannot safely determine the program from the mask, specifying
    // nucleotide query for a protein will result in a duplicate mask in the
    // worst case... not great, but acceptable.
    EBlastProgramType retval = eBlastTypeBlastn;
    if (mask.Empty() || mask->GetStrand() == eNa_strand_unknown) {
        return retval;
    }

    return retval;
}

TSeqLocInfoVector
CObjMgr_QueryFactory::ExtractUserSpecifiedMasks()
{
    TSeqLocInfoVector retval;
    if ( !m_SSeqLocVector.empty() ) {
        const EBlastProgramType kProgram = 
            s_GuessProgram(m_SSeqLocVector.front().mask);
        NON_CONST_ITERATE(TSeqLocVector, itr, m_SSeqLocVector) {
            TMaskedQueryRegions mqr = 
                PackedSeqLocToMaskedQueryRegions(itr->mask, kProgram,
                                                 itr->ignore_strand_in_mask);
            retval.push_back(mqr);
        }
    } else if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; i < m_QueryVector->Size(); i++)
            retval.push_back(m_QueryVector->GetMaskedRegions(i));
    } else {
        abort();
    }
    return retval;
}

CRef<ILocalQueryData>
CObjMgr_QueryFactory::x_MakeLocalQueryData(const CBlastOptions* opts)
{
    CRef<ILocalQueryData> retval;
    
    if ( !m_SSeqLocVector.empty() ) {
        retval.Reset(new CObjMgr_LocalQueryData(&m_SSeqLocVector, opts));
    } else if (m_QueryVector.NotEmpty()) {
        retval.Reset(new CObjMgr_LocalQueryData(*m_QueryVector, opts));
    } else {
        abort();
    }
    
    return retval;
}

CRef<IRemoteQueryData>
CObjMgr_QueryFactory::x_MakeRemoteQueryData()
{
    CRef<IRemoteQueryData> retval;

    if ( !m_SSeqLocVector.empty() ) {
        retval.Reset(new CObjMgr_RemoteQueryData(&m_SSeqLocVector));
    } else if (m_QueryVector.NotEmpty()) {
        retval.Reset(new CObjMgr_RemoteQueryData(*m_QueryVector));
    } else {
        abort();
    }

    return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
