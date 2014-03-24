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


CSeq_id_Info::CSeq_id_Info(CSeq_id::E_Choice type,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(type),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
}


CSeq_id_Info::CSeq_id_Info(const CConstRef<CSeq_id>& seq_id,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(seq_id->Which()),
      m_Seq_id(seq_id),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
}


CSeq_id_Info::~CSeq_id_Info(void)
{
    _ASSERT(m_LockCounter.Get() == 0);
}


CSeq_id_Which_Tree& CSeq_id_Info::GetTree(void) const
{
    return GetMapper().x_GetTree(GetType());
}


CConstRef<CSeq_id> CSeq_id_Info::GetPackedSeqId(TPacked /*packed*/) const
{
    NCBI_THROW(CSeq_id_MapperException, eTypeError,
               "CSeq_id_Handle is not packed");
}


void CSeq_id_Info::x_RemoveLastLock(void) const
{
    GetTree().DropInfo(this);
}


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


CSeq_id_Handle CSeq_id_Handle::GetHandle(TGi gi)
{
    return CSeq_id_Mapper::GetInstance()->GetGiHandle(gi);
}


CSeq_id_Handle CSeq_id_Handle::GetHandle(const CSeq_id& id)
{
    return CSeq_id_Mapper::GetInstance()->GetHandle(id);
}


CSeq_id_Handle CSeq_id_Handle::GetHandle(const string& str_id)
{
    CSeq_id id(str_id);
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


bool CSeq_id_Handle::HaveMatchingHandles(EAllowWeakMatch allow_weak_match) const
{
    return GetMapper().HaveMatchingHandles(*this, allow_weak_match);
}


bool CSeq_id_Handle::HaveReverseMatch(EAllowWeakMatch allow_weak_match) const
{
    return GetMapper().HaveReverseMatch(*this, allow_weak_match);
}


void CSeq_id_Handle::GetMatchingHandles(TMatches& matches) const
{
    GetMapper().GetMatchingHandles(*this, matches);
}


void CSeq_id_Handle::GetReverseMatchingHandles(TMatches& matches) const
{
    GetMatchingHandles(matches);
    GetMapper().GetReverseMatchingHandles(*this, matches);
}


void CSeq_id_Handle::GetMatchingHandles(TMatches& matches,
                                        EAllowWeakMatch allow_weak_match) const
{
    GetMapper().GetMatchingHandles(*this, matches, allow_weak_match);
}


void CSeq_id_Handle::GetReverseMatchingHandles(TMatches& matches,
                                               EAllowWeakMatch allow_weak_match) const
{
    GetMatchingHandles(matches, allow_weak_match);
    GetMapper().GetReverseMatchingHandles(*this, matches, allow_weak_match);
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
        return id.IsGi() && id.GetGi() == TGi(m_Packed);
    }
    return *this == GetMapper().GetHandle(id);
}


TIntId CSeq_id_Handle::CompareOrdered(const CSeq_id_Handle& id) const
{
    // small optimization to avoid creation of temporary CSeq_id objects
    if ( TIntId diff = Which() - id.Which() ) {
        return diff;
    }
    if ( IsGi() && id.IsGi() ) {
        return GetGi() - id.GetGi();
    }
    return GetSeqId()->CompareOrdered(*id.GetSeqId());
}


string CSeq_id_Handle::AsString() const
{
    CNcbiOstrstream os;
    if ( IsGi() ) {
        os << "gi|" << m_Packed;
    }
    else if ( m_Info ) {
        GetSeqId()->WriteAsFasta(os);
    }
    else {
        os << "unknown";
    }
    return CNcbiOstrstreamToString(os);
}


unsigned CSeq_id_Handle::GetHash(void) const
{
    unsigned hash = INT_ID_TO(unsigned, m_Packed);
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
            NStr::ToUpper(ret);
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


CNcbiOstream& operator<<(CNcbiOstream& out, const CSeq_id_Handle& idh)
{
    if ( idh.IsGi() ) {
        out << "gi|" << idh.GetPacked();
    }
    else if ( idh ) {
        idh.GetSeqId()->WriteAsFasta(out);
    }
    else {
        out << "null";
    }
    return out;
}


END_SCOPE(objects)
END_NCBI_SCOPE
