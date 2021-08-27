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
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/remote_updater.hpp>

#if defined(NCBI_OS_UNIX) && defined(_DEBUG)
#   include <cxxabi.h>
#   define TABLE2ASN_DEBUG_EXCEPTIONS
#endif

BEGIN_NCBI_SCOPE

namespace
{

    struct THugeFileWriteContext
    {
        size_t bioseq_level = 0;
        size_t entries = 0;
        CHugeFile file;

        IHugeAsnSource* source = nullptr;

        CRef<CSeq_entry> topentry;
        CRef<CSeq_submit> submit;
        std::function<CRef<CSeq_entry>()> next_entry;
    };


}

void CTbl2AsnApp::ProcessHugeFile(CNcbiOstream* output)
{
    static set<TTypeInfo> supported_types =
    {
        CBioseq_set::GetTypeInfo(),
        CBioseq::GetTypeInfo(),
        CSeq_entry::GetTypeInfo(),
        CSeq_submit::GetTypeInfo(),
    };

    CHugeAsnReader asn_reader;
    CHugeFastaReader fasta_reader(m_context);
    THugeFileWriteContext context;

    context.file.m_supported_types = &supported_types;

    context.file.Open(m_context.m_current_file);

    CConstRef<objects::CSeq_descr> descrs;

    if (m_context.m_entry_template.NotEmpty() && m_context.m_entry_template->IsSetDescr())
        descrs.Reset(&m_context.m_entry_template->GetDescr());

    auto fasta_get_next = [this, &fasta_reader, descrs]() -> CRef<CSeq_entry>
    {
        auto entry = fasta_reader.GetNextSeqEntry();
        if (entry && descrs)
            m_context.MergeSeqDescr(*entry, *descrs, false);
        return entry;
    };

    if (context.file.m_format == CFormatGuess::eGff3) {
        list<CRef<CSeq_annot>> annots;
        m_reader->LoadGFF3Fasta(*context.file.m_stream, annots);
        m_secret_files->m_Annots.splice(m_secret_files->m_Annots.end(), annots);
        fasta_reader.Open(&context.file, m_context.m_logger);
        context.source = &fasta_reader;
        context.next_entry = fasta_get_next;
    } else
    if (context.file.m_format == CFormatGuess::eFasta) {
        fasta_reader.Open(&context.file, m_context.m_logger);
        context.source = &fasta_reader;
        context.next_entry = fasta_get_next;
    } else {
        asn_reader.Open(&context.file, m_context.m_logger);
        context.source = &asn_reader;
        context.next_entry = [this, &asn_reader]() -> CRef<CSeq_entry>
        {
            auto entry = asn_reader.GetNextSeqEntry();
            if (entry && m_context.m_gapNmin > 0)
            {
                CGapsEditor gap_edit(
                    (CSeq_gap::EType)m_context.m_gap_type,
                    m_context.m_DefaultEvidence,
                    m_context.m_GapsizeToEvidence,
                    m_context.m_gapNmin,
                    m_context.m_gap_Unknown_length);
                gap_edit.ConvertNs2Gaps(*entry);
            }
            return entry;
        };

        if (m_context.m_t) {
            string msg(
                "Template file descriptors are ignored if input is ASN.1");
            m_context.m_logger->PutError(
                *unique_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_Unset,
                        eDiag_Warning, "", 0, msg)));
        }
    }

    while (context.source->GetNextBlob())
    {
        m_secret_files->m_feature_table_reader->m_local_id_counter = asn_reader.GetMaxLocalId() + 1;

        unique_ptr<CObjectOStream> ostr{
            CObjectOStream::Open(m_context.m_binary_asn1_output?eSerial_AsnBinary:eSerial_AsnText, *output)};

        auto seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");

        CConstRef<CSerialObject> topobject;
        bool is_multi_sequence = context.source->IsMultiSequence();
        is_multi_sequence |= (context.file.m_format == CFormatGuess::eFasta) && m_context.m_HandleAsSet;

        CRef<CSeq_entry> top_set;
        if (is_multi_sequence)
        {
            context.topentry = Ref(new CSeq_entry);
            context.topentry->SetSet().SetClass(m_context.m_ClassValue);
            auto& seqset = context.topentry->SetSet().SetSeq_set();
            if (context.source->IsMultiSequence())
            {
                top_set = context.topentry;
            } else {
                seqset.push_back(context.next_entry());
                top_set = seqset.front();
            }
        } else {
            top_set = context.topentry = context.next_entry();
        }

        if (descrs)
            m_context.MergeSeqDescr(*top_set, *descrs, true);

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

        if (context.file.m_format != CFormatGuess::eFasta && top_set->IsSet())
            m_context.ApplyUpdateDate(*top_set);

        top_set.Reset();

        if (context.source->IsMultiSequence())
        {
            if (context.submit)
            {
                if (m_context.m_RemotePubLookup)
                {
                    m_context.m_remote_updater->UpdatePubReferences(*context.submit);
                }

                CCleanup cleanup(nullptr, CCleanup::eScope_UseInPlace); // RW-1070 - CCleanup::eScope_UseInPlace is essential
                cleanup.ExtendedCleanup(*context.submit, CCleanup::eClean_NoNcbiUserObjects);
            }

            bool need_report = (context.file.m_format == CFormatGuess::eFasta) && !m_context.m_HandleAsSet;
            if (need_report)
            {
                m_context.m_logger->PutError(*unique_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0,
                    "File " + m_context.m_current_file + " contains multiple sequences")));
            }

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
                        if (context.submit)
                        {
                            context.submit->SetData().SetEntrys().clear();
                            context.submit->SetData().SetEntrys().push_back(entry);
                        }
                        ProcessSingleEntry(context.file.m_format, context.submit, entry);
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
            if (context.file.m_format != CFormatGuess::eFasta && context.topentry->IsSet())
                m_context.ApplyUpdateDate(*context.topentry);

            context.entries++;
        }

        try
        {
            ostr->Write(topobject, topobject->GetThisTypeInfo());
        }
        catch(const CException& ex)
        { // ASN writer populates exception with a call stack which is not neccessary
          // we need the original exception
#           ifdef TABLE2ASN_DEBUG_EXCEPTIONS
            int status;
            char * demangled = abi::__cxa_demangle(typeid(ex).name(), 0, 0, &status);
#           endif
            // ASN.1 writer populates exception with a call stack, we need to dig out the original exception
            const CException* original = &ex;
            while (original->GetPredecessor())
            {
#               ifdef TABLE2ASN_DEBUG_EXCEPTIONS
                cerr << demangled << ":" << ex.GetMsg() << std::endl;
#               endif
                original = original->GetPredecessor();
            }
#           ifdef TABLE2ASN_DEBUG_EXCEPTIONS
            free(demangled);
#           endif

            throw *original;
        }

    }
}

END_NCBI_SCOPE
