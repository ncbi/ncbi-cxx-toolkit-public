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

#include <corelib/ncbiobj.hpp>

#include <objmgr/reader.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/reader_snp.hpp>

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


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SNP_Info
/////////////////////////////////////////////////////////////////////////////

CSeq_annot_SNP_Info::CSeq_annot_SNP_Info(void)
    : m_Gi(-1)
{
}


CSeq_annot_SNP_Info::~CSeq_annot_SNP_Info(void)
{
}


size_t CIndexedStrings::GetIndex(const string& s, size_t max_index)
{
    TIndices::iterator it = m_Indices.lower_bound(s);
    if ( it != m_Indices.end() && it->first == s ) {
        return it->second;
    }
    size_t index = m_Strings.size();
    if ( index <= max_index ) {
        //NcbiCout << "string["<<index<<"] = \"" << s << "\"\n";
        m_Strings.push_back(s);
        m_Indices.insert(it, TIndices::value_type(s, index));
    }
    else {
        _ASSERT(index == max_index + 1);
    }
    return index;
}


void CIndexedStrings::StoreTo(CNcbiOstream& stream) const
{
    unsigned size = m_Strings.size();
    stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    ITERATE ( TStrings, it, m_Strings ) {
        size = it->size();
        stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
        stream.write(it->data(), size);
    }
}


void CIndexedStrings::LoadFrom(CNcbiIstream& stream,
                               size_t max_index,
                               size_t max_length)
{
    Clear();
    unsigned size;
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    if ( !stream || (size > unsigned(max_index+1)) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Bad format of SNP table");
    }
    m_Strings.resize(size);
    AutoPtr<char, ArrayDeleter<char> > buf(new char[max_length]);
    NON_CONST_ITERATE ( TStrings, it, m_Strings ) {
        stream.read(reinterpret_cast<char*>(&size), sizeof(size));
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


SSNP_Info::TAlleleIndex
CSeq_annot_SNP_Info::x_GetAlleleIndex(const string& allele)
{
    if ( allele.size() > SSNP_Info::kMax_AlleleLength )
        return SSNP_Info::kNo_AlleleIndex;
    if ( m_Alleles.IsEmpty() ) {
        // prefill by small alleles
        for ( const char* c = "-NACGT"; *c; ++c ) {
            m_Alleles.GetIndex(string(1, *c), SSNP_Info::kMax_AlleleIndex);
        }
        for ( const char* c1 = "ACGT"; *c1; ++c1 ) {
            string s(1, *c1);
            for ( const char* c2 = "ACGT"; *c2; ++c2 ) {
                m_Alleles.GetIndex(s+*c2, SSNP_Info::kMax_AlleleIndex);
            }
        }
    }
    return m_Alleles.GetIndex(allele, SSNP_Info::kMax_AlleleIndex);
}


void CSeq_annot_SNP_Info::Read(CObjectIStream& in)
{
    m_Seq_annot.Reset(new CSeq_annot); // Seq-annot object

    CReader::SetSNPReadHooks(in);

    if ( CReader::TrySNPTable() ) { // set SNP hook
        CObjectTypeInfo type = CType<CSeq_annot::TData>();
        CObjectTypeInfoVI ftable = type.FindVariant("ftable");
        ftable.SetLocalReadHook(in, new CSNP_Ftable_hook(*this));
    }
    
    in >> *m_Seq_annot;

    // we don't need index maps anymore
    m_Comments.ClearIndices();
    m_Alleles.ClearIndices();

    sort(m_SNP_Set.begin(), m_SNP_Set.end());
}

    
CRef<CSeq_entry> CSeq_annot_SNP_Info::GetEntry(void)
{
    CRef<CSeq_entry> entry(new CSeq_entry); // return value
    entry->SetSet().SetSeq_set(); // it's not optional
    entry->SetSet().SetAnnot().push_back(m_Seq_annot); // store it in Seq-entry
    return entry;
}


void CSeq_annot_SNP_Info::Reset(void)
{
    m_Gi = -1;
    m_Comments.Clear();
    m_Alleles.Clear();
    m_SNP_Set.clear();
    m_Seq_annot.Reset();
}


static const unsigned MAGIC = 0x12340001;

void CSeq_annot_SNP_Info::StoreTo(CNcbiOstream& stream) const
{
    // header
    stream.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
    stream.write(reinterpret_cast<const char*>(&m_Gi), sizeof(m_Gi));

    // strings
    m_Comments.StoreTo(stream);
    m_Alleles.StoreTo(stream);

    // simple SNPs
    unsigned size = m_SNP_Set.size();
    stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    stream.write(reinterpret_cast<const char*>(&*m_SNP_Set.begin()),
                 size*sizeof(SSNP_Info));

    // complex SNPs
    CObjectOStreamAsnBinary obj_stream(stream);
    obj_stream << *m_Seq_annot;
}


void CSeq_annot_SNP_Info::LoadFrom(CNcbiIstream& stream)
{
    Reset();

    // header
    unsigned magic;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if ( !stream || magic != MAGIC ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Incompatible version of SNP table");
    }
    stream.read(reinterpret_cast<char*>(&m_Gi), sizeof(m_Gi));

    // strings
    m_Comments.LoadFrom(stream,
                        SSNP_Info::kMax_CommentIndex,
                        SSNP_Info::kMax_CommentLength);
    m_Alleles.LoadFrom(stream,
                       SSNP_Info::kMax_AlleleIndex,
                       SSNP_Info::kMax_AlleleLength);

    // simple SNPs
    unsigned size;
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    if ( stream ) {
        m_SNP_Set.resize(size);
        stream.read(reinterpret_cast<char*>(&*m_SNP_Set.begin()),
                    size*sizeof(SSNP_Info));
    }
    size_t comments_size = m_Comments.GetSize();
    size_t alleles_size = m_Alleles.GetSize();
    ITERATE ( TSNP_Set, it, m_SNP_Set ) {
        size_t index = it->m_CommentIndex;
        if ( index != SSNP_Info::kNo_CommentIndex &&
             index >= comments_size ) {
            m_SNP_Set.clear();
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "Bad format of SNP table");
        }
        for ( size_t i = 0; i < SSNP_Info::kMax_AllelesCount; ++i ) {
            index = it->m_AllelesIndices[i];
            if ( index != SSNP_Info::kNo_AlleleIndex &&
                 index >= alleles_size ) {
                m_SNP_Set.clear();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "Bad format of SNP table");
            }
        }
    }

    // complex SNPs
    CObjectIStreamAsnBinary obj_stream(stream);
    if ( !m_Seq_annot ) {
        m_Seq_annot.Reset(new CSeq_annot);
    }
    obj_stream >> *m_Seq_annot;

    if ( !stream ) {
        m_Seq_annot.Reset();
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Bad format of SNP table");
    }
}


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
    if ( 0 ) {
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
    /*
    CObjectIStreamAsnBinary* bin = dynamic_cast<CObjectIStreamAsnBinary*>(&in);
    if ( bin ) {
        char buffer[100];
        size_t count = 0;
        try {
            count = bin->PeekBytes(buffer, sizeof(buffer));
        }
        catch ( ... ) {
            count = 0;
        }
        string s;
        s.reserve(100);
        for ( size_t i = 0; i < count; ++i ) {
            char b = buffer[i];
            s += "0123456789abcdef"[(b>>4)&15];
            s += "0123456789abcdef"[(b)&15];
            s += ' ';
        }
        s += '\n';
        NcbiCout << s;
    }
    */
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
        m_Ftable.push_back(m_Feat);
        m_Feat.Reset();
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
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
