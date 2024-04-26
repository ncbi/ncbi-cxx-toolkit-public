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
#include <ncbi/ncbi.h>
#include <insdc/insdc.h>
#include <sra/readers/sra/impl/wgs-contig.h>
#include <vdb/vdb-priv.h>
#include <numeric>

//#define COLLECT_PROFILE
//#define TEST_ACC_VERSION
//#define USE_TEST_PATH

#define USE_GLOBAL_AMBIGUITY_CACHE

#ifdef USE_GLOBAL_AMBIGUITY_CACHE
# define DEFAULT_AMBIGUITY_CACHE_SIZE "128MB"
#else
# define DEFAULT_AMBIGUITY_CACHE_SIZE "32MB"
#endif

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSReader
NCBI_DEFINE_ERR_SUBCODE_X(19);

BEGIN_NAMESPACE(objects);


NCBI_PARAM_DECL(int, WGS, DEBUG);
NCBI_PARAM_DEF_EX(int, WGS, DEBUG, 0, eParam_NoThread, WGS_DEBUG);


static int s_GetDebugLevel(void)
{
    static int value = NCBI_PARAM_TYPE(WGS, DEBUG)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS, MASTER_DESCR);
NCBI_PARAM_DEF(bool, WGS, MASTER_DESCR, true);


NCBI_PARAM_DECL(bool, WGS, CLIP_BY_QUALITY);
NCBI_PARAM_DEF_EX(bool, WGS, CLIP_BY_QUALITY, true,
                  eParam_NoThread, CSRA_CLIP_BY_QUALITY);


static bool s_GetClipByQuality(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, CLIP_BY_QUALITY)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(bool, WGS, USE_AMBIGUITY_MASK);
NCBI_PARAM_DEF_EX(bool, WGS, USE_AMBIGUITY_MASK, true,
                  eParam_NoThread, WGS_USE_AMBIGUITY_MASK);


static bool s_UseAmbiguityMask(void)
{
    static bool v = NCBI_PARAM_TYPE(WGS, USE_AMBIGUITY_MASK)::GetDefault();
    return v;
}


NCBI_PARAM_DECL(bool, WGS, USE_GAP_INFO);
NCBI_PARAM_DEF_EX(bool, WGS, USE_GAP_INFO, true,
                  eParam_NoThread, WGS_USE_GAP_INFO);


static bool s_UseGapInfo(void)
{
    static bool v = NCBI_PARAM_TYPE(WGS, USE_GAP_INFO)::GetDefault();
    return v;
}


NCBI_PARAM_DECL(bool, WGS, USE_AMBIGUITY_4NA);
NCBI_PARAM_DEF_EX(bool, WGS, USE_AMBIGUITY_4NA, true,
                  eParam_NoThread, WGS_USE_AMBIGUITY_4NA);


static bool s_UseAmbiguity4na(void)
{
    static bool v = NCBI_PARAM_TYPE(WGS, USE_AMBIGUITY_4NA)::GetDefault();
    return v;
}


NCBI_PARAM_DECL(bool, WGS, USE_FULL_4NA_BLOCKS);
NCBI_PARAM_DEF_EX(bool, WGS, USE_FULL_4NA_BLOCKS, false,
                  eParam_NoThread, WGS_USE_FULL_4NA_BLOCKS);


static bool s_UseFull4naBlocks(void)
{
    static bool v = NCBI_PARAM_TYPE(WGS, USE_FULL_4NA_BLOCKS)::GetDefault();
    return v;
}


NCBI_PARAM_DECL(string, WGS, AMBIGUITY_CACHE);
NCBI_PARAM_DEF_EX(string, WGS, AMBIGUITY_CACHE, DEFAULT_AMBIGUITY_CACHE_SIZE,
                  eParam_NoThread, WGS_AMBIGUITY_CACHE);


static size_t s_GetAmbiguityCacheSize(void)
{
    static size_t v = NStr::StringToUInt8_DataSize(NCBI_PARAM_TYPE(WGS, AMBIGUITY_CACHE)::GetDefault());
    return v;
}


#ifdef USE_TEST_PATH
NCBI_PARAM_DECL(string, WGS, TEST_PATH);
NCBI_PARAM_DEF_EX(string, WGS, TEST_PATH, "",
                  eParam_NoThread, WGS_TEST_PATH);
#endif


// fixed WGS VDB parameters
static const char kSeq_descrFirstByte = 49; // first byte of Seq-descr ASN.1
static const TSeqPos kAmbiguityBlockSize = 1024; // defined by WGS VDB schema

// split parameters, turn on/off splitting of different pieces of information
static bool kEnableSplitQual = true;
static bool kEnableSplitData = true;
static bool kEnableSplitProd = true;
static bool kEnableSplitFeat = true;

// split info fixed parameters
static int kAssignedDefaultSplitVersion = 1;
static int kMainEntryId = 1;
enum EChunkType {
    eChunk_prod,
    eChunk_data,
    eChunk_feat,
    eChunk_qual,
    kChunkIdStep = 4
};

// split configurable parameters
static const size_t kProdPerChunk = 64;
static const size_t kMinFeatCountToSplit = 64;
static const size_t kFeatPerChunk = 256;
static const TSeqPos kQualChunkSize = 64<<10; // 64KiB
static const TSeqPos kDataChunkSize = 256<<10; // 64KiB in 2na encoding
static const TSeqPos kMinDataSplitSize = 128<<10;

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
static SProfiler sw_GetBioseq;
static SProfiler sw_GetSeq_entry;
static SProfiler sw_GetSeq_entryData;
static SProfiler sw_GetSplitInfo;
static SProfiler sw_GetSplitInfoData;
static SProfiler sw_InitSplit;
static SProfiler sw_GetFeatLocIdTypeRange;
static SProfiler sw_GetFeatLocIdTypeFeat;
static SProfiler sw_GetFeatLocIdTypeFeatBytes;
static SProfiler sw_GetFeatBytes;
static SProfiler sw_GetChunk;
static SProfiler sw_CreateQualityChunk;
static SProfiler sw_CreateDataChunk;
static SProfiler sw_CreateProductsChunk;
static SProfiler sw_CreateFeaturesChunk;
static SProfiler sw__GetProtFeat;
static SProfiler sw___GetProtAnnot;
static SProfiler sw___GetProtInst;
static SProfiler sw___GetProtDescr;
static SProfiler sw____GetProtWGSAcc;
static SProfiler sw____GetProtAccVer;
static SProfiler sw____GetProtAcc;
static SProfiler sw____GetProtGI;
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
static SProfiler sw____GetContigQualSize;
static SProfiler sw____GetContigQualData;
static SProfiler sw____GetContigQualMinMax;
static SProfiler sw___GetContigAnnot;
static SProfiler sw____IsGap;
static SProfiler sw____Get2naLen;
static SProfiler sw____Get4naLen;
static SProfiler sw____GetGapLen;
static SProfiler sw____GetRaw2na;
static SProfiler sw____GetRaw4na;
static SProfiler sw____GetAmb2Mask;
static SProfiler sw____Get4na2Mask;
static SProfiler sw____Scan4na;
static SProfiler sw____GetCvt4na;
static SProfiler sw____GetAmb4na;
static SProfiler sw____GetBlk4na;
static SProfiler sw____SetGaps;
static SProfiler sw___GetContigInst;
static SProfiler sw___GetContigDescr;
static SProfiler sw___GetContigIds;
static SProfiler sw__GetContigBioseq;
static SProfiler sw_GetContigEntry;
static SProfiler sw_FeatIterator;
static SProfiler sw_ProtIterator;
static SProfiler sw_ScafIterator;
static SProfiler sw_SeqIterator;
static SProfiler sw_WGSOpen;

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
            if ( data[0] == kSeq_descrFirstByte ) {
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
// CWGSDb_Impl cursors
/////////////////////////////////////////////////////////////////////////////


// SSeq0TableCursor is helper accessor structure for SEQUENCE table
struct CWGSDb_Impl::SSeq0TableCursor : public CObject {
    explicit SSeq0TableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(ACC_PREFIX);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, ACC_CONTIG_LEN);
    DECLARE_VDB_COLUMN_AS_STRING(SEQID_GNL_PREFIX);
    DECLARE_VDB_COLUMN_AS(Uint1, MOL);
    DECLARE_VDB_COLUMN_AS(NCBI_taxid, TAXID);
};


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
    DECLARE_VDB_COLUMN_AS_STRING(DESCR);
    DECLARE_VDB_COLUMN_AS_STRING(NUC_PROT_DESCR);
    DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
    DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
    DECLARE_VDB_COLUMN_AS_STRING(PUBLIC_COMMENT);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, GAP_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, GAP_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, GAP_PROPS);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, GAP_LINKAGE);
    DECLARE_VDB_COLUMN_AS(INSDC_quality_phred, QUALITY);
    DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_hash, HASH);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
    typedef pair<TVDBRowId, TVDBRowId> row_range_t;
    DECLARE_VDB_COLUMN_AS(row_range_t, CONTIG_NAME_ROW_RANGE);
    CVDBColumnBits<2> m_READ_2na;
    DECLARE_VDB_COLUMN_AS(Uint1, AMBIGUITY_MASK);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, AMBIGUITY_POS);
    DECLARE_VDB_COLUMN_AS(INSDC_4na_bin, AMBIGUITY_4NA);
};


// SSeqTableCursor is helper accessor structure for SEQUENCE table
struct CWGSDb_Impl::SSeq4naTableCursor : public CObject {
    explicit SSeq4naTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(INSDC_4na_bin, READ); // unpacked 4na, one base per byte
};


CWGSDb_Impl::SSeq0TableCursor::SSeq0TableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(ACC_PREFIX),
      INIT_VDB_COLUMN(ACC_CONTIG_LEN),
      INIT_OPTIONAL_VDB_COLUMN(SEQID_GNL_PREFIX),
      INIT_OPTIONAL_VDB_COLUMN(MOL),
      INIT_OPTIONAL_VDB_COLUMN(TAXID)
{
}


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
      INIT_OPTIONAL_VDB_COLUMN(DESCR),
      INIT_OPTIONAL_VDB_COLUMN(NUC_PROT_DESCR),
      INIT_OPTIONAL_VDB_COLUMN(ANNOT),
      INIT_OPTIONAL_VDB_COLUMN(GB_STATE),
      INIT_OPTIONAL_VDB_COLUMN(PUBLIC_COMMENT),
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
      m_READ_2na(m_Cursor, "(INSDC:2na:packed)READ",
                 NULL, CVDBColumn::eMissing_Allow), // packed 2na
      INIT_OPTIONAL_VDB_COLUMN(AMBIGUITY_MASK),
      INIT_OPTIONAL_VDB_COLUMN(AMBIGUITY_POS),
      INIT_OPTIONAL_VDB_COLUMN(AMBIGUITY_4NA)
{
    if ( !s_UseAmbiguityMask() ) {
        m_AMBIGUITY_MASK = CVDBColumnBits<8>();
    }
    if ( !s_UseGapInfo() ) {
        m_GAP_START = CVDBColumnBits<32>();
    }
    if ( s_UseAmbiguity4na() && m_GAP_START && m_GAP_LEN && m_AMBIGUITY_POS && m_AMBIGUITY_4NA ) {
        // all fields to restore ambiguities are present
    }
    else {
        // otherwise we need 4na data
        m_AMBIGUITY_POS.Reset();
        m_AMBIGUITY_4NA.Reset();
    }

    // optimization - treat completely empty QUALITY column as inexistent - no quality graphs
    m_QUALITY.ResetIfAlwaysEmpty(m_Cursor);
}


CWGSDb_Impl::SSeq4naTableCursor::SSeq4naTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN_AS(READ, INSDC:4na:bin)
{
}


// SScfTableCursor is helper accessor structure for optional SCAFFOLD table
struct CWGSDb_Impl::SScfTableCursor : public CObject {
    SScfTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(SCAFFOLD_NAME);
    DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS_STRING(SEQID_GNL_PREFIX);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, COMPONENT_ID);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_one, COMPONENT_START);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, COMPONENT_LEN);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, COMPONENT_PROPS);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, COMPONENT_LINKAGE);
    DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
    DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
};


CWGSDb_Impl::SScfTableCursor::SScfTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(SCAFFOLD_NAME),
      INIT_OPTIONAL_VDB_COLUMN(ACCESSION),
      INIT_OPTIONAL_VDB_COLUMN(SEQID_GNL_PREFIX),
      INIT_VDB_COLUMN(COMPONENT_ID),
      INIT_VDB_COLUMN(COMPONENT_START),
      INIT_VDB_COLUMN(COMPONENT_LEN),
      INIT_VDB_COLUMN(COMPONENT_PROPS),
      INIT_OPTIONAL_VDB_COLUMN(COMPONENT_LINKAGE),
      INIT_OPTIONAL_VDB_COLUMN(CIRCULAR),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_START),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_END),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(GB_STATE)
{
}


// SProt0TableCursor is helper accessor structure for optional PROTEIN table
struct CWGSDb_Impl::SProt0TableCursor : public CObject {
    explicit SProt0TableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;
    
    DECLARE_VDB_COLUMN_AS(NCBI_gi, GI);
    //DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS_STRING(GB_ACCESSION);
    DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
    DECLARE_VDB_COLUMN_AS_STRING(SEQID_GNL_PREFIX);
    DECLARE_VDB_COLUMN_AS_STRING(PROTEIN_NAME);
};


// SProtTableCursor is helper accessor structure for optional PROTEIN table
struct CWGSDb_Impl::SProtTableCursor : public CObject {
    explicit SProtTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;
    
    //DECLARE_VDB_COLUMN_AS(NCBI_gi, GI);
    //DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    //DECLARE_VDB_COLUMN_AS_STRING(GB_ACCESSION);
    //DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
    //DECLARE_VDB_COLUMN_AS_STRING(SEQID_GNL_PREFIX);
    DECLARE_VDB_COLUMN_AS_STRING(TITLE);
    DECLARE_VDB_COLUMN_AS_STRING(DESCR);
    DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
    DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
    DECLARE_VDB_COLUMN_AS_STRING(PUBLIC_COMMENT);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, PROTEIN_LEN);
    //DECLARE_VDB_COLUMN_AS_STRING(PROTEIN_NAME);
    DECLARE_VDB_COLUMN_AS_STRING(PRODUCT_NAME);
    DECLARE_VDB_COLUMN_AS(NCBI_taxid, TAXID);
    DECLARE_VDB_COLUMN_AS_STRING(REF_ACC);
    DECLARE_VDB_COLUMN_AS(NCBI_WGS_hash, HASH);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_START);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_ROW_END);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEAT_PRODUCT_ROW_ID);
    DECLARE_VDB_COLUMN_AS_STRING(PROTEIN);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, REPLACED_BY);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, REPLACES);
};


CWGSDb_Impl::SProt0TableCursor::SProt0TableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(GI),
      //INIT_VDB_COLUMN(ACCESSION),
      INIT_OPTIONAL_VDB_COLUMN(GB_ACCESSION),
      INIT_VDB_COLUMN(ACC_VERSION),
      INIT_OPTIONAL_VDB_COLUMN(SEQID_GNL_PREFIX),
      INIT_VDB_COLUMN(PROTEIN_NAME)
{
}


CWGSDb_Impl::SProtTableCursor::SProtTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      //INIT_OPTIONAL_VDB_COLUMN(GI),
      //INIT_VDB_COLUMN(ACCESSION),
      //INIT_OPTIONAL_VDB_COLUMN(GB_ACCESSION),
      //INIT_VDB_COLUMN(ACC_VERSION),
      //INIT_OPTIONAL_VDB_COLUMN(SEQID_GNL_PREFIX),
      INIT_OPTIONAL_VDB_COLUMN(TITLE),
      INIT_OPTIONAL_VDB_COLUMN(DESCR),
      INIT_OPTIONAL_VDB_COLUMN(ANNOT),
      INIT_VDB_COLUMN(GB_STATE),
      INIT_OPTIONAL_VDB_COLUMN(PUBLIC_COMMENT),
      INIT_VDB_COLUMN(PROTEIN_LEN),
      //INIT_VDB_COLUMN(PROTEIN_NAME),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_NAME),
      INIT_OPTIONAL_VDB_COLUMN(TAXID),
      INIT_OPTIONAL_VDB_COLUMN(REF_ACC),
      INIT_OPTIONAL_VDB_COLUMN(HASH),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_START),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_ROW_END),
      INIT_OPTIONAL_VDB_COLUMN(FEAT_PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PROTEIN),
      INIT_OPTIONAL_VDB_COLUMN(REPLACED_BY),
      INIT_OPTIONAL_VDB_COLUMN(REPLACES)
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

    CObjectIStreamAsnBinary m_ObjStr;
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
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_SEQ_TYPE),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_ACCESSION),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_START),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_LEN),
      INIT_VDB_COLUMN(SEQ_FEAT)
{
    CObjectTypeInfo type;
    if ( 1 ) {
        type = CObjectTypeInfo(CType<CObject_id>());
        type.FindVariant("str")
            .SetLocalReadHook(m_ObjStr, new CPackStringChoiceHook);
    }
    if ( 1 ) {
        type = CObjectTypeInfo(CType<CImp_feat>());
        type.FindMember("key")
            .SetLocalReadHook(m_ObjStr, new CPackStringClassHook(32, 128));
    }
    if ( 1 ) {
        type = CObjectTypeInfo(CType<CDbtag>());
        type.FindMember("db")
            .SetLocalReadHook(m_ObjStr, new CPackStringClassHook);
    }
    if ( 1 ) {
        type = CType<CGb_qual>();
        type.FindMember("qual")
            .SetLocalReadHook(m_ObjStr, new CPackStringClassHook);
    }
}


// SGiIdxTableCursor is helper accessor structure for optional GI_IDX table
struct CWGSDb_Impl::SGiIdxTableCursor : public CObject {
    explicit SGiIdxTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(TVDBRowId, NUC_ROW_ID);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, PROT_ROW_ID);
};


CWGSDb_Impl::SGiIdxTableCursor::SGiIdxTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(NUC_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PROT_ROW_ID)
{
}


// SProtIdxTableCursor is helper accessor structure for optional PROT_IDX table
struct CWGSDb_Impl::SProtIdxTableCursor : public CObject {
    explicit SProtIdxTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    typedef pair<TVDBRowId, TVDBRowId> row_range_t;
    DECLARE_VDB_COLUMN_AS(row_range_t, NAME_ROW_RANGE);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, ROW_ID);
};


CWGSDb_Impl::SProtIdxTableCursor::SProtIdxTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN_BACKUP(NAME_ROW_RANGE, ACCESSION_ROW_RANGE),
      INIT_VDB_COLUMN_BACKUP(ROW_ID, PROTEIN_ROW_ID)
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
      m_IdVersion(0),
      m_ScfTableIsOpened(false),
      m_ProtTableIsOpened(false),
      m_FeatTableIsOpened(false),
      m_GiIdxTableIsOpened(false),
      m_ProtIdxTableIsOpened(false),
      m_ProtAccIndexIsOpened(0),
      m_ContigNameIndexIsOpened(0),
      m_ScaffoldNameIndexIsOpened(0),
      m_ProteinNameIndexIsOpened(0),
      m_ProductNameIndexIsOpened(0),
      m_AmbiguityCache(s_GetAmbiguityCacheSize()),
      m_IsSetMasterDescr(false),
      m_HasNoDefaultGnlId(false),
      m_HasCommonTaxId(false),
      m_FeatLocIdType(eFeatLocIdUninitialized),
      m_ProjectGBState(NCBI_gb_state_eWGSGenBankLive),
      m_SeqIdType(CSeq_id::e_not_set),
      m_CommonTaxId(ZERO_TAX_ID)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_WGSOpen);
    //static CVDBSchema schema(mgr, "wgs.schema");
    m_Db = CVDB(mgr, m_WGSPath);
    m_SeqTable = CVDBTable(m_Db, "SEQUENCE"); // SEQUENCE table must exist
    x_InitIdParams();
}


CWGSDb_Impl::~CWGSDb_Impl(void)
{
}


inline
CRef<CWGSDb_Impl::SSeq0TableCursor> CWGSDb_Impl::Seq0(TVDBRowId row)
{
    CRef<SSeq0TableCursor> curs = m_Seq0.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        curs = new SSeq0TableCursor(SeqTable());
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SSeqTableCursor> CWGSDb_Impl::Seq(TVDBRowId row)
{
    CRef<SSeqTableCursor> curs = m_Seq.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        curs = new SSeqTableCursor(SeqTable());
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SSeq4naTableCursor> CWGSDb_Impl::Seq4na(TVDBRowId /*row*/)
{
    CRef<SSeq4naTableCursor> curs; // = m_Seq.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        curs = new SSeq4naTableCursor(SeqTable());
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SScfTableCursor> CWGSDb_Impl::Scf(TVDBRowId row)
{
    CRef<SScfTableCursor> curs = m_Scf.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        if ( const CVDBTable& table = ScfTable() ) {
            curs = new SScfTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SProt0TableCursor> CWGSDb_Impl::Prot0(TVDBRowId row)
{
    CRef<SProt0TableCursor> curs = m_Prot0.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        if ( const CVDBTable& table = ProtTable() ) {
            curs = new SProt0TableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SProtTableCursor> CWGSDb_Impl::Prot(TVDBRowId row)
{
    CRef<SProtTableCursor> curs = m_Prot.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
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
        CVDBMgr::CRequestContextUpdater ctx_updater;
        if ( const CVDBTable& table = FeatTable() ) {
            curs = new SFeatTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SGiIdxTableCursor> CWGSDb_Impl::GiIdx(TVDBRowId row)
{
    CRef<SGiIdxTableCursor> curs = m_GiIdx.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        if ( const CVDBTable& table = GiIdxTable() ) {
            curs = new SGiIdxTableCursor(table);
        }
    }
    return curs;
}


inline
CRef<CWGSDb_Impl::SProtIdxTableCursor> CWGSDb_Impl::ProtIdx(TVDBRowId row)
{
    CRef<SProtIdxTableCursor> curs = m_ProtIdx.Get(row);
    if ( !curs ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        if ( const CVDBTable& table = ProtIdxTable() ) {
            curs = new SProtIdxTableCursor(table);
        }
    }
    return curs;
}


inline
void CWGSDb_Impl::Put(CRef<SSeq0TableCursor>& curs, TVDBRowId row)
{
    m_Seq0.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SSeqTableCursor>& curs, TVDBRowId row)
{
    m_Seq.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SSeq4naTableCursor>& /*curs*/, TVDBRowId /*row*/)
{
    //m_Seq.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SScfTableCursor>& curs, TVDBRowId row)
{
    m_Scf.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SProt0TableCursor>& curs, TVDBRowId row)
{
    m_Prot0.Put(curs, row);
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
void CWGSDb_Impl::Put(CRef<SGiIdxTableCursor>& curs, TVDBRowId row)
{
    m_GiIdx.Put(curs, row);
}


inline
void CWGSDb_Impl::Put(CRef<SProtIdxTableCursor>& curs, TVDBRowId row)
{
    m_ProtIdx.Put(curs, row);
}


struct CWGSDb_Impl::SAmbiguityInfo : public CObject
{
    typedef CWGSDb_Impl::SSeqTableCursor SSeqTableCursor;
    
    SAmbiguityInfo(TVDBRowId row_id, CWGSDb_Impl& db, SSeqTableCursor& cur);
    ~SAmbiguityInfo();

    size_t GetUsedMemory() const;

    vector<Uint1> GetAmbiguityBytes(SSeqTableCursor& cur) {
        return m_AmbiguityMask;
    }

    typedef CWGSSeqIterator::TWGSContigGapInfo TWGSContigGapInfo;
    TWGSContigGapInfo GetGapInfo() const;

    TSeqPos Get2naLengthBlock(TSeqPos pos, TSeqPos len) const;
    TSeqPos Get4naLengthBlock(TSeqPos pos, TSeqPos len) const;

    TSeqPos Get2naLengthExact(TSeqPos pos, TSeqPos len,
                              CWGSDb_Impl& db, SSeqTableCursor& cur) const;
    TSeqPos Get4naLengthExact(TSeqPos pos, TSeqPos len,
                              TSeqPos stop_2na_len, TSeqPos stop_gap_len,
                              CWGSDb_Impl& db, SSeqTableCursor& cur) const;
    TSeqPos GetGapLengthExact(TSeqPos pos, TSeqPos len,
                              CWGSDb_Impl& db, SSeqTableCursor& cur) const;
    
    CRef<CSeq_data> Get2na(TSeqPos pos, TSeqPos len,
                           SSeqTableCursor& cur) const;
    CRef<CSeq_data> Get4na(TSeqPos pos, TSeqPos len,
                           CWGSDb_Impl& db, SSeqTableCursor& cur) const;
    
    bool x_AmbiguousBlock(size_t block_index) const
        {
            size_t byte_index = block_index/8;
            Uint1 byte_bit = 1<<(block_index%8);
            return byte_index < m_AmbiguityMask.size() && (m_AmbiguityMask[byte_index] & byte_bit);
        }
    void x_SetAmbiguousBlock(size_t block_index)
        {
            size_t byte_index = block_index/8;
            Uint1 byte_bit = 1<<(block_index%8);
            m_AmbiguityMask[byte_index] |= byte_bit;
        }
    void x_CalculateAmbiguityMask(CWGSDb_Impl& db);
    void x_Calculate4na(CWGSDb_Impl& db) const;
    void x_Need4na(CWGSDb_Impl& db) const
        {
            if ( !m_HasAmbiguityPos && !m_Has4naBlocks ) {
                x_Calculate4na(db);
            }
        }
    bool x_AddAmbiguities(const Uint1* ptr, TSeqPos count,
                          TSeqPos pos, TWGSContigGapInfo& gap_info) const;
    bool x_AddAmbiguousBlock(const Uint1* ptr, TSeqPos count,
                             TSeqPos pos, TWGSContigGapInfo& gap_info) const;

    string m_Prefix;
    TVDBRowId m_RowId;

    mutable CFastMutex m_Mutex; // for m_4naBlocks update
    
    bool m_HasGapInfo;
    bool m_HasAmbiguityMask;
    mutable bool m_HasAmbiguityPos;
    mutable bool m_Has4naBlocks;

    vector<INSDC_coord_zero> m_GapStart;
    vector<INSDC_coord_len> m_GapLen;
    vector<NCBI_WGS_component_props> m_GapProps;
    vector<NCBI_WGS_gap_linkage> m_GapLinkage;

    vector<Uint1> m_AmbiguityMask;
    mutable vector<INSDC_coord_zero> m_AmbiguityPos;
    mutable vector<INSDC_4na_bin> m_Ambiguity4na;

    struct S4naBlock
    {
        char m_Packed4na[kAmbiguityBlockSize/2]; // packed 4na - two 4na bases per byte
    };

    typedef map<TSeqPos, S4naBlock> T4naBlocks;
    mutable T4naBlocks m_4naBlocks; // ambiguous blocks
    
    struct S4naReader
    {
        TSeqPos m_Pos;
        size_t m_AmbiguityIndex;
        T4naBlocks::const_iterator m_4naBlocksIter;
    };
    bool x_IsValid(const S4naReader& reader) const;
    S4naReader Get4naReader(TSeqPos pos, CWGSDb_Impl& db, SSeqTableCursor& cur) const;
    enum EBaseType {
        eBase_2na,
        eBase_4na,
        eBase_Gap
    };
    EBaseType GetBaseType(const S4naReader& reader) const;
    void Advance(S4naReader& reader) const;
};


template<class Value>
static void sx_Assign(vector<Value>& dst, const CVDBValueFor<Value>& src)
{
    dst.resize(src.size());
    copy_n(src.begin(), src.size(), dst.data());
}


CWGSDb_Impl::SAmbiguityInfo::SAmbiguityInfo(TVDBRowId row_id, CWGSDb_Impl& db, SSeqTableCursor& cur)
    : m_Prefix(db.GetIdPrefixWithVersion()),
      m_RowId(row_id),
      m_HasGapInfo(false),
      m_HasAmbiguityMask(false),
      m_HasAmbiguityPos(false),
      m_Has4naBlocks(false)
{
    if ( cur.m_GAP_START ) {
        sx_Assign(m_GapStart, cur.GAP_START(m_RowId));
        if ( m_GapStart.size() ) {
            sx_Assign(m_GapLen, cur.GAP_LEN(m_RowId));
            sx_Assign(m_GapProps, cur.GAP_PROPS(m_RowId));
            if ( cur.m_GAP_LINKAGE ) {
                sx_Assign(m_GapLinkage, cur.GAP_LINKAGE(m_RowId));
            }
        }
        m_HasGapInfo = true;
    }
    const bool kVerify4na = false;
    vector<Uint1> m_ExpectedAmbiguityMask;
    vector<INSDC_coord_zero> m_ExpectedAmbiguityPos;
    vector<INSDC_4na_bin> m_ExpectedAmbiguity4na;
    
    if ( kVerify4na ) {
        x_CalculateAmbiguityMask(db);
        swap(m_ExpectedAmbiguityMask, m_AmbiguityMask);
        swap(m_ExpectedAmbiguityPos, m_AmbiguityPos);
        swap(m_ExpectedAmbiguity4na, m_Ambiguity4na);
        m_HasAmbiguityMask = false;
        m_HasAmbiguityPos = false;
    }
    if ( cur.m_AMBIGUITY_MASK ) {
        // number of blocks
        sx_Assign(m_AmbiguityMask, cur.AMBIGUITY_MASK(m_RowId));
        m_HasAmbiguityMask = true;
    }
    if ( cur.m_AMBIGUITY_POS && cur.m_AMBIGUITY_4NA ) {
        sx_Assign(m_AmbiguityPos, cur.AMBIGUITY_POS(m_RowId));
        sx_Assign(m_Ambiguity4na, cur.AMBIGUITY_4NA(m_RowId));
        m_HasAmbiguityPos = true;
    }
    if ( !m_HasAmbiguityMask ) {
        x_CalculateAmbiguityMask(db);
    }
    if ( s_GetDebugLevel() >= 6 ) {
        size_t memory = GetUsedMemory();
        size_t mask_bit_count = 0;
        for ( auto bb : m_AmbiguityMask ) {
            while ( bb ) {
                ++mask_bit_count;
                bb &= bb-1;
            }
        }
        CFastMutexGuard guard(m_Mutex);
        LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                 <<NStr::NumericToString(m_GapStart.size(),NStr::fWithCommas)<<" gaps, "
                 <<NStr::NumericToString(m_AmbiguityMask.size(),NStr::fWithCommas)<<" mask bytes, "
                 <<NStr::NumericToString(mask_bit_count,NStr::fWithCommas)<<" bits, "
                 <<NStr::NumericToString(m_Ambiguity4na.size(),NStr::fWithCommas)<<" ambig, "
                 <<"size: "<<NStr::NumericToString(memory,NStr::fWithCommas));
        if ( s_GetDebugLevel() >= 7 ) {
            for ( size_t i = 0; i < 2 && i < m_AmbiguityPos.size(); ++i ) {
                LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                         <<"ambiguity at "<<m_AmbiguityPos[i]<<" - "<<m_Ambiguity4na[i]*1);
            }
        }
    }
    if ( kVerify4na ) {
        x_Need4na(db);
        for ( size_t block_index = 0;
              block_index < 8*max(m_AmbiguityMask.size(), m_ExpectedAmbiguityMask.size());
              ++block_index ) {
            bool bit = x_AmbiguousBlock(block_index);
            bool exp_bit;
            {{
                size_t byte_index = block_index/8;
                Uint1 byte_bit = 1<<(block_index%8);
                exp_bit = byte_index < m_ExpectedAmbiguityMask.size() &&
                    (m_ExpectedAmbiguityMask[byte_index] & byte_bit);
            }}
            if ( bit != exp_bit ) {
                LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "<<
                         "mask["<<block_index<<" = "<<oct<<block_index<<dec<<"] "<<bit<<", expected "<<exp_bit);
            }
        }
        size_t index = 0, exp_index = 0;
        while ( index < m_AmbiguityPos.size() || exp_index < m_ExpectedAmbiguityPos.size() ) {
            TSeqPos pos = index < m_AmbiguityPos.size()? m_AmbiguityPos[index]: kInvalidSeqPos;
            int base = index < m_AmbiguityPos.size()? m_Ambiguity4na[index]: 0;
            TSeqPos exp_pos = exp_index < m_ExpectedAmbiguityPos.size()? m_ExpectedAmbiguityPos[exp_index]: kInvalidSeqPos;
            int exp_base = exp_index < m_ExpectedAmbiguityPos.size()? m_ExpectedAmbiguity4na[exp_index]: 0;
            if ( pos == exp_pos ) {
                if ( base != exp_base ) {
                    LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "<<
                             "amb["<<pos<<" = "<<oct<<pos<<dec<<"] "<<base<<", expected "<<exp_base);
                }
                ++index;
                ++exp_index;
            }
            else if ( pos < exp_pos ) {
                LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "<<
                         "amb["<<pos<<" = "<<oct<<pos<<dec<<"] "<<base<<", expected -");
                ++index;
            }
            else {
                LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "<<
                         "amb["<<exp_pos<<" = "<<oct<<exp_pos<<dec<<"] -, expected "<<exp_base);
                ++exp_index;
            }
        }
    }
}


CWGSDb_Impl::SAmbiguityInfo::~SAmbiguityInfo()
{
    if ( s_GetDebugLevel() >= 6 ) {
        size_t memory = GetUsedMemory();
        CFastMutexGuard guard(m_Mutex);
        LOG_POST("~SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                 <<"final size: "<<NStr::NumericToString(memory,NStr::fWithCommas));
    }
}


size_t CWGSDb_Impl::SAmbiguityInfo::GetUsedMemory() const
{
    const size_t kAllocateGap = sizeof(void*)*2;
    size_t ret = kAllocateGap + sizeof(*this);
    ret += kAllocateGap + m_GapStart.size()*sizeof(m_GapStart.front());
    ret += kAllocateGap + m_GapLen.size()*sizeof(m_GapLen.front());
    ret += kAllocateGap + m_GapProps.size()*sizeof(m_GapProps.front());
    ret += kAllocateGap + m_GapLinkage.size()*sizeof(m_GapLinkage.front());
    ret += kAllocateGap + m_AmbiguityMask.size()*sizeof(m_AmbiguityMask.front());
    if ( m_HasAmbiguityPos || m_Has4naBlocks ) {
        CFastMutexGuard guard(m_Mutex);
        ret += kAllocateGap + m_AmbiguityPos.size()*sizeof(m_AmbiguityPos.front());
        ret += kAllocateGap + m_Ambiguity4na.size()*sizeof(m_Ambiguity4na.front());
        const size_t kBlockUsedMemory =
            kAllocateGap + 4*sizeof(void*) + sizeof(S4naBlock); // including map overhead
        ret += kBlockUsedMemory * m_4naBlocks.size();
    }
    return ret;
}


CWGSDb_Impl::SAmbiguityInfo::TWGSContigGapInfo
CWGSDb_Impl::SAmbiguityInfo::GetGapInfo() const
{
    TWGSContigGapInfo gap_info;
    gap_info.gaps_count = m_GapStart.size();
    gap_info.gaps_start = m_GapStart.data();
    gap_info.gaps_len = m_GapLen.data();
    gap_info.gaps_props = m_GapProps.data();
    gap_info.gaps_linkage = m_GapLinkage.data();
    return gap_info;
}


static const bool kRecoverGaps = true;


// 2na encoding has values 0-3, occupying 2 bits
// 4na encoding has values 0-15, occupying 4 bits
// unpacked 4na values occupy one base per byte
// packed 4na bases are stored two per byte, first base in highest bits
// packed 2na bases are stored four per byte, first base in highest bits

// return true if the 4na value is unambiguous
static inline
bool sx_Is2na(Uint1 b)
{
    return b && !(b&(b-1));
}


// return pointer to the first ambiguity in an unpacked 4na array
static inline
const Uint1* sx_FindAmbiguity(const Uint1* ptr, const Uint1* end)
{
    for ( ; ptr != end; ++ptr ) {
        if ( !sx_Is2na(*ptr) ) {
            return ptr;
        }
    }
    return ptr;
}


static inline
Uint1 sx_Get_4na(const char* ptr, size_t offset)
{
    Uint1 b = ptr[offset/2];
    if ( offset%2 == 0 ) {
        b = b >> 4;
    }
    return b & 0xf;
}


// return pointer to the first ambiguity in an unpacked 4na array
static inline
size_t sx_Find_4na_Ambiguity(const char* ptr, size_t offset, size_t base_count)
{
    for ( size_t i = offset; i < offset+base_count; ++i ) {
        if ( !sx_Is2na(sx_Get_4na(ptr, i)) ) {
            return i;
        }
    }
    return offset+base_count;
}


// check if unpacked 4na array has any ambiguity
static inline
bool sx_HasAmbiguity(const Uint1* ptr, const Uint1* end)
{
    return sx_FindAmbiguity(ptr, end) != end;
}


// check if unpacked 4na array has any ambiguity beside explicit gaps
static inline
bool sx_HasAmbiguity(const Uint1* ptr, TSeqPos count,
                     TSeqPos pos, CWGSDb_Impl::SAmbiguityInfo::TWGSContigGapInfo& gap_info)
{
    while ( count ) {
        gap_info.SetPos(pos);
        if ( gap_info.IsInGap(pos) ) {
            // skip gap
            TSeqPos gap_len = gap_info.GetGapLength(pos, count);
            ptr += gap_len;
            pos += gap_len;
            count -= gap_len;
        }
        else {
            TSeqPos na_len = gap_info.GetDataLength(pos, count);
            if ( sx_HasAmbiguity(ptr, ptr+na_len) ) {
                return true;
            }
            ptr += na_len;
            pos += na_len;
            count -= na_len;
        }
    }
    return false;
}


// convert 4na base to 2na base
static inline
Uint1 sx_To2na(Uint1 b)
{
    static const char s_4na_to_2na_table[16] = {
        0, 0, 1, 0,
        2, 0, 0, 0,
        3, 0, 0, 0,
        0, 0, 0, 0
    };
    return s_4na_to_2na_table[b];
}


// convert 2 bases of packed 2na byte into packed 4na byte
static
inline
char s_ConvertBits_2na_to_4na(char bits_2na)
{
    static const unsigned char table[16] = {
        0x11, 0x12, 0x14, 0x18,
        0x21, 0x22, 0x24, 0x28,
        0x41, 0x42, 0x44, 0x48,
        0x81, 0x82, 0x84, 0x88
    };
    return table[bits_2na & 0xf];
}


// convert first 2 bases of packed 2na byte into packed 4na byte
static
inline
char s_ConvertBits_2na_to_4na_1st(char bits_2na)
{
    return s_ConvertBits_2na_to_4na(bits_2na >> 4);
}


// convert last 2 bases of packed 2na byte into packed 4na byte
static
inline
char s_ConvertBits_2na_to_4na_2nd(char bits_2na)
{
    return s_ConvertBits_2na_to_4na(bits_2na);
}


// convert packed 2na (4 bases per byte) array into packed 4na (2 bases per byte) array
static
void s_Convert_2na_to_4na(char* dst_4na, const char* src_2na, size_t base_count)
{
    while ( base_count >= 4 ) {
        char bits_2na = src_2na[0];
        dst_4na[0] = s_ConvertBits_2na_to_4na_1st(bits_2na);
        dst_4na[1] = s_ConvertBits_2na_to_4na_2nd(bits_2na);
        base_count -= 4;
        src_2na += 1;
        dst_4na += 2;
    }
    if ( base_count ) {
        char bits_2na = src_2na[0] & (0xff00 >> base_count*2);
        {{
            char bits_4na = s_ConvertBits_2na_to_4na_1st(bits_2na);
            if ( base_count < 2 ) {
                bits_4na &= 0xf0;
            }
            dst_4na[0] = bits_4na;
        }}
        if ( base_count > 2 ) {
            dst_4na[1] = s_ConvertBits_2na_to_4na_2nd(bits_2na) & 0xf0;
        }
    }
}


// convert packed 2na (4 bases per byte) vector into packed 4na (2 bases per byte) vector
static
void s_Convert_2na_to_4na(vector<char>& dst_4na_vec,
                          const vector<char>& src_2na_vec,
                          size_t base_count)
{
    size_t dst_4na_byte_count = (base_count+1)/2;
    // allocate 8-byte aligned memory to allow multi-byte operations at end
    dst_4na_vec.reserve((dst_4na_byte_count+7)/8*8);
    dst_4na_vec.resize(dst_4na_byte_count);
    s_Convert_2na_to_4na(dst_4na_vec.data(), src_2na_vec.data(), base_count);
}


// set 4na value into a packed 4na vector
static
inline
void s_Set_4na(vector<char>& dst_4na_vec,
               size_t offset,
               INSDC_4na_bin amb)
{
    char& dst = dst_4na_vec[offset/2];
    if ( offset%2 == 0 ) {
        dst = (dst & 0xf) | (amb << 4);
    }
    else {
        dst = (dst & 0xf0) | amb;
    }
}


// set 4na gap of specified length into a packed 4na vector
static
inline
void s_Set_4na_gap(vector<char>& dst_4na_vec,
                   size_t offset,
                   size_t len)
{
    char* dst = dst_4na_vec.data()+ (offset/2);
    if ( len && offset%2 == 1 ) {
        // start with odd gap base
        *dst |= 0xf;
        --len;
        ++dst;
    }
    while ( len >= 2 ) {
        *dst = char(0xff);
        len -= 2;
        ++dst;
    }
    if ( len ) {
        // end with odd gap base
        *dst |= 0xf0;
    }
}


// copy 4na bases with arbitrary offset
static
void s_Copy_4na(char* dst_4na, TSeqPos dst_offset,
                const char* src_4na, TSeqPos src_offset,
                size_t base_count)
{
    if ( !base_count ) {
        return;
    }
    dst_4na += dst_offset/2;
    dst_offset %= 2;
    src_4na += src_offset/2;
    src_offset %= 2;
    // copy first odd dst base
    if ( dst_offset != 0 ) {
        Uint1 dst_b = dst_4na[0];
        Uint1 src_b = src_4na[0];
        src_4na += src_offset;
        if ( !src_offset ) {
            src_b = src_b >> 4;
        }
        src_offset ^= 1;
        dst_b = (dst_b & 0xf0) | (src_b & 0xf);
        dst_4na[0] = dst_b;
        ++dst_4na;
        dst_offset = 0;
        --base_count;
    }
    // copy pairs of bases
    if ( src_offset == 0 ) {
        size_t copy_bytes = base_count / 2;
        dst_4na = copy_n(src_4na, copy_bytes, dst_4na);
        src_4na += copy_bytes;
        base_count %= 2;
    }
    else {
        while ( base_count >= 2 ) {
            Uint1 src_b0 = src_4na[0];
            Uint1 src_b1 = src_4na[1];
            Uint1 dst_b = (src_b0 << 4) | (src_b1 >> 4);
            dst_4na[0] = dst_b;
            ++src_4na;
            ++dst_4na;
            base_count -= 2;
        }
    }
    // copy last odd base
    if ( base_count ) {
        Uint1 dst_b = dst_4na[0];
        Uint1 src_b = src_4na[0];
        if ( src_offset ) {
            src_b = src_b << 4;
        }
        dst_b = (dst_b & 0xf) | (src_b & 0xf0);
        dst_4na[0] = dst_b;
    }
}


// convert unpacked 4na (1 base per byte) array into packed 4na (2 bases per byte) array
static
void s_Pack_4na(char* dst_packed_4na,
                const Uint1* src_4na,
                size_t base_count)
{
    while ( base_count >= 2 ) {
        auto b0 = src_4na[0];
        auto b1 = src_4na[1];
        auto packed_bb = (b0 << 4)+b1;
        *dst_packed_4na = packed_bb;
        base_count -= 2;
        src_4na += 2;
        ++dst_packed_4na;
    }
    if ( base_count ) {
        auto b0 = src_4na[0];
        auto packed_bb = (b0 << 4);
        *dst_packed_4na = packed_bb;
    }
}


static
void s_SetAmbiguitiesPos(vector<char>& dst_4na_vec,
                         TSeqPos pos, TSeqPos len,
                         const vector<INSDC_coord_zero>& amb_pos,
                         const vector<INSDC_4na_bin>& amb_4na)
{
    auto iter_pos = lower_bound(amb_pos.begin(), amb_pos.end(), INSDC_coord_zero(pos));
    auto iter_4na = amb_4na.begin() + (iter_pos-amb_pos.begin());
    INSDC_coord_zero end = pos + len;
    for ( ; iter_pos != amb_pos.end() && *iter_pos < end; ++iter_pos, ++iter_4na ) {
        s_Set_4na(dst_4na_vec, *iter_pos-pos, *iter_4na);
    }
}


static
void s_SetAmbiguitiesBlocks(vector<char>& dst_4na_vec,
                            TSeqPos pos, TSeqPos len,
                            const CWGSDb_Impl::SAmbiguityInfo::T4naBlocks& blocks)
{
    TSeqPos end = pos+len;
    TSeqPos block_pos = pos - pos%kAmbiguityBlockSize;
    for ( auto iter = blocks.lower_bound(block_pos);
          iter != blocks.end() && iter->first < end;
          ++iter ) {
        TSeqPos block_pos = iter->first;
        TSeqPos dst_offset;
        TSeqPos src_offset;
        TSeqPos copy_len;
        if ( block_pos < pos ) {
            dst_offset = 0;
            src_offset = pos-block_pos;
            copy_len = min(len, kAmbiguityBlockSize-src_offset);
        }
        else {
            dst_offset = block_pos-pos;
            src_offset = 0;
            copy_len = min(end-block_pos, kAmbiguityBlockSize);
        }
        s_Copy_4na(dst_4na_vec.data(), dst_offset, iter->second.m_Packed4na, src_offset, copy_len);
    }
}


static
void s_SetGaps(vector<char>& dst_4na_vec,
               TSeqPos pos, TSeqPos len,
               CWGSSeqIterator::TWGSContigGapInfo gap_info)
{
    TSeqPos pos0 = pos;
    gap_info.SetPos(pos);
    for ( ; len > 0; ) {
        if ( gap_info.IsInGap(pos) ) {
            // add gap
            TSeqPos gap_len = gap_info.GetGapLength(pos, len);
            _ASSERT(gap_len <= len);
            s_Set_4na_gap(dst_4na_vec, pos-pos0, gap_len);
            ++gap_info;
            len -= gap_len;
            pos += gap_len;
            _ASSERT(!gap_info || pos <= gap_info.GetFrom());
        }
        else {
            // data segment
            TSeqPos rem_len = gap_info.GetDataLength(pos, len);
            _ASSERT(rem_len <= len);
            len -= rem_len;
            pos += rem_len;
        }
    }
}


bool CWGSDb_Impl::SAmbiguityInfo::x_AddAmbiguousBlock(const Uint1* ptr, TSeqPos count,
                                                      TSeqPos pos, TWGSContigGapInfo& gap_info) const
{
    bool ambiguous = sx_HasAmbiguity(ptr, count, pos, gap_info);
    if ( ambiguous ) {
        s_Pack_4na(m_4naBlocks[pos].m_Packed4na, ptr, count);
    }
    return ambiguous;
}


bool CWGSDb_Impl::SAmbiguityInfo::x_AddAmbiguities(const Uint1* ptr, TSeqPos count,
                                                   TSeqPos pos, TWGSContigGapInfo& gap_info) const
{
    bool ambiguous = false;
    while ( count ) {
        gap_info.SetPos(pos);
        if ( gap_info.IsInGap(pos) ) {
            // skip gap
            TSeqPos gap_len = gap_info.GetGapLength(pos, count);
            ptr += gap_len;
            pos += gap_len;
            count -= gap_len;
        }
        else {
            TSeqPos na_len = gap_info.GetDataLength(pos, count);
            for ( TSeqPos i = 0; i < na_len; ++i ) {
                auto b = ptr[i];
                if ( !sx_Is2na(b) ) {
                    ambiguous = true;
                    m_AmbiguityPos.push_back(pos+i);
                    m_Ambiguity4na.push_back(b);
                }
            }
            ptr += na_len;
            pos += na_len;
            count -= na_len;
        }
    }
    return ambiguous;
}


void CWGSDb_Impl::SAmbiguityInfo::x_CalculateAmbiguityMask(CWGSDb_Impl& db)
{
    if ( m_HasAmbiguityMask ) {
        return;
    }
    // calculate ambiguity mask using 4na read
    if ( m_HasAmbiguityPos ) {
        PROFILE(sw____GetAmb2Mask);
        // it's faster to use ambiguity position list if present
        if ( size_t ambiguity_count = m_AmbiguityPos.size() ) {
            size_t last_block_index = m_AmbiguityPos.back() / kAmbiguityBlockSize;
            size_t last_byte_index = last_block_index/8;
            m_AmbiguityMask.resize(last_byte_index+1);
            for ( size_t i = 0; i < ambiguity_count; ++i ) {
                x_SetAmbiguousBlock(m_AmbiguityPos[i] / kAmbiguityBlockSize);
            }
        }
        if ( s_GetDebugLevel() >= 6 ) {
            size_t memory = GetUsedMemory();
            size_t mask_bit_count = 0;
            for ( auto bb : m_AmbiguityMask ) {
                while ( bb ) {
                    ++mask_bit_count;
                    bb &= bb-1;
                }
            }
            CFastMutexGuard guard(m_Mutex);
            LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                     <<"calculated mask from ambiguities, "
                     <<NStr::NumericToString(m_AmbiguityMask.size(),NStr::fWithCommas)<<" mask bytes, "
                     <<NStr::NumericToString(mask_bit_count,NStr::fWithCommas)<<" bits, "
                     <<"size: "<<NStr::NumericToString(memory,NStr::fWithCommas));
        }
    }
    else {
        // we'll have to scan for ambiguities in 4na data

        // use full 4na blocks or individual 4na points
        bool use_full_4na_blocks = s_UseFull4naBlocks();
        
        CRef<SSeq4naTableCursor> cur4na;
        CVDBValueFor<INSDC_4na_bin> read4na;
        {{
            PROFILE(sw____GetRaw4na);
            cur4na = db.Seq4na(m_RowId);
            read4na = cur4na->READ(m_RowId);
        }}
        
        PROFILE(sw____Get4na2Mask);
        TSeqPos read_length = TSeqPos(read4na.size());
        size_t block_count = (read_length+kAmbiguityBlockSize-1) / kAmbiguityBlockSize;
        size_t mask_bit_count = 0;
        m_AmbiguityMask.resize((block_count+7)/8);
        TWGSContigGapInfo gap_info = GetGapInfo();
        for ( size_t block_index = 0; block_index < block_count; ++block_index ) {
            TSeqPos block_pos = TSeqPos(block_index*kAmbiguityBlockSize);
            const Uint1* base_ptr = read4na.data() + block_pos;
            TSeqPos base_count = min(kAmbiguityBlockSize, read_length-block_pos);
            bool ambiguous = false;
            if ( use_full_4na_blocks ) {
                ambiguous = x_AddAmbiguousBlock(base_ptr, base_count, block_pos, gap_info);
            }
            else {
                ambiguous = x_AddAmbiguities(base_ptr, base_count, block_pos, gap_info);
            }
            if ( ambiguous ) {
                x_SetAmbiguousBlock(block_index);
                ++mask_bit_count;
            }
        }
        if ( use_full_4na_blocks ) {
            m_Has4naBlocks = true;
        }
        else {
            m_HasAmbiguityPos = true;
        }
        // db.Put(cur4na, m_RowId); do not store 4na cursor in cache to free memory
        if ( s_GetDebugLevel() >= 6 ) {
            size_t memory = GetUsedMemory();
            CFastMutexGuard guard(m_Mutex);
            LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                     "calculated mask from read, "
                     <<NStr::NumericToString(m_AmbiguityMask.size(),NStr::fWithCommas)<<" mask bytes, "
                     <<NStr::NumericToString(mask_bit_count,NStr::fWithCommas)<<" bits, "
                     <<NStr::NumericToString(m_Ambiguity4na.size(),NStr::fWithCommas)<<" ambig, "
                     <<NStr::NumericToString(m_4naBlocks.size(),NStr::fWithCommas)<<" blocks, "
                     <<"size: "<<NStr::NumericToString(memory,NStr::fWithCommas));
        }
    }
    m_HasAmbiguityMask = true;
}


void CWGSDb_Impl::SAmbiguityInfo::x_Calculate4na(CWGSDb_Impl& db) const
{
    CFastMutexGuard guard(m_Mutex);
    if ( m_HasAmbiguityPos || m_Has4naBlocks ) {
        return;
    }
    
    // use full 4na blocks or individual 4na points
    bool use_full_4na_blocks = s_UseFull4naBlocks();
    
    CRef<SSeq4naTableCursor> cur4na;
    CVDBValueFor<INSDC_4na_bin> read4na;
    TSeqPos read_length = 0;
    size_t bit_count = 0;
    size_t wrong_bit_count = 0;
    TWGSContigGapInfo gap_info = GetGapInfo();
    for ( size_t block_byte = 0; block_byte < m_AmbiguityMask.size(); ++block_byte ) {
        if ( auto bits = m_AmbiguityMask[block_byte] ) {
            if ( !cur4na ) {
                PROFILE(sw____GetRaw4na);
                cur4na = db.Seq4na(m_RowId);
                read4na = cur4na->READ(m_RowId);
                read_length = TSeqPos(read4na.size());
            }
            for ( size_t block_bit = 0; block_bit < 8; ++block_bit ) {
                if ( bits & (1<<block_bit) ) {
                    PROFILE(sw____Scan4na);
                    size_t block_index = block_byte*8+block_bit;
                    TSeqPos block_pos = TSeqPos(block_index * kAmbiguityBlockSize);
                    const Uint1* base_ptr = read4na.data() + block_pos;
                    TSeqPos base_count = min(kAmbiguityBlockSize, read_length-block_pos);
                    bool ambiguous = false;
                    gap_info.SetPos(block_pos);
                    if ( use_full_4na_blocks ) {
                        ambiguous = x_AddAmbiguousBlock(base_ptr, base_count, block_pos, gap_info);
                    }
                    else {
                        ambiguous = x_AddAmbiguities(base_ptr, base_count, block_pos, gap_info);
                    }
                    if ( ambiguous ) {
                        ++bit_count;
                    }
                    else {
                        ++wrong_bit_count;
                        if ( s_GetDebugLevel() >= 7  && wrong_bit_count <= 2 ) {
                            LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                                     <<"wrong bit set at "<<block_pos);
                        }
                    }
                }
            }
        }
    }
    if ( use_full_4na_blocks ) {
        m_Has4naBlocks = true;
    }
    else {
        m_HasAmbiguityPos = true;
    }
    // db.Put(cur4na, m_RowId); do not store 4na cursor in cache to free memory
    if ( s_GetDebugLevel() >= 6 ) {
        guard.Release();
        size_t memory = GetUsedMemory();
        CFastMutexGuard guard(m_Mutex);
        LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                 <<"calculated 4na, "
                 <<NStr::NumericToString(read_length,NStr::fWithCommas)<<" bases, "
                 <<NStr::NumericToString(bit_count,NStr::fWithCommas)<<" bits, "
                 <<NStr::NumericToString(m_Ambiguity4na.size(),NStr::fWithCommas)<<" ambig, "
                 <<NStr::NumericToString(m_4naBlocks.size(),NStr::fWithCommas)<<" blocks, "
                 <<"size: "<<NStr::NumericToString(memory,NStr::fWithCommas));
        if ( s_GetDebugLevel() >= 7 ) {
            for ( size_t i = 0; i < 2 && i < m_AmbiguityPos.size(); ++i ) {
                LOG_POST("SAmbiguityInfo("<<m_Prefix<<"/"<<m_RowId<<") "
                         <<"ambiguity at "<<m_AmbiguityPos[i]<<" - "<<m_Ambiguity4na[i]*1);
            }
        }
    }
}


bool CWGSDb_Impl::SAmbiguityInfo::x_IsValid(const S4naReader& reader) const
{
    if ( m_HasAmbiguityPos ) {
        // use explicit ambiguities list
        _ASSERT(reader.m_AmbiguityIndex == m_AmbiguityPos.size() ||
                (reader.m_AmbiguityIndex < m_AmbiguityPos.size() &&
                 reader.m_Pos <= TSeqPos(m_AmbiguityPos[reader.m_AmbiguityIndex])));
    }
    else {
        // use 4na blocks
        _ASSERT(reader.m_4naBlocksIter == m_4naBlocks.end() ||
                (reader.m_Pos < reader.m_4naBlocksIter->first + kAmbiguityBlockSize));
    }
    return true;
}


CWGSDb_Impl::SAmbiguityInfo::S4naReader
CWGSDb_Impl::SAmbiguityInfo::Get4naReader(TSeqPos pos,
                                          CWGSDb_Impl& db, SSeqTableCursor& cur) const
{
    x_Need4na(db);
    S4naReader reader;
    reader.m_Pos = pos;
    if ( m_HasAmbiguityPos ) {
        // use explicit ambiguities list
        reader.m_AmbiguityIndex =
            lower_bound(m_AmbiguityPos.begin(), m_AmbiguityPos.end(), INSDC_coord_zero(pos)) - m_AmbiguityPos.begin();
    }
    else {
        // use 4na blocks
        TSeqPos block_pos = pos - pos%kAmbiguityBlockSize;
        reader.m_4naBlocksIter = m_4naBlocks.lower_bound(block_pos);
    }
    return reader;
}


CWGSDb_Impl::SAmbiguityInfo::EBaseType
CWGSDb_Impl::SAmbiguityInfo::GetBaseType(const S4naReader& reader) const
{
    _ASSERT(x_IsValid(reader));
    Uint1 base;
    if ( m_HasAmbiguityPos ) {
        // use explicit ambiguities list
        if ( reader.m_AmbiguityIndex == m_AmbiguityPos.size() ) {
            // no more ambiguities
            return eBase_2na;
        }
        // check if next ambiguity is at current position
        if ( reader.m_Pos != TSeqPos(m_AmbiguityPos[reader.m_AmbiguityIndex]) ) {
            // not an ambiguity yet
            return eBase_2na;
        }
        base = m_Ambiguity4na[reader.m_AmbiguityIndex];
    }
    else {
        // use 4na blocks
        if ( reader.m_4naBlocksIter == m_4naBlocks.end() ) {
            // no more 4na block
            return eBase_2na;
        }
        if ( reader.m_4naBlocksIter->first > reader.m_Pos ) {
            // not in a 4na block yet
            return eBase_2na;
        }
        // check actual 4na base
        TSeqPos offset = reader.m_Pos - reader.m_4naBlocksIter->first;
        base = sx_Get_4na(reader.m_4naBlocksIter->second.m_Packed4na, offset);
    }
    return base == 0xf? eBase_Gap: sx_Is2na(base)? eBase_2na: eBase_4na;
}


void CWGSDb_Impl::SAmbiguityInfo::Advance(S4naReader& reader) const
{
    _ASSERT(x_IsValid(reader));
    // advance
    ++reader.m_Pos;
    // update iterators
    if ( m_HasAmbiguityPos ) {
        // use explicit ambiguities list
        if ( reader.m_AmbiguityIndex == m_AmbiguityPos.size() ) {
            // no more ambiguities
        }
        else {
            // check if next ambiguity was at current position
            if ( reader.m_Pos > TSeqPos(m_AmbiguityPos[reader.m_AmbiguityIndex]) ) {
                // advance to next ambiguity
                ++reader.m_AmbiguityIndex;
            }
        }
    }
    else {
        // use 4na blocks
        if ( reader.m_4naBlocksIter == m_4naBlocks.end() ) {
            // no more 4na blocks
        }
        else {
            // check if we move out of current 4na block
            if ( reader.m_Pos >= reader.m_4naBlocksIter->first + kAmbiguityBlockSize ) {
                // advance to next 4na block
                ++reader.m_4naBlocksIter;
            }
        }
    }
    _ASSERT(x_IsValid(reader));
}


TSeqPos CWGSDb_Impl::SAmbiguityInfo::Get2naLengthExact(TSeqPos pos, TSeqPos len,
                                                       CWGSDb_Impl& db, SSeqTableCursor& cur) const
{
    x_Need4na(db);
    PROFILE(sw____Get2naLen);
    TSeqPos end = pos+len;
    if ( m_HasAmbiguityPos ) {
        auto iter = lower_bound(m_AmbiguityPos.begin(), m_AmbiguityPos.end(), INSDC_coord_zero(pos));
        if ( iter == m_AmbiguityPos.end() || TSeqPos(*iter) >= end ) {
            return len;
        }
        return *iter - pos;
    }
    else {
        // use 4na blocks
        TSeqPos block_pos = pos - pos%kAmbiguityBlockSize;
        for ( auto block_iter = m_4naBlocks.lower_bound(block_pos);
              block_iter != m_4naBlocks.end() && block_iter->first < end;
              ++block_iter ) {
            size_t in_block_pos = pos <= block_iter->first? 0: pos-block_iter->first;
            size_t in_block_len = min(kAmbiguityBlockSize, end-block_iter->first);
            TSeqPos amb_pos = TSeqPos(sx_Find_4na_Ambiguity(block_iter->second.m_Packed4na,
                                                            in_block_pos, in_block_len));
            if ( amb_pos < in_block_pos+in_block_len ) {
                return (block_iter->first+amb_pos) - pos;
            }
        }
        return len;
    }
}


// Calculate 4na length with gap recovering
TSeqPos CWGSDb_Impl::SAmbiguityInfo::Get4naLengthExact(TSeqPos pos, TSeqPos len,
                                                       TSeqPos stop_2na_len,
                                                       TSeqPos stop_gap_len,
                                                       CWGSDb_Impl& db, SSeqTableCursor& cur) const
{
    PROFILE(sw____Get4naLen);
    if ( len < stop_2na_len ) {
        return len;
    }
    S4naReader reader = Get4naReader(pos, db, cur);
    TSeqPos rem_len = len, len2na = 0, gap_len = 0;
    // |-------------------- len -----------------|
    // |- 4na -|- len2na -|- gap_len -$- rem_len -|
    // $ is current position
    // only one of len2na and gap_len can be above zero

    for ( ; rem_len; --rem_len, Advance(reader) ) {
        auto base_type = GetBaseType(reader);
        if ( base_type == eBase_2na ) {
            if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                return len-(rem_len+len2na);
            }
            ++len2na;
            if ( kRecoverGaps ) {
                gap_len = 0;
            }
        }
        else {
            if ( kRecoverGaps && (base_type == eBase_Gap) ) {
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


TSeqPos CWGSDb_Impl::SAmbiguityInfo::GetGapLengthExact(TSeqPos pos, TSeqPos len,
                                                       CWGSDb_Impl& db, SSeqTableCursor& cur) const
{
    PROFILE(sw____GetGapLen);
    S4naReader reader = Get4naReader(pos, db, cur);
    TSeqPos rem_len = len;
    for ( ; rem_len; --rem_len, Advance(reader) ) {
        // check both bases
        auto base_type = GetBaseType(reader);
        if ( base_type != eBase_Gap ) {
            return len-rem_len;
        }
    }
    return len;
}


// Return 2na Seq-data for specified range.
// The data mustn't have ambiguities.
CRef<CSeq_data> CWGSDb_Impl::SAmbiguityInfo::Get2na(TSeqPos pos, TSeqPos len,
                                                    SSeqTableCursor& cur) const
{
    PROFILE(sw____GetRaw2na);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi2na().Set();
    size_t bytes = (len+3)/4;
    // allocate 8-byte aligned memory to allow multi-byte operations at end
    data.reserve((bytes+7)/8*8);
    data.resize(bytes);
    cur.m_Cursor.ReadElements(m_RowId, cur.m_READ_2na, 2, pos, len,
                              data.data());
    return ret;
}


// return 4na Seq-data for specified range
CRef<CSeq_data> CWGSDb_Impl::SAmbiguityInfo::Get4na(TSeqPos pos, TSeqPos len,
                                                    CWGSDb_Impl& db, SSeqTableCursor& cur) const
{
    x_Need4na(db);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi4na().Set();
    {{
        auto seq_2na = Get2na(pos, len, cur);
        PROFILE(sw____GetCvt4na);
        s_Convert_2na_to_4na(data, seq_2na->GetNcbi2na().Get(), len);
    }}
    if ( m_HasAmbiguityPos ) {
        // restore 4na by adding ambiguous bases to 2na
        PROFILE(sw____GetAmb4na);
        // set ambiguities
        s_SetAmbiguitiesPos(data, pos, len, m_AmbiguityPos, m_Ambiguity4na);
    }
    else {
        // restore 4na by adding ambiguous blocks to 2na
        PROFILE(sw____GetBlk4na);
        s_SetAmbiguitiesBlocks(data, pos, len, m_4naBlocks);
    }
    {{
        PROFILE(sw____SetGaps);
        s_SetGaps(data, pos, len, GetGapInfo());
    }}
    return ret;
}


TSeqPos CWGSDb_Impl::SAmbiguityInfo::Get2naLengthBlock(TSeqPos pos, TSeqPos len) const
{
    TSeqPos pos0 = pos;
    TSeqPos end = pos+len;
    while ( pos != end ) {
        TSeqPos block_index = pos/kAmbiguityBlockSize;
        if ( x_AmbiguousBlock(block_index) ) {
            // 4na
            break;
        }
        pos = min(end, (block_index+1)*kAmbiguityBlockSize);
    }
    return pos-pos0;
}


TSeqPos CWGSDb_Impl::SAmbiguityInfo::Get4naLengthBlock(TSeqPos pos, TSeqPos len) const
{
    TSeqPos pos0 = pos;
    TSeqPos end = pos+len;
    while ( pos != end ) {
        TSeqPos block_index = pos/kAmbiguityBlockSize;
        if ( !x_AmbiguousBlock(block_index) ) {
            // 2na
            break;
        }
        pos = min(end, (block_index+1)*kAmbiguityBlockSize);
    }
    return pos-pos0;
}


#ifdef USE_GLOBAL_AMBIGUITY_CACHE

DEFINE_STATIC_FAST_MUTEX(s_GlobalAmbiguityCacheMutex);
typedef limited_resource_map<pair<string, TVDBRowId>, CRef<CWGSDb_Impl::SAmbiguityInfo>, size_t> TGlobalAmbiguityCache;
class SStaticGlobalAmbiguityCacheCallbacks {
public:
    TGlobalAmbiguityCache* Create() { return new TGlobalAmbiguityCache(s_GetAmbiguityCacheSize()); }
    void Cleanup(TGlobalAmbiguityCache&) {}
};
static CSafeStatic<TGlobalAmbiguityCache, SStaticGlobalAmbiguityCacheCallbacks> s_GlobalAmbiguityCache;
#endif

CRef<CWGSDb_Impl::SAmbiguityInfo> CWGSDb_Impl::GetAmbiguityInfo(TVDBRowId row)
{
#ifdef USE_GLOBAL_AMBIGUITY_CACHE
    CFastMutexGuard guard(s_GlobalAmbiguityCacheMutex);
    return s_GlobalAmbiguityCache->get(make_pair(GetWGSPath(), row));
#else
    CFastMutexGuard guard(m_AmbiguityCacheMutex);
    return m_AmbiguityCache.get(row);
#endif
}


void CWGSDb_Impl::PutAmbiguityInfo(CRef<SAmbiguityInfo>& info)
{
    if ( !info ) {
        return;
    }
    size_t used_memory = info->GetUsedMemory();
#ifdef USE_GLOBAL_AMBIGUITY_CACHE
    CFastMutexGuard guard(s_GlobalAmbiguityCacheMutex);
    s_GlobalAmbiguityCache->put(make_pair(GetWGSPath(), info->m_RowId), info, used_memory);
#else
    CFastMutexGuard guard(m_AmbiguityCacheMutex);
    m_AmbiguityCache.put(info->m_RowId, info, used_memory);
#endif
}


void CWGSDb_Impl::x_InitIdParams(void)
{
    CRef<SSeq0TableCursor> seq = Seq0();
    if ( !seq->m_Cursor.TryOpenRow(1) ) {
        m_IdPrefixWithVersion.erase();
        m_IdPrefix.erase();
        m_IdVersion = 1;
        m_IdRowDigits = 0;
        return;
    }
    CTempString acc = *seq->ACC_PREFIX(1);
    const SIZE_TYPE prefix_len = acc.find_first_of("0123456789");
    m_IdRowDigits = *seq->ACC_CONTIG_LEN(1);
    if ( m_IdRowDigits < 6 || m_IdRowDigits > 8 ) {
        NCBI_THROW_FMT(CSraException, eInitFailed,
                       "CWGSDb: bad WGS accession format: "<<acc);
    }
    m_IdPrefixWithVersion = acc.substr(0, prefix_len+2);
    m_IdPrefix = acc.substr(0, prefix_len);
    m_IdVersion = NStr::StringToNumeric<int>(acc.substr(prefix_len, 2));
    if ( seq->m_MOL ) {
        // explicit contig type
        m_ContigMolType = CSeq_inst::TMol(*seq->MOL(1));
    }
    else {
        // deduce contig type from accession prefix
        switch ( acc[0] ) {
        case 'G':
        case 'H':
        case 'I':
            m_ContigMolType = CSeq_inst::eMol_rna;
            break;
        default:
            m_ContigMolType = CSeq_inst::eMol_dna;
            break;
        }
    }
    m_IdPrefixDbWithVersion = (IsTSA()? "TSA:": "WGS:")+m_IdPrefixWithVersion;
    m_IdPrefixDb = (IsTSA()? "TSA:": "WGS:")+m_IdPrefix;
    m_HasNoDefaultGnlId = seq->m_SEQID_GNL_PREFIX && seq->SEQID_GNL_PREFIX(1).empty();
    bool has_static_taxid = seq->m_TAXID && seq->m_TAXID.IsStatic(seq->m_Cursor);
    TTaxId static_taxid = ZERO_TAX_ID;
    if ( has_static_taxid ) {
        auto value = seq->TAXID(1);
        if ( value.size() != 1 ) {
            has_static_taxid = false;
        }
        else {
            static_taxid = value[0];
        }
    }
    Put(seq);

    if ( CKMetadata meta = CKMetadata(SeqTable()) ) {
        if ( CKMDataNode node = CKMDataNode(meta, "GB_STATE", CKMDataNode::eMissing_Allow) ) {
            m_ProjectGBState = NCBI_gb_state(node.GetUint8());
        }
        if ( CKMDataNode node = CKMDataNode(meta, "REPLACED_BY", CKMDataNode::eMissing_Allow) ) {
            size_t size = node.GetSize();
            m_ReplacedBy.resize(size);
            node.GetData(&m_ReplacedBy[0], size);
        }
        if ( CKMDataNode node = CKMDataNode(meta, "SEQ_ID_TYPE", CKMDataNode::eMissing_Allow) ) {
            m_SeqIdType = CSeq_id::E_Choice(node.GetUint8());
        }
        if ( CKMDataNode node = CKMDataNode(meta, "EXTRA_TAXIDS", CKMDataNode::eMissing_Allow) ) {
            // all tax ids are separate
        }
        else if ( CKMDataNode node = CKMDataNode(meta, "TAXID", CKMDataNode::eMissing_Allow) ) {
            // common taxid
            if ( node.GetSize() != 0 ) {
                m_CommonTaxId = node.GetUint4();
                m_HasCommonTaxId = true;
                if ( has_static_taxid && static_taxid != m_CommonTaxId ) {
                    m_CommonTaxId = ZERO_TAX_ID;
                    m_HasCommonTaxId = false;
                }
            }
        }
    }
}


string CWGSDb_Impl::NormalizePathOrAccession(CTempString path_or_acc,
                                             CTempString vol_path)
{
#ifdef USE_TEST_PATH
    {
        string test_path = NCBI_PARAM_TYPE(WGS, TEST_PATH)::GetDefault();
        if ( !test_path.empty() ) {
            string file_path = CDirEntry::MakePath(test_path, path_or_acc);
            if ( CDirEntry(file_path).Exists() ) {
                LOG_POST(Warning<<"Using local test file: "<<file_path);
                return file_path;
            }
        }
    }
#endif
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
    if ( CVPath::IsPlainAccession(path_or_acc) &&
         path_or_acc.find('.') == string::npos ) { // no WGS accession sub-version
        // parse WGS accession
        const SIZE_TYPE start = 0;
        // ID-5322 : WGS prefix can consist of 4 or 6 characters, with optional
        // 2-digit version.
        // If no version is specified, set it to a default value 00, which is
        // resolved to a real version via a symlink on the file system.
        string acc = path_or_acc.substr(start);
        size_t acclen = acc.size();
        size_t digit_pos = acc.find_first_of("0123456789");
        if (digit_pos == string::npos && (acclen == 4 || acclen == 6)) {
            return string(path_or_acc) + "00";
        } else if ((digit_pos == 4 || digit_pos == 6) &&
                   acclen > digit_pos + 2) {
            // remove contig/scaffold id
            return path_or_acc.substr(0, start+digit_pos+2);
        }
    }
    return path_or_acc;
}


inline
void CWGSDb_Impl::OpenTable(CVDBTable& table,
                            atomic<bool>& table_is_opened,
                            const char* table_name)
{
    CFastMutexGuard guard(m_TableMutex);
    if ( !table_is_opened.load(memory_order_acquire) ) {
        table = CVDBTable(m_Db, table_name, CVDBTable::eMissing_Allow);
        table_is_opened.store(true, memory_order_release);
    }
}


inline
void CWGSDb_Impl::OpenIndex(const CVDBTable& table,
                            CVDBTableIndex& index,
                            atomic<Int1>& index_is_opened,
                            const char* index_name,
                            const char* backup_index_name)
{
    if ( table ) {
        CFastMutexGuard guard(m_TableMutex);
        if ( !index_is_opened.load(memory_order_acquire) ) {
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
            index_is_opened.store(type, memory_order_release);
        }
    }
    else {
        index_is_opened.store(-1, memory_order_release);
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


void CWGSDb_Impl::OpenProtIdxTable(void)
{
    OpenTable(m_ProtIdxTable, m_ProtIdxTableIsOpened, "PROT_ACC_IDX");
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


void CWGSDb_Impl::OpenProductNameIndex(void)
{
    OpenIndex(ProtTable(), m_ProductNameIndex, m_ProductNameIndexIsOpened,
              "product_name_uc", "product_name");
}


void CWGSDb_Impl::OpenProtAccIndex(void)
{
    OpenIndex(ProtTable(), m_ProtAccIndex, m_ProtAccIndexIsOpened,
              "gb_accession");
}


pair<TVDBRowId, CWGSDb_Impl::ERowType>
CWGSDb_Impl::ParseRowType(CTempString acc,
                          TAllowRowType allow_type)
{
    pair<TVDBRowId, ERowType> ret(0, eRowType_contig);
    const SIZE_TYPE start = 0;
    SIZE_TYPE prefix_len = acc.find_first_of("0123456789");
    if (prefix_len == NPOS || prefix_len >= acc.size() - 2)
        return ret;
    else prefix_len += 2;

    CTempString row = acc.substr(start+prefix_len);
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


bool sx_SetAccession(CSeq_id& id, CTempString accession)
{
    if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        const_cast<CTextseq_id*>(text_id)->SetAccession(accession);
        return true;
    }
    return false;
}


int sx_NewStringToNonNegativeInt(CTempString str);


inline
int sx_StringToNonNegativeInt(const CTempString& str)
{
    return sx_NewStringToNonNegativeInt(str);
}

int sx_NewStringToNonNegativeInt(CTempString str)
{
    const bool kSetErrno = 0;
    const bool kSetNcbiError = 0;

    int error = 0, ret = -1;
    size_t len = str.size();
    if ( !len ) {
        error = EINVAL;
    }
    else {
        unsigned v = str.data()[0] - '0';
        if (v > 9) {
            error = EINVAL;
        }
        else {
            for (size_t i = 1; i < len; ++i) {
                unsigned d = str.data()[i] - '0';
                if (d > 9) {
                    error = EINVAL;
                    break;
                }
                unsigned nv = v * 10 + d;
                const unsigned kOverflowLimit = (INT_MAX - 9) / 10 + 1;
                if ( v >= kOverflowLimit ) {
                    // possible overflow
                    if ( v > kOverflowLimit || nv > INT_MAX) {
                        error = ERANGE;
                        break;
                    }
                }
                v = nv;
            }
            if (!error) {
                ret = static_cast<int>(v);
            }
        }
    }
    if (kSetErrno) {
        errno = error;
    }
    if (kSetNcbiError && error) {
        CNcbiError::SetErrno(error, str);
    }
    return ret;
}


// return non-negative integer if the string is its canonical representation -
// no leading zeros or spaces,
// otherwise return -1
int sx_GetStringId(CTempString str)
{
    int id = sx_StringToNonNegativeInt(str);
    if ( id >= 0 ) {
        if ( str.size() == 1 || str.data()[0] != '0' ) { // no leading zeroes
            return id;
        }
    }
    return -1;
}

/*
CRange<int> sx_GetPatentRange(const CUser_object& obj, CTempString prefix)
{
    int from = -1;
    int to = -1;
    if ( auto field = obj.GetFieldRef("Patent_accession_first") ) {
        if ( field->GetData().IsStr() ) {
            CTempString str = field->GetData().GetStr();
            if ( NStr::StartsWith(str, prefix) ) {
                from = sx_StringToNonNegativeInt(str.substr(prefix.size()));
            }
        }
    }
    if ( auto field = obj.GetFieldRef("Patent_accession_last") ) {
        if ( field->GetData().IsStr() ) {
            CTempString str = field->GetData().GetStr();
            if ( NStr::StartsWith(str, prefix) ) {
                to = sx_StringToNonNegativeInt(str.substr(prefix.size()));
            }
        }
    }
    if ( from >= 0 && to >= from ) {
        return CRange<int>(from, to);
    }
    else {
        return CRange<int>::GetEmpty();
    }
}
*/

void sx_SetTag(CDbtag& tag, CTempString str)
{
    CObject_id& oid = tag.SetTag();
    int id = sx_GetStringId(str);
    if ( id >= 0 ) {
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
            CSeq_descr tmp;
            in >> tmp;
            for ( auto& desc : tmp.Set() ) {
                descr.Set().push_back(desc);
            }
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


void sx_SetSplitId(CID2S_Bioseq_Ids::C_E& split_id, CSeq_id& id)
{
    if ( id.IsGi() ) {
        split_id.SetGi(id.GetGi());
    }
    else {
        split_id.SetSeq_id(id);
    }
}


void sx_SetSplitId(CID2S_Chunk_Data::TId& split_id, CSeq_id& id)
{
    if ( id.IsGi() ) {
        split_id.SetGi(id.GetGi());
    }
    else {
        split_id.SetSeq_id(id);
    }
}


void sx_AddSplitId(CID2S_Bioseq_Ids::Tdata& split_ids, CSeq_id& id)
{
    CRef<CID2S_Bioseq_Ids::C_E> split_id(new CID2S_Bioseq_Ids::C_E);
    sx_SetSplitId(*split_id, id);
    split_ids.push_back(split_id);
}


void sx_AddSplitIds(CID2S_Bioseq_Ids::Tdata& split_ids,
                    const CBioseq::TId& ids)
{
    ITERATE ( CBioseq::TId, it, ids ) {
        sx_AddSplitId(split_ids, it->GetNCObject());
    }
}


void sx_SetSplitInterval(CID2S_Seq_loc& split_loc, CSeq_id& id,
                        TSeqPos pos, TSeqPos end)
{
    if ( id.IsGi() ) {
        CID2S_Gi_Interval& loc_gi = split_loc.SetGi_interval();
        loc_gi.SetGi(id.GetGi());
        loc_gi.SetStart(pos);
        loc_gi.SetLength(end-pos);
    }
    else {
        CID2S_Seq_id_Interval& loc_id = split_loc.SetSeq_id_interval();
        loc_id.SetSeq_id(id);
        loc_id.SetStart(pos);
        loc_id.SetLength(end-pos);
    }
}


END_LOCAL_NAMESPACE;


bool CWGSDb_Impl::IsTSA(void) const
{
    return GetContigMolType() == CSeq_inst::eMol_rna;
}


CRef<CSeq_id> CWGSDb_Impl::GetGeneralSeq_id(CTempString prefix,
                                            CTempString tag) const
{
    if ( prefix.empty() ) {
        return null;
    }
    else {
        CRef<CSeq_id> id(new CSeq_id);
        CDbtag& dbtag = id->SetGeneral();
        dbtag.SetDb(prefix);
        sx_SetTag(dbtag, tag);
        return id;
    }
}


CRef<CSeq_id> CWGSDb_Impl::GetGeneralSeq_id(CTempString tag,
                                            TGnlIdFlags gnl_id_flags) const
{
    CRef<CSeq_id> id;
    if ( m_HasNoDefaultGnlId ) {
        return id;
    }
    id = new CSeq_id;
    CDbtag& dbtag = id->SetGeneral();
    SIZE_TYPE colon = tag.rfind(':');
    if ( colon != NPOS ) {
        dbtag.SetDb(tag.substr(0, colon));
        tag = tag.substr(colon+1);
    }
    else {
        const string& db =
            gnl_id_flags & fGnlId_NoWGSVersion? m_IdPrefixDb: m_IdPrefixDbWithVersion;
        dbtag.SetDb(db);
        if ( NStr::StartsWith(tag, db) &&
             tag[db.size()] == ':' ) {
            tag = tag.substr(db.size()+1);
        }
    }
    sx_SetTag(dbtag, tag);
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetPatentSeq_id(int id) const
{
    CRef<CSeq_id> seq_id(new CSeq_id);
    CPatent_seq_id& pat_id = seq_id->SetPatent();
    pat_id.SetCit(m_PatentId.GetNCObject().SetPatent().SetCit());
    pat_id.SetSeqid(id);
    return seq_id;
}


CRef<CSeq_id>
CWGSDb_Impl::GetGeneralOrPatentSeq_id(CTempString str,
                                      TVDBRowId row,
                                      TGnlIdFlags gnl_id_flags) const
{
    if ( str.empty() ) {
        return null;
    }
    int id = sx_GetStringId(str);
    if ( id >= 0 && HasPatentId() ) {
        return GetPatentSeq_id(id);
    }
    if ( gnl_id_flags & fGnlId_NoWGSId ) {
        return null;
    }
    return GetGeneralSeq_id(str, gnl_id_flags);
}


CRef<CSeq_id>
CWGSDb_Impl::GetGeneralOrPatentSeq_id(CTempString str,
                                      const SSeq0TableCursor& cur,
                                      TVDBRowId row) const
{
    if ( str.empty() ) {
        return null;
    }
    int id = sx_GetStringId(str);
    if ( id >= 0 && HasPatentId() ) {
        return GetPatentSeq_id(id);
    }
    if ( cur.m_SEQID_GNL_PREFIX ) {
        return GetGeneralSeq_id(cur.SEQID_GNL_PREFIX(1), str);
    }
    else {
        return GetGeneralSeq_id(str);
    }
}


CRef<CSeq_id>
CWGSDb_Impl::GetGeneralOrPatentSeq_id(CTempString str,
                                      const SScfTableCursor& cur,
                                      TVDBRowId row) const
{
    if ( str.empty() ) {
        return null;
    }
    int id = sx_GetStringId(str);
    if ( id >= 0 && HasPatentId() ) {
        return GetPatentSeq_id(id);
    }
    if ( cur.m_SEQID_GNL_PREFIX ) {
        return GetGeneralSeq_id(cur.SEQID_GNL_PREFIX(1), str);
    }
    else {
        return GetGeneralSeq_id(str);
    }
}


CRef<CSeq_id>
CWGSDb_Impl::GetGeneralOrPatentSeq_id(CTempString str,
                                      const SProt0TableCursor& cur,
                                      TVDBRowId row) const
{
    if ( str.empty() ) {
        return null;
    }
    int id = sx_GetStringId(str);
    if ( id >= 0 && HasPatentId() ) {
        return GetPatentSeq_id(id);
    }
    if ( cur.m_SEQID_GNL_PREFIX ) {
        return GetGeneralSeq_id(cur.SEQID_GNL_PREFIX(1), str);
    }
    else {
        return GetGeneralSeq_id(str, fGnlId_NoWGSVersion);
    }
}


CRef<CSeq_id> CWGSDb_Impl::GetAccSeq_id(CTempString acc, int version) const
{
    PROFILE(sw_GetAccSeq_id);
    CRef<CSeq_id> id;
    if ( !acc.empty() ) {
        if ( m_SeqIdType != CSeq_id::e_not_set ) {
            id = new CSeq_id();
            id->Select(m_SeqIdType);
            sx_SetAccession(*id, acc);
        }
        else {
            id = new CSeq_id(acc);
        }
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


CSeq_inst::TMol CWGSDb_Impl::GetContigMolType(void) const
{
    return m_ContigMolType;
}


CSeq_inst::TMol CWGSDb_Impl::GetScaffoldMolType(void) const
{
    return CSeq_inst::eMol_dna;
}


CSeq_inst::TMol CWGSDb_Impl::GetProteinMolType(void) const
{
    return CSeq_inst::eMol_aa;
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
    buffer.resize_mem(size);
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
        if ( m_MasterEntry->IsSeq() ) {
            for ( auto& id : m_MasterEntry->GetSeq().GetId() ) {
                if ( id->IsPatent() ) {
                    SetPatentId(id);
                    break;
                }
            }
            /*
            m_PatentSeqIdRangeNuc = CRange<int>::GetEmpty();
            m_PatentSeqIdRangeProt = CRange<int>::GetEmpty();
            if ( HasPatentId() && m_MasterEntry->GetSeq().IsSetDescr() ) {
                for ( auto& d : m_MasterEntry->GetSeq().GetDescr().Get() ) {
                    const CSeqdesc& desc = *d;
                    if ( desc.IsUser() ) {
                        const CUser_object& obj = desc.GetUser();
                        const CObject_id& type = obj.GetType();
                        if ( type.IsStr() &&
                             type.GetStr() == "PatentProjects" ) {
                            m_PatentSeqIdRangeNuc = sx_GetPatentRange(obj, GetIdPrefixWithVersion());
                            m_PatentSeqIdRangeProt = m_PatentSeqIdRangeNuc;
                        }
                        if ( type.IsStr() &&
                             type.GetStr() == "PatentProjectsNucleotide" ) {
                            m_PatentSeqIdRangeNuc = sx_GetPatentRange(obj, GetIdPrefixWithVersion());
                        }
                        if ( type.IsStr() &&
                             type.GetStr() == "PatentProjectsProtein" ) {
                            m_PatentSeqIdRangeProt = sx_GetPatentRange(obj, GetIdPrefixWithVersion());
                        }
                    }
                }
            }
            */
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
    case CSeqdesc::e_Genbank:
    case CSeqdesc::e_Embl:
        return eDescr_default;
    case CSeqdesc::e_User:
        if ( desc.GetUser().GetType().IsStr() ) {
            // only specific user objects are passed from WGS master
            const string& name = desc.GetUser().GetType().GetStr();
            if ( name == "DBLink" ||
                 name == "GenomeProjectsDB" ||
                 name == "StructuredComment" ||
                 name == "FeatureFetchPolicy" ||
                 name == "Unverified") {
                return eDescr_default;
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

static string
s_GetUserObjectType(const CSeqdesc& desc)
{
    string uo_type;

    if (desc.IsUser() && desc.GetUser().GetType().IsStr()) {
        uo_type = desc.GetUser().GetType().GetStr();
        if (uo_type == "StructuredComment") {
            ITERATE (CUser_object::TData, it, desc.GetUser().GetData()) {
                if ((*it)->GetLabel().IsStr() &&
                    (*it)->GetLabel().GetStr() == "StructuredCommentPrefix") {
                    string data = ((*it)->GetData().IsStr() ?
                                   (string) (*it)->GetData().GetStr() :
                                   NStr::IntToString((*it)->GetData().GetInt()));
                    uo_type += "|" + data;
                    break;
                }
            }
        }
    }

    return uo_type;
}

static void 
s_AddUserObjectType(const CSeqdesc& desc, set<string>& existing_uo_types)
{
    string uo_type = s_GetUserObjectType(desc);

    if (!uo_type.empty() && existing_uo_types.count(uo_type) == 0) {
        existing_uo_types.insert(uo_type);
    }
}

void CWGSDb_Impl::AddMasterDescr(CSeq_descr& descr, const CBioseq* main_seq, TFlags flags) const
{
    if ( !GetMasterDescr().empty() ) {
        unsigned type_mask = 0;
        set<string> existing_uo_types;
        
        ITERATE ( CSeq_descr::Tdata, it, descr.Get() ) {
            const CSeqdesc& desc = **it;
            type_mask |= 1 << desc.Which();
            s_AddUserObjectType(desc, existing_uo_types);
        }

        if (main_seq && main_seq->IsSetDescr()) {
            for (auto& desc : main_seq->GetDescr().Get()) {
                type_mask |= 1 << desc->Which();
                s_AddUserObjectType(*desc, existing_uo_types);
            }
        }

        string kMasterDescrMark = "WithMasterDescr";
        if ( existing_uo_types.find(kMasterDescrMark) == existing_uo_types.end() ) {
            ITERATE ( TMasterDescr, it, GetMasterDescr() ) {
                const CSeqdesc& desc = **it;
                if ( CWGSDb::GetMasterDescrType(desc) == CWGSDb::eDescr_default &&
                     (type_mask & (1 << desc.Which())) ) {
                    bool skip = true;
                    string uo_type = s_GetUserObjectType(desc);
                    if (!uo_type.empty() && existing_uo_types.count(uo_type) == 0)
                        skip = false;
                    // omit master descr if contig already has one of that type
                    if (skip)
                        continue;
                }
                descr.Set().push_back(*it);
            }
            if ( flags & fMasterDescrMark ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                auto& user_object = desc->SetUser();
                user_object.SetType().SetStr(kMasterDescrMark);
                user_object.SetData();
                descr.Set().push_back(desc);
            }
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


void CWGSDb_Impl::SetPatentId(CRef<CSeq_id> id)
{
    m_PatentId = id;
}

/*
int CWGSDb_Impl::GetPatentSeqIdNuc(CTempString str_id) const
{
    if ( !HasPatentId() ) {
        return 0;
    }
    int id = sx_GetStringId(str_id);
    return id >= 0 && IsValidPatentSeqIdNuc(id)? id: 0;
}


int CWGSDb_Impl::GetPatentSeqIdProt(CTempString str_id) const
{
    if ( !HasPatentId() ) {
        return 0;
    }
    int id = sx_GetStringId(str_id);
    return id >= 0 && IsValidPatentSeqIdProt(id)? id: 0;
}
*/

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
    return GI_FROM(TVDBRowId, gi);
}


pair<TGi, TGi> CWGSDb_Impl::GetNucGiRange(void)
{
    pair<TGi, TGi> ret;
    if ( CRef<SGiIdxTableCursor> idx = GiIdx() ) {
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
    if ( CRef<SGiIdxTableCursor> idx = GiIdx() ) {
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
        TVDBRowIdRange row_range = seq->m_GI.GetRowIdRange(seq->m_Cursor);
        for ( TVDBRowCount i = 0; i < row_range.second; ++i ) {
            row_id = row_range.first+i;
            TIntId gi = GI_TO(TIntId, s_ToGi(*seq->GI(row_id), "CWGSDb::GetNucGiRanges()"));
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


CWGSDb_Impl::SProtAccInfo::SProtAccInfo(CTempString acc, Uint4& id)
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


string CWGSDb_Impl::SProtAccInfo::GetAcc(Uint4 id) const
{
    string acc = m_AccPrefix;
    acc.resize(m_IdLength, '0');
    for ( SIZE_TYPE i = m_IdLength; id; id /= 10 ) {
        acc[--i] += id % 10;
    }
    return acc;
}


CWGSDb_Impl::TProtAccRanges CWGSDb_Impl::GetProtAccRanges(void)
{
    TProtAccRanges ranges;
    if ( CRef<SProt0TableCursor> seq = Prot0() ) {
        TVDBRowId row_id = 0;
        TVDBRowIdRange row_range = seq->m_GB_ACCESSION.GetRowIdRange(seq->m_Cursor);
        for ( TVDBRowCount i = 0; i < row_range.second; ++i ) {
            row_id = row_range.first+i;
            CTempString acc = *seq->GB_ACCESSION(row_id);
            if ( acc.empty() ) {
                continue;
            }
            Uint4 id;
            SProtAccInfo info(acc, id);
            if ( !info ) {
                continue;
            }
            TProtAccRanges::iterator it = ranges.lower_bound(info);
            if ( it == ranges.end() || it->first != info ) {
                TIdRange range(id, id+1);
                ranges.insert(it, TProtAccRanges::value_type(info, range));
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
    TIntId row_id = GI_TO(TIntId, gi);
    if ( CRef<SGiIdxTableCursor> idx = GiIdx(row_id) ) {
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
    TIntId row_id = GI_TO(TIntId, gi);
    if ( CRef<SGiIdxTableCursor> idx = GiIdx(row_id) ) {
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
    TIntId row_id = GI_TO(TIntId, gi);
    if ( CRef<SGiIdxTableCursor> idx = GiIdx(row_id) ) {
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
    if ( 1 ) {
        CRef<SSeqTableCursor> seq = Seq();
        if ( seq->m_CONTIG_NAME_ROW_RANGE ) {
            seq->m_Cursor.SetParam("CONTIG_NAME_QUERY", name);
            SSeqTableCursor::row_range_t range;
            CVDBValueFor<SSeqTableCursor::row_range_t> value =
                seq->CONTIG_NAME_ROW_RANGE(0, CVDBValue::eMissing_Allow);
            if ( !value.empty() ) {
                range = *value;
            }
            Put(seq);
            return range.first;
        }
        Put(seq);
    }
    const CVDBTableIndex& index = ContigNameIndex();
    return Lookup(name, index, m_ContigNameIndexIsOpened.load(memory_order_relaxed) == 2);
}


TVDBRowId CWGSDb_Impl::GetScaffoldNameRowId(const string& name)
{
    const CVDBTableIndex& index = ScaffoldNameIndex();
    return Lookup(name, index, m_ScaffoldNameIndexIsOpened.load(memory_order_relaxed) == 2);
}


TVDBRowId CWGSDb_Impl::GetProteinNameRowId(const string& name)
{
    const CVDBTableIndex& index = ProteinNameIndex();
    return Lookup(name, index, m_ProteinNameIndexIsOpened.load(memory_order_relaxed) == 2);
}


TVDBRowId CWGSDb_Impl::GetProductNameRowId(const string& name)
{
    const CVDBTableIndex& index = ProductNameIndex();
    return Lookup(name, index, m_ProductNameIndexIsOpened.load(memory_order_relaxed) == 2);
}


TVDBRowId CWGSDb_Impl::GetProtAccRowId(const string& acc, int ask_version)
{
    TVDBRowId prot_row_id = 0;
    if ( CRef<SProtIdxTableCursor> idx = ProtIdx() ) {
        CVDBMgr::CRequestContextUpdater ctx_updater;
        SProtIdxTableCursor::row_range_t range;
        string tmp = acc;
        const char* query_param_name;
        if ( NStr::Equal(idx->m_ROW_ID.GetName(), "ROW_ID") ) {
            query_param_name = "NAME_QUERY";
        }
        else {
            query_param_name = "ACCESSION_QUERY";
        }
        idx->m_Cursor.SetParam(query_param_name, NStr::ToUpper(tmp));
        CVDBValueFor<SProtIdxTableCursor::row_range_t> value =
            idx->NAME_ROW_RANGE(0, CVDBValue::eMissing_Allow);
        if ( !value.empty() ) {
            range = *value;
        }
        if ( range.first && range.first <= range.second ) {
            CVDBValueFor<TVDBRowId> prot_rows = idx->ROW_ID(range.first);
            if ( !prot_rows.empty() ) {
                if ( ask_version > 0 ) {
                    // check if version exists
                    size_t version_index = size_t(prot_rows.size() == 1? 0: ask_version-1);
                    if ( version_index < prot_rows.size() ) {
                        // check if version mathces
                        prot_row_id = prot_rows[version_index];
                        if ( prot_row_id ) {
                            CRef<SProt0TableCursor> prot = Prot0(prot_row_id);
                            int actual_version = *prot->ACC_VERSION(prot_row_id);
                            Put(prot, prot_row_id);
                            if ( actual_version != ask_version ) {
                                // version mismatch
                                prot_row_id = 0;
                            }
                        }
                    }
                }
                else if ( ask_version == -1 ) {
                    // last version
                    prot_row_id = prot_rows[prot_rows.size()-1];
                }
            }
        }
        Put(idx);
    }
    return prot_row_id;
}


bool CWGSDb_Impl::CanHaveGis()
{
    bool can_have_gis = false;
    auto cur = Seq();
    if (cur->m_GI) {
        auto gi_range = cur->m_Cursor.GetRowIdRange(cur->m_GI.GetIndex());
        if ( gi_range.second ) {
            auto value = cur->GI(gi_range.first, CVDBValue::eMissing_Allow);
            if (value.size() == 1 && *value != 0) {
                can_have_gis = true;
            }
        }
    }
    Put(cur);
    return can_have_gis;
}


TVDBRowCount CWGSDb_Impl::GetTotalFeatureCount()
{
    TVDBRowCount feature_count = 0;
    if ( auto cur = Feat() ) {
        feature_count = cur->m_Cursor.GetRowIdRange().second;
        Put(cur);
    }
    return feature_count;
}


bool CWGSDb_Impl::HasFeatures()
{
    return GetTotalFeatureCount() > 0;
}


bool CWGSDb_Impl::HasStandardFeatLocIdType()
{
    if (GetWGSPath().find_first_of("\\/.:") != NPOS) {
        // non-standard path
        return false;
    }
    if (IsReplaced()) {
        // old or replaced WGS project
        return false;
    }
    if (GetProjectGBState() == NCBI_gb_state_eWGSGenBankMissing) {
        // disabled WGS project
        return false;
    }
    return true;
}


CWGSDb_Impl::EFeatLocIdType CWGSDb_Impl::DetermineFeatLocIdType()
{
    // assume no feature id correction
    EFeatLocIdType loc_id_type = eFeatLocIdGi;
    if (HasStandardFeatLocIdType()) {
        // shortcut for regular VDB files
        if (!CanHaveGis()) {
            loc_id_type = eFeatLocIdAccVer;
        }
        return loc_id_type;
    }
    try {
        if ( CRef<SFeatTableCursor> cur = Feat() ) {
            TVDBRowId feat_row_id = 1;
            try {
                PROFILE(sw_GetFeatLocIdTypeRange);
                CRef<SSeqTableCursor> seq = Seq();
                auto row_range = seq->m_Cursor.GetRowIdRange(seq->m_FEAT_ROW_START.GetIndex());
                for ( TVDBRowCount i = 0; i < row_range.second; ++i ) {
                    auto seq_row_id = row_range.first+i;
                    auto row_start = seq->FEAT_ROW_START(seq_row_id);
                    if ( !row_start.empty() ) {
                        feat_row_id = *row_start;
                        break;
                    }
                }
                Put(seq);
            }
            catch ( exception& /*ignored*/ ) {
                // use first feature in the file
            }
            PROFILE(sw_GetFeatLocIdTypeFeat);
            CRef<CSeq_feat> feat(new CSeq_feat);
            CTempString bytes;
            {{
                PROFILE(sw_GetFeatLocIdTypeFeatBytes);
                bytes = *cur->SEQ_FEAT(feat_row_id);
            }}
            cur.GetNCObject().m_ObjStr.OpenFromBuffer(bytes.data(), bytes.size());
            cur.GetNCObject().m_ObjStr >> *feat;
            Put(cur);
            CTypeConstIterator<CSeq_id> seq_id(Begin(feat->GetLocation()));
            if ( const CTextseq_id* id = !seq_id? 0: seq_id->GetTextseq_Id() ) {
                if ( id->IsSetVersion() ) {
                    loc_id_type = eFeatLocIdAccVer;
                }
                else {
                    loc_id_type = eFeatLocIdAccNoVer;
                }
            }
        }
    }
    catch ( exception& /*ignored*/ ) {
        // assume no feature id correction
    }
    return loc_id_type;
}


CWGSDb_Impl::EFeatLocIdType CWGSDb_Impl::GetFeatLocIdType()
{
    auto loc_id_type = m_FeatLocIdType.load(memory_order_relaxed);
    if ( loc_id_type == eFeatLocIdUninitialized ) {
        // determine and cache for the future
        loc_id_type = DetermineFeatLocIdType();
        m_FeatLocIdType.store(loc_id_type, memory_order_relaxed);
    }
    return loc_id_type;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSSeqIterator
/////////////////////////////////////////////////////////////////////////////


void CWGSSeqIterator::TWGSContigGapInfo::SetPos(TSeqPos pos)
{
    // skip gaps starting before the requested position
    while ( *this && GetToOpen() <= pos ) {
        ++*this;
    }
}


enum EFromFlags {
    eFromFlags
};
enum EFromSplitVersion {
    eFromSplitVersion
};


struct SWGSCreateInfo : SWGSDb_Defs
{
    explicit
    SWGSCreateInfo(const CWGSDb& db)
        : db(db),
          flags(fDefaultFlags),
          split_prod(false),
          split_data(false),
          split_feat(false),
          split_qual(false),
          split_version(kAssignedDefaultSplitVersion)
        {
        }
        
    SWGSCreateInfo(const CWGSDb& db, EFromFlags, TFlags flags)
        : SWGSCreateInfo(db)
        {
            if ( flags != fDefaultFlags ) {
                x_SetFlags(flags);
            }
        }

    SWGSCreateInfo(const CWGSDb& db, EFromSplitVersion, TSplitVersion split_version)
        : SWGSCreateInfo(db)
        {
            if ( split_version != kDefaultSplitVersion ) {
                x_SetSplitVersion(split_version);
            }
        }

    // set flags and corresponding split_version
    void x_SetFlags(TFlags flags);
    
    // set split_version and corresponding flags
    void x_SetSplitVersion(TSplitVersion split_version);

    CWGSDb db;
    TFlags flags;
    bool split_prod, split_data, split_feat, split_qual;
    CRef<CSeq_id> main_id;
    CRef<CSeq_id> feat_id;
    CRef<CBioseq> main_seq;
    CRef<CSeq_entry> entry;
    CRef<CID2S_Split_Info> split;
    TSplitVersion split_version;
    CRef<CID2S_Chunk> chunk;
    CRef<CWGSAsnBinData> data;

    template<class Iter>
    void x_SetId(Iter& it)
        {
            main_id = it.GetId(flags);
            feat_id = main_id;
            // fix feature ids
            // it can be accession.version and accession
            if ( feat_id->IsGi() ) {
                EFeatLocIdType feat_loc_id_type = db->GetFeatLocIdType();
                if ( feat_loc_id_type != eFeatLocIdGi ) {
                    feat_id = it.GetId(flags & ~fIds_gi);
                }
            }
        }
    void x_ResetId()
        {
            main_id = null;
            feat_id = null;
        }
    template<class Iter>
    void x_SetSeq(Iter& it)
        {
            main_seq = new CBioseq();
            x_SetId(it);
        }
    void x_SetSeq(CWGSProteinIterator& it)
        {
            main_seq = new CBioseq();
            x_SetId(it);
        }
    void x_ResetSeq()
        {
            main_seq = null;
            x_ResetId();
        }

    void x_AddDescr(CTempString bytes);
    void x_AddFeature(const CWGSFeatureIterator& it,
                      CSeq_annot::TData::TFtable& dst);
    void x_AddFeaturesDirect(TVDBRowIdRange range,
                             vector<TVDBRowId>& product_row_ids);
    void x_AddFeaturesSplit(TVDBRowIdRange range,
                            vector<TVDBRowId>& product_row_ids);
    void x_AddFeatures(TVDBRowIdRange range,
                       vector<TVDBRowId>& product_row_ids);
    void x_AddFeatures(TVDBRowIdRange range);
    CBioseq_set& x_GetProtSet(void);
    void x_CreateProtSet(TVDBRowIdRange range);
    void x_AddProducts(const vector<TVDBRowId>& product_row_ids);
};


bool CWGSSeqIterator::x_Excluded(void) const
{
    if ( *this ) {
        auto state = GetGBState();
        // skip artificial entries with 'missing' state
        if ( state ==  NCBI_gb_state_eWGSGenBankMissing ) {
            return true;
        }
        // skip not included entries
        if ( !(m_IncludeFlags & TIncludeFlags(1 << state)) ) {
            return true;
        }
    }
    return false;
}


void CWGSSeqIterator::Reset(void)
{
    if ( m_Cur0 ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur0, m_CurrId);
            if ( m_Cur ) {
                GetDb().Put(m_Cur, m_CurrId);
            }
        }
        else {
            m_Cur.Reset();
            m_Cur0.Reset();
        }
    }
    m_Db.Reset();
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    m_AccVersion = eLatest;
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSSeqIterator& iter)
    : m_AccVersion(eLatest)
{
    *this = iter;
}


CWGSSeqIterator& CWGSSeqIterator::operator=(const CWGSSeqIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur0 = iter.m_Cur0;
        m_Cur = iter.m_Cur;
        m_CurrId = iter.m_CurrId;
        m_AccVersion = iter.m_AccVersion;
        m_FirstGoodId = iter.m_FirstGoodId;
        m_FirstBadId = iter.m_FirstBadId;
        m_IncludeFlags = iter.m_IncludeFlags;
        m_ClipByQuality = iter.m_ClipByQuality;
    }
    return *this;
}


CWGSSeqIterator::CWGSSeqIterator(void)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0),
      m_AccVersion(eLatest),
      m_IncludeFlags(fIncludeDefault),
      m_ClipByQuality(true)
{
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 EIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId row,
                                 EIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId first_row,
                                 TVDBRowId last_row,
                                 EIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, first_row, last_row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 CTempString acc,
                                 EIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, acc);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId row,
                                 TIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId first_row,
                                 TVDBRowId last_row,
                                 TIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, first_row, last_row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 CTempString acc,
                                 TIncludeFlags include_flags,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, include_flags, clip_type, acc);
}


static inline
CWGSSeqIterator::TIncludeFlags s_ToFlags(CWGSSeqIterator::EWithdrawn withdrawn)
{
    if ( withdrawn == CWGSSeqIterator::eIncludeWithdrawn ) {
        return CWGSSeqIterator::fIncludeDefault | CWGSSeqIterator::fIncludeWithdrawn;
    }
    else {
        return CWGSSeqIterator::fIncludeDefault;
    }
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, s_ToFlags(withdrawn), clip_type);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId row,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, s_ToFlags(withdrawn), clip_type, row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 TVDBRowId first_row,
                                 TVDBRowId last_row,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, s_ToFlags(withdrawn), clip_type, first_row, last_row);
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 CTempString acc,
                                 EWithdrawn withdrawn,
                                 EClipType clip_type)
    : m_AccVersion(eLatest)
{
    x_Select(wgs_db, s_ToFlags(withdrawn), clip_type, acc);
}


CWGSSeqIterator::~CWGSSeqIterator(void)
{
    Reset();
}



void CWGSSeqIterator::x_Select(const CWGSDb& wgs_db,
                               TIncludeFlags include_flags,
                               EClipType clip_type)
{
    x_Init(wgs_db, include_flags, clip_type, 0);
    x_Settle();
}


void CWGSSeqIterator::x_Select(const CWGSDb& wgs_db,
                               TIncludeFlags include_flags,
                               EClipType clip_type,
                               TVDBRowId row)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    x_Init(wgs_db, include_flags, clip_type, row);
    SelectRow(row);
}


void CWGSSeqIterator::x_Select(const CWGSDb& wgs_db,
                               TIncludeFlags include_flags,
                               EClipType clip_type,
                               TVDBRowId first_row,
                               TVDBRowId last_row)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    x_Init(wgs_db, include_flags, clip_type, first_row);
    if ( m_FirstBadId == 0 ) {
        return;
    }
    if ( first_row > m_FirstGoodId ) {
        m_CurrId = m_FirstGoodId = first_row;
        m_AccVersion = eLatest;
    }
    if ( last_row < m_FirstBadId-1 ) {
        m_FirstBadId = last_row+1;
    }
    x_Settle();
}


void CWGSSeqIterator::x_Select(const CWGSDb& wgs_db,
                               TIncludeFlags include_flags,
                               EClipType clip_type,
                               CTempString acc)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    if ( TVDBRowId row = wgs_db.ParseContigRow(acc) ) {
        x_Init(wgs_db, include_flags, clip_type, row);
        SelectRow(row);
    }
    else {
        // bad format
        m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
        m_AccVersion = eLatest;
    }
}


void CWGSSeqIterator::x_Init(const CWGSDb& wgs_db,
                             TIncludeFlags include_flags,
                             EClipType clip_type,
                             TVDBRowId get_row)
{
    PROFILE(sw_SeqIterator);
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    m_AccVersion = eLatest;
    m_ClipByQuality = false;
    if ( !wgs_db ) {
        return;
    }
    m_Cur0 = wgs_db.GetNCObject().Seq0(get_row);
    m_Cur = wgs_db.GetNCObject().Seq(get_row);
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs_db;
    m_IncludeFlags = include_flags;
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
    TVDBRowIdRange range = m_Cur->m_CONTIG_NAME.GetRowIdRange(m_Cur->m_Cursor);
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
    m_AccVersion = eLatest;
    return *this;
}


CWGSSeqIterator& CWGSSeqIterator::operator++(void)
{
    x_CheckValid("CWGSSeqIterator::operator++");
    m_AmbiguityInfo = null;
    ++m_CurrId;
    x_Settle();
    return *this;
}


void CWGSSeqIterator::x_Settle(void)
{
    while ( *this && x_Excluded() ) {
        ++m_CurrId;
        m_AccVersion = eLatest;
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
    if ( !m_Cur->m_GI || m_AccVersion.m_Offset != 0 ) {
        return ZERO_GI;
    }
    CVDBValueFor<NCBI_gi> gi = m_Cur->GI(m_CurrId);
    return gi.empty()? ZERO_GI: s_ToGi(*gi, "CWGSSeqIterator::GetGi()");
}


CTempString CWGSSeqIterator::GetAccession(void) const
{
    x_CheckValid("CWGSSeqIterator::GetAccession");
    return *CVDBStringValue(m_Cur->ACCESSION(m_CurrId));
}


int CWGSSeqIterator::GetLatestAccVersion(void) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    x_CheckValid("CWGSSeqIterator::GetLatestAccVersion");
    return *m_Cur->ACC_VERSION(m_CurrId);
}


unsigned CWGSSeqIterator::GetAccVersionCount(void) const
{
    x_CheckValid("CWGSSeqIterator::GetAccVersionCount");
#ifdef TEST_ACC_VERSION
    if ( GetLatestAccVersion() > 1 &&
         m_Cur->TRIM_START(m_CurrId).size() == 1 ) {
        return 2;
    }
#endif
    return unsigned(m_Cur->TRIM_START(m_CurrId).size());
}


bool CWGSSeqIterator::HasAccVersion(int version) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    if ( version == -1 ) {
        // latest version
        return true;
    }
    int latest_version = GetLatestAccVersion();
    return version <= latest_version &&
        version > int(latest_version - GetAccVersionCount());
}


CWGSSeqIterator::SVersionSelector
CWGSSeqIterator::x_GetAccVersionSelector(int version) const
{
    SVersionSelector ret(eLatest);
    if ( version != -1 ) {
        int latest_version = GetLatestAccVersion();
        int oldest_version = latest_version - GetAccVersionCount() + 1;
        if ( version > latest_version || version < oldest_version ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "CWGSSeqIterator: "<<
                           GetDb().m_IdPrefixWithVersion<<"/"<<m_CurrId<<
                           " version "<<version<<
                           " is out of VDB version range: "<<
                           oldest_version<<"-"<<latest_version);
        }
        ret.m_Offset = version - latest_version;
    }
    return ret;
}


void CWGSSeqIterator::SelectAccVersion(int version)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    m_AccVersion = x_GetAccVersionSelector(version);
}


CRef<CSeq_id> CWGSSeqIterator::GetAccSeq_id(void) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
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


CRef<CSeq_id> CWGSSeqIterator::GetGeneralOrPatentSeq_id(void) const
{
    return GetDb().GetGeneralOrPatentSeq_id(GetContigName(), *m_Cur0, m_CurrId);
}


CRef<CSeq_id> CWGSSeqIterator::GetGeneralSeq_id(void) const
{
    return GetGeneralOrPatentSeq_id();
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

static TTaxId s_GetTaxId(const CVDBValueFor<NCBI_taxid>& value)
{
    return value.empty()? ZERO_TAX_ID: TAX_ID_FROM(int, value[0]);
}


bool CWGSSeqIterator::HasTaxId(void) const
{
    return GetDb().HasCommonTaxId() || m_Cur0->m_TAXID;
}


TTaxId CWGSSeqIterator::GetTaxId(void) const
{
    x_CheckValid("CWGSSeqIterator::GetTaxId");
    if ( GetDb().HasCommonTaxId() ) {
        return GetDb().GetCommonTaxId();
    }
    return s_GetTaxId(m_Cur0->TAXID(m_CurrId));
}


bool CWGSSeqIterator::HasSeqHash(void) const
{
    x_CheckValid("CWGSSeqIterator::GetSeqHash");
    return m_Cur->m_HASH;
}


CWGSSeqIterator::THash CWGSSeqIterator::GetSeqHash(void) const
{
    return HasSeqHash()? *m_Cur->HASH(m_CurrId): 0;
}


TSeqPos CWGSSeqIterator::GetRawSeqLength(void) const
{
    return *m_Cur->READ_LEN(m_CurrId);
}


TSeqPos CWGSSeqIterator::GetClipQualityLeft(void) const
{
    CVDBValueFor<INSDC_coord_zero> arr = m_Cur->TRIM_START(m_CurrId);
#ifdef TEST_ACC_VERSION
    if ( GetLatestAccVersion() > 1 && m_AccVersion.m_Offset != 0 && arr.size() == 1 ) {
        return *arr - 5*m_AccVersion.m_Offset;
    }
#endif
    return arr[arr.size()-1+m_AccVersion.m_Offset];
}


TSeqPos CWGSSeqIterator::GetClipQualityLength(void) const
{
    CVDBValueFor<INSDC_coord_len> arr = m_Cur->TRIM_LEN(m_CurrId);
#ifdef TEST_ACC_VERSION
    if ( GetLatestAccVersion() > 1 && m_AccVersion.m_Offset != 0 && arr.size() == 1 ) {
        TSeqPos len = *arr + 8*m_AccVersion.m_Offset;
        return len < *arr? len: 0;
    }
#endif
    return arr[arr.size()-1+m_AccVersion.m_Offset];
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


TSeqPos CWGSSeqIterator::GetSeqOffset(EClipType clip_type) const
{
    return GetClipByQualityFlag(clip_type)?
        GetClipQualityLeft(): 0;
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
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
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
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw___GetContigIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
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
    x_CheckValid("CWGSSeqIterator::HasSeqDescrBytes");
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


bool CWGSSeqIterator::HasNucProtDescrBytes(void) const
{
    x_CheckValid("CWGSSeqIterator::HasNucProtDescrBytes");
    return m_Cur->m_NUC_PROT_DESCR && !m_Cur->NUC_PROT_DESCR(m_CurrId).empty();
}


CTempString CWGSSeqIterator::GetNucProtDescrBytes(void) const
{
    x_CheckValid("CWGSSeqIterator::GetNucProtDescrBytes");
    CTempString descr_bytes;
    if ( m_Cur->m_NUC_PROT_DESCR ) {
        descr_bytes = m_Cur->NUC_PROT_DESCR(m_CurrId);
    }
    return descr_bytes;
}


bool CWGSSeqIterator::HasSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::HasSeq_descr");
    if ( flags & fSeqDescr ) {
        if ( HasSeqDescrBytes() ) {
            return true;
        }
    }
    if ( flags & fNucProtDescr ) {
        if ( HasNucProtDescrBytes() ) {
            return true;
        }
    }
    if ( flags & fMasterDescr ) {
        if ( !GetDb().GetMasterDescr().empty() ) {
            return true;
        }
    }
    return false;
}


CRef<CSeq_descr> CWGSSeqIterator::GetSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetSeq_descr");
    CRef<CSeq_descr> ret(new CSeq_descr);
    if ( (flags & fSeqDescr) && m_Cur->m_DESCR ) {
        sx_AddDescrBytes(*ret, *m_Cur->DESCR(m_CurrId));
    }
    if ( (flags & fNucProtDescr) && m_Cur->m_NUC_PROT_DESCR ) {
        sx_AddDescrBytes(*ret, *m_Cur->NUC_PROT_DESCR(m_CurrId));
    }
    if ( flags & fMasterDescr ) {
        GetDb().AddMasterDescr(*ret, nullptr, flags);
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


bool CWGSSeqIterator::CanHaveQualityGraph(void) const
{
    x_CheckValid("CWGSSeqIterator::CanHaveQualityGraph");
    return m_Cur->m_QUALITY;
}


bool CWGSSeqIterator::HasQualityGraph(void) const
{
    x_CheckValid("CWGSSeqIterator::HasQualityGraph");
    return m_Cur->m_QUALITY && !m_Cur->QUALITY(m_CurrId).empty();
}


inline
TSeqPos CWGSSeqIterator::x_GetQualityArraySize(void) const
{
    PROFILE(sw____GetContigQualSize);
    return m_Cur->m_Cursor.GetElementCount(m_CurrId, m_Cur->m_QUALITY, 8);
}


void
CWGSSeqIterator::GetQualityVec(vector<INSDC_quality_phred>& quality_vec) const
{
    x_CheckValid("CWGSSeqIterator::GetQualityArray");

    TSeqPos pos = GetSeqOffset();
    TSeqPos end = x_GetQualityArraySize();
    if ( end <= pos ) {
        quality_vec.clear();
        return;
    }
    TSeqPos size = end-pos;
    quality_vec.reserve((size+7)/8*8);
    quality_vec.resize(size);
    m_Cur->m_Cursor.ReadElements(m_CurrId, m_Cur->m_QUALITY, 8, pos, size,
                                 quality_vec.data());
}


string CWGSSeqIterator::GetQualityAnnotName(void) const
{
    return "Phrap Graph";
}


static inline void s_GetMinMax(const Uint1* arr, size_t size,
                               Uint1& min_v, Uint1& max_v)
{
    Uint1 min_v0 = 0xff, max_v0 = 0;
    Uint1 min_v1 = 0xff, max_v1 = 0;
    Uint1 min_v2 = 0xff, max_v2 = 0;
    Uint1 min_v3 = 0xff, max_v3 = 0;
    for ( ; size >= 4; arr += 4, size -= 4 ) {
        Uint1 v0 = arr[0];
        Uint1 v1 = arr[1];
        Uint1 v2 = arr[2];
        Uint1 v3 = arr[3];
        if ( v0 < min_v0 ) min_v0 = v0;
        if ( v1 < min_v1 ) min_v1 = v1;
        if ( v2 < min_v2 ) min_v2 = v2;
        if ( v3 < min_v3 ) min_v3 = v3;
        if ( v0 > max_v0 ) max_v0 = v0;
        if ( v1 > max_v1 ) max_v1 = v1;
        if ( v2 > max_v2 ) max_v2 = v2;
        if ( v3 > max_v3 ) max_v3 = v3;
    }
    for ( ; size > 0; arr += 1, size -= 1 ) {
        Uint1 v0 = arr[0];
        if ( v0 < min_v0 ) min_v0 = v0;
        if ( v0 > max_v0 ) max_v0 = v0;
    }
    min_v0 = min(min_v0, min_v2);
    max_v0 = max(max_v0, max_v2);
    min_v1 = min(min_v1, min_v3);
    max_v1 = max(max_v1, max_v3);
    min_v = min(min_v0, min_v1);
    max_v = max(max_v0, max_v1);
}


void CWGSSeqIterator::GetQualityAnnot(TAnnotSet& annot_set,
                                      TFlags flags) const
{
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    info.x_SetId(*this);
    x_GetQualityAnnot(annot_set, info);
}


void CWGSSeqIterator::x_GetQualityAnnot(TAnnotSet& annot_set,
                                        SWGSCreateInfo& info,
                                        TSeqPos pos,
                                        TSeqPos len) const
{
    x_CheckValid("CWGSSeqIterator::GetQualityAnnot");
    if ( !(info.flags & fQualityGraph) || !m_Cur->m_QUALITY ) {
        return;
    }
    
    PROFILE(sw___GetContigQual);
    TSeqPos end = len == kInvalidSeqPos? kInvalidSeqPos: pos + len;
    pos = max(pos, GetSeqOffset());
    end = min(end, x_GetQualityArraySize());
    if ( end <= pos ) {
        return;
    }
    TSeqPos size = end-pos;
    CByte_graph::TValues values;
    {
        PROFILE(sw____GetContigQualData);
        values.reserve((size+7)/8*8);
        values.resize(size);
        m_Cur->m_Cursor.ReadElements(m_CurrId, m_Cur->m_QUALITY, 8, pos, size,
                                     values.data());
    }

    Uint1 min_q = 0, max_q = 0;
    {
        PROFILE(sw____GetContigQualMinMax);
        s_GetMinMax((const Uint1*)values.data(), values.size(), min_q, max_q);
    }
    if ( max_q == 0 ) {
        return;
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CAnnotdesc> name(new CAnnotdesc);
    name->SetName(GetQualityAnnotName());
    annot->SetDesc().Set().push_back(name);
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetTitle("Phrap Quality");
    CSeq_interval& loc = graph->SetLoc().SetInt();
    loc.SetId(*info.main_id);
    loc.SetFrom(pos);
    loc.SetTo(end-1);
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

    if ( m_AccVersion.m_Offset != 0 ) {
        // not the last version of sequence
        return NCBI_gb_state_eWGSGenBankReplaced;
    }
    CVDBMgr::CRequestContextUpdater ctx_updater;
    NCBI_gb_state state = 0;
    if ( m_Cur->m_GB_STATE ) {
        state = *m_Cur->GB_STATE(m_CurrId);
    }
    if ( !state ) {
        state = m_Db->GetProjectGBState();
    }
    return state;
}


bool CWGSSeqIterator::HasPublicComment(void) const
{
    x_CheckValid("CWGSSeqIterator::HasPublicComment");

    if ( !m_Cur->m_PUBLIC_COMMENT ) {
        return false;
    }
    return !m_Cur->PUBLIC_COMMENT(m_CurrId).empty();
}


CTempString CWGSSeqIterator::GetPublicComment(void) const
{
    x_CheckValid("CWGSSeqIterator::GetPublicComment");

    if ( !m_Cur->m_PUBLIC_COMMENT ) {
        return string();
    }
    return *m_Cur->PUBLIC_COMMENT(m_CurrId);
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


struct CWGSSeqIterator::SAmbiguityAccess {
    SAmbiguityAccess(CRef<CWGSDb_Impl::SAmbiguityInfo>& info,
                     CWGSDb_Impl& db,
                     const CRef<CWGSDb_Impl::SSeqTableCursor>& seq,
                     TVDBRowId row_id)
        : m_Db(&db),
          m_Seq(seq),
          m_AmbiguityInfo(info)
        {
            if ( !m_AmbiguityInfo ) {
                m_AmbiguityInfo = db.GetAmbiguityInfo(row_id);
                if ( !m_AmbiguityInfo ) {
                    m_AmbiguityInfo = new CWGSDb_Impl::SAmbiguityInfo(row_id, *m_Db, *m_Seq);
                }
                info = m_AmbiguityInfo;
            }
        }
    ~SAmbiguityAccess()
        {
            if ( m_AmbiguityInfo ) {
                m_Db->PutAmbiguityInfo(m_AmbiguityInfo);
            }
        }

    SAmbiguityInfo* operator->() const
        {
            return m_AmbiguityInfo.GetNCPointer();
        }

    vector<Uint1> GetAmbiguityBytes() const
        {
            return operator->()->GetAmbiguityBytes(m_Seq.GetNCObject());
        }

    TSeqPos Get2naLengthExact(TSeqPos pos, TSeqPos len) const
        {
            return operator->()->Get2naLengthExact(pos, len,
                                                   m_Db.GetNCObject(), m_Seq.GetNCObject());
        }
    TSeqPos Get4naLengthExact(TSeqPos pos, TSeqPos len,
                              TSeqPos stop_2na_len, TSeqPos stop_gap_len) const
        {
            return operator->()->Get4naLengthExact(pos, len, stop_2na_len, stop_gap_len,
                                                   m_Db.GetNCObject(), m_Seq.GetNCObject());
        }
    TSeqPos GetGapLengthExact(TSeqPos pos, TSeqPos len) const
        {
            return operator->()->GetGapLengthExact(pos, len,
                                                   m_Db.GetNCObject(), m_Seq.GetNCObject());
        }

    CRef<CSeq_data> Get2na(TSeqPos pos, TSeqPos len) const
        {
            return operator->()->Get2na(pos, len,
                                        m_Seq.GetNCObject());
        }
    CRef<CSeq_data> Get4na(TSeqPos pos, TSeqPos len) const
        {
            return operator->()->Get4na(pos, len,
                                        m_Db.GetNCObject(), m_Seq.GetNCObject());
        }
    
private:
    SAmbiguityAccess(const SAmbiguityAccess&) = delete;
    void operator=(const SAmbiguityAccess&) = delete;
    
    CRef<CWGSDb_Impl> m_Db;
    CRef<CWGSDb_Impl::SSeqTableCursor> m_Seq;
    CRef<CWGSDb_Impl::SAmbiguityInfo> m_AmbiguityInfo;
};


CWGSSeqIterator::SAmbiguityAccess CWGSSeqIterator::GetAmbiguity() const
{
    return SAmbiguityAccess(m_AmbiguityInfo, GetDb(), m_Cur, m_CurrId);
}


void CWGSSeqIterator::GetGapInfo(TWGSContigGapInfo& gap_info) const
{
    x_CheckValid("CWGSSeqIterator::GetGapInfo");

    if ( HasGapInfo() ) {
        gap_info = GetAmbiguity()->GetGapInfo();
    }
    else {
        gap_info = TWGSContigGapInfo();
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
CRef<CSeq_literal> sx_MakeGapLiteral(TSeqPos len,
                                     NCBI_WGS_component_props props,
                                     NCBI_WGS_gap_linkage gap_linkage)
{
    CRef<CSeq_literal> literal(new CSeq_literal);
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
        -NCBI_WGS_gap_repeat |
        -NCBI_WGS_gap_unknown_type;
    _ASSERT(props < 0);
    int len_type    = -(-props & kLenTypeMask);
    int gap_type    = -(-props & kGapTypeMask);
    literal->SetLength(len);
    if ( len_type == NCBI_WGS_gap_unknown ) {
        literal->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }
    if ( gap_type || gap_linkage ) {
        CSeq_gap& gap = literal->SetSeq_data().SetGap();
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
        case NCBI_WGS_gap_unknown_type:
            gap.SetType(CSeq_gap::eType_unknown);
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
    return literal;
}


vector<Uint1> CWGSSeqIterator::GetAmbiguityBytes() const
{
    return GetAmbiguity().GetAmbiguityBytes();
}


CRef<CSeq_data> CWGSSeqIterator::Get2na(TSeqPos pos, TSeqPos len) const
{
    return GetAmbiguity().Get2na(pos, len);
}


CRef<CSeq_data> CWGSSeqIterator::Get4na(TSeqPos pos, TSeqPos len) const
{
    return GetAmbiguity().Get4na(pos, len);
}


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
// static const TSeqPos kMin2naSize = 2048;
// Actually use kAmbiguityBlockSize (=1024), it's optimal enough
// and allows to use precomputed ambiguity info directly
static const TSeqPos kMin2naSize = kAmbiguityBlockSize;

// size of chinks if the segment is split
static const TSeqPos kChunk4naSize = 1<<16; // 64Ki bases or 32KiB
static const TSeqPos kChunk2naSize = 1<<17; // 128Ki bases or 32KiB

// min size of segment to split
static const TSeqPos kSplit4naSize = kChunk4naSize+kChunk4naSize/4;
static const TSeqPos kSplit2naSize = kChunk2naSize; //+kChunk2naSize/4;

// end of delta control constants
/////////////////////////////////////////////////////////////////////////////

enum EDeltaType {
    eDelta_all,
    eDelta_split
};


inline
void CWGSSeqIterator::x_AddGap(TSegments& segments,
                               TSeqPos pos, TSeqPos len,
                               const TWGSContigGapInfo& gap_info) const
{
    SSegment seg;
    seg.range.SetFrom(pos);
    seg.range.SetLength(len);
    seg.is_gap = true;
    NCBI_WGS_component_props props = *gap_info.gaps_props;
    NCBI_WGS_gap_linkage linkage = 0;
    if ( gap_info.gaps_linkage ) {
        linkage = *gap_info.gaps_linkage;
    }
    seg.literal = sx_MakeGapLiteral(len, props, linkage);
    segments.push_back(seg);
}


inline
COpenRange<TSeqPos>
CWGSSeqIterator::x_NormalizeSeqRange(COpenRange<TSeqPos> range) const
{
    range.SetToOpen(min(range.GetToOpen(), GetSeqLength()));
    return range;
}


// add raw data as delta segments with explicit gap info
void CWGSSeqIterator::x_GetSegmentsWithExplicitGaps(TSegments& segments,
                                                    COpenRange<TSeqPos> range,
                                                    TWGSContigGapInfo gap_info,
                                                    TInstSegmentFlags flags) const
{
    range = x_NormalizeSeqRange(range);
    TSeqPos raw_offset = GetSeqOffset();
    TSeqPos pos = range.GetFrom() + raw_offset;
    TSeqPos len = range.GetLength();

    gap_info.SetPos(pos);
    auto ambiguity = GetAmbiguity();

    for ( ; len > 0; ) {
        if ( gap_info.IsInGap(pos) ) {
            // add gap
            TSeqPos gap_len = gap_info.GetGapLength(pos, len);
            _ASSERT(gap_len <= len);
            if ( flags & fInst_MakeGaps) {
                x_AddGap(segments, pos - raw_offset, gap_len, gap_info);
            }
            ++gap_info;
            len -= gap_len;
            pos += gap_len;
            _ASSERT(!gap_info || pos <= gap_info.GetFrom());
            continue;
        }
        
        // data segment
        TSeqPos rem_len = gap_info.GetDataLength(pos, len);
        _ASSERT(rem_len <= len);
        
        if ( flags & fInst_Split ) {
            // break data at the next chunk boundary
            TSeqPos chunk_start =
                (pos-raw_offset)/kDataChunkSize*kDataChunkSize;
            TSeqPos chunk_end = chunk_start + kDataChunkSize;
            rem_len = min(rem_len, chunk_end - pos);
        }

        bool is_2na;
        TSeqPos seg_len;
        if ( flags & fInst_Minimal ) {
            // whole region is either 2na or 4na
            seg_len = ambiguity->Get2naLengthBlock(pos, rem_len);
            if ( seg_len == rem_len ) {
                // 2na
                is_2na = true;
            }
            else {
                // 4na
                seg_len = rem_len;
                is_2na = false;
            }
        }
        else {
            // determine optimal sequence piece for regular delta
            seg_len = ambiguity->Get2naLengthBlock(pos, min(rem_len, kSplit2naSize));
            if ( seg_len >= kMin2naSize || seg_len == rem_len ) {
                if ( seg_len > kSplit2naSize ) {
                    seg_len = kChunk2naSize;
                }
                // 2na
                is_2na = true;
            }
            else {
                _ASSERT(seg_len < kSplit4naSize && seg_len < rem_len);
                TSeqPos seg_len_2na = seg_len;
                seg_len += ambiguity->Get4naLengthBlock(pos+seg_len,
                                                        min(rem_len, kSplit4naSize)-seg_len);
                if ( seg_len == seg_len_2na ) {
                    // no 4na added, so encode 2na, even if it's small
                    _ASSERT(seg_len > 0);
                    is_2na = true;
                }
                else {
                    // 4na
                    // limit too long 4na segments
                    if ( seg_len >= kSplit4naSize ) {
                        seg_len = kChunk4naSize;
                    }
                    is_2na = false;
                }
            }
        }
        
        SSegment seg;
        seg.is_gap = false;
        seg.range.SetFrom(pos - raw_offset);
        seg.range.SetLength(seg_len);
        if ( flags & fInst_MakeData ) {
            // actually generate Seq-data
            seg.literal = new CSeq_literal;
            seg.literal->SetLength(seg_len);
            if ( is_2na ) {
                // 2na
                seg.literal->SetSeq_data(*ambiguity.Get2na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                seg.literal->SetSeq_data(*ambiguity.Get4na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
        }
        segments.push_back(seg);
        pos += seg_len;
        len -= seg_len;
    }
}


// add raw data as delta segments with gap recovering
void CWGSSeqIterator::x_GetSegmentsWithRecoveredGaps(TSegments& segments,
                                                     COpenRange<TSeqPos> range) const
{
    range = x_NormalizeSeqRange(range);
    TSeqPos raw_offset = GetSeqOffset();
    TSeqPos pos = range.GetFrom() + raw_offset;
    TSeqPos len = range.GetLength();

    // max size of gap segment
    const TSeqPos kMinGapSize = 20;
    // size of gap segment if its actual size is unknown
    const TSeqPos kUnknownGapSize = 100;
    SAmbiguityAccess ambiguity = GetAmbiguity();
    
    for ( ; len > 0; ) {
        SSegment seg;
        seg.range.SetFrom(pos - raw_offset);
        seg.is_gap = false;
        seg.literal = new CSeq_literal;

        TSeqPos rem_len = len;
        TSeqPos seg_len = ambiguity.Get2naLengthExact(pos, min(rem_len, kSplit2naSize));
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len > kSplit2naSize ) {
                seg_len = kChunk2naSize;
            }
            seg.literal->SetSeq_data(*ambiguity.Get2na(pos, seg_len));
            _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos seg_len_2na = seg_len;
            seg_len += ambiguity.Get4naLengthExact(pos+seg_len,
                                                   min(rem_len, kSplit4naSize)-seg_len,
                                                   kMin2naSize, kMinGapSize);
            if ( kRecoverGaps && seg_len == 0 ) {
                seg_len = ambiguity.GetGapLengthExact(pos, rem_len);
                _ASSERT(seg_len > 0);
                seg.is_gap = true;
                if ( seg_len == kUnknownGapSize ) {
                    seg.literal->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                }
            }
            else if ( seg_len == seg_len_2na ) {
                seg.literal->SetSeq_data(*ambiguity.Get2na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                if ( seg_len >= kSplit4naSize ) {
                    seg_len = kChunk4naSize;
                }
                seg.literal->SetSeq_data(*ambiguity.Get4na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
        }

        seg.range.SetLength(seg_len);
        seg.literal->SetLength(seg_len);
        segments.push_back(seg);
        pos += seg_len;
        len -= seg_len;
    }
}


void CWGSSeqIterator::x_SetDelta(CSeq_inst& inst,
                                 const TSegments& segments) const
{
    inst.SetRepr(CSeq_inst::eRepr_delta);
    TSeqPos pos = 0;
    CDelta_ext::Tdata& delta = inst.SetExt().SetDelta().Set();
    ITERATE ( TSegments, it, segments ) {
        CRef<CDelta_seq> seq(new CDelta_seq);
        _ASSERT(it->range.GetFrom() == pos);
        if ( it->literal ) {
            _ASSERT(it->range.GetLength() == it->literal->GetLength());
            seq->SetLiteral(it->literal.GetNCObject());
        }
        else {
            seq->SetLiteral().SetLength(it->range.GetLength());
        }
        delta.push_back(seq);
        pos += it->range.GetLength();
    }
    _ASSERT(pos == inst.GetLength());
}


void CWGSSeqIterator::x_SetDeltaOrData(CSeq_inst& inst,
                                       const TSegments& segments) const
{
    if ( segments.size() == 1 && !segments[0].is_gap ) {
        // plain single data segment, delta is not necessary
        _ASSERT(segments[0].literal);
        _ASSERT(!segments[0].literal->IsSetFuzz());
        _ASSERT(segments[0].literal->IsSetSeq_data());
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetSeq_data(segments[0].literal.GetNCObject().SetSeq_data());
        inst.ResetStrand();
        inst.ResetExt();
    }
    else {
        x_SetDelta(inst, segments);
    }
}


CRef<CSeq_inst> CWGSSeqIterator::x_GetSeq_inst(SWGSCreateInfo& info) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw___GetContigInst);
    x_CheckValid("CWGSSeqIterator::GetSeq_inst");
    auto ambiguity = GetAmbiguity();
    CRef<CSeq_inst> inst(new CSeq_inst);
    inst->SetMol(GetDb().GetContigMolType());
    if ( IsCircular() ) {
        inst->SetTopology(CSeq_inst::eTopology_circular);
    }
    TSeqPos length = GetSeqLength();
    inst->SetLength(length);
    if ( length == 0 ) {
        inst->SetRepr(CSeq_inst::eRepr_not_set);
        return inst;
    }
    COpenRange<TSeqPos> whole(0, length);
    if ( (info.flags & fMaskInst) == fInst_ncbi4na ) {
        inst->SetRepr(CSeq_inst::eRepr_raw);
        inst->SetSeq_data(*ambiguity.Get4na(GetSeqOffset(), length));
    }
    else if ( HasGapInfo() ) {
        CRef<CSeq_id> id = GetAccSeq_id();
        TWGSContigGapInfo gap_info;
        GetGapInfo(gap_info);
        if ( !info.split_data ) {
            TSegments segments;
            TInstSegmentFlags inst_flags = fInst_MakeGaps|fInst_MakeData;
            x_GetSegmentsWithExplicitGaps(segments, whole, gap_info, inst_flags);
            x_SetDeltaOrData(*inst, segments);
        }
        else {
            // split
            TSegments segments;
            TInstSegmentFlags inst_flags = fInst_MakeGaps|fInst_Split;
            x_GetSegmentsWithExplicitGaps(segments, whole, gap_info, inst_flags);
            x_SetDelta(*inst, segments);

            CRef<CID2S_Chunk_Info> chunk;
            ITERATE ( TSegments, it, segments ) {
                if ( it->is_gap ) {
                    continue;
                }
                TSeqPos pos = it->range.GetFrom();
                TSeqPos end = it->range.GetToOpen();
                int chunk_id = pos/kDataChunkSize*kChunkIdStep + eChunk_data;
                if ( !chunk || chunk->GetId() != chunk_id ) {
                    chunk = new CID2S_Chunk_Info;
                    chunk->SetId().Set(chunk_id);
                    info.split->SetChunks().push_back(chunk);
                }
                CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
                chunk->SetContent().push_back(content);
                sx_SetSplitInterval(content->SetSeq_data(), *info.main_id,
                                    pos, end);
            }
        }
    }
    else {
        TSegments segments;
        x_GetSegmentsWithRecoveredGaps(segments, whole);
        x_SetDeltaOrData(*inst, segments);
    }
    return inst;
}


CRef<CSeq_inst> CWGSSeqIterator::GetSeq_inst(TFlags flags) const
{
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    return x_GetSeq_inst(info);
}


void SWGSCreateInfo::x_SetFlags(TFlags flags)
{
    this->flags = flags;
    split_version = kAssignedDefaultSplitVersion;
}
    

void SWGSCreateInfo::x_SetSplitVersion(TSplitVersion split_version)
{
    if ( split_version == kDefaultSplitVersion ) {
        flags = fDefaultFlags;
    }
    else if ( split_version == kAssignedDefaultSplitVersion ) {
        flags = fDefaultFlags;
    }
    else {
        NCBI_THROW_FMT(CSraException, eInvalidArg,
                       "SWGSCreateInfo::SetSplitVersion("<<split_version<<"): "
                       "unknown split version");
    }
    this->split_version = split_version;
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
        //LOG_POST(MSerial_AsnText<<*dst.back());
    }
}


CBioseq_set& SWGSCreateInfo::x_GetProtSet(void)
{
    if ( !entry->IsSet() ) {
        CRef<CBioseq_set> seqset(new CBioseq_set);
        seqset->SetClass(CBioseq_set::eClass_nuc_prot);
        if ( split_feat || split_prod ) {
            seqset->SetId().SetId(kMainEntryId);
        }
        CRef<CSeq_entry> main_entry(new CSeq_entry);
        main_entry->SetSeq(*main_seq);
        seqset->SetSeq_set().push_back(main_entry);
        entry->SetSet(*seqset);
    }
    return entry->SetSet();
}


void SWGSCreateInfo::x_AddFeaturesDirect(TVDBRowIdRange range,
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
                CRef<CSeq_annot> annot(new CSeq_annot);
                x_GetProtSet().SetAnnot().push_back(annot);
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


struct SWGSFeatChunkInfo : public SWGSDb_Defs {
    CRef<CSeq_id> main_id;
    CRef<CSeq_id> feat_id;
    CRef<CID2S_Bioseq_Ids::C_E> seq_place;

    size_t feat_count;

    struct SFeatureSet {
        vector< COpenRange<TSeqPos> > loc_ranges;
        bitset<CSeqFeatData::e_MaxChoice> feat_types;

        void Reset() {
            loc_ranges.clear();
            feat_types.reset();
        }
        
        void AddFeatType(NCBI_WGS_feattype feat_type)
            {
                if ( feat_type >= CSeqFeatData::e_MaxChoice ) {
                    feat_type = CSeqFeatData::e_not_set;
                }
                feat_types.set(feat_type);
            }
        
        static
        bool ExpandRange(COpenRange<TSeqPos>& dst, COpenRange<TSeqPos> src)
            {
                static const TSeqPos kMaxGap = 100000;
                if ( src.GetFrom() >= dst.GetFrom() ) {
                    // after
                    if ( src.GetFrom() > dst.GetToOpen() + kMaxGap ) {
                        // too far
                        return false;
                    }
                }
                else {
                    // before
                    if ( src.GetToOpen() + kMaxGap < dst.GetFrom() ) {
                        // too far
                        return false;
                    }
                    dst.SetFrom(src.GetFrom());
                }
                if ( src.GetToOpen() > dst.GetToOpen() ) {
                    dst.SetToOpen(src.GetToOpen());
                }
                return true;
            }
        void AddFeatRange(COpenRange<TSeqPos> range)
            {
                if ( loc_ranges.empty() || !ExpandRange(loc_ranges.back(), range) ) {
                    loc_ranges.push_back(range);
                }
            }
        void AddFeature(NCBI_WGS_feattype type, COpenRange<TSeqPos> range)
            {
                AddFeatType(type);
                AddFeatRange(range);
            }

        bool HasFeatures() const
            {
                return feat_types.any();
            }
        void AddContent(CID2S_Chunk_Info& chunk, CSeq_id& feat_id);
    };
    SFeatureSet features[2]; // w/o and w/ product

    SWGSFeatChunkInfo(CSeq_id& main_id, CSeq_id& feat_id)
        : main_id(&main_id),
          feat_id(&feat_id),
          seq_place(new CID2S_Bioseq_Ids::C_E)
        {
            sx_SetSplitId(*seq_place, main_id);
            Reset();
        }
    
    void Reset() {
        feat_count = 0;
        for ( auto& fs : features ) {
            fs.Reset();
        }
    }

    void AddFeature(bool with_product, NCBI_WGS_feattype type, COpenRange<TSeqPos> range)
        {
            features[with_product].AddFeature(type, range);
            ++feat_count;
        }

    CRef<CID2S_Chunk_Info> CreateChunkInfo(int index,
                                           CWGSProteinIterator& prot_it,
                                           const vector<TVDBRowId>& product_row_ids,
                                           size_t product_index);
};


static void s_AddGiRange(CID2S_Seq_loc::TLoc_set& loc_set,
                         CSeq_id::TGi gi_range_start,
                         CSeq_id::TGi gi_range_stop)
{
    if ( gi_range_stop == gi_range_start ) {
        return;
    }
    CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
    if ( gi_range_stop == gi_range_start+GI_CONST(1) ) {
        loc->SetWhole_gi(gi_range_start);
    }
    else {
        CID2S_Gi_Range& gi_range = loc->SetWhole_gi_range();
        gi_range.SetStart(gi_range_start);
        gi_range.SetCount(GI_TO(CID2S_Gi_Range::TCount, gi_range_stop - gi_range_start));
    }
    loc_set.push_back(loc);
}


CRef<CID2S_Chunk_Info> SWGSFeatChunkInfo::CreateChunkInfo(int index,
                                                          CWGSProteinIterator& prot_it,
                                                          const vector<TVDBRowId>& product_row_ids,
                                                          size_t product_index)
{
    // pack sorted locations once more
    //sort(loc_ranges.begin(), loc_ranges.end());
    CRef<CID2S_Chunk_Info> chunk(new CID2S_Chunk_Info);
    chunk->SetId().Set(index*kChunkIdStep+eChunk_feat);
    CRef<CID2S_Chunk_Content> content;
    if ( features[0].HasFeatures() ) {
        // add annot place on sequence
        content = new CID2S_Chunk_Content;
        chunk->SetContent().push_back(content);
        content->SetSeq_annot_place().SetBioseqs().Set().push_back(seq_place);
        
        // add annot types and locations
        features[0].AddContent(*chunk, *feat_id);
    }
    if ( features[1].HasFeatures() ) {
        // add annot place on nuc-prot-set
        content = new CID2S_Chunk_Content;
        chunk->SetContent().push_back(content);
        content->SetSeq_annot_place().SetBioseq_sets().Set().push_back(kMainEntryId);

        // add annot types and locations
        features[1].AddContent(*chunk, *feat_id);

        // add annot types and locations for products
        CID2S_Seq_annot_Info& annot_info = chunk->SetContent().back()->SetSeq_annot();
        CRef<CID2S_Seq_loc> old_loc(&annot_info.SetSeq_loc());
        annot_info.ResetSeq_loc();
        auto& loc_set = annot_info.SetSeq_loc().SetLoc_set();
        loc_set.push_back(old_loc);
        EFeatLocIdType feat_loc_id_type = eFeatLocIdUninitialized;
        CSeq_id::TGi gi_range_start = ZERO_GI, gi_range_stop = ZERO_GI;
        for ( auto it = product_row_ids.begin()+product_index; it != product_row_ids.end(); ++it ) {
            if ( !prot_it.SelectRow(*it) ) {
                ERR_POST_X(11, "CWGSDb::x_AddProducts: "
                           "invalid protein row id: "<<*it);
                continue;
            }
            // fix feature ids
            // it can be accession.version and accession
            CSeq_id::TGi gi = prot_it.GetGi();
            if ( gi != ZERO_GI ) {
                if ( feat_loc_id_type == eFeatLocIdUninitialized ) {
                    feat_loc_id_type = prot_it.GetDb().GetFeatLocIdType();
                }
                if ( feat_loc_id_type == eFeatLocIdGi ) {
                    if ( gi != gi_range_stop ) {
                        s_AddGiRange(loc_set, gi_range_start, gi_range_stop);
                        gi_range_start = gi;
                    }
                    gi_range_stop = gi+GI_CONST(1);
                    continue;
                }
            }
            s_AddGiRange(loc_set, gi_range_start, gi_range_stop);
            CRef<CSeq_id> feat_id = prot_it.GetId(fIds_acc|fIds_gnl);
            //LOG_POST("Feat info for "<<feat_id->AsFastaString());
            CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
            loc->SetWhole_seq_id(*feat_id);
            loc_set.push_back(loc);
        }
        s_AddGiRange(loc_set, gi_range_start, gi_range_stop);
    }

    // add empty feat-ids to prevent loading by id
    content = new CID2S_Chunk_Content;
    chunk->SetContent().push_back(content);
    content->SetFeat_ids();

    // done
    Reset();
    return chunk;
}


void SWGSFeatChunkInfo::SFeatureSet::AddContent(CID2S_Chunk_Info& chunk, CSeq_id& feat_id)
{
    // add features
    CRef<CID2S_Chunk_Content> content;
    content = new CID2S_Chunk_Content;
    chunk.SetContent().push_back(content);
    CID2S_Seq_annot_Info& annot_info = content->SetSeq_annot();
    // types
    for ( int type = CSeqFeatData::e_not_set; type < CSeqFeatData::e_MaxChoice; ++type ) {
        if ( feat_types[type] ) {
            CRef<CID2S_Feat_type_Info> type_info(new CID2S_Feat_type_Info);
            type_info->SetType(CSeqFeatData::E_Choice(type));
            annot_info.SetFeat().push_back(type_info);
        }
    }
    // locations
    CID2S_Seq_id_Ints& intervals = annot_info.SetSeq_loc().SetSeq_id_ints();
    intervals.SetSeq_id(feat_id);
    //LOG_POST("Feat info for "<<feat_id.AsFastaString());
    for ( auto r : loc_ranges ) {
        CRef<CID2S_Interval> interval(new CID2S_Interval);
        interval->SetStart(r.GetFrom());
        interval->SetLength(r.GetLength());
        intervals.SetInts().push_back(interval);
    }
}


void SWGSCreateInfo::x_AddFeaturesSplit(TVDBRowIdRange range,
                                        vector<TVDBRowId>& product_row_ids)
{
    // for each chunk we need to create:
    // ID2S-Chunk-Info.content.seq-annot-place. nuc-prot-set entry id or contig Seq-id
    // ID2S-Chunk-Info.content.seq-annot.feat&seq-loc
    int chunk_index = 0;
    SWGSFeatChunkInfo c(*main_id, *feat_id);
    COpenRange<TSeqPos> seq_range;
    size_t product_index = 0;
    CWGSProteinIterator prot_it(db);
    for ( CWGSFeatureIterator feat_it(db, range); feat_it; ++feat_it ) {
        bool with_product = false;
        if ( TVDBRowId product_row_id = feat_it.GetProductRowId() ) {
            // product feature
            product_row_ids.push_back(product_row_id);
            with_product = true;
        }
        c.AddFeature(with_product, feat_it.GetFeatType(), feat_it.GetLocRange());
        if ( c.feat_count == kFeatPerChunk ) {
            split->SetChunks().push_back(c.CreateChunkInfo(chunk_index++,
                                                           prot_it,
                                                           product_row_ids, product_index));
            product_index = product_row_ids.size();
        }
    }
    if ( c.feat_count ) {
        split->SetChunks().push_back(c.CreateChunkInfo(chunk_index,
                                                       prot_it,
                                                       product_row_ids, product_index));
    }
    if ( !product_row_ids.empty() ) {
        x_GetProtSet();
    }
}


inline
void SWGSCreateInfo::x_AddFeatures(TVDBRowIdRange range,
                                   vector<TVDBRowId>& product_row_ids)
{
    if ( split_feat ) {
        x_AddFeaturesSplit(range, product_row_ids);
    }
    else {
        x_AddFeaturesDirect(range, product_row_ids);
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


void CWGSSeqIterator::x_AddQualityChunkInfo(SWGSCreateInfo& info) const
{
    CRef<CID2S_Bioseq_Ids::C_E> place(new CID2S_Bioseq_Ids::C_E);
    sx_SetSplitId(*place, *info.main_id);
    
    TSeqPos size = GetSeqLength();
    for ( TSeqPos k = 0, pos = 0; pos < size; ++k, pos += kQualChunkSize ) {
        TSeqPos end = min(size, pos + kQualChunkSize);
        int chunk_id = k*kChunkIdStep + eChunk_qual;

        CRef<CID2S_Chunk_Info> chunk(new CID2S_Chunk_Info);
        info.split->SetChunks().push_back(chunk);

        chunk->SetId().Set(chunk_id);

        CRef<CID2S_Chunk_Content> content;

        // content of quality annot
        content = new CID2S_Chunk_Content;
        chunk->SetContent().push_back(content);
        content->SetFeat_ids();
        CID2S_Seq_annot_Info& annot_info =
            content->SetSeq_annot();
        annot_info.SetName(GetQualityAnnotName());
        annot_info.SetGraph();
        sx_SetSplitInterval(annot_info.SetSeq_loc(), *info.main_id, pos, end);

        // place of quality annot
        content = new CID2S_Chunk_Content;
        chunk->SetContent().push_back(content);
        content->SetSeq_annot_place().SetBioseqs().Set().push_back(place);
    }
}


void CWGSSeqIterator::x_CreateBioseq(SWGSCreateInfo& info) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw__GetContigBioseq);
    _ASSERT(!info.main_seq);
    info.x_SetSeq(*this);
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetContigDescr);
        if ( (info.flags & fMaskDescr) == fSeqDescr && !(info.flags & fSeqDescrObj)  ) {
            // only own descriptors
            if ( m_Cur->m_DESCR ) {
                CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
                if ( !descr.empty() ) {
                    info.x_AddDescr(*descr);
                }
            }
        }
        else {
            // full descirptor collection
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
            }
        }
    }
    if ( info.flags & fMaskAnnot ) {
        PROFILE(sw___GetContigAnnot);
        GetAnnotSet(info.main_seq->SetAnnot(), info.flags);
        bool has_split_annot = false;
        if ( (info.flags & fQualityGraph) && CanHaveQualityGraph() ) {
            if ( info.split_qual ) {
                x_AddQualityChunkInfo(info);
                has_split_annot = true;
            }
            else {
                x_GetQualityAnnot(info.main_seq->SetAnnot(), info);
            }
        }
        if ( !has_split_annot && info.main_seq->GetAnnot().empty() ) {
            info.main_seq->ResetAnnot();
        }
    }
    info.main_seq->SetInst(*x_GetSeq_inst(info));
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
    CRef<CSeq_id> save_main_id = main_id;
    CRef<CSeq_id> save_feat_id = feat_id;
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
            ERR_POST_X(11, "CWGSDb::x_AddProducts: "
                       "invalid protein row id: "<<*it);
            continue;
        }
        entry = null;
        x_ResetSeq();
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
    main_id = save_main_id;
    feat_id = save_feat_id;
    entry = save_entry;
}


void SWGSCreateInfo::x_CreateProtSet(TVDBRowIdRange range)
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
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
                    chunk->SetId().Set(chunk_index*kChunkIdStep + eChunk_prod);
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
                    ERR_POST_X(12, "CWGSDb::x_CreateProtSet: "
                               "invalid protein row id: "<<*it);
                    continue;
                }
                CBioseq::TId prot_ids;
                prot_it.GetIds(prot_ids, flags);
                sx_AddSplitIds(*ids, prot_ids);
            }
        }
        else {
            x_AddProducts(product_row_ids);
        }
    }
}


static
void sx_AddMasterDescr(const CWGSDb& db, SWGSCreateInfo& info, SWGSDb_Defs::TFlags flags)
{
    if ( !db->GetMasterDescr().empty() ) {
        db->AddMasterDescr(info.entry->SetDescr(), info.main_seq, flags);
    }
}


void CWGSSeqIterator::x_CreateEntry(SWGSCreateInfo& info) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_GetContigEntry);
    if ( !(info.flags & fSeqAnnot) || !info.db->FeatTable() ) {
        // plain sequence only without FEATURE table
        x_CreateBioseq(info);
    }
    else {
        TFlags flags = info.flags;
        info.flags = flags & ~(fMasterDescr | fNucProtDescr);
        if ( flags & fMasterDescr ) {
            // we need main sequence descriptors deserialized
            info.flags |= fSeqDescrObj;
        }
        x_CreateBioseq(info);
        info.flags = flags;
        info.x_CreateProtSet(GetLocFeatRowIdRange());
        if ( flags & (fNucProtDescr | fMasterDescr) ) {
            if ( (flags & fNucProtDescr) && m_Cur->m_NUC_PROT_DESCR ) {
                CVDBStringValue descr = m_Cur->NUC_PROT_DESCR(m_CurrId);
                if ( !descr.empty() ) {
                    sx_AddDescrBytes(info.entry->SetDescr(), descr);
                }
            }
            if ( flags & fMasterDescr ) {
                sx_AddMasterDescr(m_Db, info, flags);
            }
        }
    }
}


bool CWGSSeqIterator::x_InitSplit(SWGSCreateInfo& info) const
{
    // split data if...
    PROFILE(sw_InitSplit);
    info.split = null;
    info.data = null;
    if ( kEnableSplitData && (info.flags & fSplitSeqData) &&
         ((info.flags & fMaskInst) == fInst_delta) && // delta is requested
         HasGapInfo() && // we have explicit gap info
         GetSeqLength() >= kMinDataSplitSize // data is big enough
        ) {
        info.split_data = true;
    }
    if ( kEnableSplitProd && (info.flags & fSplitProducts) &&
         sx_HasMoreProducts(m_Db, GetLocFeatRowIdRange(), kProdPerChunk) ) {
        // split products if there are many enough
        info.split_prod = true;
    }
    if ( kEnableSplitFeat && (info.flags & fSplitFeatures) && // if split is enabled and requested
         GetLocFeatRowIdRange().second >= kMinFeatCountToSplit && // if there are anough features
         GetDb().GetFeatLocIdType() != eFeatLocIdAccNoVer ) { // if feat Seq-ids are unambiguous
        // split features if there are many enough
        info.split_feat = true;
    }
    if ( kEnableSplitQual && (info.flags & fSplitQualityGraph) &&
         CanHaveQualityGraph() ) {
        info.split_qual = true;
    }
    if ( !info.split_data && !info.split_prod && !info.split_feat &&
         !info.split_qual ) {
        return false;
    }
    info.entry = new CSeq_entry;
    info.split = new CID2S_Split_Info;
    info.split->SetSkeleton(*info.entry);
    info.split->SetChunks();
    return true;
}


void CWGSSeqIterator::x_CreateSplit(SWGSCreateInfo& info) const
{
    x_CreateEntry(info);
}


void CWGSSeqIterator::x_CreateQualityChunk(SWGSCreateInfo& info,
                                           unsigned index) const
{
    PROFILE(sw_CreateQualityChunk);
    CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
    sx_SetSplitId(data->SetId(), *info.main_id);
    x_GetQualityAnnot(data->SetAnnots(), info,
                      index*kQualChunkSize, kQualChunkSize);
    info.chunk->SetData().push_back(data);
}


void CWGSSeqIterator::x_CreateDataChunk(SWGSCreateInfo& info,
                                        unsigned index) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_CreateDataChunk);
    CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
    sx_SetSplitId(data->SetId(), *info.main_id);
    COpenRange<TSeqPos> range;
    range.SetFrom(index*kDataChunkSize);
    range.SetLength(kDataChunkSize);

    TWGSContigGapInfo gap_info;
    GetGapInfo(gap_info);
    TSegments segments;
    TInstSegmentFlags inst_flags = fInst_MakeData;
    x_GetSegmentsWithExplicitGaps(segments, range, gap_info, inst_flags);
    ITERATE ( TSegments, it, segments ) {
        _ASSERT(!it->is_gap);
        _ASSERT(it->literal && it->literal->IsSetSeq_data());
        CRef<CID2S_Sequence_Piece> piece(new CID2S_Sequence_Piece);
        piece->SetStart(it->range.GetFrom());
        piece->SetData().push_back(it->literal);
        data->SetSeq_data().push_back(piece);
    }
    info.chunk->SetData().push_back(data);
}


void CWGSSeqIterator::x_CreateProductsChunk(SWGSCreateInfo& info,
                                            unsigned index) const
{
    PROFILE(sw_CreateProductsChunk);
    vector<TVDBRowId> product_row_ids;
    TVDBRowId skip = index*kProdPerChunk;
    for ( CWGSFeatureIterator feat_it(m_Db, GetLocFeatRowIdRange()); feat_it; ++feat_it ) {
        if ( TVDBRowId row_id = feat_it.GetProductRowId() ) {
            if ( skip ) {
                --skip;
                continue;
            }
            product_row_ids.push_back(row_id);
            if ( product_row_ids.size() == kProdPerChunk ) {
                break;
            }
        }
    }
    info.x_AddProducts(product_row_ids);
}


void CWGSSeqIterator::x_CreateFeaturesChunk(SWGSCreateInfo& info,
                                            unsigned index) const
{
    PROFILE(sw_CreateFeaturesChunk);
    // select range of feature table rows
    auto range = GetLocFeatRowIdRange();
    auto feat_start = range.first + kFeatPerChunk*index;
    auto feat_stop = min(range.first+range.second, feat_start+kFeatPerChunk);
    range.first = feat_start;
    range.second = max(feat_start, feat_stop)-feat_start;
    // create features
    info.chunk->SetData();
    CSeq_annot::TData::TFtable* main_features = 0;
    CSeq_annot::TData::TFtable* product_features = 0;
    for ( CWGSFeatureIterator feat_it(m_Db, range); feat_it; ++feat_it ) {
        CSeq_annot::TData::TFtable* dst;
        if ( feat_it.GetProductRowId() ) {
            // product feature
            if ( !product_features ) {
                CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
                info.chunk->SetData().push_back(data);
                data->SetId().SetBioseq_set(kMainEntryId);
                CRef<CSeq_annot> annot(new CSeq_annot);
                data->SetAnnots().push_back(annot);
                product_features = &annot->SetData().SetFtable();
            }
            dst = product_features;
        }
        else {
            // plain feature
            if ( !main_features ) {
                CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
                info.chunk->SetData().push_back(data);
                data->SetId().SetSeq_id(*info.main_id);
                CRef<CSeq_annot> annot(new CSeq_annot);
                data->SetAnnots().push_back(annot);
                main_features = &annot->SetData().SetFtable();
            }
            dst = main_features;
        }
        info.x_AddFeature(feat_it, *dst);
    }
}


void CWGSSeqIterator::x_CreateChunk(SWGSCreateInfo& info,
                                    TChunkId chunk_id) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_GetChunk);
    info.x_SetId(*this);
    EChunkType type = EChunkType(chunk_id%kChunkIdStep);
    unsigned index = chunk_id/kChunkIdStep;
    if ( type == eChunk_qual ) {
        x_CreateQualityChunk(info, index);
    }
    else if ( type == eChunk_prod ) {
        x_CreateProductsChunk(info, index);
    }
    else if ( type == eChunk_feat ) {
        x_CreateFeaturesChunk(info, index);
    }
    else if ( type == eChunk_data ) {
        x_CreateDataChunk(info, index);
    }
    else {
        NCBI_THROW_FMT(CSraException, eInvalidArg,
                       "CWGSSeqIterator::CreateChunk("<<chunk_id<<"): "
                       "unsupported chunk type: "<<type);
    }
}


CRef<CBioseq> CWGSSeqIterator::GetBioseq(TFlags flags) const
{
    PROFILE(sw_GetBioseq);
    x_CheckValid("CWGSSeqIterator::GetBioseq");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSSeqIterator::GetSeq_entry(TFlags flags) const
{
    PROFILE(sw_GetSeq_entry);
    x_CheckValid("CWGSSeqIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    info.entry = new CSeq_entry;
    x_CreateEntry(info);
    return info.entry;
}


CRef<CAsnBinData> CWGSSeqIterator::GetSeq_entryData(TFlags flags) const
{
    PROFILE(sw_GetSeq_entryData);
    x_CheckValid("CWGSSeqIterator::GetSeq_entryData");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    info.entry = new CSeq_entry;
    info.data = new CWGSAsnBinData(*info.entry);
    x_CreateEntry(info);
    return CRef<CAsnBinData>(info.data);
}


CRef<CID2S_Split_Info> CWGSSeqIterator::GetSplitInfo(TFlags flags) const
{
    return GetSplitInfoAndVersion(flags).first;
}


pair<CRef<CID2S_Split_Info>, CWGSSeqIterator::TSplitVersion>
CWGSSeqIterator::GetSplitInfoAndVersion(TFlags flags) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_GetSplitInfo);
    x_CheckValid("CWGSSeqIterator::GetSplitInfo");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    if ( x_InitSplit(info) ) {
        x_CreateSplit(info);
    }
    return make_pair(info.split, info.split_version);
}


CRef<CAsnBinData> CWGSSeqIterator::GetSplitInfoData(TFlags flags) const
{
    return GetSplitInfoDataAndVersion(flags).first;
}


pair<CRef<CAsnBinData>, CWGSSeqIterator::TSplitVersion>
CWGSSeqIterator::GetSplitInfoDataAndVersion(TFlags flags) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw_GetSplitInfoData);
    x_CheckValid("CWGSSeqIterator::GetSplitInfoData");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    if ( x_InitSplit(info) ) {
        info.data = new CWGSAsnBinData(*info.split);
        x_CreateSplit(info);
    }
    return make_pair(CRef<CAsnBinData>(info.data), info.split_version);
}


CRef<CID2S_Chunk> CWGSSeqIterator::GetChunk(TChunkId chunk_id,
                                            TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetChunk");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    info.chunk = new CID2S_Chunk;
    x_CreateChunk(info, chunk_id);
    return info.chunk;
}


CRef<CID2S_Chunk> CWGSSeqIterator::GetChunkForVersion(TChunkId chunk_id,
                                                      TSplitVersion split_version) const
{
    x_CheckValid("CWGSSeqIterator::GetChunk");
    SWGSCreateInfo info(m_Db, eFromSplitVersion, split_version);
    info.chunk = new CID2S_Chunk;
    x_CreateChunk(info, chunk_id);
    return info.chunk;
}


CRef<CAsnBinData> CWGSSeqIterator::GetChunkData(TChunkId chunk_id,
                                                TFlags flags) const
{
    x_CheckValid("CWGSSeqIterator::GetChunkData");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    info.chunk = new CID2S_Chunk;
    info.data = new CWGSAsnBinData(*info.chunk);
    x_CreateChunk(info, chunk_id);
    return CRef<CAsnBinData>(info.data);
}


CRef<CAsnBinData> CWGSSeqIterator::GetChunkDataForVersion(TChunkId chunk_id,
                                                          TSplitVersion split_version) const
{
    x_CheckValid("CWGSSeqIterator::GetChunkData");
    SWGSCreateInfo info(m_Db, eFromSplitVersion, split_version);
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


CWGSScaffoldIterator::CWGSScaffoldIterator(void)
    : m_CurrId(0),
      m_FirstGoodId(0),
      m_FirstBadId(0)
{
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
    PROFILE(sw_ScafIterator);
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs_db ) {
        return;
    }
    m_Cur = wgs_db.GetNCObject().Scf();
    if ( !m_Cur ) {
        return;
    }
    m_Db = wgs_db;
    TVDBRowIdRange range = m_Cur->m_SCAFFOLD_NAME.GetRowIdRange(m_Cur->m_Cursor);
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
    CVDBMgr::CRequestContextUpdater ctx_updater;
    return *CVDBStringValue(m_Cur->ACCESSION(m_CurrId));
}


int CWGSScaffoldIterator::GetAccVersion(void) const
{
    // scaffolds always have version 1
    return 1;
}


NCBI_gb_state CWGSScaffoldIterator::GetGBState(void) const
{
    x_CheckValid("CWGSScaffoldIterator::GetGBState");

    CVDBMgr::CRequestContextUpdater ctx_updater;
    NCBI_gb_state state = 0;
    if ( m_Cur->m_GB_STATE ) {
        state = *m_Cur->GB_STATE(m_CurrId);
    }
    if ( !state ) {
        state = m_Db->GetProjectGBState();
    }
    return state;
}


CRef<CSeq_id> CWGSScaffoldIterator::GetAccSeq_id(void) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
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


CRef<CSeq_id> CWGSScaffoldIterator::GetGeneralOrPatentSeq_id(void) const
{
    CTempString name = GetScaffoldName();
    if ( name.empty() || sx_GetStringId(name) == m_CurrId ) {
        return null;
    }
    return GetDb().GetGeneralSeq_id(name);
}


CRef<CSeq_id> CWGSScaffoldIterator::GetGeneralSeq_id(void) const
{
    return GetGeneralOrPatentSeq_id();
}


CRef<CSeq_id> CWGSScaffoldIterator::GetGiSeq_id(void) const
{
    return null;
}


CRef<CSeq_id> CWGSScaffoldIterator::GetId(TFlags flags) const
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
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
            return id;
        }
    }
    
    NCBI_THROW_FMT(CSraException, eDataError,
                   "CWGSScaffoldIterator::GetId("<<flags<<"): "
                   "no valid id found: "<<
                   GetDb().m_IdPrefixWithVersion<<"/"<<m_CurrId);
}


void CWGSScaffoldIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw___GetScaffoldIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
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
        GetDb().AddMasterDescr(*ret, nullptr, flags);
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
    CVDBMgr::CRequestContextUpdater ctx_updater;
    x_CheckValid("CWGSScaffoldIterator::GetSeq_inst");

    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = 0;
    inst->SetMol(GetDb().GetScaffoldMolType());
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
        CVDBValueFor<NCBI_WGS_gap_linkage> linkages_val = m_Cur->COMPONENT_LINKAGE(m_CurrId);
        if ( !linkages_val.empty() ) {
            size_t gaps_count = 0;
            for ( size_t i = 0; i < lens.size(); ++i ) {
                NCBI_WGS_component_props props = propss[i];
                if ( props < 0 ) {
                    // gap
                    ++gaps_count;
                }
            }
            if ( linkages_val.size() != gaps_count ) {
                NCBI_THROW(CSraException, eDataError,
                           "CWGSScaffoldIterator: inconsistent gap info");
            }
            linkages = linkages_val.data();
        }
    }
    CDelta_ext::Tdata& delta = inst->SetExt().SetDelta().Set();
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        NCBI_WGS_component_props props = propss[i];
        CRef<CDelta_seq> seg(new CDelta_seq);
        if ( props < 0 ) {
            // gap
            CRef<CSeq_literal> literal =
                sx_MakeGapLiteral(len, props, linkages? *linkages++: 0);
            seg->SetLiteral(*literal);
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
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw__GetScaffoldBioseq);
    _ASSERT(!info.main_seq);
    info.x_SetSeq(*this);
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetContigDescr);
        if ( (info.flags & fMaskDescr) == fSeqDescr  ) {
            // only own descriptors
            /*
            if ( m_Cur->m_DESCR ) {
                CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
                if ( !descr.empty() ) {
                    info.x_AddDescr(*descr);
                }
            }
            */
        }
        else {
            // full descirptor collection
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
            }
        }
    }
    info.main_seq->SetInst(*GetSeq_inst(info.flags));
}


void CWGSScaffoldIterator::x_CreateEntry(SWGSCreateInfo& info) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
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
            sx_AddMasterDescr(m_Db, info, flags);
        }
    }
}


CRef<CBioseq> CWGSScaffoldIterator::GetBioseq(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetBioseq");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSScaffoldIterator::GetSeq_entry(TFlags flags) const
{
    x_CheckValid("CWGSScaffoldIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
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
            GetDb().Put(m_Cur, GI_TO(TIntId, m_CurrGi));
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
    m_Cur = GetDb().GiIdx();
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
    m_FirstBadGi = GI_FROM(TIntId, range.first+range.second);
    m_CurrGi = GI_FROM(TIntId, range.first);
}


bool CWGSGiIterator::x_Excluded(void)
{
    if ( m_FilterSeqType != eProt && m_Cur->m_NUC_ROW_ID ) {
        CVDBValueFor<TVDBRowId> value =
            m_Cur->NUC_ROW_ID(GI_TO(TIntId, m_CurrGi), CVDBValue::eMissing_Allow);
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
            m_Cur->PROT_ROW_ID(GI_TO(TIntId, m_CurrGi), CVDBValue::eMissing_Allow);
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
    if ( m_Cur0 ) {
        if ( m_Db ) {
            GetDb().Put(m_Cur0);
            if ( m_Cur ) {
                GetDb().Put(m_Cur);
            }
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
        m_Cur0 = iter.m_Cur0;
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
    PROFILE(sw_ProtIterator);
    m_CurrId = m_FirstGoodId = m_FirstBadId = 0;
    if ( !wgs_db ) {
        return;
    }
    m_Cur0 = wgs_db.GetNCObject().Prot0();
    if ( !m_Cur0 ) {
        return;
    }
    m_Db = wgs_db;
    TVDBRowIdRange range = m_Cur0->m_Cursor.GetRowIdRange();
    m_FirstGoodId = m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


void CWGSProteinIterator::x_Cur() const
{
    if ( !m_Cur ) {
        const_cast<CWGSProteinIterator*>(this)->m_Cur = GetDb().Prot();
    }
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
    return m_Cur0->m_GI && GetGi() != ZERO_GI;
}


CSeq_id::TGi CWGSProteinIterator::GetGi(void) const
{
    x_CheckValid("CWGSProteinIterator::GetGi");
    if ( m_Cur0->m_GI ) {
        CVDBValueFor<NCBI_gi> gi = m_Cur0->GI(m_CurrId);
        if ( !gi.empty() ) {
            return s_ToGi(*gi, "CWGSProteinIterator::GetGi()");
        }
    }
    return ZERO_GI;
}


CTempString CWGSProteinIterator::GetAccession(void) const
{
    PROFILE(sw____GetProtAcc);
    x_CheckValid("CWGSProteinIterator::GetAccession");
    if ( m_Cur0->m_GB_ACCESSION ) {
        return *CVDBStringValue(m_Cur0->GB_ACCESSION(m_CurrId));
    }
    else {
        return CTempString();
    }
}


int CWGSProteinIterator::GetAccVersion(void) const
{
    PROFILE(sw____GetProtAccVer);
    x_CheckValid("CWGSProteinIterator::GetAccVersion");
    return *m_Cur0->ACC_VERSION(m_CurrId);
}


CRef<CSeq_id> CWGSProteinIterator::GetAccSeq_id(void) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw____GetProtAccSeq_id);
    CRef<CSeq_id> id;
    CTempString acc = GetAccession();
    if ( !acc.empty() ) {
        id = GetDb().GetAccSeq_id(acc, GetAccVersion());
    }
    else {
        id = GetDb().GetProteinSeq_id(m_CurrId);
    }
    return id;
}


CRef<CSeq_id> CWGSProteinIterator::GetGeneralOrPatentSeq_id(void) const
{
    return GetDb().GetGeneralOrPatentSeq_id(GetProteinName(), *m_Cur0, m_CurrId);
}


CRef<CSeq_id> CWGSProteinIterator::GetGeneralSeq_id(void) const
{
    return GetGeneralOrPatentSeq_id();
}


CRef<CSeq_id> CWGSProteinIterator::GetGiSeq_id(void) const
{
    PROFILE(sw____GetProtGISeq_id);
    CRef<CSeq_id> id;
    CSeq_id::TGi gi = GetGi();
    if ( gi != ZERO_GI ) {
        id = new CSeq_id;
        id->SetGi(gi);
    }
    return id;
}


CRef<CSeq_id> CWGSProteinIterator::GetId(TFlags flags) const
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
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
            return id;
        }
    }
    
    NCBI_THROW_FMT(CSraException, eDataError,
                   "CWGSProteinIterator::GetId("<<flags<<"): "
                   "no valid id found: "<<
                   GetDb().m_IdPrefixWithVersion<<"/"<<m_CurrId);
}


void CWGSProteinIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw___GetProtIds);
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( flags & fIds_gnl ) {
        // gnl
        if ( CRef<CSeq_id> id = GetGeneralOrPatentSeq_id() ) {
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
    return *CVDBStringValue(m_Cur0->PROTEIN_NAME(m_CurrId));
}


CTempString CWGSProteinIterator::GetProductName(void) const
{
    x_CheckValid("CWGSProteinIterator::GetProductName");
    x_Cur();
    return *CVDBStringValue(m_Cur->PRODUCT_NAME(m_CurrId));
}


bool CWGSProteinIterator::HasTaxId(void) const
{
    if ( GetDb().HasCommonTaxId() ) {
        return true;
    }
    x_Cur();
    return m_Cur->m_TAXID;
}


TTaxId CWGSProteinIterator::GetTaxId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetTaxId");
    if ( GetDb().HasCommonTaxId() ) {
        return GetDb().GetCommonTaxId();
    }
    x_Cur();
    return s_GetTaxId(m_Cur->TAXID(m_CurrId));
}


bool CWGSProteinIterator::HasSeqHash(void) const
{
    x_CheckValid("CWGSProteinIterator::GetSeqHash");
    x_Cur();
    return m_Cur->m_HASH;
}


CWGSProteinIterator::THash CWGSProteinIterator::GetSeqHash(void) const
{
    return HasSeqHash()? *m_Cur->HASH(m_CurrId): 0;
}


TSeqPos CWGSProteinIterator::GetSeqLength(void) const
{
    x_CheckValid("CWGSProteinIterator::GetSeqLength");
    x_Cur();
    return *m_Cur->PROTEIN_LEN(m_CurrId);
}


bool CWGSProteinIterator::HasRefAcc(void) const
{
    x_CheckValid("CWGSProteinIterator::HasRefAcc");
    x_Cur();
    return m_Cur->m_REF_ACC;
}


CTempString CWGSProteinIterator::GetRefAcc(void) const
{
    x_CheckValid("CWGSProteinIterator::GetRefAcc");
    x_Cur();
    return *CVDBStringValue(m_Cur->REF_ACC(m_CurrId));
}


NCBI_gb_state CWGSProteinIterator::GetGBState(void) const
{
    x_CheckValid("CWGSProteinIterator::GetGBState");
    x_Cur();
    CVDBMgr::CRequestContextUpdater ctx_updater;
    NCBI_gb_state state = 0;
    if ( m_Cur->m_GB_STATE ) {
        state = *m_Cur->GB_STATE(m_CurrId);
    }
    if ( !state ) {
        state = m_Db->GetProjectGBState();
    }
    return state;
}


bool CWGSProteinIterator::HasPublicComment(void) const
{
    x_CheckValid("CWGSProteinIterator::HasPublicComment");
    x_Cur();
    if ( !m_Cur->m_PUBLIC_COMMENT ) {
        return false;
    }
    return !m_Cur->PUBLIC_COMMENT(m_CurrId).empty();
}


CTempString CWGSProteinIterator::GetPublicComment(void) const
{
    x_CheckValid("CWGSProteinIterator::GetPublicComment");
    x_Cur();
    if ( !m_Cur->m_PUBLIC_COMMENT ) {
        return string();
    }
    return *m_Cur->PUBLIC_COMMENT(m_CurrId);
}


bool CWGSProteinIterator::HasTitle(void) const
{
    x_CheckValid("CWGSProteinIterator::HasTitle");
    x_Cur();
    return m_Cur->m_TITLE && !m_Cur->TITLE(m_CurrId).empty();
}


CTempString CWGSProteinIterator::GetTitle(void) const
{
    x_CheckValid("CWGSProteinIterator::GetTitle");
    x_Cur();
    if ( !m_Cur->m_TITLE ) {
        return CTempString();
    }
    return *CVDBStringValue(m_Cur->TITLE(m_CurrId));
}


TVDBRowIdRange CWGSProteinIterator::GetLocFeatRowIdRange(void) const
{
    x_CheckValid("CWGSProteinIterator::GetLocFeatRowIdRange");
    x_Cur();
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


size_t CWGSProteinIterator::GetProductFeatCount(void) const
{
    x_CheckValid("CWGSProteinIterator::GetProductFeatCount");
    x_Cur();
    if ( !m_Cur->m_FEAT_PRODUCT_ROW_ID ) {
        return 0;
    }
    return m_Cur->FEAT_PRODUCT_ROW_ID(m_CurrId).size();
}


TVDBRowId CWGSProteinIterator::GetProductFeatRowId(size_t index) const
{
    x_CheckValid("CWGSProteinIterator::GetProductFeatRowId");
    x_Cur();
    if ( !m_Cur->m_FEAT_PRODUCT_ROW_ID ) {
        return 0;
    }
    return m_Cur->FEAT_PRODUCT_ROW_ID(m_CurrId)[index];
}


TVDBRowId CWGSProteinIterator::GetBestProductFeatRowId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetBestProductFeatRowId");
    x_Cur();
    if ( !m_Cur->m_FEAT_PRODUCT_ROW_ID ) {
        return 0;
    }
    CVDBValueFor<TVDBRowId> row = m_Cur->FEAT_PRODUCT_ROW_ID(m_CurrId);
    return row.empty()? 0: row[row.size()-1];
}


TVDBRowId CWGSProteinIterator::GetProductFeatRowId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetProductFeatRowId");
    x_Cur();
    if ( !m_Cur->m_FEAT_PRODUCT_ROW_ID ) {
        return 0;
    }
    CVDBValueFor<TVDBRowId> row = m_Cur->FEAT_PRODUCT_ROW_ID(m_CurrId);
    return row.empty()? 0: *row;
}


TVDBRowId CWGSProteinIterator::GetReplacedByRowId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetReplacedByRowId");
    x_Cur();
    if ( m_Cur->m_REPLACED_BY ) {
        CVDBValueFor<TVDBRowId> value = m_Cur->REPLACED_BY(m_CurrId);
        if ( !value.empty() ) {
            return *value;
        }
    }
#ifdef TEST_ACC_VERSION
    if ( m_CurrId % 3 != 0 ) {
        return m_CurrId+1;
    }
#endif
    return 0;
}


TVDBRowId CWGSProteinIterator::GetReplacesRowId(void) const
{
    x_CheckValid("CWGSProteinIterator::GetReplacesRowId");
    x_Cur();
    if ( m_Cur->m_REPLACES ) {
        CVDBValueFor<TVDBRowId> value = m_Cur->REPLACES(m_CurrId);
        if ( !value.empty() ) {
            return *value;
        }
    }
#ifdef TEST_ACC_VERSION
    if ( m_CurrId % 3 != 1 ) {
        return m_CurrId-1;
    }
#endif
    return 0;
}


bool CWGSProteinIterator::HasSeq_descr(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::HasSeq_descr");
    x_Cur();
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
    x_Cur();
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
        GetDb().AddMasterDescr(*ret, nullptr, flags);
    }
    if ( ret->Get().empty() ) {
        ret.Reset();
    }
    return ret;
}


bool CWGSProteinIterator::HasAnnotSet(void) const
{
    x_CheckValid("CWGSProteinIterator::HasAnnotSet");
    x_Cur();
    return m_Cur->m_ANNOT && !m_Cur->ANNOT(m_CurrId).empty();
}


void CWGSProteinIterator::GetAnnotSet(TAnnotSet& annot_set, TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetAnnotSet");
    x_Cur();
    if ( (flags & fSeqAnnot) && m_Cur->m_ANNOT ) {
        sx_AddAnnotBytes(annot_set, *m_Cur->ANNOT(m_CurrId));
    }
}


CRef<CSeq_inst> CWGSProteinIterator::GetSeq_inst(TFlags flags) const
{
    PROFILE(sw___GetProtInst);
    x_CheckValid("CWGSProteinIterator::GetSeq_inst");
    x_Cur();
    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = GetSeqLength();
    inst->SetMol(GetDb().GetProteinMolType());
    inst->SetLength(length);
    inst->SetRepr(CSeq_inst::eRepr_raw);
    inst->SetSeq_data().SetNcbieaa().Set() = *m_Cur->PROTEIN(m_CurrId);
    if ( 1 ) {
        // add history info
        TVDBRowId replaced_by_row = GetReplacedByRowId();
        TVDBRowId replaces_row = GetReplacesRowId();
        if ( replaced_by_row || replaces_row ) {
            CSeq_hist& hist = inst->SetHist();
            if ( replaced_by_row ) {
                CWGSProteinIterator it(m_Db, replaced_by_row);
                hist.SetReplaced_by().SetIds().push_back(it.GetId());
            }
            if ( replaces_row ) {
                CWGSProteinIterator it(m_Db, replaces_row);
                hist.SetReplaces().SetIds().push_back(it.GetId());
            }
        }
    }
    return inst;
}


void CWGSProteinIterator::x_CreateBioseq(SWGSCreateInfo& info) const
{
    CVDBMgr::CRequestContextUpdater ctx_updater;
    PROFILE(sw__GetProtBioseq);
    _ASSERT(!info.main_seq);
    x_Cur();
    info.x_SetSeq(*this);
    if ( info.entry ) {
        _ASSERT(info.entry->Which() == CSeq_entry::e_not_set);
        info.entry->SetSeq(*info.main_seq);
    }
    GetIds(info.main_seq->SetId(), info.flags);
    if ( info.flags & fMaskDescr ) {
        PROFILE(sw___GetProtDescr);
        if ( (info.flags & fMaskDescr) == fSeqDescr  ) {
            // only own descriptors
            if ( m_Cur->m_DESCR ) {
                CVDBStringValue descr = m_Cur->DESCR(m_CurrId);
                if ( !descr.empty() ) {
                    info.x_AddDescr(*descr);
                }
            }
        }
        else {
            // full descirptor collection
            if ( CRef<CSeq_descr> descr = GetSeq_descr(info.flags) ) {
                info.main_seq->SetDescr(*descr);
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
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
    x_CreateBioseq(info);
    return info.main_seq;
}


CRef<CSeq_entry> CWGSProteinIterator::GetSeq_entry(TFlags flags) const
{
    x_CheckValid("CWGSProteinIterator::GetSeq_entry");
    SWGSCreateInfo info(m_Db, eFromFlags, flags);
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
    PROFILE(sw_FeatIterator);
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
    if ( !m_Cur->m_PRODUCT_ROW_ID ) {
        return 0;
    }
    CVDBValueFor<TVDBRowId> row = m_Cur->PRODUCT_ROW_ID(m_CurrId);
    return row.empty()? 0: *row;
}


NCBI_WGS_feattype CWGSFeatureIterator::GetFeatType(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetFeatType");
    return *m_Cur->FEAT_TYPE(m_CurrId);
}


TSeqPos CWGSFeatureIterator::GetLocStart(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetLocStart");
    return *m_Cur->LOC_START(m_CurrId);
}


TSeqPos CWGSFeatureIterator::GetLocLength(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetLocLength");
    return *m_Cur->LOC_LEN(m_CurrId);
}


CRange<TSeqPos> CWGSFeatureIterator::GetLocRange(void) const
{
    x_CheckValid("CWGSFeatureIterator::GetLocRange");
    CRange<TSeqPos> range;
    range.SetFrom(*m_Cur->LOC_START(m_CurrId));
    range.SetLength(*m_Cur->LOC_LEN(m_CurrId));
    return range;
}


CTempString CWGSFeatureIterator::GetSeq_featBytes(void) const
{
    PROFILE(sw_GetFeatBytes);
    return *m_Cur->SEQ_FEAT(m_CurrId);
}


CRef<CSeq_feat> CWGSFeatureIterator::GetSeq_feat(void) const
{
    PROFILE(sw_Feat);
    CRef<CSeq_feat> feat(new CSeq_feat);
    CTempString bytes = GetSeq_featBytes();
    m_Cur.GetNCObject().m_ObjStr.OpenFromBuffer(bytes.data(), bytes.size());
    m_Cur.GetNCObject().m_ObjStr >> *feat;
    return feat;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
