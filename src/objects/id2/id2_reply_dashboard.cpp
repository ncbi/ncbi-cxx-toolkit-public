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
*   Class for tracking retrieval results (reply updaters) for an ID2
*   packet across retrievers and serial numbers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/id2/id2_reply_dashboard.hpp>

#include <corelib/ncbiutil.hpp>
#include <objects/id2/ID2_Error.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static
CRef<IID2ReplyUpdater> s_MakeInitUpdater(const CID2_Request& req)
{
    CRef<SID2ReplyEvent> metadata
        (new SID2ReplyEvent(SID2ReplyEvent::fStrongMainReply, 0, 0));
    IID2ReplyUpdater::TReplies replies;
    replies.emplace_back(new CID2_Reply);
    auto& reply = *replies.back();
    reply.SetReply().SetInit();
    if (req.IsSetSerial_number()) {
        auto n = req.GetSerial_number();
        reply.SetSerial_number(n);
        metadata->serial_number = n;
    }
    reply.SetEnd_of_reply();
    return CRef<IID2ReplyUpdater>(new CID2ReplyAppender(*metadata, replies));
}


CID2ReplyDashboard::CID2ReplyDashboard(SRichID2Packet& packet)
    : IID2ReplyDashboard(packet), m_RetrieverRecords(1),
      m_OutstandingRequestCount(packet.packet->Get().size()), m_MajorEvents(0),
      m_Semaphore(0, 1)
{
    _TRACE("Dashboard " << this << " initialized for " << MSerial_FlatAsnText
           << packet.packet);
    for (const auto& it : packet.requests) {
        CRef<SRequestRecord> rqr(new SRequestRecord);
        rqr->forecasts.push_back(SID2ReplyEvent::eNoReply);
        if (it.second->req->GetRequest().IsInit()) {
            // Synthesize init replies.  (Retrievers will still get to see
            // the requests via UseFactory's call to PreparseInitRequest.)
            rqr->main_updater = s_MakeInitUpdater(*it.second->req);
            if ( !packet.force_reply_order  ||  m_RequestRecords.empty()
                ||  m_RequestRecords.end()->second->state != eRRS_incomplete) {
                m_ReadyToSend.push(it.first);
                rqr->state = eRRS_queued;
            } else {
                rqr->state = eRRS_queueable;
            }
        } else {
            rqr->state = eRRS_incomplete;
        }
        rqr->had_synonym_updates = it.second->preresolved;
        m_RequestRecords.insert(make_pair(it.first, rqr));
    }
}


CID2ReplyDashboard::~CID2ReplyDashboard(void)
{
    _TRACE("Dashboard " << this << " destroyed");
    CWriteLockGuard guard(m_RWLock);
    for (auto& it : m_RetrieverRecords) {
        if (it.Empty()) {
            _ASSERT(&it == &m_RetrieverRecords.front());
        } else {
            _ASSERT(&it != &m_RetrieverRecords.front());
            if (it->retriever.NotEmpty()) {
                it->retriever->Cancel();
            }
        }
    }
}

void CID2ReplyDashboard::UseFactory(IID2RetrieverFactory& factory,
                                    unsigned int max_tries)
{
    _TRACE("Dashboard " << this << " using factory " << factory.GetName()
           << " for retriever(s) #" << m_RetrieverRecords.size() << "-#"
           << (m_RetrieverRecords.size() + max_tries - 1));
    _ASSERT(max_tries > 0);
    unsigned int factory_id
        = static_cast<unsigned int>(m_FactoryRecords.size());
    unsigned int next_retriever_id
        = static_cast<unsigned int>(m_RetrieverRecords.size());
    m_FactoryRecords.emplace_back
        (new SFactoryRecord(factory, next_retriever_id, max_tries));
    CRef<IID2Retriever> first_retriever = factory.NewRetriever(*this);
    m_RetrieverRecords.back()->factory_id = factory_id;
    for (unsigned int i = 1;  i < max_tries;  ++i) {
        CRef<IID2Retriever> retriever = first_retriever->Clone();
        m_RetrieverRecords.back()->factory_id = factory_id;
    }
    if (GetPacket().fresh_init.NotEmpty()) {
        m_RetrieverRecords.back()->retriever
            ->PreparseInitRequest(*x_UpdatePacket().fresh_init);
    }
}


CID2ReplyDashboard::TMajorEvents CID2ReplyDashboard::WaitForMajorEvent(
    const CTimeout& timeout, ETimeoutType timeout_type)
{
    _TRACE("Dashboard " << this << " waiting for major event");
    {{
        CWriteLockGuard guard(m_RWLock);
        if ( !x_HasMajorEvent() ) {
            x_NotifyOfMajorEvents();
        }
        if (x_HasMajorEvent()) {
            _TRACE("... no waiting needed: "
                   << const_cast<const TMajorEvents&>(m_MajorEvents));
            m_Semaphore.TryWait(CTimeout::eZero);
            return const_cast<const TMajorEvents&>(m_MajorEvents);
        }
    }}
    if (m_Semaphore.TryWait(timeout)) {
        _TRACE("... beat timeout with "
            << const_cast<const TMajorEvents&>(m_MajorEvents));
        return const_cast<const TMajorEvents&>(m_MajorEvents);
    }
    _TRACE("... hit timeout");
    
    CWriteLockGuard guard(m_RWLock);
    TMajorEvents events
        = const_cast<const TMajorEvents&>(m_MajorEvents) | fTimeout;
    if (timeout_type == eHardTimeout) {
        for (auto& it : m_RetrieverRecords) {
            if (it.Empty()) {
                _ASSERT(&it == &m_RetrieverRecords.front());
            } else {
                _ASSERT(&it != &m_RetrieverRecords.front());
                if (it->retriever.NotEmpty()) {
                    it->retriever->Cancel();
                }
            }
        }
        events &= ~fRetrieverReady;
        events |= fFullyReplied;
    } else { // soft timeout; allow asynchronous retries
        for (auto& it : m_FactoryRecords) {
            if (it->progress == IID2Retriever::eRunning) {
                it->progress = IID2Retriever::eReady;
                events |= fRetrieverReady;
            }
        }
    }
    return events;
}


void CID2ReplyDashboard::GetAvailableReplies(TReplies& replies)
{
    CWriteLockGuard guard(m_RWLock);
    const_cast<TMajorEvents&>(m_MajorEvents) &= ~fReplyAvailable;
    _TRACE("Dashboard " << this << " returning " << m_ReadyToSend.size()
           << " reply set(s)");
    while ( !m_ReadyToSend.empty() ) {
        auto n = m_ReadyToSend.front();
        m_ReadyToSend.pop();
        TReplies subreplies;
        SRequestRecord& req = *m_RequestRecords.find(n)->second;
        _ASSERT(req.state == eRRS_queued);
        if (req.main_updater.Empty()) {
            subreplies.push_back(x_NewErrorReply(n));
        } else {
            req.main_updater->UpdateReplies(subreplies);
        }
        for (auto& it : req.supplementary_updaters) {
            it->UpdateReplies(subreplies);
        }
        _TRACE("... for request #" << n << ": " << subreplies.size()
               << " reply/ies");
        x_UpdatePacket().RestoreSerialNumber(subreplies, n);
        replies.splice(replies.end(), subreplies);
        req.state = eRRS_sent;
        --m_OutstandingRequestCount;
    }
    _TRACE("... " << replies.size() << " total reply/ies");
}   


void CID2ReplyDashboard::GetReadyRetrievers(TRetrievers& retrievers)
{
    _TRACE("Dashboard " << this << " checking for ready retrievers");
    CWriteLockGuard guard(m_RWLock);
    const_cast<TMajorEvents&>(m_MajorEvents) &= ~fRetrieverReady;
    for (auto& it : m_FactoryRecords) {
        CRef<IID2Retriever> retriever
            = x_GetRetriever(*it, IID2Retriever::eReady);
        if (retriever.NotEmpty()) {
            retrievers.push_back(retriever);
        }
    }
    _TRACE("... found " << retrievers.size());
}


int CID2ReplyDashboard::RegisterRetriever(IID2Retriever& retriever)
{
    _ASSERT(&retriever.GetFactory()
            == m_FactoryRecords.back()->factory.GetPointerOrNull());
    size_t n = m_RetrieverRecords.size();
    _TRACE("Dashboard " << this << " registering retriever #" << n << ": "
           << &retriever);
    m_RetrieverRecords.emplace_back(new SRetrieverRecord(retriever));
    for (auto& it : m_RequestRecords) {
        auto& forecasts = it.second->forecasts;
        _ASSERT(forecasts.size() == n);
        forecasts.push_back(SID2ReplyEvent::eAny);
        if (n == 1) {
            forecasts[0] = SID2ReplyEvent::eAny;
        }
    }
    return static_cast<int>(n);
}


CRef<IID2Retriever> CID2ReplyDashboard::GetReplacement(int retriever_id)
{
    _TRACE("Dashboard " << this << " identifying replacement for retriever #"
           << retriever_id);
    _ASSERT(retriever_id > 0);
    _ASSERT(retriever_id < static_cast<int>(m_RetrieverRecords.size()));
    CWriteLockGuard guard(m_RWLock);
    unsigned int factory_id = m_RetrieverRecords[retriever_id]->factory_id;
    return x_GetRetriever(*m_FactoryRecords[factory_id],
                          IID2Retriever::eRunning);
}


void CID2ReplyDashboard::MarkRetrieverDone(int retriever_id)
{
    _TRACE("Dashboard " << this << " marking retriever #" << retriever_id
           << " done");
    _ASSERT(retriever_id > 0);
    _ASSERT(retriever_id < static_cast<int>(m_RetrieverRecords.size()));
    CWriteLockGuard guard(m_RWLock);
    auto& record = *m_RetrieverRecords[retriever_id];
    _ASSERT(record.retriever.NotEmpty());
    record.retriever.Reset();
    if (record.waiting_for.event_type != SID2ReplyEvent::eNoReply) {
        record.waiting_for.event_type = SID2ReplyEvent::eNoReply;
        record.semaphore.Post();
    }
    bool seen_incomplete = false;
    for (auto& it : m_RequestRecords) {
        bool queueable = false;
        if ( !seen_incomplete  &&  it.second->state == eRRS_queueable) {
            _ASSERT(GetPacket().force_reply_order);
            queueable = true;
        } else if (it.second->state == eRRS_incomplete) {
            auto& forecast = it.second->forecasts[retriever_id];
            if (forecast == SID2ReplyEvent::eNoReply) {
                seen_incomplete = true;
                continue;
            }
            forecast = SID2ReplyEvent::eNoReply;
            it.second->UpdateForecastSummary();
            if (it.second->IsStillIncomplete()) {
                seen_incomplete = true;
            } else {
                queueable = true;
            }
        }
        if (queueable) {
            if (seen_incomplete  &&  GetPacket().force_reply_order) {
                it.second->state = eRRS_queueable;
            } else {
                m_ReadyToSend.push(it.first);
                it.second->state = eRRS_queued;
            }
        }
    }
    for (auto& it : m_RetrieverRecords) {
        if (it.Empty()) {
            _ASSERT(&it == &m_RetrieverRecords.front());
        } else {
            _ASSERT(&it != &m_RetrieverRecords.front());
            if (it->waiting_for.event_type != SID2ReplyEvent::eNoReply
                &&  (it->waiting_for.retriever_id == retriever_id
                     ||  (it->waiting_for.retriever_id <= 0
                          &&  !IsStillPlausible(it->waiting_for) ))) {
                // Defuse to avoid overflowing (and throwing) if a second
                // retriever completes before the target wakes up.
                it->waiting_for.event_type = SID2ReplyEvent::eNoReply;
                it->semaphore.Post();
            }
        }
    }
    x_NotifyOfMajorEvents();
}


void CID2ReplyDashboard::CancelRetries(int retriever_id)
{
    _ASSERT(retriever_id > 0);
    _ASSERT(retriever_id < static_cast<int>(m_RetrieverRecords.size()));
    CWriteLockGuard guard(m_RWLock);
    auto& fr = m_FactoryRecords[m_RetrieverRecords[retriever_id]->factory_id];
    _TRACE("Dashboard " << this << " canceling retries for "
           << fr->factory->GetName() << " per retriever #" << retriever_id);
    unsigned int start = fr->first_retriever_id;
    for (unsigned int i = start;  i < start + fr->tries_available;  ++i) {
        auto retriever = m_RetrieverRecords[i]->retriever;
        if (retriever.NotEmpty()) {
            _ASSERT(&retriever->GetFactory()
                    == fr->factory.GetPointerOrNull());
            retriever->Cancel();
        }
    }
    fr->progress = IID2Retriever::eDone;
}


void CID2ReplyDashboard::AcceptSynonyms(const SID2ReplyEvent& event,
                                        const TSeq_id_HandleSet& ids)
{
    _TRACE("Dashboard " << this << " accepting synonyms for request #"
           << event.serial_number << " from retriever #"
           << event.retriever_id);
    _ASSERT(event.event_type == SID2ReplyEvent::fSynonymUpdate);
    _ASSERT(event.serial_number > 0);
    _ASSERT(event.retriever_id > 0);
    CWriteLockGuard guard(m_RWLock);
    _ASSERT(event.retriever_id
            < static_cast<int>(m_RetrieverRecords.size()));
    auto it = x_UpdatePacket().requests.find(event.serial_number);
    _ASSERT(it != x_UpdatePacket().requests.end());
    CRef<SID2RequestWithSynonyms> entry
        (new SID2RequestWithSynonyms(*it->second->req));
    set_union(it->second->ids.begin(), it->second->ids.end(),
              ids.begin(), ids.end(), inserter(entry->ids, entry->ids.end()));
    entry->acc = FindBestChoice(
        entry->ids, [](const auto& h){ return h.GetSeqId()->TextScore(); });
    entry->preresolved = true;
    it->second.AtomicResetFrom(entry);
    auto record = m_RequestRecords.find(event.serial_number);
    _ASSERT(record != m_RequestRecords.end());
    record->second->had_synonym_updates = true;
    auto& forecast = record->second->forecasts[event.retriever_id];
    _ASSERT((forecast & event.event_type) == event.event_type);
    forecast &= ~event.event_type;
    record->second->UpdateForecastSummary();
    x_NotifyOfMajorEvents();
    x_NotifyRetrievers(event);
}


void CID2ReplyDashboard::AcceptReplyUpdater(IID2ReplyUpdater& updater)
{
    const SID2ReplyEvent& metadata = updater.GetEventMetadata();
    _TRACE("Dashboard " << this << " accepting updater for request #"
           << metadata.serial_number << " from retriever #"
           << metadata.retriever_id << " with event type "
           << metadata.event_type);
    _ASSERT(metadata.event_type == SID2ReplyEvent::fErrorReply
            ||  metadata.event_type == SID2ReplyEvent::fSupplementaryReply
            ||  metadata.event_type == SID2ReplyEvent::fWeakMainReply
            ||  metadata.event_type == SID2ReplyEvent::fStrongMainReply);
    _ASSERT(metadata.serial_number > 0);
    _ASSERT(metadata.retriever_id > 0);
    CWriteLockGuard guard(m_RWLock);
    _ASSERT(metadata.retriever_id
            < static_cast<int>(m_RetrieverRecords.size()));
    auto it = m_RequestRecords.find(metadata.serial_number);
    _ASSERT(it != m_RequestRecords.end()  &&  it->second.NotEmpty());
    auto& req = *(it->second);
    auto& forecast = req.forecasts[metadata.retriever_id];
    _ASSERT((forecast & metadata.event_type) == metadata.event_type);
    if  (req.state != eRRS_incomplete) {
        _ASSERT(metadata.event_type != SID2ReplyEvent::fSupplementaryReply);
        _ASSERT(req.main_updater.NotEmpty());
        _ASSERT(0 == ((SID2ReplyEvent::GetStrongerReplyTypes
                       (req.main_updater->GetEventMetadata().event_type))
                      & metadata.event_type));
        return;
    }
    forecast &= ~SID2ReplyEvent::eAnyReply;
    req.UpdateForecastSummary();
    if (metadata.event_type == SID2ReplyEvent::fSupplementaryReply) {
        req.supplementary_updaters.emplace_back(&updater);
    } else {
        if (req.main_updater.Empty()
            ||  0 != ((SID2ReplyEvent::GetStrongerReplyTypes
                       (req.main_updater->GetEventMetadata().event_type))
                      & metadata.event_type)) {
            req.main_updater.Reset(&updater);
        } else {
            return;
        }
    }
    x_QueueIfNewlyComplete(it);
    x_NotifyRetrievers(metadata);
}


void CID2ReplyDashboard::AcceptStatusForecast(const SID2ReplyEvent& event)
{
    _TRACE("Dashboard " << this << " accepting forecast " << event.event_type
           << " for request #" << event.serial_number << " from retriever #"
           << event.retriever_id);
    _ASSERT(event.serial_number > 0);
    _ASSERT(event.retriever_id > 0);
    CWriteLockGuard guard(m_RWLock);
    _ASSERT(event.retriever_id < static_cast<int>(m_RetrieverRecords.size()));
    auto it = m_RequestRecords.find(event.serial_number);
    _ASSERT(it != m_RequestRecords.end()  &&  it->second.NotEmpty());
    auto& req = *(it->second);
    auto& forecast = req.forecasts[event.retriever_id];
    if (forecast == event.event_type) {
        return;
    }
    _ASSERT((forecast & event.event_type) == event.event_type);
    forecast = event.event_type;
    // propagate early forecasts to clones
    auto factory_id = m_RetrieverRecords[event.retriever_id]->factory_id;
    const auto& fr = *m_FactoryRecords[factory_id];
    if (fr.progress < IID2Retriever::eRunning) {
        _ASSERT(static_cast<int>(fr.first_retriever_id) == event.retriever_id);
        _ASSERT(fr.tries_used == 0);
        for (unsigned int i = 1;  i < fr.tries_available;  ++i) {
            req.forecasts[fr.first_retriever_id + i] = forecast;
        }
    }
    req.UpdateForecastSummary();
    x_QueueIfNewlyComplete(it);
}


void CID2ReplyDashboard::WaitForMatchingEvent(const SID2ReplyEvent& event)
{
    _TRACE("Dashboard " << this << " waiting for event of type "
           << event.event_type << " with request #" << event.serial_number
           << " for retriever #" << -event.retriever_id);
    _ASSERT(event.retriever_id < 0);
    _ASSERT(event.retriever_id > -static_cast<int>(m_RetrieverRecords.size()));
    SRetrieverRecord& rr = *m_RetrieverRecords[-event.retriever_id];
    {{
        CWriteLockGuard guard(m_RWLock);
        _ASSERT(rr.waiting_for.event_type == SID2ReplyEvent::eNoReply);
        if (IsStillPlausible(event)) {
            rr.waiting_for = event;
        } else {
            return;
        }
    }}
    rr.semaphore.Wait();
    // No lock needed for this assertion -- nothing should post to the
    // semaphore without first supplying eNoReply, and only the
    // retriever's thread, which is busy calling this method, can
    // supply any other event type (via this very method).
    _ASSERT(rr.waiting_for.event_type == SID2ReplyEvent::eNoReply);
}


void CID2ReplyDashboard::x_NotifyRetrievers(const SID2ReplyEvent& event)
{
    _TRACE("Dashboard " << this << " broadcasting event of type "
           << event.event_type << " with request #" << event.serial_number
           << " from retriever #" << event.retriever_id);
    for (size_t n = 1;  n < m_RetrieverRecords.size();  ++n) {
        auto& rr = *m_RetrieverRecords[n];
        if (rr.retriever.Empty()
            ||  event.retriever_id == static_cast<int>(n)) {
            continue;
        }
        rr.retriever->AcknowledgeEvent(event);
        if (rr.waiting_for.event_type != SID2ReplyEvent::eNoReply
            &&  (rr.waiting_for.Matches(event)
                 ||  !IsStillPlausible(rr.waiting_for) )) {
            // Defuse to avoid overflowing (and throwing) if a second
            // match materializes before the target wakes up.  (Always
            // called with the dashboard's overall write lock held,
            // so no further protection should be necessary.)
            rr.waiting_for.event_type = SID2ReplyEvent::eNoReply;
            rr.semaphore.Post();
        }
    }
}


void CID2ReplyDashboard::x_NotifyOfMajorEvents(void)
{
    bool already_notified = x_HasMajorEvent();
    _TRACE("Dashboard " << this << " checking for immediate major events;"
           " already notified: " << already_notified);
    TMajorEvents& nv_major_events
        = const_cast<TMajorEvents&>(m_MajorEvents);
    for (auto& it : m_FactoryRecords) {
        // Loop unconditionally, and always all the way through
        // (albeit possibly via an indirect self-call, as noted
        // below), to catch status changes for all primary retrievers
        // that still identified as unready.
        if (it->progress == IID2Retriever::eUnready) {
            auto& rr = *m_RetrieverRecords[it->first_retriever_id];
            if (rr.retriever.Empty()) {
                // Presumably in the midst of (roundabout) recursion
                // triggered by the below GetProgress() call, which
                // would overflow the semaphore if broken less
                // aggressively.
                _TRACE("... bailing to break recursion");
                return;
            }
            it->progress = rr.retriever->GetProgress();
            if (it->progress == IID2Retriever::eDone) {
                _ASSERT(rr.retriever.Empty()); // from MarkRetrieverDone
            }
        }
        if (it->progress == IID2Retriever::eReady) {
            nv_major_events |= fRetrieverReady;
            _TRACE("... retriever ready");
        }
    }
    if ( !m_ReadyToSend.empty() ) {
        nv_major_events |= fReplyAvailable;
        _TRACE("... reply available");
        _ASSERT(m_OutstandingRequestCount >= m_ReadyToSend.size());
    } else if (m_OutstandingRequestCount == 0) {
        nv_major_events |= fFullyReplied;
        _TRACE("... fully replied");
    }
    if ( !already_notified  &&  x_HasMajorEvent()) {
        _TRACE("... posting to semaphore");
        m_Semaphore.Post();
    }
    _TRACE("... done");
}


CRef<CID2_Reply> CID2ReplyDashboard::x_NewErrorReply(TSerialNumber sn) const
{
    CRef<CID2_Error> error(new CID2_Error);
    error->SetSeverity(CID2_Error::eSeverity_failed_command);
    error->SetMessage("No reply at all from any retriever: "
                      + NStr::TransformJoin(
                          m_FactoryRecords.begin(), m_FactoryRecords.end(),
                          ", ",
                          [](const auto &it) {
                              return it->factory->GetName();
                          }));
    CRef<CID2_Reply> result(new CID2_Reply);
    result->SetSerial_number(sn);
    result->SetError().push_back(error);
    result->SetEnd_of_reply();
    result->SetReply().SetEmpty();
    return result;
}


CRef<IID2Retriever> CID2ReplyDashboard::x_GetRetriever(
    SFactoryRecord& fr, IID2Retriever::EProgress max_progress)
{
    _TRACE("Dashboard " << this << " looking for " << fr.factory->GetName()
           << " retriever");
    if (fr.progress >= IID2Retriever::eReady
        &&  fr.progress <= max_progress) {
        _ASSERT(fr.tries_used < fr.tries_available);
        unsigned int n = fr.first_retriever_id + fr.tries_used;
        if (++fr.tries_used == fr.tries_available) {
            fr.progress = IID2Retriever::eDone;
        } else {
            fr.progress = IID2Retriever::eRunning;
        }
        _TRACE("... returning #" << n);
        return m_RetrieverRecords[n]->retriever;
    } else {
        _TRACE("... bailing");
        return null;
    }
}


void CID2ReplyDashboard::x_QueueIfNewlyComplete(TRequestRecords::iterator it)
{
    _TRACE("Dashboard " << this << " reevaluating request #" << it->first);
    if (it->second->state != eRRS_incomplete) {
        _TRACE("... already marked as queueable or better");
        return;
    } else if (it->second->IsStillIncomplete()) {
        _TRACE("... still incomplete");
        return;
    } else if (GetPacket().force_reply_order) {
        auto it2 = it;
        while (it2 != m_RequestRecords.begin()) {
            if (it2 != it  &&  it2->second->state < eRRS_queued) {
                _TRACE("... complete but waiting for request #"
                       << it2->first);
                it->second->state = eRRS_queueable;
                return;
            } else {
                --it2;
            }
        }
    }
    _TRACE("... fully ready");
    m_ReadyToSend.push(it->first);
    it->second->state = eRRS_queued;
    if (GetPacket().force_reply_order) {
        while (++it != m_RequestRecords.end()
               &&  it->second->state == eRRS_queueable) {
            _TRACE("... also unblocked request #" << it->first);
            m_ReadyToSend.push(it->first);
            it->second->state = eRRS_queued;
        }
    }
    x_NotifyOfMajorEvents();
}


bool CID2ReplyDashboard::x_IsStillX(const SID2ReplyEvent& event,
                                    SRequestRecord::EIsStillX criterion) const
{
    _TRACE("Dashboard " << this << " checking status of request #"
           << event.serial_number << " on retriever #" << event.retriever_id
           << " with event mask " << event.event_type << " and criterion "
           << criterion);
    CReadLockGuard guard(m_RWLock);
    if (event.serial_number == 0) {
        for (const auto& it : m_RequestRecords) {
            if (it.second->IsStillX(event, criterion)) {
                return true;
            }
        }
        return false;
    } else {
        auto it = m_RequestRecords.find(event.serial_number);
        if (it == m_RequestRecords.end()) {
            return false;
        } else {
            return it->second->IsStillX(event, criterion);
        }
    }
}


bool CID2ReplyDashboard::SRequestRecord::IsStillX(const SID2ReplyEvent& event,
                                                  EIsStillX criterion) const
{
    if (state != eRRS_incomplete) {
        return false;
    } else if (event.retriever_id < 0) {
        SID2ReplyEvent event2(event.event_type, event.serial_number, 0);
        for (int i = 1;  i < static_cast<int>(forecasts.size());  ++i) {
            if (i + event.retriever_id != 0) {
                event2.retriever_id = i;
                if (IsStillX(event2, criterion)) {
                    return true;
                }
            }
        }
        return false;
    } else {
        _ASSERT(event.retriever_id < static_cast<int>(forecasts.size()));
    }

    auto nexus = forecasts[event.retriever_id] & event.event_type;
    return (nexus != 0
            &&  (criterion == eIsStillPlausible
                 ||  (nexus & SID2ReplyEvent::fSupplementaryReply) != 0
                 ||  ((nexus & SID2ReplyEvent::fSynonymUpdate) != 0
                      &&  !had_synonym_updates)
                 ||  main_updater.Empty()
                 ||  0 != (nexus & (SID2ReplyEvent::GetStrongerReplyTypes
                                    (main_updater->GetEventMetadata()
                                     .event_type)))));
}


bool CID2ReplyDashboard::SRequestRecord::IsStillIncomplete(void) const
{
    if (state != eRRS_incomplete) {
        _TRACE("... explicitly complete");
        return false;
    } else if ((forecasts[0] & ~SID2ReplyEvent::fSynonymUpdate) == 0) {
        _TRACE("... implicitly complete: no further reply updates available");
        return false;
    } else if (main_updater.Empty()
               ||  (forecasts[0] & SID2ReplyEvent::fSupplementaryReply) != 0) {
        _TRACE("... still waiting for main updater and/or supplements");
        return true;
    } else {
        auto main_type = main_updater->GetEventMetadata().event_type,
             stronger  = SID2ReplyEvent::GetStrongerReplyTypes(main_type);
        _ASSERT((stronger & SID2ReplyEvent::fErrorReply) == 0);
        bool result = (forecasts[0] & stronger) != 0;
        if (result) {
            _TRACE("... still waiting for stronger reply -- "
                   << (forecasts[0] & stronger) << " > " << main_type);
        } else {
            _TRACE("... implicitly complete: no stronger replies available");
        }
        return result;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
