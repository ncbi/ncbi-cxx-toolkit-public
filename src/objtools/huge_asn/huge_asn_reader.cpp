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

#include <objects/seq/Bioseq.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <serial/objistr.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/readers/objhook_lambdas.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CHugeAsnReader::~CHugeAsnReader()
{
}

CHugeAsnReader::CHugeAsnReader()
{
}

CHugeAsnReader::CHugeAsnReader(CHugeFile* file, ILineErrorListener * pMessageListener)
{
    Open(file, pMessageListener);
}

void CHugeAsnReader::x_ResetIndex()
{
    m_max_local_id = 0;
    m_bioseq_list.clear();
    m_bioseq_set_list.clear();
    m_submit_block.Reset();
    m_seq_id_types = {};

// flattenization structures, readonly after flattenization, accept m_Current
    m_bioseq_index.clear();
    m_FlattenedIndex.clear();
    m_FlattenedSets.clear();
    m_top_ids.clear();
    m_top_entry.Reset();
    m_Current = m_FlattenedSets.end();

}


bool CHugeAsnReader::IsHugeSet(CBioseq_set::TClass setClass)
{
    return (setClass == CBioseq_set::eClass_genbank ||
        //    setClass == CBioseq_set::eClass_wgs_set ||
            setClass == CBioseq_set::eClass_not_set);
}


void CHugeAsnReader::Open(CHugeFile* file, ILineErrorListener * pMessageListener)
{
    x_ResetIndex();

    m_file.Reset(file);
    mp_MessageListener = pMessageListener;
}

bool CHugeAsnReader::IsMultiSequence() const
{
    return m_FlattenedSets.size()>1;
    //return m_bioseq_index.size()>1;
}

const CHugeAsnReader::TBioseqSetInfo* CHugeAsnReader::FindTopObject(CConstRef<CSeq_id> seqid) const
{
    auto it = m_FlattenedIndex.lower_bound(seqid);
    if (it == m_FlattenedIndex.end())
        return nullptr;
    if (it->first->CompareOrdered(*seqid) == 0)
        return &*it->second;
    if (it->first->Compare(*seqid) != CSeq_id::E_SIC::e_YES)
        return nullptr;

    return &*it->second;
}

const CHugeAsnReader::TBioseqInfo* CHugeAsnReader::FindBioseq(CConstRef<CSeq_id> seqid) const
{
    auto it = m_bioseq_index.lower_bound(seqid);
    if (it == m_bioseq_index.end())
        return nullptr;
    if (it->first->CompareOrdered(*seqid) == 0)
        return &*it->second;
    if (it->first->Compare(*seqid) != CSeq_id::E_SIC::e_YES)
        return nullptr;

    return &*it->second;
}



static CConstRef<CSeqdesc> s_GetDescriptor(const CSeq_descr& descr, CSeqdesc::E_Choice choice)
{
    if (descr.IsSet()) {
        for (auto pDesc : descr.Get()) {
            if (pDesc && (pDesc->Which() == choice)) {
                return pDesc;
            }
        }
    }

    return {};
}


CConstRef<CSeqdesc> CHugeAsnReader::GetClosestDescriptor(const TBioseqInfo& info, CSeqdesc::E_Choice choice) const
{
    CConstRef<CSeqdesc> result;

    if (info.m_descr) {
        result = s_GetDescriptor(*info.m_descr, choice);
        if (result) {
            return result;
        }
    }

    auto parentSet = info.m_parent_set;
    while (parentSet != end(m_bioseq_set_list)) {
        if (parentSet->m_descr) {
            result = s_GetDescriptor(*parentSet->m_descr, choice);
            if (result) {
                return result;
            }
        }
        parentSet = parentSet->m_parent_set;
    }

    return result;
}


CConstRef<CSeqdesc> CHugeAsnReader::GetClosestDescriptor(const CSeq_id& id, CSeqdesc::E_Choice choice) const
{
    CConstRef<CSeq_id> pId(&id);
    const auto* pInfo = FindBioseq(pId);
    if (!pInfo) {
        return {};
    }
    return GetClosestDescriptor(*pInfo, choice);
}


CRef<CSeq_entry> CHugeAsnReader::LoadSeqEntry(CConstRef<CSeq_id> seqid) const
{
    auto info = FindTopObject(seqid);
    if (info)
        return LoadSeqEntry(*info);
    else
        return {};
}

CRef<CSeq_entry> CHugeAsnReader::LoadSeqEntry(const TBioseqSetInfo& info, eAddTopEntry add_top_entry) const
{
    auto entry = Ref(new CSeq_entry);
    auto obj_stream = m_file->MakeObjStream(info.m_pos);
    if (info.m_class == CBioseq_set::eClass_not_set)
    {
        obj_stream->Read(&entry->SetSeq(), CBioseq::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    } else {
        obj_stream->Read(&entry->SetSet(), CBioseq_set::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    }

    if (add_top_entry == eAddTopEntry::yes && m_top_entry) {
        auto pNewEntry = Ref(new CSeq_entry());
        pNewEntry->Assign(*m_top_entry);
        pNewEntry->SetSet().SetSeq_set().push_back(entry);
        return pNewEntry;
    }

    return entry;
}

CRef<CBioseq> CHugeAsnReader::LoadBioseq(CConstRef<CSeq_id> seqid) const
{
    auto it = m_bioseq_index.lower_bound(seqid);
    if (it == m_bioseq_index.end())
        return {};
    if (it->first->Compare(*seqid) != CSeq_id::E_SIC::e_YES)
        return {};

    auto obj_stream = m_file->MakeObjStream(it->second->m_pos);
    auto bioseq = Ref(new CBioseq);
    obj_stream->Read(bioseq, CBioseq::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    return bioseq;
}

unique_ptr<CObjectIStream> CHugeAsnReader::MakeObjStream(TFileSize pos) const
{
    return m_file->MakeObjStream(pos);
}

bool CHugeAsnReader::GetNextBlob()
{
    if (m_next_pos >= m_file->m_filesize)
        return false;

    x_IndexNextAsn1();
    return true;
}

void CHugeAsnReader::x_SetHooks(CObjectIStream& objStream, CHugeAsnReader::TContext& context)
{
    CObjectTypeInfo bioseq_info = CType<CBioseq>();
    CObjectTypeInfo bioseq_set_info = CType<CBioseq_set>();
    CObjectTypeInfo seqinst_info = CType<CSeq_inst>();

    auto bioseq_id_mi = bioseq_info.FindMember("id");
    //auto bioseqset_class_mi = bioseq_set_info.FindMember("class");
    //auto bioseqset_descr_mi = bioseq_set_info.FindMember("descr");
    auto bioseqset_seqset_mi = bioseq_set_info.FindMember("seq-set");
    auto bioseqset_annot_mi = bioseq_set_info.FindMember("annot");
    auto seqinst_len_mi = seqinst_info.FindMember("length");
    auto seqinst_mol_mi = seqinst_info.FindMember("mol");
    auto seqinst_repr_mi = seqinst_info.FindMember("repr");
    auto bioseq_descr_mi = bioseq_info.FindMember("descr");


    SetLocalSkipHook(bioseq_id_mi, objStream,
        [&context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_ids, (*member).GetTypeInfo());
    });

    SetLocalSkipHook(bioseq_descr_mi, objStream,
        [&context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        auto descr = Ref(new CSeq_descr);
        in.ReadObject(&descr, (*member).GetTypeInfo());
        context.bioseq_stack.back().m_descr = descr;
    });

    SetLocalSkipHook(seqinst_len_mi, objStream,
        [&context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_length, (*member).GetTypeInfo());
    });

    SetLocalSkipHook(seqinst_mol_mi, objStream,
        [&context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_mol, (*member).GetTypeInfo());
    });


    SetLocalSkipHook(seqinst_repr_mi, objStream,
        [&context](CObjectIStream& in, const CObjectTypeInfoMI& member)
    {
        in.ReadObject(&context.bioseq_stack.back().m_repr, (*member).GetTypeInfo());
    });


    x_SetFeatIdHooks(objStream);

    SetLocalReadHook(bioseqset_seqset_mi, objStream,
            [](CObjectIStream& in, const CObjectInfoMI& member)
            {
                (*member).GetTypeInfo()->DefaultSkipData(in);
            });


    SetLocalReadHook(bioseqset_annot_mi, objStream,
            [&context, this](CObjectIStream& in, const CObjectInfoMI& member)
            {
                auto pos = in.GetStreamPos() + m_next_pos;
                auto& set_rec = context.bioseq_set_stack.back();
                set_rec->m_annot_pos = pos;
                (*member).GetTypeInfo()->DefaultSkipData(in);
            });

    x_SetBioseqSetHooks(objStream, context);

    x_SetBioseqHooks(objStream, context);

    SetLocalSkipHook(CType<CSubmit_block>(), objStream,
        [this](CObjectIStream& in, const CObjectTypeInfo& /*type*/)
    {
        auto submit_block = Ref(new CSubmit_block);
        in.Read(submit_block, CSubmit_block::GetTypeInfo(), CObjectIStream::eNoFileHeader);
        m_submit_block = submit_block;
    });


    for(auto hook: m_more_hooks)
    {
        hook(objStream);
    }

}

void CHugeAsnReader::ExtendReadHooks(t_more_hooks hooks)
{
    m_more_hooks.push_back(hooks);
}

void CHugeAsnReader::x_SetBioseqHooks(CObjectIStream& objStream, CHugeAsnReader::TContext& context)
{
    CObjectTypeInfo bioseq_info = CType<CBioseq>();

    SetLocalSkipHook(bioseq_info, objStream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        auto pos = in.GetStreamPos() + m_next_pos;

        context.bioseq_stack.push_back({});
        auto parent = context.bioseq_set_stack.back();

        type.GetTypeInfo()->DefaultSkipData(in);

        auto& bioseqinfo = context.bioseq_stack.back();
        m_bioseq_list.push_back({pos, parent, bioseqinfo.m_length, bioseqinfo.m_descr, bioseqinfo.m_ids, bioseqinfo.m_mol, bioseqinfo.m_repr});
        context.bioseq_stack.pop_back();
    });
}


void CHugeAsnReader::x_SetBioseqSetHooks(CObjectIStream& objStream, CHugeAsnReader::TContext& context)
{
    CObjectTypeInfo bioseq_set_info = CType<CBioseq_set>();

    SetLocalSkipHook(bioseq_set_info, objStream,
            [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
            {
                auto pos = in.GetStreamPos() + m_next_pos;
                auto parent = context.bioseq_set_stack.back();
                m_bioseq_set_list.push_back({pos, parent});

                auto last = prev(m_bioseq_set_list.end());

                context.bioseq_set_stack.push_back(last);
                auto pBioseqSet = Ref(new CBioseq_set());
                type.GetTypeInfo()->DefaultReadData(in, pBioseqSet);

                if (pBioseqSet->IsSetLevel()) {
                    last->m_Level = pBioseqSet->GetLevel();
                }

                last->m_class = pBioseqSet->GetClass();
                if (pBioseqSet->IsSetDescr()) {
                    last->m_descr.Reset(&(pBioseqSet->GetDescr()));
                }

                if (IsHugeSet(last->m_class) &&
                    last->HasAnnot()) {
                    m_HasHugeSetAnnot = true;
                }

                context.bioseq_set_stack.pop_back();
            });
}


void CHugeAsnReader::x_SetFeatIdHooks(CObjectIStream& objStream)
{
    SetLocalSkipHook(CType<CFeat_id>(), objStream,
        [this](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        auto pFeatId = Ref(new CFeat_id());
        type.GetTypeInfo()->DefaultReadData(in, pFeatId);
        if (pFeatId->IsLocal() && pFeatId->GetLocal().IsId())
        {
            m_max_local_id = std::max(m_max_local_id, pFeatId->GetLocal().GetId());
        }
    });

    SetLocalReadHook(CType<CFeat_id>(), objStream,
            [this](CObjectIStream& in, const CObjectInfo& object)
    {
        auto* pObject = object.GetObjectPtr();
        object.GetTypeInfo()->DefaultReadData(in, pObject);
        auto* pFeatId = CTypeConverter<CFeat_id>::SafeCast(pObject);
        if (pFeatId->IsLocal() && pFeatId->GetLocal().IsId())
        {
            m_max_local_id = std::max(m_max_local_id, pFeatId->GetLocal().GetId());
        }
    });
}

void CHugeAsnReader::ResetTopEntry()
{
    m_top_entry.Reset();
}

void CHugeAsnReader::x_IndexNextAsn1()
{
    x_ResetIndex();
    m_current_pos = m_next_pos;
    auto object_type = m_file->RecognizeContent(m_current_pos);

    auto obj_stream = m_file->MakeObjStream(m_current_pos);

    TContext context;
    x_SetHooks(*obj_stream, context);

    m_HasHugeSetAnnot = false;
    // Ensure there is at least one bioseq_set_info object exists
    obj_stream->SkipFileHeader(object_type);
    m_bioseq_set_list.push_back({ 0, m_bioseq_set_list.end() });
    context.bioseq_set_stack.push_back(m_bioseq_set_list.begin());
    obj_stream->Skip(object_type, CObjectIStream::eNoFileHeader);
    obj_stream->EndOfData(); // force to SkipWhiteSpace
    m_next_pos += obj_stream->GetStreamPos();
}

CRef<CSerialObject> CHugeAsnReader::ReadAny()
{
    if (m_current_pos >= m_file->m_filesize)
        return {};

    x_ResetIndex();
    auto object_type = m_file->RecognizeContent(m_current_pos);
    if (object_type == nullptr || !object_type->IsCObject())
        return {};

    auto obj_stream = m_file->MakeObjStream(m_current_pos);

    auto obj_info = obj_stream->Read(object_type);
    CRef<CSerialObject> serial(static_cast<CSerialObject*>(obj_info.GetObjectPtr()));
    obj_stream->EndOfData(); // force to SkipWhiteSpace
    m_current_pos += obj_stream->GetStreamPos();
    //_ASSERT(m_current_pos == m_next_pos);

    return serial;
}


CHugeAsnReader::TStreamPos CHugeAsnReader::GetCurrentPos() const
{
    return m_current_pos;
}


void CHugeAsnReader::x_ThrowDuplicateId(
    const TBioseqSetInfo& existingInfo,
    const TBioseqSetInfo& newInfo,
    const CSeq_id& duplicateId)
{
    auto filename = m_file->m_filename;
    auto existingPos = existingInfo.m_pos;
    auto newPos = newInfo.m_pos;

    auto existingFilePos = NStr::UInt8ToString(existingPos);
    auto newFilePos = NStr::UInt8ToString(newPos);
    if (!filename.empty()) {
        existingFilePos = filename + ":" + existingFilePos;
        newFilePos = filename + ":" + newFilePos;
    }
    string msg = "duplicate Bioseq id " + objects::GetLabel(duplicateId) +
        " present in the set starting at " + existingFilePos;
    if (newPos != existingPos) {
        msg += " and the set starting at " + newFilePos;
    }
    NCBI_THROW(CHugeFileException, eDuplicateSeqIds, msg);
}


const CBioseq_set::TClass* CHugeAsnReader::GetTopLevelClass() const
{
    return m_pTopLevelClass;
}

std::tuple<CRef<CSeq_descr>, std::list<CRef<CSeq_annot>>> CHugeAsnReader::x_GetTopLevelDescriptors() const
{
    CRef<CSeq_descr> pDescriptors;
    std::list<CRef<CSeq_annot>> pAnnots;
    if (GetBiosets().size() < 2) {
        return {pDescriptors, pAnnots};
    }

    CObjectTypeInfo bioseq_set_info = CType<CBioseq_set>();
    auto bioseqset_annot_mi = bioseq_set_info.FindMember("annot");

    auto top = next(GetBiosets().begin());
    if (top->m_descr) {
        pDescriptors = Ref(new CSeq_descr());
        pDescriptors->Assign(*top->m_descr);
    }

    if (top->HasAnnot()) {
        auto obj_stream = MakeObjStream(top->m_annot_pos);
        bioseqset_annot_mi.GetMemberInfo()->GetTypeInfo()->ReadData(*obj_stream, &pAnnots);
    }

    for (auto it = next(top); it != end(GetBiosets()); ++it) {
        if (!IsHugeSet(it->m_class)) {
            break;
        }

        if (it->m_descr) {
            if (!pDescriptors) {
                pDescriptors = Ref(new CSeq_descr());
                pDescriptors->Assign(*it->m_descr);
            }
            else {
                for (auto pDesc : it->m_descr->Get()) {
                    pDescriptors->Set().push_back(pDesc);
                }
            }
        }
        if (it->HasAnnot()) {
            auto obj_stream = MakeObjStream(it->m_annot_pos);
            list< CRef< CSeq_annot > > annots;
            bioseqset_annot_mi.GetMemberInfo()->GetTypeInfo()->ReadData(*obj_stream, &annots);
            if (!annots.empty()) {
                pAnnots.splice(pAnnots.end(), std::move(annots));
            }
        }
    }
    return {pDescriptors, pAnnots};
}

bool CHugeAsnReader::HasNestedGenbankSets() const
{
    if (GetBiosets().size() <= 2) {
        return false;
    }
    auto it = next(GetBiosets().begin());
    return (it->m_class == CBioseq_set::eClass_genbank &&
            next(it)->m_class == CBioseq_set::eClass_genbank);
}

bool CHugeAsnReader::HasLoneProteins() const
{
    for (auto it = m_bioseq_list.begin(); it!= m_bioseq_list.end(); ++it)
    {
        auto& rec = *it;
        if (rec.m_mol != CSeq_inst::eMol_aa)
            continue;
        auto& parent = rec.m_parent_set;
        if (parent->m_class == CBioseq_set::eClass_nuc_prot)
            continue;

        return true;
    }
    return false;
}

void CHugeAsnReader::FlattenGenbankSet()
{
    m_pTopLevelClass = nullptr;
    m_FlattenedSets.clear();
    m_top_ids.clear();
    m_FlattenedIndex.clear();

    // single bioseq not contained in set
    if (m_bioseq_list.size() == 1 && m_bioseq_set_list.size() == 1) {
        m_bioseq_set_list.begin()->m_pos = m_bioseq_list.begin()->m_pos;
    }


    for (auto it = m_bioseq_list.begin(); it!= m_bioseq_list.end(); ++it)
    {
        auto& rec = *it;
        auto& parent = rec.m_parent_set;

        if (auto _class = parent->m_class; IsHugeSet(_class))
        { // create fake bioseq_set
            m_FlattenedSets.push_back({rec.m_pos, m_bioseq_set_list.cend(), objects::CBioseq_set::eClass_not_set});
            m_top_ids.push_back(rec.m_ids.front());
        } else {

            auto grandParent = parent->m_parent_set;
            while (!IsHugeSet(grandParent->m_class)) {
                parent = grandParent;
                grandParent = grandParent->m_parent_set;
            }
            if (m_FlattenedSets.empty() || (m_FlattenedSets.back().m_pos != parent->m_pos)) {
                m_FlattenedSets.push_back(*parent);
                m_top_ids.push_back(rec.m_ids.front());
            }
        }
        auto last = --m_FlattenedSets.end();
        for (auto id: rec.m_ids) {
            m_seq_id_types.set(id->Which());
            auto existingIndex = m_FlattenedIndex.find(id);
            if (existingIndex != m_FlattenedIndex.end()) {
                x_ThrowDuplicateId(*(existingIndex->second), *last, *id);
            }
            m_FlattenedIndex[id] = last;
            m_bioseq_index[id] = it;
        }
    }

    if (GetBiosets().size()>1)
    {
        auto top = next(GetBiosets().begin());
        if (m_FlattenedSets.size() == 1) {
            // exposing the whole top entry
            if (HasNestedGenbankSets()) {
                CRef<CSeq_descr> pDescriptors;
                std::list<CRef<CSeq_annot>> pAnnots;
                std::tie(pDescriptors, pAnnots) = x_GetTopLevelDescriptors();
                auto pTopEntry = Ref(new CSeq_entry());
                if (top->m_Level) {
                    pTopEntry->SetSet().SetLevel() = top->m_Level.value();
                }
                pTopEntry->SetSet().SetClass() = top->m_class;
                if (pDescriptors) {
                    pTopEntry->SetSet().SetDescr().Assign(*pDescriptors);
                }
                if (!pAnnots.empty()) {
                    auto& dest = pTopEntry->SetAnnot();
                    dest.splice(dest.end(), std::move(pAnnots));
                }
                m_top_entry = pTopEntry;
            }
            else if (GetSubmitBlock() ||
                    top->m_Level ||
                    top->m_descr ||
                    !IsHugeSet(top->m_class)) {
                m_FlattenedSets.clear();
                m_FlattenedSets.push_back(*top);
                for (auto& it : m_FlattenedIndex) {
                    it.second = m_FlattenedSets.begin();
                }
            }
        }
        else { // m_FlattenedSets.size() > 1)
            CRef<CSeq_descr> pDescriptors;
            std::list<CRef<CSeq_annot>> pAnnots;
            std::tie(pDescriptors, pAnnots) = x_GetTopLevelDescriptors();
            if (pDescriptors || !pAnnots.empty() || top->m_Level) {
                auto top_entry = Ref(new CSeq_entry());
                if (top->m_Level) {
                    top_entry->SetSet().SetLevel() = top->m_Level.value();
                }
                top_entry->SetSet().SetClass() = top->m_class;
                if (pDescriptors) {
                    top_entry->SetSet().SetDescr(*pDescriptors);
                }
                if (!pAnnots.empty()) {
                    auto& dest = top_entry->SetAnnot();
                    dest.splice(dest.end(), std::move(pAnnots));
                }
                m_top_entry = top_entry;
            }
        }

        m_pTopLevelClass = &(next(GetBiosets().begin())->m_class);
    }

    m_Current = m_FlattenedSets.begin();
}

CConstRef<CSubmit_block> CHugeAsnReader::GetSubmitBlock() const
{
    return m_submit_block;
}

bool CHugeAsnReader::IsNotJustLocalOrGeneral() const
{
    TSeqIdTypes all = m_seq_id_types;
    all.reset(CSeq_id::e_Local);
    all.reset(CSeq_id::e_General);
    return !all.empty();
}

bool CHugeAsnReader::HasRefSeq() const
{
    return m_seq_id_types.test(CSeq_id::e_Other);
}

CRef<CSeq_entry> CHugeAsnReader::GetNextSeqEntry()
{
    if (m_Current == end(m_FlattenedSets)) {
        m_FlattenedSets.clear();
        m_Current = m_FlattenedSets.end();
        return {};
    }

    const auto addTopEntry =
        (m_FlattenedSets.size() == 1 &&
           HasNestedGenbankSets()) ?
        eAddTopEntry::yes :
        eAddTopEntry::no;

    return LoadSeqEntry(*m_Current++, addTopEntry);
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
