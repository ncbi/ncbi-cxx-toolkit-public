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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 * CCassQueryList class that maintains list of async queries against Cass*
 */

#include <ncbi_pch.hpp>
#include <sstream>
#include <objtools/pubseq_gateway/impl/cassandra/cass_query_list.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

constexpr const unsigned int CCassQueryList::kDfltMaxQuery;
constexpr const uint64_t CCassQueryList::kReadyPushWaitTimeout;
constexpr const uint64_t CCassQueryList::kReadyPopWaitTimeout;
constexpr const size_t CCassQueryList::kNotifyQueueLen;
constexpr const unsigned int CCassQueryList::kResetRelaxTime;


/** CCassQueryList::CQryNotification */

CCassQueryList::CQryNotification::CQryNotification(shared_ptr<CCassQueryList> query_list, size_t index)  :
    m_query_list(query_list),
    m_index(index)
{}

void CCassQueryList::CQryNotification::OnData() {
    shared_ptr<CCassQueryList> ql = m_query_list.lock();
    if (ql) {
        while (!ql->m_ready.push_wait(m_index, kReadyPushWaitTimeout)) {
            this_thread::yield();
        }
    }
}


/** CCassQueryList */

string CCassQueryList::SQrySlotStateStr[] = {"ssAvailable", "ssAttached", "ssReadingRow", "ssReseting", "ssReleasing"};

shared_ptr<CCassQueryList> CCassQueryList::Create(shared_ptr<CCassConnection> cass_conn) noexcept {
    struct CCassQueryListAccess : public CCassQueryList {};
    shared_ptr<CCassQueryList> rv = make_shared<CCassQueryListAccess>();
    rv->m_self_weak = rv;
    rv->m_cass_conn = cass_conn;
    rv->m_keyspace = cass_conn->Keyspace();
    return rv;
}

CCassQueryList::~CCassQueryList() {
    m_notification_arr.clear();
    if (!m_query_arr.empty() || !m_pending_arr.empty()) {
        if (NumberOfActiveQueries() > 0) {
            ERR_POST(Error << "Destructing non-empty CCassQueryList -- has active queries");
        }
        if (NumberOfBusySlots() > 0) {
            ERR_POST(Error << "Destructing non-empty CCassQueryList -- has busy slots");
        }
        if (NumberOfPendingSlots() > 0) {
            ERR_POST(Error << "Destructing non-empty CCassQueryList -- has pending tasks");
        }
        Cancel();
    }
}

CCassQueryList& CCassQueryList::SetMaxQueries(size_t max_queries) {
    m_max_queries = max_queries;
    if (m_notification_arr.size() < m_max_queries) {
        m_notification_arr.reserve(m_max_queries);
        shared_ptr<CCassQueryList> self = m_self_weak.lock();
        while (m_notification_arr.size() < m_max_queries) {
            m_notification_arr.push_back(make_shared<CQryNotification>(self, m_notification_arr.size()));
        }
    }
    return *this;
}

string CCassQueryList::GetKeyspace() const {
    return m_keyspace;
}

CCassQueryList& CCassQueryList::SetKeyspace(const string& keyspace) {
    m_keyspace = keyspace;
    return *this;
}

CCassQueryList& CCassQueryList::SetTickCB(TCassQueryListTickCB cb) {
    m_tick_cb = cb;
    return *this;
}

size_t CCassQueryList::GetMaxQueries() const {
    return m_max_queries;
}

bool CCassQueryList::HasError() const {
    return m_has_error;
}

void CCassQueryList::Tick() {
    if (m_tick_cb)
        m_tick_cb();
}

size_t CCassQueryList::NumberOfActiveQueries() const {
    size_t rv = 0;
    for (const auto& slot : m_query_arr) {
        if (slot.m_state != ssAvailable && slot.m_state != ssReleasing)
            ++rv;
    }
    return rv;
}

size_t CCassQueryList::NumberOfBusySlots() const {
    return m_attached_slots;
}

size_t CCassQueryList::NumberOfPendingSlots() const {
    return m_pending_arr.size();
}

string CCassQueryList::ToString() const {
    stringstream ss;
    for (size_t i = 0; i < m_query_arr.size(); ++i) {
        const SQrySlot& slot = m_query_arr[i];
        ss << "SLOT #" << i << ": st:" << (slot.m_state < ssLast ? SQrySlotStateStr[slot.m_state] : "???") << ", sql: " << (slot.m_qry ? slot.m_qry->GetSQL() : "");
    }
    return ss.str();
}

void CCassQueryList::CheckAccess() {
    thread::id exp{};
    if (!m_owning_thread.compare_exchange_strong(exp, this_thread::get_id())) {
        if (m_owning_thread != this_thread::get_id()) {
            NCBI_THROW(CCassandraException, eSeqFailed, "CCassQueryList is a single-thread object");
        }
    }
}

void CCassQueryList::Execute(unique_ptr<ICassQueryListConsumer> consumer, int retry_count, bool post_async) {
    CheckAccess();
    SQrySlot* slot;
    do {
        if (m_query_arr.size() < m_max_queries) {
            if (m_query_arr.empty() && m_notification_arr.empty()) {
                SetMaxQueries(m_max_queries);
            }
            size_t index = m_query_arr.size();
            while (m_query_arr.size() < m_max_queries) {
                m_query_arr.push_back({nullptr, nullptr, m_query_arr.size(), 0, ssAvailable});
            }
            slot = &m_query_arr[index];
        } else {
            slot = CheckSlots(false, !post_async);
        }
        if (post_async && !slot) {
            m_pending_arr.push_back({move(consumer), retry_count});
            Tick();
            return;
        }
    }
    while (!slot);

    AttachSlot(slot, {move(consumer), retry_count});
}

bool CCassQueryList::HasEmptySlot()
{
    if (m_query_arr.empty())
        return true;
    return CheckSlots(false, false) != nullptr;
}

void CCassQueryList::Yield(bool wait)
{
    CheckAccess();
    bool expected = false;
    if (m_yield_in_progress.compare_exchange_strong(expected, true)) {
        try {
            size_t index = numeric_limits<size_t>::max();
            while (m_ready.pop_wait(&index, wait ? kReadyPopWaitTimeout : 0)) {
                CheckSlot(index, false);
            }
            while (!m_pending_arr.empty()) {
                SQrySlot *slot = CheckSlots(false, false);
                if (slot && !m_pending_arr.empty())
                    CheckPending(slot);
                else
                    break;
            }
        }
        catch (...) {
            m_yield_in_progress.store(false);
            throw;
        }
        m_yield_in_progress.store(false);
    }
}

void CCassQueryList::DetachSlot(SQrySlot* slot)
{
    assert(slot->m_state != ssAvailable);
    if (slot->m_state != ssAvailable) {
        slot->m_consumer = nullptr;
        if (slot->m_qry)
            slot->m_qry->Close();
        --m_attached_slots;
        slot->m_state = ssAvailable;
    }
}

void CCassQueryList::AttachSlot(SQrySlot* slot, SPendingSlot&& pending_slot)
{
    assert(slot->m_state == ssAvailable);
    slot->m_state = ssAttached;
    ++m_attached_slots;
    assert(pending_slot.m_retry_count > 0);
    assert(pending_slot.m_retry_count < 1000);
    slot->m_retry_count = pending_slot.m_retry_count;
    assert(slot->m_consumer == nullptr);
    slot->m_consumer = move(pending_slot.m_consumer);

    if (!slot->m_qry) {
        slot->m_qry = m_cass_conn->NewQuery();
    } else {
        slot->m_qry->Close();
    }
    assert(slot->m_index < m_notification_arr.size());
    slot->m_qry->SetOnData3(m_notification_arr[slot->m_index]);
    if (slot->m_consumer) {
        bool b = slot->m_consumer->Start(slot->m_qry, *this, slot->m_index);
        if (!b) {
            ERR_POST(Info << "Consumer refused to start, detaching...");
            assert(!slot->m_qry->IsActive());
            DetachSlot(slot);
            CheckPending(slot);
        }
    }
    Tick();
}

void CCassQueryList::CheckPending(SQrySlot* slot) {
    assert(slot->m_state == ssAvailable);    
    if (!m_pending_arr.empty()) {
        SPendingSlot pending_slot = move(m_pending_arr.back());
        m_pending_arr.pop_back();
        AttachSlot(slot, move(pending_slot));
    }
}

void CCassQueryList::Release(SQrySlot* slot) {
    bool release = true;
    assert(slot->m_state != ssReleasing);
    assert(slot->m_state != ssAvailable);
    slot->m_state = ssReleasing;
    if (slot->m_consumer) {
        release = slot->m_consumer->Finish(slot->m_qry, *this, slot->m_index);
        if (release) {
            slot->m_consumer = nullptr;
        }
    }
    if (release) {
        DetachSlot(slot);
        CheckPending(slot);
    } else {
        assert(slot->m_qry);
        assert(slot->m_qry->IsActive());
        slot->m_state = ssAttached;
    }
}

void CCassQueryList::ReadRows(SQrySlot* slot) {
    bool do_continue = true;
    assert(slot->m_state == ssAttached);
    while (do_continue && slot->m_state == ssAttached) {
        do_continue = false;
        async_rslt_t state = slot->m_qry->NextRow();
        switch (state) {
            case ar_done: {
                if (slot->m_state != ssReleasing) {
                    Release(slot);
                }
                break;
            }
            case ar_wait:
                break;
            case ar_dataready:
                assert(slot->m_state != ssReadingRow);
                if (slot->m_state != ssReadingRow) {
                    slot->m_state = ssReadingRow;
                    try {
                        if (slot->m_consumer) {
                            do_continue = slot->m_consumer->ProcessRow(slot->m_qry, *this, slot->m_index);
                        } else {
                            do_continue = true;
                        }
                        if (slot->m_state == ssReadingRow) {
                            slot->m_state = ssAttached;
                        } else { // slot re-populated with different consumer
                            do_continue = false;
                            break;
                        }
                    }
                    catch (...) {
                        if (slot->m_state == ssReadingRow) {
                            slot->m_state = ssAttached;
                        }
                        throw;
                    }
                    assert(!do_continue || slot->m_state == ssAttached);
                } else {
                    NCBI_THROW(CCassandraException, eSeqFailed, "Recusive ReadRows call detected");
                }

                if (!do_continue && slot->m_state != ssReleasing && slot->m_state != ssAvailable) {
                    Release(slot);
                }
                break;
            default:;
        }
    }
}

shared_ptr<CCassQuery> CCassQueryList::Extract(size_t slot_index) {
    if (slot_index < m_query_arr.size()) {
        auto& slot = m_query_arr[slot_index];
        shared_ptr<CCassQuery> rv = slot.m_qry;
        slot.m_qry = nullptr;
        Release(&slot);
        return rv;
    }
    return nullptr;
}

void CCassQueryList::Cancel(ICassQueryListConsumer* consumer, const exception* e) {
    CheckAccess();
    bool found = false;
    for (size_t i = 0; i < m_query_arr.size(); ++i) {
        auto slot = &m_query_arr[i];
        if (slot->m_state != ssAvailable && slot->m_consumer.get() == consumer) {
            m_has_error = true;
            if (slot->m_consumer) {
                slot->m_consumer->Failed(slot->m_qry, *this, slot->m_index, e);
            }
            DetachSlot(slot);
            Tick();
            found = true;
            break;
        }
    }
    if (!found) {
        for (size_t i = 0; i < m_pending_arr.size(); ++i) {
            auto& it = m_pending_arr[i];
            if (it.m_consumer.get() == consumer) {
                m_has_error = true;
                m_pending_arr.erase(m_pending_arr.begin() + i);
                Tick();
                break;
            }
        }
    }
}

void CCassQueryList::Cancel(const exception* e) {
    CheckAccess();
    m_pending_arr.clear();
    for (size_t i = 0; i < m_query_arr.size(); ++i) {
        auto slot = &m_query_arr[i];
        if (slot->m_state != ssAvailable) {
            m_has_error = true;
            if (slot->m_consumer) {
                slot->m_consumer->Failed(slot->m_qry, *this, slot->m_index, e);
            }
            DetachSlot(slot);
        }
        slot->m_qry = nullptr;
        Tick();
    }
    m_query_arr.clear();
    m_pending_arr.clear();
}

void CCassQueryList::Finalize() {
    CheckAccess();
    bool any;
    do {
        any = false;
        while (!m_pending_arr.empty()) {
            SQrySlot *slot = CheckSlots(false);
            if (slot) {
                CheckPending(slot);
            }
        }
        for (const auto& slot : m_query_arr) {
            if (slot.m_qry) {
                any = true;
                break;
            }
        }
        if (any) {
            CheckSlots(true);
        }
        if (SSignalHandler::s_CtrlCPressed()) {
            NCBI_THROW(CCassandraException, eUserCancelled, "SIGINT delivered");
        }
    } while (any);
    Tick();
    m_query_arr.clear();
}

CCassQueryList::SQrySlot* CCassQueryList::CheckSlots(bool discard, bool wait) {
    if (discard) {
        while (!m_query_arr.empty()) {
            size_t index;
            if (!m_ready.pop_wait(&index, 0)) {
                index = m_query_arr.size() - 1;
            }
            while (m_query_arr[index].m_qry == nullptr && index > 0) {
                --index;
            }
            if (index == 0 && m_query_arr[index].m_qry == nullptr) {
                return nullptr;
            }

            SQrySlot *slot = CheckSlot(index, discard);
            if (slot) {
                return slot;
            }
        }
    } else {
        for (size_t index = 0; index < m_query_arr.size(); ++index) {
            SQrySlot *slot = &m_query_arr[index];
            if (slot->m_state == ssAvailable) {
                return slot;
            }
        }
        Yield(wait);
    }
    return nullptr;
}

CCassQueryList::SQrySlot* CCassQueryList::CheckSlot(size_t index, bool discard) {
    assert(index < m_query_arr.size());
    if (index >= m_query_arr.size()) {
        return nullptr;
    }
    SQrySlot *slot = &m_query_arr[index];
    if (slot->m_qry) {
        bool need_restart = false;
        string ex_msg;
        try {
            if (slot->m_qry->IsActive()) {
                switch (slot->m_qry->WaitAsync(0)) {
                    case ar_done:
                        if (slot->m_state != ssReleasing)
                            Release(slot);
                        break;
                    case ar_dataready:
                        if (slot->m_state == ssAttached)
                            ReadRows(slot);
                        break;
                    default:;
                }
            }
        }
        catch (const CCassandraException& e) {
            if (
                (
                    e.GetErrCode() == CCassandraException::eQueryTimeout
                    || e.GetErrCode() == CCassandraException::eQueryFailedRestartable
                )
                && --slot->m_retry_count > 0
            ) {
                ex_msg = e.what();
                need_restart = true;
            } else {
                m_has_error = true;
                if (slot->m_consumer) {
                    slot->m_consumer->Failed(slot->m_qry, *this, slot->m_index, &e);
                }
                DetachSlot(slot);
                throw;
            }
        }
        if (need_restart) {
            if (slot->m_state == ssAttached) {
                slot->m_state = ssReseting;
                if (slot->m_consumer) {
                    slot->m_consumer->Reset(slot->m_qry, *this, slot->m_index);
                    if (slot->m_state != ssReseting) {
                        return nullptr;
                    }
                }
                this_thread::sleep_for(chrono::milliseconds(kResetRelaxTime));
                slot->m_qry->Restart();
                ERR_POST(Warning << "CCassQueryList::CheckSlots: exception (IGNORING & RESTARTING) [" << index << "]: " << ex_msg <<  "\nquery: " << slot->m_qry->ToString().c_str());
                if (slot->m_state != ssReseting) {
                    return nullptr;
                }
                slot->m_state = ssAttached;
            } else {
                assert(slot->m_state == ssAvailable);
                if (slot->m_state != ssAvailable) {
                    ERR_POST(Error << "CCassQueryList::CheckSlots: exception [" << index << "]: " << ex_msg <<  " -- unexpected state: " << slot->m_state << "\nquery: " << slot->m_qry->ToString().c_str());
                    m_has_error = true;
                    if (slot->m_consumer) {
                        slot->m_consumer->Failed(slot->m_qry, *this, slot->m_index, nullptr);
                    }
                    DetachSlot(slot);
                    m_query_arr[index].m_qry->Close();
                }
            }
        }
        if (slot->m_state == ssAvailable) {
            if (discard) {
                slot->m_qry = m_query_arr[index].m_qry = nullptr;
            } else if (slot->m_qry && slot->m_qry->IsAsync()) {
                slot->m_qry->Close();
            }
            return slot;
        }
    } else if (!discard && slot->m_state == ssAvailable) {
        return slot;
    }

    if (SSignalHandler::s_CtrlCPressed()) {
        NCBI_THROW(CCassandraException, eUserCancelled, "SIGINT delivered");
    }
    return nullptr;
}

END_IDBLOB_SCOPE

