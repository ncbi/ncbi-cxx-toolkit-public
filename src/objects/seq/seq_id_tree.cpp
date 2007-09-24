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
*   Seq-id mapper for Object Manager
*
*/

#include <ncbi_pch.hpp>
#include "seq_id_tree.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CSeq_id_***_Tree::
//
//    Seq-id sub-type specific trees
//

CSeq_id_Which_Tree::CSeq_id_Which_Tree(CSeq_id_Mapper* mapper)
    : m_Mapper(mapper)
{
    _ASSERT(mapper);
}


CSeq_id_Which_Tree::~CSeq_id_Which_Tree(void)
{
}


bool CSeq_id_Which_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return false; // Assume no matches by default
}


void CSeq_id_Which_Tree::FindMatch(const CSeq_id_Handle& id,
                                   TSeq_id_MatchList& id_list) const
{
    id_list.insert(id); // only exact match by default
}


bool CSeq_id_Which_Tree::Match(const CSeq_id_Handle& h1,
                               const CSeq_id_Handle& h2) const
{
    if ( h1 == h2 ) {
        return true;
    }
    if ( HaveMatch(h1) ) {
        TSeq_id_MatchList id_list;
        FindMatch(h1, id_list);
        return id_list.find(h2) != id_list.end();
    }
    return false;
}


bool CSeq_id_Which_Tree::IsBetterVersion(const CSeq_id_Handle& /*h1*/,
                                         const CSeq_id_Handle& /*h2*/) const
{
    return false; // No id version by default
}


inline
CSeq_id_Info* CSeq_id_Which_Tree::CreateInfo(CSeq_id::E_Choice type)
{
    return new CSeq_id_Info(type, m_Mapper);
}


bool CSeq_id_Which_Tree::HaveReverseMatch(const CSeq_id_Handle& ) const
{
    return false; // Assume no reverse matches by default
}


void CSeq_id_Which_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                          TSeq_id_MatchList& id_list)
{
    id_list.insert(id);
    return;
}


CSeq_id_Info* CSeq_id_Which_Tree::CreateInfo(const CSeq_id& id)
{
    CRef<CSeq_id> id_ref(new CSeq_id);
    id_ref->Assign(id);
    return new CSeq_id_Info(id_ref, m_Mapper);
}


void CSeq_id_Which_Tree::DropInfo(const CSeq_id_Info* info)
{
    TWriteLockGuard guard(m_TreeLock);
    if ( info->GetLockCounter() != 0 ) {
        _ASSERT(info->m_Seq_id);
        return;
    }
    if ( !info->m_Seq_id ) {
        _ASSERT(info->GetLockCounter() == 0);
        return;
    }
    x_Unindex(info);
    _ASSERT(info->GetLockCounter()==0);
    _ASSERT(info->m_Seq_id);
    const_cast<CSeq_id_Info*>(info)->m_Seq_id.Reset();
}


CSeq_id_Handle CSeq_id_Which_Tree::GetGiHandle(int /*gi*/)
{
    NCBI_THROW(CIdMapperException, eTypeError, "Invalid seq-id type");
}


void CSeq_id_Which_Tree::Initialize(CSeq_id_Mapper* mapper,
                                    vector<CRef<CSeq_id_Which_Tree> >& v)
{
    v.resize(CSeq_id::e_MaxChoice);
    v[CSeq_id::e_not_set].Reset(new CSeq_id_not_set_Tree(mapper));
    v[CSeq_id::e_Local].Reset(new CSeq_id_Local_Tree(mapper));
    v[CSeq_id::e_Gibbsq].Reset(new CSeq_id_Gibbsq_Tree(mapper));
    v[CSeq_id::e_Gibbmt].Reset(new CSeq_id_Gibbmt_Tree(mapper));
    v[CSeq_id::e_Giim].Reset(new CSeq_id_Giim_Tree(mapper));
    // These three types share the same accessions space
    CRef<CSeq_id_Which_Tree> gb(new CSeq_id_GB_Tree(mapper));
    v[CSeq_id::e_Genbank] = gb;
    v[CSeq_id::e_Embl] = gb;
    v[CSeq_id::e_Ddbj] = gb;
    v[CSeq_id::e_Pir].Reset(new CSeq_id_Pir_Tree(mapper));
    v[CSeq_id::e_Swissprot].Reset(new CSeq_id_Swissprot_Tree(mapper));
    v[CSeq_id::e_Patent].Reset(new CSeq_id_Patent_Tree(mapper));
    v[CSeq_id::e_Other].Reset(new CSeq_id_Other_Tree(mapper));
    v[CSeq_id::e_General].Reset(new CSeq_id_General_Tree(mapper));
    v[CSeq_id::e_Gi].Reset(new CSeq_id_Gi_Tree(mapper));
    // see above    v[CSeq_id::e_Ddbj] = gb;
    v[CSeq_id::e_Prf].Reset(new CSeq_id_Prf_Tree(mapper));
    v[CSeq_id::e_Pdb].Reset(new CSeq_id_PDB_Tree(mapper));
    v[CSeq_id::e_Tpg].Reset(new CSeq_id_Tpg_Tree(mapper));
    v[CSeq_id::e_Tpe].Reset(new CSeq_id_Tpe_Tree(mapper));
    v[CSeq_id::e_Tpd].Reset(new CSeq_id_Tpd_Tree(mapper));
    v[CSeq_id::e_Gpipe].Reset(new CSeq_id_Gpipe_Tree(mapper));
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_not_set_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_not_set_Tree::CSeq_id_not_set_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_not_set_Tree::~CSeq_id_not_set_Tree(void)
{
}


bool CSeq_id_not_set_Tree::Empty(void) const
{
    return true;
}


inline
bool CSeq_id_not_set_Tree::x_Check(const CSeq_id& id) const
{
    return id.Which() == CSeq_id::e_not_set;
}


void CSeq_id_not_set_Tree::DropInfo(const CSeq_id_Info* /*info*/)
{
}


void CSeq_id_not_set_Tree::x_Unindex(const CSeq_id_Info* /*info*/)
{
}


CSeq_id_Handle CSeq_id_not_set_Tree::FindInfo(const CSeq_id& /*id*/) const
{
    // LOG_POST(Warning << "CSeq_id_Mapper::GetHandle() -- uninitialized seq-id");
    return CSeq_id_Handle();
}


CSeq_id_Handle CSeq_id_not_set_Tree::FindOrCreate(const CSeq_id& /*id*/)
{
    // LOG_POST(Warning << "CSeq_id_Mapper::GetHandle() -- uninitialized seq-id");
    return CSeq_id_Handle();
}


void CSeq_id_not_set_Tree::FindMatch(const CSeq_id_Handle& /*id*/,
                                     TSeq_id_MatchList& /*id_list*/) const
{
    LOG_POST(Warning << "CSeq_id_Mapper::GetMatchingHandles() -- "
             "uninitialized seq-id");
}


void CSeq_id_not_set_Tree::FindMatchStr(string /*sid*/,
                                        TSeq_id_MatchList& /*id_list*/) const
{
}


void CSeq_id_not_set_Tree::FindReverseMatch(const CSeq_id_Handle& /*id*/,
                                            TSeq_id_MatchList& /*id_list*/)
{
    LOG_POST(Warning << "CSeq_id_Mapper::GetReverseMatchingHandles() -- "
             "uninitialized seq-id");
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_int_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_int_Tree::CSeq_id_int_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_int_Tree::~CSeq_id_int_Tree(void)
{
}


bool CSeq_id_int_Tree::Empty(void) const
{
    return m_IntMap.empty();
}


CSeq_id_Handle CSeq_id_int_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT(x_Check(id));
    int value = x_Get(id);

    TReadLockGuard guard(m_TreeLock);
    TIntMap::const_iterator it = m_IntMap.find(value);
    if (it != m_IntMap.end()) {
        return CSeq_id_Handle(it->second);
    }
    return CSeq_id_Handle();
}


CSeq_id_Handle CSeq_id_int_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    int value = x_Get(id);

    TWriteLockGuard guard(m_TreeLock);
    pair<TIntMap::iterator, bool> ins =
        m_IntMap.insert(TIntMap::value_type(value, 0));
    if ( ins.second ) {
        ins.first->second = CreateInfo(id);
    }
    return CSeq_id_Handle(ins.first->second);
}


void CSeq_id_int_Tree::x_Unindex(const CSeq_id_Info* info)
{
    _ASSERT(x_Check(*info->GetSeqId()));
    int value = x_Get(*info->GetSeqId());

    _VERIFY(m_IntMap.erase(value));
}


void CSeq_id_int_Tree::FindMatchStr(string sid,
                                    TSeq_id_MatchList& id_list) const
{
    int value;
    try {
        value = NStr::StringToInt(sid);
    }
    catch (const CStringException& /*ignored*/) {
        // Not an integer value
        return;
    }
    TReadLockGuard guard(m_TreeLock);
    TIntMap::const_iterator it = m_IntMap.find(value);
    if (it != m_IntMap.end()) {
        id_list.insert(CSeq_id_Handle(it->second));
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gibbsq_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gibbsq_Tree::CSeq_id_Gibbsq_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_int_Tree(mapper)
{
}


bool CSeq_id_Gibbsq_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGibbsq();
}


int CSeq_id_Gibbsq_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGibbsq();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gibbmt_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gibbmt_Tree::CSeq_id_Gibbmt_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_int_Tree(mapper)
{
}


bool CSeq_id_Gibbmt_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGibbmt();
}


int CSeq_id_Gibbmt_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGibbmt();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gi_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gi_Tree::CSeq_id_Gi_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper),
      m_SharedInfo(CreateInfo(CSeq_id::e_Gi))
{
}


CSeq_id_Gi_Tree::~CSeq_id_Gi_Tree(void)
{
    m_ZeroInfo.Reset();
    _ASSERT(m_SharedInfo);
    m_SharedInfo.Reset();
}


bool CSeq_id_Gi_Tree::Empty(void) const
{
    return true;
}


inline
bool CSeq_id_Gi_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGi();
}


inline
int CSeq_id_Gi_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGi();
}


void CSeq_id_Gi_Tree::DropInfo(const CSeq_id_Info* /*info*/)
{
}


void CSeq_id_Gi_Tree::x_Unindex(const CSeq_id_Info* /*info*/)
{
}


CSeq_id_Handle CSeq_id_Gi_Tree::GetGiHandle(int gi)
{
    if ( gi ) {
        return CSeq_id_Handle(m_SharedInfo, gi);
    }
    else {
        if ( !m_ZeroInfo ) {
            TWriteLockGuard guard(m_TreeLock);
            if ( !m_ZeroInfo ) {
                CRef<CSeq_id> zero_id(new CSeq_id);
                zero_id->SetGi(0);
                m_ZeroInfo.Reset(CreateInfo(*zero_id));
            }
        }
        return CSeq_id_Handle(m_ZeroInfo);
    }
}


CSeq_id_Handle CSeq_id_Gi_Tree::FindInfo(const CSeq_id& id) const
{
    CSeq_id_Handle ret;
    _ASSERT(x_Check(id));
    int gi = x_Get(id);
    if ( gi ) {
        ret = CSeq_id_Handle(m_SharedInfo, gi);
    }
    else if ( m_ZeroInfo ) {
        ret = CSeq_id_Handle(m_ZeroInfo);
    }
    return ret;
}


CSeq_id_Handle CSeq_id_Gi_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    return GetGiHandle(x_Get(id));
}


void CSeq_id_Gi_Tree::FindMatchStr(string sid,
                                   TSeq_id_MatchList& id_list) const
{
    int gi;
    try {
        gi = NStr::StringToInt(sid);
    }
    catch (const CStringException& /*ignored*/) {
        // Not an integer value
        return;
    }
    if ( gi ) {
        id_list.insert(CSeq_id_Handle(m_SharedInfo, gi));
    }
    else if ( m_ZeroInfo ) {
        id_list.insert(CSeq_id_Handle(m_ZeroInfo));
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Textseq_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Textseq_Tree::CSeq_id_Textseq_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Textseq_Tree::~CSeq_id_Textseq_Tree(void)
{
}


bool CSeq_id_Textseq_Tree::Empty(void) const
{
    return m_ByName.empty() && m_ByAccession.empty();
}


bool CSeq_id_Textseq_Tree::x_Equals(const CTextseq_id& id1,
                                    const CTextseq_id& id2)
{
    if ( id1.IsSetAccession() != id2.IsSetAccession() ) {
        return false;
    }
    if ( id1.IsSetName() != id2.IsSetName() ) {
        return false;
    }
    if ( id1.IsSetVersion() != id2.IsSetVersion() ) {
        return false;
    }
    if ( id1.IsSetRelease() != id2.IsSetRelease() ) {
        return false;
    }
    if ( id1.IsSetAccession() &&
         !NStr::EqualNocase(id1.GetAccession(), id2.GetAccession()) ) {
        return false;
    }
    if ( id1.IsSetName() &&
         !NStr::EqualNocase(id1.GetName(), id2.GetName()) ) {
        return false;
    }
    if ( id1.IsSetVersion() &&
         id1.GetVersion() != id2.GetVersion() ) {
        return false;
    }
    if ( id1.IsSetRelease() &&
         id1.GetRelease() != id2.GetRelease() ) {
        return false;
    }
    return true;
}


CSeq_id_Info*
CSeq_id_Textseq_Tree::x_FindVersionEqual(const TVersions& ver_list,
                                         CSeq_id::E_Choice type,
                                         const CTextseq_id& tid) const
{
    ITERATE(TVersions, vit, ver_list) {
        CConstRef<CSeq_id> id = (*vit)->GetSeqId();
        if ( id->Which() == type && x_Equals(tid, x_Get(*id)) ) {
            return *vit;
        }
    }
    return 0;
}


CSeq_id_Info* CSeq_id_Textseq_Tree::x_FindInfo(CSeq_id::E_Choice type,
                                               const CTextseq_id& tid) const
{
    TStringMap::const_iterator it;
    if ( tid.IsSetAccession() ) {
        it = m_ByAccession.find(tid.GetAccession());
        if (it == m_ByAccession.end()) {
            return 0;
        }
    }
    else if ( tid.IsSetName() ) {
        it = m_ByName.find(tid.GetName());
        if (it == m_ByName.end()) {
            return 0;
        }
    }
    else {
        return 0;
    }
    return x_FindVersionEqual(it->second, type, tid);
}


CSeq_id_Handle CSeq_id_Textseq_Tree::FindInfo(const CSeq_id& id) const
{
    // Note: if a record is found by accession, no name is checked
    // even if it is also set.
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    // Can not compare if no accession given
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(id.Which(), tid));
}

CSeq_id_Handle CSeq_id_Textseq_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(id.Which(), tid);
    if ( !info ) {
        info = CreateInfo(id);

        if ( tid.IsSetAccession() ) {
            TVersions& ver = m_ByAccession[tid.GetAccession()];
            _ASSERT(!x_FindVersionEqual(ver, id.Which(), tid));
            ver.push_back(info);
        }
        if ( tid.IsSetName() ) {
            TVersions& ver = m_ByName[tid.GetName()];
            _ASSERT(!x_FindVersionEqual(ver, id.Which(), tid));
            ver.push_back(info);
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Textseq_Tree::x_Unindex(const CSeq_id_Info* info)
{
    _ASSERT(x_Check(*info->GetSeqId()));
    const CTextseq_id& tid = x_Get(*info->GetSeqId());

    if ( tid.IsSetAccession() ) {
        TStringMap::iterator it =
            m_ByAccession.find(tid.GetAccession());
        if (it != m_ByAccession.end()) {
            NON_CONST_ITERATE(TVersions, vit, it->second) {
                if (*vit == info) {
                    it->second.erase(vit);
                    break;
                }
            }
            if (it->second.empty())
                m_ByAccession.erase(it);
        }
    }
    if ( tid.IsSetName() ) {
        TStringMap::iterator it = m_ByName.find(tid.GetName());
        if (it != m_ByName.end()) {
            NON_CONST_ITERATE(TVersions, vit, it->second) {
                if (*vit == info) {
                    it->second.erase(vit);
                    break;
                }
            }
            if (it->second.empty())
                m_ByName.erase(it);
        }
    }
}


inline
bool x_IsDefaultSwissprotRelease(const CTextseq_id& tid)
{
    return tid.IsSetRelease() &&
        (tid.GetRelease() == "standard"  ||  tid.GetRelease() == "prelim");
}


void CSeq_id_Textseq_Tree::x_FindVersionMatch(const TVersions& ver_list,
                                              const CTextseq_id& tid,
                                              TSeq_id_MatchList& id_list,
                                              bool by_accession,
                                              bool is_swissprot) const
{
    ITERATE(TVersions, vit, ver_list) {
        const CTextseq_id& tst = x_Get(*(*vit)->GetSeqId());
        if ( by_accession ) {
            // acc.ver should match
            _ASSERT(NStr::EqualNocase(tst.GetAccession(), tid.GetAccession()));
            if ( tid.IsSetVersion() ) {
                if ( !tst.IsSetVersion() ||
                     tst.GetVersion() != tid.GetVersion() ) {
                    continue;
                }
            }
        }
        else {
            // name.rel should match
            _ASSERT(NStr::EqualNocase(tst.GetName(), tid.GetName()));
            if ( tst.IsSetAccession() && tid.IsSetAccession() ) {
                continue;
            }
            if ( tid.IsSetRelease() ) {
                if (!is_swissprot  ||  tst.IsSetRelease()  ||
                    !x_IsDefaultSwissprotRelease(tid)) {
                    if ( !tst.IsSetRelease() ||
                         tst.GetRelease() != tid.GetRelease() ) {
                        continue;
                    }
                }
            }
        }
        id_list.insert(CSeq_id_Handle(*vit));
    }
}


bool CSeq_id_Textseq_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindMatch(const CSeq_id_Handle& id,
                                     TSeq_id_MatchList& id_list) const
{
    //_ASSERT(id && id == FindInfo(id.GetSeqId()));
    const CTextseq_id& tid = x_Get(*id.GetSeqId());
    if ( tid.IsSetAccession() ) {
        TReadLockGuard guard(m_TreeLock);
        TStringMap::const_iterator it = m_ByAccession.find(tid.GetAccession());
        if (it != m_ByAccession.end()) {
            x_FindVersionMatch(it->second, tid, id_list, true, false);
        }
    }
    if ( tid.IsSetName() ) {
        TReadLockGuard guard(m_TreeLock);
        TStringMap::const_iterator it = m_ByName.find(tid.GetName());
        if (it != m_ByName.end()) {
            x_FindVersionMatch(it->second, tid, id_list, false,
                id.GetSeqId()->IsSwissprot());
        }
    }
}


void CSeq_id_Textseq_Tree::FindMatchStr(string sid,
                                        TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    // ignore '.' in the search string - cut it out
    sid = sid.substr(0, sid.find('.'));
    // Find by accession
    TStringMap::const_iterator it = m_ByAccession.find(sid);
    if (it == m_ByAccession.end()) {
        it = m_ByName.find(sid);
        if (it == m_ByName.end())
            return;
    }
    ITERATE(TVersions, vit, it->second) {
        id_list.insert(CSeq_id_Handle(*vit));
    }
}


bool CSeq_id_Textseq_Tree::Match(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const
{
    return CSeq_id_Which_Tree::Match(h1, h2);
}


bool CSeq_id_Textseq_Tree::IsBetterVersion(const CSeq_id_Handle& h1,
                                           const CSeq_id_Handle& h2) const
{
    CConstRef<CSeq_id> id1 = h1.GetSeqId();
    _ASSERT(x_Check(*id1));
    CConstRef<CSeq_id> id2 = h2.GetSeqId();
    _ASSERT(x_Check(*id2));
    const CTextseq_id& tid1 = x_Get(*id1);
    const CTextseq_id& tid2 = x_Get(*id2);
    // Compare versions. If only one of the two ids has version,
    // consider it is better.
    if ( tid1.IsSetVersion() ) {
        if ( tid2.IsSetVersion() )
            return tid1.GetVersion() > tid2.GetVersion();
        else
            return true; // Only h1 has version
    }
    return false; // h1 has no version, so it can not be better than h2
}


bool CSeq_id_Textseq_Tree::HaveReverseMatch(const CSeq_id_Handle&) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                            TSeq_id_MatchList& id_list)
{
    id_list.insert(id);

    CConstRef<CSeq_id> orig_id = id.GetSeqId();
    const CTextseq_id& orig_tid = x_Get(*orig_id);

    CRef<CSeq_id> tmp(new CSeq_id);
    tmp->Assign(*orig_id);
    CTextseq_id& tid = const_cast<CTextseq_id&>(x_Get(*tmp));
    bool A = orig_tid.IsSetAccession();
    bool N = orig_tid.IsSetName();
    bool v = orig_tid.IsSetVersion();
    bool r = orig_tid.IsSetRelease();

    if ( A  &&  (N  ||  v  ||  r) ) {
        // A only
        tid.Reset();
        tid.SetAccession(orig_tid.GetAccession());
        id_list.insert(FindOrCreate(*tmp));
        if ( v  &&  (N  ||  r) ) {
            // A.v
            tid.SetVersion(orig_tid.GetVersion());
            id_list.insert(FindOrCreate(*tmp));
        }
        if ( N ) {
            // Collect all alternative names
            set<string> alt_names;
            TSeq_id_MatchList name_matches;
            FindMatch(id, name_matches);
            ITERATE(TSeq_id_MatchList, mit, name_matches) {
                CConstRef<CSeq_id> mid = mit->GetSeqId();
                const CTextseq_id& tmid = x_Get(*mid);
                if ( tmid.IsSetName() ) {
                    alt_names.insert(tmid.GetName());
                }
            }

            ITERATE(set<string>, name_it, alt_names) {
                // N only
                tid.Reset();
                tid.SetName(*name_it);
                id_list.insert(FindOrCreate(*tmp));
                if ( v  ||  r ) {
                    if ( r ) {
                        // N.r
                        tid.SetRelease(orig_tid.GetRelease());
                        id_list.insert(FindOrCreate(*tmp));
                        tid.ResetRelease();
                    }
                    // A + N
                    tid.SetAccession(orig_tid.GetAccession());
                    id_list.insert(FindOrCreate(*tmp));
                    if ( v  &&  r ) {
                        // A.v + N
                        tid.SetVersion(orig_tid.GetVersion());
                        id_list.insert(FindOrCreate(*tmp));
                        // A + N.r
                        tid.ResetVersion();
                        tid.SetRelease(orig_tid.GetRelease());
                        id_list.insert(FindOrCreate(*tmp));
                    }
                    else if ( v  &&  *name_it != orig_tid.GetName() ) {
                        tid.SetVersion(orig_tid.GetVersion());
                        id_list.insert(FindOrCreate(*tmp));
                    }
                }
            }
        }
    }
    else if ( N  &&  (v  ||  r) ) {
        // N only
        tid.Reset();
        tid.SetName(orig_tid.GetName());
        id_list.insert(FindOrCreate(*tmp));
        if ( v  &&  r ) {
            tid.SetRelease(orig_tid.GetRelease());
            id_list.insert(FindOrCreate(*tmp));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_GB_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_GB_Tree::CSeq_id_GB_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_GB_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGenbank()  ||  id.IsEmbl()  ||  id.IsDdbj();
}


const CTextseq_id& CSeq_id_GB_Tree::x_Get(const CSeq_id& id) const
{
    switch ( id.Which() ) {
    case CSeq_id::e_Genbank:
        return id.GetGenbank();
    case CSeq_id::e_Embl:
        return id.GetEmbl();
    case CSeq_id::e_Ddbj:
        return id.GetDdbj();
    default:
        NCBI_THROW(CIdMapperException, eTypeError, "Invalid seq-id type");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Pir_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Pir_Tree::CSeq_id_Pir_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Pir_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsPir();
}


const CTextseq_id& CSeq_id_Pir_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetPir();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Swissprot_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Swissprot_Tree::CSeq_id_Swissprot_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Swissprot_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsSwissprot();
}


const CTextseq_id& CSeq_id_Swissprot_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetSwissprot();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Prf_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Prf_Tree::CSeq_id_Prf_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Prf_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsPrf();
}


const CTextseq_id& CSeq_id_Prf_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetPrf();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpg_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpg_Tree::CSeq_id_Tpg_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Tpg_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsTpg();
}


const CTextseq_id& CSeq_id_Tpg_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetTpg();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpe_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpe_Tree::CSeq_id_Tpe_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Tpe_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsTpe();
}


const CTextseq_id& CSeq_id_Tpe_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetTpe();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Tpd_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Tpd_Tree::CSeq_id_Tpd_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Tpd_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsTpd();
}


const CTextseq_id& CSeq_id_Tpd_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetTpd();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gpipe_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Gpipe_Tree::CSeq_id_Gpipe_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Gpipe_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsGpipe();
}


const CTextseq_id& CSeq_id_Gpipe_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGpipe();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Other_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Other_Tree::CSeq_id_Other_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Other_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsOther();
}


const CTextseq_id& CSeq_id_Other_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetOther();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Local_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Local_Tree::CSeq_id_Local_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Local_Tree::~CSeq_id_Local_Tree(void)
{
}


bool CSeq_id_Local_Tree::Empty(void) const
{
    return m_ByStr.empty() && m_ById.empty();
}


CSeq_id_Info* CSeq_id_Local_Tree::x_FindInfo(const CObject_id& oid) const
{
    if ( oid.IsStr() ) {
        TByStr::const_iterator it = m_ByStr.find(oid.GetStr());
        if (it != m_ByStr.end()) {
            return it->second;
        }
    }
    else if ( oid.IsId() ) {
        TById::const_iterator it = m_ById.find(oid.GetId());
        if (it != m_ById.end()) {
            return it->second;
        }
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_Local_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsLocal() );
    const CObject_id& oid = id.GetLocal();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(oid));
}


CSeq_id_Handle CSeq_id_Local_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(id.IsLocal());
    const CObject_id& oid = id.GetLocal();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(oid);

    if ( !info ) {
        info = CreateInfo(id);
        if ( oid.IsStr() ) {
            _VERIFY(m_ByStr.insert(TByStr::value_type(oid.GetStr(),
                                                      info)).second);
        }
        else if ( oid.IsId() ) {
            _VERIFY(m_ById.insert(TById::value_type(oid.GetId(),
                                                    info)).second);
        }
        else {
            NCBI_THROW(CIdMapperException, eEmptyError,
                       "Can not create index for an empty local seq-id");
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Local_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT(id->IsLocal());
    const CObject_id& oid = id->GetLocal();

    if ( oid.IsStr() ) {
        _VERIFY(m_ByStr.erase(oid.GetStr()));
    }
    else if ( oid.IsId() ) {
        _VERIFY(m_ById.erase(oid.GetId()));
    }
}


void CSeq_id_Local_Tree::FindMatchStr(string sid,
                                      TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    // In any case search in strings
    TByStr::const_iterator str_it = m_ByStr.find(sid);
    if (str_it != m_ByStr.end()) {
        id_list.insert(CSeq_id_Handle(str_it->second));
    }
    else {
        try {
            int value = NStr::StringToInt(sid);
            TById::const_iterator int_it = m_ById.find(value);
            if (int_it != m_ById.end()) {
                id_list.insert(CSeq_id_Handle(int_it->second));
            }
        }
        catch (const CStringException& /*ignored*/) {
            // Not an integer value
            return;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Tree::CSeq_id_General_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_General_Tree::~CSeq_id_General_Tree(void)
{
}


bool CSeq_id_General_Tree::Empty(void) const
{
    return m_DbMap.empty();
}


CSeq_id_Info* CSeq_id_General_Tree::x_FindInfo(const CDbtag& dbid) const
{
    TDbMap::const_iterator db = m_DbMap.find(dbid.GetDb());
    if (db == m_DbMap.end())
        return 0;
    const STagMap& tm = db->second;
    const CObject_id& oid = dbid.GetTag();
    if ( oid.IsStr() ) {
        STagMap::TByStr::const_iterator it = tm.m_ByStr.find(oid.GetStr());
        if (it != tm.m_ByStr.end()) {
            return it->second;
        }
    }
    else if ( oid.IsId() ) {
        STagMap::TById::const_iterator it = tm.m_ById.find(oid.GetId());
        if (it != tm.m_ById.end()) {
            return it->second;
        }
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_General_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsGeneral() );
    const CDbtag& dbid = id.GetGeneral();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(dbid));
}


CSeq_id_Handle CSeq_id_General_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsGeneral() );
    const CDbtag& dbid = id.GetGeneral();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(dbid);
    if ( !info ) {
        info = CreateInfo(id);
        STagMap& tm = m_DbMap[dbid.GetDb()];
        const CObject_id& oid = dbid.GetTag();
        if ( oid.IsStr() ) {
            _VERIFY(tm.m_ByStr.insert
                    (STagMap::TByStr::value_type(oid.GetStr(), info)).second);
        }
        else if ( oid.IsId() ) {
            _VERIFY(tm.m_ById.insert(STagMap::TById::value_type(oid.GetId(),
                                                                info)).second);
        }
        else {
            NCBI_THROW(CIdMapperException, eEmptyError,
                       "Can not create index for an empty db-tag");
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_General_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsGeneral() );
    const CDbtag& dbid = id->GetGeneral();

    TDbMap::iterator db_it = m_DbMap.find(dbid.GetDb());
    _ASSERT(db_it != m_DbMap.end());
    STagMap& tm = db_it->second;
    const CObject_id& oid = dbid.GetTag();
    if ( oid.IsStr() ) {
        _VERIFY(tm.m_ByStr.erase(oid.GetStr()));
    }
    else if ( oid.IsId() ) {
        _VERIFY(tm.m_ById.erase(oid.GetId()));
    }
    if (tm.m_ByStr.empty()  &&  tm.m_ById.empty())
        m_DbMap.erase(db_it);
}


void CSeq_id_General_Tree::FindMatchStr(string sid,
                                        TSeq_id_MatchList& id_list) const
{
    int value;
    bool ok;
    try {
        value = NStr::StringToInt(sid);
        ok = true;
    }
    catch (const CStringException&) {
        // Not an integer value
        value = -1;
        ok = false;
    }
    TReadLockGuard guard(m_TreeLock);
    ITERATE(TDbMap, db_it, m_DbMap) {
        // In any case search in strings
        STagMap::TByStr::const_iterator str_it =
            db_it->second.m_ByStr.find(sid);
        if (str_it != db_it->second.m_ByStr.end()) {
            id_list.insert(CSeq_id_Handle(str_it->second));
        }
        if ( ok ) {
            STagMap::TById::const_iterator int_it =
                db_it->second.m_ById.find(value);
            if (int_it != db_it->second.m_ById.end()) {
                id_list.insert(CSeq_id_Handle(int_it->second));
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Giim_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Giim_Tree::CSeq_id_Giim_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Giim_Tree::~CSeq_id_Giim_Tree(void)
{
}


bool CSeq_id_Giim_Tree::Empty(void) const
{
    return m_IdMap.empty();
}


CSeq_id_Info* CSeq_id_Giim_Tree::x_FindInfo(const CGiimport_id& gid) const
{
    TIdMap::const_iterator id_it = m_IdMap.find(gid.GetId());
    if (id_it == m_IdMap.end())
        return 0;
    ITERATE (TGiimList, dbr_it, id_it->second) {
        CConstRef<CSeq_id> id = (*dbr_it)->GetSeqId();
        const CGiimport_id& gid2 = id->GetGiim();
        // Both Db and Release must be equal
        if ( !gid.Equals(gid2) ) {
            return *dbr_it;
        }
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_Giim_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsGiim() );
    const CGiimport_id& gid = id.GetGiim();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(gid));
}


CSeq_id_Handle CSeq_id_Giim_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsGiim() );
    const CGiimport_id& gid = id.GetGiim();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(gid);
    if ( !info ) {
        info = CreateInfo(id);
        m_IdMap[gid.GetId()].push_back(info);
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Giim_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsGiim() );
    const CGiimport_id& gid = id->GetGiim();

    TIdMap::iterator id_it = m_IdMap.find(gid.GetId());
    _ASSERT(id_it != m_IdMap.end());
    TGiimList& giims = id_it->second;
    NON_CONST_ITERATE(TGiimList, dbr_it, giims) {
        if (*dbr_it == info) {
            giims.erase(dbr_it);
            break;
        }
    }
    if ( giims.empty() )
        m_IdMap.erase(id_it);
}


void CSeq_id_Giim_Tree::FindMatchStr(string sid,
                                     TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    try {
        int value = NStr::StringToInt(sid);
        TIdMap::const_iterator it = m_IdMap.find(value);
        if (it == m_IdMap.end())
            return;
        ITERATE(TGiimList, git, it->second) {
            id_list.insert(CSeq_id_Handle(*git));
        }
    }
    catch (CStringException) {
        // Not an integer value
        return;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Patent_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Patent_Tree::CSeq_id_Patent_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Patent_Tree::~CSeq_id_Patent_Tree(void)
{
}


bool CSeq_id_Patent_Tree::Empty(void) const
{
    return m_CountryMap.empty();
}


CSeq_id_Info* CSeq_id_Patent_Tree::x_FindInfo(const CPatent_seq_id& pid) const
{
    const CId_pat& cit = pid.GetCit();
    TByCountry::const_iterator cntry_it = m_CountryMap.find(cit.GetCountry());
    if (cntry_it == m_CountryMap.end())
        return 0;

    const string* number;
    const SPat_idMap::TByNumber* by_number;
    if ( cit.GetId().IsNumber() ) {
        number = &cit.GetId().GetNumber();
        by_number = &cntry_it->second.m_ByNumber;
    }
    else if ( cit.GetId().IsApp_number() ) {
        number = &cit.GetId().GetApp_number();
        by_number = &cntry_it->second.m_ByApp_number;
    }
    else {
        return 0;
    }

    SPat_idMap::TByNumber::const_iterator num_it = by_number->find(*number);
    if (num_it == by_number->end())
        return 0;
    SPat_idMap::TBySeqid::const_iterator seqid_it =
        num_it->second.find(pid.GetSeqid());
    if (seqid_it != num_it->second.end()) {
        return seqid_it->second;
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_Patent_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(pid));
}

CSeq_id_Handle CSeq_id_Patent_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(pid);
    if ( !info ) {
        const CId_pat& cit = pid.GetCit();
        SPat_idMap& country = m_CountryMap[cit.GetCountry()];
        if ( cit.GetId().IsNumber() ) {
            SPat_idMap::TBySeqid& num =
                country.m_ByNumber[cit.GetId().GetNumber()];
            _ASSERT(num.find(pid.GetSeqid()) == num.end());
            info = CreateInfo(id);
            num[pid.GetSeqid()] = info;
        }
        else if ( cit.GetId().IsApp_number() ) {
            SPat_idMap::TBySeqid& app = country.m_ByApp_number[
                cit.GetId().GetApp_number()];
            _ASSERT(app.find(pid.GetSeqid()) == app.end());
            info = CreateInfo(id);
            app[pid.GetSeqid()] = info;
        }
        else {
            // Can not index empty patent number
            NCBI_THROW(CIdMapperException, eEmptyError,
                       "Cannot index empty patent number");
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Patent_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsPatent() );
    const CPatent_seq_id& pid = id->GetPatent();

    TByCountry::iterator country_it =
        m_CountryMap.find(pid.GetCit().GetCountry());
    _ASSERT(country_it != m_CountryMap.end());
    SPat_idMap& pats = country_it->second;
    if ( pid.GetCit().GetId().IsNumber() ) {
        SPat_idMap::TByNumber::iterator num_it =
            pats.m_ByNumber.find(pid.GetCit().GetId().GetNumber());
        _ASSERT(num_it != pats.m_ByNumber.end());
        SPat_idMap::TBySeqid::iterator seqid_it =
            num_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != num_it->second.end());
        _ASSERT(seqid_it->second == info);
        num_it->second.erase(seqid_it);
        if ( num_it->second.empty() )
            pats.m_ByNumber.erase(num_it);
    }
    else if ( pid.GetCit().GetId().IsApp_number() ) {
        SPat_idMap::TByNumber::iterator app_it =
            pats.m_ByApp_number.find(pid.GetCit().GetId().GetApp_number());
        _ASSERT( app_it != pats.m_ByApp_number.end() );
        SPat_idMap::TBySeqid::iterator seqid_it =
            app_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != app_it->second.end());
        _ASSERT(seqid_it->second == info);
        app_it->second.erase(seqid_it);
        if ( app_it->second.empty() )
            pats.m_ByNumber.erase(app_it);
    }
    if (country_it->second.m_ByNumber.empty()  &&
        country_it->second.m_ByApp_number.empty())
        m_CountryMap.erase(country_it);
}


void CSeq_id_Patent_Tree::FindMatchStr(string sid,
                                       TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    ITERATE (TByCountry, cit, m_CountryMap) {
        SPat_idMap::TByNumber::const_iterator nit =
            cit->second.m_ByNumber.find(sid);
        if (nit != cit->second.m_ByNumber.end()) {
            ITERATE(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.insert(CSeq_id_Handle(iit->second));
            }
        }
        SPat_idMap::TByNumber::const_iterator ait =
            cit->second.m_ByApp_number.find(sid);
        if (ait != cit->second.m_ByApp_number.end()) {
            ITERATE(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.insert(CSeq_id_Handle(iit->second));
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_PDB_Tree
/////////////////////////////////////////////////////////////////////////////


CSeq_id_PDB_Tree::CSeq_id_PDB_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_PDB_Tree::~CSeq_id_PDB_Tree(void)
{
}


bool CSeq_id_PDB_Tree::Empty(void) const
{
    return m_MolMap.empty();
}


inline string CSeq_id_PDB_Tree::x_IdToStrKey(const CPDB_seq_id& id) const
{
// this is an attempt to follow the undocumented rules of PDB
// ("documented" as code written elsewhere)
    string skey = id.GetMol().Get();
    switch (char chain = (char)id.GetChain()) {
    case '\0': skey += " ";   break;
    case '|':  skey += "VB";  break;
    default:
        if ( islower((unsigned char)chain) ) {
            skey.append(2, (char)toupper((unsigned char)chain));
            break;
        }
        else {
            skey += chain;
            break;
        }
    }
    return skey;
}


CSeq_id_Info* CSeq_id_PDB_Tree::x_FindInfo(const CPDB_seq_id& pid) const
{
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if (mol_it == m_MolMap.end())
        return 0;
    ITERATE(TSubMolList, it, mol_it->second) {
        CConstRef<CSeq_id> id = (*it)->GetSeqId();
        if (pid.Equals(id->GetPdb())) {
            return *it;
        }
    }
    // Not found
    return 0;
}


CSeq_id_Handle CSeq_id_PDB_Tree::FindInfo(const CSeq_id& id) const
{
    _ASSERT( id.IsPdb() );
    const CPDB_seq_id& pid = id.GetPdb();
    TReadLockGuard guard(m_TreeLock);
    return CSeq_id_Handle(x_FindInfo(pid));
}


CSeq_id_Handle CSeq_id_PDB_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsPdb() );
    const CPDB_seq_id& pid = id.GetPdb();
    TWriteLockGuard guard(m_TreeLock);
    CSeq_id_Info* info = x_FindInfo(pid);
    if ( !info ) {
        info = CreateInfo(id);
        TSubMolList& sub = m_MolMap[x_IdToStrKey(id.GetPdb())];
        ITERATE(TSubMolList, sub_it, sub) {
            _ASSERT(!info->GetSeqId()->GetPdb()
                    .Equals((*sub_it)->GetSeqId()->GetPdb()));
        }
        sub.push_back(info);
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_PDB_Tree::x_Unindex(const CSeq_id_Info* info)
{
    CConstRef<CSeq_id> id = info->GetSeqId();
    _ASSERT( id->IsPdb() );
    const CPDB_seq_id& pid = id->GetPdb();

    TMolMap::iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    _ASSERT(mol_it != m_MolMap.end());
    NON_CONST_ITERATE(TSubMolList, it, mol_it->second) {
        if (*it == info) {
            CConstRef<CSeq_id> id2 = (*it)->GetSeqId();
            _ASSERT(pid.Equals(id2->GetPdb()));
            mol_it->second.erase(it);
            break;
        }
    }
    if ( mol_it->second.empty() )
        m_MolMap.erase(mol_it);
}


bool CSeq_id_PDB_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_PDB_Tree::FindMatch(const CSeq_id_Handle& id,
                                 TSeq_id_MatchList& id_list) const
{
    //_ASSERT(id && id == FindInfo(id.GetSeqId()));
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CPDB_seq_id& pid = seq_id->GetPdb();
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if (mol_it == m_MolMap.end())
        return;
    ITERATE(TSubMolList, it, mol_it->second) {
        CConstRef<CSeq_id> seq_id2 = (*it)->GetSeqId();
        const CPDB_seq_id& pid2 = seq_id2->GetPdb();
        // Ignore date if not set in id
        if ( pid.IsSetRel() ) {
            if ( !pid2.IsSetRel()  ||
                !pid.GetRel().Equals(pid2.GetRel()) )
                continue;
        }
        id_list.insert(CSeq_id_Handle(*it));
    }
}


void CSeq_id_PDB_Tree::FindMatchStr(string sid,
                                    TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mit = m_MolMap.find(sid);
    if (mit == m_MolMap.end())
        return;
    ITERATE(TSubMolList, sub_it, mit->second) {
        id_list.insert(CSeq_id_Handle(*sub_it));
    }
}


const char* CIdMapperException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eTypeError:   return "eTypeError";
    case eSymbolError: return "eSymbolError";
    case eEmptyError:  return "eEmptyError";
    case eOtherError:  return "eOtherError";
    default:           return CException::GetErrCodeString();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
