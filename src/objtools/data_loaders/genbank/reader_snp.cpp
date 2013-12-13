/*  $Id$
 * ===========================================================================
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
 *  Author:  Anton Butanaev, Eugene Vasilchenko
 *
 *  File Description: Data reader from ID1
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/processor.hpp> //for hooks

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <corelib/reader_writer.hpp>

#include <algorithm>
#include <numeric>

// for debugging
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// utility function

NCBI_PARAM_DECL(bool, GENBANK, SNP_TABLE_STAT);
NCBI_PARAM_DEF_EX(bool, GENBANK, SNP_TABLE_STAT, false,
                  eParam_NoThread, GENBANK_SNP_TABLE_STAT);

static bool CollectSNPStat(void)
{
    static const bool s_Value =
        NCBI_PARAM_TYPE(GENBANK, SNP_TABLE_STAT)::GetDefault();
    return s_Value;
}


/////////////////////////////////////////////////////////////////////////////
// hook classes

namespace {

class CSeq_annot_hook : public CReadObjectHook
{
public:
    void ReadObject(CObjectIStream& in,
                    const CObjectInfo& object)
        {
            m_Seq_annot = CType<CSeq_annot>::Get(object);
            DefaultRead(in, object);
            m_Seq_annot = null;
        }
    
    CRef<CSeq_annot>    m_Seq_annot;
};


class CSNP_Ftable_hook : public CReadChoiceVariantHook
{
public:
    CSNP_Ftable_hook(CTSE_SetObjectInfo& set_info)
        : m_SetObjectInfo(&set_info),
          m_Seq_annot_hook(new CSeq_annot_hook)
        {
        }

    void ReadChoiceVariant(CObjectIStream& in,
                           const CObjectInfoCV& variant);

    CRef<CTSE_SetObjectInfo>          m_SetObjectInfo;
    CRef<CSeq_annot_hook>           m_Seq_annot_hook;
};


class CSNP_Seq_feat_hook : public CReadContainerElementHook
{
public:
    CSNP_Seq_feat_hook(CSeq_annot_SNP_Info& annot_snp_info,
                       CSeq_annot::TData::TFtable& ftable);
    ~CSNP_Seq_feat_hook(void);

    void ReadContainerElement(CObjectIStream& in,
                              const CObjectInfo& ftable);

private:
    CSeq_annot_SNP_Info&        m_Seq_annot_SNP_Info;
    CSeq_annot::TData::TFtable& m_Ftable;
    CRef<CSeq_feat>             m_Feat;
    size_t                      m_Count[SSNP_Info::eSNP_Type_last];
};


void CSNP_Ftable_hook::ReadChoiceVariant(CObjectIStream& in,
                                         const CObjectInfoCV& variant)
{
    _ASSERT(m_Seq_annot_hook->m_Seq_annot);
    CObjectInfo data_info = variant.GetChoiceObject();
    CObjectInfo ftable_info = *variant;
    CSeq_annot::TData& data = *CType<CSeq_annot::TData>::Get(data_info);

    CRef<CSeq_annot_SNP_Info> snp_info
        (new CSeq_annot_SNP_Info(*m_Seq_annot_hook->m_Seq_annot));
    {{
        CSNP_Seq_feat_hook hook(*snp_info, data.SetFtable());
        ftable_info.ReadContainer(in, hook);
    }}
    snp_info->x_FinishParsing();
    if ( !snp_info->empty() ) {
        m_SetObjectInfo->m_Seq_annot_InfoMap[m_Seq_annot_hook->m_Seq_annot].m_SNP_annot_Info = snp_info;
    }
}


CSNP_Seq_feat_hook::CSNP_Seq_feat_hook(CSeq_annot_SNP_Info& annot_snp_info,
                                       CSeq_annot::TData::TFtable& ftable)
    : m_Seq_annot_SNP_Info(annot_snp_info),
      m_Ftable(ftable)
{
    fill(m_Count, m_Count+SSNP_Info::eSNP_Type_last, 0);
}


static size_t s_TotalCount[SSNP_Info::eSNP_Type_last] = { 0 };


CSNP_Seq_feat_hook::~CSNP_Seq_feat_hook(void)
{
    if ( CollectSNPStat() ) {
        size_t total =
            accumulate(m_Count, m_Count+SSNP_Info::eSNP_Type_last, 0);
        NcbiCout << "CSeq_annot_SNP_Info statistic (gi = " <<
            m_Seq_annot_SNP_Info.GetGi() << "):\n";
        for ( size_t i = 0; i < SSNP_Info::eSNP_Type_last; ++i ) {
            NcbiCout <<
                setw(40) << SSNP_Info::s_SNP_Type_Label[i] << ": " <<
                setw(6) << m_Count[i] << "  " <<
                setw(3) << int(m_Count[i]*100.0/total+.5) << "%\n";
            s_TotalCount[i] += m_Count[i];
        }
        NcbiCout << NcbiEndl;

        total =
            accumulate(s_TotalCount, s_TotalCount+SSNP_Info::eSNP_Type_last,0);
        NcbiCout << "cumulative CSeq_annot_SNP_Info statistic:\n";
        for ( size_t i = 0; i < SSNP_Info::eSNP_Type_last; ++i ) {
            NcbiCout <<
                setw(40) << SSNP_Info::s_SNP_Type_Label[i] << ": " <<
                setw(6) << s_TotalCount[i] << "  " <<
                setw(3) << int(s_TotalCount[i]*100.0/total+.5) << "%\n";
        }
        NcbiCout << NcbiEndl;
    }
}


#ifdef _DEBUG
NCBI_PARAM_DECL(bool, GENBANK, SNP_TABLE_DUMP);
NCBI_PARAM_DEF_EX(bool, GENBANK, SNP_TABLE_DUMP, 0,
                  eParam_NoThread, GENBANK_SNP_TABLE_DUMP);
#endif

void CSNP_Seq_feat_hook::ReadContainerElement(CObjectIStream& in,
                                              const CObjectInfo& /*ftable*/)
{
    if ( !m_Feat ) {
        m_Feat.Reset(new CSeq_feat);
    }
    in.ReadObject(&*m_Feat, m_Feat->GetTypeInfo());
    SSNP_Info snp_info;
    SSNP_Info::ESNP_Type snp_type =
        snp_info.ParseSeq_feat(*m_Feat, m_Seq_annot_SNP_Info);
    ++m_Count[snp_type];
    if ( snp_type == SSNP_Info::eSNP_Simple ) {
        m_Seq_annot_SNP_Info.x_AddSNP(snp_info);
    }
    else {
#ifdef _DEBUG
        static const bool s_Value =
            NCBI_PARAM_TYPE(GENBANK, SNP_TABLE_DUMP)::GetDefault();
        if ( s_Value ) {
            NcbiCerr <<
                "CSNP_Seq_feat_hook::ReadContainerElement: complex SNP: " <<
                SSNP_Info::s_SNP_Type_Label[snp_type] << ":\n" << 
                MSerial_AsnText << *m_Feat;
        }
#endif
        m_Ftable.push_back(m_Feat);
        m_Feat.Reset();
    }
}


} // anonymous namespace


void CSeq_annot_SNP_Info_Reader::Parse(CObjectIStream& in,
                                       const CObjectInfo& object,
                                       CTSE_SetObjectInfo& set_info)
{
    CProcessor::SetSNPReadHooks(in);
    
    if ( CProcessor::TrySNPTable() ) { // set SNP hook
        CRef<CSNP_Ftable_hook> hook(new CSNP_Ftable_hook(set_info));
        CObjectHookGuard<CSeq_annot> guard(*hook->m_Seq_annot_hook, &in);
        CObjectHookGuard<CSeq_annot::TData> guard2("ftable", *hook, &in);
        in.Read(object);
    }
    else {
        in.Read(object);
    }
}


CRef<CSeq_annot_SNP_Info>
CSeq_annot_SNP_Info_Reader::ParseAnnot(CObjectIStream& in)
{
    CRef<CSeq_annot_SNP_Info> ret;

    CRef<CSeq_annot> annot(new CSeq_annot);
    CTSE_SetObjectInfo set_info;
    Parse(in, Begin(*annot), set_info);
    if ( !set_info.m_Seq_annot_InfoMap.empty() ) {
        _ASSERT(set_info.m_Seq_annot_InfoMap.size() == 1);
        _ASSERT(set_info.m_Seq_annot_InfoMap.begin()->first == annot);
        ret = set_info.m_Seq_annot_InfoMap.begin()->second.m_SNP_annot_Info;
    }
    else {
        ret = new CSeq_annot_SNP_Info(*annot);
    }

    return ret;
}


void CSeq_annot_SNP_Info_Reader::Parse(CObjectIStream& in,
                                       CSeq_entry& tse,
                                       CTSE_SetObjectInfo& set_info)
{
    Parse(in, Begin(tse), set_info);
}


/////////////////////////////////////////////////////////////////////////////
// reading and storing in binary format

static void write_unsigned(CNcbiOstream& stream,
                           size_t n,
                           const char* name)
{
    if ( sizeof(n) > 4 && Uint4(n) != n ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "write_unsigned overflow for "<<name<<": "<<n);
    }
    char c[4] = {
        char(n >> 24),
        char(n >> 16),
        char(n >>  8),
        char(n      )
    };
    stream.write(c, sizeof(c));
}


static unsigned read_unsigned(CNcbiIstream& stream,
                              const char* name)
{
    char c[4];
    stream.read(c, sizeof(c));
    if ( !stream ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   string("Cannot read ")+name);
    }
    unsigned n =
        (Uint1(c[0])<<24) | 
        (Uint1(c[1])<<16) |
        (Uint1(c[2])<< 8) |
        (Uint1(c[3])    );
    return n;
}


static void write_size(CNcbiOstream& stream, size_t size)
{
    // use ASN.1 binary like format
    while ( size >= (1<<7) ) {
        stream.put(char(size | (1<<7)));
        size >>= 7;
    }
    stream.put(char(size));
}


static size_t read_size(CNcbiIstream& stream, const char* name)
{
    size_t size = 0;
    static const int total_bits = sizeof(size)*8;
    int shift = 0;
    Uint1 c;
    do {
        c = stream.get();
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       string("Cannot read ")+name);
        }
        size_t bits = c & ((1<<7)-1);
        if ( shift+7 > total_bits && (c>>(total_bits-shift)) != 0 ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       string("read_size overflow for ")+name);
        }
        size |= bits << shift;
        shift += 7;
    } while ( c & (1<<7) );
    return size;
}


void StoreIndexedStringsTo(CNcbiOstream& stream,
                           const CIndexedStrings& strings)
{
    write_size(stream, strings.GetSize());
    for (size_t idx = 0; idx < strings.GetSize(); ++idx) {
        size_t size = strings.GetString(idx).size();
        write_size(stream, size);
        stream.write(strings.GetString(idx).data(), size);
    }
}


void LoadIndexedStringsFrom(CNcbiIstream& stream,
                            CIndexedStrings& strings,
                            size_t max_index,
                            size_t max_length)
{
    strings.Clear();
    size_t count = read_size(stream, "SNP table strings count");
    if ( count > unsigned(max_index+1) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "SNP table string count is too big");
    }
    strings.Resize(count);
    AutoPtr<char, ArrayDeleter<char> > buf(new char[max_length]);
    for (size_t idx = 0; idx < strings.GetSize(); ++idx) {
        size_t size = read_size(stream, "SNP table string size");
        if ( size > max_length ) {
            strings.Clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "SNP table string is too long");
        }
        stream.read(buf.get(), size);
        if ( !stream ) {
            strings.Clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Cannot read SNP table string");
        }
        strings.SetString(idx).assign(buf.get(), buf.get() + size);
    }
}


void StoreIndexedOctetStringsTo(CNcbiOstream& stream,
                                const CIndexedOctetStrings& strings)
{
    size_t element_size = strings.GetElementSize();
    write_size(stream, element_size);
    if ( element_size ) {
        size_t total_size = strings.GetTotalSize();
        write_size(stream, total_size);
        stream.write(&strings.GetTotalString()[0], total_size);
    }
}


void LoadIndexedOctetStringsFrom(CNcbiIstream& stream,
                                 CIndexedOctetStrings& strings,
                                 size_t max_index,
                                 size_t max_length)
{
    strings.Clear();
    size_t element_size =
        read_size(stream, "SNP table OCTET STRING element size");
    if ( element_size ) {
        size_t total_size =
            read_size(stream, "SNP table OCTET STRING total size");
        if ( element_size == 0 || total_size%element_size != 0 ||
             total_size > element_size*(max_index+1) ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "SNP table OCTET STRING count is too big");
        }
        CIndexedOctetStrings::TOctetString s;
        s.resize(total_size);
        stream.read(&s[0], total_size);
        if ( !stream ) {
            strings.Clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Cannot read SNP table OCTET STRING");
        }
        strings.SetTotalString(element_size, s);
    }
}


namespace {

class CSeq_annot_WriteHook : public CWriteObjectHook
{
public:
    typedef CSeq_annot_SNP_Info_Reader::TAnnotToIndex TIndex;

    void WriteObject(CObjectOStream& stream,
                     const CConstObjectInfo& object)
        {
            const CSeq_annot* ptr = CType<CSeq_annot>::Get(object);
            m_Index.insert(TIndex::value_type(ConstRef(ptr), m_Index.size()));
            DefaultWrite(stream, object);
        }
            
    TIndex m_Index;
};


class CSeq_annot_ReadHook : public CReadObjectHook
{
public:
    typedef CSeq_annot_SNP_Info_Reader::TIndexToAnnot TIndex;

    void ReadObject(CObjectIStream& stream,
                    const CObjectInfo& object)
        {
            CSeq_annot* ptr = CType<CSeq_annot>::Get(object);
            m_Index.push_back(Ref(ptr));
            DefaultRead(stream, object);
        }
            
    TIndex m_Index;
};

}


static const unsigned MAGIC = 0x12340007;

void CSeq_annot_SNP_Info_Reader::Write(CNcbiOstream& stream,
                                       const CConstObjectInfo& object,
                                       const CTSE_SetObjectInfo& set_info)
{
    write_unsigned(stream, MAGIC, "SNP table magic number");

    CRef<CSeq_annot_WriteHook> hook(new CSeq_annot_WriteHook);
    {{
        CObjectOStreamAsnBinary obj_stream(stream);
        obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
        CObjectHookGuard<CSeq_annot> guard(*hook, &obj_stream);
        obj_stream.Write(object);
    }}

    write_unsigned(stream, set_info.m_Seq_annot_InfoMap.size(),
                   "number of SNP table annots");
    ITERATE ( CTSE_SetObjectInfo::TSeq_annot_InfoMap, it,
              set_info.m_Seq_annot_InfoMap ) {
        TAnnotToIndex::const_iterator iter = hook->m_Index.find(it->first);
        if ( iter == hook->m_Index.end() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Orphan CSeq_annot_SNP_Info");
        }
        write_unsigned(stream, iter->second, "SNP table annot index");
        x_Write(stream, *it->second.m_SNP_annot_Info);
    }
    if ( !stream ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "SNP table store failed");
    }
}


void CSeq_annot_SNP_Info_Reader::Read(CNcbiIstream& stream,
                                      const CObjectInfo& object,
                                      CTSE_SetObjectInfo& set_info)
{
    unsigned magic = read_unsigned(stream, "SNP table magic number");
    if ( magic != MAGIC ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Incompatible version of SNP table");
    }

    CRef<CSeq_annot_ReadHook> hook(new CSeq_annot_ReadHook);
    {{
        CObjectIStreamAsnBinary obj_stream(stream);
        CObjectHookGuard<CSeq_annot> guard(*hook, &obj_stream);
        obj_stream.Read(object);
    }}

    unsigned count = read_unsigned(stream, "number of SNP table annots");
    for ( unsigned i = 0; i < count; ++i ) {
        unsigned index = read_unsigned(stream, "SNP table annot index");
        if ( index >= hook->m_Index.size() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Orphan CSeq_annot_SNP_Info");
        }
        TAnnotRef annot = hook->m_Index[index];
        _ASSERT(annot);
        CRef<CSeq_annot_SNP_Info>& snp_info =
            set_info.m_Seq_annot_InfoMap[annot].m_SNP_annot_Info;
        if ( snp_info ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Duplicate CSeq_annot_SNP_Info");
        }
        snp_info = new CSeq_annot_SNP_Info;
        x_Read(stream, *snp_info);
        snp_info->m_Seq_annot = annot;
    }
}


void CSeq_annot_SNP_Info_Reader::Write(CNcbiOstream& stream,
                                       const CSeq_annot_SNP_Info& snp_info)
{
    x_Write(stream, snp_info);

    // complex Set_Info
    CObjectOStreamAsnBinary obj_stream(stream);
    obj_stream << *snp_info.m_Seq_annot;
}


void CSeq_annot_SNP_Info_Reader::Read(CNcbiIstream& stream,
                                      CSeq_annot_SNP_Info& snp_info)
{
    x_Read(stream, snp_info);

    // complex Set_Info
    CRef<CSeq_annot> annot(new CSeq_annot);
    {{
        CObjectIStreamAsnBinary obj_stream(stream);
        CProcessor::SetSNPReadHooks(obj_stream);
        obj_stream >> *annot;
    }}
    if ( !stream ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Bad format of SNP table");
    }
    snp_info.m_Seq_annot = annot;
}


void CSeq_annot_SNP_Info_Reader::x_Write(CNcbiOstream& stream,
                                         const CSeq_annot_SNP_Info& snp_info)
{
    // header
    write_unsigned(stream, MAGIC, "SNP table magic number");
    write_unsigned(stream, GI_TO(unsigned, snp_info.GetGi()), "SNP table GI");

    // strings
    StoreIndexedStringsTo(stream, snp_info.m_Comments);
    StoreIndexedStringsTo(stream, snp_info.m_Alleles);
    StoreIndexedStringsTo(stream, snp_info.m_Extra);
    StoreIndexedStringsTo(stream, snp_info.m_QualityCodesStr);
    StoreIndexedOctetStringsTo(stream, snp_info.m_QualityCodesOs);

    // simple Set_Info
    size_t count = snp_info.m_SNP_Set.size();
    write_size(stream, count);
    stream.write(reinterpret_cast<const char*>(&snp_info.m_SNP_Set[0]),
                 count*sizeof(SSNP_Info));
}


static const size_t kMax_CommentLength = 65536;
static const size_t kMax_ExtraLength = 256;
static const size_t kMax_AlleleLength = 256;
static const size_t kMax_QualityLength = 32;

void CSeq_annot_SNP_Info_Reader::x_Read(CNcbiIstream& stream,
                                        CSeq_annot_SNP_Info& snp_info)
{
    snp_info.Reset();

    // header
    unsigned magic = read_unsigned(stream, "SNP table magic number");
    if ( magic != MAGIC ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Incompatible version of SNP table");
    }
    snp_info.x_SetGi(GI_FROM(unsigned, read_unsigned(stream, "SNP table GI")));

    // strings
    LoadIndexedStringsFrom(stream,
                           snp_info.m_Comments,
                           SSNP_Info::kMax_CommentIndex,
                           kMax_CommentLength);
    LoadIndexedStringsFrom(stream,
                           snp_info.m_Alleles,
                           SSNP_Info::kMax_AlleleIndex,
                           kMax_AlleleLength);
    LoadIndexedStringsFrom(stream,
                           snp_info.m_Extra,
                           SSNP_Info::kMax_ExtraIndex,
                           kMax_ExtraLength);
    LoadIndexedStringsFrom(stream,
                           snp_info.m_QualityCodesStr,
                           SSNP_Info::kMax_QualityCodesIndex,
                           kMax_QualityLength);
    LoadIndexedOctetStringsFrom(stream,
                                snp_info.m_QualityCodesOs,
                                SSNP_Info::kMax_QualityCodesIndex,
                                kMax_QualityLength);

    // simple Set_Info
    size_t count = read_size(stream, "SNP table simple SNP count");
    if ( count ) {
        snp_info.m_SNP_Set.resize(count);
        stream.read(reinterpret_cast<char*>(&snp_info.m_SNP_Set[0]),
                    count*sizeof(SSNP_Info));
        if ( !stream ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Cannot read SNP table simple SNPs");
        }
    }
    size_t comments_size = snp_info.m_Comments.GetSize();
    size_t alleles_size = snp_info.m_Alleles.GetSize();
    size_t extra_size = snp_info.m_Extra.GetSize();
    size_t quality_str_size = snp_info.m_QualityCodesStr.GetSize();
    size_t quality_os_size = snp_info.m_QualityCodesOs.GetSize();
    ITERATE ( CSeq_annot_SNP_Info::TSNP_Set, it, snp_info.m_SNP_Set ) {
        size_t index = it->m_CommentIndex;
        if ( index != SSNP_Info::kNo_CommentIndex &&
             index >= comments_size ) {
            snp_info.Reset();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "SNP table bad comment index");
        }
        index = it->m_ExtraIndex;
        if ( index != SSNP_Info::kNo_ExtraIndex &&
             index >= extra_size ) {
            snp_info.Reset();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "SNP table bad extra index");
        }
        switch ( it->m_Flags & SSNP_Info::fQualityCodesMask ) {
        case 0:
            break;
        case SSNP_Info::fQualityCodesStr:
            index = it->m_QualityCodesIndex;
            if ( index >= quality_str_size ) {
                snp_info.Reset();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "SNP table bad quality code str index");
            }
            break;
        case SSNP_Info::fQualityCodesOs:
            index = it->m_QualityCodesIndex;
            if ( index >= quality_os_size ) {
                snp_info.Reset();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "SNP table bad quality code os index");
            }
            break;
        default:
            snp_info.Reset();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "SNP table bad quality code type");
        }
        for ( int i = SSNP_Info::kMax_AllelesCount-1; i >= 0; --i ) {
            index = it->m_AllelesIndices[i];
            if ( index != SSNP_Info::kNo_AlleleIndex &&
                 index >= alleles_size ) {
                snp_info.Reset();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "SNP table bad allele index");
            }
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
