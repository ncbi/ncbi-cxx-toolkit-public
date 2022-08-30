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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistl.hpp>

#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/serialbase.hpp>

#include <objmgr/scope.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objtools/writers/async_writers.hpp>
#include <objtools/edit/huge_file.hpp>
#include <objtools/readers/objhook_lambdas.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE
USING_SCOPE(edit);

CGenBankAsyncWriter::CGenBankAsyncWriter(CObjectOStream* o_stream, 
        EDuplicateIdPolicy policy)
    : m_ostream{o_stream}, 
      m_DuplicateIdPolicy{policy} {}


CGenBankAsyncWriter::~CGenBankAsyncWriter() {}


void CGenBankAsyncWriter::Write(CConstRef<CSerialObject> topobject) {
    m_ostream->WriteObject(topobject, topobject->GetThisTypeInfo());
}


void CGenBankAsyncWriter::WriteAsyncMT(CConstRef<CSerialObject> topobject,
        TPullNextFunction pull_next_token,
        TProcessFunction process_func,
        TProcessFunction chain_func)
{
    auto pull_next_task = x_make_producer_task(pull_next_token, process_func);

    TPullNextFunction get_next_token = [this, chain_func](TToken& token) -> bool
    {
        auto token_future = x_pop_front();
        if (!token_future.valid()) {
            return false;
        }
        token = token_future.get(); // this can throw an exception that was caught within the thread
        if (chain_func) {
            chain_func(token);
        }
        return true;
    };

    x_write(topobject, get_next_token);
}


void CGenBankAsyncWriter::WriteAsync2T(CConstRef<CSerialObject> topobject,
            TPullNextFunction pull_next_token,
            TProcessFunction process_func,
            TProcessFunction chain_func)
{
    auto produce_tokens_task = std::async(std::launch::async, [this, pull_next_token, process_func, chain_func]()
        {
            try
            {
                TAsyncToken token;
                while ((pull_next_token(token)))
                {
                    process_func(token);
                    if (chain_func) {
                        chain_func(token);
                    }
                    std::promise<TToken> empty_promise;
                    auto fut = empty_promise.get_future();
                    empty_promise.set_value(std::move(token));
                    m_queue.push_back(std::move(fut));
                }
                Complete();
            }
            catch(...)
            {
                PostException(std::current_exception());
            }
        }
    );

    auto get_next_token = [this](TToken& token) -> bool
    {
        auto token_future = x_pop_front();
        if (!token_future.valid()) {
            return false;
        }
        token = token_future.get(); // this can throw an exception that was caught within the thread
        return true;
    };

    x_write(topobject, get_next_token);
}

void CGenBankAsyncWriter::WriteAsyncST(CConstRef<CSerialObject> topobject,
        TPullNextFunction pull_next_token,
        TProcessFunction process_func,
        TProcessFunction chain_func)
{
    auto get_next_token = [pull_next_token, process_func, chain_func](TToken& token) -> bool
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


void CGenBankAsyncWriter::x_write(CConstRef<CSerialObject> topobject, TPullNextFunction get_next_token)
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


    if (m_DuplicateIdPolicy == eIgnore) { 
        m_ostream->WriteObject(topobject, topobject->GetThisTypeInfo());
        return;
    }

    
    TIdSet processedIds, duplicateIds;
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

END_objects_SCOPE
END_NCBI_SCOPE
