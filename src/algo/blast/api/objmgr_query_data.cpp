#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

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
#include <objtools/simple/simple_om.hpp>
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
//
// CBlastQuerySourceTSeqLocs 
//
/////////////////////////////////////////////////////////////////////////////

/// Implements the IBlastQuerySource interface using a list of Seq-locs as data
/// source
class CBlastQuerySourceTSeqLocs : public CBlastQuerySourceOM {
public:
    /// Parametrized constructor
    CBlastQuerySourceTSeqLocs(const CObjMgr_QueryFactory::TSeqLocs& seqlocs,
                              const CBlastOptions* opts);
private:
    TSeqLocVector* 
    x_CreateTSeqLocVector(const CObjMgr_QueryFactory::TSeqLocs& seqlocs);
};

CBlastQuerySourceTSeqLocs::CBlastQuerySourceTSeqLocs
    (const CObjMgr_QueryFactory::TSeqLocs& seqlocs, const CBlastOptions* opts)
    : CBlastQuerySourceOM(*x_CreateTSeqLocVector(seqlocs), opts)
{
    // this will ensure that the parent class deletes the TSeqLocVector
    // allocated above
    m_OwnTSeqLocVector = true;  
}

TSeqLocVector*
CBlastQuerySourceTSeqLocs::x_CreateTSeqLocVector
    (const CObjMgr_QueryFactory::TSeqLocs& seqlocs)
{
    TSeqLocVector* retval = new TSeqLocVector;
    retval->reserve(seqlocs.size());

    CRef<CScope> scope(CSimpleOM::NewScope());

    ITERATE(CObjMgr_QueryFactory::TSeqLocs, cref_seqloc, seqlocs) {
        _ASSERT(cref_seqloc->GetNonNullPointer());
        retval->push_back(SSeqLoc(cref_seqloc->GetPointer(), scope));
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////

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
    
    CRef<CBioseq_set> rv(new CBioseq_set);
    rv->SetSeq_set().swap(se_list);
    
    return rv;
}

static CRef<CBioseq_set>
s_TSeqLocVectorToBioseqSet(const TSeqLocVector* queries)
{
    list< CRef<CSeq_entry> > se_list;
    
    ITERATE(TSeqLocVector, query, *queries) {
        const CBioseq * cbs = 
            query->scope->GetBioseqHandle(*query->seqloc).GetBioseqCore();
        
        CRef<CSeq_entry> se(new CSeq_entry);
        se->SetSeq(*const_cast<CBioseq*>(cbs));
        
        se_list.push_back(se);
    }
    
    CRef<CBioseq_set> rv(new CBioseq_set);
    rv->SetSeq_set().swap(se_list);
    
    return rv;
}

static CObjMgr_QueryFactory::TSeqLocs
s_TSeqLocVectorToTSeqLocs(const TSeqLocVector* queries)
{
    CObjMgr_QueryFactory::TSeqLocs retval;
    
    ITERATE(TSeqLocVector, query, *queries) {
        CRef<CSeq_loc> sl(const_cast<CSeq_loc *>(&* query->seqloc));
        retval.push_back(sl);
    }
    
    return retval;
}

static CObjMgr_QueryFactory::TSeqLocs
s_QueryVectorToTSeqLocs(const CBlastQueryVector & queries)
{
    CObjMgr_QueryFactory::TSeqLocs retval;
    
    for(size_t i = 0; i < queries.Size(); i++) {
        CSeq_loc * slp =
            const_cast<CSeq_loc *>(&* queries.GetQuerySeqLoc(i));
        
        retval.push_back(CRef<CSeq_loc>(slp));
    }
    
    return retval;
}

static CRef<CBioseq_set>
s_TSeqLocsToBioseqSet(const CObjMgr_QueryFactory::TSeqLocs* seqlocs)
{
    _ASSERT(seqlocs);
    _ASSERT(!seqlocs->empty());

    CRef<CBioseq_set> retval(new CBioseq_set);
    CRef<CScope> scope = CSimpleOM::NewScope();

    ITERATE(CObjMgr_QueryFactory::TSeqLocs, itr, *seqlocs) {
        if (const CSeq_id* seqid = (*itr)->GetId()) {
            CRef<CSeq_entry> seq_entry(new CSeq_entry);
            CBioseq_Handle bh = scope->GetBioseqHandle(*seqid);
            seq_entry->SetSeq(const_cast<CBioseq&>(*bh.GetBioseqCore()));
            retval->SetSeq_set().push_back(seq_entry);
        }
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgr_LocalQueryData
//
/////////////////////////////////////////////////////////////////////////////

class CObjMgr_LocalQueryData : public ILocalQueryData
{
public:
    CObjMgr_LocalQueryData(TSeqLocVector* queries,
                           const CBlastOptions* options);
    CObjMgr_LocalQueryData(CBlastQueryVector & queries,
                           const CBlastOptions* options);
    CObjMgr_LocalQueryData(const CObjMgr_QueryFactory::TSeqLocs* seqlocs,
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
    const CObjMgr_QueryFactory::TSeqLocs* m_SeqLocs;
    const CBlastOptions* m_Options;
    AutoPtr<IBlastQuerySource> m_QuerySource;
};

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData(TSeqLocVector * queries,
                                               const CBlastOptions * opts)
    : m_Queries(queries), m_SeqLocs(0), m_Options(opts)
{
    m_QuerySource.reset(new CBlastQuerySourceOM(*queries, opts));
}

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData(CBlastQueryVector   & qv,
                                               const CBlastOptions * opts)
    : m_QueryVector(& qv), m_SeqLocs(0), m_Options(opts)
{
    m_QuerySource.reset(new CBlastQuerySourceOM(qv, opts));
}

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData
    (const CObjMgr_QueryFactory::TSeqLocs* seqlocs, const CBlastOptions* opts)
    : m_Queries(0), m_SeqLocs(seqlocs), m_Options(opts)
{
    m_QuerySource.reset(new CBlastQuerySourceTSeqLocs(*seqlocs, opts));
}

BLAST_SequenceBlk*
CObjMgr_LocalQueryData::GetSequenceBlk()
{
    if (m_SeqBlk.Get() == NULL) {
        if (m_Queries || m_SeqLocs || m_QueryVector.NotEmpty()) {
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
    
    /// Construct query data from a list of CSeq_loc objects.
    /// @param queries Queries expressed as a list of CSeq_loc objects.
    CObjMgr_RemoteQueryData(const CObjMgr_QueryFactory::TSeqLocs* queries);
    
    /// Accessor for the CBioseq_set.
    virtual CRef<objects::CBioseq_set> GetBioseqSet();

    /// Accessor for the TSeqLocs.
    virtual TSeqLocs GetSeqLocs();

private:
    /// Queries, if input representation is TSeqLocVector, or NULL.
    const TSeqLocVector* m_Queries;

    /// Queries, if input representation is a CBlastQueryVector, or NULL.
    const CRef<CBlastQueryVector> m_QueryVector;

    /// Queries, if input representation is a list of CSeq_locs, or NULL.
    const CObjMgr_QueryFactory::TSeqLocs* m_ClientSeqLocs;
};

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData(const TSeqLocVector* queries)
    : m_Queries(queries), m_ClientSeqLocs(0)
{}

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData(CBlastQueryVector & qv)
    : m_QueryVector(& qv), m_ClientSeqLocs(0)
{}

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData
    (const CObjMgr_QueryFactory::TSeqLocs* seqlocs)
    : m_Queries(0), m_ClientSeqLocs(seqlocs)
{}

CRef<CBioseq_set>
CObjMgr_RemoteQueryData::GetBioseqSet()
{
    if (m_Bioseqs.Empty()) {
        if (m_QueryVector.NotEmpty()) {
            m_Bioseqs.Reset(s_QueryVectorToBioseqSet(*m_QueryVector));
        } else if (m_Queries) {
            m_Bioseqs.Reset(s_TSeqLocVectorToBioseqSet(m_Queries));
        } else if (m_ClientSeqLocs) {
            m_Bioseqs.Reset(s_TSeqLocsToBioseqSet(m_ClientSeqLocs));
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
        } else if (m_ClientSeqLocs) {
            m_SeqLocs = *m_ClientSeqLocs;
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
    : m_SSeqLocVector(&queries), m_SeqLocs(0), m_OwnSeqLocs(false)
{
    if (queries.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty TSeqLocVector");
    }
}

CObjMgr_QueryFactory::CObjMgr_QueryFactory(CBlastQueryVector & queries)
    : m_SSeqLocVector(0), m_QueryVector(& queries), 
    m_SeqLocs(0), m_OwnSeqLocs(false)
{
    if (queries.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty CBlastQueryVector");
    }
}

CObjMgr_QueryFactory::CObjMgr_QueryFactory(CRef<objects::CSeq_loc> seqloc)
    : m_SSeqLocVector(0), m_SeqLocs(new TSeqLocs(1, seqloc)),
    m_OwnSeqLocs(true)
{
    if ( seqloc.Empty() || seqloc->IsNull() || 
         (seqloc->Which() == CSeq_loc::e_not_set)) {
        delete m_SeqLocs;
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty/invalid Seq-loc");
    }
}

CObjMgr_QueryFactory::CObjMgr_QueryFactory(const TSeqLocs& seqlocs)
    : m_SSeqLocVector(0), m_SeqLocs(&seqlocs), m_OwnSeqLocs(false)
{
    if ( seqlocs.empty() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty Seq-loc list");
    }
}

CObjMgr_QueryFactory::~CObjMgr_QueryFactory()
{
    if (m_OwnSeqLocs) {
        delete m_SeqLocs;
    }
}

vector< CRef<CScope> >
CObjMgr_QueryFactory::ExtractScopes()
{
    vector< CRef<CScope> > retval;
    if (m_SSeqLocVector) {
        _ASSERT( !m_SSeqLocVector->empty() );
        NON_CONST_ITERATE(TSeqLocVector, itr, *m_SSeqLocVector)
            retval.push_back(itr->scope);
    } else if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; i < m_QueryVector->Size(); i++)
            retval.push_back(m_QueryVector->GetScope(i));
    } else if (m_SeqLocs) {
        retval.assign(m_SeqLocs->size(), CSimpleOM::NewScope());
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
    if (m_SSeqLocVector) {
        _ASSERT( !m_SSeqLocVector->empty() );
        const EBlastProgramType kProgram = 
            s_GuessProgram(m_SSeqLocVector->front().mask);
        NON_CONST_ITERATE(TSeqLocVector, itr, *m_SSeqLocVector) {
            TMaskedQueryRegions mqr = 
                PackedSeqLocToMaskedQueryRegions(itr->mask, kProgram,
                                                 itr->ignore_strand_in_mask);
            retval.push_back(mqr);
        }
    } else if (m_QueryVector.NotEmpty()) {
        for (CBlastQueryVector::size_type i = 0; i < m_QueryVector->Size(); i++)
            retval.push_back(m_QueryVector->GetMaskedRegions(i));
    } else if (m_SeqLocs) {
        retval.assign(m_SeqLocs->size(), TMaskedQueryRegions());
    } else {
        abort();
    }
    return retval;
}

CRef<ILocalQueryData>
CObjMgr_QueryFactory::x_MakeLocalQueryData(const CBlastOptions* opts)
{
    CRef<ILocalQueryData> retval;
    
    if (m_SSeqLocVector) {
        retval.Reset(new CObjMgr_LocalQueryData(m_SSeqLocVector, opts));
    } else if (m_QueryVector.NotEmpty()) {
        retval.Reset(new CObjMgr_LocalQueryData(*m_QueryVector, opts));
    } else if (m_SeqLocs) {
        retval.Reset(new CObjMgr_LocalQueryData(m_SeqLocs, opts));
    } else {
        abort();
    }
    
    return retval;
}

CRef<IRemoteQueryData>
CObjMgr_QueryFactory::x_MakeRemoteQueryData()
{
    CRef<IRemoteQueryData> retval;

    if (m_SSeqLocVector) {
        retval.Reset(new CObjMgr_RemoteQueryData(m_SSeqLocVector));
    } else if (m_QueryVector.NotEmpty()) {
        retval.Reset(new CObjMgr_RemoteQueryData(*m_QueryVector));
    } else if (m_SeqLocs) {
        retval.Reset(new CObjMgr_RemoteQueryData(m_SeqLocs));
    } else {
        abort();
    }

    return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
