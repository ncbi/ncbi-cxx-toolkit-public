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
#include <objects/misc/error_codes.hpp>
#include <corelib/ncbi_param.hpp>
#include "seq_id_tree.hpp"
#include <objects/seq/seq_id_mapper.hpp>


#define NCBI_USE_ERRCODE_X   Objects_SeqIdMap


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//#define NCBI_SLOW_ATOMIC_SWAP
#ifdef NCBI_SLOW_ATOMIC_SWAP
DEFINE_STATIC_FAST_MUTEX(sx_GetSeqIdMutex);
#endif

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


static inline void s_AssignTextseq_id(CTextseq_id& new_tid,
                                      const CTextseq_id& old_tid)
{
    if (old_tid.IsSetAccession()) {
        new_tid.SetAccession(old_tid.GetAccession());
    }
    if (old_tid.IsSetVersion()) {
        new_tid.SetVersion(old_tid.GetVersion());
    }
    if (old_tid.IsSetName()) {
        new_tid.SetName(old_tid.GetName());
    }
    if (old_tid.IsSetRelease()) {
        new_tid.SetRelease(old_tid.GetRelease());
    }
}


CSeq_id_Info* CSeq_id_Which_Tree::CreateInfo(const CSeq_id& id)
{
    CRef<CSeq_id> id_ref(new CSeq_id);
    switch (id.Which()) {
    case CSeq_id::e_Gi:
        id_ref->SetGi(id.GetGi());
        break;

    case CSeq_id::e_Local:
        if ( id.GetLocal().IsStr() ) {
            id_ref->SetLocal().SetStr(id.GetLocal().GetStr());
        }
        else {
            id_ref->SetLocal().SetId(id.GetLocal().GetId());
        }
        break;

    case CSeq_id::e_Other:
        s_AssignTextseq_id(id_ref->SetOther(), id.GetOther());
        break;

    case CSeq_id::e_Genbank:
        s_AssignTextseq_id(id_ref->SetGenbank(), id.GetGenbank());
        break;

    case CSeq_id::e_Embl:
        s_AssignTextseq_id(id_ref->SetEmbl(), id.GetEmbl());
        break;

    case CSeq_id::e_Ddbj:
        s_AssignTextseq_id(id_ref->SetDdbj(), id.GetDdbj());
        break;

    case CSeq_id::e_Gpipe:
        s_AssignTextseq_id(id_ref->SetGpipe(), id.GetGpipe());
        break;

    case CSeq_id::e_Named_annot_track:
        s_AssignTextseq_id(id_ref->SetNamed_annot_track(),
                           id.GetNamed_annot_track());
        break;

    default:
        id_ref->Assign(id);
        break;
    }
    return new CSeq_id_Info(id_ref, m_Mapper);
}


void CSeq_id_Which_Tree::DropInfo(const CSeq_id_Info* info)
{
    TWriteLockGuard guard(m_TreeLock);
    if ( info->GetLockCounter() != 0 ) {
        _ASSERT(info->m_Seq_id_Type != CSeq_id::e_not_set);
        return;
    }
    if ( info->m_Seq_id_Type == CSeq_id::e_not_set ) {
        _ASSERT(info->GetLockCounter() == 0);
        return;
    }
    x_Unindex(info);
    _ASSERT(info->GetLockCounter()==0);
    _ASSERT(info->m_Seq_id_Type != CSeq_id::e_not_set);
    const_cast<CSeq_id_Info*>(info)->m_Seq_id_Type = CSeq_id::e_not_set;
}


CSeq_id_Handle CSeq_id_Which_Tree::GetGiHandle(TGi /*gi*/)
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
    v[CSeq_id::e_Named_annot_track].Reset(new CSeq_id_Named_annot_track_Tree(mapper));
}


static const size_t kMallocOverhead = 2*sizeof(void*);

static size_t sx_StringMemory(const string& s)
{
    size_t size = s.capacity();
    if ( size ) {
        if ( size + sizeof(void*) > sizeof(string) ) {
            // ref-counted
            size += sizeof(void*) + kMallocOverhead;
        }
    }
    return size;
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
    return null;
}


CSeq_id_Handle CSeq_id_not_set_Tree::FindOrCreate(const CSeq_id& /*id*/)
{
    return null;
}


void CSeq_id_not_set_Tree::FindMatch(const CSeq_id_Handle& /*id*/,
                                     TSeq_id_MatchList& /*id_list*/) const
{
    LOG_POST_X(3, Warning << "CSeq_id_Mapper::GetMatchingHandles() -- "
               "uninitialized seq-id");
}


void CSeq_id_not_set_Tree::FindMatchStr(const string& /*sid*/,
                                        TSeq_id_MatchList& /*id_list*/) const
{
}


void CSeq_id_not_set_Tree::FindReverseMatch(const CSeq_id_Handle& /*id*/,
                                            TSeq_id_MatchList& /*id_list*/)
{
    LOG_POST_X(4, Warning << "CSeq_id_Mapper::GetReverseMatchingHandles() -- "
               "uninitialized seq-id");
}


size_t CSeq_id_not_set_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
        out << "virtual, no memory" << endl;
    }
    return 0;
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
    TPacked value = x_Get(id);

    TReadLockGuard guard(m_TreeLock);
    TIntMap::const_iterator it = m_IntMap.find(value);
    if (it != m_IntMap.end()) {
        return CSeq_id_Handle(it->second);
    }
    return null;
}


CSeq_id_Handle CSeq_id_int_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    TPacked value = x_Get(id);

    TWriteLockGuard guard(m_TreeLock);
    pair<TIntMap::iterator, bool> ins =
        m_IntMap.insert(TIntMap::value_type(value, nullptr));
    if ( ins.second ) {
        ins.first->second = CreateInfo(id);
    }
    return CSeq_id_Handle(ins.first->second);
}


void CSeq_id_int_Tree::x_Unindex(const CSeq_id_Info* info)
{
    _ASSERT(x_Check(*info->GetSeqId()));
    TPacked value = x_Get(*info->GetSeqId());

    _VERIFY(m_IntMap.erase(value));
}


void CSeq_id_int_Tree::FindMatchStr(const string& sid,
                                    TSeq_id_MatchList& id_list) const
{
    TPacked value;
    try {
        value = NStr::StringToNumeric<TPacked>(sid);
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


size_t CSeq_id_int_Tree::Dump(CNcbiOstream& out,
                              CSeq_id::E_Choice type,
                              int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = m_IntMap.size(), elem_size = 0, extra_size = 0;
    if ( count ) {
        elem_size = sizeof(int)+sizeof(void*); // map value
        elem_size += sizeof(int)+3*sizeof(void*); // red/black tree overhead
        elem_size += sizeof(CSeq_id_Info); //
        elem_size += sizeof(CSeq_id); //
        // malloc overhead:
        // map value, CSeq_id_Info, CSeq_id
        elem_size += 3*kMallocOverhead;
    }
    size_t bytes = count*elem_size+extra_size;
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TIntMap, it, m_IntMap ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
    }
    return total_bytes;
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


CSeq_id_Gibbsq_Tree::TPacked CSeq_id_Gibbsq_Tree::x_Get(const CSeq_id& id) const
{
    return INT_ID_FROM(CSeq_id::TGibbsq, id.GetGibbsq());
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


CSeq_id_Gibbmt_Tree::TPacked CSeq_id_Gibbmt_Tree::x_Get(const CSeq_id& id) const
{
    return INT_ID_FROM(CSeq_id::TGibbmt, id.GetGibbmt());
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Gi_Tree
/////////////////////////////////////////////////////////////////////////////


CConstRef<CSeq_id> CSeq_id_Gi_Info::GetPackedSeqId(TPacked gi) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_Gi_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<TThis*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id = ret;
#else
    const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    const_cast<CSeq_id&>(*ret).SetGi(gi);
    return ret;
}


CSeq_id_Gi_Tree::CSeq_id_Gi_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper),
      m_SharedInfo(new CSeq_id_Gi_Info(m_Mapper))
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
TGi CSeq_id_Gi_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetGi();
}


void CSeq_id_Gi_Tree::DropInfo(const CSeq_id_Info* /*info*/)
{
}


void CSeq_id_Gi_Tree::x_Unindex(const CSeq_id_Info* /*info*/)
{
}


CSeq_id_Handle CSeq_id_Gi_Tree::GetGiHandle(TGi gi)
{
    if ( gi != ZERO_GI ) {
        return CSeq_id_Handle(m_SharedInfo, gi);
    }
    else {
        if ( !m_ZeroInfo ) {
            TWriteLockGuard guard(m_TreeLock);
            if ( !m_ZeroInfo ) {
                CRef<CSeq_id> zero_id(new CSeq_id);
                zero_id->SetGi(ZERO_GI);
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
    TPacked gi = GI_TO(TPacked, x_Get(id));
    if (gi) {
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


void CSeq_id_Gi_Tree::FindMatchStr(const string& sid,
                                   TSeq_id_MatchList& id_list) const
{
    TPacked gi;
    try {
        gi = NStr::StringToNumeric<TPacked>(sid);
    }
    catch (const CStringException& /*ignored*/) {
        // Not an integer value
        return;
    }
    if (gi) {
        id_list.insert(CSeq_id_Handle(m_SharedInfo, gi));
    }
    else if ( m_ZeroInfo ) {
        id_list.insert(CSeq_id_Handle(m_ZeroInfo));
    }
}


size_t CSeq_id_Gi_Tree::Dump(CNcbiOstream& out,
                             CSeq_id::E_Choice type,
                             int details) const
{
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
        out << "virtual, small constant memory";
        out << endl;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Textseq_Tree
/////////////////////////////////////////////////////////////////////////////


NCBI_PARAM_DECL(bool, OBJECTS, PACK_TEXTID);
NCBI_PARAM_DEF_EX(bool, OBJECTS, PACK_TEXTID, true,
                  eParam_NoThread, OBJECTS_PACK_TEXTID);
static const bool s_PackTextid =
    NCBI_PARAM_TYPE(OBJECTS, PACK_TEXTID)::GetDefault();

NCBI_PARAM_DECL(bool, OBJECTS, PACK_GENERAL);
NCBI_PARAM_DEF_EX(bool, OBJECTS, PACK_GENERAL, true,
                  eParam_NoThread, OBJECTS_PACK_GENERAL);
static const bool s_PackGeneral =
    NCBI_PARAM_TYPE(OBJECTS, PACK_GENERAL)::GetDefault();


static inline
void s_RestoreNumber(string& str, size_t pos, size_t len, TIntId number)
{
    char* start = &str[pos];
    char* ptr = start + len;
    while ( number ) {
        *--ptr = '0' + number % 10;
        number /= 10;
    }
    while ( ptr > start ) {
        *--ptr = '0';
    }
}

static inline
TIntId s_ParseNumber(const string& str, size_t pos, size_t len)
{
    TIntId number = 0;
    for ( size_t i = pos; i < pos+len; ++i ) {
        number = number * 10 + (str[i]-'0');
    }
    return number;
}


CSeq_id_Textseq_Info::CSeq_id_Textseq_Info(CSeq_id::E_Choice type,
                                           CSeq_id_Mapper* mapper,
                                           const TKey& key)
    : CSeq_id_Info(type, mapper),
      m_Key(key)
{
}


CSeq_id_Textseq_Info::~CSeq_id_Textseq_Info(void)
{
}


CSeq_id_Textseq_Info::TKey
CSeq_id_Textseq_Info::ParseAcc(const string& acc,
                               const CTextseq_id* tid)
{
    TKey key;
    int len = acc.size(), prefix_len = len, most_significant = -1;
    while ( prefix_len ) {
        char c = acc[--prefix_len];
        if ( c >= '1' && c <= '9' ) {
            most_significant = prefix_len;
        }
        else if ( c != '0' ) {
            ++prefix_len;
            break;
        }
    }
    if ( most_significant < 0 ) {
        return key;
    }
    int acc_digits = len - prefix_len, real_digits = len - most_significant;
    if ( acc_digits < 2 || acc_digits > 12 ||
         real_digits > 9 || acc_digits*2 < prefix_len ) {
        return key;
    }
    if ( prefix_len <= 4 ) {
        // good
    }
    else if ( prefix_len == 3 ) {
        if ( (acc[0] != 'N' && acc[0] != 'Y') ||
             (acc[1] != 'P' && acc[1] != 'C') ||
             (acc[2] != '_') ) {
            return key;
        }
    }
    else {
        return key;
    }
    if ( acc_digits > 6 && real_digits < acc_digits ) {
        acc_digits = max(6, real_digits);
        prefix_len = len - acc_digits;
    }
    key.m_Prefix = acc.substr(0, prefix_len);
    unsigned hash = 0;
    for ( int i = 0; i < 3 && i < prefix_len; ++i ) {
        hash = (hash << 8) | toupper(key.m_Prefix[i] & 0xff);
    }
    hash = (hash << 8) | (acc_digits << 1);
    key.m_Hash = hash;
    if ( tid && tid->IsSetVersion() ) {
        key.SetVersion(tid->GetVersion());
    }
    return key;
}


void CSeq_id_Textseq_Info::Restore(CTextseq_id& id, TPacked param) const
{
    if ( !id.IsSetAccession() ) {
        id.SetAccession(GetAccPrefix());
        string& acc = id.SetAccession();
        acc.resize(acc.size() + GetAccDigits(), '0');
        if ( IsSetVersion() ) {
            id.SetVersion(GetVersion());
        }
    }
    s_RestoreNumber(id.SetAccession(),
                    GetAccPrefix().size(), GetAccDigits(), param);
}


inline
CSeq_id_Textseq_Info::TPacked CSeq_id_Textseq_Info::ParseAccNumber(const string& acc) const
{
    return s_ParseNumber(acc, GetAccPrefix().size(), GetAccDigits());
}


inline
CSeq_id_Textseq_Info::TPacked CSeq_id_Textseq_Info::Pack(const CTextseq_id& tid) const
{
    return ParseAccNumber(tid.GetAccession());
}


CConstRef<CSeq_id> CSeq_id_Textseq_Info::GetPackedSeqId(TPacked param) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_Textseq_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<TThis*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id = ret;
#else
    const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    // split accession number and version
    const_cast<CSeq_id&>(*ret).Select(GetType(), eDoNotResetVariant);
    Restore(*const_cast<CTextseq_id*>(ret->GetTextseq_Id()), param);
    return ret;
}


CSeq_id_Textseq_Tree::CSeq_id_Textseq_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Which_Tree(mapper)
{
}


CSeq_id_Textseq_Tree::~CSeq_id_Textseq_Tree(void)
{
}


bool CSeq_id_Textseq_Tree::Empty(void) const
{
    return m_ByName.empty() && m_ByAcc.empty() && m_PackedMap.empty();
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


CSeq_id_Info* CSeq_id_Textseq_Tree::x_FindStrInfo(const TStringMap& str_map,
                                                  const string& str,
                                                  CSeq_id::E_Choice type,
                                                  const CTextseq_id& tid) const
{
    for ( TStringMapCI vit = str_map.find(str);
          vit != str_map.end() && NStr::EqualNocase(vit->first, str);
          ++vit ) {
        CConstRef<CSeq_id> id = vit->second->GetSeqId();
        if ( id->Which() == type && x_Equals(tid, x_Get(*id)) ) {
            return vit->second;
        }
    }
    return 0;
}


inline
CSeq_id_Info* CSeq_id_Textseq_Tree::x_FindStrInfo(CSeq_id::E_Choice type,
                                                  const CTextseq_id& tid) const
{
    if ( tid.IsSetAccession() ) {
        return x_FindStrInfo(m_ByAcc, tid.GetAccession(), type, tid);
    }
    else if ( tid.IsSetName() ) {
        return x_FindStrInfo(m_ByName, tid.GetName(), type, tid);
    }
    else {
        return 0;
    }
}


CSeq_id_Handle CSeq_id_Textseq_Tree::FindInfo(const CSeq_id& id) const
{
    // Note: if a record is found by accession, no name is checked
    // even if it is also set.
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    // Can not compare if no accession given
    TReadLockGuard guard(m_TreeLock);
    if ( s_PackTextid &&
         tid.IsSetAccession() && !tid.IsSetName() && !tid.IsSetRelease() ) {
        TPackedKey key =
            CSeq_id_Textseq_Info::ParseAcc(tid.GetAccession(), &tid);
        if ( key ) {
            TPackedMap_CI it = m_PackedMap.find(key);
            if ( it == m_PackedMap.end() ) {
                return null;
            }
            return CSeq_id_Handle(it->second, it->second->Pack(tid));
        }
    }
    return CSeq_id_Handle(x_FindStrInfo(id.Which(), tid));
}

CSeq_id_Handle CSeq_id_Textseq_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT(x_Check(id));
    const CTextseq_id& tid = x_Get(id);
    TWriteLockGuard guard(m_TreeLock);
    if ( s_PackTextid &&
         tid.IsSetAccession() && !tid.IsSetName() && !tid.IsSetRelease() ) {
        TPackedKey key =
            CSeq_id_Textseq_Info::ParseAcc(tid.GetAccession(), &tid);
        if ( key ) {
            TPackedMap_I it = m_PackedMap.lower_bound(key);
            if ( it == m_PackedMap.end() ||
                 m_PackedMap.key_comp()(key, it->first) ) {
                CConstRef<CSeq_id_Textseq_Info> info
                    (new CSeq_id_Textseq_Info(id.Which(), m_Mapper, key));
                it = m_PackedMap.insert(it, TPackedMapValue(key, info));
            }
            return CSeq_id_Handle(it->second, it->second->Pack(tid));
        }
    }
    CSeq_id_Info* info = x_FindStrInfo(id.Which(), tid);
    if ( !info ) {
        info = CreateInfo(id);
        if ( tid.IsSetAccession() ) {
            m_ByAcc.insert(TStringMapValue(tid.GetAccession(), info));
        }
        if ( tid.IsSetName() ) {
            m_ByName.insert(TStringMapValue(tid.GetName(), info));
        }
    }
    return CSeq_id_Handle(info);
}


void CSeq_id_Textseq_Tree::x_Erase(TStringMap& str_map,
                                   const string& key,
                                   const CSeq_id_Info* info)
{
    for ( TStringMap::iterator it = str_map.find(key);
          it != str_map.end() && NStr::EqualNocase(it->first, key);
          ++it ) {
        if ( it->second == info ) {
            str_map.erase(it);
            return;
        }
    }
}


void CSeq_id_Textseq_Tree::x_Unindex(const CSeq_id_Info* info)
{
    if ( !m_PackedMap.empty() ) {
        const CSeq_id_Textseq_Info* sinfo =
            dynamic_cast<const CSeq_id_Textseq_Info*>(info);
        if ( sinfo ) {
            m_PackedMap.erase(sinfo->GetKey());
            return;
        }
    }
    _ASSERT(x_Check(*info->GetSeqId()));
    const CTextseq_id& tid = x_Get(*info->GetSeqId());
    if ( tid.IsSetAccession() ) {
        x_Erase(m_ByAcc, tid.GetAccession(), info);
    }
    if ( tid.IsSetName() ) {
        x_Erase(m_ByName, tid.GetName(), info);
    }
}


static inline
bool x_IsDefaultSwissprotRelease(const CTextseq_id& tid)
{
    return tid.IsSetRelease() &&
        (tid.GetRelease() == "reviewed"  ||  tid.GetRelease() == "unreviewed");
}


void CSeq_id_Textseq_Tree::x_FindStrMatch(TSeq_id_MatchList& id_list,
                                          bool by_accession,
                                          const TStringMap& str_map,
                                          const string& str,
                                          CSeq_id::E_Choice which,
                                          const CTextseq_id* tid) const
{
    for ( TStringMapCI vit = str_map.find(str);
          vit != str_map.end() && NStr::EqualNocase(vit->first, str);
          ++vit ) {
        const CTextseq_id& tst = x_Get(*vit->second->GetSeqId());
        if ( by_accession ) {
            // acc.ver should match
            if ( tid->IsSetVersion() ) {
                if ( !tst.IsSetVersion() ||
                     tst.GetVersion() != tid->GetVersion() ) {
                    continue;
                }
            }
        }
        else {
            // name.rel should match
            if ( tst.IsSetAccession() && tid->IsSetAccession() ) {
                continue;
            }
            if ( tid->IsSetRelease() ) {
                if ( which != CSeq_id::e_Swissprot || tst.IsSetRelease()  ||
                     !x_IsDefaultSwissprotRelease(*tid)) {
                    if ( !tst.IsSetRelease() ||
                         tst.GetRelease() != tid->GetRelease() ) {
                        continue;
                    }
                }
            }
        }
        id_list.insert(CSeq_id_Handle(vit->second));
    }
}


void CSeq_id_Textseq_Tree::x_FindMatchByAcc(TSeq_id_MatchList& id_list,
                                            const string& str,
                                            CSeq_id::E_Choice which,
                                            const CTextseq_id* tid) const
{
    if ( !m_PackedMap.empty() ) {
        if ( TPackedKey key = CSeq_id_Textseq_Info::ParseAcc(str, tid) ) {
            if ( key.IsSetVersion() ) {
                // only same version
                TPackedMap_CI it = m_PackedMap.find(key);
                if ( it != m_PackedMap.end() ) {
                    const CSeq_id_Textseq_Info* info = it->second;
                    TPacked packed = info->ParseAccNumber(str);
                    id_list.insert(CSeq_id_Handle(info, packed));
                }
            }
            else {
                // all versions
                TPacked packed = 0;
                for ( TPackedMap_CI it = m_PackedMap.lower_bound(key);
                      it != m_PackedMap.end() && it->first.SameHashNoVer(key);
                      ++it ) {
                    if ( it->first.EqualAcc(key) ) {
                        const CSeq_id_Textseq_Info* info = it->second;
                        if ( packed == 0 ) {
                            packed = info->ParseAccNumber(str);
                        }
                        _ASSERT(packed == info->ParseAccNumber(str));
                        id_list.insert(CSeq_id_Handle(info, packed));
                    }
                }
            }
        }
    }
    x_FindStrMatch(id_list, true, m_ByAcc, str, which, tid);
}


inline
void CSeq_id_Textseq_Tree::x_FindMatchByName(TSeq_id_MatchList& id_list,
                                             const string& str,
                                             CSeq_id::E_Choice which,
                                             const CTextseq_id* tid) const
{
    x_FindStrMatch(id_list, false, m_ByName, str, which, tid);
}


bool CSeq_id_Textseq_Tree::HaveMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindMatch(const CSeq_id_Handle& id,
                                     TSeq_id_MatchList& id_list) const
{
    id_list.insert(id);
    TReadLockGuard guard(m_TreeLock);
    if ( id.IsPacked() ) {
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( !info->IsSetVersion() ) {
            // add all known versions
            const TPackedKey& key = info->GetKey();
            for ( TPackedMap_CI it = m_PackedMap.lower_bound(key);
                  it != m_PackedMap.end() && it->first.SameHashNoVer(key);
                  ++it ) {
                if ( it->first.EqualAcc(key) ) {
                    const CSeq_id_Textseq_Info* info = it->second;
                    id_list.insert(CSeq_id_Handle(info, id.GetPacked()));
                }
            }
        }
        if ( !m_ByAcc.empty() ) {
            TStringMapCI it = m_ByAcc.lower_bound(info->GetAccPrefix());
            if ( it != m_ByAcc.end() &&
                 NStr::StartsWith(it->first, info->GetAccPrefix(),
                                  NStr::eNocase) ) {
                // have similar accessions
                CTextseq_id tid;
                info->Restore(tid, id.GetPacked());
                x_FindMatchByAcc(id_list, tid.GetAccession(), id.Which(), &tid);
            }
        }
    }
    else {
        const CTextseq_id& tid = x_Get(*id.GetSeqId());
        if ( tid.IsSetAccession() ) {
            x_FindMatchByAcc(id_list, tid.GetAccession(), id.Which(), &tid);
        }
        if ( tid.IsSetName() ) {
            x_FindMatchByName(id_list, tid.GetName(), id.Which(), &tid);
        }
    }
}


void CSeq_id_Textseq_Tree::FindMatchStr(const string& sid,
                                        TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    // ignore '.' in the search string - cut it out
    SIZE_TYPE dot = sid.find('.');
    if ( dot != NPOS ) {
        string acc = sid.substr(0, dot);
        x_FindMatchByAcc(id_list, acc, CSeq_id::e_not_set, 0);
        x_FindMatchByName(id_list, acc, CSeq_id::e_not_set, 0);
    }
    else {
        x_FindMatchByAcc(id_list, sid, CSeq_id::e_not_set, 0);
        x_FindMatchByName(id_list, sid, CSeq_id::e_not_set, 0);
    }
}


bool CSeq_id_Textseq_Tree::Match(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const
{
    return CSeq_id_Which_Tree::Match(h1, h2);
}


inline
bool CSeq_id_Textseq_Tree::x_GetVersion(int& version,
                                        const CSeq_id_Handle& id) const
{
    if ( id.IsPacked() ) {
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( !info->IsSetVersion() ) {
            version = 0;
            return false;
        }
        version = info->GetVersion();
        return true;
    }
    else {
        CConstRef<CSeq_id> id1 = id.GetSeqId();
        const CTextseq_id* tid1 = id1->GetTextseq_Id();
        if ( !tid1->IsSetVersion() ) {
            version = 0;
            return false;
        }
        version = tid1->GetVersion();
        return true;
    }
}


bool CSeq_id_Textseq_Tree::IsBetterVersion(const CSeq_id_Handle& h1,
                                           const CSeq_id_Handle& h2) const
{
    // Compare versions. If only one of the two ids has version,
    // consider it as better.
    int version1, version2;
    return x_GetVersion(version1, h1) &&
        (!x_GetVersion(version2, h2) || version1 > version2);
}


bool CSeq_id_Textseq_Tree::HaveReverseMatch(const CSeq_id_Handle&) const
{
    return true;
}


void CSeq_id_Textseq_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                            TSeq_id_MatchList& id_list)
{
    id_list.insert(id);
    if ( id.IsPacked() ) {
        const CSeq_id_Textseq_Info* info =
            static_cast<const CSeq_id_Textseq_Info*>(GetInfo(id));
        if ( info->IsSetVersion() ) {
            TPackedKey key = info->GetKey();
            key.ResetVersion();
            TPackedMap_CI it = m_PackedMap.find(key);
            if ( it != m_PackedMap.end() ) {
                id_list.insert(CSeq_id_Handle(it->second, id.GetPacked()));
            }
        }
        return;
    }

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


size_t CSeq_id_Textseq_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{
        size_t size = m_ByAcc.size() + m_ByName.size();
        size_t elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(string)+sizeof(void*); // map value
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CTextseq_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CTextseq_id
            elem_size += 4*kMallocOverhead;
            ITERATE ( TStringMap, it, m_ByAcc ) {
                const CTextseq_id& id =
                    *it->second->GetSeqId()->GetTextseq_Id();
                extra_size += sx_StringMemory(id.GetAccession());
                if ( id.IsSetName() ) {
                    extra_size += sx_StringMemory(id.GetName());
                }
                if ( id.IsSetRelease() ) {
                    extra_size += sx_StringMemory(id.GetRelease());
                }
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<size << " handles, "<<bytes<<" bytes"<<endl;
        }
    }}
    {{
        size_t size = m_PackedMap.size(), elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(TPackedKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Textseq_Info); //
            // malloc overhead:
            // map value, CSeq_id_Textseq_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedMap, it, m_PackedMap ) {
                extra_size += sx_StringMemory(it->first.m_Prefix);
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " " <<size << " packed handles, "<<bytes<<" bytes"<<endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TStringMap, it, m_ByAcc ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
        ITERATE ( TPackedMap, it, m_PackedMap ) {
            out << "  packed prefix "
                << it->first.m_Prefix<<"."<<it->first.m_Version << endl;
        }
    }
    return total_bytes;
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
// CSeq_id_Named_annot_track_Tree
/////////////////////////////////////////////////////////////////////////////

CSeq_id_Named_annot_track_Tree::CSeq_id_Named_annot_track_Tree(CSeq_id_Mapper* mapper)
    : CSeq_id_Textseq_Tree(mapper)
{
}


bool CSeq_id_Named_annot_track_Tree::x_Check(const CSeq_id& id) const
{
    return id.IsNamed_annot_track();
}


const CTextseq_id& CSeq_id_Named_annot_track_Tree::x_Get(const CSeq_id& id) const
{
    return id.GetNamed_annot_track();
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


void CSeq_id_Local_Tree::FindMatchStr(const string& sid,
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
            TPacked value = NStr::StringToNumeric<TPacked>(sid);
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


size_t CSeq_id_Local_Tree::Dump(CNcbiOstream& out,
                                CSeq_id::E_Choice type,
                                int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{
        size_t size = m_ByStr.size(), elem_size = 0, extra_size = 0;
        if ( size ) {
            elem_size = sizeof(string)+sizeof(void*); // map value
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CObject_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CObject_id
            elem_size += 4*kMallocOverhead;
            ITERATE ( TByStr, it, m_ByStr ) {
                extra_size += sx_StringMemory(it->first);
            }
        }
        size_t bytes = extra_size + size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " " <<size << " str handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{
        size_t size = m_ById.size(), elem_size = 0;
        if ( size ) {
            elem_size = sizeof(int)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_Info); //
            elem_size += sizeof(CSeq_id); //
            elem_size += sizeof(CObject_id); //
            // malloc overhead:
            // map value, CSeq_id_Info, CSeq_id, CObject_id
            elem_size += 4*kMallocOverhead;
        }
        size_t bytes = size*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<size << " int handles, "<<bytes<<" bytes" << endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TByStr, it, m_ByStr ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
        ITERATE ( TById, it, m_ById ) {
            out << "  " << it->second->GetSeqId()->AsFastaString() << endl;
        }
    }
    return total_bytes;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Id_Info
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Id_Info::CSeq_id_General_Id_Info(CSeq_id_Mapper* mapper,
                                                 const TKey& key)
    : CSeq_id_Info(CSeq_id::e_General, mapper),
      m_Key(key)
{
}


CSeq_id_General_Id_Info::~CSeq_id_General_Id_Info(void)
{
}


CSeq_id_General_Id_Info::TPacked CSeq_id_General_Id_Info::Pack(const CDbtag& dbtag) const
{
    TPacked id = dbtag.GetTag().GetId();
    if ( id <= 0 ) {
        --id;
    }
    return id;
}


void CSeq_id_General_Id_Info::Restore(CDbtag& dbtag, TPacked param) const
{
    if ( !dbtag.IsSetDb() ) {
        dbtag.SetDb(GetDbtag());
    }
    if ( param < 0 ) {
        ++param;
    }
    dbtag.SetTag().SetId(CObject_id::TId(param));
}


CConstRef<CSeq_id> CSeq_id_General_Id_Info::GetPackedSeqId(TPacked param) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_General_Id_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<TThis*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id = ret;
#else
    const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    Restore(const_cast<CSeq_id&>(*ret).SetGeneral(), param);
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_General_Str_Info
/////////////////////////////////////////////////////////////////////////////


CSeq_id_General_Str_Info::CSeq_id_General_Str_Info(CSeq_id_Mapper* mapper,
                                                 const TKey& key)
    : CSeq_id_Info(CSeq_id::e_General, mapper),
      m_Key(key)
{
}


CSeq_id_General_Str_Info::~CSeq_id_General_Str_Info(void)
{
}


CSeq_id_General_Str_Info::TKey
CSeq_id_General_Str_Info::Parse(const CDbtag& dbtag)
{
    TKey key;
    key.m_Key = 0;
    const string& str = dbtag.GetTag().GetStr();
    int len = str.size(), prefix_len = len, str_digits = 0;
    // find longest digit substring
    int cur_digits = 0;
    for ( int i = len; i >= 0; ) {
        char c = --i < 0? 0: str[i];
        if ( c >= '0' && c <= '9' ) {
            ++cur_digits;
        }
        else {
            if ( !str_digits || cur_digits > str_digits+2 ) {
                str_digits = cur_digits;
                prefix_len = i+1;
            }
            cur_digits = 0;
        }
    }
    if ( str_digits > 9 ) {
        prefix_len += str_digits - 9;
        str_digits = 9;
    }
    key.m_Db = dbtag.GetDb();
    if ( prefix_len > 0 ) {
        key.m_StrPrefix = str.substr(0, prefix_len);
    }
    if ( SIZE_TYPE(prefix_len + str_digits) < str.size() ) {
        key.m_StrSuffix = str.substr(prefix_len+str_digits);
    }
    int hash = 1;
    for ( int i = 0; i < 3 && i < prefix_len; ++i ) {
        hash = (hash << 8) | toupper(key.m_StrPrefix[prefix_len-1-i] & 0xff);
    }
    key.m_Key = (hash << 8) | str_digits;
    return key;
}


inline
CSeq_id_General_Str_Info::TPacked CSeq_id_General_Str_Info::ParseStrNumber(const string& str) const
{
    return s_ParseNumber(str, GetStrPrefix().size(), GetStrDigits());
}


inline
CSeq_id_General_Str_Info::TPacked CSeq_id_General_Str_Info::Pack(const CDbtag& dbtag) const
{
    TPacked id = ParseStrNumber(dbtag.GetTag().GetStr());
    if ( id <= 0 ) {
        --id;
    }
    return id;
}


void CSeq_id_General_Str_Info::Restore(CDbtag& dbtag, TPacked param) const
{
    if ( !dbtag.IsSetDb() ) {
        dbtag.SetDb(GetDbtag());
    }
    CObject_id& obj_id = dbtag.SetTag();
    if ( !obj_id.IsStr() ) {
        obj_id.SetStr(GetStrPrefix());
        string& str = obj_id.SetStr();
        str.resize(str.size() + GetStrDigits(), '0');
        if ( !GetStrSuffix().empty() ) {
            str += GetStrSuffix();
        }
    }
    if ( param < 0 ) {
        ++param;
    }
    s_RestoreNumber(obj_id.SetStr(),
                    GetStrPrefix().size(), GetStrDigits(), param);
}


CConstRef<CSeq_id> CSeq_id_General_Str_Info::GetPackedSeqId(TPacked param) const
{
    CConstRef<CSeq_id> ret;
    typedef CSeq_id_General_Str_Info TThis;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<TThis*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id = ret;
#else
    const_cast<TThis*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<TThis*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    Restore(const_cast<CSeq_id&>(*ret).SetGeneral(), param);
    return ret;
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
    return m_DbMap.empty() && m_PackedIdMap.empty() && m_PackedStrMap.empty();
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
    if ( s_PackGeneral ) {
        switch ( dbid.GetTag().Which() ) {
        case CObject_id::e_Str:
        {
            TPackedStrKey key = CSeq_id_General_Str_Info::Parse(dbid);
            TPackedStrMap::const_iterator it = m_PackedStrMap.find(key);
            if ( it == m_PackedStrMap.end() ) {
                break;
            }
            return CSeq_id_Handle(it->second, it->second->Pack(dbid));
        }
        case CObject_id::e_Id:
        {
            const string& key = dbid.GetDb();
            TPackedIdMap::const_iterator it = m_PackedIdMap.find(key);
            if ( it == m_PackedIdMap.end() ) {
                break;
            }
            return CSeq_id_Handle(it->second, it->second->Pack(dbid));
        }
        default:
            break;
        }
        return null;
    }
    return CSeq_id_Handle(x_FindInfo(dbid));
}


CSeq_id_Handle CSeq_id_General_Tree::FindOrCreate(const CSeq_id& id)
{
    _ASSERT( id.IsGeneral() );
    const CDbtag& dbid = id.GetGeneral();
    TWriteLockGuard guard(m_TreeLock);
    if ( s_PackGeneral ) {
        switch ( dbid.GetTag().Which() ) {
        case CObject_id::e_Str:
        {
            TPackedStrKey key = CSeq_id_General_Str_Info::Parse(dbid);
            TPackedStrMap::iterator it = m_PackedStrMap.lower_bound(key);
            if ( it == m_PackedStrMap.end() ||
                 m_PackedStrMap.key_comp()(key, it->first) ) {
                CConstRef<CSeq_id_General_Str_Info> info
                    (new CSeq_id_General_Str_Info(m_Mapper, key));
                it = m_PackedStrMap.insert
                    (it, TPackedStrMap::value_type(key, info));
            }
            return CSeq_id_Handle(it->second, it->second->Pack(dbid));
        }
        case CObject_id::e_Id:
        {
            const string& key = dbid.GetDb();
            TPackedIdMap::iterator it = m_PackedIdMap.lower_bound(key);
            if ( it == m_PackedIdMap.end() ||
                 !NStr::EqualNocase(it->first, key) ) {
                CConstRef<CSeq_id_General_Id_Info> info
                    (new CSeq_id_General_Id_Info(m_Mapper, key));
                it = m_PackedIdMap.insert
                    (it, TPackedIdMap::value_type(key, info));
            }
            return CSeq_id_Handle(it->second, it->second->Pack(dbid));
        }
        default:
            break;
        }
    }
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
    if ( !m_PackedStrMap.empty() ) {
        const CSeq_id_General_Str_Info* sinfo =
            dynamic_cast<const CSeq_id_General_Str_Info*>(info);
        if ( sinfo ) {
            m_PackedStrMap.erase(sinfo->GetKey());
            return;
        }
    }
    if ( !m_PackedIdMap.empty() ) {
        const CSeq_id_General_Id_Info* sinfo =
            dynamic_cast<const CSeq_id_General_Id_Info*>(info);
        if ( sinfo ) {
            m_PackedIdMap.erase(sinfo->GetKey());
            return;
        }
    }

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


bool CSeq_id_General_Tree::HaveMatch(const CSeq_id_Handle& id) const
{
    return true;
}


void CSeq_id_General_Tree::FindMatch(const CSeq_id_Handle& id,
                                     TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    id_list.insert(id);
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CDbtag& dbtag = seq_id->GetGeneral();
    const CObject_id& obj_id = dbtag.GetTag();
    if ( obj_id.IsId() ) {
        int n = obj_id.GetId();
        if ( n >= 0 ) {
            CSeq_id seq_id2;
            CDbtag& dbtag2 = seq_id2.SetGeneral();
            dbtag2.SetDb(dbtag.GetDb());
            dbtag2.SetTag().SetStr(NStr::IntToString(n));
            CSeq_id_Handle id2 = FindInfo(seq_id2);
            if ( id2 ) {
                id_list.insert(id2);
            }
        }
    }
    else {
        const string& s = obj_id.GetStr();
        int n = NStr::StringToNonNegativeInt(s);
        if ( n >= 0 && NStr::IntToString(n) == s ) {
            CSeq_id seq_id2;
            CDbtag& dbtag2 = seq_id2.SetGeneral();
            dbtag2.SetDb(dbtag.GetDb());
            dbtag2.SetTag().SetId(n);
            CSeq_id_Handle id2 = FindInfo(seq_id2);
            if ( id2 ) {
                id_list.insert(id2);
            }
        }
    }
}


void CSeq_id_General_Tree::FindMatchStr(const string& sid,
                                        TSeq_id_MatchList& id_list) const
{
    TPacked value;
    bool ok;
    try {
        value = NStr::StringToNumeric<TPacked>(sid);
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


size_t CSeq_id_General_Tree::Dump(CNcbiOstream& out,
                                  CSeq_id::E_Choice type,
                                  int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): "<<endl;
    }
    {{ // m_DbMap
        size_t count = 0, bytes = 0;
        ITERATE ( TDbMap, it, m_DbMap ) {
            bytes += sizeof(string)+sizeof(STagMap); // map value
            bytes += sizeof(int)+3*sizeof(void*); // red/black tree
            // malloc overhead:
            // map value
            bytes += 1*kMallocOverhead;
            bytes += sx_StringMemory(it->first);
            ITERATE ( STagMap::TById, it2, it->second.m_ById ) {
                count += 1;
                bytes += sizeof(it2->first)+sizeof(it2->second); // map
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CObject_id);
                bytes += 4*kMallocOverhead;
            }
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                count += 1;
                bytes += sizeof(it2->first)+sizeof(it2->second); // map
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CObject_id);
                bytes += 4*kMallocOverhead;
                bytes += sx_StringMemory(it2->first);
            }
        }
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{ // m_PackedIdMap
        size_t count = m_PackedIdMap.size(), elem_size = 0, extra_size = 0;
        if ( count ) {
            elem_size = sizeof(TPackedIdKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_General_Id_Info); //
            // malloc overhead:
            // map value, CSeq_id_General_Id_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedIdMap, it, m_PackedIdMap ) {
                extra_size += sx_StringMemory(it->first);
            }
        }
        size_t bytes = extra_size + count*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " packed int handles, "<<bytes<<" bytes" << endl;
        }
    }}
    {{ // m_PackedStrMap
        size_t count = m_PackedStrMap.size(), elem_size = 0, extra_size = 0;
        if ( count ) {
            elem_size = sizeof(TPackedIdKey)+sizeof(void*);
            elem_size += sizeof(int)+3*sizeof(void*); // red/black tree
            elem_size += sizeof(CSeq_id_General_Str_Info); //
            // malloc overhead:
            // map value, CSeq_id_General_Id_Info
            elem_size += 2*kMallocOverhead;
            ITERATE ( TPackedStrMap, it, m_PackedStrMap ) {
                extra_size += sx_StringMemory(it->first.m_Db);
                extra_size += sx_StringMemory(it->first.m_StrPrefix);
                extra_size += sx_StringMemory(it->first.m_StrSuffix);
            }
        }
        size_t bytes = extra_size + count*elem_size;
        total_bytes += bytes;
        if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
            out << " "<<count << " packed str handles, "<<bytes<<" bytes" << endl;
        }
    }}
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TDbMap, it, m_DbMap ) {
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                out << "  "<<it2->second->GetSeqId()->AsFastaString() << endl;
            }
            ITERATE ( STagMap::TByStr, it2, it->second.m_ByStr ) {
                out << "  "<<it2->second->GetSeqId()->AsFastaString() << endl;
            }
        }
        ITERATE ( TPackedIdMap, it, m_PackedIdMap ) {
            out << "  packed int "<<it->first << endl;
        }
        ITERATE ( TPackedStrMap, it, m_PackedStrMap ) {
            out << "  packed str "<<it->first.m_Key<<"/"<<it->first.m_Db<<"/"
                <<it->first.m_StrPrefix<<"/"<<it->first.m_StrSuffix << endl;
        }
    }
    return total_bytes;
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


void CSeq_id_Giim_Tree::FindMatchStr(const string& sid,
                                     TSeq_id_MatchList& id_list) const
{
    TReadLockGuard guard(m_TreeLock);
    try {
        TPacked value = NStr::StringToNumeric<TPacked>(sid);
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


size_t CSeq_id_Giim_Tree::Dump(CNcbiOstream& out,
                               CSeq_id::E_Choice type,
                               int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TIdMap, it, m_IdMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 2*kMallocOverhead;
        size_t size2 = it->second.size();
        count += size2;
        bytes += it->second.capacity()*sizeof(void*);
        bytes += size2*sizeof(CSeq_id_Info);
        bytes += size2*sizeof(CSeq_id);
        bytes += size2*sizeof(CGiimport_id);
        ITERATE ( TGiimList, it2, it->second ) {
            const CGiimport_id& id = (*it2)->GetSeqId()->GetGiim();
            if ( id.IsSetDb() ) {
                bytes += sx_StringMemory(id.GetDb());
            }
            if ( id.IsSetRelease() ) {
                bytes += sx_StringMemory(id.GetRelease());
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TIdMap, it, m_IdMap ) {
            ITERATE ( TGiimList, it2, it->second ) {
                out << "  "<<(*it2)->GetSeqId()->AsFastaString() << endl;
            }
        }
    }
    return total_bytes;
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
            pats.m_ByApp_number.erase(app_it);
    }
    if (country_it->second.m_ByNumber.empty()  &&
        country_it->second.m_ByApp_number.empty())
        m_CountryMap.erase(country_it);
}


void CSeq_id_Patent_Tree::FindMatchStr(const string& sid,
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


size_t CSeq_id_Patent_Tree::Dump(CNcbiOstream& out,
                                 CSeq_id::E_Choice type,
                                 int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TByCountry, it, m_CountryMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 1*kMallocOverhead;
        bytes += sx_StringMemory(it->first);
        ITERATE ( SPat_idMap::TByNumber, it2, it->second.m_ByNumber ) {
            bytes += sizeof(it2->first) + sizeof(it2->second);
            bytes += sizeof(int)+3*sizeof(void*); // red/black tree
            // malloc overhead:
            // map value, vector
            bytes += 1*kMallocOverhead;
            bytes += sx_StringMemory(it2->first);
            ITERATE ( SPat_idMap::TBySeqid, it3, it2->second ) {
                count += 1;
                bytes += sizeof(it2->first) + sizeof(it2->second);
                bytes += sizeof(int)+3*sizeof(void*); // red/black tree
                bytes += sizeof(CSeq_id_Info);
                bytes += sizeof(CSeq_id);
                bytes += sizeof(CPatent_seq_id);
                bytes += sizeof(CId_pat);
                // malloc overhead:
                // map value, 
                bytes += 5*kMallocOverhead;
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TByCountry, it, m_CountryMap ) {
            ITERATE ( SPat_idMap::TByNumber, it2, it->second.m_ByNumber ) {
                ITERATE ( SPat_idMap::TBySeqid, it3, it2->second ) {
                    out << "  "<<it3->second->GetSeqId()->AsFastaString() << endl;
                }
            }
        }
    }
    return total_bytes;
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


void CSeq_id_PDB_Tree::FindMatchStr(const string& sid,
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


bool CSeq_id_PDB_Tree::HaveReverseMatch(const CSeq_id_Handle& ) const
{
    return true;
}


void CSeq_id_PDB_Tree::FindReverseMatch(const CSeq_id_Handle& id,
                                        TSeq_id_MatchList& id_list)
{
    //_ASSERT(id && id == FindInfo(id.GetSeqId()));
    id_list.insert(id);
    CConstRef<CSeq_id> seq_id = id.GetSeqId();
    const CPDB_seq_id& pid = seq_id->GetPdb();
    if ( !pid.IsSetRel() )
        return;
    // find id without release date
    TReadLockGuard guard(m_TreeLock);
    TMolMap::const_iterator mol_it = m_MolMap.find(x_IdToStrKey(pid));
    if (mol_it == m_MolMap.end())
        return;
    ITERATE(TSubMolList, it, mol_it->second) {
        CConstRef<CSeq_id> seq_id2 = (*it)->GetSeqId();
        const CPDB_seq_id& pid2 = seq_id2->GetPdb();
        // Ignore date if set in id
        if ( pid2.IsSetRel() )
            continue;
        id_list.insert(CSeq_id_Handle(*it));
    }
}


size_t CSeq_id_PDB_Tree::Dump(CNcbiOstream& out,
                              CSeq_id::E_Choice type,
                              int details) const
{
    size_t total_bytes = 0;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << "CSeq_id_Handles("<<CSeq_id::SelectionName(type)<<"): ";
    }
    size_t count = 0, bytes = 0;
    ITERATE ( TMolMap, it, m_MolMap ) {
        bytes += sizeof(it->first) + sizeof(it->second);
        bytes += sizeof(int)+3*sizeof(void*); // red/black tree
        // malloc overhead:
        // map value, vector
        bytes += 2*kMallocOverhead;
        bytes += sx_StringMemory(it->first);
        size_t size2 = it->second.size();
        count += size2;
        bytes += it->second.capacity()*sizeof(void*);
        bytes += size2*sizeof(CSeq_id_Info);
        bytes += size2*sizeof(CSeq_id);
        bytes += size2*sizeof(CPDB_seq_id);
        ITERATE ( TSubMolList, it2, it->second ) {
            const CPDB_seq_id& id = (*it2)->GetSeqId()->GetPdb();
            if ( id.IsSetRel() ) {
                bytes += sizeof(CDate);
                bytes += kMallocOverhead;
            }
        }
    }
    total_bytes += bytes;
    if ( details >= CSeq_id_Mapper::eDumpStatistics ) {
        out << count << " handles, "<<bytes<<" bytes" << endl;
    }
    if ( details >= CSeq_id_Mapper::eDumpAllIds ) {
        ITERATE ( TMolMap, it, m_MolMap ) {
            ITERATE ( TSubMolList, it2, it->second ) {
                out << "  "<<(*it2)->GetSeqId()->AsFastaString() << endl;
            }
        }
    }
    return total_bytes;
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
