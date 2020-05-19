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
*   Interfaces for looking up data pertaining to ID2 requests.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/id2/id2_reply_dashboard.hpp>
#include <objects/id2/ID2_Reply_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Reply_Get_Seq_id.hpp>
#include <objects/misc/error_codes.hpp>

/// Shared with id2_retriever_types.cpp and id2_reply_dashboard.cpp.
#define NCBI_USE_ERRCODE_X Objects_ID2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void IID2ReplyUpdater::TidyReplies(TReplies& replies)
{
    if (replies.empty()) {
        return;
    } else if (replies.size() > 1) {
        map<CID2_Reply::TReply::E_Choice, TReplies> by_choice;
        for (const auto& reply : replies) {
            by_choice[reply->GetReply().Which()].push_back(reply);
        }
        replies.clear();
        for (auto& subreplies : by_choice) {
            replies.splice(replies.end(), subreplies.second);
        }
    }
    ERASE_ITERATE(TReplies, reply, replies) {
        if ((*reply)->GetReply().IsEmpty()  &&  !(*reply)->IsSetError()
            &&  !(*reply)->IsSetParams()  &&  !(*reply)->IsSetDiscard()
            &&  (reply != replies.begin()  ||  reply_next != replies.end())) {
            replies.erase(reply);
        } else {
            (*reply)->ResetEnd_of_reply();
            if ((*reply)->GetReply().IsGet_seq_id()) {
                auto& gsi = (*reply)->SetReply().SetGet_seq_id();
                if (reply_next == replies.end()
                    ||  !(*reply_next)->GetReply().IsGet_seq_id()) {
                    gsi.SetEnd_of_reply();
                } else {
                    gsi.ResetEnd_of_reply();
                }
            } else if ((*reply)->GetReply().IsGet_blob_id()) {
                auto& gbi = (*reply)->SetReply().SetGet_blob_id();
                if (reply_next == replies.end()
                    ||  !(*reply_next)->GetReply().IsGet_blob_id()) {
                    gbi.SetEnd_of_reply();
                } else {
                    gbi.ResetEnd_of_reply();
                }
            }
        }
    }
    replies.back()->SetEnd_of_reply();
}


bool IID2ReplyUpdater::HasOnlyErrors(const TReplies& replies)
{
    for (const auto &it : replies) {
        if ( !it->IsSetError()  ||  !it->GetReply().IsEmpty() ) {
            return false;
        }
    }
    return true;
}


//////////////////////////////////////////////////////////////////////


IID2Retriever::IID2Retriever(IID2ReplyDashboard&     dashboard,
                             IID2RetrieverFactory&   factory,
                             SRetrievalContext_Base* context)
    : m_Dashboard(&dashboard), m_Factory(&factory), m_Context(context),
      m_Packet(&dashboard.GetPacket()),
      m_RetrieverID(dashboard.RegisterRetriever(*this)),
      m_RelativeRetrieverID(context == NULL ? 0 : context->total_clones++),
      m_Progress(eUnready)
{
    for (const auto& it : m_Packet->requests) {
        m_Forecasts[it.first] = SID2ReplyEvent::eAny;
    }
}


CRef<IID2Retriever> IID2Retriever::Clone(void)
{
    auto clone = m_Factory->NewRetriever(*m_Dashboard.Lock(),
                                         &x_GetRetrievalContext());
    _ASSERT(clone->x_GetTypeIndex()      == x_GetTypeIndex());
    _ASSERT(clone->m_Dashboard           == m_Dashboard);
    _ASSERT(clone->m_Factory             == m_Factory);
    _ASSERT(clone->m_Context             == m_Context);
    _ASSERT(clone->m_RetrieverID         >  m_RetrieverID);
    _ASSERT(clone->m_RelativeRetrieverID >  m_RelativeRetrieverID);
    _ASSERT(clone->m_RetrieverID - clone->m_RelativeRetrieverID
            == m_RetrieverID - m_RelativeRetrieverID);
    const_cast<TEvents&>(clone->m_Events) = const_cast<TEvents&>(m_Events);
    for (const auto& it : m_Forecasts) {
        clone->x_UpdateForecast(it.first, it.second);
    }
    clone->m_Progress = m_Progress;
    clone->x_CloneExtras(*this);
    
    return clone;
}


IID2Retriever::ERunnability IID2Retriever::GetRunnability(void)
{
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.IsNull()) {
        return eRunnable_never;
    }
    
    ERunnability   result = eRunnable_never;
    SID2ReplyEvent event(SID2ReplyEvent::eNoReply, 0, m_RetrieverID);
    for (const auto& it : m_Forecasts) {
        event.serial_number = it.first;
        event.event_type    = it.second;
        if (event.event_type == SID2ReplyEvent::eNoReply) {
            continue;
        }
        const auto& request = m_Packet->requests.find(event.serial_number);
        _ASSERT(request != m_Packet->requests.end());
        ERunnability subresult
            = x_GetRunnability(*request->second, event.event_type);
        if (subresult == eRunnable_never) {
            _ASSERT(event.event_type == SID2ReplyEvent::eNoReply);
        } else if (event.event_type == SID2ReplyEvent::eNoReply) {
            _ASSERT(subresult == eRunnable_never);
        }
        x_UpdateForecast(event.serial_number, event.event_type);
        if (subresult > result  &&  dashboard->IsStillNeeded(event)) {
            result = subresult;
        }
    }
    return result;
}


IID2Retriever::ERunStatus IID2Retriever::Run(TRunFlags flags)
{
    _ASSERT(m_Progress < eRunning);
    // Defer possible indirect self-destruction (some actions can result
    // in removal from dashboard)
    CRef<IID2Retriever> self_ref(this);
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.Empty()) {
        m_Progress = eDone;
        return eSucceeded;
    }
    // Otherwise, keep the dashboard through x_Run -- this is a
    // non-exclusive "lock" that simply prevents premature cleanup.
    m_Progress = eRunning;
    try {
        ERunStatus status = x_Run(flags);
        switch (status) {
        case eFailed:
            MarkDone();
            break;
        case ePaused:
            _ASSERT((flags & fCanDeferSupplementaryUpdates) != 0);
            break;
        case eSucceeded:
            x_CancelRetries();
            break;
        }
        return status;
    } catch (...) {
        try {
            MarkDone();
        } NCBI_CATCH_ALL_X(5, "IID2Retriever::MarkDone")
        throw;
    }
}


CRef<IID2Retriever> IID2Retriever::GetReplacement(void)
{
    _ASSERT(m_Progress == eDone);
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.Empty()) {
        return null;
    } else {
        auto replacement = dashboard->GetReplacement(m_RetrieverID);
        if (replacement.NotEmpty()) {
            _ASSERT(replacement->m_Factory == m_Factory
                    &&  replacement->m_RetrieverID > m_RetrieverID);
        }
        return replacement;
    }
}


void IID2Retriever::MarkDone(void)
{
    if (m_Progress == eDone) {
        return;
    }
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.NotEmpty()) {
        dashboard->MarkRetrieverDone(m_RetrieverID);
        // Need to keep for use by GetReplacement, but the m_Progress
        // check should make any redundant calls safe.
        // m_Dashboard.Reset();
    }
    m_Progress = eDone;
}


void IID2Retriever::x_SubmitSynonyms(TSerialNumber serial_number,
                                     const TSeq_id_HandleSet& ids)
{
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.NotEmpty()) {
        static const TEventType kEventType = SID2ReplyEvent::fSynonymUpdate;
        auto it = m_Forecasts.find(serial_number);
        _ASSERT(it != m_Forecasts.end());
        _ASSERT((it->second & kEventType) == kEventType);
        SID2ReplyEvent event(kEventType, serial_number, m_RetrieverID);
        dashboard->AcceptSynonyms(event, ids);
        // x_UpdateForecast(serial_number, it->second & ~kEventType);
        it->second &= ~kEventType;
    }
}


void IID2Retriever::x_SubmitReplyUpdater(IID2ReplyUpdater& updater)
{
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.NotEmpty()) {
        const auto& metadata = updater.GetEventMetadata();
        _ASSERT(metadata.retriever_id == m_RetrieverID);
        auto it = m_Forecasts.find(metadata.serial_number);
        _ASSERT(it != m_Forecasts.end());
        _ASSERT((it->second & metadata.event_type) == metadata.event_type);
        dashboard->AcceptReplyUpdater(updater);
        // x_UpdateForecast(serial_number,
        //                  it->second & ~SID2ReplyEvent::eAnyReply);
        it->second &= ~SID2ReplyEvent::eAnyReply;
    }
}


void IID2Retriever::x_UpdateForecast(TSerialNumber serial_number,
                                     TEventType forecast)
{
    auto it = m_Forecasts.find(serial_number);
    _ASSERT(it != m_Forecasts.end());
    _ASSERT((forecast & it->second) == forecast);
    if (forecast != it->second) {
        it->second = forecast;
        auto dashboard = m_Dashboard.Lock();
        if (dashboard.NotEmpty()) {
            SID2ReplyEvent event(forecast, serial_number, m_RetrieverID);
            dashboard->AcceptStatusForecast(event);
        }
    }
}


bool IID2Retriever::x_IsStillNeededHere(TSerialNumber serial_number,
                                        TEventType event_type) const
{
    if (m_Progress == eDone) {
        return false;
    }
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.Empty()) {
        return false;
    } else {
#ifdef _DEBUG
        auto it = m_Forecasts.find(serial_number);
        _ASSERT(it != m_Forecasts.end());
        _ASSERT((it->second & event_type) == event_type);
#endif        
        SID2ReplyEvent event(event_type, serial_number, m_RetrieverID);
        return dashboard->IsStillNeeded(event);
    }
}


bool IID2Retriever::x_IsStillPlausibleElsewhere(TSerialNumber serial_number,
                                                TEventType event_type) const
{
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.Empty()) {
        return false;
    } else {
        SID2ReplyEvent event(event_type, serial_number, -m_RetrieverID);
        return dashboard->IsStillPlausible(event);
    }
}


void IID2Retriever::x_CancelRetries(void)
{
    auto dashboard = m_Dashboard.Lock();
    if (dashboard.Empty()) {
        m_Progress = eDone;
    } else {
        dashboard->CancelRetries(m_RetrieverID);
    }
}


bool IID2Retriever::x_CheckForEvents(TEvents& events,
                                     TSerialNumber serial_number,
                                     TEventType type, TCFEFlags flags)
{
    SID2ReplyEvent target(type, serial_number, -m_RetrieverID);
    bool found = x_CheckForEvents(events, target, flags);
    if ( !found  &&  (flags & fCFE_Wait) != 0) {
        auto dashboard = m_Dashboard.Lock();
        if (dashboard.Empty()) {
            return false;
        }
        dashboard->WaitForMatchingEvent(target);
        // WaitForMatchingEvent doesn't return anything, and even if
        // it did, it would still be best to recheck unconditionally
        // to avoid possible race-induced false negatives.
        found = x_CheckForEvents(events, target, flags);
    }
    return found;
}


bool IID2Retriever::x_CheckForEvents(TEvents& events,
                                     const SID2ReplyEvent& target,
                                     TCFEFlags flags)
{
    bool found = false;
    CFastMutexGuard guard(m_EventsMutex);
    TEvents& nv_events = const_cast<TEvents&>(m_Events);
    ERASE_ITERATE(TEvents, it, nv_events) {
        if ((*it)->Matches(target)) {
            events.push_back(*it);
            found = true;
            if ((flags & fCFE_KeepEvents) == 0) {
                nv_events.erase(it);
            }
        }
    }
    return found;
}


void IID2Retriever::x_CheckRunnability(void)
{
    _ASSERT(m_Progress == eUnready);
    switch (GetRunnability()) {
    case eRunnable_never:
        x_CancelRetries();
        break;
    case eRunnable_maybe_later:
        break;
    case eRunnable_now:
        m_Progress = eReady;
        break;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
