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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id handle for Object Manager
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <serial/typeinfo.hpp>
#include "seq_id_tree.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Info
//

//#define NCBI_SLOW_ATOMIC_SWAP
#ifdef NCBI_SLOW_ATOMIC_SWAP
DEFINE_STATIC_FAST_MUTEX(sx_GetSeqIdMutex);
#endif


CSeq_id_Info::CSeq_id_Info(CSeq_id::E_Choice type,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(type),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
    m_LockCounter.Set(0);
}


CSeq_id_Info::CSeq_id_Info(const CConstRef<CSeq_id>& seq_id,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(seq_id->Which()),
      m_Seq_id(seq_id),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
    m_LockCounter.Set(0);
}


CConstRef<CSeq_id> CSeq_id_Info::GetGiSeqId(int gi) const
{
    CConstRef<CSeq_id> ret;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<CSeq_id_Info*>(this)->m_Seq_id = ret;
#else
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    const_cast<CSeq_id&>(*ret).SetGi(gi);
    return ret;
}


CSeq_id_Info::~CSeq_id_Info(void)
{
    _ASSERT(m_LockCounter.Get() == 0);
}


CSeq_id_Which_Tree& CSeq_id_Info::GetTree(void) const
{
    return GetMapper().x_GetTree(GetType());
}



void CSeq_id_Info::x_RemoveLastLock(void) const
{
    GetTree().DropInfo(this);
}


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqId(void) const
{
    _ASSERT(m_Info);
    CConstRef<CSeq_id> ret;
    if ( IsGi() ) {
        ret = m_Info->GetGiSeqId(GetGi());
    }
    else {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqIdOrNull(void) const
{
    CConstRef<CSeq_id> ret;
    if ( IsGi() ) {
        _ASSERT(m_Info);
        ret = m_Info->GetGiSeqId(GetGi());
    }
    else if ( m_Info ) {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CSeq_id_Handle CSeq_id_Handle::GetGiHandle(int gi)
{
    return CSeq_id_Mapper::GetInstance()->GetGiHandle(gi);
}


CSeq_id_Handle CSeq_id_Handle::GetHandle(const CSeq_id& id)
{
    return CSeq_id_Mapper::GetInstance()->GetHandle(id);
}


bool CSeq_id_Handle::HaveMatchingHandles(void) const
{
    return GetMapper().HaveMatchingHandles(*this);
}


bool CSeq_id_Handle::HaveReverseMatch(void) const
{
    return GetMapper().HaveReverseMatch(*this);
}


void CSeq_id_Handle::GetMatchingHandles(TMatches& matches) const
{
    GetMapper().GetMatchingHandles(*this, matches);
}


void CSeq_id_Handle::GetReverseMatchingHandles(TMatches& matches) const
{
    GetMapper().GetReverseMatchingHandles(*this, matches);
}


bool CSeq_id_Handle::IsBetter(const CSeq_id_Handle& h) const
{
    return GetMapper().x_IsBetter(*this, h);
}


bool CSeq_id_Handle::MatchesTo(const CSeq_id_Handle& h) const
{
    return GetMapper().x_Match(*this, h);
}


bool CSeq_id_Handle::operator==(const CSeq_id& id) const
{
    if ( IsGi() ) {
        return id.IsGi() && id.GetGi() == GetGi();
    }
    return *this == GetMapper().GetHandle(id);
}


string CSeq_id_Handle::AsString() const
{
    CNcbiOstrstream os;
    if ( IsGi() ) {
        os << "gi|" << GetGi();
    }
    else if ( m_Info ) {
        m_Info->GetSeqId()->WriteAsFasta(os);
    }
    else {
        os << "unknown";
    }
    return CNcbiOstrstreamToString(os);
}


unsigned CSeq_id_Handle::GetHash(void) const
{
    unsigned hash = m_Gi;
    if ( !hash ) {
        hash = unsigned((intptr_t)(m_Info.GetPointerOrNull())>>3);
    }
    return hash;
}


string GetDirectLabel(const CSeq_id& id)
{
    string ret;
    if ( !id.IsGi() ) {
        if ( id.IsGeneral() ) {
            const CDbtag& dbtag = id.GetGeneral();
            const CObject_id& obj_id = dbtag.GetTag();
            if ( obj_id.IsStr() && dbtag.GetDb() == "LABEL" ) {
                ret = obj_id.GetStr();
            }
        }
        else {
            const CTextseq_id* text_id = id.GetTextseq_Id();
            if ( text_id &&
                 text_id->IsSetAccession() &&
                 text_id->IsSetVersion() ) {
                ret = text_id->GetAccession() + '.' +
                    NStr::IntToString(text_id->GetVersion());
            }
        }
    }
    return ret;
}


string GetDirectLabel(const CSeq_id_Handle& idh)
{
    string ret;
    if ( !idh.IsGi() ) {
        ret = GetDirectLabel(*idh.GetSeqId());
    }
    return ret;
}


string GetLabel(const CSeq_id& id)
{
    string ret;
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if ( text_id ) {
        if ( text_id->IsSetAccession() ) {
            ret = text_id->GetAccession();
        }
        else if ( text_id->IsSetName() ) {
            ret = text_id->GetName();
        }
        if ( text_id->IsSetVersion() ) {
            ret += '.';
            ret += NStr::IntToString(text_id->GetVersion());
        }
    }
    else if ( id.IsGeneral() ) {
        const CDbtag& dbtag = id.GetGeneral();
        const CObject_id& obj_id = dbtag.GetTag();
        if ( obj_id.IsStr() && dbtag.GetDb() == "LABEL" ) {
            ret = obj_id.GetStr();
        }
    }
    if ( ret.empty() ) {
        ret = id.AsFastaString();
    }
    return ret;
}


string GetLabel(const CSeq_id_Handle& idh)
{
    string ret;
    if ( idh.IsGi() ) {
        ret = idh.AsString();
    }
    else {
        ret = GetLabel(*idh.GetSeqId());
    }
    return ret;
}


string GetLabel(const vector<CSeq_id_Handle>& ids)
{
    string ret;
    CSeq_id_Handle best_id;
    int best_score = kMax_Int;
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        CConstRef<CSeq_id> id = it->GetSeqId();
        int score = id->TextScore();
        if ( score < best_score ) {
            best_score = score;
            best_id = *it;
        }
    }
    if ( best_id ) {
        ret = GetLabel(best_id);
    }
    return ret;
}


string GetLabel(const vector<CRef<CSeq_id> >& ids)
{
    string ret;
    const CSeq_id* best_id = 0;
    int best_score = kMax_Int;
    ITERATE ( vector<CRef<CSeq_id> >, it, ids ) {
        const CSeq_id& id = **it;
        int score = id.TextScore();
        if ( score < best_score ) {
            best_score = score;
            best_id = &id;
        }
    }
    if ( best_id ) {
        ret = GetLabel(*best_id);
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
