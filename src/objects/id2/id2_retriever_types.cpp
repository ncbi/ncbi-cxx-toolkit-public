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
* Author:  Aaron Ucko
*
* File Description:
*   Data structures used by IID2Retriever and CID2ReplyDashboard.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/id2/impl/id2_retriever_types.hpp>
#include <objects/id2/ID2S_Request_Get_Chunks.hpp>
#include <objects/id2/ID2_Param.hpp>
#include <objects/id2/ID2_Params.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Info.hpp>
#include <objects/id2/ID2_Request_Get_Seq_id.hpp>
#include <objects/id2/ID2_Request_ReGet_Blob.hpp>
#include <objects/id2/ID2_Seq_id.hpp>
#include <objects/misc/error_codes.hpp>

/// Shared with id2_retriever.cpp and id2_reply_dashboard.cpp.
#define NCBI_USE_ERRCODE_X Objects_ID2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SID2RequestWithSynonyms::SID2RequestWithSynonyms(const CID2_Request& req_in)
    : req(&req_in), preresolved(false)
{
    typedef CID2_Request::TRequest TBody;
    const TBody& body       = req_in.GetRequest();
    auto         which      = body.Which();
    bool         recognized = false;
    switch (which) {
    case TBody::e_Init:
    case TBody::e_Get_packages:
        recognized = preresolved = true;
        break;
    case TBody::e_Get_seq_id:
        x_ExtractID(body.GetGet_seq_id(), "get-seq-id");
        recognized = true;
        break;
    case TBody::e_Get_blob_id:
        x_ExtractID(body.GetGet_blob_id().GetSeq_id(), "get-blob-id");
        recognized = true;
        break;
    case TBody::e_Get_blob_info:
        x_ExtractID(body.GetGet_blob_info());
        recognized = true;
        break;
    case TBody::e_Reget_blob:
        x_ExtractID(body.GetReget_blob().GetBlob_id());
        recognized = true;
        break;
    case TBody::e_Get_chunks:
        x_ExtractID(body.GetGet_chunks().GetBlob_id());
        recognized = true;
        break;        
    case TBody::e_not_set:
        break;
    }
    if (acc) {
        _ASSERT(recognized);
        _ASSERT(blob_id.Empty());
        ids.insert(acc);
    } else if ( !recognized ) {
        preresolved = true;
        _ASSERT(blob_id.Empty());
        ERR_POST_X(1,
                   Warning
                   << "SID2RequestWithSynonyms: Received ID2 request"
                   " with unsupported choice value "
                   << TBody::SelectionName(which) << " ("
                   << static_cast<int>(which) << ')');
    }
}


void SID2RequestWithSynonyms::x_ExtractID(const CID2_Request_Get_Seq_id& gsi,
                                          const char* choice)
{
    const auto& id    = gsi.GetSeq_id();
    auto        which = id.Which();
    switch (which) {
    case CID2_Seq_id::e_String:
        try {
            acc = CSeq_id_Handle::GetHandle(id.GetString());
        } catch (exception& e) {
            ERR_POST_X(2,
                       Warning << "SID2RequestWithSynonyms: Received ID2 "
                       << choice << " request for unparsable string "
                       << id.GetString() << ": " << e.what());
        }
        return;
    case CID2_Seq_id::e_Seq_id:
        acc = CSeq_id_Handle::GetHandle(id.GetSeq_id());
        return;
    case CID2_Seq_id::e_not_set:
        break;
    }
    ERR_POST_X(3,
               Warning << "SID2RequestWithSynonyms: Received ID2 " << choice
               << " request with unsupported ID2-Seq-id choice value "
               << CID2_Seq_id::SelectionName(which) << " ("
               << static_cast<int>(which) << ')');
}


void SID2RequestWithSynonyms::x_ExtractID(
    const CID2_Request_Get_Blob_Info& gbi)
{
    typedef CID2_Request_Get_Blob_Info::TBlob_id TBlob_id;
    const TBlob_id& id    = gbi.GetBlob_id();
    auto            which = id.Which();
    switch (which) {
    case TBlob_id::e_Blob_id:
        x_ExtractID(id.GetBlob_id());
        return;
    case TBlob_id::e_Resolve:
        x_ExtractID(id.GetResolve().GetRequest().GetSeq_id(), "get-blob-info");
        return;
    case CID2_Request_Get_Blob_Info::TBlob_id::e_not_set:
        break;
    }
    ERR_POST_X(4,
               Warning << "SID2RequestWithSynonyms: Received ID2 get-blob-info"
               " request with unsupported choice value "
               << TBlob_id::SelectionName(which) << " ("
               << static_cast<int>(which) << ')');
}


//////////////////////////////////////////////////////////////////////


SRichID2InitRequest::SRichID2InitRequest(const CID2_Request& request)
    : raw_request(&request)
{
    _ASSERT(request.GetRequest().IsInit());
    if (request.IsSetParams()) {
        for (const auto& it : request.GetParams().Get()) {
            if (it->GetName() == "id2:allow") {
                id2_allow.insert(it->GetValue().begin(), it->GetValue().end());
                break; // consolidated in practice
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////


SRichID2Packet::SRichID2Packet(const CID2_Request_Packet& main_packet,
                               const SRichID2InitRequest* init_request_in)
    : packet(&main_packet), init_request(init_request_in),
      force_reply_order(false)
{
    if (x_NeedRenumbering()) {
        CRef<CID2_Request_Packet> renumbered(new CID2_Request_Packet);
        orig_serial_numbers.resize(main_packet.Get().size());
        TIndex n = 0;
        for (const auto& it : main_packet.Get()) {
            if (it->IsSetSerial_number()) {
                orig_serial_numbers[n] = it->GetSerial_number();
            }
            CRef<CID2_Request> request(new CID2_Request);
            request->Assign(*it);
            request->SetSerial_number(++n);
            renumbered->Set().push_back(request);
        }
        packet = renumbered;
    }

    for (const auto& it : packet->Get()) {
        TIndex n = it->GetSerial_number();
        requests[n] = new SID2RequestWithSynonyms(*it);
        if (it->GetRequest().IsInit()  &&
            (init_request.Empty()  ||  it != init_request->raw_request)) {
            fresh_init.Reset(new SRichID2InitRequest(*it));
            init_request = fresh_init;
        }
    }
}


void SRichID2Packet::RestoreSerialNumber(TReplies& replies, TIndex n) const
{
    if (orig_serial_numbers.empty()) {
        _ASSERT( !force_reply_order );
        return;
    }
    _ASSERT(orig_serial_numbers.size() == packet->Get().size());
    auto n2 = orig_serial_numbers[n - 1];
    if (n2.IsNull()) {
        _ASSERT(force_reply_order);
        for (auto& it : replies) {
            _ASSERT(it->GetSerial_number() == n);
            it->ResetSerial_number();
        }
    } else {
        for (auto& it : replies) {
            _ASSERT(it->GetSerial_number() == n);
            it->SetSerial_number(n2);
        }
    }
}


bool SRichID2Packet::x_NeedRenumbering(void)
{
    set<TIndex> serial_numbers;
    bool        need_renumbering = false;
    TIndex      n = 0;
    for (const auto& it : packet->Get()) {
        if ( !it->IsSetSerial_number() ) {
            force_reply_order = true;
            return true;
        } else {
            serial_numbers.insert(it->GetSerial_number());
            if (static_cast<TIndex>(serial_numbers.size()) < ++n) {
                force_reply_order = true;
                return true;
            } else if (it->GetSerial_number() <= 0) {
                need_renumbering = true;
                // keep going -- could still trigger force_reply_order
            }
        }
    }
    return need_renumbering;
}


//////////////////////////////////////////////////////////////////////


bool SID2ReplyEvent::Matches(const SID2ReplyEvent& e) const
{
    if ((event_type & e.event_type) == eNoReply  &&  event_type != eNoReply
        &&  e.event_type != eNoReply) {
        return false;
    } else if (serial_number != e.serial_number  &&  serial_number != 0
               &&  e.serial_number != 0) {
        return false;
    } else if (retriever_id != e.retriever_id  &&  retriever_id > 0
               &&  e.retriever_id > 0) {
        return false;
    } else {
        return true;
    }
}


SID2ReplyEvent::TEventType SID2ReplyEvent::GetStrongerReplyTypes(
    TEventType event_type)
{
    TEventType result = eNoReply;
    static const EEventType kProbes[]
        = { fStrongMainReply, fWeakMainReply, fErrorReply };
    for (auto it : kProbes) {
        if ((event_type & it) == 0) {
            result |= it;
        } else {
            break;
        }
    }
    return result;
}


END_SCOPE(objects)
END_NCBI_SCOPE
