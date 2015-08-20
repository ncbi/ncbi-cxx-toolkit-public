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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SNP files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/snpread.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_param.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <sra/error_codes.hpp>

#include <sra/readers/sra/kdbread.hpp>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   SNPReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);


#define RC_NO_MORE_ALIGNMENTS RC(rcApp, rcQuery, rcSearching, rcRow, rcNotFound)


/////////////////////////////////////////////////////////////////////////////
// CSNPDb_Impl
/////////////////////////////////////////////////////////////////////////////


CSNPDb_Impl::SSNPTableCursor::SSNPTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(FROM),
      INIT_VDB_COLUMN(LEN),
      INIT_VDB_COLUMN(RS_ALLELE),
      INIT_VDB_COLUMN(RS_ALT_ALLELE),
      INIT_VDB_COLUMN(RS_ID),
      INIT_VDB_COLUMN(BIT_FIELD)
{
}


CSNPDb_Impl::CSNPDb_Impl(CVDBMgr& mgr,
                         CTempString path_or_acc)
    : m_Mgr(mgr),
      m_SNPTable(m_Mgr, path_or_acc)
{
    // only one ref seq
    CRef<SSNPTableCursor> snp = SNP();

    SRefInfo info;

    string ref_id = *snp->ACCESSION(1);
    info.m_Name = info.m_SeqId = ref_id;
    info.m_Seq_ids.push_back(Ref(new CSeq_id(ref_id)));
    info.m_Seq_id_Handle = CSeq_id_Handle::GetHandle(*info.GetMainSeq_id());
    info.m_SeqLength = kInvalidSeqPos;
    info.m_Circular = false;
    pair<int64_t, uint64_t> row_range = snp->m_Cursor.GetRowIdRange();
    info.m_RowFirst = row_range.first;
    info.m_RowLast = row_range.first+row_range.second-1;
    m_RefList.push_back(info);
    
    NON_CONST_ITERATE ( TRefInfoList, it, m_RefList ) {
        m_RefMapByName.insert
            (TRefInfoMapByName::value_type(it->m_Name, it));
        ITERATE ( CBioseq::TId, id_it, it->m_Seq_ids ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**id_it);
            m_RefMapBySeq_id.insert
                (TRefInfoMapBySeq_id::value_type(idh, it));
        }
    }

    Put(snp);
}


CSNPDb_Impl::~CSNPDb_Impl(void)
{
}


CRef<CSNPDb_Impl::SSNPTableCursor> CSNPDb_Impl::SNP(void)
{
    CRef<SSNPTableCursor> curs = m_SNP.Get();
    if ( !curs ) {
        curs = new SSNPTableCursor(SNPTable());
    }
    return curs;
}


void CSNPDb_Impl::x_CalcSeqLength(const SRefInfo& info)
{
    /*
    CRef<SRefTableCursor> ref(Ref());
    TSeqPos last_len = *ref->SEQ_LEN(info.m_RowLast);
    info.m_SeqLength =
        GetRowSize()*TSeqPos(info.m_RowLast-info.m_RowFirst)+last_len;
    Put(ref);
    */
    info.m_SeqLength = kInvalidSeqPos-1;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbRefSeqIterator
/////////////////////////////////////////////////////////////////////////////


CSNPDbRefSeqIterator::CSNPDbRefSeqIterator(const CSNPDb& db)
    : m_Db(db),
      m_Iter(db->GetRefInfoList().begin())
{
}


CSNPDbRefSeqIterator::CSNPDbRefSeqIterator(const CSNPDb& db,
                                           const string& name,
                                           EByName)
{
    CSNPDb_Impl::TRefInfoMapByName::const_iterator iter =
        db->m_RefMapByName.find(name);
    if ( iter != db->m_RefMapByName.end() ) {
        m_Db = db;
        m_Iter = iter->second;
    }
}


CSNPDbRefSeqIterator::CSNPDbRefSeqIterator(const CSNPDb& db,
                                           const CSeq_id_Handle& seq_id)
{
    CSNPDb_Impl::TRefInfoMapBySeq_id::const_iterator iter =
        db->m_RefMapBySeq_id.find(seq_id);
    if ( iter != db->m_RefMapBySeq_id.end() ) {
        m_Db = db;
        m_Iter = iter->second;
    }
}


const CSNPDb_Impl::SRefInfo& CSNPDbRefSeqIterator::GetInfo(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSraException, eInvalidState,
                   "CSNPDbRefSeqIterator is invalid");
    }
    return *m_Iter;
}


bool CSNPDbRefSeqIterator::IsCircular(void) const
{
    return GetInfo().m_Circular;
}


TSeqPos CSNPDbRefSeqIterator::GetSeqLength(void) const
{
    const CSNPDb_Impl::SRefInfo& info = **this;
    if ( info.m_SeqLength == kInvalidSeqPos ) {
        GetDb().x_CalcSeqLength(info);
    }
    return info.m_SeqLength;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbIterator
/////////////////////////////////////////////////////////////////////////////


CSNPDbIterator::CSNPDbIterator(void)
    : m_Error(RC_NO_MORE_ALIGNMENTS),
      m_CurSNPId(0),
      m_FirstBadSNPId(0),
      m_SearchMode(eSearchByOverlap)
{
}


CSNPDbIterator::CSNPDbIterator(const CSNPDb& db,
                               const CSeq_id_Handle& ref_id,
                               TSeqPos ref_pos,
                               TSeqPos window,
                               ESearchMode search_mode)
    : m_RefIter(db, ref_id),
      m_Error(RC_NO_MORE_ALIGNMENTS)
{
    COpenRange<TSeqPos> range(ref_pos, window? ref_pos+window: kInvalidSeqPos);
    Select(range, search_mode);
}


CSNPDbIterator::CSNPDbIterator(const CSNPDb& db,
                               const CSeq_id_Handle& ref_id,
                               COpenRange<TSeqPos> range,
                               ESearchMode search_mode)
    : m_RefIter(db, ref_id),
      m_Error(RC_NO_MORE_ALIGNMENTS)
{
    Select(range, search_mode);
}


CSNPDbIterator::~CSNPDbIterator(void)
{
    if ( m_SNP ) {
        GetDb().Put(m_SNP);
    }
}


void CSNPDbIterator::Select(COpenRange<TSeqPos> ref_range,
                            ESearchMode search_mode)
{
    m_Error = RC_NO_MORE_ALIGNMENTS;
    m_ArgRange = ref_range;
    m_SearchMode = search_mode;

    if ( !m_RefIter ) {
        return;
    }
    
    m_SNP = GetDb().SNP();
    pair<int64_t, uint64_t> range = m_SNP->m_Cursor.GetRowIdRange();
    m_CurSNPId = range.first;
    m_FirstBadSNPId = range.first+range.second;

    x_Settle();
}


inline
bool CSNPDbIterator::x_Excluded(void)
{
    TSeqPos ref_pos = *m_SNP->FROM(m_CurSNPId)-1; // one-based
    if ( ref_pos >= m_ArgRange.GetToOpen() ) {
        // no more
        m_FirstBadSNPId = m_CurSNPId;
        return false;
    }
    TSeqPos ref_len = *m_SNP->LEN(m_CurSNPId);
    TSeqPos ref_end = ref_pos + ref_len;
    if ( ref_end <= m_ArgRange.GetFrom() ) {
        return true;
    }
    m_CurRange.SetFrom(ref_pos);
    m_CurRange.SetToOpen(ref_end);
    return false;
}


void CSNPDbIterator::x_Settle(void)
{
    while ( *this && x_Excluded() ) {
        ++m_CurSNPId;
    }
}


void CSNPDbIterator::x_Next(void)
{
    x_CheckValid("CSNPDbIterator::operator++");
    ++m_CurSNPId;
    x_Settle();
}


void CSNPDbIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CSNPDbIterator::"<<method<<"(): Invalid iterator state");
}


CTempString CSNPDbIterator::GetAllele() const
{
    x_CheckValid("CSNPDbIterator::GetAllele");
    return *m_SNP->RS_ALLELE(m_CurSNPId);
}


CTempString CSNPDbIterator::GetAltAllele() const
{
    x_CheckValid("CSNPDbIterator::GetAltAllele");
    return *m_SNP->RS_ALT_ALLELE(m_CurSNPId);
}


Uint4 CSNPDbIterator::GetRsId() const
{
    x_CheckValid("CSNPDbIterator::GetRsId");
    return *m_SNP->RS_ID(m_CurSNPId);
}


void CSNPDbIterator::GetQualityCodes(vector<char>& codes) const
{
    x_CheckValid("CSNPDbIterator::GetQualityCodes");
    CVDBValueFor<Uint1> data = m_SNP->BIT_FIELD(m_CurSNPId);
    size_t size = data.size();
    const char* ptr = reinterpret_cast<const char*>(data.data());
    codes.assign(ptr, ptr+size);
}


template<size_t ValueSize>
static inline
bool x_IsStringConstant(const string& str, const char (&value)[ValueSize])
{
    return str.size() == ValueSize-1 && str == value;
}

#define x_SetStringConstant(obj, Field, value)                          \
    if ( !(obj).NCBI_NAME2(IsSet,Field)() ||                            \
         !x_IsStringConstant((obj).NCBI_NAME2(Get,Field)(), value) ) {  \
        (obj).NCBI_NAME2(Set,Field)((value));                           \
    }


template<class T>
static inline
T& x_GetPrivate(CRef<T>& ref)
{
    T* ptr = ref.GetPointerOrNull();
    if ( !ptr || !ptr->ReferencedOnlyOnce() ) {
        ref = ptr = new T;
    }
    return *ptr;
}


struct CSNPDbIterator::SCreateCache {
    CRef<CSeq_feat> m_Feat;
    CRef<CImp_feat> m_Imp;
    CRef<CSeq_interval> m_LocInt;
    CRef<CSeq_point> m_LocPnt;
    CRef<CGb_qual> m_Allele0;
    CRef<CGb_qual> m_Allele1;
    CRef<CDbtag> m_Dbtag;
    CRef<CUser_object> m_Ext;
    CRef<CObject_id> m_ObjectIdQAdata;
    CRef<CObject_id> m_ObjectIdQualityCodes;
    CRef<CUser_field> m_QualityCodes;

#define ALLELE_CACHE
#ifdef ALLELE_CACHE
    CRef<CGb_qual> m_AlleleCache_empty;
    CRef<CGb_qual> m_AlleleCache_minus;
    CRef<CGb_qual> m_AlleleCacheA;
    CRef<CGb_qual> m_AlleleCacheC;
    CRef<CGb_qual> m_AlleleCacheG;
    CRef<CGb_qual> m_AlleleCacheT;
#endif

    CGb_qual& x_GetCommonAllele(CRef<CGb_qual>& cache, CTempString val)
        {
            CGb_qual* qual = cache.GetPointerOrNull();
            if ( !qual ) {
                cache = qual = new CGb_qual;
                qual->SetQual("replace");
                qual->SetVal(val);
            }
            return *qual;
        }
    CGb_qual& x_GetCachedAllele(CRef<CGb_qual>& cache, CTempString val)
        {
            CGb_qual& qual = x_GetPrivate(cache);
            x_SetStringConstant(qual, Qual, "replace");
            qual.SetVal(val);
            return qual;
        }
    CGb_qual& GetAllele(CRef<CGb_qual>& cache, CTempString val)
        {
#ifdef ALLELE_CACHE
            if ( val.size() == 1 ) {
                switch ( val[0] ) {
                case 'A': return x_GetCommonAllele(m_AlleleCacheA, val);
                case 'C': return x_GetCommonAllele(m_AlleleCacheC, val);
                case 'G': return x_GetCommonAllele(m_AlleleCacheG, val);
                case 'T': return x_GetCommonAllele(m_AlleleCacheT, val);
                case '-': return x_GetCommonAllele(m_AlleleCache_minus, val);
                default: break;
                }
            }
            if ( val.size() == 0 ) {
                return x_GetCommonAllele(m_AlleleCache_empty, val);
            }
#endif
            return x_GetCachedAllele(cache, val);
        }
};


inline
CSNPDbIterator::SCreateCache& CSNPDbIterator::x_GetCreateCache(void) const
{
    if ( !m_CreateCache ) {
        m_CreateCache = new SCreateCache;
    }
    return *m_CreateCache;
}


static inline
CObject_id& x_GetObject_id(CRef<CObject_id>& cache, const char* name)
{
    if ( !cache ) {
        cache = new CObject_id();
        cache->SetStr(name);
    }
    return *cache;
}


CRef<CSeq_feat> CSNPDbIterator::GetSeq_feat(TFlags flags) const
{
    x_CheckValid("CSNPDbIterator::GetSeq_feat");

    if ( !(flags & fUseSharedObjects) ) {
        m_CreateCache.reset();
    }
    SCreateCache& cache = x_GetCreateCache();
    CSeq_feat& feat = x_GetPrivate(cache.m_Feat);
    {{
        CSeqFeatData& data = feat.SetData();
        data.Reset();
        CImp_feat& imp = x_GetPrivate(cache.m_Imp);
        x_SetStringConstant(imp, Key, "variation");
        imp.ResetLoc();
        imp.ResetDescr();
        data.SetImp(imp);
    }}
    {{
        CSeq_loc& loc = feat.SetLocation();
        TSeqPos len = GetSNPLength();
        loc.Reset();
        if ( len == 1 ) {
            CSeq_point& loc_pnt = x_GetPrivate(cache.m_LocPnt);
            loc_pnt.SetId(*GetSeqId());
            TSeqPos pos = GetSNPPosition();
            loc_pnt.SetPoint(pos);
            loc.SetPnt(loc_pnt);
        }
        else {
            CSeq_interval& loc_int = x_GetPrivate(cache.m_LocInt);
            loc_int.SetId(*GetSeqId());
            TSeqPos pos = GetSNPPosition();
            loc_int.SetFrom(pos);
            loc_int.SetTo(pos+len-1);
            loc.SetInt(loc_int);
        }
    }}
    if ( flags & fIncludeAlleles ) {
        CSeq_feat::TQual& quals = feat.SetQual();
        quals.resize(2);
        quals[0] = null;
        quals[1] = null;
        quals[0] = &cache.GetAllele(cache.m_Allele0, GetAllele());
        quals[1] = &cache.GetAllele(cache.m_Allele1, GetAltAllele());
    }
    else {
        feat.ResetQual();
    }
    if ( flags & fIncludeRsId ) {
        CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
        dbxref.resize(1);
        dbxref[0] = null;
        CDbtag& dbtag = x_GetPrivate(cache.m_Dbtag);
        x_SetStringConstant(dbtag, Db, "dbSNP");
        dbtag.SetTag().SetId(GetRsId());
        dbxref[0] = &dbtag;
    }
    else {
        feat.ResetDbxref();
    }
    feat.ResetExt();
    if ( flags & (fIncludeQualityCodes|fIncludeNeighbors) ) {
        CUser_object& ext = x_GetPrivate(cache.m_Ext);
        ext.SetType(x_GetObject_id(cache.m_ObjectIdQAdata,
                                   "dbSnpQAdata"));
        CUser_object::TData& data = ext.SetData();
        data.clear();
        if ( flags & fIncludeNeighbors ) {
        }
        if ( flags & fIncludeQualityCodes ) {
            CUser_field& field = x_GetPrivate(cache.m_QualityCodes);
            field.SetLabel(x_GetObject_id(cache.m_ObjectIdQualityCodes,
                                          "QualityCodes"));
            GetQualityCodes(field.SetData().SetOs());
            ext.SetData().push_back(Ref(&field));
        }
        feat.SetExt(ext);
    }
    return Ref(&feat);
}


/////////////////////////////////////////////////////////////////////////////


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
