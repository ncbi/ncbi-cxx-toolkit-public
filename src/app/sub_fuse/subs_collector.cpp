/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <serial/objistr.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include "subs_collector.hpp"

/////////////////////////////////////////////////
// CSubmissionCollector

namespace subfuse
{

using namespace std;
USING_NCBI_SCOPE;
USING_SCOPE(objects);

CSubmissionCollector::CSubmissionCollector(CNcbiOstream& out) :
    m_out(out),
    m_header_not_set(true),
    m_cur_offset(0),
    m_cur_max_id(0)
{}

static void StartWriting(CObjectOStream& out, const CSeq_submit& seq_submit)
{
    const CTypeInfo* seq_submit_type_info = seq_submit.GetThisTypeInfo();
    out.WriteFileHeader(seq_submit_type_info);

    const CClassTypeInfo* seq_submit_class_type =
        CTypeConverter<CClassTypeInfo>::SafeCast(seq_submit_type_info);

    out.BeginClass(seq_submit_class_type);

    out.BeginClassMember(seq_submit_class_type->GetMemberInfo("sub")->GetId());
    out.WriteObject(&seq_submit.GetSub(), seq_submit.GetSub().GetThisTypeInfo());
    out.EndClassMember();

    // start writing 'data'
    const CMemberInfo* data_info = seq_submit_class_type->GetMemberInfo("data");
    out.BeginClassMember(data_info->GetId());

    const CPointerTypeInfo* data_pointer_type = CTypeConverter<CPointerTypeInfo>::SafeCast(data_info->GetTypeInfo());
    const CChoiceTypeInfo* data_choice_type = CTypeConverter<CChoiceTypeInfo>::SafeCast(data_pointer_type->GetPointedType());

    out.PushFrame(CObjectStackFrame::eFrameChoice, data_choice_type, &data_info->GetId());
    out.BeginChoice(data_choice_type);

    // start writing 'entrys'
    const CVariantInfo* data_variant_info = data_choice_type->GetVariantInfo("entrys");

    out.PushFrame(CObjectStackFrame::eFrameChoiceVariant, data_variant_info->GetId());
    out.BeginChoiceVariant(data_choice_type, data_variant_info->GetId());

    const CContainerTypeInfo* data_container_type = CTypeConverter<CContainerTypeInfo>::SafeCast(data_variant_info->GetTypeInfo());
    out.BeginContainer(data_container_type);

    // start writing 'set'
    CObjectTypeInfo entry_type_info = CType<CSeq_entry>();

    out.BeginContainerElement(entry_type_info.GetTypeInfo()); // begins the next seq-entry

    const CChoiceTypeInfo* entry_choice_type = entry_type_info.GetChoiceTypeInfo();
    out.BeginChoice(entry_choice_type);

    // start writing entry 'bioseq-set' of 'genbank' class
    const CVariantInfo* entry_variant_info = entry_choice_type->GetVariantInfo("set");
    out.BeginChoiceVariant(entry_choice_type, entry_variant_info->GetId());

    const CClassTypeInfo* entry_class_type = CTypeConverter<CClassTypeInfo>::SafeCast(entry_variant_info->GetTypeInfo());
    out.BeginClass(entry_class_type);

    // writing class 'genbank'
    CObjectTypeInfo set_type_info = CType<CBioseq_set>();

    const CMemberInfo* set_class_info = set_type_info.FindMember("class").GetMemberInfo();
    CBioseq_set::EClass set_class = CBioseq_set::eClass_genbank;

    out.WriteClassMember(set_class_info->GetId(), set_class_info->GetTypeInfo(), &set_class);

    // start writing 'entrys' inside genbank bioseq-set
    const CMemberInfo* seqset_class_info = set_type_info.FindMember("seq-set").GetMemberInfo();
    out.BeginClassMember(seqset_class_info->GetId());

    const CContainerTypeInfo* seqset_container_type = CTypeConverter<CContainerTypeInfo>::SafeCast(seqset_class_info->GetTypeInfo());
    out.BeginContainer(seqset_container_type);
}

void CSubmissionCollector::AdjustLocalIds(CSeq_entry& entry)
{
    if (entry.IsSet()) {

        if (entry.SetSet().IsSetSeq_set()) {
            for (auto cur_entry : entry.SetSet().SetSeq_set()) {
                AdjustLocalIds(*cur_entry);
            }
        }
    }

    if (entry.IsSetAnnot()) {

        for (auto annot : entry.SetAnnot()) {
            if (annot->IsFtable()) {
                AdjustLocalIds(*annot);
            }
        }
    }
}

void CSubmissionCollector::AdjustLocalIds(CSeq_annot& annot)
{
    _ASSERT(annot.IsFtable() && "annot should be a feature table at this point");

    if (annot.IsSetData()) {

        for (auto feat : annot.SetData().SetFtable()) {

            AdjustLocalIds(*feat);
        }
    }
}

void CSubmissionCollector::AdjustLocalIds(CSeq_feat& feat)
{
    if (feat.IsSetId() && feat.GetId().IsLocal() && feat.GetId().GetLocal().IsId()) {

        CObject_id& obj_id = feat.SetId().SetLocal();

        int id = obj_id.GetId();
        obj_id.SetId(id + m_cur_offset);

        if (id > m_cur_max_id) {
            m_cur_max_id = id;
        }
    }

    if (feat.IsSetXref()) {

        for (auto xref : feat.SetXref()) {

            if (xref->IsSetId() && xref->GetId().IsLocal() && xref->GetId().GetLocal().IsId()) {

                CObject_id& obj_id = xref->SetId().SetLocal();

                int id = obj_id.GetId();
                obj_id.SetId(id + m_cur_offset);
            }
        }
    }
}

static void WriteContainerElement(CObjectOStream& out, const CSeq_entry& entry)
{
    out.BeginContainerElement(entry.GetThisTypeInfo());
    out.WriteObject(&entry, entry.GetThisTypeInfo());
    out.EndContainerElement();
}

bool CSubmissionCollector::ProcessFile(const string& name)
{
    CNcbiIfstream in(name.c_str());
    unique_ptr<CObjectIStream> obj_in(CObjectIStream::Open(eSerial_AsnText, in, eNoOwnership));

    bool process = true;
    while (process) {

        CSeq_submit seq_submit;

        // Get seq-submit to validate
        try {

            string header = obj_in->ReadFileHeader();

            if (header != "Seq-submit") {
                return false;
            }
            obj_in->Read(ObjectInfo(seq_submit), CObjectIStream::eNoFileHeader);
        }
        catch (CEofException& ) {
            process = false;
            continue;
        }
        catch (CException&) {
            return false;
        }

        if (m_header_not_set) {

            StartWriting(m_out, seq_submit);
            m_header_not_set = false;
        }

        CCleanup cleanup;

        if (seq_submit.GetData().IsEntrys()) {
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, entry, seq_submit.SetData().SetEntrys()) {

                m_cur_max_id = 0;
                if ((*entry)->IsSet() && (*entry)->GetSet().IsSetClass() && (*entry)->GetSet().GetClass() == CBioseq_set::eClass_genbank) {

                    if ((*entry)->GetSet().IsSetSeq_set()) {
                        NON_CONST_ITERATE(CBioseq_set::TSeq_set, internal_entry, (*entry)->SetSet().SetSeq_set()) {

                            AdjustLocalIds(**internal_entry);
                            cleanup.BasicCleanup(**internal_entry);
                            WriteContainerElement(m_out, **internal_entry);
                        }
                    }
                }
                else {
                    AdjustLocalIds(**entry);
                    cleanup.BasicCleanup(**entry);
                    WriteContainerElement(m_out, **entry);
                }

                m_cur_offset += m_cur_max_id;
            }
        }
    }

    return true;
}

bool CSubmissionCollector::CompleteProcessing()
{
    CObjectOStream& out = m_out;

    out.EndContainer(); // seq-set contains entries
    out.EndClassMember(); // seq-set

    out.EndClass(); // set
    out.EndChoiceVariant(); // set
    out.EndChoice(); // set
    out.EndContainerElement(); // set

    out.EndContainer(); // data contains entries
    out.EndChoiceVariant(); // entries
    out.EndChoice(); // entries

    out.EndClassMember(); // data
    out.EndClass(); // seq_submit

    return true;
}

}