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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*
*
*/

#include <ncbi_pch.hpp>

#include "huge_asn_reader.hpp"

#include <objtools/readers/format_guess_ex.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <serial/objistr.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/objhook_lambdas.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE

using namespace objects;


bool CRefLess::operator()(const CConstRef<objects::CSeq_id>& l, const CConstRef<objects::CSeq_id>& r) const
{
    return *l < *r;
}

CHugeAsnReader::~CHugeAsnReader()
{
}

CHugeAsnReader::CHugeAsnReader()
{
}

CHugeAsnReader::CHugeAsnReader(CHugeFile* file, objects::ILineErrorListener * pMessageListener)
{
    Open(file, pMessageListener);
}

void CHugeAsnReader::x_ResetIndex()
{
    m_total_seqs = 0;
    m_total_sets = 0;
    m_max_local_id = 0;
    m_submit.Reset();
    m_bioseq_list.clear();
    m_bioseq_index.clear();
    m_bioseq_set_index.clear();
    m_flattened.clear();
    m_current = m_flattened.end();
}

void CHugeAsnReader::Open(CHugeFile* file, objects::ILineErrorListener * pMessageListener)
{
    x_ResetIndex();

    m_file = file;
    mp_MessageListener = pMessageListener;
}

void CHugeAsnReader::PrintAllSeqIds() const
{
    char report_buffer[80];
    for (auto& rec: m_bioseq_index)
    {
        sprintf(report_buffer, "%15ld:%s\n", rec.second->m_pos, rec.first->AsFastaString().c_str());
        std::cout << report_buffer;
        #if 0
        #if 0
            auto bioseq = LoadBioseq(rec.first);
        #else
            auto entry = LoadSeqEntry(rec.first);
            auto bioseq = Ref(&entry->GetSet().GetSeq_set().front()->GetSeq());4
            std::cout << entry->GetSet().GetClass() << ":";
        #endif
        std::cout <<
            (bioseq->IsNa()? "na" : "-") << ":" <<
            (bioseq->IsAa()? "aa" : "-") << ":" <<
            (bioseq->IsSetLength()? bioseq->GetLength() : 0) <<
            "\n";
        #endif
    }
    std::cout << "Total seqids:" << m_total_seqs << ":" << m_bioseq_index.size() << std::endl;
}

size_t CHugeAsnReader::FindTopObject(CConstRef<objects::CSeq_id> seqid) const
{
    auto it = m_bioseq_index.find(seqid);
    if (it == m_bioseq_index.end())
        return std::numeric_limits<size_t>::max();
    else
        return std::numeric_limits<size_t>::max(); //it->second->m_parent_set;
}

CRef<objects::CSeq_entry> CHugeAsnReader::LoadSeqEntry(const TBioseqSetInfo& info) const
{
    auto entry = Ref(new CSeq_entry);
    auto obj_stream = x_MakeObjStream(info.m_pos);
    if (info.m_class == CBioseq_set::eClass_not_set)
    {
        obj_stream->Read(&entry->SetSeq(), CBioseq::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    } else {
        obj_stream->Read(&entry->SetSet(), CBioseq_set::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    }
    return entry;
}

CRef<objects::CBioseq> CHugeAsnReader::LoadBioseq(CConstRef<CSeq_id> seqid) const
{
    auto it = m_bioseq_index.find(seqid);
    if (it == m_bioseq_index.end())
        return {};

    auto obj_stream = x_MakeObjStream(it->second->m_pos);
    auto bioseq = Ref(new CBioseq);
    obj_stream->Read(bioseq, CBioseq::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    return bioseq;
}

unique_ptr<CObjectIStream> CHugeAsnReader::x_MakeObjStream(TFileSize pos) const
{
    unique_ptr<CObjectIStream> str;

    if (m_file->m_memory)
        str.reset(CObjectIStream::CreateFromBuffer(
        m_file->m_serial_format, m_file->m_memory+pos, m_file->m_filesize-pos));
    else {
        std::unique_ptr<std::ifstream> stream{new std::ifstream(m_file->m_filename, ios::binary)};
        stream->seekg(pos);
        str.reset(CObjectIStream::Open(m_file->m_serial_format, *stream.release(), eTakeOwnership));
    }

    str->UseMemoryPool();

    return str;
}

CRef<CSeq_entry> CHugeAsnReader::GetNextSeqEntry()
{
    if (m_current == m_flattened.end())
        return {};
    else
        return LoadSeqEntry(*m_current++);
}

bool CHugeAsnReader::GetNextBlob()
{
    if (m_streampos >= m_file->m_filesize)
        return false;

    x_IndexNextAsn1();
    return true;
}

void CHugeAsnReader::x_IndexNextAsn1()
{
    x_ResetIndex();
    auto object_type = m_file->RecognizeContent(m_streampos);

    auto obj_stream = x_MakeObjStream(m_streampos);

    CObjectTypeInfo bioseq_info = CType<CBioseq>();
    CObjectTypeInfo bioseq_set_info = CType<CBioseq_set>();
    CObjectTypeInfo seqsubmit_info = CType<CSeq_submit>();
    CObjectTypeInfo seqinst_info = CType<CSeq_inst>();

    auto bioseq_id_mi = bioseq_info.FindMember("id");
    auto bioseqset_class_mi = bioseq_set_info.FindMember("class");
    auto seqsubmit_data_mi = seqsubmit_info.FindMember("data");
    auto bioseqset_descr_mi = bioseq_set_info.FindMember("descr");
    auto seqinst_len_mi = seqinst_info.FindMember("length");

    // temporal structure for indexing
    struct TBioseqInfoRec
    {
        CBioseq::TId m_ids;
        TSeqPos      m_length  = 0;
    };

    struct TContext
    {
        std::deque<TBioseqInfoRec> bioseq_stack;
        std::deque<TBioseqSetIndex::iterator> bioseq_set_stack;
    };

    TContext context;

    SetLocalSkipHook(bioseq_id_mi, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_ids, (*member).GetTypeInfo());
    });

    SetLocalSkipHook(bioseqset_class_mi, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        CBioseq_set::TClass _class;
        in.ReadObject(&_class, (*member).GetTypeInfo());
        context.bioseq_set_stack.back()->m_class = _class;
    });

    SetLocalSkipHook(bioseqset_descr_mi, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        auto descr = Ref(new CSeq_descr);
        in.ReadObject(&descr, (*member).GetTypeInfo());
        context.bioseq_set_stack.back()->m_descr = descr;
    });

    SetLocalSkipHook(seqinst_len_mi, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_length, (*member).GetTypeInfo());
    });

    SetLocalSkipHook(CType<CFeat_id>(), *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        auto id = Ref(new CFeat_id);
        type.GetTypeInfo()->DefaultReadData(in, id);
        if (id->IsLocal() && id->GetLocal().IsId())
        {
            m_max_local_id = std::max(m_max_local_id, id->GetLocal().GetId());
        }
    });


    SetLocalSkipHook(bioseq_info, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        m_total_seqs++;
        auto pos = in.GetStreamPos() + m_streampos;

        context.bioseq_stack.push_back({});
        auto parent = context.bioseq_set_stack.back();

        type.GetTypeInfo()->DefaultSkipData(in);

        auto& bioseqinfo = context.bioseq_stack.back();
        m_bioseq_list.push_back({pos, parent, bioseqinfo.m_length});
        auto last = --m_bioseq_list.end();

        for (auto& id: bioseqinfo.m_ids)
        {
            m_bioseq_index[id] = last;
        }
        context.bioseq_stack.pop_back();
    });

    SetLocalSkipHook(bioseq_set_info, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        m_total_sets++;
        auto pos = in.GetStreamPos() + m_streampos;

        auto parent = context.bioseq_set_stack.back();
        m_bioseq_set_index.push_back({pos, parent});

        auto last = --(m_bioseq_set_index.end());

        context.bioseq_set_stack.push_back(last);
        type.GetTypeInfo()->DefaultSkipData(in);
        context.bioseq_set_stack.pop_back();
    });

    SetLocalSkipHook(seqsubmit_info, *obj_stream,
        [this](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        auto submit = Ref(new CSeq_submit);
        in.Read(submit, CSeq_submit::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        m_submit = submit;
    });

    SetLocalReadHook(seqsubmit_data_mi, *obj_stream,
        [this, &context](CObjectIStream& in, const CObjectInfoMI& member)
    {
        auto submit = CTypeConverter<CSeq_submit>::SafeCast(member.GetClassObject().GetObjectPtr());
        //submit->SetData().SetEntrys().clear();
        submit->ResetData();
        //std::cout << MSerial_AsnText << MSerial_VerifyNo << *submit << std::endl;
        in.SkipObject(member.GetMemberType());
    });

    // Ensure there is at least on bioseq_set_info object exists
    m_bioseq_set_index.push_back({ m_streampos, m_bioseq_set_index.end() });
    context.bioseq_set_stack.push_back(m_bioseq_set_index.begin());
    obj_stream->Skip(object_type);
    obj_stream->EndOfData(); // force to SkipWhiteSpace
    m_streampos += obj_stream->GetStreamPos();

    FlattenGenbankSet();
}

void CHugeAsnReader::FlattenGenbankSet()
{
    m_flattened.clear();

    for (auto& rec: GetBioseqs())
    {
        auto parent = rec.m_parent_set;
        auto _class = parent->m_class;
        if (_class == CBioseq_set::eClass_not_set ||
            _class == CBioseq_set::eClass_genbank)
        { // create fake bioseq_set
            m_flattened.push_back({rec.m_pos, GetBiosets().end(), objects::CBioseq_set::eClass_not_set });
        } else {
            if (m_flattened.empty() || (m_flattened.back().m_pos != parent->m_pos))
                m_flattened.push_back(*parent);
        }
    }

    m_current = m_flattened.begin();
}

END_NCBI_SCOPE
