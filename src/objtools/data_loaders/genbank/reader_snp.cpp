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
#include <corelib/ncbi_config_value.hpp>
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

#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <util/reader_writer.hpp>

#include <algorithm>
#include <numeric>

// for debugging
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// utility function

static bool CollectSNPStat(void)
{
    static bool var = GetConfigFlag("GENBANK", "SNP_TABLE_STAT");
    return var;
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
    CSNP_Ftable_hook(void)
        : m_Seq_annot_hook(new CSeq_annot_hook)
        {
        }

    void ReadChoiceVariant(CObjectIStream& in,
                           const CObjectInfoCV& variant);

    typedef CSeq_annot_SNP_Info_Reader::TSNP_InfoMap TSNP_InfoMap;
    TSNP_InfoMap                    m_SNP_InfoMap;
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
        m_SNP_InfoMap[m_Seq_annot_hook->m_Seq_annot] = snp_info;
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
        static int dump_feature = GetConfigInt("GENBANK", "SNP_TABLE_DUMP");
        if ( dump_feature > 0 ) {
            --dump_feature;
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
                                       TSNP_InfoMap& snps)
{
    CProcessor::SetSNPReadHooks(in);
    
    if ( CProcessor::TrySNPTable() ) { // set SNP hook
        CRef<CSNP_Ftable_hook> hook(new CSNP_Ftable_hook);

        CObjectTypeInfo seq_annot_type = CType<CSeq_annot>();
        seq_annot_type.SetLocalReadHook(in, hook->m_Seq_annot_hook);

        CObjectTypeInfo seq_annot_data_type = CType<CSeq_annot::TData>();
        CObjectTypeInfoVI ftable = seq_annot_data_type.FindVariant("ftable");
        ftable.SetLocalReadHook(in, hook);

        in.Read(object);

        snps.swap(hook->m_SNP_InfoMap);
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
    TSNP_InfoMap snps;
    Parse(in, Begin(*annot), snps);
    if ( !snps.empty() ) {
        _ASSERT(snps.size() == 1);
        _ASSERT(snps.begin()->first == annot);
        ret = snps.begin()->second;
    }
    else {
        ret = new CSeq_annot_SNP_Info(*annot);
    }

    return ret;
}


void CSeq_annot_SNP_Info_Reader::Parse(CObjectIStream& in,
                                       CSeq_entry& tse,
                                       TSNP_InfoMap& snps)
{
    Parse(in, Begin(tse), snps);
}


/////////////////////////////////////////////////////////////////////////////
// reading and storing in binary format

static void write_unsigned(CNcbiOstream& stream, unsigned n)
{
    stream.write(reinterpret_cast<const char*>(&n), sizeof(n));
}


static unsigned read_unsigned(CNcbiIstream& stream)
{
    unsigned n;
    stream.read(reinterpret_cast<char*>(&n), sizeof(n));
    return n;
}


static void write_size(CNcbiOstream& stream, unsigned size)
{
    // use ASN.1 binary like format
    while ( size >= (1<<7) ) {
        stream.put(char(size | (1<<7)));
        size >>= 7;
    }
    stream.put(char(size));
}


static unsigned read_size(CNcbiIstream& stream)
{
    unsigned size = 0;
    int shift = 0;
    char c = char(1<<7);
    while ( c & (1<<7) ) {
        c = stream.get();
        size |= (c & ((1<<7)-1)) << shift;
        shift += 7;
    }
    return size;
}


void CIndexedStrings::StoreTo(CNcbiOstream& stream) const
{
    write_size(stream, m_Strings.size());
    ITERATE ( TStrings, it, m_Strings ) {
        unsigned size = it->size();
        write_size(stream, size);
        stream.write(it->data(), size);
    }
}


void CIndexedStrings::LoadFrom(CNcbiIstream& stream,
                               size_t max_index,
                               size_t max_length)
{
    Clear();
    unsigned count = read_size(stream);
    if ( !stream || (count > unsigned(max_index+1)) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Bad format of SNP table");
    }
    m_Strings.resize(count);
    AutoPtr<char, ArrayDeleter<char> > buf(new char[max_length]);
    NON_CONST_ITERATE ( TStrings, it, m_Strings ) {
        unsigned size = read_size(stream);
        if ( !stream || (size > max_length) ) {
            Clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Bad format of SNP table");
        }
        stream.read(buf.get(), size);
        if ( !stream ) {
            Clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Bad format of SNP table");
        }
        it->assign(buf.get(), buf.get()+size);
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
            const CSeq_annot* ptr = CType<CSeq_annot>::Get(object);
            m_Index.push_back(ConstRef(ptr));
            DefaultRead(stream, object);
        }
            
    TIndex m_Index;
};

}


static const unsigned MAGIC = 0x12340003;


void CSeq_annot_SNP_Info_Reader::Write(CNcbiOstream& stream,
                                       const CConstObjectInfo& object,
                                       const TSNP_InfoMap& snps)
{
    write_unsigned(stream, MAGIC);

    CRef<CSeq_annot_WriteHook> hook(new CSeq_annot_WriteHook);
    {{
        CObjectOStreamAsnBinary obj_stream(stream);
        obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
        CObjectTypeInfo type = CType<CSeq_annot>();
        type.SetLocalWriteHook(obj_stream, hook);
        obj_stream.Write(object);
    }}

    write_unsigned(stream, snps.size());
    ITERATE ( TSNP_InfoMap, it, snps ) {
        TAnnotToIndex::const_iterator iter = hook->m_Index.find(it->first);
        if ( iter == hook->m_Index.end() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Orphan CSeq_annot_SNP_Info");
        }
        write_unsigned(stream, iter->second);
        x_Write(stream, *it->second);
    }
}


void CSeq_annot_SNP_Info_Reader::Read(CNcbiIstream& stream,
                                      const CObjectInfo& object,
                                      TSNP_InfoMap& snps)
{
    unsigned magic = read_unsigned(stream);
    if ( !stream || magic != MAGIC ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Incompatible version of SNP table");
    }

    CRef<CSeq_annot_ReadHook> hook(new CSeq_annot_ReadHook);
    {{
        CObjectIStreamAsnBinary obj_stream(stream);
        CObjectTypeInfo type = CType<CSeq_annot>();
        type.SetLocalReadHook(obj_stream, hook);
        //CProcessor::SetSNPReadHooks(obj_stream);
        obj_stream.Read(object);
    }}

    unsigned count = read_unsigned(stream);
    for ( unsigned i = 0; i < count; ++i ) {
        unsigned index = read_unsigned(stream);
        if ( index >= hook->m_Index.size() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Orphan CSeq_annot_SNP_Info");
        }
        TAnnotRef annot = hook->m_Index[index];
        _ASSERT(annot);
        TAnnotSNPRef& snp_info = snps[annot];
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

    // complex SNPs
    CObjectOStreamAsnBinary obj_stream(stream);
    obj_stream << *snp_info.m_Seq_annot;
}


void CSeq_annot_SNP_Info_Reader::Read(CNcbiIstream& stream,
                                      CSeq_annot_SNP_Info& snp_info)
{
    x_Read(stream, snp_info);

    // complex SNPs
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
    write_unsigned(stream, MAGIC);
    write_unsigned(stream, snp_info.GetGi());

    // strings
    snp_info.m_Comments.StoreTo(stream);
    snp_info.m_Alleles.StoreTo(stream);

    // simple SNPs
    unsigned count = snp_info.m_SNP_Set.size();
    write_size(stream, count);
    stream.write(reinterpret_cast<const char*>(&snp_info.m_SNP_Set[0]),
                 count*sizeof(SSNP_Info));
}


void CSeq_annot_SNP_Info_Reader::x_Read(CNcbiIstream& stream,
                                        CSeq_annot_SNP_Info& snp_info)
{
    snp_info.Reset();

    // header
    unsigned magic = read_unsigned(stream);
    if ( !stream || magic != MAGIC ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Incompatible version of SNP table");
    }
    snp_info.x_SetGi(read_unsigned(stream));

    // strings
    snp_info.m_Comments.LoadFrom(stream,
                                 SSNP_Info::kMax_CommentIndex,
                                 SSNP_Info::kMax_CommentLength);
    snp_info.m_Alleles.LoadFrom(stream,
                                SSNP_Info::kMax_AlleleIndex,
                                SSNP_Info::kMax_AlleleLength);

    // simple SNPs
    unsigned count = read_size(stream);
    if ( stream ) {
        snp_info.m_SNP_Set.resize(count);
        stream.read(reinterpret_cast<char*>(&snp_info.m_SNP_Set[0]),
                    count*sizeof(SSNP_Info));
    }
    size_t comments_size = snp_info.m_Comments.GetSize();
    size_t alleles_size = snp_info.m_Alleles.GetSize();
    ITERATE ( CSeq_annot_SNP_Info::TSNP_Set, it, snp_info.m_SNP_Set ) {
        size_t index = it->m_CommentIndex;
        if ( index != SSNP_Info::kNo_CommentIndex &&
             index >= comments_size ) {
            snp_info.Reset();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Bad format of SNP table");
        }
        for ( size_t i = 0; i < SSNP_Info::kMax_AllelesCount; ++i ) {
            index = it->m_AllelesIndices[i];
            if ( index != SSNP_Info::kNo_AlleleIndex &&
                 index >= alleles_size ) {
                snp_info.Reset();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "Bad format of SNP table");
            }
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.19  2005/03/10 20:55:07  vasilche
 * New CReader/CWriter schema of CGBDataLoader.
 *
 * Revision 1.18  2005/02/23 21:15:38  vasilche
 * Do not flush in the middle of SNP table.
 *
 * Revision 1.17  2005/01/05 18:46:13  vasilche
 * Added GetConfigXxx() functions.
 *
 * Revision 1.16  2004/10/07 14:08:10  vasilche
 * Use static GetConfigXxx() functions.
 * Try to deal with withdrawn and private blobs without exceptions.
 * Use correct desc mask in split data.
 *
 * Revision 1.15  2004/09/14 19:10:02  vasilche
 * Use SConfigIntValue and SConfigBoolValue for configurables.
 * Avoid exceptions when dealing with confidential or withdrawn data.
 *
 * Revision 1.14  2004/08/12 14:19:53  vasilche
 * Allow SNP Seq-entry in addition to SNP Seq-annot.
 *
 * Revision 1.13  2004/08/04 14:55:18  vasilche
 * Changed TSE locking scheme.
 * TSE cache is maintained by CDataSource.
 * Added ID2 reader.
 * CSeqref is replaced by CBlobId.
 *
 * Revision 1.12  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/03/16 16:04:20  vasilche
 * Removed conversion warning
 *
 * Revision 1.10  2004/02/06 16:13:19  vasilche
 * Added parsing "replace" as a synonym of "allele" in SNP qualifiers.
 * More compact format of SNP table in cache. SNP table version increased.
 * Fixed null pointer exception when SNP features are loaded from cache.
 *
 * Revision 1.9  2004/01/13 16:55:55  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.8  2003/10/23 13:47:56  vasilche
 * Fixed Reset() method: m_Gi should be -1.
 *
 * Revision 1.7  2003/10/21 16:29:13  vasilche
 * Added check for errors in SNP table loaded from cache.
 *
 * Revision 1.6  2003/10/21 15:21:21  vasilche
 * Avoid use of non-constant array sizes of stack arrays.
 *
 * Revision 1.5  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.4  2003/09/30 16:22:03  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.3  2003/08/19 18:35:21  vasilche
 * CPackString classes were moved to SERIAL library.
 *
 * Revision 1.2  2003/08/15 19:19:16  vasilche
 * Fixed memory leak in string packing hooks.
 * Fixed processing of 'partial' flag of features.
 * Allow table packing of non-point SNP.
 * Allow table packing of SNP with long alleles.
 *
 * Revision 1.1  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 */
