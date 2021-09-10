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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   Converter of various files into ASN.1 format, main application function
*
*/

#include <ncbi_pch.hpp>
#include "table2asn.hpp"

#include "huge_file.hpp"
#include "huge_asn_reader.hpp"
#include "fasta_ex.hpp"
#include "feature_table_reader.hpp"
#include "multireader.hpp"

#include <objtools/readers/objhook_lambdas.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/remote_updater.hpp>

#if defined(NCBI_OS_UNIX) && defined(_DEBUG)
#   include <cxxabi.h>
#   define TABLE2ASN_DEBUG_EXCEPTIONS
#endif

BEGIN_NCBI_SCOPE

namespace
{

    class CFlattenedAsn: public CHugeAsnReader
    {
    public:
        bool IsMultiSequence() override { return m_flattened.size()>1; }
        const TBioseqSetInfo& GetTopObject()
        {
            auto top = GetBiosets().begin();
            if (GetBiosets().size()>1)
            {
                top++;
            }
            return *top;
        }
        CRef<objects::CSeq_entry> GetNextSeqEntry() override
        {
            if (m_current == m_flattened.end())
            {
                m_flattened.clear();
                m_current = m_flattened.end();
                return {};
            }
            else
            {
#ifdef _DEBUG
                if (m_current->m_parent_set != m_bioseq_set_index.end())
                {
                    if (m_current->m_parent_set->m_descr.NotEmpty())
                        std::cerr << "Has top parent: " << m_current->m_pos << m_current->m_parent_set->m_class << std::endl;
                }
#endif
                return LoadSeqEntry(*m_current++);
            }
        }

        bool GetNextBlob() override
        {
            bool ret = CHugeAsnReader::GetNextBlob();
            if (ret)
                FlattenGenbankSet();
            return ret;
        }

        bool CheckDescriptors(CSeqdesc::E_Choice which)
        {
            for (auto rec: GetBioseqs())
            {
                if (rec.m_descr.NotEmpty())
                {
                    for (auto descr: rec.m_descr->Get())
                    {
                        if (descr->Which() == which)
                            return true;
                    }
                }
            }
            /*
            for (auto rec: GetBiosets())
            {
                if (rec.m_descr.NotEmpty())
                {
                    for (auto descr: rec.m_descr->Get())
                    {
                        if (descr->Which() == which)
                            return true;
                    }
                }
            }
            */
            return false;
        }

    protected:
        void FlattenGenbankSet();

        TBioseqSetIndex m_flattened;
        TBioseqSetIndex::iterator m_current;
    };

void CFlattenedAsn::FlattenGenbankSet()
{
    m_flattened.clear();

    for (auto& rec: GetBioseqs())
    {
        auto parent = rec.m_parent_set;
        auto _class = parent->m_class;
        if ((_class == CBioseq_set::eClass_not_set) ||
            (_class == CBioseq_set::eClass_genbank))
        { // create fake bioseq_set
            m_flattened.push_back({rec.m_pos, GetBiosets().end(), objects::CBioseq_set::eClass_not_set, {} });
        } else {
            if (m_flattened.empty() || (m_flattened.back().m_pos != parent->m_pos))
                m_flattened.push_back(*parent);
        }
    }

    if ((m_flattened.size() == 1) && (GetBiosets().size()>1))
    {// exposing the whole top entry
        auto top = GetBiosets().begin();
        top++;
        if (GetSubmit().NotEmpty()
            || (top->m_class != CBioseq_set::eClass_genbank)
            || top->m_descr.NotEmpty())
        {
            m_flattened.clear();
            m_flattened.push_back(*top);
        }
    }

    m_current = m_flattened.begin();
}

struct THugeFileWriteContext
{
    size_t bioseq_level = 0;
    size_t entries      = 0;
    CBioseq_set::TClass m_ClassValue = CBioseq_set::eClass_not_set;
    bool   is_fasta     = false;
    CHugeFile file;

    IHugeAsnSource* source = nullptr;

    CFlattenedAsn asn_reader;

    CRef<CSeq_entry> topentry;
    CRef<CSeq_submit> submit;
    std::function<CRef<CSeq_entry>()> next_entry;

    void PopulateTopEntry(CRef<CSeq_entry>& top_set, bool handle_as_set, CBioseq_set::TClass classValue)
    {
        m_ClassValue = CBioseq_set::eClass_not_set;

        if (source->IsMultiSequence() || (is_fasta && handle_as_set))
        {
            topentry = Ref(new CSeq_entry);
            auto& seqset = topentry->SetSet().SetSeq_set();
            m_ClassValue = topentry->SetSet().SetClass() = classValue;
            if (!is_fasta)
            {
                auto top = asn_reader.GetTopObject();
                if (top.m_class != CBioseq_set::eClass_not_set)
                    m_ClassValue = topentry->SetSet().SetClass() = top.m_class;
                if (top.m_descr.NotEmpty() && top.m_descr->IsSet() && !top.m_descr->Get().empty())
                {
                    topentry->SetSet().SetDescr().Assign(*top.m_descr);
                }
            }
            if (source->IsMultiSequence())
            {
                top_set = topentry;
            } else {
                seqset.push_back(next_entry());
                top_set = seqset.front();
            }
        } else {
            top_set = topentry = next_entry();
        }

        topentry->Parentize();
    }

    void PopulateTempTopObject(CRef<CSeq_submit>& temp_submit, CRef<CSeq_entry>&temp_top, CRef<CSeq_entry> entry)
    {
        temp_top = entry;
        if (submit)
        {
            temp_submit.Reset(new CSeq_submit);
            temp_submit->Assign(*submit);
            if (temp_submit->IsSetData() && temp_submit->GetData().IsEntrys() &&
                !temp_submit->GetData().GetEntrys().empty())
            {
                temp_top = temp_submit->SetData().SetEntrys().front();
                temp_top->SetSet().SetSeq_set().push_back(entry);
            } else
            {
                temp_submit->SetData().SetEntrys().clear();
                temp_submit->SetData().SetEntrys().push_back(entry);
            }
        } else
        if (topentry->IsSet())
        {
            topentry->SetSet().SetSeq_set().clear();
            topentry->SetSet().SetSeq_set().push_back(entry);
            temp_top = topentry;
        }
        temp_top->Parentize();
    }

};

} // namespace

void CTbl2AsnApp::ProcessHugeFile(CNcbiOstream* output)
{
    static set<TTypeInfo> supported_types =
    {
        CBioseq_set::GetTypeInfo(),
        CBioseq::GetTypeInfo(),
        CSeq_entry::GetTypeInfo(),
        CSeq_submit::GetTypeInfo(),
    };

    CHugeFastaReader fasta_reader(m_context);
    THugeFileWriteContext context;

    context.file.m_supported_types = &supported_types;

    context.file.Open(m_context.m_current_file);

    if (context.file.m_format == CFormatGuess::eGff3) {
        list<CRef<CSeq_annot>> annots;
        m_reader->LoadGFF3Fasta(*context.file.m_stream, annots);
        m_secret_files->m_Annots.splice(m_secret_files->m_Annots.end(), annots);
        fasta_reader.Open(&context.file, m_context.m_logger);
        context.source = &fasta_reader;
        context.is_fasta = true;
    } else
    if (context.file.m_format == CFormatGuess::eFasta) {
        fasta_reader.Open(&context.file, m_context.m_logger);
        context.source = &fasta_reader;
        context.is_fasta = true;
    } else {
        context.asn_reader.Open(&context.file, m_context.m_logger);
        context.source = &context.asn_reader;

        if (m_context.m_t) {
            string msg(
                "Template file descriptors are ignored if input is ASN.1");
            m_context.m_logger->PutError(
                *unique_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_Unset,
                        eDiag_Warning, "", 0, msg)));
        }
    }

    CConstRef<objects::CSeq_descr> descrs;

    if (m_context.m_entry_template.NotEmpty() && m_context.m_entry_template->IsSetDescr())
        descrs.Reset(&m_context.m_entry_template->GetDescr());

    context.next_entry = [this, &context, &descrs ]() -> CRef<CSeq_entry>
    {
        auto entry = context.source->GetNextSeqEntry();
        if (entry)
        {
            if (context.is_fasta)
            {
                if (descrs)
                    CTable2AsnContext::MergeSeqDescr(*entry, *descrs, false);
            } else
            if (m_context.m_gapNmin > 0)
            {
                CGapsEditor gap_edit(
                    (CSeq_gap::EType)m_context.m_gap_type,
                    m_context.m_DefaultEvidence,
                    m_context.m_GapsizeToEvidence,
                    m_context.m_gapNmin,
                    m_context.m_gap_Unknown_length);
                gap_edit.ConvertNs2Gaps(*entry);
            }
        }
        return entry;
    };

    while (context.source->GetNextBlob())
    {
        m_secret_files->m_feature_table_reader->m_local_id_counter = context.asn_reader.GetMaxLocalId() + 1;

        unique_ptr<CObjectOStream> ostr{
            CObjectOStream::Open(m_context.m_binary_asn1_output?eSerial_AsnBinary:eSerial_AsnText, *output)};

        auto seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");

        CConstRef<CSerialObject> topobject;

        m_context.m_huge_files_mode = context.source->IsMultiSequence();

        CRef<CSeq_entry> top_set;
        context.PopulateTopEntry(top_set, m_context.m_HandleAsSet, m_context.m_ClassValue);

        if (m_context.m_save_bioseq_set)
        {
            if (context.topentry->IsSet())
                topobject.Reset(&context.topentry->SetSet());
            else
                topobject.Reset(&context.topentry->SetSeq());
        } else {
            if (context.source->GetSubmit())
            {
                context.submit.Reset(new CSeq_submit);
                context.submit->Assign(*context.source->GetSubmit());

                context.submit->SetData().SetEntrys().clear();
                context.submit->SetData().SetEntrys().push_back(context.topentry);
            }
            topobject = m_context.CreateSubmitFromTemplate(context.topentry, context.submit);
        }

        if (context.is_fasta && descrs)
            m_context.MergeSeqDescr(*top_set, *descrs, true);

        if (m_context.m_huge_files_mode)
        {
            bool need_update_date = !context.is_fasta && context.asn_reader.CheckDescriptors(CSeqdesc::e_Create_date);

            ProcessTopEntry(context.file.m_format, need_update_date, context.submit, context.topentry);

            SetLocalWriteHook(seq_set_member.GetMemberType(), *ostr,
            [this, &context](CObjectOStream& out, const CConstObjectInfo& object)
            {
                context.bioseq_level++;
                if (context.bioseq_level == 1)
                {
                    COStreamContainer out_container(out, object.GetTypeInfo());
                    CRef<objects::CSeq_entry> entry;
                    while ((entry = context.next_entry()))
                    {
                        CRef<CSeq_submit> temp_submit;
                        CRef<CSeq_entry> temp_top;
                        context.PopulateTempTopObject(temp_submit, temp_top, entry);
                        ProcessSingleEntry(context.file.m_format, temp_submit, temp_top);
                        if (entry) {
                            out_container << *entry;
                            context.entries++;
                        }
                    }
                } else {
                    object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
                }
                context.bioseq_level--;
            });
        } else {
            ProcessSingleEntry(context.file.m_format, context.submit, context.topentry);
            if (context.submit.Empty())
                topobject = context.topentry;

            context.entries++;
        }

        try
        {
            ostr->Write(topobject, topobject->GetThisTypeInfo());
        }
        catch(const CException& ex)
        { // ASN writer populates exception with a call stack which is not neccessary
          // we need the original exception
            // ASN.1 writer populates exception with a call stack, we need to dig out the original exception
            const CException* original = &ex;
            while (original->GetPredecessor())
            {
                original = original->GetPredecessor();
            }

            throw *original;
        }

    }
}

END_NCBI_SCOPE
