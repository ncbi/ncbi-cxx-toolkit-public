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
    IHugeAsnSource*   source   = nullptr;
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

template<typename _token>
class TAsyncPipeline
{
public:
    using TToken  = _token;
    using TFuture = std::future<TToken>;
    using TProcessFunction  = std::function<void(TToken&)>;
    using TPullNextFunction = std::function<bool(TToken&)>;
    using TProcessingQueue  = CMessageQueue<TFuture>;
    TAsyncPipeline() {}
    ~TAsyncPipeline() {}

    void SetDepth(size_t n) {
        m_queue.Trottle().m_limit = n;
    }

    void PostData(TToken data, TProcessFunction process_func)
    {
        TFuture fut = std::async(std::launch::async, [process_func](TToken _data) -> TToken
            {
                process_func(_data);
                return _data;
            },
            data);
        m_queue.push_back(std::move(fut));
    }

    void PostException(std::exception_ptr _excp_ptr)
    {
        m_queue.clear();
        std::promise<TToken> exc_prom;
        std::future<TToken> fut = exc_prom.get_future();
        exc_prom.set_exception(_excp_ptr);
        m_queue.push_back(std::move(fut));
    }

    void Complete()
    {
        m_queue.push_back({});
    }

    auto ReceiveData(TProcessFunction consume_func)
    {
        return std::async(std::launch::async, [consume_func, this]()
            {
                while(true)
                {
                    auto token_future = x_pop_front();
                    if (!token_future.valid())
                        break;
                    auto token = token_future.get(); // this can throw an exception that was caught within the thread
                    //if (chain_func) chain_func(token);
                    if (consume_func) {
                        consume_func(token);
                    }
                }
            });
    }

protected:
    auto x_pop_front()
    {
        return m_queue.pop_front();
    }

    std::future<void> x_make_producer_task(TPullNextFunction pull_next_token, TProcessFunction process_func)
    {
        return std::async(std::launch::async, [this, pull_next_token, process_func]()
            {
                try
                {
                    TAsyncToken token;
                    while ((pull_next_token(token)))
                    {
                        PostData(token, process_func);
                    }
                    Complete();
                }
                catch(...)
                {
                    PostException(std::current_exception());
                }
            }
        );
    }

private:
    TProcessingQueue m_queue {5};
};

class CFlatFileAsyncWriter: public TAsyncPipeline<TAsyncToken>
{
public:
    using _MyBase = TAsyncPipeline<TAsyncToken>;
    CFlatFileAsyncWriter()  {}
    ~CFlatFileAsyncWriter() {}

    auto Write(std::ostream& o_stream) {
        TProcessFunction _f = [&o_stream](TAsyncToken& token)
            {
                o_stream << std::move(token.ff);
            };
        return ReceiveData(_f);
    }
private:
};

class CGenBankAsyncWriter: public TAsyncPipeline<TAsyncToken>
{
public:
    using _MyBase = TAsyncPipeline<TAsyncToken>;
    using _MyBase::TProcessFunction;
    using _MyBase::TPullNextFunction;
    using _MyBase::TToken;

    CGenBankAsyncWriter(CObjectOStream* o_stream)
        : m_ostream{o_stream}
    {}
    ~CGenBankAsyncWriter()
    {}

    void Write(CConstRef<CSerialObject> topobject)
    {
        *m_ostream << *topobject;
    }

    void WriteAsyncMT(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func)
    {
        auto pull_next_task = x_make_producer_task(pull_next_token, process_func);

        TPullNextFunction get_next_token = [this, chain_func](TToken& token) -> bool
        {
            auto token_future = x_pop_front();
            if (!token_future.valid())
                return false;
            token = token_future.get(); // this can throw an exception that was caught within the thread
            if (chain_func) {
                chain_func(token);
            }
            return true;
        };

        x_write(topobject, get_next_token);
    }

    void WriteAsyncST(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func)
    {
        auto get_next_token = [this, pull_next_token, process_func, chain_func](TToken& token) -> bool
        {
            if (!pull_next_token(token))
                return false;

            process_func(token);
            if (chain_func) {
                chain_func(token);
            }
            return true;
        };

        x_write(topobject, get_next_token);
    }

protected:

    void x_write(CConstRef<CSerialObject> topobject, TPullNextFunction get_next_token)
    {
        size_t bioseq_level = 0;
        auto seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");
        SetLocalWriteHook(seq_set_member.GetMemberType(), *m_ostream,
            [this, &bioseq_level, get_next_token]
                (CObjectOStream& out, const CConstObjectInfo& object)
        {
            bioseq_level++;
            if (bioseq_level == 1)
            {
                COStreamContainer out_container(out, object.GetTypeInfo());
                TToken token;
                while(get_next_token(token))
                {
                    auto entry = token.entry;
                    if (entry) {
                        out_container << *entry;
                    }
                }
            } else {
                object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
            }
            bioseq_level--;
        });

        *m_ostream << *topobject;
    }

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

        auto process_async = [&context, this](TAsyncToken& token)
            {
                ProcessSingleEntry(context.file.m_format, token);
            };

        std::mutex proc_mutex;
        auto make_ff_async = [this, &proc_mutex](TAsyncToken& token) {
            std::stringstream ostr;

            std::lock_guard<std::mutex> g{proc_mutex};

            MakeFlatFile(token.seh, token.submit, ostr);

            token.ff = ostr.str();
            //std::cerr << token.ff;
        };

        auto make_next_token = [&context](TAsyncToken& token) -> bool
        {
            token.entry = context.next_entry();
            if (token.entry.Empty())
                return false;
            context.PopulateTempTopObject(token.submit, token.top_entry, token.entry);
            return true;
        };

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

        CGenBankAsyncWriter::TProcessFunction chain_func;

        if (m_context.m_make_flatfile) {
            chain_func = [this](TAsyncToken& token) {
                MakeFlatFile(token.seh, token.submit, m_context.GetOstream(".gbf"));
            };
        }

        if (m_context.m_huge_files_mode)
        {
            bool need_update_date = !context.is_fasta && CheckDescriptors(context.asn_reader, CSeqdesc::e_Create_date);

            ProcessTopEntry(context.file.m_format, need_update_date, context.m_submit, context.m_topentry);
            bool allow_mt = true;
            #ifdef _DEBUG
            //allow_mt = true;
            #endif
            if (allow_mt) {
                auto ff_file = m_context.GetOstream(".gbf");
                unique_ptr<CFlatFileAsyncWriter> ff_writer;
                std::future<void> ff_task;
                if (m_context.m_make_flatfile) {
                    ff_writer.reset(new CFlatFileAsyncWriter);
                    chain_func = [&ff_writer, make_ff_async](CGenBankAsyncWriter::TToken& token) {
                        ff_writer->PostData(token, make_ff_async);
                    };
                    ff_task = ff_writer->Write(ff_file);
                }
                async_writer.WriteAsyncMT(topobject, make_next_token, process_async, chain_func);
                if (ff_writer) {
                    // signal ff writer to stop
                    ff_writer->Complete();
                }
                // now it will wait until ff_task completes
            } else {
                async_writer.WriteAsyncST(topobject, make_next_token, process_async, chain_func);
            }
        } else {
            TAsyncToken token;
            token.submit = context.m_submit;
            token.entry  = token.top_entry = context.m_topentry;
            process_async(token);
            async_writer.Write(topobject);
            if (chain_func)
                chain_func(token);
        }

    }
}

END_NCBI_SCOPE
