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

#include <serial/objostr.hpp>
#include <serial/objectio.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/util/sequence.hpp>

#include <util/message_queue.hpp>
#include <future>
#include <objtools/writers/multi_source_file.hpp>
#include <objtools/writers/async_writers.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
USING_SCOPE(objects::edit);


namespace
{

bool s_AddUpdateDescriptor(const CHugeAsnReader& asn_reader)
{
    for (auto rec: asn_reader.GetBioseqs()) {
        if (rec.m_descr) {
            if (auto parentClass = rec.m_parent_set->m_class;
                    parentClass != CBioseq_set::eClass_genbank &&
                    parentClass != CBioseq_set::eClass_not_set) {
                continue;
            }
            for (auto descr: rec.m_descr->Get()) {
                if (descr->Which() == CSeqdesc::e_Create_date) {
                    return true;
                }
            }
        }
    }
    return false;
}

/*
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
    return false;
}
*/

struct THugeFileWriteContext
{
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
class CFlatFileAsyncWriter
{
public:
    using TToken = _token;
    CFlatFileAsyncWriter()  {
        m_multi_writer.SetMaxWriters(20);
    }
    ~CFlatFileAsyncWriter() {}

    using TFFFunction = std::function<void(TToken&, std::ostream&)>;

    void Post(TAsyncToken& iotoken, TFFFunction ff_func)
    {
        auto output = m_multi_writer.NewStream();
        std::thread(
            [ff_func](TAsyncToken token, CMultiSourceOStream ostr)
        {
            try {
                ff_func(token, ostr);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what();
            }
            catch(...)
            {
                std::cerr << "unknown exception\n";
            }
        }, iotoken, std::move(output)).detach();
    }

    auto Write(std::ostream& o_stream) {
        m_multi_writer.Open(o_stream);
    }
private:
    CMultiSourceWriter m_multi_writer;
};



} // namespace

void CTbl2AsnApp::ProcessHugeFile(CHugeFile& hugeFile, CNcbiOstream* output)
{
    THugeFileWriteContext context;
    CHugeFastaReader fasta_reader(m_context);
    if (hugeFile.m_format == CFormatGuess::eGff3) {
        TAnnotMap annotMap;
        m_reader->LoadGFF3Fasta(*hugeFile.m_stream, annotMap);
        for (auto entry : annotMap) {
            auto it = m_secret_files->m_AnnotMap.find(entry.first);    
            if (it == m_secret_files->m_AnnotMap.end()) {
                m_secret_files->m_AnnotMap.emplace(entry.first, entry.second);
            }
            else {
                it->second.splice(it->second.end(), entry.second);
            }
        }

        fasta_reader.Open(&hugeFile, m_context.m_logger);
        context.source = &fasta_reader;
        context.is_fasta = true;
    } else
    if (hugeFile.m_format == CFormatGuess::eFasta) {
        fasta_reader.Open(&hugeFile, m_context.m_logger);
        context.source = &fasta_reader;
        context.is_fasta = true;
    } else {
        context.asn_reader.Open(&hugeFile, m_context.m_logger);
        context.source = &context.asn_reader;

        if (m_context.m_t) {
            string msg(
                "Template file descriptors are ignored if input is ASN.1");
            m_context.m_logger->PutError(
                *unique_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_Unset,
                        eDiag_Warning, "", 0, "", "", "", msg)));
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

        auto process_async = [&hugeFile, this](TAsyncToken& token)
            {
                ProcessSingleEntry(hugeFile.m_format, token);
            };

        std::mutex ff_mutex;
        auto make_ff_async = [this, &ff_mutex](TAsyncToken& token, std::ostream& ostr)
        {
            //std::lock_guard<std::mutex> g{ff_mutex};
            MakeFlatFile(token.seh, token.submit, ostr);
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

        using TWriter = CGenBankAsyncWriterEx<TAsyncToken>;
        TWriter async_writer(ostr.get());
        async_writer.SetDepth(10);

        TWriter::TProcessFunction ff_chain_func;

        if (m_context.m_make_flatfile) {
            ff_chain_func = [this](TAsyncToken& token) {
                MakeFlatFile(token.seh, token.submit, m_context.GetOstream(eFiles::gbf));
            };
        }

        if (m_context.m_huge_files_mode)
        {
            const size_t numThreads = m_context.m_use_threads.value_or(1);
            bool need_update_date = !context.is_fasta && s_AddUpdateDescriptor(context.asn_reader);

            ProcessTopEntry(hugeFile.m_format, need_update_date, context.m_submit, context.m_topentry);
            if (numThreads>=3) {
                std::ofstream ff_file;
                ff_file.exceptions(ios::failbit | ios::badbit);

                CFlatFileAsyncWriter<TAsyncToken> ff_writer;
                if (m_context.m_make_flatfile) {
                    ff_file.open(m_context.GenerateOutputFilename(eFiles::gbf));
                    ff_chain_func = [&ff_writer, make_ff_async](TAsyncToken& token) {
                        ff_writer.Post(token, make_ff_async);
                    };
                    ff_writer.Write(ff_file);
                }
                async_writer.WriteAsyncMT(topobject, make_next_token, process_async, ff_chain_func);
                // now it will wait until all ff_writer tasks complete
            } else
            if (numThreads == 2) {
                async_writer.WriteAsync2T(topobject, make_next_token, process_async, ff_chain_func);
            } else {
                async_writer.WriteAsyncST(topobject, make_next_token, process_async, ff_chain_func);
            }
        } else {
            TAsyncToken token;
            token.submit = context.m_submit;
            token.entry  = token.top_entry = context.m_topentry;
            process_async(token);
            async_writer.Write(topobject);
            if (ff_chain_func)
                ff_chain_func(token);
        }

    }
}

END_NCBI_SCOPE
