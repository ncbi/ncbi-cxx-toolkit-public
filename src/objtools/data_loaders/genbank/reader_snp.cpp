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
#include <objtools/data_loaders/genbank/reader_snp.hpp>

#include <objtools/data_loaders/genbank/reader.hpp>

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

#include <util/reader_writer.hpp>

#include <algorithm>
#include <numeric>

// for debugging
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSNP_Ftable_hook : public CReadChoiceVariantHook
{
public:
    CSNP_Ftable_hook(CSeq_annot_SNP_Info& annot_snp_info)
        : m_Seq_annot_SNP_Info(annot_snp_info)
        {
        }

    void ReadChoiceVariant(CObjectIStream& in,
                           const CObjectInfoCV& variant);

private:
    CSeq_annot_SNP_Info&   m_Seq_annot_SNP_Info;
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
    CObjectInfo data_info = variant.GetChoiceObject();
    CObjectInfo ftable_info = *variant;
    CSeq_annot::TData& data = *CType<CSeq_annot::TData>::Get(data_info);
    _ASSERT(ftable_info.GetObjectPtr() == static_cast<TConstObjectPtr>(&data.GetFtable()));
    CSNP_Seq_feat_hook hook(m_Seq_annot_SNP_Info, data.SetFtable());
    ftable_info.ReadContainer(in, hook);
}


static int s_GetEnvInt(const char* env, int def_val)
{
    const char* val = ::getenv(env);
    if ( val ) {
        try {
            return NStr::StringToInt(val);
        }
        catch (...) {
        }
    }
    return def_val;
}


static bool s_SNP_stat = s_GetEnvInt("GENBANK_SNP_TABLE_STAT", 0) > 0;


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
    if ( s_SNP_stat ) {
        size_t total =
            accumulate(m_Count, m_Count+SSNP_Info::eSNP_Type_last, 0);
        NcbiCout << "CSeq_annot_SNP_Info statistic:\n";
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
        static int dump_feature = s_GetEnvInt("GENBANK_SNP_TABLE_DUMP", 0);
        if ( dump_feature ) {
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


void CSeq_annot_SNP_Info_Reader::Parse(CObjectIStream& in,
                                       CSeq_annot_SNP_Info& snp_info)
{
    snp_info.m_Seq_annot.Reset(new CSeq_annot); // Seq-annot object

    CReader::SetSNPReadHooks(in);

    if ( CReader::TrySNPTable() ) { // set SNP hook
        CObjectTypeInfo type = CType<CSeq_annot::TData>();
        CObjectTypeInfoVI ftable = type.FindVariant("ftable");
        ftable.SetLocalReadHook(in, new CSNP_Ftable_hook(snp_info));
    }
    
    in >> *snp_info.m_Seq_annot;

    // we don't need index maps anymore
    snp_info.m_Comments.ClearIndices();
    snp_info.m_Alleles.ClearIndices();

    sort(snp_info.m_SNP_Set.begin(), snp_info.m_SNP_Set.end());
}


static const unsigned MAGIC = 0x12340002;

void CSeq_annot_SNP_Info_Reader::Write(CNcbiOstream& stream,
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

    // complex SNPs
    CObjectOStreamAsnBinary obj_stream(stream);
    obj_stream << *snp_info.m_Seq_annot;
}


void CSeq_annot_SNP_Info_Reader::Read(CNcbiIstream& stream,
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

    // complex SNPs
    CObjectIStreamAsnBinary obj_stream(stream);
    if ( !snp_info.m_Seq_annot ) {
        snp_info.m_Seq_annot.Reset(new CSeq_annot);
    }
    obj_stream >> *snp_info.m_Seq_annot;

    if ( !stream ) {
        snp_info.m_Seq_annot.Reset();
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Bad format of SNP table");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
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
