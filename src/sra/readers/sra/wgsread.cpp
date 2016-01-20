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

//#define COLLECT_PROFILE

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSReader
NCBI_DEFINE_ERR_SUBCODE_X(19);

BEGIN_NAMESPACE(objects);


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


static const char kSeq_descrFirstByte = 49; // first byte of Seq-descr ASN.1

// split parameters
static bool kEnableSplitQual = true;
static bool kEnableSplitData = true;
static bool kEnableSplitProd = false;

static int kMainEntryId = 1;
enum EChunkType {
    eChunk_prod,
    eChunk_data,
    eChunk_feat,
    eChunk_qual,
    kChunkIdStep = 4
};
static const size_t kProdPerChunk = 64;
static const TSeqPos kDataChunkSize = 256<<10; // 64KiB in 2na encoding
static const TSeqPos kQualChunkSize = 64<<10; // 64KiB


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
static SProfiler sw____GetContigQualSize;
static SProfiler sw____GetContigQualData;
static SProfiler sw____GetContigQualMinMax;
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
    DECLARE_VDB_COLUMN_AS(INSDC_4na_bin, READ); // unpacked 4na
    CVDBColumnBits<2> m_READ_2na;

    mutable TVDBRowId m_4naCacheRow;
    mutable COpenRange<TSeqPos> m_4naCacheRange;
    mutable CSimpleBufferT<Uint1> m_4naCache;
    const Uint1* GetUnpacked4na(TVDBRowId row, TSeqPos pos, TSeqPos len) const;
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
      INIT_VDB_COLUMN_AS(READ, INSDC:4na:bin), // unpacked 4na
      m_READ_2na(m_Cursor, "(INSDC:2na:packed)READ",
                 NULL, CVDBColumn::eMissing_Allow) // packed 2na
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
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_SEQ_TYPE),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_ACCESSION),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_ROW_ID),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_START),
      INIT_OPTIONAL_VDB_COLUMN(PRODUCT_LEN),
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
        NStr::Tokenize(vol_path, ":", dirs);
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


const Uint1*
CWGSDb_Impl::SSeqTableCursor::GetUnpacked4na(TVDBRowId row,
                                             TSeqPos pos, TSeqPos len) const
{
    COpenRange<TSeqPos> range;
    range.SetFrom(pos);
    range.SetLength(len);
    if ( row == m_4naCacheRow && m_4naCacheRange.IntersectingWith(range) ) {
        // reuse previously read data
        if ( m_4naCacheRange == range ) {
            return m_4naCache.data();
        }
    }
    {
        PROFILE(sw____GetRaw4na);
        m_4naCacheRow = row;
        m_4naCacheRange = range;
        m_4naCache.resize(len);
        m_Cursor.ReadElements(row, m_READ, 8, pos, len, m_4naCache.data());
    }
    return m_4naCache.data();
}


struct SWGSCreateInfo 
{
    typedef int TFlags;

    SWGSCreateInfo(const CWGSDb& db, TFlags flags)
        : db(db),
          flags(flags),
          split_prod(false),
          split_data(false),
          split_feat(false),
          split_qual(false)
        {
        }

    CWGSDb db;
    TFlags flags;
    bool split_prod, split_data, split_feat, split_qual;
    CRef<CSeq_id> main_id;
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
    if ( !m_Cur->m_GI ) {
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


TSeqPos CWGSSeqIterator::GetSeqOffset(EClipType clip_type) const
{
    return GetClipByQualityFlag(clip_type)? GetClipQualityLeft(): 0;
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
    SWGSCreateInfo info(m_Db, flags);
    info.main_id = GetId(flags);
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
        -NCBI_WGS_gap_repeat;
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


static const bool kRecoverGaps = true;


static inline
bool sx_Is2na(Uint1 b)
{
    return b && !(b&(b-1));
}


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


inline
const Uint1* CWGSSeqIterator::x_GetUnpacked4na(TSeqPos pos, TSeqPos len) const
{
    return m_Cur->GetUnpacked4na(m_CurrId, pos, len);
}


TSeqPos CWGSSeqIterator::x_Get2naLength(TSeqPos pos, TSeqPos len) const
{
    PROFILE(sw____Get2naLen);
    const Uint1* ptr = x_GetUnpacked4na(pos, len);
    TSeqPos rem_len = len;
    for ( ; rem_len; --rem_len, ++ptr ) {
        if ( !sx_Is2na(*ptr) ) {
            return len-rem_len;
        }
    }
    return len;
}


TSeqPos CWGSSeqIterator::x_Get4naLength(TSeqPos pos, TSeqPos len,
                                        TSeqPos stop_2na_len,
                                        TSeqPos stop_gap_len) const
{
    PROFILE(sw____Get4naLen);
    if ( len < stop_2na_len ) {
        return len;
    }
    const Uint1* ptr = x_GetUnpacked4na(pos, len);
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


TSeqPos CWGSSeqIterator::x_GetGapLength(TSeqPos pos, TSeqPos len) const
{
    PROFILE(sw____GetGapLen);
    const Uint1* ptr = x_GetUnpacked4na(pos, len);
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
        size_t bytes = (len+3)/4;
        // allocate 8-byte aligned memory to allow multi-byte operations at end
        data.reserve((bytes+7)/8*8);
        data.resize(bytes);
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
    const Uint1* ptr = x_GetUnpacked4na(pos, len);
    for ( ; len >= 4; len -= 4, ptr += 4 ) {
        Uint1 v0 = ptr[0];
        Uint1 v1 = ptr[1];
        Uint1 v2 = ptr[2];
        Uint1 v3 = ptr[3];
        Uint1 b =
            (sx_To2na(v0)<<6) + 
            (sx_To2na(v1)<<4) + 
            (sx_To2na(v2)<<2) + 
            (sx_To2na(v3));
        data.push_back(b);
    }
    if ( len > 0 ) {
        // trailing odd bases 1..3
        Uint1 b = sx_To2na(ptr[0])<<6;
        if ( len > 1 ) {
            b |= sx_To2na(ptr[1])<<4;
            if ( len > 2 ) {
                b |= sx_To2na(ptr[2])<<2;
            }
        }
        data.push_back(b);
    }
    return ret;
}


// return 4na Seq-data for specified range
CRef<CSeq_data> CWGSSeqIterator::Get4na(TSeqPos pos, TSeqPos len) const
{
    PROFILE(sw____Get4na);
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi4na().Set();
    size_t dst_len = (len+1)/2;
    const Uint1* ptr = x_GetUnpacked4na(pos, len);
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
void CWGSSeqIterator::x_GetSegments(TSegments& segments,
                                    COpenRange<TSeqPos> range,
                                    TWGSContigGapInfo gap_info,
                                    TInstSegmentFlags flags) const
{
    range = x_NormalizeSeqRange(range);
    TSeqPos raw_offset = GetSeqOffset();
    TSeqPos pos = range.GetFrom() + raw_offset;
    TSeqPos len = range.GetLength();

    // skip gaps starting befor requested range
    while ( gap_info && gap_info.GetFrom() < pos ) {
        TSeqPos gap_end = gap_info.GetToOpen();
        if ( gap_end > pos ) {
            TSeqPos skip = gap_end - pos;
            if ( flags & fInst_MakeGaps ) {
                x_AddGap(segments, pos - raw_offset, skip, gap_info);
            }
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

    for ( ; len > 0; ) {
        TSeqPos gap_start = kInvalidSeqPos;
        TSeqPos rem_len = len;
        if ( gap_info ) {
            // add gap
            gap_start = *gap_info.gaps_start;
            if ( pos == gap_start ) {
                TSeqPos gap_len = *gap_info.gaps_len;
                if ( gap_len > len ) {
                    if ( flags & fInst_MakeGaps) {
                        x_AddGap(segments, pos - raw_offset, len, gap_info);
                    }
                    // no more sequence
                    break;
                }
                if ( flags & fInst_MakeGaps ) {
                    x_AddGap(segments, pos - raw_offset, gap_len, gap_info);
                }
                ++gap_info;
                len -= gap_len;
                pos += gap_len;
                continue;
            }
            rem_len = min(rem_len, gap_start-pos);
        }

        if ( flags & fInst_SplitInfo ) {
            // prepare empty literals for split Seq-data
            TSeqPos chunk_start =
                (pos-raw_offset)/kDataChunkSize*kDataChunkSize;
            TSeqPos chunk_end = chunk_start + kDataChunkSize;
            TSeqPos seg_len = min(rem_len, chunk_end - pos);
            SSegment seg;
            seg.range.SetFrom(pos - raw_offset);
            seg.range.SetLength(seg_len);
            seg.is_gap = false;
            segments.push_back(seg);
            len -= seg_len;
            pos += seg_len;
            continue;
        }

        if ( (flags & fInst_MakeData) && (flags & fInst_WholeData) ) {
            // add fixed number of Seq-data for chunks
            SSegment seg;
            seg.range.SetFrom(pos - raw_offset);
            seg.is_gap = false;
            seg.literal = new CSeq_literal;
            TSeqPos seg_len = x_Get2naLength(pos, rem_len);
            if ( seg_len == rem_len ) {
                // 2na
                seg.literal->SetSeq_data(*Get2na(pos, seg_len));

                _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                seg_len = rem_len;
                // 4na
                seg.literal->SetSeq_data(*Get4na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
            seg.range.SetLength(seg_len);
            seg.literal->SetLength(seg_len);
            segments.push_back(seg);
            pos += seg_len;
            len -= seg_len;
            continue;
        }

        if ( flags & fInst_MakeData ) {
            // add optimal sequence piece for regular delta
            SSegment seg;
            seg.range.SetFrom(pos - raw_offset);
            seg.is_gap = false;
            seg.literal = new CSeq_literal;
            TSeqPos seg_len = x_Get2naLength(pos, min(rem_len, kSplit2naSize));
            if ( seg_len >= kMin2naSize || seg_len == len ) {
                if ( seg_len > kSplit2naSize ) {
                    seg_len = kChunk2naSize;
                }
                // 2na
                seg.literal->SetSeq_data(*Get2na(pos, seg_len));

                _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                TSeqPos seg_len_2na = seg_len;
                seg_len += x_Get4naLength(pos+seg_len,
                                          min(rem_len, kSplit4naSize)-seg_len,
                                          kMin2naSize, kInvalidSeqPos);
                if ( seg_len == seg_len_2na ) {
                    // 2na
                    seg.literal->SetSeq_data(*Get2na(pos, seg_len));
                    _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
                }
                else {
                    if ( seg_len >= kSplit4naSize ) {
                        seg_len = kChunk4naSize;
                    }
                    // 4na
                    seg.literal->SetSeq_data(*Get4na(pos, seg_len));
                    _ASSERT(seg.literal->GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
                }
            }
            seg.range.SetLength(seg_len);
            seg.literal->SetLength(seg_len);
            segments.push_back(seg);
            pos += seg_len;
            len -= seg_len;
            continue;
        }
    }
}


// add raw data as delta segments with gap recovering
void CWGSSeqIterator::x_GetSegments(TSegments& segments,
                                    COpenRange<TSeqPos> range) const
{
    range = x_NormalizeSeqRange(range);
    TSeqPos raw_offset = GetSeqOffset();
    TSeqPos pos = range.GetFrom() + raw_offset;
    TSeqPos len = range.GetLength();

    // max size of gap segment
    const TSeqPos kMinGapSize = 20;
    const TSeqPos kMaxGapSize = 200;
    // size of gap segment if its actual size is unknown
    const TSeqPos kUnknownGapSize = 100;
    
    for ( ; len > 0; ) {
        SSegment seg;
        seg.range.SetFrom(pos - raw_offset);
        seg.is_gap = false;
        seg.literal = new CSeq_literal;

        TSeqPos rem_len = len;
        TSeqPos seg_len = x_Get2naLength(pos, min(rem_len, kSplit2naSize));
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len > kSplit2naSize ) {
                seg_len = kChunk2naSize;
            }
            seg.literal->SetSeq_data(*Get2na(pos, seg_len));
            _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos seg_len_2na = seg_len;
            seg_len += x_Get4naLength(pos+seg_len,
                                      min(rem_len, kSplit4naSize)-seg_len,
                                      kMin2naSize, kMinGapSize);
            if ( kRecoverGaps && seg_len == 0 ) {
                seg_len = x_GetGapLength(pos, min(rem_len, kMaxGapSize));
                _ASSERT(seg_len > 0);
                if ( seg_len == kUnknownGapSize ) {
                    seg.is_gap = true;
                    seg.literal->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                }
            }
            else if ( seg_len == seg_len_2na ) {
                seg.literal->SetSeq_data(*Get2na(pos, seg_len));
                _ASSERT(seg.literal->GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                if ( seg_len >= kSplit4naSize ) {
                    seg_len = kChunk4naSize;
                }
                seg.literal->SetSeq_data(*Get4na(pos, seg_len));
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
    COpenRange<TSeqPos> whole(0, length);
    if ( (info.flags & fMaskInst) == fInst_ncbi4na ) {
        inst->SetRepr(CSeq_inst::eRepr_raw);
        inst->SetSeq_data(*Get4na(GetSeqOffset(), length));
    }
    else if ( HasGapInfo() ) {
        inst->SetStrand(CSeq_inst::eStrand_ds);
        CRef<CSeq_id> id = GetAccSeq_id();
        TWGSContigGapInfo gap_info;
        GetGapInfo(gap_info);
        if ( !info.split_data ) {
            TSegments segments;
            x_GetSegments(segments, whole, gap_info, fInst_MakeDelta);
            x_SetDeltaOrData(*inst, segments);
        }
        else {
            // split
            TSegments segments;
            x_GetSegments(segments, whole, gap_info, fInst_MakeSplit);
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
        inst->SetStrand(CSeq_inst::eStrand_ds);
        TSegments segments;
        x_GetSegments(segments, whole);
        x_SetDeltaOrData(*inst, segments);
    }
    return inst;
}


CRef<CSeq_inst> CWGSSeqIterator::GetSeq_inst(TFlags flags) const
{
    SWGSCreateInfo info(m_Db, flags);
    return x_GetSeq_inst(info);
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
    PROFILE(sw__GetContigBioseq);
    _ASSERT(!info.main_seq);
    info.main_seq = new CBioseq();
    info.main_id = GetId(info.flags);
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
    CRef<CSeq_id> save_id = main_id;
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
        main_seq = null;
        main_id = null;
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
    main_id = save_id;
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
                CRef<CSeq_id> prot_id = prot_it.GetId(flags);
                sx_AddSplitId(*ids, *prot_id);
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
        info.flags = flags & ~fMasterDescr;
        x_CreateBioseq(info);
        info.flags = flags;
        info.x_CreateProtSet(GetLocFeatRowIdRange());
        if ( flags & fMasterDescr ) {
            sx_AddMasterDescr(m_Db, *info.entry);
        }
    }
}


bool CWGSSeqIterator::x_InitSplit(SWGSCreateInfo& info) const
{
    // split data if...
    if ( kEnableSplitData &&
         ((info.flags & fMaskInst) == fInst_delta) && // delta is requested
         HasGapInfo() && // we have explicit gap info
         GetSeqLength() > kDataChunkSize // data is big enough
        ) {
        info.split_data = true;
    }
    if ( kEnableSplitProd &&
         sx_HasMoreProducts(m_Db, GetLocFeatRowIdRange(), kProdPerChunk) ) {
        // split products if there are many enough
        info.split_prod = true;
    }
    if ( kEnableSplitQual &&
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


void CWGSSeqIterator::x_CreateChunk(SWGSCreateInfo& info,
                                    TChunkId chunk_id) const
{
    PROFILE(sw_GetChunk);
    info.main_id = GetId(info.flags);
    EChunkType type = EChunkType(chunk_id%kChunkIdStep);
    size_t index = chunk_id/kChunkIdStep;
    if ( type == eChunk_qual ) {
        CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
        sx_SetSplitId(data->SetId(), *info.main_id);
        x_GetQualityAnnot(data->SetAnnots(), info,
                          index*kQualChunkSize, kQualChunkSize);
        info.chunk->SetData().push_back(data);
    }
    else if ( type == eChunk_prod ) {
        vector<TVDBRowId> product_row_ids;
        sx_GetProductsSlice(m_Db, GetLocFeatRowIdRange(),
                            index*kProdPerChunk, kProdPerChunk,
                            product_row_ids);
        info.x_AddProducts(product_row_ids);
    }
    else if ( type == eChunk_data ) {
        CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
        sx_SetSplitId(data->SetId(), *info.main_id);
        COpenRange<TSeqPos> range;
        range.SetFrom(index*kDataChunkSize);
        range.SetLength(kDataChunkSize);

        TWGSContigGapInfo gap_info;
        GetGapInfo(gap_info);
        TSegments segments;
        x_GetSegments(segments, range, gap_info, fInst_MakeChunk);
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
    else {
        NCBI_THROW_FMT(CSraException, eInvalidArg,
                       "CWGSSeqIterator::CreateChunk("<<chunk_id<<"): "
                       "unsupported chunk type: "<<type);
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
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
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
    PROFILE(sw__GetScaffoldBioseq);
    _ASSERT(!info.main_seq);
    info.main_seq = new CBioseq();
    info.main_id = GetId(info.flags);
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
        if ( CRef<CSeq_id> id = GetGeneralSeq_id() ) {
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
    info.main_id = GetId(info.flags);
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
    if ( !m_Cur->m_PRODUCT_ROW_ID ) {
        return 0;
    }
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
