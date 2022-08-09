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

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include "fasta_ex.hpp"
#include "feature_table_reader.hpp"
#include "multireader.hpp"

#include <objtools/readers/objhook_lambdas.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seq/Seq_descr.hpp>

#include "message_queue.hpp"
#include <future>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(objects::edit);

namespace
{

bool CheckDescriptors(const CHugeAsnReader& asn_reader, CSeqdesc::E_Choice which)
{
    for (auto rec: asn_reader.GetBioseqs())
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
    for (auto rec: asn_reader.GetBiosets())
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

struct THugeFileWriteContext
{
    CHugeFile         file;
    CHugeAsnReader    asn_reader;
    bool              is_fasta = false;
    IHugeAsnSource*   source     = nullptr;
    CRef<CSeq_entry>  m_topentry;
    CRef<CSeq_submit> m_submit;

    std::function<CRef<CSeq_entry>()> next_entry;

    void PopulateTopEntry(bool handle_as_set, CBioseq_set::TClass classValue)
    {
        if (source->IsMultiSequence() || (is_fasta && handle_as_set))
        {
            m_topentry = Ref(new CSeq_entry);
            if (!is_fasta && asn_reader.GetTopEntry()) {
                m_topentry->Assign(*asn_reader.GetTopEntry());
            } else {
                m_topentry->SetSet().SetClass() = classValue;
            }
            if (!source->IsMultiSequence()) {
                m_topentry->SetSet().SetSeq_set().push_back(next_entry());
            }
        } else {
            m_topentry = next_entry();
        }

        m_topentry->Parentize();
    }

    void PopulateTempTopObject(CRef<CSeq_submit>& temp_submit, CRef<CSeq_entry>&temp_top, CRef<CSeq_entry> entry) const
    {
        temp_top = entry;
        if (m_submit)
        {
            temp_submit.Reset(new CSeq_submit);
            temp_submit->Assign(*m_submit);
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
        if (m_topentry->IsSet())
        {
            temp_top.Reset(new CSeq_entry);
            temp_top->Assign(*m_topentry);
            temp_top->SetSet().SetSeq_set().clear();
            temp_top->SetSet().SetSeq_set().push_back(entry);
        }
        temp_top->Parentize();
    }

};

class CGenBankAsyncWriter
{
public:
    using TFuture = std::future<CConstRef<CSeq_entry>>;
    using TPullFutureFunction = std::function<std::future<CConstRef<CSeq_entry>>(void)>;
    using TPullEntryFunction  = std::function<CConstRef<CSeq_entry>(void)>;

    CGenBankAsyncWriter(CObjectOStream* o_stream)
        : m_ostream{o_stream}
    {}
    ~CGenBankAsyncWriter()
    {}

    virtual void Write(CConstRef<CSerialObject> topobject)
    {
        *m_ostream << *topobject;
    }

    static TFuture MakeEmptyFuture(CConstRef<CSeq_entry> entry)
    {
        std::promise<CConstRef<CSeq_entry>> prom;
        std::future<CConstRef<CSeq_entry>> fut = prom.get_future();
        prom.set_value(entry);
        return fut;
    }

    virtual void Write(CConstRef<CSerialObject> topobject, TPullEntryFunction pull_next_entry)
    {
        Write(topobject, [pull_next_entry]() -> TFuture
            {
                auto entry = pull_next_entry();
                if (entry)
                    return MakeEmptyFuture(entry);
                else
                    return {};
            }
        );
    }

    virtual void Write(CConstRef<CSerialObject> topobject, TPullFutureFunction pull_next_future)
    {
        CMessageQueue<TFuture> proc_queue(5);
        auto pull_future = std::async(std::launch::async, [&proc_queue, pull_next_future, this]
            {
                try
                {
                    TFuture future_entry;
                    while ((future_entry = pull_next_future()).valid())
                    {
                        proc_queue.push_back(std::move(future_entry));
                    }
                    proc_queue.push_back({});

                }
                catch(...)
                {
                    proc_queue.clear();

                    // wrap exception into the process queue
                    std::promise<CConstRef<CSeq_entry>> exc_prom;
                    std::future<CConstRef<CSeq_entry>> fut = exc_prom.get_future();
                    exc_prom.set_exception(std::current_exception());
                    proc_queue.push_back(std::move(fut));
                }
            }
        );

        size_t bioseq_level = 0;
        auto seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");
        SetLocalWriteHook(seq_set_member.GetMemberType(), *m_ostream,
            [this, &bioseq_level, &proc_queue]
                (CObjectOStream& out, const CConstObjectInfo& object)
        {
            bioseq_level++;
            if (bioseq_level == 1)
            {
                COStreamContainer out_container(out, object.GetTypeInfo());
                while(true)
                {
                    auto entry_future = proc_queue.pop_front();
                    if (!entry_future.valid())
                        break;
                    auto entry = entry_future.get(); // this can throw an exception that was caught within the thread
                    if (entry)
                        out_container << *entry;
                    else
                        break;
                }
            } else {
                object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
            }
            bioseq_level--;
        });

        *m_ostream << *topobject;
    }

protected:
private:
    CObjectOStream* m_ostream = nullptr;
};

} // namespace

void CTbl2AsnApp::ProcessHugeFile(CNcbiOstream* output)
{
    static const set<TTypeInfo> supported_types =
    {
        CBioseq_set::GetTypeInfo(),
        CBioseq::GetTypeInfo(),
        CSeq_entry::GetTypeInfo(),
        CSeq_submit::GetTypeInfo(),
    };

    CHugeFastaReader fasta_reader(m_context);
    THugeFileWriteContext context;

    context.file.m_supported_types = &supported_types;

    try {
        context.file.Open(m_context.m_current_file);
    }
    catch (CObjReaderParseException& e) {
        auto message = e.GetMsg();
        if (message == "File format not supported") {
            context.file.m_format = CFormatGuess::eFasta;
        }
        else throw;
    }
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
            if (context.is_fasta) {
                if (descrs)
                    CTable2AsnContext::MergeSeqDescr(*entry, *descrs, false);
            } else
            if (m_context.m_gapNmin > 0) {
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
        if (!context.is_fasta)
            context.asn_reader.FlattenGenbankSet();

        m_secret_files->m_feature_table_reader->m_local_id_counter = context.asn_reader.GetMaxLocalId() + 1;

        m_context.m_huge_files_mode = context.source->IsMultiSequence();

        context.PopulateTopEntry(m_context.m_HandleAsSet, m_context.m_ClassValue);

        if (context.is_fasta && descrs)
        {
            CRef<CSeq_entry> top_set;
            //if (!context.source->IsMultiSequence() && m_context.m_HandleAsSet)
            if (context.m_topentry->IsSet() &&
                context.m_topentry->GetSet().IsSetSeq_set() &&
                !context.m_topentry->GetSet().GetSeq_set().empty())
            {
                top_set = context.m_topentry->SetSet().SetSeq_set().front();
            } else {
                top_set = context.m_topentry;
            }

            m_context.MergeSeqDescr(*top_set, *descrs, true);
        }

#ifdef TABLE2ASN_ALLOW_MT
        std::mutex proc_mutex;
        auto process_async = [&context, this, &proc_mutex](CRef<CSeq_entry> entry) -> CConstRef<CSeq_entry>
            {
                CRef<CSeq_submit> temp_submit;
                CRef<CSeq_entry> temp_top;
                context.PopulateTempTopObject(temp_submit, temp_top, entry);

                std::lock_guard<std::mutex> g(proc_mutex);
                ProcessSingleEntry(context.file.m_format, temp_submit, temp_top);
                return entry;
            };

        auto produce_next_future = [&context, process_async]() -> CGenBankAsyncWriter::TFuture
        {
            CRef<CSeq_entry> entry = context.next_entry();
            if (entry) {
                CGenBankAsyncWriter::TFuture fut = std::async(std::launch::async, process_async, entry);
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return fut;
            }
            return {};
        };
#else
        auto produce_next_entry = [&context, this]() -> CConstRef<CSeq_entry>
        {
            CRef<CSeq_entry> entry = context.next_entry();
            if (entry) {
                CRef<CSeq_submit> temp_submit;
                CRef<CSeq_entry> temp_top;
                context.PopulateTempTopObject(temp_submit, temp_top, entry);
                ProcessSingleEntry(context.file.m_format, temp_submit, temp_top);
            }
            return entry;
        };

#endif

        CConstRef<CSerialObject> topobject; // top object is used to write output, can be submit, entry, bioseq, bioseq_set

        if (m_context.m_save_bioseq_set) {
            if (context.m_topentry->IsSet())
                topobject.Reset(&context.m_topentry->SetSet());
            else
                topobject.Reset(&context.m_topentry->SetSeq());
        } else {
            if (context.source->GetSubmitBlock())
            {
                context.m_submit.Reset(new CSeq_submit);
                context.m_submit->SetSub().Assign(*context.source->GetSubmitBlock());

                context.m_submit->SetData().SetEntrys().clear();
                context.m_submit->SetData().SetEntrys().push_back(context.m_topentry);
            }
            topobject = m_context.CreateSubmitFromTemplate(context.m_topentry, context.m_submit);
        }

        unique_ptr<CObjectOStream> ostr{
            CObjectOStream::Open(m_context.m_binary_asn1_output?eSerial_AsnBinary:eSerial_AsnText, *output)};

        CGenBankAsyncWriter async_writer(ostr.get());

        if (m_context.m_huge_files_mode)
        {
            bool need_update_date = !context.is_fasta && CheckDescriptors(context.asn_reader, CSeqdesc::e_Create_date);

            ProcessTopEntry(context.file.m_format, need_update_date, context.m_submit, context.m_topentry);
            #ifdef TABLE2ASN_ALLOW_MT
                async_writer.Write(topobject, produce_next_future);
            #else
                async_writer.Write(topobject, produce_next_entry);
            #endif


        } else {
            ProcessSingleEntry(context.file.m_format, context.m_submit, context.m_topentry);
            //if (context.m_submit.Empty()) topobject = context.m_topentry;

            async_writer.Write(topobject);
        }

    }
}

END_NCBI_SCOPE
