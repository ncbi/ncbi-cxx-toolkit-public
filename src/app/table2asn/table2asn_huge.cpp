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
#include <objtools/edit/remote_updater.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <util/message_queue.hpp>
#include <future>
#include <objtools/writers/multi_source_file.hpp>
#include <objtools/writers/async_writers.hpp>
#include <objtools/validator/huge_file_validator.hpp>
#include <objtools/validator/utilities.hpp>
#include "table2asn_validator.hpp"
#include "src_quals.hpp"
#include "visitors.hpp"

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
USING_SCOPE(objects::edit);
USING_SCOPE(objects::validator);


namespace
{

    bool s_AddUpdateDescriptor(const CHugeAsnReader& asn_reader)
    {
        for (auto rec : asn_reader.GetBioseqs()) {
            if (rec.m_descr) {
                if (auto parentClass = rec.m_parent_set->m_class;
                    parentClass != CBioseq_set::eClass_genbank &&
                    parentClass != CBioseq_set::eClass_not_set) {
                    continue;
                }
                for (auto descr : rec.m_descr->Get()) {
                    if (descr->Which() == CSeqdesc::e_Create_date) {
                        return true;
                    }
                }
            }
        }
        return false;
    }


    struct THugeFileWriteContext {
        CHugeAsnReader    asn_reader;
        bool              is_fasta = false;
        IHugeAsnSource*   source   = nullptr;
        CRef<CSeq_entry>  m_topentry;
        CRef<CSeq_submit> m_submit;
        atomic_bool       m_PubLookupDone{ false };
        std::mutex        m_cleanup_mutex;

        std::function<CRef<CSeq_entry>()> next_entry;

        void PopulateTopEntry(bool handle_as_set, CBioseq_set::TClass classValue)
        {
            if (source->IsMultiSequence() || (is_fasta && handle_as_set)) {
                m_topentry = Ref(new CSeq_entry);
                if (! is_fasta && asn_reader.GetTopEntry()) {
                    m_topentry->Assign(*asn_reader.GetTopEntry());
                } else {
                    m_topentry->SetSet().SetClass() = classValue;
                }
                if (! source->IsMultiSequence()) {
                    m_topentry->SetSet().SetSeq_set().push_back(next_entry());
                }
            } else {
                m_topentry = next_entry();
            }

            m_topentry->Parentize();
        }

        void PopulateTempTopObject(CRef<CSeq_submit>& temp_submit, CRef<CSeq_entry>& temp_top, CRef<CSeq_entry> entry) const
        {
            temp_top = entry;
            if (m_submit) {
                temp_submit.Reset(new CSeq_submit);
                temp_submit->Assign(*m_submit);
                if (temp_submit->IsSetData() && temp_submit->GetData().IsEntrys() &&
                    ! temp_submit->GetData().GetEntrys().empty()) {
                    temp_top = temp_submit->SetData().SetEntrys().front();
                    temp_top->SetSet().SetSeq_set().push_back(entry);
                } else {
                    temp_submit->SetData().SetEntrys().clear();
                    temp_submit->SetData().SetEntrys().push_back(entry);
                }
            } else if (m_topentry->IsSet()) {
                temp_top.Reset(new CSeq_entry);
                temp_top->Assign(*m_topentry);
                temp_top->SetSet().SetSeq_set().clear();
                temp_top->SetSet().SetSeq_set().push_back(entry);
            }
            temp_top->Parentize();
        }
    };


    template <typename _token>
    class CFlatFileAsyncWriter
    {
    public:
        using TToken = _token;
        CFlatFileAsyncWriter()
        {
            m_multi_writer.SetMaxWriters(20);
        }
        ~CFlatFileAsyncWriter() {}

        using TFFFunction = std::function<void(TToken&, std::ostream&)>;

        void Post(TAsyncToken& iotoken, TFFFunction ff_func)
        {
            auto output = m_multi_writer.NewStream();
            std::thread(
                [ff_func](TAsyncToken token, CMultiSourceOStream ostr) {
                    try {
                        ff_func(token, ostr);
                    } catch (const std::exception& e) {
                        std::cerr << e.what();
                    } catch (...) {
                        std::cerr << "unknown exception\n";
                    }
                },
                iotoken,
                std::move(output))
                .detach();
        }

        auto Write(std::ostream& o_stream)
        {
            m_multi_writer.Open(o_stream);
        }

    private:
        CMultiSourceWriter m_multi_writer;
    };


    void s_AppendValStats(const TValStats& current, TValStats& total)
    {
        for (size_t sev = 0; sev < current.size(); ++sev) {
            total[sev].total += current[sev].total;
            // process individual error codes
            const auto& individual = current[sev].individual;
            for (auto it = begin(individual); it != end(individual); ++it) {
                auto code  = it->first;
                auto count = it->second;
                total[sev].individual[code] += count;
            }
        }
    }


} // namespace


void CTbl2AsnApp::ProcessHugeFile(CHugeFile& hugeFile, CNcbiOstream* output)
{
    THugeFileWriteContext context;
    CHugeFastaReader      fasta_reader(m_context);
    if (hugeFile.m_format == CFormatGuess::eGff3) {
        TAnnotMap annotMap;
        m_reader->LoadGFF3Fasta(*hugeFile.m_stream, annotMap);
        for (auto entry : annotMap) {
            auto it = m_secret_files->m_AnnotMap.find(entry.first);
            if (it == m_secret_files->m_AnnotMap.end()) {
                m_secret_files->m_AnnotMap.emplace(entry.first, entry.second);
            } else {
                it->second.splice(it->second.end(), entry.second);
            }
        }

        fasta_reader.Open(&hugeFile, m_context.m_logger);
        context.source   = &fasta_reader;
        context.is_fasta = true;
    } else if (hugeFile.m_format == CFormatGuess::eFasta) {
        fasta_reader.Open(&hugeFile, m_context.m_logger);
        context.source   = &fasta_reader;
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
                                       eDiag_Warning,
                                       "",
                                       0,
                                       "",
                                       "",
                                       "",
                                       msg)));
        }
    }

    bool                           firstEntryInBlob = true;
    CConstRef<objects::CSeq_descr> descrs;

    if (m_context.m_entry_template.NotEmpty() && m_context.m_entry_template->IsSetDescr())
        descrs.Reset(&m_context.m_entry_template->GetDescr());

    context.next_entry = [this, &context, &descrs, &firstEntryInBlob]() -> CRef<CSeq_entry> {
        auto entry = context.source->GetNextSeqEntry();
        if (entry) {
            if (firstEntryInBlob) {
                string hugeSetId;
                int    dummyVersion;
                if (entry->IsSeq()) {
                    hugeSetId = validator::GetAccessionFromBioseq(entry->GetSeq(), &dummyVersion);
                } else {
                    hugeSetId = validator::GetAccessionFromBioseqSet(entry->GetSet(), &dummyVersion);
                }
                m_validator->GetContextPtr()->HugeSetId = hugeSetId;
                firstEntryInBlob                        = false;
            }


            if (context.is_fasta) {
                if (descrs)
                    CTable2AsnContext::MergeSeqDescr(*entry, *descrs, false);
            } else if (m_context.m_gapNmin > 0) {
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

    const bool validate = ! m_context.m_validate.empty();
    TValStats  valStats;
    while (context.source->GetNextBlob()) {
        firstEntryInBlob = true;
        if (! context.is_fasta)
            context.asn_reader.FlattenGenbankSet();

        m_secret_files->m_feature_table_reader->m_local_id_counter = context.asn_reader.GetMaxLocalId() + 1;

        m_context.m_huge_files_mode = context.source->IsMultiSequence();

        future<void> validator_writetask;
        if (validate) {
            auto mode = m_context.m_huge_files_mode ? CValidMessageHandler::EMode::Asynchronous : CValidMessageHandler::EMode::Serial;
            m_context.pValMsgHandler.reset(new CValidMessageHandler(m_context.GetOstream(eFiles::val), mode));
            if (m_context.m_huge_files_mode) {
                validator_writetask = async([this] { m_context.pValMsgHandler->Write(); });
            }
        }

        context.PopulateTopEntry(m_context.m_HandleAsSet, m_context.m_ClassValue);

        if (context.is_fasta && descrs) {
            CRef<CSeq_entry> top_set;
            if (context.m_topentry->IsSet() &&
                context.m_topentry->GetSet().IsSetSeq_set() &&
                ! context.m_topentry->GetSet().GetSeq_set().empty()) {
                top_set = context.m_topentry->SetSet().SetSeq_set().front();
            } else {
                top_set = context.m_topentry;
            }
            m_context.MergeSeqDescr(*top_set, *descrs, true);
        }

        auto process_async = [&hugeFile, this](TAsyncToken& token) {
            ProcessSingleEntry(hugeFile.m_format, token);
        };

        std::mutex ff_mutex;
        auto       make_ff_async = [this, &ff_mutex](TAsyncToken& token, std::ostream& ostr) {
            MakeFlatFile(token.seh, token.submit, ostr);
        };

        auto make_next_token = [&context](TAsyncToken& token) -> bool {
            token.entry = context.next_entry();
            if (token.entry.Empty()) {
                return false;
            }
            context.PopulateTempTopObject(token.submit, token.top_entry, token.entry);
            token.pPubLookupDone = &context.m_PubLookupDone;
            token.cleanup_mutex  = &context.m_cleanup_mutex;
            return true;
        };

        CConstRef<CSerialObject> topobject; // top object is used to write output, can be submit, entry, bioseq, bioseq_set

        if (m_context.m_save_bioseq_set) {
            if (context.m_topentry->IsSet())
                topobject.Reset(&context.m_topentry->SetSet());
            else
                topobject.Reset(&context.m_topentry->SetSeq());
        } else {
            if (context.source->GetSubmitBlock()) {
                context.m_submit.Reset(new CSeq_submit);
                context.m_submit->SetSub().Assign(*context.source->GetSubmitBlock());

                context.m_submit->SetData().SetEntrys().clear();
                context.m_submit->SetData().SetEntrys().push_back(context.m_topentry);
            }
            topobject = m_context.CreateSubmitFromTemplate(context.m_topentry, context.m_submit);
        }

        unique_ptr<CObjectOStream> ostr{
            CObjectOStream::Open(m_context.m_binary_asn1_output ? eSerial_AsnBinary : eSerial_AsnText, *output)
        };

        using TWriter = CGenBankAsyncWriterEx<TAsyncToken>;
        TWriter async_writer(ostr.get());
        async_writer.SetDepth(10);

        TWriter::TProcessFunction ff_chain_func;

        if (m_context.m_make_flatfile) {
            ff_chain_func = [this](TAsyncToken& token) {
                MakeFlatFile(token.seh, token.submit, m_context.GetOstream(eFiles::gbf));
            };
        }

        if (m_context.m_huge_files_mode) {
            const size_t numThreads       = m_context.m_use_threads.value_or(1);
            bool         need_update_date = ! context.is_fasta && s_AddUpdateDescriptor(context.asn_reader);

            ProcessTopEntry(hugeFile.m_format, need_update_date, context.m_submit, context.m_topentry);
            if (numThreads >= 3) {
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
            } else if (numThreads == 2) {
                async_writer.WriteAsync2T(topobject, make_next_token, process_async, ff_chain_func);
            } else {
                async_writer.WriteAsyncST(topobject, make_next_token, process_async, ff_chain_func);
            }
        } else {
            TAsyncToken token;
            token.submit = context.m_submit;
            token.entry = token.top_entry = context.m_topentry;
            token.pPubLookupDone          = &context.m_PubLookupDone;
            token.cleanup_mutex           = &context.m_cleanup_mutex;
            process_async(token);
            async_writer.Write(topobject);
            if (ff_chain_func)
                ff_chain_func(token);
        }

        if (m_context.pValMsgHandler) {
            if (m_context.m_huge_files_mode) {
                m_context.pValMsgHandler->RequestStop();
                validator_writetask.wait();
                auto postponed = m_context.pValMsgHandler->GetPostponed();

                g_PostprocessErrors(m_validator->GetGlobalInfo(),
                                    m_validator->GetContextPtr()->HugeSetId,
                                    postponed);

                // Add stats from postponed messages
                for (const auto& pErrItem : postponed) {
                    const auto sev = pErrItem->GetSev();
                    valStats[sev].total++;
                    valStats[sev].individual[pErrItem->GetErrIndex()]++;
                }

                for (const auto& pErrItem : postponed) {
                    g_FormatErrItem(*pErrItem, m_context.GetOstream(eFiles::val));
                }
            }

            TValStats stats = m_context.pValMsgHandler->GetStats();
            s_AppendValStats(stats, valStats);
        }
    }               // Blob processed

    if (validate) { // Format stats
        size_t totalErrors{ 0 };
        for (auto statsEntry : valStats) {
            totalErrors += statsEntry.total;
        }
        if (totalErrors > 0) {
            CNcbiOfstream statsStream;
            statsStream.exceptions(ios::failbit | ios::badbit);
            statsStream.open(m_context.GenerateOutputFilename(eFiles::stats, m_context.m_base_name));
            g_FormatValStats(valStats, totalErrors, statsStream);
        }
    }
}


void CTbl2AsnApp::ProcessSingleEntry(CFormatGuess::EFormat inputFormat, TAsyncToken& token)
{
    auto& submit = token.submit;
    auto& entry  = token.top_entry;
    auto& scope  = token.scope;
    auto& seh    = token.seh;

    scope = Ref(new CScope(*m_context.m_ObjMgr));
    scope->AddDefaults();

    /*
        for FASTA inputs 'entry' argument is:
            - always a single seq object
        for ASN1. inputs:
            - either single seq object or seq-set if it's a nuc-prot-set
        'submit' is already processed clean and may contain single entry that points to 'entry' argument,
        'submit' object is not neccessary the one going to the output
    */

    CRef<CSerialObject> obj;
    if (submit)
        obj = submit;
    else
        obj = entry;

    if (m_context.m_SetIDFromFile) {
        m_context.SetSeqId(*entry);
    }

    m_context.ApplyAccession(*entry);
    m_context.ApplyFileTracks(*entry);

    const bool readModsFromTitle =
        inputFormat == CFormatGuess::eFasta ||
        inputFormat == CFormatGuess::eAlignment;
    ProcessSecretFiles1Phase(readModsFromTitle, token);

    if (m_context.m_RemoteTaxonomyLookup) {
        m_context.m_remote_updater->UpdateOrgFromTaxon(*entry);
    } else {
        VisitAllBioseqs(*entry, CTable2AsnContext::UpdateTaxonFromTable);
    }

    m_secret_files->m_feature_table_reader->m_replacement_protein = m_secret_files->m_replacement_proteins;
    m_secret_files->m_feature_table_reader->MergeCDSFeatures(*entry, token);
    entry->Parentize();

    m_secret_files->m_feature_table_reader->MoveProteinSpecificFeats(*entry);

    if (m_secret_files->m_possible_proteins.NotEmpty())
        m_secret_files->m_feature_table_reader->AddProteins(*m_secret_files->m_possible_proteins, *entry);

    m_context.CorrectCollectionDates(*entry);

    if (m_context.m_HandleAsSet) {
        // m_secret_files->m_feature_table_reader->ConvertNucSetToSet(entry);
    }

    if ((inputFormat == CFormatGuess::eTextASN) ||
        (inputFormat == CFormatGuess::eBinaryASN)) {
        // if create-date exists apply update date
        m_context.ApplyCreateUpdateDates(*entry);
    }

    m_context.ApplyComments(*entry);

    ProcessSecretFiles2Phase(*entry);

    m_secret_files->m_feature_table_reader->MakeGapsFromFeatures(*entry);

    if (m_context.m_delay_genprodset) {
        VisitAllFeatures(*entry, [this](CSeq_feat& feature) { m_context.RenameProteinIdsQuals(feature); });
    } else {
        VisitAllFeatures(*entry, [this](CSeq_feat& feature) { m_context.RemoveProteinIdsQuals(feature); });
    }

    seh = scope->AddTopLevelSeqEntry(*entry);
    CCleanup::ConvertPubFeatsToPubDescs(seh);

    // Do not repeat expensive processing of the top-level entry; RW-2107
    if ((inputFormat != CFormatGuess::eFasta) || ! *token.pPubLookupDone) {
        if (m_context.m_RemotePubLookup) {
            m_context.m_remote_updater->UpdatePubReferences(*obj);
            *token.pPubLookupDone = true;
        }
        if (m_context.m_postprocess_pubs) {
            m_context.m_remote_updater->PostProcessPubs(*entry);
        }
    }

    if (m_context.m_cleanup.find('-') == string::npos) {
        if (token.cleanup_mutex) {
            std::lock_guard<std::mutex> g{ *token.cleanup_mutex };
            m_validator->Cleanup(submit, seh, m_context.m_cleanup);
        } else {
            m_validator->Cleanup(submit, seh, m_context.m_cleanup);
        }
    }

    // make asn.1 look nicier
    edit::SortSeqDescr(*entry);

    m_secret_files->m_feature_table_reader->ChangeDeltaProteinToRawProtein(*entry);

    m_validator->UpdateECNumbers(*entry);

    if (! m_context.m_validate.empty()) {
        _ASSERT(m_context.pValMsgHandler);
        m_validator->Validate(submit, entry, *(m_context.pValMsgHandler));
    }

    m_validator->CollectDiscrepancies(submit, seh);

    // ff generator is invoked in other places
}


void CTbl2AsnApp::ProcessSecretFiles1Phase(bool readModsFromTitle, TAsyncToken& token)
{
    auto modMergePolicy =
        m_context.m_accumulate_mods ? CModHandler::eAppendPreserve : CModHandler::ePreserve;

    g_ApplyMods(
        m_global_files.mp_src_qual_map.get(),
        m_secret_files->mp_src_qual_map.get(),
        m_context.mCommandLineMods,
        readModsFromTitle,
        m_context.m_verbose,
        modMergePolicy,
        m_logger,
        *token.top_entry);

    if (! m_context.m_huge_files_mode) {
        if (m_global_files.m_descriptors)
            m_reader->ApplyDescriptors(*token.top_entry, *m_global_files.m_descriptors);
        if (m_secret_files->m_descriptors)
            m_reader->ApplyDescriptors(*token.top_entry, *m_secret_files->m_descriptors);
    }

    if (! m_global_files.m_AnnotMap.empty() || ! m_secret_files->m_AnnotMap.empty()) {
        AddAnnots(*token.top_entry);
    }
}

END_NCBI_SCOPE
