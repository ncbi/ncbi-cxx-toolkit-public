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
 *   Access to WGS files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_param.hpp>
#include <util/line_reader.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>
#include <serial/pack_string.hpp>
#include <serial/objhook.hpp>
#include <serial/objectio.hpp>
#include <sra/error_codes.hpp>

#include <sra/readers/sra/kdbread.hpp>

//#define WGS_USE_PACKED_4NA
//#define COLLECT_PROFILE

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSReader
NCBI_DEFINE_ERR_SUBCODE_X(11);

BEGIN_NAMESPACE(objects);


NCBI_PARAM_DECL(bool, WGS, MASTER_DESCR);
NCBI_PARAM_DEF(bool, WGS, MASTER_DESCR, true);


#define DEGAULT_GI_INDEX_PATH                                   \
    NCBI_TRACES04_PATH "/wgs01/wgs_aux/list.wgs_gi_ranges"

#define DEGAULT_PROT_ACC_INDEX_PATH                             \
    NCBI_TRACES04_PATH "/wgs01/wgs_aux/list.wgs_acc_ranges"


NCBI_PARAM_DECL(string, WGS, GI_INDEX);
NCBI_PARAM_DEF(string, WGS, GI_INDEX, DEGAULT_GI_INDEX_PATH);


NCBI_PARAM_DECL(string, WGS, PROT_ACC_INDEX);
NCBI_PARAM_DEF(string, WGS, PROT_ACC_INDEX, DEGAULT_PROT_ACC_INDEX_PATH);


NCBI_PARAM_DECL(bool, WGS, CLIP_BY_QUALITY);
NCBI_PARAM_DEF_EX(bool, WGS, CLIP_BY_QUALITY, true,
                  eParam_NoThread, CSRA_CLIP_BY_QUALITY);


static bool s_GetClipByQuality(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, CLIP_BY_QUALITY)> s_Value;
    return s_Value->Get();
}


static const char kSeq_descrFirstByte = 49; // first byte of Seq-descr ASN.1

// split parameters
static bool kSplitEnabled = false;
static int kMainEntryId = 1;
enum EChunkType {
    eChunk_prot,
    eChunk_data,
    eChunk_feat,
    kChunkIdStep = 4
};
static const size_t kProdPerChunk = 64;


#ifdef COLLECT_PROFILE
struct SProfiler
{
    const char* name;
    size_t count;
    CStopWatch sw;
    SProfiler() : name(0), count(0) {}
    ~SProfiler() {
        if ( name )
            cout << name<<" calls: "<<count<<" time: "<<sw.Elapsed()<<endl;
    }
};
struct SProfilerGuard
{
    SProfiler& sw;
    SProfilerGuard(SProfiler& sw, const char* name)
        : sw(sw)
        {
            sw.name = name;
            sw.count += 1;
            sw.sw.Start();
        }
    ~SProfilerGuard()
        {
            sw.sw.Stop();
        }
};

static SProfiler sw_Serialize;
static SProfiler sw_Feat;
static SProfiler sw_GetAccSeq_id;
static SProfiler sw_GetChunk;
static SProfiler sw__GetProtFeat;
static SProfiler sw___GetProtAnnot;
static SProfiler sw___GetProtInst;
static SProfiler sw___GetProtDescr;
static SProfiler sw____GetProtWGSAcc;
static SProfiler sw____GetProtAccVer;
static SProfiler sw____GetProtAcc;
static SProfiler sw____GetProtGISeq_id;
static SProfiler sw____GetProtGnlSeq_id;
static SProfiler sw____GetProtAccSeq_id;
static SProfiler sw___GetProtIds;
static SProfiler sw__GetProtBioseq;
static SProfiler sw_GetProtEntry;
static SProfiler sw__GetScaffoldFeat;
static SProfiler sw___GetScaffoldQual;
static SProfiler sw___GetScaffoldAnnot;
static SProfiler sw___GetScaffoldInst;
static SProfiler sw___GetScaffoldDescr;
static SProfiler sw___GetScaffoldIds;
static SProfiler sw__GetScaffoldBioseq;
static SProfiler sw_GetScaffoldEntry;
static SProfiler sw__GetContigFeat;
static SProfiler sw___GetContigQual;
static SProfiler sw___GetContigAnnot;
static SProfiler sw____Get2na;
static SProfiler sw____Get4na;
static SProfiler sw____IsGap;
static SProfiler sw____Get2naLen;
static SProfiler sw____Get4naLen;
static SProfiler sw____GetGapLen;
static SProfiler sw____GetRaw2na;
static SProfiler sw____GetRaw4na;
static SProfiler sw___GetContigInst;
static SProfiler sw___GetContigDescr;
static SProfiler sw___GetContigIds;
static SProfiler sw__GetContigBioseq;
static SProfiler sw_GetContigEntry;

# define PROFILE(var) SProfilerGuard guard(var, #var)
#else
# define PROFILE(var)
#endif

/////////////////////////////////////////////////////////////////////////////
// CAsnBinData
/////////////////////////////////////////////////////////////////////////////


CAsnBinData::CAsnBinData(CSerialObject& obj)
    : m_MainObject(&obj)
{
}


CAsnBinData::~CAsnBinData(void)
{
}


void CAsnBinData::Serialize(CObjectOStreamAsnBinary& out) const
{
    out << *m_MainObject;
}


class CWGSAsnBinData : public CAsnBinData
{
public:
    explicit CWGSAsnBinData(CSerialObject& obj)
        : CAsnBinData(obj),
          m_EmptyDescr(new CSeq_descr)
        {
        }
    virtual ~CWGSAsnBinData(void)
        {
        }

    virtual void Serialize(CObjectOStreamAsnBinary& out) const;

    typedef CSeq_annot::TData::TFtable TFtable;
    struct SFtableInfo {
        vector<char> m_Bytes;

        void AddFeature(const CTempString& data)
            {
                m_Bytes.insert(m_Bytes.end(), data.begin(), data.end());
            }
    };
    typedef vector<char> TDescrInfo;

    void AddFeature(TFtable& ftable, const CTempString& data)
        {
            m_FtableMap[&ftable].AddFeature(data);
        }
    void AddDescr(CBioseq& seq, const CTempString& data)
        {
            seq.SetDescr(*m_EmptyDescr);
            vector<char>& dst = m_DescrMap[&seq];
            if ( 0 && data[0] == kSeq_descrFirstByte ) {
                // test for DESCR variant with Seqdesc list insead of Seq-descr
                dst.assign(data.begin()+2, data.end()-2);
            }
            else {
                dst.assign(data.begin(), data.end());
            }
        }

    typedef map<const TFtable*, SFtableInfo> TFtableMap;
    typedef map<const CBioseq*, TDescrInfo> TDescrMap;
    TFtableMap m_FtableMap;
    TDescrMap m_DescrMap;
    CRef<CSeq_descr> m_EmptyDescr;
};

class CFtableWriteHook : public CWriteChoiceVariantHook
{
public:
    typedef const CSeq_annot::TData::TFtable* TKey;
    typedef CWGSAsnBinData::SFtableInfo TInfo;
    typedef map<TKey, TInfo> TInfoMap;
    CFtableWriteHook(const TInfoMap& info_map)
        : info_map(info_map)
        {
        }

    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& variant)
        {
            CConstObjectInfo var_info = variant.GetVariant();
            TKey key = (TKey)var_info.GetObjectPtr();
            TInfoMap::const_iterator iter = info_map.find(key);
            if ( iter != info_map.end() ) {
                COStreamContainer cont(out, var_info);
                ITERATE ( CSeq_annot::TData::TFtable, it, *key ) {
                    cont << **it;
                }
                const TInfo& info = iter->second;
                out.Write(info.m_Bytes.data(), info.m_Bytes.size());
            }
            else {
                DefaultWrite(out, variant);
            }
        }

    const TInfoMap& info_map;
};


class CDescrWriteHook : public CWriteClassMemberHook
{
public:
    typedef const CBioseq* TKey;
    typedef CWGSAsnBinData::TDescrInfo TInfo;
    typedef map<TKey, TInfo> TInfoMap;
    CDescrWriteHook(const TInfoMap& info_map)
        : info_map(info_map)
        {
        }

    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& member)
        {
            TKey key = (TKey)member.GetClassObject().GetObjectPtr();
            TInfoMap::const_iterator iter = info_map.find(key);
            if ( iter != info_map.end() ) {
                COStreamClassMember mem(out, member);
                const TInfo& info = iter->second;
                if ( info.data()[0] == kSeq_descrFirstByte ) {
                    // Seq-descr
                    out.Write(info.data(), info.size());
                }
                else {
                    CObjectTypeInfo cont = member.GetMemberType();
                    while ( cont.GetTypeFamily() == eTypeFamilyPointer ) {
                        cont = cont.GetPointedType();
                    }
                    COStreamContainer mem(out, cont);
                    out.Write(info.data(), info.size());
                }
            }
            else {
                DefaultWrite(out, member);
            }
        }

    const TInfoMap& info_map;
};


void CWGSAsnBinData::Serialize(CObjectOStreamAsnBinary& out) const
{
    PROFILE(sw_Serialize);
    CFtableWriteHook hook1(m_FtableMap);
    CObjectHookGuard<CSeq_annot::TData> guard1("ftable", hook1, &out);
    CDescrWriteHook hook2(m_DescrMap);
    CObjectHookGuard<CBioseq> guard2("descr", hook2, &out);
    CAsnBinData::Serialize(out);
}


/////////////////////////////////////////////////////////////////////////////
// CWGSGiResolver
/////////////////////////////////////////////////////////////////////////////


string CWGSGiResolver::GetDefaultIndexPath(void)
{
    return NCBI_PARAM_TYPE(WGS, GI_INDEX)::GetDefault();
}


CWGSGiResolver::CWGSGiResolver(void)
{
    LoadFirst(GetDefaultIndexPath());
}


CWGSGiResolver::CWGSGiResolver(const string& index_path)
{
    LoadFirst(index_path);
}


CWGSGiResolver::~CWGSGiResolver(void)
{
}


bool CWGSGiResolver::IsValid(void) const
{
    CMutexGuard guard(m_Mutex);
    return !m_Index.m_Index.empty();
}


void CWGSGiResolver::LoadFirst(const string& index_path)
{
    CMutexGuard guard(m_Mutex);
    m_IndexPath = index_path;
    if ( !x_Load(m_Index) ) {
        m_Index.m_Index.clear();
    }
}


bool CWGSGiResolver::Update(void)
{
    SIndexInfo index;
    if ( !x_Load(index, &m_Index.m_Timestamp) ) {
        return false;
    }
    CMutexGuard guard(m_Mutex);
    index.m_Index.swap(m_Index.m_Index);
    swap(index.m_Timestamp, m_Index.m_Timestamp);
    return true;
}


bool CWGSGiResolver::x_Load(SIndexInfo& index,
                            const CTime* old_timestamp) const
{
    //CStopWatch sw(CStopWatch::eStart);
    if ( !CDirEntry(m_IndexPath).GetTime(&index.m_Timestamp) ) {
        // failed to get timestamp
        return false;
    }
    //LOG_POST_X(4, "CWGSGiResolver: got time in " << sw.Elapsed() << " s");
    if ( old_timestamp && index.m_Timestamp == *old_timestamp ) {
        // same timestamp
        return false;
    }

    CMemoryFile stream(m_IndexPath);
    if ( !stream.GetPtr() ) {
        // no index file
        return false;
    }

    index.m_Index.clear();
    vector<CTempString> tokens;
    size_t count = 0;
    for ( CMemoryLineReader line(&stream); !line.AtEOF(); ) {
        tokens.clear();
        NStr::Split(*++line, " ", tokens);
        if ( tokens.size() != 3 ) {
            if ( tokens.empty() || tokens[0][0] == '#' ) {
                // allow empty lines and comments starting with #
                continue;
            }
            ERR_POST_X(1, "CWGSGiResolver: bad index file format: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        CTempString wgs_acc = tokens[0];
        if ( wgs_acc.size() < SWGSAccession::kMinWGSAccessionLength ||
             wgs_acc.size() > SWGSAccession::kMaxWGSAccessionLength ) {
            ERR_POST_X(2, "CWGSGiResolver: bad wgs accession length: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        TIntId id1 =
            NStr::StringToNumeric<TIntId>(tokens[1], NStr::fConvErr_NoThrow);
        TIntId id2 =
            NStr::StringToNumeric<TIntId>(tokens[2], NStr::fConvErr_NoThrow);
        if ( id1 <= 0 || id1 > id2 ) {
            ERR_POST_X(3, "CWGSGiResolver: bad gi range: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        index.m_Index
            .insert(TIndex::value_type(CRange<TIntId>(id1, id2),
                                       SWGSAccession(wgs_acc)));
        ++count;
    }
    //LOG_POST_X(4, "CWGSGiResolver: loaded "<<count<<" entries in "<<sw.Elapsed()<<" s");
    return true;
}


CWGSGiResolver::TAccessionList CWGSGiResolver::FindAll(TGi gi) const
{
    TAccessionList ret;
    CMutexGuard guard(m_Mutex);
    for ( TIndex::const_iterator it(m_Index.m_Index.begin(CRange<TIntId>(gi, gi))); it; ++it ) {
        ret.push_back(it->second.acc);
    }
    return ret;
}


CWGSGiResolver::TIdRanges CWGSGiResolver::GetIdRanges(void) const
{
    TIdRanges ret;
    CMutexGuard guard(m_Mutex);
    ITERATE ( TIndex, it, m_Index.m_Index ) {
        TIdRange range(it->first.GetFrom(), it->first.GetTo());
        ret.push_back(TIdRangePair(it->second.acc, range));
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSProtAccResolver
/////////////////////////////////////////////////////////////////////////////


string CWGSProtAccResolver::GetDefaultIndexPath(void)
{
    return NCBI_PARAM_TYPE(WGS, PROT_ACC_INDEX)::GetDefault();
}


CWGSProtAccResolver::CWGSProtAccResolver(void)
{
    LoadFirst(GetDefaultIndexPath());
}


CWGSProtAccResolver::CWGSProtAccResolver(const string& index_path)
{
    LoadFirst(index_path);
}


CWGSProtAccResolver::~CWGSProtAccResolver(void)
{
}


bool CWGSProtAccResolver::IsValid(void) const
{
    CMutexGuard guard(m_Mutex);
    return !m_Index.m_Index.empty();
}


void CWGSProtAccResolver::LoadFirst(const string& index_path)
{
    CMutexGuard guard(m_Mutex);
    m_IndexPath = index_path;
    if ( !x_Load(m_Index) ) {
        m_Index.m_Index.clear();
    }
}


bool CWGSProtAccResolver::Update(void)
{
    SIndexInfo index;
    if ( !x_Load(index, &m_Index.m_Timestamp) ) {
        return false;
    }
    CMutexGuard guard(m_Mutex);
    index.m_Index.swap(m_Index.m_Index);
    swap(index.m_Timestamp, m_Index.m_Timestamp);
    return true;
}


bool CWGSProtAccResolver::x_Load(SIndexInfo& index,
                                 const CTime* old_timestamp) const
{
    //CStopWatch sw(CStopWatch::eStart);
    if ( !CDirEntry(m_IndexPath).GetTime(&index.m_Timestamp) ) {
        // failed to get timestamp
        return false;
    }
    if ( old_timestamp && index.m_Timestamp == *old_timestamp ) {
        // same timestamp
        return false;
    }

    CMemoryFile stream(m_IndexPath);
    if ( !stream.GetPtr() ) {
        // no index file
        return false;
    }

    vector<CTempString> tokens;
    size_t count = 0;
    for ( CMemoryLineReader line(&stream); !line.AtEOF(); ) {
        tokens.clear();
        NStr::Split(*++line, " ", tokens);
        if ( tokens.size() != 3 ) {
            if ( tokens.empty() || tokens[0][0] == '#' ) {
                // allow empty lines and comments starting with #
                continue;
            }
            ERR_POST_X(5, "CWGSProtAccResolver: bad index file format: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        CTempString wgs_acc = tokens[0];
        if ( wgs_acc.size() < SWGSAccession::kMinWGSAccessionLength ||
             wgs_acc.size() > SWGSAccession::kMaxWGSAccessionLength ) {
            ERR_POST_X(6, "CWGSProtAccResolver: bad wgs accession length: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        Uint4 id1;
        SAccInfo info1(tokens[1], id1);
        Uint4 id2;
        SAccInfo info2(tokens[2], id2);
        if ( !info1 || info1 != info2 || id1 > id2 ) {
            ERR_POST_X(7, "CWGSProtAccResolver: bad accession range: "<<
                       m_IndexPath<<":"<<line.GetLineNumber()<<": "<<*line);
            return false;
        }
        index.m_Index[info1]
            .insert(TRangeIndex::value_type(CRange<Uint4>(id1, id2),
                                            SWGSAccession(wgs_acc)));
        ++count;
    }
    //LOG_POST_X(8, "CWGSProtAccResolver: loaded "<<count<<" entries in "<<sw.Elapsed()<<" s");
    return true;
}


CWGSProtAccResolver::SAccInfo::SAccInfo(CTempString acc, Uint4& id)
    : m_IdLength(0)
{
    SIZE_TYPE prefix = 0;
    while ( prefix < acc.size() && isalpha(acc[prefix]&0xff) ) {
        ++prefix;
    }
    if ( prefix == acc.size() || prefix == 0 || acc.size()-prefix > 9 ) {
        // no prefix, or no digits, or too many digits
        return;
    }
    Uint4 v = 0;
    for ( SIZE_TYPE i = prefix; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return;
        }
        v = v*10 + (c-'0');
    }
    id = v;
    m_AccPrefix = acc.substr(0, prefix);
    NStr::ToUpper(m_AccPrefix);
    m_IdLength = Uint4(acc.size());
}


string CWGSProtAccResolver::SAccInfo::GetAcc(Uint4 id) const
{
    string acc = m_AccPrefix;
    acc.resize(m_IdLength, '0');
    for ( SIZE_TYPE i = m_IdLength; id; id /= 10 ) {
        acc[--i] += id % 10;
    }
    return acc;
}


CWGSProtAccResolver::TAccessionList
CWGSProtAccResolver::FindAll(const string& acc) const
{
    TAccessionList ret;
    Uint4 id;
    SAccInfo info(acc, id);
    if ( info ) {
        CMutexGuard guard(m_Mutex);
        TIndex::const_iterator it = m_Index.m_Index.find(info);
        if ( it != m_Index.m_Index.end() ) {
            for ( TRangeIndex::const_iterator it2(it->second.begin(CRange<Uint4>(id, id))); it2; ++it2 ) {
                ret.push_back(it2->second.acc);
            }
        }
    }
    return ret;
}


CWGSProtAccResolver::TIdRanges CWGSProtAccResolver::GetIdRanges(void) const
{
    TIdRanges ret;
    CMutexGuard guard(m_Mutex);
    ITERATE ( TIndex, it, m_Index.m_Index ) {
        ITERATE ( TRangeIndex, it2, it->second ) {
            TIdRange range(it->first.GetAcc(it2->first.GetFrom()),
                           it->first.GetAcc(it2->first.GetTo()));
            ret.push_back(TIdRangePair(it2->second.acc, range));
        }
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSDb_Impl cursors
/////////////////////////////////////////////////////////////////////////////


#ifdef WGS_USE_PACKED_4NA
typedef CVDBValueFor4Bits TSequence4na;
#else
typedef CVDBValueFor<Uint1> TSequence4na;
#endif

// SSeqTableCursor is helper accessor structure for SEQUENCE table
struct CWGSDb_Impl::SSeqTableCursor : public CObject {
    explicit SSeqTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(NCBI_gi, GI);
    DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
    DECLARE_VDB_COLUMN_AS_STRING(CONTIG_NAME);
    DECLARE_VDB_COLUMN_AS_STRING(NAME);
    DECLARE_VDB_COLUMN_AS_STRING(TITLE);
    DECLARE_VDB_COLUMN_AS_STRING(LABEL);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, READ_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, READ_LEN);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, TRIM_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, TRIM_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_taxid, TAXID);
    DECLARE_VDB_COLUMN_AS_STRING(DESCR);
    DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
    DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, GAP_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, GAP_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, GAP_PROPS);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, GAP_LINKAGE);
    DECLARE_VDB_COLUMN_AS(INSDC_quality_phred, QUALITY);
    DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    DECLARE_VDB_COLUMN_AS(Uint4 /*NCBI_WGS_hash*/, HASH);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
    typedef pair<TVDBRowId, TVDBRowId> row_range_t;
    DECLARE_VDB_COLUMN_AS(row_range_t, CONTIG_NAME_ROW_RANGE);

#ifdef WGS_USE_PACKED_4NA
    DECLARE_VDB_COLUMN_AS_4BITS(READ);
#else
    DECLARE_VDB_COLUMN_AS(INSDC_4na_bin, READ);
#endif
    CVDBColumnBits<2> m_READ_2na;

    mutable TVDBRowId m_4naCacheRow;
    mutable TSequence4na m_4naCache;
    TSequence4na Get4na(TVDBRowId row) const;
};


CWGSDb_Impl::SSeqTableCursor::SSeqTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(GI),
      INIT_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(ACC_VERSION),
      INIT_VDB_COLUMN(CONTIG_NAME),
      INIT_VDB_COLUMN(NAME),
      INIT_VDB_COLUMN(TITLE),
      INIT_VDB_COLUMN(LABEL),
      INIT_VDB_COLUMN(READ_START),
      INIT_VDB_COLUMN(READ_LEN),
      INIT_VDB_COLUMN(TRIM_START),
      INIT_VDB_COLUMN(TRIM_LEN),
      INIT_OPTIONAL_VDB_COLUMN(TAXID),
      INIT_OPTIONAL_VDB_COLUMN(DESCR),
      INIT_OPTIONAL_VDB_COLUMN(ANNOT),
      INIT_OPTIONAL_VDB_COLUMN(GB_STATE),
      INIT_OPTIONAL_VDB_COLUMN(GAP_START),
      INIT_OPTIONAL_VDB_COLUMN(GAP_LEN),
      INIT_OPTIONAL_VDB_COLUMN(GAP_PROPS),
      INIT_OPTIONAL_VDB_COLUMN(GAP_LINKAGE),
      INIT_OPTIONAL_VDB_COLUMN(QUALITY),
      INIT_OPTIONAL_VDB_COLUMN(CIRCULAR),
      INIT_OPTIONAL_VDB_COLUMN(HASH),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_START),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_END),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(CONTIG_NAME_ROW_RANGE),
#ifdef WGS_USE_PACKED_4NA
      INIT_VDB_COLUMN_AS(READ, INSDC:4na:packed),
#else
      INIT_VDB_COLUMN_AS(READ, INSDC:4na:bin),
#endif
      m_READ_2na(m_Cursor, "(INSDC:2na:packed)READ",
                 NULL, CVDBColumn::eMissing_Allow)
{
}


// SScfTableCursor is helper accessor structure for optional SCAFFOLD table
struct CWGSDb_Impl::SScfTableCursor : public CObject {
    SScfTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(SCAFFOLD_NAME);
    DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, COMPONENT_ID);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_one, COMPONENT_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, COMPONENT_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, COMPONENT_PROPS);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, COMPONENT_LINKAGE);
    DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
};


CWGSDb_Impl::SScfTableCursor::SScfTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(SCAFFOLD_NAME),
      INIT_OPTIONAL_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(COMPONENT_ID),
      INIT_VDB_COLUMN(COMPONENT_START),
      INIT_VDB_COLUMN(COMPONENT_LEN),
      INIT_VDB_COLUMN(COMPONENT_PROPS),
      INIT_OPTIONAL_VDB_COLUMN(COMPONENT_LINKAGE),
      INIT_OPTIONAL_VDB_COLUMN(CIRCULAR),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_START),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_END),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_PRODUCT_ROW_ID)
{
}


// SProtTableCursor is helper accessor structure for optional PROTEIN table
struct CWGSDb_Impl::SProtTableCursor : public CObject {
    explicit SProtTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;
    
    DECLARE_VDB_COLUMN_AS(NCBI_gi, GI);
    DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS_STRING(GB_ACCESSION);
    DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
    DECLARE_VDB_COLUMN_AS_STRING(TITLE);
    DECLARE_VDB_COLUMN_AS_STRING(DESCR);
    DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
    DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, PROTEIN_LEN);
    DECLARE_VDB_COLUMN_AS_STRING(PROTEIN_NAME);
    DECLARE_VDB_COLUMN_AS_STRING(REF_ACC);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
    DECLARE_VDB_COLUMN_AS_STRING(PROTEIN);
};


CWGSDb_Impl::SProtTableCursor::SProtTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(GI),
      INIT_VDB_COLUMN(ACCESSION),
      INIT_OPTIONAL_VDB_COLUMN(GB_ACCESSION),
      INIT_VDB_COLUMN(ACC_VERSION),
      INIT_OPTIONAL_VDB_COLUMN(TITLE),
      INIT_OPTIONAL_VDB_COLUMN(DESCR),
      INIT_OPTIONAL_VDB_COLUMN(ANNOT),
      INIT_VDB_COLUMN(GB_STATE),
      INIT_VDB_COLUMN(PROTEIN_LEN),
      INIT_VDB_COLUMN(PROTEIN_NAME),
      INIT_OPTIONAL_VDB_COLUMN(REF_ACC),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_START),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_END),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PROTEIN)
{
}


// SFeatTableCursor is helper accessor structure for optional FEATURE table
struct CWGSDb_Impl::SFeatTableCursor : public CObject {
    explicit SFeatTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_feattype, FEAT_TYPE);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_seqtype, LOC_SEQ_TYPE);
    DECLARE_VDB_COLUMN_AS_STRING(LOC_ACCESSION);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, LOC_ROW_ID);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, LOC_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, LOC_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_loc_strand, LOC_STRAND);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_seqtype, PRODUCT_SEQ_TYPE);
    DECLARE_VDB_COLUMN_AS_STRING(PRODUCT_ACCESSION);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, PRODUCT_ROW_ID);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, PRODUCT_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, PRODUCT_LEN);
    DECLARE_VDB_COLUMN_AS_STRING(SEQ_FEAT);
};


CWGSDb_Impl::SFeatTableCursor::SFeatTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(FEAT_TYPE),
      INIT_VDB_COLUMN(LOC_SEQ_TYPE),
      INIT_VDB_COLUMN(LOC_ACCESSION),
      INIT_VDB_COLUMN(LOC_ROW_ID),
      INIT_VDB_COLUMN(LOC_START),
      INIT_VDB_COLUMN(LOC_LEN),
      INIT_VDB_COLUMN(LOC_STRAND),
      INIT_VDB_COLUMN(PRODUCT_SEQ_TYPE),
      INIT_VDB_COLUMN(PRODUCT_ACCESSION),
      INIT_VDB_COLUMN(PRODUCT_ROW_ID),
      INIT_VDB_COLUMN(PRODUCT_START),
      INIT_VDB_COLUMN(PRODUCT_LEN),
      INIT_VDB_COLUMN(SEQ_FEAT)
{
}


// SIdxTableCursor is helper accessor structure for optional GI_IDX table
struct CWGSDb_Impl::SIdxTableCursor : public CObject {
    explicit SIdxTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(TVDBRowId, NUC_ROW_ID);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, PROT_ROW_ID);
};


CWGSDb_Impl::SIdxTableCursor::SIdxTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(NUC_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PROT_ROW_ID)
{
}


/////////////////////////////////////////////////////////////////////////////
// CWGSDb_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDb_Impl::CWGSDb_Impl(CVDBMgr& mgr,
                         CTempString path_or_acc,
                         CTempString vol_path)
    : m_Mgr(mgr),
      m_WGSPath(NormalizePathOrAccession(path_or_acc, vol_path)),
      m_Db(mgr, m_WGSPath),
      m_SeqTable(m_Db, "SEQUENCE"), // SEQUENCE table must exist
      m_IdVersion(0),
      m_ScfTableIsOpened(false),
      m_ProtTableIsOpened(false),
      m_FeatTableIsOpened(false),
      m_GiIdxTableIsOpened(false),
      m_ProtAccIndexIsOpened(0),
      m_ContigNameIndexIsOpened(0),
      m_ScaffoldNameIndexIsOpened(0),
      m_ProteinNameIndexIsOpened(0),
      m_IsSetMasterDescr(false)
{
    x_InitIdParams();
}


CWGSDb_Impl::~CWGSDb_Impl(void)
{
}


inline
CRef<CWGSDb_Impl::SSeqTableCursor> CWGSDb_Impl::Seq(TVDBRowId row)
{
    CRef<SSeqTableCursor> curs = m_Seq.Get(row);
    if ( !curs ) {
        curs = new SSeqTableCursor(SeqTable());
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SScfTableCursor> CWGSDb_Impl::Scf(TVDBRowId row)
{
    CRef<SScfTableCursor> curs = m_Scf.Get(row);
    if ( !curs ) {
        if ( const CVDBTable& table = ScfTable() ) {
            curs = new SScfTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SProtTableCursor> CWGSDb_Impl::Prot(TVDBRowId row)
{
    CRef<SProtTableCursor> curs = m_Prot.Get(row);
    if ( !curs ) {
        if ( const CVDBTable& table = ProtTable() ) {
            curs = new SProtTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SFeatTableCursor> CWGSDb_Impl::Feat(TVDBRowId row)
{
    CRef<SFeatTableCursor> curs = m_Feat.Get(row);
    if ( !curs ) {
        if ( const CVDBTable& table = FeatTable() ) {
            curs = new SFeatTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SIdxTableCursor> CWGSDb_Impl::Idx(TVDBRowId row)
{
    CRef<SIdxTableCursor> curs = m_GiIdx.Get(row);
    if ( !curs ) {
        if ( const CVDBTable& table = GiIdxTable() ) {
            curs = new SIdxTableCursor(table);
        }
    }
    return curs;
}


inline
void CWGSDb_Impl::Put(CRef<SSeqTableCursor>& curs, TVDBRowId row)
{
    m_Seq.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SScfTableCursor>& curs, TVDBRowId row)
{
    m_Scf.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SProtTableCursor>& curs, TVDBRowId row)
{
    m_Prot.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SFeatTableCursor>& curs, TVDBRowId row)
{
    m_Feat.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SIdxTableCursor>& curs, TVDBRowId row)
{
    m_GiIdx.Put(curs, row);
}


void CWGSDb_Impl::x_InitIdParams(void)
{
    CRef<SSeqTableCursor> seq = Seq();
    if ( !seq->m_Cursor.TryOpenRow(1) ) {
        m_IdPrefixWithVersion.erase();
        m_IdPrefix.erase();
        m_IdVersion = 1;
        m_IdRowDigits = 0;
        return;
    }
    CTempString acc = *seq->ACCESSION(1);
    const SIZE_TYPE prefix_len = 4;
    m_IdRowDigits = acc.size() - (prefix_len + 2);
    if ( m_IdRowDigits < 6 || m_IdRowDigits > 8 ) {
        NCBI_THROW_FMT(CSraException, eInitFailed,
                       "CWGSDb: bad WGS accession format: "<<acc);
    }
    m_IdPrefixWithVersion = acc.substr(0, prefix_len+2);
    m_IdPrefix = acc.substr(0, prefix_len);
    m_IdVersion = NStr::StringToNumeric<int>(acc.substr(prefix_len, 2));
    m_IdPrefixDb = (IsTSA()? "TSA:": "WGS:")+m_IdPrefixWithVersion;
    Put(seq);
}


string CWGSDb_Impl::NormalizePathOrAccession(CTempString path_or_acc,
                                             CTempString vol_path)
{
    if ( 1 ) {
        static bool kTryTestFiles =
            getenv("HOME") && strcmp(getenv("HOME"), "/home/vasilche") == 0;
        if ( kTryTestFiles ) {
            string test_dir = "/home/dondosha/TEST";
            string path = CDirEntry::MakePath(test_dir, path_or_acc);
            if ( CDirEntry(path).Exists() ) {
                return path;
            }
        }
    }
    if ( !vol_path.empty() ) {
        vector<CTempString> dirs;
        NStr::Split(vol_path, ":", dirs);
        ITERATE ( vector<CTempString>, it, dirs ) {
            string path = CDirEntry::MakePath(*it, path_or_acc);
            if ( CDirEntry(path).Exists() ) {
                return path;
            }
        }
        string path = CDirEntry::MakePath(vol_path, path_or_acc);
        if ( CDirEntry(path).Exists() ) {
            return path;
        }
    }
    if ( CVPath::IsPlainAccession(path_or_acc) ) {
        // parse WGS accession
        const SIZE_TYPE start = 0;
        // then there should be "ABCD01" or "ABCD"
        if ( path_or_acc.size() == start + 4 ) {
            // add default version 1
            return string(path_or_acc) + "01";
        }
        if ( 0 && path_or_acc.size() > start + 6 ) {
            // remove contig/scaffold id
            return path_or_acc.substr(0, start+6);
        }
    }
    return path_or_acc;
}


inline
void CWGSDb_Impl::OpenTable(CVDBTable& table,
                            volatile bool& table_is_opened,
                            const char* table_name)
{
    CFastMutexGuard guard(m_TableMutex);
    if ( !table_is_opened ) {
        table = CVDBTable(m_Db, table_name, CVDBTable::eMissing_Allow);
        table_is_opened = true;
    }
}


inline
void CWGSDb_Impl::OpenIndex(const CVDBTable& table,
                            CVDBTableIndex& index,
                            volatile Int1& index_is_opened,
                            const char* index_name,
                            const char* backup_index_name)
{
    if ( table ) {
        CFastMutexGuard guard(m_TableMutex);
        if ( !index_is_opened ) {
            Int1 type = -1;
            index = CVDBTableIndex(table, index_name,
                                   CVDBTableIndex::eMissing_Allow);
            if ( index ) {
                type = 1;
            }
            else if ( backup_index_name ) {
                index = CVDBTableIndex(table, backup_index_name,
                                       CVDBTableIndex::eMissing_Allow);
                if ( index ) {
                    type = 2;
                }
            }
            index_is_opened = type;
        }
    }
    else {
        index_is_opened = -1;
    }
}


void CWGSDb_Impl::OpenScfTable(void)
{
    OpenTable(m_ScfTable, m_ScfTableIsOpened, "SCAFFOLD");
}


void CWGSDb_Impl::OpenProtTable(void)
{
    OpenTable(m_ProtTable, m_ProtTableIsOpened, "PROTEIN");
}


void CWGSDb_Impl::OpenFeatTable(void)
{
    OpenTable(m_FeatTable, m_FeatTableIsOpened, "FEATURE");
}


void CWGSDb_Impl::OpenGiIdxTable(void)
{
    OpenTable(m_GiIdxTable, m_GiIdxTableIsOpened, "GI_IDX");
}


void CWGSDb_Impl::OpenContigNameIndex(void)
{
    OpenIndex(SeqTable(), m_ContigNameIndex, m_ContigNameIndexIsOpened,
              "contig_name_uc", "contig_name");
}


void CWGSDb_Impl::OpenScaffoldNameIndex(void)
{
    OpenIndex(ScfTable(), m_ScaffoldNameIndex, m_ScaffoldNameIndexIsOpened,
              "scaffold_name_uc", "scaffold_name");
}


void CWGSDb_Impl::OpenProteinNameIndex(void)
{
    OpenIndex(ProtTable(), m_ProteinNameIndex, m_ProteinNameIndexIsOpened,
              "protein_name_uc", "protein_name");
}


void CWGSDb_Impl::OpenProtAccIndex(void)
{
    OpenIndex(ProtTable(), m_ProtAccIndex, m_ProtAccIndexIsOpened,
              "gb_accession");
}


pair<TVDBRowId, char> CWGSDb_Impl::ParseRowType(CTempString acc,
                                                TAllowRowType allow_type) const
{
    pair<TVDBRowId, TAllowRowType> ret(0, eRowType_contig);
    const SIZE_TYPE start = 0;
    CTempString row = acc.substr(start+6);
    if ( row[0] == 'S' ) {
        if ( !(allow_type & fAllowRowType_scaffold) ) {
            return ret;
        }
        ret.second = eRowType_scaffold;
        row = row.substr(1); // skip scaffold prefix
    }
    else if ( row[0] == 'P' ) {
        if ( !(allow_type & fAllowRowType_protein) ) {
            return ret;
        }
        ret.second = eRowType_protein;
        row = row.substr(1); // skip scaffold prefix
    }
    else {
        if ( !(allow_type & fAllowRowType_contig) ) {
            return ret;
        }
    }
    ret.first = NStr::StringToNumeric<TVDBRowId>(row, NStr::fConvErr_NoThrow);
    if ( ret.first < 0 ) {
        ret.first = 0;
    }
    return ret;
}


TVDBRowId CWGSDb_Impl::ParseRow(CTempString acc, bool* is_scaffold) const
{
    TAllowRowType allow_type = fAllowRowType_contig;
    if ( is_scaffold ) {
        allow_type |= fAllowRowType_scaffold;
    }
    pair<TVDBRowId, TRowType> rt = ParseRowType(acc, allow_type);
    if ( is_scaffold ) {
        *is_scaffold = rt.second == eRowType_scaffold;
    }
    return rt.first;
}


BEGIN_LOCAL_NAMESPACE;

bool sx_SetVersion(CSeq_id& id, int version)
{
    if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        const_cast<CTextseq_id*>(text_id)->SetVersion(version);
        return true;
    }
    return false;
}


bool sx_SetName(CSeq_id& id, CTempString name)
{
    if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        const_cast<CTextseq_id*>(text_id)->SetName(name);
        return true;
    }
    return false;
}

void sx_SetTag(CDbtag& tag, CTempString str)
{
    CObject_id& oid = tag.SetTag();
    int id = NStr::StringToNonNegativeInt(str);
    if ( id >= 0  && 
         (str.size() == 1 || (str[0] != '0' && str[0] != '+'))) {
        oid.SetId(id);
    }
    else {
        oid.SetStr(str);
    }
}


void sx_AddDescrBytes(CSeq_descr& descr, CTempString bytes)
{
    if ( !bytes.empty() ) {
        CObjectIStreamAsnBinary in(bytes.data(), bytes.size());
        // hack to determine if the data
        // is of type Seq-descr (starts with byte 49)
        // or of type Seqdesc (starts with byte >= 160)
        if ( bytes[0] == kSeq_descrFirstByte ) {
            in >> descr;
        }
        else {
            while ( in.HaveMoreData() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                in >> *desc;
                descr.Set().push_back(desc);
            }
        }
    }
}


void sx_AddAnnotBytes(CBioseq::TAnnot& annot_set, CTempString bytes)
{
    if ( !bytes.empty() ) {
        CObjectIStreamAsnBinary in(bytes.data(), bytes.size());
        while ( in.HaveMoreData() ) {
            CRef<CSeq_annot> annot(new CSeq_annot);
            in >> *annot;
            annot_set.push_back(annot);
        }
    }
}


END_LOCAL_NAMESPACE;


bool CWGSDb_Impl::IsTSA(void) const
{
    return m_IdPrefix[0] == 'G';
}


CRef<CSeq_id> CWGSDb_Impl::GetGeneralSeq_id(CTempString tag) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefixWithVersion.empty() ) {
        return id;
    }
    id = new CSeq_id;
    CDbtag& dbtag = id->SetGeneral();
    SIZE_TYPE colon = tag.rfind(':');
    if ( colon != NPOS ) {
        dbtag.SetDb(tag.substr(0, colon));
        sx_SetTag(dbtag, tag.substr(colon+1));
    }
    else if ( !m_IdPrefixWithVersion.empty() ) {
        dbtag.SetDb(m_IdPrefixDb);
        if ( NStr::StartsWith(tag, m_IdPrefixDb) &&
             tag[m_IdPrefixDb.size()] == ':' ) {
            sx_SetTag(dbtag, tag.substr(m_IdPrefixDb.size()+1));
        }
        else {
            sx_SetTag(dbtag, tag);
        }
    }
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetAccSeq_id(CTempString acc, int version) const
{
    PROFILE(sw_GetAccSeq_id);
    CRef<CSeq_id> id;
    if ( !acc.empty() ) {
        id = new CSeq_id(acc);
        sx_SetVersion(*id, version);
    }
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetAccSeq_id(ERowType type,
                                        TVDBRowId row_id,
                                        int version) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefixWithVersion.empty() ) {
        return id;
    }
    CNcbiOstrstream str;
    str << m_IdPrefixWithVersion;
    if ( type != eRowType_contig ) {
        str << char(type);
    }
    str << setfill('0') << setw(m_IdRowDigits) << row_id;
    string id_str = CNcbiOstrstreamToString(str);
    id = new CSeq_id(id_str);
    sx_SetVersion(*id, version);
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetMasterSeq_id(void) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefix.empty() ) {
        return id;
    }
    string master_acc = m_IdPrefix;
    master_acc.resize(master_acc.size() + 2 + m_IdRowDigits, '0');
    id = new CSeq_id(master_acc);
    if ( !sx_SetVersion(*id, m_IdVersion) ) {
        id = null;
    }
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetContigSeq_id(TVDBRowId row_id) const
{
    return GetAccSeq_id(eRowType_contig, row_id, 1);
}


CRef<CSeq_id> CWGSDb_Impl::GetScaffoldSeq_id(TVDBRowId row_id) const
{
    return GetAccSeq_id(eRowType_scaffold, row_id, 1);
}


CRef<CSeq_id> CWGSDb_Impl::GetProteinSeq_id(TVDBRowId row_id) const
{
    return GetAccSeq_id(eRowType_protein, row_id, 1);
}


void CWGSDb_Impl::ResetMasterDescr(void)
{
    m_MasterDescr.clear();
    m_IsSetMasterDescr = false;
}


bool CWGSDb_Impl::LoadMasterDescr(int filter)
{
    if ( !IsSetMasterDescr() &&
         NCBI_PARAM_TYPE(WGS, MASTER_DESCR)::GetDefault() ) {
        x_LoadMasterDescr(filter);
    }
    return IsSetMasterDescr();
}


size_t CWGSDb_Impl::GetMasterDescrBytes(TMasterDescrBytes& buffer)
{
    buffer.clear();
    CKMetadata meta(SeqTable());
    if ( !meta ) {
        return 0;
    }
    CKMDataNode node(meta, "MASTER", CKMDataNode::eMissing_Allow);
    if ( !node ) {
        return 0;
    }
    size_t size = node.GetSize();
    if ( !size ) {
        return 0;
    }
    buffer.resize(size);
    node.GetData(buffer.data(), size);
    return size;
}


CRef<CSeq_entry> CWGSDb_Impl::GetMasterDescrEntry(void)
{
    if ( !m_MasterEntry ) {
        CFastMutexGuard guard(m_TableMutex);
        if ( !m_MasterEntry ) {
            TMasterDescrBytes buffer;
            if ( !GetMasterDescrBytes(buffer) ) {
                return null;
            }
            
            CObjectIStreamAsnBinary str(buffer.data(), buffer.size());
            CRef<CSeq_entry> master_entry(new CSeq_entry());
            str >> *master_entry;
            m_MasterEntry =  master_entry;
        }
    }
    return m_MasterEntry;
}


void CWGSDb_Impl::x_LoadMasterDescr(int filter)
{
    if ( CRef<CSeq_entry> master_entry = GetMasterDescrEntry() ) {
        if ( master_entry->IsSetDescr() ) {
            SetMasterDescr(master_entry->GetDescr().Get(), filter);
        }
    }
}


CWGSDb::EDescrType
CWGSDb::GetMasterDescrType(const CSeqdesc& desc)
{
    switch ( desc.Which() ) {
    case CSeqdesc::e_Pub:
    case CSeqdesc::e_Comment:
        return eDescr_force;
    case CSeqdesc::e_Source:
    case CSeqdesc::e_Molinfo:
    case CSeqdesc::e_Create_date:
    case CSeqdesc::e_Update_date:
        return eDescr_default;
    case CSeqdesc::e_User:
        if ( desc.GetUser().GetType().IsStr() ) {
            // only specific user objects are passed from WGS master
            const string& name = desc.GetUser().GetType().GetStr();
            if ( name == "DBLink" ||
                 name == "GenomeProjectsDB" ||
                 name == "StructuredComment" ||
                 name == "FeatureFetchPolicy" ) {
                return eDescr_force;
            }
        }
        return eDescr_skip;
    default:
        return eDescr_skip;
    }
}


void CWGSDb_Impl::SetMasterDescr(const TMasterDescr& descr,
                                 int filter)
{
    if ( filter == CWGSDb::eDescrDefaultFilter ) {
        TMasterDescr descr2;
        ITERATE ( CSeq_descr::Tdata, it, descr ) {
            if ( CWGSDb::GetMasterDescrType(**it) != CWGSDb::eDescr_skip ) {
                descr2.push_back(Ref(SerialClone(**it)));
            }
        }
        SetMasterDescr(descr2, CWGSDb::eDescrNoFilter);
        return;
    }
    m_MasterDescr = descr;
    m_IsSetMasterDescr = true;
}


void CWGSDb_Impl::AddMasterDescr(CSeq_descr& descr) const
{
    if ( !GetMasterDescr().empty() ) {
        unsigned type_mask = 0;
        ITERATE ( CSeq_descr::Tdata, it, descr.Get() ) {
            const CSeqdesc& desc = **it;
            type_mask |= 1 << desc.Which();
        }
        ITERATE ( TMasterDescr, it, GetMasterDescr() ) {
            const CSeqdesc& desc = **it;
            if ( CWGSDb::GetMasterDescrType(desc) == CWGSDb::eDescr_default &&
                 (type_mask & (1 << desc.Which())) ) {
                // omit master descr if contig already has one of that type
                continue;
            }
            descr.Set().push_back(*it);
        }
    }
}


CRef<CSeq_entry> CWGSDb_Impl::GetMasterSeq_entry(void) const
{
    if ( m_MasterEntry ) {
        return m_MasterEntry;
    }

    // generate one
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& seq = entry->SetSeq();
    seq.SetId().push_back(GetMasterSeq_id());
    if ( !m_MasterDescr.empty() ) {
        seq.SetDescr().Set() = m_MasterDescr;
    }
    CSeq_inst& inst = seq.SetInst();
    inst.SetRepr(CSeq_inst::eRepr_virtual);
    inst.SetMol(CSeq_inst::eMol_dna);
    return entry;
}


TGi CWGSDb_Impl::GetMasterGi(void) const
{
    if ( m_MasterEntry ) {
        const CBioseq::TId& ids = m_MasterEntry->GetSeq().GetId();
        ITERATE ( CBioseq::TId, it, ids ) {
            const CSeq_id& id = **it;
            if ( id.IsGi() ) {
                return id.GetGi();
            }
        }
    }
    return ZERO_GI;
}


static inline TGi s_ToGi(TVDBRowId gi, const char* method)
{
    if ( gi < 0 ||
         (sizeof(TIntId) != sizeof(gi) && TVDBRowId(TIntId(gi)) != gi) ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       method<<": GI is too big: "<<gi);
    }
    return TIntId(gi);
}


pair<TGi, TGi> CWGSDb_Impl::GetNucGiRange(void)
{
    pair<TGi, TGi> ret;
    if ( CRef<SIdxTableCursor> idx = Idx() ) {
        if ( idx->m_NUC_ROW_ID ) {
            TVDBRowIdRange row_range =
                idx->m_NUC_ROW_ID.GetRowIdRange(idx->m_Cursor);
            if ( row_range.second ) {
                ret.first = s_ToGi(row_range.first,
                                   "CWGSDb::GetNucGiRange()");
                ret.second = s_ToGi(row_range.first + row_range.second - 1,
                                    "CWGSDb::GetNucGiRange()");
            }
        }
        Put(idx);
    }
    return ret;
}


pair<TGi, TGi> CWGSDb_Impl::GetProtGiRange(void)
{
    pair<TGi, TGi> ret;
    if ( CRef<SIdxTableCursor> idx = Idx() ) {
        if ( idx->m_PROT_ROW_ID ) {
            TVDBRowIdRange row_range =
                idx->m_PROT_ROW_ID.GetRowIdRange(idx->m_Cursor);
            if ( row_range.second ) {
                ret.first = s_ToGi(row_range.first,
                                   "CWGSDb::GetProtGiRange()");
                ret.second = s_ToGi(row_range.first + row_range.second - 1,
                                    "CWGSDb::GetProtGiRange()");
            }
        }
        Put(idx);
    }
    return ret;
}


void CWGSDb_Impl::x_SortGiRanges(TGiRanges& ranges)
{
    if ( ranges.empty() ) {
        return;
    }
    sort(ranges.begin(), ranges.end());
    TGiRanges::iterator dst = ranges.begin();
    for ( TGiRanges::iterator i = dst+1; i != ranges.end(); ++i ) {
        if ( i->GetFrom() == dst->GetToOpen() ) {
            dst->SetToOpen(i->GetToOpen());
        }
        else {
            *++dst = *i;
        }
    }
    ranges.erase(dst+1, ranges.end());
}


CWGSDb_Impl::TGiRanges CWGSDb_Impl::GetNucGiRanges(void)
{
    TGiRanges ranges;
    TVDBRowId row_id = 0;
    CRef<SSeqTableCursor> seq = Seq();
    if ( seq->m_GI ) {
        TIntId gi_start = -1, gi_end = -1;
        TVDBRowIdRange row_range = seq->m_Cursor.GetRowIdRange();
        for ( TVDBRowCount i = 0; i < row_range.second; ++i ) {
            row_id = row_range.first+i;
            TIntId gi = s_ToGi(*seq->GI(row_id), "CWGSDb::GetNucGiRanges()");
            if ( !gi ) {
                continue;
            }
            if ( gi != gi_end ) {
                if ( gi_end != gi_start ) {
                    ranges.push_back(TGiRange(gi_start, gi_end));
                }
                gi_start = gi;
            }
            gi_end = gi+1;
        }
        if ( gi_end != gi_start ) {
            ranges.push_back(TGiRange(gi_start, gi_end));
        }
        x_SortGiRanges(ranges);
    }
    Put(seq, row_id);
    return ranges;
}


CWGSDb_Impl::TGiRanges CWGSDb_Impl::GetProtGiRanges(void)
{
    TGiRanges ranges;
    return ranges;
}


CWGSDb_Impl::TAccRanges CWGSDb_Impl::GetProtAccRanges(void)
{
    TAccRanges ranges;
    if ( CRef<SProtTableCursor> seq = Prot() ) {
        TVDBRowId row_id = 0;
        TVDBRowIdRange row_range = seq->m_Cursor.GetRowIdRange();
        for ( TVDBRowCount i = 0; i < row_range.second; ++i ) {
            row_id = row_range.first+i;
            CTempString acc = *seq->GB_ACCESSION(row_id);
            if ( acc.empty() ) {
                continue;
            }
            Uint4 id;
            SAccInfo info(acc, id);
            if ( !info ) {
                continue;
            }
            TAccRanges::iterator it = ranges.lower_bound(info);
            if ( it == ranges.end() || it->first != info ) {
                TIdRange range(id, id+1);
                ranges.insert(it, TAccRanges::value_type(info, range));
            }
            else {
                if ( id < it->second.GetFrom() ) {
                    it->second.SetFrom(id);
                }
                else if ( id >= it->second.GetToOpen() ) {
                    it->second.SetTo(id);
                }
            }
        }
        Put(seq, row_id);
    }
    return ranges;
}


pair<TVDBRowId, bool> CWGSDb_Impl::GetGiRowId(TGi gi)
{
    pair<TVDBRowId, bool> ret;
    TIntId row_id = gi;
    if ( CRef<SIdxTableCursor> idx = Idx(row_id) ) {
        if ( idx->m_NUC_ROW_ID ) {
            CVDBValueFor<TVDBRowId> value =
                idx->NUC_ROW_ID(row_id, CVDBValue::eMissing_Allow);
            if ( !value.empty() ) {
                ret.first = *value;
            }
        }
        if ( !ret.first && idx->m_PROT_ROW_ID ) {
            CVDBValueFor<TVDBRowId> value =
                idx->PROT_ROW_ID(row_id, CVDBValue::eMissing_Allow);
            if ( !value.empty() ) {
                ret.first = *value;
            }
        }
        Put(idx, row_id);
    }
    return ret;
}


TVDBRowId CWGSDb_Impl::GetNucGiRowId(TGi gi)
{
    TVDBRowId ret = 0;
    TIntId row_id = gi;
    if ( CRef<SIdxTableCursor> idx = Idx(row_id) ) {
        if ( idx->m_NUC_ROW_ID ) {
            CVDBValueFor<TVDBRowId> value =
                idx->NUC_ROW_ID(row_id, CVDBValue::eMissing_Allow);
            if ( !value.empty() ) {
                ret = *value;
            }
        }
        Put(idx, row_id);
    }
    return ret;
}


TVDBRowId CWGSDb_Impl::GetProtGiRowId(TGi gi)
{
    TVDBRowId ret = 0;
    TIntId row_id = gi;
    if ( CRef<SIdxTableCursor> idx = Idx(row_id) ) {
        if ( idx->m_PROT_ROW_ID ) {
            CVDBValueFor<TVDBRowId> value =
                idx->PROT_ROW_ID(row_id, CVDBValue::eMissing_Allow);
            if ( !value.empty() ) {
                ret = *value;
            }
        }
        Put(idx, row_id);
    }
    return ret;
}


inline
TVDBRowId CWGSDb_Impl::Lookup(const string& name,
                              const CVDBTableIndex& index,
                              bool upcase)
{
    if ( !index ) {
        return 0;
    }
    if ( upcase && !NStr::IsUpper(name) ) {
        // upcase
        string tmp = name;
        TVDBRowIdRange range = index.Find(NStr::ToUpper(tmp));
        return range.second? range.first: 0;
    }
    else {
        TVDBRowIdRange range = index.Find(name);
        return range.second? range.first: 0;
    }
}


TVDBRowId CWGSDb_Impl::GetContigNameRowId(const string& name)
{
    if ( 0 ) {
        SSeqTableCursor::row_range_t range;
        CRef<SSeqTableCursor> seq = Seq();
        if ( seq->m_CONTIG_NAME_ROW_RANGE ) {
            seq->m_Cursor.SetParam("CONTIG_NAME_QUERY", name);
            range = *seq->CONTIG_NAME_ROW_RANGE(1);
            LOG_POST("range("<<name<<"): "<<range.first<<"-"<<range.second);
        }
        Put(seq);
        return 0;
    }
    const CVDBTableIndex& index = ContigNameIndex();
    return Lookup(name, index, m_ContigNameIndexIsOpened == 1);
}


TVDBRowId CWGSDb_Impl::GetScaffoldNameRowId(const string& name)
{
    const CVDBTableIndex& index = ScaffoldNameIndex();
    return Lookup(name, index, m_ScaffoldNameIndexIsOpened == 1);
}


TVDBRowId CWGSDb_Impl::GetProteinNameRowId(const string& name)
{
    const CVDBTableIndex& index = ProteinNameIndex();
    return Lookup(name, index, m_ProteinNameIndexIsOpened == 1);
}


TVDBRowId CWGSDb_Impl::GetProtAccRowId(const string& acc)
{
    const CVDBTableIndex& index = ProtAccIndex();
    return Lookup(acc, index, true);
}


/////////////////////////////////////////////////////////////////////////////
// CWGSSeqIterator
/////////////////////////////////////////////////////////////////////////////


TSequence4na CWGSDb_Impl::SSeqTableCursor::Get4na(TVDBRowId row) const
{
    if ( m_4naCache.empty() || row != m_4naCacheRow ) {
        PROFILE(sw____GetRaw4na);
        m_4naCache = READ(row);
        m_4naCacheRow = row;
    }
    return m_4naCache;
}


struct SWGSCreateInfo 
{
    typedef int TFlags;

    SWGSCreateInfo(const CWGSDb& db, TFlags flags)
        : db(db),
          flags(flags),
          split_prod(false),
          split_data(false),
          split_feat(false)
        {
        }

    CWGSDb db;
    TFlags flags;
    bool split_prod, split_data, split_feat;
    CRef<CBioseq> main_seq;
    CRef<CSeq_entry> entry;
    CRef<CID2S_Split_Info> split;
    CRef<CID2S_Chunk> chunk;
    CRef<CWGSAsnBinData> data;

    void x_AddDescr(CTempString bytes);
    void x_AddFeature(const CWGSFeatureIterator& it,
                      CSeq_annot::TData::TFtable& dst);
    void x_AddFeatures(TVDBRowIdRange range,
                       vector<TVDBRowId>& product_row_ids);
    void x_AddFeatures(TVDBRowIdRange range);
    void x_CreateProtSet(TVDBRowIdRange range);
    void x_AddProducts(const vector<TVDBRowId>& product_row_ids);
};


bool CWGSSeqIterator::x_Excluded(void) const
{
    if ( *this ) {
        // check special gb states
        switch ( GetGBState() ) {
        case 3: // withdrawn
            return m_Withdrawn == eExcludeWithdrawn || GetSeqLength() == 0;
        case 5: // absent
            return true;
        default:
            break;
        }
    }
    return false;
}


void CWGSSeqIterator::Reset(void)
{
    if ( m_Cur ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur, m_CurrId);
        }
        else {
            m_Cur.Reset();
        }
    }
    m_Db.Reset();
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
}


CWGSSeqIterator::CWGSSeqIterator(void)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0),
      m_Withdrawn(eExcludeWithdrawn),
      m_ClipByQuality(true)
{
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSSeqIterator& iter)
{
    *this = iter;
}


CWGSSeqIterator& CWGSSeqIterator::operator=(const CWGSSeqIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_CurrId = iter.m_CurrId;
        m_FirstGoodId = iter.m_FirstGoodId;
        m_FirstBadId = iter.m_FirstBadId;
        m_Withdrawn = iter.m_Withdrawn;
        m_ClipByQuality = iter.m_ClipByQuality;
    }
    return *this;
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
{
    x_Init(wgs_db, withdrawn, clip_type, 0);
    x_Settle();
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId row,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
{
    x_Init(wgs_db, withdrawn, clip_type, row);
    SelectRow(row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId first_row,
                                 TVDBRowId last_row,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
{
    x_Init(wgs_db, withdrawn, clip_type, first_row);
    if ( m_FirstBadId == 0 ) {
        return;
    }
    if ( first_row > m_FirstGoodId ) {
        m_CurrId = m_FirstGoodId = first_row;
    }
    if ( last_row < m_FirstBadId-1 ) {
        m_FirstBadId = last_row+1;
    }
    x_Settle();
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 CTempString acc,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
{
    if ( TVDBRowId row = wgs_db.ParseContigRow(acc) ) {
        x_Init(wgs_db, withdrawn, clip_type, row);
        SelectRow(row);
    }
    else {
        // bad format
        m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    }
}


CWGSSeqIterator::~CWGSSeqIterator(void)
{
    Reset();
}


void CWGSSeqIterator::x_Init(const CWGSDb& wgs_db,
                             EWithdrawn withdrawn,
                             EClipType clip_type,
                             TVDBRowId get_row)
{
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs_db ) {
        return;
    }
    m_Cur = wgs_db.GetNCObject().Seq(get_row);
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs_db;
    m_Withdrawn = withdrawn;
    switch ( clip_type ) {
    case eNoClip:
        m_ClipByQuality = false;
        break;
    case eClipByQuality: 
        m_ClipByQuality = true;
        break;
    default:
        m_ClipByQuality = s_GetClipByQuality();
        break;
    }
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


CWGSSeqIterator& CWGSSeqIterator::SelectRow(TVDBRowId row)
{
    if ( row < m_FirstGoodId ) {
        // before the first id
        m_CurrId = m_FirstBadId;
    }
    else {
        m_CurrId = row;
        if ( x_Excluded() ) {
            m_CurrId = m_FirstBadId;
        }
    }
    return *this;
}


void CWGSSeqIterator::x_Settle(void)
{
    while ( *this && x_Excluded() ) {
        ++m_CurrId;
    }
}


void CWGSSeqIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSSeqIterator::"<<method<<"(): Invalid iterator state");
}


bool CWGSSeqIterator::HasGi(void) const
{
    return m_Cur->m_GI && GetGi() != ZERO_GI;
}


CSeq_id::TGi CWGSSeqIterator::GetGi(void) const
{
    x_CheckValid("CWGSSeqIterator::GetGi");
    CVDBValueFor<NCBI_gi> gi = m_Cur->GI(m_CurrId);
    return gi.empty()? ZERO_GI: s_ToGi(*gi, "CWGSSeqIterator::GetGi()");
}


CTempString CWGSSeqIterator::GetAccession(void) const
{
    x_CheckValid("CWGSSeqIterator::GetAccession");
    return *CVDBStringValue(m_Cur->ACCESSION(m_CurrId));
}


int CWGSSeqIterator::GetAccVersion(void) const
{
    x_CheckValid("CWGSSeqIterator::GetAccVersion");
    return *m_Cur->ACC_VERSION(m_CurrId);
}


CRef<CSeq_id> CWGSSeqIterator::GetAccSeq_id(void) const
{
    return GetDb().GetAccSeq_id(GetAccession(), GetAccVersion());
}


CRef<CSeq_id> CWGSSeqIterator::GetGiSeq_id(void) const
{
    CRef<CSeq_id> id;
    if ( m_Cur->m_GI ) {
        CSeq_id::TGi gi = GetGi();
        if ( gi != ZERO_GI ) {
            id = new CSeq_id;
            id->SetGi(gi);
        }
    }
    return id;
}


CRef<CSeq_id> CWGSSeqIterator::GetGeneralSeq_id(void) const
{
    return GetDb().GetGeneralSeq_id(GetContigName());
}


CTempString CWGSSeqIterator::GetContigName(void) const
{
    x_CheckValid("CWGSSeqIterator::GetContigName");
    return *m_Cur->CONTIG_NAME(m_CurrId);
}

bool CWGSSeqIterator::HasTitle(void) const
{
    x_CheckValid("CWGSSeqIterator::HasTitle");
    return m_Cur->m_TITLE && !m_Cur->TITLE(m_CurrId).empty();
}

CTempString CWGSSeqIterator::GetTitle(void) const
{
    x_CheckValid("CWGSSeqIterator::GetTitle");
    return *m_Cur->TITLE(m_CurrId);
}

bool CWGSSeqIterator::HasTaxId(void) const
{
    return m_Cur->m_TAXID;
}


int CWGSSeqIterator::GetTaxId(void) const
{
    x_CheckValid("CWGSSeqIterator::GetTaxId");
    return *m_Cur->TAXID(m_CurrId);
}


bool CWGSSeqIterator::HasSeqHash(void) const
{
    x_CheckValid("CWGSSeqIterator::GetSeqHash");
    return m_Cur->m_HASH;
}


int CWGSSeqIterator::GetSeqHash(void) const
{
    return HasSeqHash()? *m_Cur->HASH(m_CurrId): 0;
}


TSeqPos CWGSSeqIterator::GetRawSeqLength(void) const
{
    return *m_Cur->READ_LEN(m_CurrId);
}


TSeqPos CWGSSeqIterator::GetClipQualityLeft(void) const
{
    return *m_Cur->TRIM_START(m_CurrId);
}


TSeqPos CWGSSeqIterator::GetClipQualityLength(void) const
{
    return *m_Cur->TRIM_LEN(m_CurrId);
}


bool CWGSSeqIterator::HasClippingInfo(void) const
{
    if ( GetClipQualityLeft() != 0 ) {
        return true;
    }
    if ( GetClipQualityLength() != GetRawSeqLength() ) {
        return true;
    }
    return false;
}


TSeqPos CWGSSeqIterator::GetSeqLength(EClipType clip_type) const
{
    return GetClipByQualityFlag(clip_type)?
        GetClipQualityLength(): GetRawSeqLength();
}


CRef<CSeq_id> CWGSSeqIterator::GetId(TFlags flags) const
{
    if ( flags & fIds_gi ) {
        // gi
        if ( CRef<CSeq_id> id = GetGiSeq_id() ) {
            return id;
        }
    }

    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            return id;
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
            return id;
        }
    }
    
    NCBI_THROW_FMT(CSraException, eDataError,
                   "CWGSSeqIterator::GetId("<<flags<<"): "
                   "no valid id found: "<<
                   GetDb().m_IdPrefixWithVersion<<"/"<<m_CurrId);
}


void CWGSSeqIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    PROFILE(sw___GetContigIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gi ) {
        // gi
        if ( CRef<CSeq_id> id = GetGiSeq_id() ) {
            ids.push_back(id);
        }
    }
}


bool CWGSSeqIterator::HasSeqDescrBytes(void) const
{
    x_CheckValid("CWGSSeqIterator::GetSeqDescrBytes");
    return m_Cur->m_DESCR && !m_Cur->DESCR(m_CurrId).empty();
}

CTempString CWGSSeqIterator::GetSeqDescrBytes(void) const
{
    x_CheckValid("CWGSSeqIterator::GetSeqDescrBytes");
    CTempString descr_bytes;
    if ( m_Cur->m_DESCR ) {
        descr_bytes = m_Cur->DESCR(m_CurrId);
    }
    return descr_bytes;
}

bool CWGSSeqIterator::HasSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::HasSeq_descr");
    if ( (flags & fSeqDescr) && HasSeqDescrBytes() ) {
        return true;
    }
    if ( (flags & fMasterDescr) && !GetDb().GetMasterDescr().empty() ) {
        return true;
    }
    return false;
}


CRef<CSeq_descr> CWGSSeqIterator::GetSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSeq_descr");
    CRef<CSeq_descr> ret(new CSeq_descr);
    if ( flags & fSeqDescr ) {
        if( m_Cur->m_DESCR ) {
            sx_AddDescrBytes(*ret, *m_Cur->DESCR(m_CurrId));
        }
    }
    if ( flags & fMasterDescr ) {
        GetDb().AddMasterDescr(*ret);
    }
    if ( ret->Get().empty() ) {
        ret.Reset();
    }
    return ret;
}


TVDBRowIdRange CWGSSeqIterator::GetLocFeatRowIdRange(void) const
{
    x_CheckValid("CWGSSeqIterator::GetLocFeatRowIdRange");
    
    if ( !m_Cur->m_FEAT_ROW_START ) {
        return TVDBRowIdRange(0, 0);
    }
    CVDBValueFor<TVDBRowId> start_val = m_Cur->FEAT_ROW_START(m_CurrId);
    if ( start_val.empty() ) {
        return TVDBRowIdRange(0, 0);
    }
    TVDBRowId start = *start_val;
    TVDBRowId end = *m_Cur->FEAT_ROW_END(m_CurrId);
    if ( end < start ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       "CWGSSeqIterator::GetLocFeatRowIdRange: "
                       "feature row range is invalid: "<<start<<","<<end);
    }
    return TVDBRowIdRange(start, end-start+1);
}


bool CWGSSeqIterator::HasAnnotSet(void) const
{
    x_CheckValid("CWGSSeqIterator::HasAnnotSet");
    return m_Cur->m_ANNOT && !m_Cur->ANNOT(m_CurrId).empty();
}


CTempString CWGSSeqIterator::GetAnnotBytes() const
{
    x_CheckValid("CWGSSeqIterator::GetAnnotBytes");
    return *m_Cur->ANNOT(m_CurrId);
}


void CWGSSeqIterator::GetAnnotSet(TAnnotSet& annot_set, TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetAnnotSet");
    if ( (flags & fSeqAnnot) && m_Cur->m_ANNOT ) {
        sx_AddAnnotBytes(annot_set, *m_Cur->ANNOT(m_CurrId));
    }
}


bool CWGSSeqIterator::HasQualityGraph(void) const
{
    x_CheckValid("CWGSSeqIterator::HasQualityGraph");
    return m_Cur->m_QUALITY && !m_Cur->QUALITY(m_CurrId).empty();
}


void
CWGSSeqIterator::GetQualityVec(vector<INSDC_quality_phred>& quality_vec) const
{
    x_CheckValid("CWGSSeqIterator::GetQualityArray");
    CVDBValueFor<INSDC_quality_phred> quality = m_Cur->QUALITY(m_CurrId);
    size_t size = quality.size();
    quality_vec.resize(size);
    for ( size_t i = 0; i < size; ++i )
        quality_vec[i] = quality[i];
}


void CWGSSeqIterator::GetQualityAnnot(TAnnotSet& annot_set,
                                      TFlags flags) const
{
    PROFILE(sw___GetContigQual);
    x_CheckValid("CWGSSeqIterator::GetQualityAnnot");
    if ( !(flags & fQualityGraph) || !m_Cur->m_QUALITY ) {
        return;
    }
    CVDBValueFor<INSDC_quality_phred> quality(m_Cur->QUALITY(m_CurrId));
    size_t size = quality.size();
    if ( size == 0 ) {
        return;
    }
    CByte_graph::TValues values(size);
    INSDC_quality_phred min_q = 0xff, max_q = 0;
    for ( size_t i = 0; i < size; ++i ) {
        INSDC_quality_phred q = quality[i];
        values[i] = q;
        if ( q < min_q ) {
            min_q = q;
        }
        if ( q > max_q ) {
            max_q = q;
        }
    }
    if ( max_q == 0 ) {
        return;
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CAnnotdesc> name(new CAnnotdesc);
    name->SetName("Phrap Graph");
    annot->SetDesc().Set().push_back(name);
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetTitle("Phrap Quality");
    CSeq_interval& loc = graph->SetLoc().SetInt();
    loc.SetId(*GetId(flags));
    loc.SetFrom(0);
    loc.SetTo(TSeqPos(size-1));
    graph->SetNumval(TSeqPos(size));
    CByte_graph& bytes = graph->SetGraph().SetByte();
    bytes.SetValues().swap(values);
    bytes.SetAxis(0);
    bytes.SetMin(min_q);
    bytes.SetMax(max_q);
    annot->SetData().SetGraph().push_back(graph);
    annot_set.push_back(annot);
}


NCBI_gb_state CWGSSeqIterator::GetGBState(void) const
{
    x_CheckValid("CWGSSeqIterator::GetGBState");

    return m_Cur->m_GB_STATE? *m_Cur->GB_STATE(m_CurrId): 0;
}


bool CWGSSeqIterator::IsCircular(void) const
{
    x_CheckValid("CWGSSeqIterator::IsCircular");

    return m_Cur->m_CIRCULAR && *m_Cur->CIRCULAR(m_CurrId);
}


bool CWGSSeqIterator::HasGapInfo(void) const
{
    return m_Cur->m_GAP_START;
}


void CWGSSeqIterator::GetGapInfo(TWGSContigGapInfo& gap_info) const
{
    x_CheckValid("CWGSSeqIterator::GetGapInfo");

    if ( !HasGapInfo() ) {
        gap_info = TWGSContigGapInfo();
        return;
    }

    CVDBValueFor<INSDC_coord_zero> start = m_Cur->GAP_START(m_CurrId);
    gap_info.gaps_start = start.data();
    size_t gaps_count = gap_info.gaps_count = start.size();
    if ( !start.empty() ) {
        CVDBValueFor<INSDC_coord_len> len = m_Cur->GAP_LEN(m_CurrId);
        CVDBValueFor<NCBI_WGS_component_props> props =
            m_Cur->GAP_PROPS(m_CurrId);
        if ( len.size() != gaps_count || props.size() != gaps_count ) {
            NCBI_THROW(CSraException, eDataError,
                       "CWGSSeqIterator: inconsistent gap info");
        }
        gap_info.gaps_len = len.data();
        gap_info.gaps_props = props.data();
        if ( m_Cur->m_GAP_LINKAGE ) {
            CVDBValueFor<NCBI_WGS_gap_linkage> linkage =
                m_Cur->GAP_LINKAGE(m_CurrId);
            if ( linkage.size() != gaps_count ) {
                NCBI_THROW(CSraException, eDataError,
                           "CWGSSeqIterator: inconsistent gap info");
            }
            gap_info.gaps_linkage = linkage.data();
        }
    }
}

static
void sx_AddEvidence(CSeq_gap& gap, CLinkage_evidence::TType type)
{
    CRef<CLinkage_evidence> evidence(new CLinkage_evidence);
    evidence->SetType(type);
    gap.SetLinkage_evidence().push_back(evidence);
}


static
void sx_MakeGap(CDelta_seq& seg,
                TSeqPos len,
                NCBI_WGS_component_props props,
                NCBI_WGS_gap_linkage gap_linkage)
{
    static const int kLenTypeMask =
        -NCBI_WGS_gap_known |
        -NCBI_WGS_gap_unknown;
    static const int kGapTypeMask =
        -NCBI_WGS_gap_scaffold |
        -NCBI_WGS_gap_contig |
        -NCBI_WGS_gap_centromere |
        -NCBI_WGS_gap_short_arm |
        -NCBI_WGS_gap_heterochromatin |
        -NCBI_WGS_gap_telomere |
        -NCBI_WGS_gap_repeat;
    _ASSERT(props < 0);
    CSeq_literal& literal = seg.SetLiteral();
    int len_type    = -(-props & kLenTypeMask);
    int gap_type    = -(-props & kGapTypeMask);
    literal.SetLength(len);
    if ( len_type == NCBI_WGS_gap_unknown ) {
        literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }
    if ( gap_type || gap_linkage ) {
        CSeq_gap& gap = literal.SetSeq_data().SetGap();
        switch ( gap_type ) {
        case 0:
            gap.SetType(CSeq_gap::eType_unknown);
            break;
        case NCBI_WGS_gap_scaffold:
            gap.SetType(CSeq_gap::eType_scaffold);
            break;
        case NCBI_WGS_gap_contig:
            gap.SetType(CSeq_gap::eType_contig);
            break;
        case NCBI_WGS_gap_centromere:
            gap.SetType(CSeq_gap::eType_centromere);
            break;
        case NCBI_WGS_gap_short_arm:
            gap.SetType(CSeq_gap::eType_short_arm);
            break;
        case NCBI_WGS_gap_heterochromatin:
            gap.SetType(CSeq_gap::eType_heterochromatin);
            break;
        case NCBI_WGS_gap_telomere:
            gap.SetType(CSeq_gap::eType_telomere);
            break;
        case NCBI_WGS_gap_repeat:
            gap.SetType(CSeq_gap::eType_repeat);
            break;
        default:
            break;
        }
        // linkage-evidence bits should be in order of ASN.1 specification
        if ( gap_linkage & NCBI_WGS_gap_linkage_linked ) {
            gap.SetLinkage(gap.eLinkage_linked);
        }
        CLinkage_evidence::TType type = CLinkage_evidence::eType_paired_ends;
        NCBI_WGS_gap_linkage bit = NCBI_WGS_gap_linkage_evidence_paired_ends;
        for ( ; bit && bit <= gap_linkage; bit<<=1, ++type ) {
            if ( gap_linkage & bit ) {
                sx_AddEvidence(gap, type);
            }
        }
    }
}


static const bool kRecoverGaps = true;

#ifdef WGS_USE_PACKED_4NA
// For 4na data in byte {base0 4 (highest) bits, base1 4 bits} the bits are:
// 5: base0 is not 2na
// 4: base1 is not 2na
// 3-2: base0 2na encoding, or 1 if gap
// 1-0: base1 2na encoding, or 1 if gap
enum {
    f1st_4na      = 1<<5,
    f1st_2na_max  = 3<<2,
    f1st_gap_bit  = 1<<2,
    f1st_gap      = f1st_4na|f1st_gap_bit,
    f1st_mask     = f1st_4na|f1st_2na_max,
    f2nd_4na      = 1<<4,
    f2nd_2na_max  = 3<<0,
    f2nd_gap_bit  = 1<<0,
    f2nd_gap      = f2nd_4na|f2nd_gap_bit,
    f2nd_mask     = f2nd_4na|f2nd_2na_max,
    fBoth_2na_max = f1st_2na_max|f2nd_2na_max,
    fBoth_4na     = f1st_4na|f2nd_4na,
    fBoth_gap     = f1st_gap|f2nd_gap
};
static const Uint1 sx_4naFlags[256] = {
    // 2nd (low 4 bits) meaning (default=16)                        // 1st high
    //  =0  =1      =2              =3                         =17  // =32
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    16,  0,  1, 16,  2, 16, 16, 16,  3, 16, 16, 16, 16, 16, 16, 17, // =0
    20,  4,  5, 20,  6, 20, 20, 20,  7, 20, 20, 20, 20, 20, 20, 21, // =4
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    24,  8,  9, 24, 10, 24, 24, 24, 11, 24, 24, 24, 24, 24, 24, 25, // =8
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    28, 12, 13, 28, 14, 28, 28, 28, 15, 28, 28, 28, 28, 28, 28, 29, // =12
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    52, 36, 37, 52, 38, 52, 52, 52, 39, 52, 52, 52, 52, 52, 52, 53  // =36
};


#if 0
static inline
bool sx_4naIs2naBoth(Uint1 b4na)
{
    return sx_4naFlags[b4na] <= fBoth_2na_max;
}
#endif

static inline
bool sx_4naIs2na1st(Uint1 b4na)
{
    return (sx_4naFlags[b4na]&f1st_mask) <= f1st_2na_max;
}


static inline
bool sx_4naIs2na2nd(Uint1 b4na)
{
    return (sx_4naFlags[b4na]&f2nd_mask) <= f2nd_2na_max;
}


static inline
bool sx_4naIsGapBoth(Uint1 b4na)
{
    return b4na == 0xff;
}


static inline
bool sx_4naIsGap1st(Uint1 b4na)
{
    return (b4na&0xf0) == 0xf0;
}


static inline
bool sx_4naIsGap2nd(Uint1 b4na)
{
    return (b4na&0x0f) == 0x0f;
}


#if 0
static inline
bool sx_Is2na(const CVDBValueFor4Bits& read,
              TSeqPos pos,
              TSeqPos len)
{
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIs2na2nd(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        --len;
        if ( len == 0 ) {
            return true;
        }
    }
    while ( len >= 2 ) {
        // check both bases
        if ( !sx_4naIs2naBoth(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        len -= 2;
    }
    if ( len > 0 ) {
        // check one more base
        if ( !sx_4naIs2na1st(*raw_ptr) ) {
            return false;
        }
    }
    return true;
}
#endif

static inline
bool sx_IsGap(const CVDBValueFor4Bits& read,
              TSeqPos pos,
              TSeqPos len)
{
    PROFILE(sw____IsGap);
    _ASSERT(len > 0);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIsGap2nd(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        --len;
        if ( len == 0 ) {
            return true;
        }
    }
    while ( len >= 2 ) {
        // check both bases
        if ( !sx_4naIsGapBoth(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        len -= 2;
    }
    if ( len > 0 ) {
        // check one more base
        if ( !sx_4naIsGap1st(*raw_ptr) ) {
            return false;
        }
    }
    return true;
}


static inline
TSeqPos sx_Get2naLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    PROFILE(sw____Get2naLen);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len;
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIs2na2nd(*raw_ptr) ) {
            return 0;
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( flags & fBoth_4na ) {
            if ( !(flags & f1st_4na) ) {
                --rem_len;
            }
            return len-rem_len;
        }
        ++raw_ptr;
        rem_len -= 2;
    }
    if ( rem_len > 0 ) {
        // check one more base
        if ( sx_4naIs2na1st(*raw_ptr) ) {
            --rem_len;
        }
    }
    return len-rem_len;
}


static inline
TSeqPos sx_Get4naLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos, TSeqPos len,
                        TSeqPos stop_2na_len, TSeqPos stop_gap_len)
{
    PROFILE(sw____Get4naLen);
    if ( len < stop_2na_len ) {
        return len;
    }
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len, len2na = 0, gap_len = 0;
    // |-------------------- len -----------------|
    // |- 4na -|- len2na -|- gap_len -$- rem_len -|
    // $ is current position
    // only one of len2na and gap_len can be above zero

    if ( raw_pos % 2 != 0 ) {
        // check odd base
        Uint1 b4na = *raw_ptr;
        if ( kRecoverGaps && sx_4naIsGap2nd(b4na) ) {
            gap_len = 1;
            if ( stop_gap_len == 1 ) { // gap length limit
                return 0;
            }
        }
        if ( sx_4naIs2na2nd(b4na) ) {
            len2na = 1;
            if ( stop_2na_len == 1 ) { // 2na length limit
                return 0;
            }
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    if ( (len2na+rem_len < stop_2na_len) &&
         (!kRecoverGaps || gap_len+rem_len < stop_gap_len) ) {
        // not enough bases to reach stop_2na_len or stop_gap_len
        return len;
    }

    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( !(flags & fBoth_4na) ) { // both 2na
            ++raw_ptr;
            rem_len -= 2;
            len2na += 2;
            if ( len2na >= stop_2na_len ) { // reached limit on 2na len
                return len-(rem_len+len2na);
            }
            if ( kRecoverGaps ) {
                gap_len = 0;
                if ( (rem_len < stop_gap_len) &&
                     (len2na+rem_len < stop_2na_len) ) {
                    // not enough bases to reach stop_2na_len or stop_gap_len
                    return len;
                }
            }
        }
        else {
            if ( !(flags & f1st_4na) ) { // 1st 2na
                if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                    return len-(rem_len+len2na);
                }
                ++len2na;
                if ( kRecoverGaps ) {
                    gap_len = 0;
                }
            }
            else {
                if ( kRecoverGaps && (flags & f1st_gap_bit) ) {
                    if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                        return len-(rem_len+gap_len);
                    }
                    ++gap_len;
                }
                len2na = 0;
            }
            --rem_len;
            if ( !(flags & f2nd_4na) ) { // 2nd 2na
                if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                    return len-(rem_len+len2na);
                }
                ++len2na;
                if ( kRecoverGaps ) {
                    gap_len = 0;
                }
            }
            else {
                if ( kRecoverGaps && (flags & f2nd_gap_bit) ) {
                    if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                        return len-(rem_len+gap_len);
                    }
                    ++gap_len;
                }
                len2na = 0;
            }
            --rem_len;
            ++raw_ptr;
            if ( (len2na+rem_len < stop_2na_len) &&
                 (!kRecoverGaps || gap_len+rem_len < stop_gap_len) ) {
                // not enough bases to reach stop_2na_len or stop_gap_len
                return len;
            }
        }
    }
    if ( rem_len > 0 ) {
        // check one more base
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( !(flags & f1st_4na) ) { // 1st 2na
            if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                return len-(rem_len+len2na);
            }
            ++len2na;
            if ( kRecoverGaps ) {
                gap_len = 0;
            }
        }
        else {
            if ( kRecoverGaps && (flags & f1st_gap_bit) ) {
                if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                    return len-(rem_len+gap_len);
                }
                ++gap_len;
            }
            len2na = 0;
        }
        --rem_len;
    }
    _ASSERT(len2na < stop_2na_len);
    _ASSERT(!kRecoverGaps || gap_len < stop_gap_len);
    return len;
}


static inline
TSeqPos sx_GetGapLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    PROFILE(sw____GetGapLen);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len;
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIsGap2nd(*raw_ptr) ) {
            return 0;
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 b4na = *raw_ptr;
        if ( !sx_4naIsGapBoth(b4na) ) {
            if ( sx_4naIsGap1st(b4na) ) {
                --rem_len;
            }
            return len-rem_len;
        }
        ++raw_ptr;
        rem_len -= 2;
    }
    if ( rem_len > 0 ) {
        // check one more base
        if ( sx_4naIsGap1st(*raw_ptr) ) {
            --rem_len;
        }
    }
    return len-rem_len;
}


// Return 2na Seq-data for specified range.
// The data mustn't have ambiguities.
CRef<CSeq_data> CWGSSeqIterator::Get2na(TSeqPos pos, TSeqPos len)
{
    if ( m_Cur->m_READ_2na ) {
        PROFILE(sw____GetRaw2na);
        CRef<CSeq_data> ret(new CSeq_data);
        vector<char>& data = ret->SetNcbi2na().Set();
        data.resize((len+3)/4);
        m_Cur->m_Cursor.ReadElements(m_CurrId, m_Cur->m_READ_2na, 2, pos, len,
                                     data.data());
        return ret;
    }

    PROFILE(sw____Get2na);
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi2na().Set();
    size_t dst_len = (len+3)/4;
    data.reserve(dst_len);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 == 0 ) {
        // even - no shift
        // 4na (01,23) -> 2na (0123)
        while ( len >= 4 ) {
            // combine 4 bases
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // 01
            Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 23
            _ASSERT(!(b2na0 & fBoth_4na));
            _ASSERT(!(b2na1 & fBoth_4na));
            raw_ptr += 2;
            Uint1 b2na = (b2na0<<4)+b2na1; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            // trailing odd bases 1..3
            Uint1 b2na;
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // 01
            if ( len == 1 ) {
                _ASSERT(!(b2na0 & f1st_4na));
                b2na = (b2na0<<4) & 0xc0; // 0xxx
            }
            else {
                _ASSERT(!(b2na0 & fBoth_4na));
                b2na = (b2na0<<4) & 0xff; // 01xx
                if ( len == 3 ) {
                    Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 2x
                    _ASSERT(!(b2na1 & f1st_4na));
                    b2na += b2na1 & 0xc; // 012x
                }
            }
            data.push_back(b2na);
        }
    }
    else {
        // odd - shift
        // 4na (x0,12,3x) -> 2na (0123)
        while ( len >= 4 ) {
            // combine 4 bases
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // x0
            Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 12
            Uint1 b2na2 = sx_4naFlags[Uint1(raw_ptr[2])]; // 3x
            raw_ptr += 2;
            _ASSERT(!(b2na0 & f2nd_4na));
            _ASSERT(!(b2na1 & fBoth_4na));
            _ASSERT(!(b2na2 & f1st_4na));
            Uint1 b2na = ((b2na0<<6)+(b2na1<<2)+((b2na2>>2)&3))&0xff; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            // trailing odd bases 1..3
            Uint1 b2na;
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // x0
            _ASSERT(!(b2na0 & f2nd_4na));
            b2na = b2na0<<6;
            if ( len > 1 ) {
                Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 12
                if ( len == 2 ) {
                    _ASSERT(!(b2na1 & f1st_4na));
                    b2na1 &= 0xc; // 1x
                }
                else {
                    _ASSERT(!(b2na1 & fBoth_4na));
                }
                b2na += b2na1<<2; // 01xx or 012x
            }
            b2na &= 0xff;
            data.push_back(b2na);
        }
    }
    return ret;
}


// return 4na Seq-data for specified range
CRef<CSeq_data> CWGSSeqIterator::Get4na(TSeqPos pos, TSeqPos len)
{
    PROFILE(sw____Get4na);
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi4na().Set();
    size_t dst_len = (len+1)/2;
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 == 0 ) {
        data.assign(raw_ptr, raw_ptr+dst_len);
        if ( len&1 ) {
            // mask unused nibble
            data.back() &= 0xf0;
        }
    }
    else {
        data.reserve(dst_len);
        Uint1 pv = *raw_ptr++;
        for ( ; len >= 2; len -= 2 ) {
            Uint1 v = *raw_ptr++;
            Uint1 b = (pv<<4)+(v>>4);
            data.push_back(b);
            pv = v;
        }
        if ( len ) {
            Uint1 b = (pv<<4);
            data.push_back(b);
        }
    }
    return ret;
}

#else

typedef CVDBValueFor<Uint1> TSequence4na;

static inline
bool sx_Is2na(Uint1 b)
{
    return b && !(b&(b-1));
}


static inline
bool sx_IsGap(const CVDBValueFor<Uint1>& read,
              TSeqPos pos,
              TSeqPos len)
{
    PROFILE(sw____IsGap);
    _ASSERT(len > 0);
    for ( const Uint1* ptr = read.data()+pos; len; --len, ++ptr ) {
        if ( *ptr != 0xf ) {
            return false;
        }
    }
    return true;
}


static inline
TSeqPos sx_Get2naLength(const CVDBValueFor<Uint1>& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    PROFILE(sw____Get2naLen);
    TSeqPos rem_len = len;
    for ( const Uint1* ptr = read.data()+pos; rem_len; --rem_len, ++ptr ) {
        if ( !sx_Is2na(*ptr) ) {
            return len-rem_len;
        }
    }
    return len;
}


static inline
TSeqPos sx_Get4naLength(const CVDBValueFor<Uint1>& read,
                        TSeqPos pos, TSeqPos len,
                        TSeqPos stop_2na_len, TSeqPos stop_gap_len)
{
    PROFILE(sw____Get4naLen);
    if ( len < stop_2na_len ) {
        return len;
    }
    const Uint1* ptr = read.data()+pos;
    TSeqPos rem_len = len, len2na = 0, gap_len = 0;
    // |-------------------- len -----------------|
    // |- 4na -|- len2na -|- gap_len -$- rem_len -|
    // $ is current position
    // only one of len2na and gap_len can be above zero

    for ( ; rem_len; --rem_len, ++ptr ) {
        // check both bases
        Uint1 v = *ptr;
        if ( sx_Is2na(v) ) {
            if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                return len-(rem_len+len2na);
            }
            ++len2na;
            if ( kRecoverGaps ) {
                gap_len = 0;
            }
        }
        else {
            if ( kRecoverGaps && v == 0xf ) {
                if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                    return len-(rem_len+gap_len);
                }
                ++gap_len;
            }
            len2na = 0;
        }
    }
    _ASSERT(len2na < stop_2na_len);
    _ASSERT(!kRecoverGaps || gap_len < stop_gap_len);
    return len;
}


static inline
TSeqPos sx_GetGapLength(const CVDBValueFor<Uint1>& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    PROFILE(sw____GetGapLen);
    const Uint1* ptr = read.data()+pos;
    TSeqPos rem_len = len;
    for ( ; rem_len; --rem_len, ++ptr ) {
        // check both bases
        if ( *ptr != 0xf ) {
            return len-rem_len;
        }
    }
    return len;
}


// Return 2na Seq-data for specified range.
// The data mustn't have ambiguities.
CRef<CSeq_data> CWGSSeqIterator::Get2na(TSeqPos pos, TSeqPos len) const
{
    if ( m_Cur->m_READ_2na ) {
        PROFILE(sw____GetRaw2na);
        CRef<CSeq_data> ret(new CSeq_data);
        vector<char>& data = ret->SetNcbi2na().Set();
        data.resize((len+3)/4);
        m_Cur->m_Cursor.ReadElements(m_CurrId, m_Cur->m_READ_2na, 2, pos, len,
                                     data.data());
        return ret;
    }

    PROFILE(sw____Get2na);
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi2na().Set();
    size_t dst_len = (len+3)/4;
    data.reserve(dst_len);
    TSequence4na read = m_Cur->Get4na(m_CurrId);
    const Uint1* ptr = read.data()+pos;
    for ( ; len >= 4; len -= 4, ptr += 4 ) {
        Uint1 v0 = ptr[0];
        Uint1 v1 = ptr[1];
        Uint1 v2 = ptr[2];
        Uint1 v3 = ptr[3];
        Uint1 b = (0x40<<v0)+(0x10<<v1)+(0x4<<v2)+(0x1<<v3);
        data.push_back(b);
    }
    if ( len > 0 ) {
        // trailing odd bases 1..3
        Uint1 b = 0x40<<ptr[0];
        if ( len > 1 ) {
            b |= 0x10<<ptr[1];
            if ( len > 2 ) {
                b |= 0x4<<ptr[2];
            }
        }
        data.push_back(b);
    }
    return ret;
}


// return 4na Seq-data for specified range
CRef<CSeq_data> CWGSSeqIterator::Get4na(TSeqPos pos, TSeqPos len) const
{
    TSequence4na read = m_Cur->Get4na(m_CurrId);
    PROFILE(sw____Get4na);
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi4na().Set();
    size_t dst_len = (len+1)/2;
    const Uint1* ptr = read.data() + pos;
    data.reserve(dst_len);
    for ( ; len >= 2; len -= 2, ptr += 2 ) {
        Uint1 v0 = ptr[0];
        Uint1 v1 = ptr[1];
        Uint1 b = (v0<<4)+v1;
        data.push_back(b);
    }
    if ( len ) {
        Uint1 v0 = ptr[0];
        Uint1 b = (v0<<4);
        data.push_back(b);
    }
    return ret;
}

#endif


/////////////////////////////////////////////////////////////////////////////
// delta control constants

// kMin2naSize is the minimal size of 2na segment that will
// save memory if inserted in between 4na segments.
// It's determined by formula MinLen = 8*MemoryOverfeadOfSegment.
// The memory overhead of a segment in total is
// (assuming allocation overhead equal to 2 pointers):
// 18*sizeof(void*)+7*sizeof(int)
// (+sizeof(int) on some 64-bit platforms due to alignment).
// So one segment memory overhead is 100 bytes on 32-bit platform,
// and 176 bytes on 64-bit platform.
// This leads to threshold size of 800 bases on 32-bit platforms and
// 1408 bases on most 64-bit platforms.
// We'll use slightly bigger threshold to take into account
// possible CPU overhead for 2na operations.
static const TSeqPos kMin2naSize = 2048;

// size of chinks if the segment is split
static const TSeqPos kChunk4naSize = 1<<16; // 64Ki bases or 32KiB
static const TSeqPos kChunk2naSize = 1<<17; // 128Ki bases or 32KiB

// min size of segment to split
static const TSeqPos kSplit4naSize = kChunk4naSize+kChunk4naSize/4;
static const TSeqPos kSplit2naSize = kChunk2naSize+kChunk2naSize/4;

// end of delta control constants
/////////////////////////////////////////////////////////////////////////////

enum EDeltaType {
    eDelta_all,
    eDelta_split
};

// add raw data as delta segments with explicit gap info
CRef<CDelta_ext> CWGSSeqIterator::GetDelta(TSeqPos pos, TSeqPos len,
                                           TWGSContigGapInfo gap_info) const
{
    // skip gaps starting befor requested range
    while ( gap_info && gap_info.GetFrom() < pos ) {
        TSeqPos gap_end = gap_info.GetToOpen();
        if ( gap_end > pos ) {
            TSeqPos skip = gap_end - pos;
            pos = gap_end;
            // adjust range to exclude gap covering start of the range
            if ( skip >= len ) {
                // requested range is completely in the gap
                len = 0;
            }
            else {
                len -= skip;
            }
        }
        ++gap_info;
    }

    CRef<CDelta_ext> ret(new CDelta_ext);
    CDelta_ext::Tdata& delta = ret->Set();
    TSequence4na read = m_Cur->Get4na(m_CurrId);
    for ( ; len > 0; ) {
        CRef<CDelta_seq> seg(new CDelta_seq);
        TSeqPos gap_start = kInvalidSeqPos;
        TSeqPos rem_len = len;
        if ( gap_info ) {
            gap_start = *gap_info.gaps_start;
            if ( pos == gap_start ) {
                TSeqPos gap_len = *gap_info.gaps_len;
                if ( gap_len > len ) {
                    NCBI_THROW_FMT(CSraException, eDataError,
                                   "CWGSSeqIterator: "<<
                                   GetAccSeq_id()->AsFastaString()<<
                                   ": gap at "<< pos << " is too long");
                }
                NCBI_WGS_component_props props = *gap_info.gaps_props;
                NCBI_WGS_gap_linkage linkage = 0;
                if ( gap_info.gaps_linkage ) {
                    linkage = *gap_info.gaps_linkage;
                }
                sx_MakeGap(*seg, gap_len, props, linkage);
                ++gap_info;
                delta.push_back(seg);
                len -= gap_len;
                pos += gap_len;
                continue;
            }
            rem_len = min(rem_len, gap_start-pos);
        }

        CSeq_literal& literal = seg->SetLiteral();
        TSeqPos seg_len = sx_Get2naLength(read, pos, min(rem_len, kSplit2naSize));
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len > kSplit2naSize ) {
                seg_len = kChunk2naSize;
            }
            literal.SetSeq_data(*Get2na(pos, seg_len));
            _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos seg_len_2na = seg_len;
            seg_len += sx_Get4naLength(read, pos+seg_len,
                                       min(rem_len, kSplit4naSize)-seg_len,
                                       kMin2naSize, kInvalidSeqPos);
            if ( seg_len == seg_len_2na ) {
                literal.SetSeq_data(*Get2na(pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                if ( seg_len >= kSplit4naSize ) {
                    seg_len = kChunk4naSize;
                }
                literal.SetSeq_data(*Get4na(pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
        }

        literal.SetLength(seg_len);
        delta.push_back(seg);
        pos += seg_len;
        len -= seg_len;
    }
    return ret;
}


// add raw data as delta segments with gap recovering
CRef<CDelta_ext> CWGSSeqIterator::GetDelta(TSeqPos pos, TSeqPos len) const
{
    // max size of gap segment
    const TSeqPos kMinGapSize = 20;
    const TSeqPos kMaxGapSize = 200;
    // size of gap segment if its actual size is unknown
    const TSeqPos kUnknownGapSize = 100;
    
    CRef<CDelta_ext> ret(new CDelta_ext);
    CDelta_ext::Tdata& delta = ret->Set();
    TSequence4na read = m_Cur->Get4na(m_CurrId);
    for ( ; len > 0; ) {
        CRef<CDelta_seq> seg(new CDelta_seq);
        CSeq_literal& literal = seg->SetLiteral();

        TSeqPos rem_len = len;
        TSeqPos seg_len = sx_Get2naLength(read, pos, min(rem_len, kSplit2naSize));
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len > kSplit2naSize ) {
                seg_len = kChunk2naSize;
            }
            literal.SetSeq_data(*Get2na(pos, seg_len));
            _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos seg_len_2na = seg_len;
            seg_len += sx_Get4naLength(read, pos+seg_len,
                                       min(rem_len, kSplit4naSize)-seg_len,
                                       kMin2naSize, kMinGapSize);
            if ( kRecoverGaps && seg_len == 0 ) {
                seg_len = sx_GetGapLength(read, pos, min(rem_len, kMaxGapSize));
                _ASSERT(seg_len > 0);
                if ( seg_len == kUnknownGapSize ) {
                    literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                }
            }
            else if ( seg_len == seg_len_2na ) {
                literal.SetSeq_data(*Get2na(pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                if ( seg_len >= kSplit4naSize ) {
                    seg_len = kChunk4naSize;
                }
                literal.SetSeq_data(*Get4na(pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
        }

        literal.SetLength(seg_len);
        delta.push_back(seg);
        pos += seg_len;
        len -= seg_len;
    }
    return ret;
}


CRef<CSeq_inst> CWGSSeqIterator::GetSeq_inst(TFlags flags) const
{
    PROFILE(sw___GetContigInst);
    x_CheckValid("CWGSSeqIterator::GetSeq_inst");
    CRef<CSeq_inst> inst(new CSeq_inst);
    inst->SetMol(GetDb().IsTSA()? CSeq_inst::eMol_rna: CSeq_inst::eMol_dna);
    if ( IsCircular() ) {
        inst->SetTopology(CSeq_inst::eTopology_circular);
    }
    TSeqPos length = GetSeqLength();
    inst->SetLength(length);
    if ( length == 0 ) {
        inst->SetRepr(CSeq_inst::eRepr_not_set);
        return inst;
    }
    TSeqPos pos = GetClipByQualityFlag()? GetClipQualityLeft(): 0;
    if ( (flags & fMaskInst) == fInst_ncbi4na ) {
        inst->SetRepr(CSeq_inst::eRepr_raw);
        inst->SetSeq_data(*Get4na(pos, length));
    }
    else if ( HasGapInfo() ) {
        inst->SetRepr(CSeq_inst::eRepr_delta);
        inst->SetStrand(CSeq_inst::eStrand_ds);
        CRef<CSeq_id> id = GetAccSeq_id();
        TWGSContigGapInfo gap_info;
        GetGapInfo(gap_info);
        if  ( (flags & fMaskInst) == fInst_delta ) {
            CRef<CDelta_ext> delta = GetDelta(pos, length, gap_info);
            inst->SetExt().SetDelta(*delta);
            if ( delta->Set().size() == 1 ) {
                CDelta_seq& seg = *delta->Set().front();
                if ( seg.IsLiteral() &&
                     seg.GetLiteral().GetLength() == inst->GetLength() &&
                     !seg.GetLiteral().IsSetFuzz() &&
                     seg.GetLiteral().IsSetSeq_data() ) {
                    // single segment, delta is not necessary
                    inst->SetRepr(CSeq_inst::eRepr_raw);
                    inst->SetSeq_data(seg.SetLiteral().SetSeq_data());
                    inst->ResetStrand();
                    inst->ResetExt();
                }
            }
        }
        else {
            // split
            CRef<CDelta_ext> delta = GetDelta(pos, length, gap_info);
            inst->SetExt().SetDelta(*delta);
        }
    }
    else {
        inst->SetRepr(CSeq_inst::eRepr_delta);
        inst->SetStrand(CSeq_inst::eStrand_ds);
        CRef<CSeq_id> id = GetAccSeq_id();
        CRef<CDelta_ext> delta = GetDelta(pos, length);
        inst->SetExt().SetDelta(*delta);
        if ( delta->Set().size() == 1 ) {
            CDelta_seq& seg = *delta->Set().front();
            if ( seg.IsLiteral() &&
                 seg.GetLiteral().GetLength() == inst->GetLength() &&
                 !seg.GetLiteral().IsSetFuzz() &&
                 seg.GetLiteral().IsSetSeq_data() ) {
                // single segment, delta is not necessary
                inst->SetRepr(CSeq_inst::eRepr_raw);
                inst->SetSeq_data(seg.SetLiteral().SetSeq_data());
                inst->ResetStrand();
                inst->ResetExt();
            }
        }
    }
    return inst;
}


inline
void SWGSCreateInfo::x_AddDescr(CTempString bytes)
{
    if ( bytes.empty() ) {
        return;
    }
    if ( data ) {
        data->AddDescr(*main_seq, bytes);
    }
    else {
        sx_AddDescrBytes(main_seq->SetDescr(), bytes);
    }
}


inline
void SWGSCreateInfo::x_AddFeature(const CWGSFeatureIterator& it,
                                  CSeq_annot::TData::TFtable& dst)
{
    if ( data ) {
        data->AddFeature(dst, it.GetSeq_featBytes());
    }
    else {
        dst.push_back(it.GetSeq_feat());
    }
}


void SWGSCreateInfo::x_AddFeatures(TVDBRowIdRange range,
                                   vector<TVDBRowId>& product_row_ids)
{
    CSeq_annot::TData::TFtable* main_features = 0;
    CSeq_annot::TData::TFtable* product_features = 0;
    for ( CWGSFeatureIterator feat_it(db, range); feat_it; ++feat_it ) {
        CSeq_annot::TData::TFtable* dst;
        if ( TVDBRowId product_row_id = feat_it.GetProductRowId() ) {
            // product feature
            product_row_ids.push_back(product_row_id);
            if ( !product_features ) {
                CRef<CBioseq_set> seqset(new CBioseq_set);
                seqset->SetClass(CBioseq_set::eClass_nuc_prot);
                CRef<CSeq_entry> main_entry(new CSeq_entry);
                main_entry->SetSeq(*main_seq);
                seqset->SetSeq_set().push_back(main_entry);
                entry->SetSet(*seqset);
                CRef<CSeq_annot> annot(new CSeq_annot);
                seqset->SetAnnot().push_back(annot);
                product_features = &annot->SetData().SetFtable();
            }
            dst = product_features;
        }
        else {
            // plain feature
            if ( !main_features ) {
                CRef<CSeq_annot> annot(new CSeq_annot);
                main_seq->SetAnnot().push_back(annot);
                main_features = &annot->SetData().SetFtable();
            }
            dst = main_features;
        }
        x_AddFeature(feat_it, *dst);
    }
}


void SWGSCreateInfo::x_AddFeatures(TVDBRowIdRange range)
{
    CSeq_annot::TData::TFtable* main_features = 0;
    for ( CWGSFeatureIterator feat_it(db, range); feat_it; ++feat_it ) {
        // plain feature
        if ( !main_features ) {
            CRef<CSeq_annot> annot(new CSeq_annot);
            main_seq->SetAnnot().push_back(annot);
            main_features = &annot->SetData().SetFtable();
        }
        x_AddFeature(feat_it, *main_features);
    }
}


void CWGSSeqIterator::x_CreateBioseq(SWGSCreateInfo& info) const
{
    PROFILE(sw__GetContigBioseq);
    _ASSERT(!info.main_seq);
    info.main_seq = new CBioseq();
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetContigDescr);
        if ( info.flags & fMasterDescr ) {
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
            }
        }
        else if ( m_Cur->m_DESCR ) {
            // only own descriptors
            CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
            if ( !descr.empty() ) {
                info.x_AddDescr(*descr);
            }
        }
    }
    if ( info.flags & fMaskAnnot ) {
        PROFILE(sw___GetContigAnnot);
        GetAnnotSet(info.main_seq->SetAnnot(), info.flags);
        GetQualityAnnot(info.main_seq->SetAnnot(), info.flags);
        if ( info.main_seq->GetAnnot().empty() ) {
            info.main_seq->ResetAnnot();
        }
    }
    info.main_seq->SetInst(*GetSeq_inst(info.flags));
}


static
void sx_GetProductsSlice(const CWGSDb& db, TVDBRowIdRange range,
                         size_t skip, size_t count,
                         vector<TVDBRowId>& product_row_ids)
{
    for ( CWGSFeatureIterator feat_it(db, range); feat_it; ++feat_it ) {
        if ( TVDBRowId row_id = feat_it.GetProductRowId() ) {
            if ( skip ) {
                --skip;
                continue;
            }
            product_row_ids.push_back(row_id);
            if ( product_row_ids.size() == count ) {
                break;
            }
        }
    }
}


static
bool sx_HasMoreProducts(const CWGSDb& db, TVDBRowIdRange range, size_t count)
{
    for ( CWGSFeatureIterator feat_it(db, range); feat_it; ++feat_it ) {
        if ( feat_it.GetProductRowId() ) {
            if ( count ) {
                --count;
                continue;
            }
            return true;
        }
    }
    return false;
}


void SWGSCreateInfo::x_AddProducts(const vector<TVDBRowId>& product_row_ids)
{
    // add products
    TFlags save_flags = flags;
    CRef<CBioseq> save_seq = main_seq;
    CRef<CSeq_entry> save_entry = entry;
    CWGSProteinIterator prot_it(db);
    flags = prot_it.fDefaultFlags & ~prot_it.fMasterDescr;
    CBioseq_set::TSeq_set* entries = 0;
    CID2S_Chunk_Data::TBioseqs* bioseqs = 0;
    if ( chunk ) {
        CRef<CID2S_Chunk_Data> chunk_data(new CID2S_Chunk_Data);
        chunk->SetData().push_back(chunk_data);
        chunk_data->SetId().SetBioseq_set(kMainEntryId);
        bioseqs = &chunk_data->SetBioseqs();
    }
    else {
        entries = &save_entry->SetSet().SetSeq_set();
    }
    ITERATE ( vector<TVDBRowId>, it, product_row_ids ) {
        if ( !prot_it.SelectRow(*it) ) {
            ERR_POST_X(9, "CWGSDb::MakeSeqOrSet: "
                       "invalid protein row id: "<<*it);
            continue;
        }
        entry = null;
        main_seq = null;
        prot_it.x_CreateBioseq(*this);
        if ( entries ) {
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSeq(*main_seq);
            entries->push_back(entry);
        }
        else {
            bioseqs->push_back(main_seq);
        }
    }
    flags = save_flags;
    main_seq = save_seq;
    entry = save_entry;
}


void SWGSCreateInfo::x_CreateProtSet(TVDBRowIdRange range)
{
    _ASSERT(entry->IsSeq() && &entry->GetSeq() == main_seq);
    vector<TVDBRowId> product_row_ids;
    {
        PROFILE(sw__GetContigFeat);
        x_AddFeatures(range, product_row_ids);
    }
    if ( !product_row_ids.empty() ) {
        if ( split_prod ) {
            _ASSERT(entry && entry->IsSet());
            entry->SetSet().SetId().SetId(kMainEntryId);
            int chunk_index = 0;
            size_t prod_count = 0;
            CID2S_Bioseq_Ids::Tdata* ids = 0;
            CWGSProteinIterator prot_it(db);
            ITERATE ( vector<TVDBRowId>, it, product_row_ids ) {
                if ( !ids || prod_count == kProdPerChunk ) {
                    CRef<CID2S_Chunk_Info> chunk(new CID2S_Chunk_Info);
                    split->SetChunks().push_back(chunk);
                    chunk->SetId().Set(chunk_index*kChunkIdStep + eChunk_prot);
                    prod_count = 0;
                    ++chunk_index;

                    CRef<CID2S_Chunk_Content> content;
                    content = new CID2S_Chunk_Content;
                    chunk->SetContent().push_back(content);
                    content->SetFeat_ids();

                    content = new CID2S_Chunk_Content;
                    chunk->SetContent().push_back(content);
                    CRef<CID2S_Bioseq_place_Info> place_info(new CID2S_Bioseq_place_Info);
                    content->SetBioseq_place().push_back(place_info);
                    place_info->SetBioseq_set(kMainEntryId);
                    ids = &place_info->SetSeq_ids().Set();
                }
                ++prod_count;
                if ( !prot_it.SelectRow(*it) ) {
                    ERR_POST_X(9, "CWGSDb::MakeSeqOrSet: "
                               "invalid protein row id: "<<*it);
                    continue;
                }
                CRef<CID2S_Bioseq_Ids::C_E> add_id;
                if ( TGi gi = prot_it.GetGi() ) {
                    add_id = new CID2S_Bioseq_Ids::C_E;
                    add_id->SetGi(gi);
                    ids->push_back(add_id);
                }
                if ( CRef<CSeq_id> id = prot_it.GetAccSeq_id() ) {
                    add_id = new CID2S_Bioseq_Ids::C_E;
                    add_id->SetSeq_id(*id);
                    ids->push_back(add_id);
                }
                if ( CRef<CSeq_id> id = prot_it.GetGeneralSeq_id() ) {
                    add_id = new CID2S_Bioseq_Ids::C_E;
                    add_id->SetSeq_id(*id);
                    ids->push_back(add_id);
                }
            }
        }
        else {
            x_AddProducts(product_row_ids);
        }
    }
}


static
void sx_AddMasterDescr(const CWGSDb& db, CSeq_entry& entry)
{
    CSeq_descr descr;
    db->AddMasterDescr(descr);
    const CSeq_descr::Tdata& src = descr.Get();
    if ( !src.empty() ) {
        CSeq_descr::Tdata& dst = entry.SetDescr().Set();
        dst.insert(dst.end(), src.begin(), src.end());
    }
}


void CWGSSeqIterator::x_CreateEntry(SWGSCreateInfo& info) const
{
    PROFILE(sw_GetContigEntry);
    if ( !(info.flags & fSeqAnnot) || !info.db->FeatTable() ) {
        // plain sequence only without FEATURE table
        x_CreateBioseq(info);
    }
    else {
        TFlags flags = info.flags;
        info.flags = flags & ~fSeqAnnot & ~fMasterDescr;
        x_CreateBioseq(info);
        info.flags = flags;
        info.x_CreateProtSet(GetLocFeatRowIdRange());
        if ( flags & fMasterDescr ) {
            sx_AddMasterDescr(m_Db, *info.entry);
        }
        GetQualityAnnot(info.main_seq->SetAnnot(), flags);
        if ( info.main_seq->GetAnnot().empty() ) {
            info.main_seq->ResetAnnot();
        }
    }
}


bool CWGSSeqIterator::x_InitSplit(SWGSCreateInfo& info) const
{
    if ( !kSplitEnabled ) {
        return false;
    }
    // split data if...
    if ( ((info.flags & fMaskInst) == fInst_delta) && // delta is requested
         HasGapInfo() && // we have explicit gap info
         GetSeqLength() > kSplit2naSize // data is big enough
        ) {
        info.split_data = true;
    }
    if ( sx_HasMoreProducts(m_Db, GetLocFeatRowIdRange(), kProdPerChunk) ) {
        // split products if there are many enough
        info.split_prod = true;
    }
    if ( !info.split_data && !info.split_prod && !info.split_feat ) {
        return false;
    }
    info.entry = new CSeq_entry;
    info.split = new CID2S_Split_Info;
    info.split->SetSkeleton(*info.entry);
    return true;
}


void CWGSSeqIterator::x_CreateSplit(SWGSCreateInfo& info) const
{
    // split data if...
    if ( ((info.flags & fMaskInst) == fInst_delta) && // delta is requested
         HasGapInfo() && // we have explicit gap info
         GetSeqLength() > kSplit2naSize // data is big enough
        ) {
        info.split_data = true;
    }
    if ( sx_HasMoreProducts(m_Db, GetLocFeatRowIdRange(), kProdPerChunk) ) {
        // split products if there are many enough
        info.split_prod = true;
    }
    x_CreateEntry(info);
}


void CWGSSeqIterator::x_CreateChunk(SWGSCreateInfo& info,
                                    TChunkId chunk_id) const
{
    PROFILE(sw_GetChunk);
    EChunkType type = EChunkType(chunk_id%kChunkIdStep);
    size_t index = chunk_id/kChunkIdStep;
    if ( type == eChunk_prot ) {
        vector<TVDBRowId> product_row_ids;
        sx_GetProductsSlice(m_Db, GetLocFeatRowIdRange(),
                            index*kProdPerChunk, kProdPerChunk,
                            product_row_ids);
        info.x_AddProducts(product_row_ids);
    }
}


CRef<CBioseq> CWGSSeqIterator::GetBioseq(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetBioseq");
    SWGSCreateInfo info(m_Db, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSSeqIterator::GetSeq_entry(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, flags);
    info.entry = new CSeq_entry;
    x_CreateEntry(info);
    return info.entry;
}


CRef<CAsnBinData> CWGSSeqIterator::GetSeq_entryData(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSeq_entryData");
    SWGSCreateInfo info(m_Db, flags);
    info.entry = new CSeq_entry;
    info.data = new CWGSAsnBinData(*info.entry);
    x_CreateEntry(info);
    return CRef<CAsnBinData>(info.data);
}


CRef<CID2S_Split_Info> CWGSSeqIterator::GetSplitInfo(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSplitInfo");
    SWGSCreateInfo info(m_Db, flags);
    if ( !x_InitSplit(info) ) {
        return null;
    }
    x_CreateSplit(info);
    return info.split;
}


CRef<CAsnBinData> CWGSSeqIterator::GetSplitInfoData(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSplitInfoData");
    SWGSCreateInfo info(m_Db, flags);
    if ( !x_InitSplit(info) ) {
        return null;
    }
    info.data = new CWGSAsnBinData(*info.split);
    x_CreateSplit(info);
    return CRef<CAsnBinData>(info.data);
}


CRef<CID2S_Chunk> CWGSSeqIterator::GetChunk(TChunkId chunk_id,
                                            TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetChunk");
    SWGSCreateInfo info(m_Db, flags);
    info.chunk = new CID2S_Chunk;
    x_CreateChunk(info, chunk_id);
    return info.chunk;
}


CRef<CAsnBinData> CWGSSeqIterator::GetChunkData(TChunkId chunk_id,
                                                TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetChunkData");
    SWGSCreateInfo info(m_Db, flags);
    info.chunk = new CID2S_Chunk;
    info.data = new CWGSAsnBinData(*info.chunk);
    x_CreateChunk(info, chunk_id);
    return CRef<CAsnBinData>(info.data);
}


/////////////////////////////////////////////////////////////////////////////
// CWGSScaffoldIterator
/////////////////////////////////////////////////////////////////////////////


void CWGSScaffoldIterator::Reset(void)
{
    if ( m_Cur ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur);
        }
        else {
            m_Cur.Reset();
        }
    }
    m_Db.Reset();
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
}


CWGSScaffoldIterator::CWGSScaffoldIterator(void)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0)
{
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSScaffoldIterator& iter)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0)
{
    *this = iter;
}


CWGSScaffoldIterator&
CWGSScaffoldIterator::operator=(const CWGSScaffoldIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_CurrId = iter.m_CurrId;
        m_FirstGoodId = iter.m_FirstGoodId;
        m_FirstBadId = iter.m_FirstBadId;
    }
    return *this;
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db)
{
    x_Init(wgs_db);
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db,
                                           TVDBRowId row)
{
    x_Init(wgs_db);
    SelectRow(row);
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db,
                                           CTempString acc)
{
    if ( TVDBRowId row = wgs_db.ParseScaffoldRow(acc) ) {
        x_Init(wgs_db);
        SelectRow(row);
    }
    else {
        // bad format
        m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    }
}


CWGSScaffoldIterator::~CWGSScaffoldIterator(void)
{
    Reset();
}


void CWGSScaffoldIterator::x_Init(const CWGSDb& wgs_db)
{
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs_db ) {
        return;
    }
    m_Cur = wgs_db.GetNCObject().Scf();
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs_db;
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


CWGSScaffoldIterator& CWGSScaffoldIterator::SelectRow(TVDBRowId row)
{
    if ( row < m_FirstGoodId ) {
        m_CurrId = m_FirstBadId;
    }
    else {
        m_CurrId = row;
    }
    return *this;
}


void CWGSScaffoldIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSScaffoldIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


CTempString CWGSScaffoldIterator::GetAccession(void) const
{
    x_CheckValid("CWGSScaffoldIterator::GetAccession");
    if ( !m_Cur->m_ACCESSION ) {
        return CTempString();
    }
    return *CVDBStringValue(m_Cur->ACCESSION(m_CurrId));
}


int CWGSScaffoldIterator::GetAccVersion(void) const
{
    // scaffolds always have version 1
    return 1;
}


CRef<CSeq_id> CWGSScaffoldIterator::GetAccSeq_id(void) const
{
    CRef<CSeq_id> id;
    CTempString acc = GetAccession();
    if ( !acc.empty() ) {
        id = GetDb().GetAccSeq_id(acc, GetAccVersion());
    }
    else {
        id = GetDb().GetScaffoldSeq_id(m_CurrId);
    }
    return id;
}


CRef<CSeq_id> CWGSScaffoldIterator::GetGeneralSeq_id(void) const
{
    return GetDb().GetGeneralSeq_id(GetScaffoldName());
}


CRef<CSeq_id> CWGSScaffoldIterator::GetGiSeq_id(void) const
{
    return null;
}


void CWGSScaffoldIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    PROFILE(sw___GetScaffoldIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gi ) {
        // gi
        if ( CRef<CSeq_id> id = GetGiSeq_id() ) {
            ids.push_back(id);
        }
    }
}


CTempString CWGSScaffoldIterator::GetScaffoldName(void) const
{
    x_CheckValid("CWGSScaffoldIterator::GetScaffoldName");
    return *CVDBStringValue(m_Cur->SCAFFOLD_NAME(m_CurrId));
}


bool CWGSScaffoldIterator::HasSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::HasSeq_descr");

    return (flags & fMasterDescr) && !GetDb().GetMasterDescr().empty();
}


CRef<CSeq_descr> CWGSScaffoldIterator::GetSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetSeq_descr");

    CRef<CSeq_descr> ret(new CSeq_descr);
    if ( flags & fMasterDescr ) {
        GetDb().AddMasterDescr(*ret);
    }
    if ( ret->Get().empty() ) {
        ret.Reset();
    }
    return ret;
}


TSeqPos CWGSScaffoldIterator::GetSeqLength(void) const
{
    x_CheckValid("CWGSScaffoldIterator::GetSeqLength");

    TSeqPos length = 0;
    CVDBValueFor<INSDC_coord_len> lens = m_Cur->COMPONENT_LEN(m_CurrId);
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        length += len;
    }
    return length;
}


bool CWGSScaffoldIterator::IsCircular(void) const
{
    x_CheckValid("CWGSScaffoldIterator::IsCircular");

    return m_Cur->m_CIRCULAR && *m_Cur->CIRCULAR(m_CurrId);
}


TVDBRowIdRange CWGSScaffoldIterator::GetLocFeatRowIdRange(void) const
{
    x_CheckValid("CWGSScaffoldIterator::GetLocFeatRowIdRange");
    
    if ( !m_Cur->m_FEAT_ROW_START ) {
        return TVDBRowIdRange(0, 0);
    }
    CVDBValueFor<TVDBRowId> start_val = m_Cur->FEAT_ROW_START(m_CurrId);
    if ( start_val.empty() ) {
        return TVDBRowIdRange(0, 0);
    }
    TVDBRowId start = *start_val;
    TVDBRowId end = *m_Cur->FEAT_ROW_END(m_CurrId);
    if ( end < start ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       "CWGSScaffoldIterator::GetLocFeatRowIdRange: "
                       "feature row range is invalid: "<<start<<","<<end);
    }
    return TVDBRowIdRange(start, end-start+1);
}


CRef<CSeq_inst> CWGSScaffoldIterator::GetSeq_inst(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetSeq_inst");

    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = 0;
    inst->SetMol(CSeq_inst::eMol_dna);
    if ( IsCircular() ) {
        inst->SetTopology(CSeq_inst::eTopology_circular);
    }
    inst->SetStrand(CSeq_inst::eStrand_ds);
    inst->SetRepr(CSeq_inst::eRepr_delta);
    int id_ind = 0;
    CVDBValueFor<TVDBRowId> ids = m_Cur->COMPONENT_ID(m_CurrId);
    CVDBValueFor<INSDC_coord_len> lens = m_Cur->COMPONENT_LEN(m_CurrId);
    CVDBValueFor<INSDC_coord_one> starts = m_Cur->COMPONENT_START(m_CurrId);
    CVDBValueFor<NCBI_WGS_component_props> propss = m_Cur->COMPONENT_PROPS(m_CurrId);
    const NCBI_WGS_gap_linkage* linkages = 0;
    if ( m_Cur->m_COMPONENT_LINKAGE ) {
        linkages = m_Cur->COMPONENT_LINKAGE(m_CurrId).data();
    }
    CDelta_ext::Tdata& delta = inst->SetExt().SetDelta().Set();
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        NCBI_WGS_component_props props = propss[i];
        CRef<CDelta_seq> seg(new CDelta_seq);
        if ( props < 0 ) {
            // gap
            sx_MakeGap(*seg, len, props, linkages? *linkages++: 0);
        }
        else {
            // contig
            TSeqPos start = starts[i];
            if ( start == 0 || len == 0 ) {
                NCBI_THROW_FMT(CSraException, eDataError,
                               "CWGSScaffoldIterator: component is bad for "+
                               GetAccSeq_id()->AsFastaString());
            }
            --start; // make start zero-based
            TVDBRowId row_id = ids[id_ind++];
            CSeq_interval& interval = seg->SetLoc().SetInt();
            interval.SetFrom(start);
            interval.SetTo(start+len-1);
            interval.SetId(*GetDb().GetContigSeq_id(row_id));
            if ( props & NCBI_WGS_strand_minus ) {
                interval.SetStrand(eNa_strand_minus);
            }
            else if ( props & NCBI_WGS_strand_plus ) {
                // default Seq-interval strand is plus
            }
            else {
                interval.SetStrand(eNa_strand_unknown);
            }
        }
        delta.push_back(seg);
        length += len;
    }
    inst->SetLength(length);
    return inst;
}


void CWGSScaffoldIterator::x_CreateBioseq(SWGSCreateInfo& info) const
{
    PROFILE(sw__GetScaffoldBioseq);
    _ASSERT(!info.main_seq);
    info.main_seq = new CBioseq();
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetContigDescr);
        if ( info.flags & fMaskDescr ) {
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
            }
        }
        /*
        else if ( m_Cur->m_DESCR ) {
            // only own descriptors
            CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
            if ( !descr.empty() ) {
                info.x_AddDescr(*descr);
            }
        }
        */
    }
    info.main_seq->SetInst(*GetSeq_inst(info.flags));
}


void CWGSScaffoldIterator::x_CreateEntry(SWGSCreateInfo& info) const
{
    PROFILE(sw_GetScaffoldEntry);
    if ( !(info.flags & fSeqAnnot) || !info.db->FeatTable() ) {
        // plain sequence only without FEATURE table
        x_CreateBioseq(info);
    }
    else {
        TFlags flags = info.flags;
        info.flags = flags & ~fSeqAnnot & ~fMasterDescr;
        x_CreateBioseq(info);
        info.flags = flags;
        info.x_CreateProtSet(GetLocFeatRowIdRange());
        if ( flags & fMasterDescr ) {
            sx_AddMasterDescr(m_Db, *info.entry);
        }
    }
}


CRef<CBioseq> CWGSScaffoldIterator::GetBioseq(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetBioseq");
    SWGSCreateInfo info(m_Db, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSScaffoldIterator::GetSeq_entry(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, flags);
    info.entry = new CSeq_entry;
    x_CreateEntry(info);
    return info.entry;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSGiIterator
/////////////////////////////////////////////////////////////////////////////


void CWGSGiIterator::Reset(void)
{
    if ( m_Cur ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur, TIntId(m_CurrGi));
        }
        else {
            m_Cur.Reset();
        }
    }
    m_Db.Reset();
    m_CurrGi = m_FirstBadGi = ZERO_GI;
    m_CurrRowId = 0;
    m_CurrSeqType = eAll;
}


CWGSGiIterator::CWGSGiIterator(void)
    : m_CurrGi(ZERO_GI), m_FirstBadGi(ZERO_GI)
{
}


CWGSGiIterator::CWGSGiIterator(const CWGSGiIterator& iter)
    : m_CurrGi(ZERO_GI), m_FirstBadGi(ZERO_GI)
{
    *this = iter;
}


CWGSGiIterator&
CWGSGiIterator::operator=(const CWGSGiIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_CurrGi = iter.m_CurrGi;
        m_FirstBadGi = iter.m_FirstBadGi;
        m_CurrRowId = iter.m_CurrRowId;
        m_CurrSeqType = iter.m_CurrSeqType;
        m_FilterSeqType = iter.m_FilterSeqType;
    }
    return *this;
}


CWGSGiIterator::CWGSGiIterator(const CWGSDb& wgs_db, ESeqType seq_type)
{
    x_Init(wgs_db, seq_type);
    x_Settle();
}


CWGSGiIterator::CWGSGiIterator(const CWGSDb& wgs_db, TGi gi, ESeqType seq_type)
{
    x_Init(wgs_db, seq_type);
    if ( *this ) {
        TGi first_gi = m_CurrGi;
        m_CurrGi = gi;
        if ( gi < first_gi || gi >= m_FirstBadGi ) {
            m_CurrRowId = 0;
            m_CurrSeqType = eAll;
            m_FirstBadGi = gi;
        }
        else if ( x_Excluded() ) {
            m_FirstBadGi = gi;
        }
    }
}


CWGSGiIterator::~CWGSGiIterator(void)
{
    Reset();
}


void CWGSGiIterator::x_Init(const CWGSDb& wgs_db, ESeqType seq_type)
{
    m_Db = wgs_db;
    m_Cur = GetDb().Idx();
    if ( !m_Cur ) {
        m_Db.Reset();
        m_FirstBadGi = ZERO_GI;
        m_CurrGi = ZERO_GI;
        m_CurrRowId = 0;
        m_CurrSeqType = eAll;
        return;
    }
    m_FilterSeqType = seq_type;
    if ( (seq_type == eProt || !m_Cur->m_NUC_ROW_ID) &&
         (seq_type == eNuc || !m_Cur->m_PROT_ROW_ID) ) {
        // no asked type of sequences in index
        Reset();
        return;
    }
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstBadGi = TIntId(range.first+range.second);
    m_CurrGi = TIntId(range.first);
}


bool CWGSGiIterator::x_Excluded(void)
{
    if ( m_FilterSeqType != eProt && m_Cur->m_NUC_ROW_ID ) {
        CVDBValueFor<TVDBRowId> value =
            m_Cur->NUC_ROW_ID(TIntId(m_CurrGi), CVDBValue::eMissing_Allow);
        if ( !value.empty() ) {
            m_CurrRowId = *value;
            if ( m_CurrRowId ) {
                m_CurrSeqType = eNuc;
                return false;
            }
        }
    }
    if ( m_FilterSeqType != eNuc && m_Cur->m_PROT_ROW_ID ) {
        CVDBValueFor<TVDBRowId> value =
            m_Cur->PROT_ROW_ID(TIntId(m_CurrGi), CVDBValue::eMissing_Allow);
        if ( !value.empty() ) {
            m_CurrRowId = *value;
            if ( m_CurrRowId ) {
                m_CurrSeqType = eProt;
                return false;
            }
        }
    }
    m_CurrSeqType = eAll;
    m_CurrRowId = 0;
    return true;
}


void CWGSGiIterator::x_Settle(void)
{
    while ( *this && x_Excluded() ) {
        ++m_CurrGi;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CWGSProteinIterator
/////////////////////////////////////////////////////////////////////////////


void CWGSProteinIterator::Reset(void)
{
    if ( m_Cur ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur);
        }
        else {
            m_Cur.Reset();
        }
    }
    m_Db.Reset();
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
}


CWGSProteinIterator::CWGSProteinIterator(void)
    : m_CurrId(0), m_FirstGoodId(0), m_FirstBadId(0)
{
}


CWGSProteinIterator::CWGSProteinIterator(const CWGSProteinIterator& iter)
    : m_CurrId(0), m_FirstGoodId(0), m_FirstBadId(0)
{
    *this = iter;
}


CWGSProteinIterator&
CWGSProteinIterator::operator=(const CWGSProteinIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_CurrId = iter.m_CurrId;
        m_FirstGoodId = iter.m_FirstGoodId;
        m_FirstBadId = iter.m_FirstBadId;
    }
    return *this;
}


CWGSProteinIterator::CWGSProteinIterator(const CWGSDb& wgs_db)
{
    x_Init(wgs_db);
}


CWGSProteinIterator::CWGSProteinIterator(const CWGSDb& wgs_db, TVDBRowId row)
{
    x_Init(wgs_db);
    SelectRow(row);
}


CWGSProteinIterator::CWGSProteinIterator(const CWGSDb& wgs_db, CTempString acc)
{
    if ( TVDBRowId row = wgs_db.ParseProteinRow(acc) ) {
        x_Init(wgs_db);
        SelectRow(row);
    }
    else {
        // bad format
        m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    }
}


CWGSProteinIterator::~CWGSProteinIterator(void)
{
    Reset();
}


void CWGSProteinIterator::x_Init(const CWGSDb& wgs_db)
{
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs_db ) {
        return;
    }
    m_Cur = wgs_db.GetNCObject().Prot();
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs_db;
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


CWGSProteinIterator& CWGSProteinIterator::SelectRow(TVDBRowId row)
{
    if ( row < m_FirstGoodId ) {
        m_CurrId = m_FirstBadId;
    }
    else {
        m_CurrId = row;
    }
    return *this;
}


void CWGSProteinIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSProteinIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


bool CWGSProteinIterator::HasGi(void) const
{
    return m_Cur->m_GI && GetGi() != ZERO_GI;
}


CSeq_id::TGi CWGSProteinIterator::GetGi(void) const
{
    x_CheckValid("CWGSProteinIterator::GetGi");
    CVDBValueFor<NCBI_gi> gi = m_Cur->GI(m_CurrId);
    return gi.empty()? ZERO_GI: s_ToGi(*gi, "CWGSProteinIterator::GetGi()");
}


CTempString CWGSProteinIterator::GetAccession(void) const
{
    PROFILE(sw____GetProtAcc);
    x_CheckValid("CWGSProteinIterator::GetAccession");
    return *CVDBStringValue(m_Cur->GB_ACCESSION(m_CurrId));
}


int CWGSProteinIterator::GetAccVersion(void) const
{
    PROFILE(sw____GetProtAccVer);
    x_CheckValid("CWGSProteinIterator::GetAccVersion");
    return *m_Cur->ACC_VERSION(m_CurrId);
}


CRef<CSeq_id> CWGSProteinIterator::GetAccSeq_id(void) const
{
    PROFILE(sw____GetProtAccSeq_id);
    CRef<CSeq_id> id;
    CTempString acc = GetAccession();
    if ( !acc.empty() ) {
        id = GetDb().GetAccSeq_id(acc, GetAccVersion());
    }
    else {
        id = GetDb().GetProteinSeq_id(m_CurrId);
    }
    {
        PROFILE(sw____GetProtWGSAcc);
        CTempString name = m_Cur->ACCESSION(m_CurrId);
        if ( !name.empty() ) {
            sx_SetName(*id, name);
        }
    }
    return id;
}


CRef<CSeq_id> CWGSProteinIterator::GetGeneralSeq_id(void) const
{
    PROFILE(sw____GetProtGnlSeq_id);
    CRef<CSeq_id> id;
    if ( !id ) {
        id = GetDb().GetGeneralSeq_id(GetProteinName());
    }
    return id;
}


CRef<CSeq_id> CWGSProteinIterator::GetGiSeq_id(void) const
{
    PROFILE(sw____GetProtGISeq_id);
    CRef<CSeq_id> id;
    if ( m_Cur->m_GI ) {
        CSeq_id::TGi gi = GetGi();
        if ( gi != ZERO_GI ) {
            id = new CSeq_id;
            id->SetGi(gi);
        }
    }
    return id;
}


void CWGSProteinIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    PROFILE(sw___GetProtIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gi ) {
        // gi
        if ( CRef<CSeq_id> id = GetGiSeq_id() ) {
            ids.push_back(id);
        }
    }
}


CTempString CWGSProteinIterator::GetProteinName(void) const
{
    x_CheckValid("CWGSProteinIterator::GetProteinName");
    return *CVDBStringValue(m_Cur->PROTEIN_NAME(m_CurrId));
}


TSeqPos CWGSProteinIterator::GetSeqLength(void) const
{
    x_CheckValid("CWGSProteinIterator::GetSeqLength");
    return *m_Cur->PROTEIN_LEN(m_CurrId);
}


bool CWGSProteinIterator::HasRefAcc(void) const
{
    x_CheckValid("CWGSProteinIterator::HasRefAcc");
    return m_Cur->m_REF_ACC;
}


CTempString CWGSProteinIterator::GetRefAcc(void) const
{
    x_CheckValid("CWGSProteinIterator::GetRefAcc");
    return *CVDBStringValue(m_Cur->REF_ACC(m_CurrId));
}


NCBI_gb_state CWGSProteinIterator::GetGBState(void) const
{
    x_CheckValid("CWGSProteinIterator::GetGBState");

    return m_Cur->m_GB_STATE? *m_Cur->GB_STATE(m_CurrId): 0;
}


bool CWGSProteinIterator::HasTitle(void) const
{
    x_CheckValid("CWGSProteinIterator::HasTitle");

    return m_Cur->m_TITLE && !m_Cur->TITLE(m_CurrId).empty();
}


CTempString CWGSProteinIterator::GetTitle(void) const
{
    x_CheckValid("CWGSProteinIterator::GetTitle");

    if ( !m_Cur->m_TITLE ) {
        return CTempString();
    }
    return *CVDBStringValue(m_Cur->TITLE(m_CurrId));
}


TVDBRowIdRange CWGSProteinIterator::GetLocFeatRowIdRange(void) const
{
    x_CheckValid("CWGSProteinIterator::GetLocFeatRowIdRange");
    
    if ( !m_Cur->m_FEAT_ROW_START ) {
        return TVDBRowIdRange(0, 0);
    }
    CVDBValueFor<TVDBRowId> start_val = m_Cur->FEAT_ROW_START(m_CurrId);
    if ( start_val.empty() ) {
        return TVDBRowIdRange(0, 0);
    }
    TVDBRowId start = *start_val;
    TVDBRowId end = *m_Cur->FEAT_ROW_END(m_CurrId);
    if ( end < start ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       "CWGSProteinIterator::GetLocFeatRowIdRange: "
                       "feature row range is invalid: "<<start<<","<<end);
    }
    return TVDBRowIdRange(start, end-start+1);
}


TVDBRowId CWGSProteinIterator::GetProductFeatRowId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetProductFeatRowId");
    
    if ( !m_Cur->m_FEAT_PRODUCT_ROW_ID ) {
        return 0;
    }
    CVDBValueFor<TVDBRowId> row = m_Cur->FEAT_PRODUCT_ROW_ID(m_CurrId);
    return row.empty()? 0: *row;
}


bool CWGSProteinIterator::HasSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::HasSeq_descr");

    if ( flags & fSeqDescr ) {
        if ( m_Cur->m_DESCR && !m_Cur->DESCR(m_CurrId).empty() ) {
            return true;
        }
        if ( !GetTitle().empty() ) {
            return true;
        }
    }
    if ( (flags & fMasterDescr) && !GetDb().GetMasterDescr().empty() ) {
        return true;
    }
    return false;
}


CRef<CSeq_descr> CWGSProteinIterator::GetSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetSeq_descr");

    CRef<CSeq_descr> ret(new CSeq_descr);
    if ( flags & fSeqDescr ) {
        if ( m_Cur->m_DESCR ) {
            sx_AddDescrBytes(*ret, *m_Cur->DESCR(m_CurrId));
        }
        else {
            CTempString title = GetTitle();
            if ( !title.empty() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetTitle(title);
                ret->Set().push_back(desc);
            }
        }
    }
    if ( flags & fMasterDescr ) {
        GetDb().AddMasterDescr(*ret);
    }
    if ( ret->Get().empty() ) {
        ret.Reset();
    }
    return ret;
}


bool CWGSProteinIterator::HasAnnotSet(void) const
{
    x_CheckValid("CWGSProteinIterator::HasAnnotSet");

    return m_Cur->m_ANNOT && !m_Cur->ANNOT(m_CurrId).empty();
}


void CWGSProteinIterator::GetAnnotSet(TAnnotSet& annot_set, TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetAnnotSet");

    if ( (flags & fSeqAnnot) && m_Cur->m_ANNOT ) {
        sx_AddAnnotBytes(annot_set, *m_Cur->ANNOT(m_CurrId));
    }
}


CRef<CSeq_inst> CWGSProteinIterator::GetSeq_inst(TFlags flags) const
{
    PROFILE(sw___GetProtInst);
    x_CheckValid("CWGSProteinIterator::GetSeq_inst");

    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = GetSeqLength();
    inst->SetMol(CSeq_inst::eMol_aa);
    inst->SetLength(length);
    CTempString ref_acc;
    if ( HasRefAcc() ) {
        ref_acc = GetRefAcc();
    }
    if ( !m_Cur->m_PROTEIN ||
         (!ref_acc.empty() && ref_acc != GetAccession()) ) {
        // WP_ reference
        inst->SetRepr(CSeq_inst::eRepr_delta);
        CRef<CDelta_seq> seg(new CDelta_seq);
        CSeq_interval& interval = seg->SetLoc().SetInt();
        interval.SetFrom(0);
        interval.SetTo(length-1);
        interval.SetStrand(eNa_strand_plus);
        interval.SetId().Set(GetRefAcc());
        inst->SetExt().SetDelta().Set().push_back(seg);
    }
    else {
        inst->SetRepr(CSeq_inst::eRepr_raw);
        inst->SetSeq_data().SetNcbieaa().Set() = *m_Cur->PROTEIN(m_CurrId);
    }
    return inst;
}


void CWGSProteinIterator::x_CreateBioseq(SWGSCreateInfo& info) const
{
    PROFILE(sw__GetProtBioseq);
    _ASSERT(!info.main_seq);
    info.main_seq = new CBioseq();
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetProtDescr);
        if ( info.flags & fMasterDescr ) {
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
            }
        }
        else if ( m_Cur->m_DESCR ) {
            // only own descriptors
            CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
            if ( !descr.empty() ) {
                info.x_AddDescr(*descr);
            }
        }
    }
    if ( info.flags & fMaskAnnot ) {
        if ( !info.db->FeatTable() ) {
            // plain sequence only without FEATURE table
            PROFILE(sw___GetProtAnnot);
            GetAnnotSet(info.main_seq->SetAnnot());
            if ( info.main_seq->GetAnnot().empty() ) {
                info.main_seq->ResetAnnot();
            }
        }
        else {
            // generate features from FEATURE table
            PROFILE(sw__GetProtFeat);
            info.x_AddFeatures(GetLocFeatRowIdRange());
        }
    }
    info.main_seq->SetInst(*GetSeq_inst(info.flags));
}


void CWGSProteinIterator::x_CreateEntry(SWGSCreateInfo& info) const
{
    PROFILE(sw_GetProtEntry);
    x_CreateBioseq(info);
}


CRef<CBioseq> CWGSProteinIterator::GetBioseq(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetBioseq");
    SWGSCreateInfo info(m_Db, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSProteinIterator::GetSeq_entry(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, flags);
    info.entry = new CSeq_entry;
    x_CreateEntry(info);
    return info.entry;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSFeatureIterator
/////////////////////////////////////////////////////////////////////////////


void CWGSFeatureIterator::Reset(void)
{
    if ( m_Cur ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur);
        }
        else {
            m_Cur.Reset();
        }
    }
    m_Db.Reset();
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
}


CWGSFeatureIterator::CWGSFeatureIterator(void)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0)
{
}


CWGSFeatureIterator::CWGSFeatureIterator(const CWGSFeatureIterator& iter)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0)
{
    *this = iter;
}


CWGSFeatureIterator&
CWGSFeatureIterator::operator=(const CWGSFeatureIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_CurrId = iter.m_CurrId;
        m_FirstGoodId = iter.m_FirstGoodId;
        m_FirstBadId = iter.m_FirstBadId;
    }
    return *this;
}


CWGSFeatureIterator::CWGSFeatureIterator(const CWGSDb& wgs)
{
    x_Init(wgs);
}


CWGSFeatureIterator::CWGSFeatureIterator(const CWGSDb& wgs, TVDBRowId row)
{
    x_Init(wgs);
    SelectRow(row);
}


CWGSFeatureIterator::CWGSFeatureIterator(const CWGSDb& db,
                                         TVDBRowIdRange row_range)
{
    x_Init(db);
    if ( !m_Db ) {
        return;
    }
    m_FirstGoodId = m_CurrId = max(m_FirstGoodId, row_range.first);
    m_FirstBadId = min(m_FirstBadId, TVDBRowId(row_range.first+row_range.second));
}


CWGSFeatureIterator::~CWGSFeatureIterator(void)
{
    Reset();
}


CWGSFeatureIterator&
CWGSFeatureIterator::SelectRow(TVDBRowId row)
{
    if ( row < m_FirstGoodId ) {
        m_CurrId = m_FirstBadId;
    }
    else {
        m_CurrId = row;
    }
    return *this;
}


CWGSFeatureIterator&
CWGSFeatureIterator::SelectRowRange(TVDBRowIdRange row_range)
{
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = max(range.first, row_range.first);
    m_FirstBadId = min(range.first+range.second,
                       row_range.first+row_range.second);
    return *this;
}


void CWGSFeatureIterator::x_Init(const CWGSDb& wgs)
{
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs ) {
        return;
    }
    m_Cur = wgs.GetNCObject().Feat();
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs;
    TVDBRowIdRange range = m_Cur->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


void CWGSFeatureIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSFeatureIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


NCBI_WGS_seqtype CWGSFeatureIterator::GetLocSeqType(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetLocSeqType");
    return *m_Cur->LOC_SEQ_TYPE(m_CurrId);
}


NCBI_WGS_seqtype CWGSFeatureIterator::GetProductSeqType(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetProductSeqType");
    return *m_Cur->PRODUCT_SEQ_TYPE(m_CurrId);
}


TVDBRowId CWGSFeatureIterator::GetLocRowId(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetLocRowId");
    return *m_Cur->LOC_ROW_ID(m_CurrId);
}


TVDBRowId CWGSFeatureIterator::GetProductRowId(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetProductRowId");
    CVDBValueFor<TVDBRowId> row = m_Cur->PRODUCT_ROW_ID(m_CurrId);
    return row.empty()? 0: *row;
}


CTempString CWGSFeatureIterator::GetSeq_featBytes(void) const
{
    return *m_Cur->SEQ_FEAT(m_CurrId);
}


CRef<CSeq_feat> CWGSFeatureIterator::GetSeq_feat(void) const
{
    PROFILE(sw_Feat);
    CRef<CSeq_feat> feat(new CSeq_feat);
    CTempString bytes = GetSeq_featBytes();
    if ( 0 ) {
        CObjectIStreamAsnBinary in(bytes.data(), bytes.size());
        in >> *feat;
    }
    else {
        static CObjectIStreamAsnBinary in;
        static bool init_hooks = 0;
        if ( init_hooks ) {
            init_hooks = 0;
            CObjectTypeInfo type;

            if ( 1 ) {
                type = CObjectTypeInfo(CType<CObject_id>());
                type.FindVariant("str")
                    .SetLocalReadHook(in, new CPackStringChoiceHook);
            }

            if ( 1 ) {
                type = CObjectTypeInfo(CType<CImp_feat>());
                type.FindMember("key")
                    .SetLocalReadHook(in, new CPackStringClassHook(32, 128));
            }

            if ( 1 ) {
                type = CObjectTypeInfo(CType<CDbtag>());
                type.FindMember("db")
                    .SetLocalReadHook(in, new CPackStringClassHook);
            }

            if ( 1 ) {
                type = CType<CGb_qual>();
                type.FindMember("qual")
                    .SetLocalReadHook(in, new CPackStringClassHook);
            }
        }
        in.OpenFromBuffer(bytes.data(), bytes.size());
        in >> *feat;
    }
    return feat;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
