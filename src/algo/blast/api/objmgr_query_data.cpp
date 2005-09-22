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

class CBlastQuerySourceTSeqLocs : public IBlastQuerySource {
public:
    CBlastQuerySourceTSeqLocs(const CObjMgr_QueryFactory::TSeqLocs& seqlocs);
    ENa_strand GetStrand(int i) const;
    CConstRef<CSeq_loc> GetMask(int i) const;
    CConstRef<CSeq_loc> GetSeqLoc(int i) const;
    SBlastSequence GetBlastSequence(int i,
                                     EBlastEncoding encoding,
                                     ENa_strand strand,
                                     ESentinelType sentinel,
                                     string* warnings = 0) const;
    TSeqPos GetLength(int i) const;
    TSeqPos Size() const;
private:
    vector< CRef<CSeq_loc> > m_SeqLocs;
    mutable CRef<CScope> m_Scope;
};

CBlastQuerySourceTSeqLocs::CBlastQuerySourceTSeqLocs
    (const CObjMgr_QueryFactory::TSeqLocs& seqlocs)
{
    m_SeqLocs.reserve(seqlocs.size());
    ITERATE(CObjMgr_QueryFactory::TSeqLocs, itr, seqlocs) {
        ASSERT(itr->GetNonNullPointer());
        m_SeqLocs.push_back(*itr);
    }
    m_Scope = CSimpleOM::NewScope();
}

inline ENa_strand 
CBlastQuerySourceTSeqLocs::GetStrand(int i) const 
{ 
    return m_SeqLocs[i]->GetStrand(); 
}

inline CConstRef<CSeq_loc> 
CBlastQuerySourceTSeqLocs::GetMask(int /*i*/) const 
{ 
    return CConstRef<CSeq_loc>(0); 
}

inline CConstRef<CSeq_loc> 
CBlastQuerySourceTSeqLocs::GetSeqLoc(int i) const 
{ 
    return m_SeqLocs[i]; 
}

inline SBlastSequence 
CBlastQuerySourceTSeqLocs::GetBlastSequence(int i,
                                            EBlastEncoding encoding,
                                            ENa_strand strand,
                                            ESentinelType sentinel,
                                            string* warnings) const 
{
    return GetSequence(*m_SeqLocs[i], encoding, m_Scope, strand, sentinel, 
                       warnings);
}

TSeqPos 
CBlastQuerySourceTSeqLocs::GetLength(int i) const 
{ 
    TSeqPos retval = sequence::GetLength(*m_SeqLocs[i], m_Scope); 
    ASSERT(retval != numeric_limits<TSeqPos>::max());
    return retval;
}

inline TSeqPos 
CBlastQuerySourceTSeqLocs::Size() const 
{ 
    return m_SeqLocs.size(); 
}

/////////////////////////////////////////////////////////////////////////////

BLAST_SequenceBlk*
s_SafeSetupQueries(const IBlastQuerySource& queries,
                   const CBlastOptions* options,
                   const BlastQueryInfo* query_info)
{
    ASSERT(options);
    ASSERT(query_info);
    ASSERT( !queries.Empty() );

    CBLAST_SequenceBlk retval;
    Blast_Message* blast_msg = NULL;
    TAutoUint1ArrayPtr gc = FindGeneticCode(options->GetQueryGeneticCode());

    SetupQueries_OMF(queries, query_info, &retval, options->GetProgramType(), 
                     options->GetStrandOption(), gc.get(), &blast_msg);

    string error_message;
    if (blast_msg) {
        error_message = blast_msg->message;
        Blast_MessageFree(blast_msg);
    }
    if (retval.Get() == NULL) {
        error_message += "\nblast::SetupQueries failure";
    }
    if ( !error_message.empty() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, error_message);
    }

    return retval.Release();
}

BlastQueryInfo*
s_SafeSetupQueryInfo(const IBlastQuerySource& queries,
                     const CBlastOptions* options)
{
    ASSERT(!queries.Empty());
    ASSERT(options);

    CBlastQueryInfo retval;
    SetupQueryInfo_OMF(queries, options->GetProgramType(),
                       options->GetStrandOption(), &retval);

    if (retval.Get() == NULL) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "blast::SetupQueryInfo failed");
    }
    return retval.Release();
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

static CRef<CBioseq_set>
s_TSeqLocsToBioseqSet(const CObjMgr_QueryFactory::TSeqLocs* seqlocs)
{
    ASSERT(seqlocs);
    ASSERT(!seqlocs->empty());

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
    CObjMgr_LocalQueryData(const TSeqLocVector* queries,
                           const CBlastOptions* options);
    CObjMgr_LocalQueryData(const CObjMgr_QueryFactory::TSeqLocs* seqlocs,
                           const CBlastOptions* options);
    
    BLAST_SequenceBlk* GetSequenceBlk();
    BlastQueryInfo* GetQueryInfo();
    
    
    /// Get the number of queries.
    virtual int GetNumQueries();
    
    /// Get the Seq_loc for the sequence indicated by index.
    virtual CConstRef<CSeq_loc> GetSeq_loc(int index);
    
    /// Get the length of the sequence indicated by index.
    virtual int GetSeqLength(int index);
    
private:
    const TSeqLocVector* m_Queries;     ///< Adaptee in adapter design pattern
    const CObjMgr_QueryFactory::TSeqLocs* m_SeqLocs;
    const CBlastOptions* m_Options;
    AutoPtr<IBlastQuerySource> m_QuerySource;
};

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData(const TSeqLocVector * queries,
                                               const CBlastOptions * opts)
    : m_Queries(queries), m_SeqLocs(0), m_Options(opts)
{
    m_QuerySource.reset(new CBlastQuerySourceOM(*queries));
}

CObjMgr_LocalQueryData::CObjMgr_LocalQueryData
    (const CObjMgr_QueryFactory::TSeqLocs* seqlocs, const CBlastOptions* opts)
    : m_Queries(0), m_SeqLocs(seqlocs), m_Options(opts)
{
    m_QuerySource.reset(new CBlastQuerySourceTSeqLocs(*seqlocs));
}

BLAST_SequenceBlk*
CObjMgr_LocalQueryData::GetSequenceBlk()
{
    if (m_SeqBlk.Get() == NULL) {
        if (m_Queries || m_SeqLocs) {
            m_SeqBlk.Reset(s_SafeSetupQueries(*m_QuerySource, 
                                              m_Options, 
                                              GetQueryInfo()));
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
            m_QueryInfo.Reset(s_SafeSetupQueryInfo(*m_QuerySource, m_Options));
        } else {
            abort();
        }
    }
    return m_QueryInfo.Get();
}

int
CObjMgr_LocalQueryData::GetNumQueries()
{
    int retval = m_QuerySource->Size();
    ASSERT(retval == GetQueryInfo()->num_queries);
    return retval;
}

CConstRef<CSeq_loc> 
CObjMgr_LocalQueryData::GetSeq_loc(int index)
{
    return m_QuerySource->GetSeqLoc(index);
}

int 
CObjMgr_LocalQueryData::GetSeqLength(int index)
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
    CObjMgr_RemoteQueryData(const TSeqLocVector* queries);
    CObjMgr_RemoteQueryData(const CObjMgr_QueryFactory::TSeqLocs* queries);
    
    CRef<objects::CBioseq_set> GetBioseqSet();
    TSeqLocs GetSeqLocs();

private:
    const TSeqLocVector* m_Queries;
    const CObjMgr_QueryFactory::TSeqLocs* m_ClientSeqLocs;
};

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData(const TSeqLocVector* queries)
    : m_Queries(queries), m_ClientSeqLocs(0)
{}

CObjMgr_RemoteQueryData::CObjMgr_RemoteQueryData
    (const CObjMgr_QueryFactory::TSeqLocs* seqlocs)
    : m_Queries(0), m_ClientSeqLocs(seqlocs)
{}

CRef<CBioseq_set>
CObjMgr_RemoteQueryData::GetBioseqSet()
{
    if (m_Bioseqs.Empty()) {
        if (m_Queries) {
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
        if (m_Queries) {
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

CObjMgr_QueryFactory::CObjMgr_QueryFactory(const TSeqLocVector& queries)
    : m_SSeqLocVector(&queries), m_SeqLocs(0), m_OwnSeqLocs(false)
{
    if (queries.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Empty SSeqLocVector");
    }
}

CObjMgr_QueryFactory::CObjMgr_QueryFactory(CRef<objects::CSeq_loc> seqloc)
    : m_SSeqLocVector(0), m_SeqLocs(new TSeqLocs(1, seqloc)),
    m_OwnSeqLocs(true)
{
    if ( seqloc.Empty() || seqloc->IsNull() || 
         (seqloc->Which() == CSeq_loc::e_not_set)) {
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

CRef<ILocalQueryData>
CObjMgr_QueryFactory::x_MakeLocalQueryData(const CBlastOptions* opts)
{
    CRef<ILocalQueryData> retval;
    
    if (m_SSeqLocVector) {
        retval.Reset(new CObjMgr_LocalQueryData(m_SSeqLocVector, opts));
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
