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
*/

#include <ncbi_pch.hpp>

#include <objtools/writers/async_writers.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/readers/objhook_lambdas.hpp>

#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(objects)
USING_SCOPE(edit);

CGenBankAsyncWriter::CGenBankAsyncWriter(CObjectOStream* o_stream,
        EDuplicateIdPolicy policy)
    : m_ostream{o_stream},
      m_DuplicateIdPolicy{policy} {}


CGenBankAsyncWriter::~CGenBankAsyncWriter() {}


void CGenBankAsyncWriter::Write(CConstRef<CSerialObject> topobject) {
    m_ostream->WriteObject(topobject, topobject->GetThisTypeInfo());
}

void CGenBankAsyncWriter::StartWriter(CConstRef<CSerialObject> topobject)
{
    auto next_function = [this]()
    {
        return m_write_queue.pop_front();
    };

    m_writer_task = std::async(std::launch::async, [this, next_function](CConstRef<CSerialObject> topobject)
    {
        Write(topobject, next_function);
    }, std::move(topobject));
}

void CGenBankAsyncWriter::PushNextEntry(CConstRef<CSeq_entry> entry)
{
    m_write_queue.push_back(std::move(entry));
}

void CGenBankAsyncWriter::FinishWriter()
{
    m_write_queue.push_back({});
    m_writer_task.wait();
}

void CGenBankAsyncWriter::CancelWriter()
{
    m_write_queue.clear();
    m_write_queue.push_back({});
    m_writer_task.wait();
}

using TIdSet = set<CRef<CSeq_id>, PPtrLess<CRef<CSeq_id>>>;
static void s_ReportDuplicateIds(const TIdSet& duplicateIds)
{
    if (duplicateIds.empty()) {
        return;
    }
    string msg = "duplicate Bioseq id";
    if (duplicateIds.size() > 1) {
        msg += "s";
    }
    for (auto pId : duplicateIds) {
        msg += "\n";
        msg += GetLabel(*pId);
    }
    NCBI_THROW(CHugeFileException, eDuplicateSeqIds, msg);
}


void CGenBankAsyncWriter::Write(CConstRef<CSerialObject> topobject, TGetNextFunction get_next_entry)
{
    size_t bioseq_level = 0;
    auto seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");
    SetLocalWriteHook(seq_set_member.GetMemberType(), *m_ostream,
        [this, &bioseq_level, get_next_entry]
            (CObjectOStream& out, const CConstObjectInfo& object)
    {
        bioseq_level++;
        if (bioseq_level == 1)
        {
            COStreamContainer out_container(out, object.GetTypeInfo());
            CConstRef<CSeq_entry> entry;
            while((entry = get_next_entry()))
            {
                if (entry)
                    out_container << *entry;
                entry.Reset();
            }
        } else {
            object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
        }
        bioseq_level--;
    });


    TIdSet processedIds, duplicateIds;
    if (m_DuplicateIdPolicy != eIgnore)
    {
        SetLocalWriteHook(CObjectTypeInfo(CType<CBioseq>()).FindMember("id"),
                *m_ostream,
                [&processedIds, &duplicateIds, this](CObjectOStream& out, const CConstObjectInfoMI& member)
                {
                    out.WriteClassMember(member);
                    const auto& container = *CType<CBioseq::TId>::GetUnchecked(*member);
                    for (auto pId : container) {
                        if (!processedIds.insert(pId).second) {
                            if (m_DuplicateIdPolicy == eThrowImmediately) {
                                s_ReportDuplicateIds({pId});
                            }
                            else {
                                duplicateIds.insert(pId);
                            }
                        }
                    }
                });
    }

    m_ostream->WriteObject(topobject, topobject->GetThisTypeInfo());
    s_ReportDuplicateIds(duplicateIds);
}

END_SCOPE(objects)
END_SCOPE(ncbi)
