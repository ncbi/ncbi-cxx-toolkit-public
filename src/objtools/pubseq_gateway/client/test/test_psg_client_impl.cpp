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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include "../psg_client_impl.hpp"

#ifdef HAVE_PSG_CLIENT

#include <corelib/ncbidiag.hpp>
#include <corelib/test_boost.hpp>

#include <algorithm>
#include <barrier>
#include <deque>
#include <future>
#include <thread>
#include <random>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

const char kAllowedChars[] = "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr int kExpectedTerminalError = -2;

struct SRandom
{
    SRandom() :
        m_Engine(random_device()()),
        m_CharsDistribution(0, sizeof(kAllowedChars) - 2)
    {
    }

    size_t Get(size_t min = 0, size_t max = numeric_limits<size_t>::max())
    {
        assert(min <= max);
        return uniform_int_distribution<size_t>(min, max)(m_Engine);
    }

    string GetString(size_t length)
    {
        auto get_char = [&]() { return kAllowedChars[m_CharsDistribution(m_Engine)]; };

        string result;
        generate_n(back_inserter(result), length, get_char);
        return result;
    }

    void Fill(char* buf, size_t size)
    {
        while (size > 0) {
            auto random = m_Distribution(m_Engine);
            auto to_copy = min(size, sizeof(random));
            memcpy(buf, reinterpret_cast<char*>(&random), to_copy);
            buf += to_copy;
            size -= to_copy;
        }
    }

    template <class TIterator>
    void Shuffle(TIterator first, TIterator last)
    {
        shuffle(first, last, m_Engine);
    }

private:
    mt19937 m_Engine;
    uniform_int_distribution<size_t> m_Distribution;
    uniform_int_distribution<size_t> m_CharsDistribution;
};

struct SFixture
{
    using TData = pair<vector<char>, vector<SPSG_Message>>;
    using TIncomingData = deque<string>;

    constexpr static size_t kSizeMin = 100 * 1024;
    constexpr static size_t kSizeMax = 1024 * 1024;
    constexpr static size_t kSleepMin = 5;
    constexpr static size_t kSleepMax = 13;
    constexpr static size_t kReceiversMin = 2;
    constexpr static size_t kReceiversMax = 10;

    struct SReceiver
    {
        SReceiver(TIncomingData& src, SPSG_TimedRequest request) :
            m_It(src.begin()),
            m_End(src.end()),
            m_Request(std::move(request)),
            m_Buf(kSizeMax)
        {
            SetStream();
        }

        bool Process();
        void Complete();

    private:
        void SetStream()
        {
            m_Stream.str(m_It == m_End ? string() : *m_It);
            m_Stream.clear();
            m_Stream.seekg(0);
        }

        TIncomingData::iterator m_It;
        TIncomingData::iterator m_End;
        stringstream m_Stream;
        SPSG_TimedRequest m_Request;
        vector<char> m_Buf;
    };

    struct SReceivers
    {
        SReceivers(const SPSG_Params& params, shared_ptr<SPSG_Reply>& reply, auto&... src) :
            m_Request(make_shared<SPSG_Request>(string(), reply, CDiagContext::GetRequestContext().Clone(), params))
        {
            Add(src...);
        }

        void Add(auto&... src)
        {
            x_Add(src...);
            x_Process();
        }

        void Complete()
        {
            for (auto& receiver : m_Receivers) {
                receiver->Complete();
            }
        }

    private:
        void x_Add() {}

        void x_Add(TIncomingData& src)
        {
            m_Receivers.emplace_back(make_unique<SReceiver>(src, m_Request));
        }

        void x_Add(TIncomingData& src, auto&... rest)
        {
            x_Add(src);
            x_Add(rest...);
        }

        void x_Process()
        {
            // Receivers cannot run in parallel, each SPSG_TimedRequest needs its own reply for that
            for (auto& receiver : m_Receivers) {
                while (receiver->Process());

                auto ms = chrono::milliseconds(r.Get(kSleepMin, kSleepMax));
                this_thread::sleep_for(ms);
            }
        }

        shared_ptr<SPSG_Request> m_Request;
        vector<unique_ptr<SReceiver>> m_Receivers;
    };

    static thread_local SRandom r;
    unordered_map<string, TData> src_blobs;
    TIncomingData src_chunks;
    TIncomingData unparsable_chunks;
    TIncomingData error_503_chunks;

    SFixture();

    template <class TReadImpl>
    void MtReading();
};

thread_local SRandom SFixture::r;

void s_OutputArgs(string& s, SRandom& r, vector<string> args)
{
    ostringstream os(s);

    r.Shuffle(args.begin(), args.end());

    const char* delim = "\n\nPSG-Reply-Chunk: ";

    for (auto& arg : args) {
        os << delim << arg;
        delim = "&";
    }

    os << '\n';
    s = os.str();
}

vector<string> s_GetReplyMetaArgs(size_t n_chunks)
{
    return {
        "item_type=reply",
        "chunk_type=meta",
        "item_id=0",
        "n_chunks=" + to_string(n_chunks)
    };
}

vector<string> s_GetBlobMetaArgs(size_t item_id, const string& blob_id, size_t n_chunks)
{
    return {
        "item_type=blob",
        "chunk_type=meta",
        "item_id=" + to_string(item_id),
        "blob_id=" + blob_id,
        "n_chunks=" + to_string(n_chunks)
    };
}

vector<string> s_GetBlobDataArgs(size_t item_id, const string& blob_id, size_t chunk, size_t size)
{
    return {
        "item_type=blob",
        "chunk_type=data",
        "item_id=" + to_string(item_id),
        "blob_id=" + blob_id,
        "blob_chunk=" + to_string(chunk),
        "size=" + to_string(size)
    };
}

vector<string> s_GetBlobMessageArgs(size_t item_id, const string& blob_id, string severity, size_t size)
{
    NStr::ToLower(severity);
    return {
        "item_type=blob",
        "chunk_type=message",
        "item_id=" + to_string(item_id),
        "blob_id=" + blob_id,
        "severity=" + severity,
        "size=" + to_string(size)
    };
}

bool SFixture::SReceiver::Process()
{
    while (m_It != m_End) {
        while (m_Stream) {
            m_Stream.read(m_Buf.data(), r.Get(kSizeMin, kSizeMax));

            if (auto read = m_Stream.gcount()) {
                if (auto [processor_id, req] = m_Request.Get(); req) {
                    auto result = req->OnReplyData(processor_id, m_Buf.data(), read, false);

                    if (result == SPSG_Request::eContinue) {
                        return true;
                    } else if (result == SPSG_Request::eStop) {
                        req->OnReplyDone(processor_id)->SetComplete();
                    }
                }

                m_It = m_End;
                return false;
            }
        }

        ++m_It;
        SetStream();
    }

    return false;
}

void SFixture::SReceiver::Complete()
{
    if (auto [processor_id, req] = m_Request.Get(); req) {
        req->OnReplyDone(processor_id)->SetComplete();
    }
}

SFixture::SFixture()
{
    const size_t kBlobsMin = 3;
    const size_t kBlobsMax = 11;
    const size_t kChunksMin = 3;
    const size_t kChunksMax = 17;
    const size_t kMessagesMin = 0;
    const size_t kMessagesMax = 3;
    const size_t kMessageSizeMin = 20;
    const size_t kMessageSizeMax = 100;
    const size_t kPrefixPartMin = 1;
    const size_t kPrefixPartMax = 18;

    SetDiagPostLevel(eDiag_Info);

    // Generating source

    auto blobs_number = r.Get(kBlobsMin, kBlobsMax);
    src_blobs.reserve(blobs_number);
    vector<char> buf(kSizeMax);

    while (blobs_number > 0) {
        auto blob_id = "id_" + to_string(r.Get());
        auto rv = src_blobs.emplace(blob_id, TData());

        // Blob ID already taken
        if (!rv.second) continue;

        auto& blob_data = rv.first->second.first;
        auto& blob_messages = rv.first->second.second;
        auto chunks_number = r.Get(kChunksMin, kChunksMax);
        auto messages_number = r.Get(kMessagesMin, kMessagesMax);
        auto n_chunks = chunks_number + messages_number + 1;

        src_chunks.emplace_back();
        s_OutputArgs(src_chunks.back(), r, s_GetBlobMetaArgs(blobs_number, blob_id, n_chunks));

        for (size_t i = 0; i < chunks_number; ++i) {
            src_chunks.emplace_back();
            auto& chunk_stream = src_chunks.back();
            auto chunk_size = r.Get(kSizeMin, kSizeMax);

            s_OutputArgs(src_chunks.back(), r, s_GetBlobDataArgs(blobs_number, blob_id, i, chunk_size));
            r.Fill(buf.data(), chunk_size);
            chunk_stream.append(buf.data(), chunk_size);
            blob_data.insert(blob_data.end(), &buf[0], &buf[chunk_size]);
        }

        for (size_t i = 0; i < messages_number; ++i) {
            src_chunks.emplace_back();
            auto& message_stream = src_chunks.back();
            auto message_size = r.Get(kMessageSizeMin, kMessageSizeMax);
            auto message = r.GetString(message_size);
            auto severity = EDiagSev((r.Get(0, 8) + 5) % 6); // eDiag_Info, eDiag_Warning or eDiag_Trace are twice as probable
            auto code = r.Get(0, 1) ? int(r.Get(300, 350)) : optional<int>{};

            auto blob_message = s_GetBlobMessageArgs(blobs_number, blob_id, CNcbiDiag::SeverityName(severity), message_size);

            if (code) {
                blob_message.emplace_back("code=" + to_string(*code));
            }

            if (severity == eDiag_Trace) {
                _DEBUG_CODE(blob_messages.emplace_back(SPSG_Message{message, severity, code}););
            } else {
                blob_messages.emplace_back(SPSG_Message{message, severity, code});
            }

            s_OutputArgs(src_chunks.back(), r, blob_message);
            message_stream += message;
        }

        --blobs_number;
    }

    r.Shuffle(src_chunks.begin(), src_chunks.end());

    src_chunks.emplace_back();
    s_OutputArgs(src_chunks.back(), r, s_GetReplyMetaArgs(src_chunks.size()));


    // Generating unparsable source (starting with a good looking part of the prefix)

    for (size_t i = r.Get(kChunksMin, kChunksMax); i > 0; --i) {
        auto chunk_size = r.Get(kSizeMin, kSizeMax);
        r.Fill(buf.data(), chunk_size);
        unparsable_chunks.emplace_back();
        unparsable_chunks.back().append(buf.data(), chunk_size);
    }

    unparsable_chunks.emplace_front(src_chunks.front().substr(0, r.Get(kPrefixPartMin, kPrefixPartMax)));


    // Generating error 503 source (splitting first chunk)

    auto blob_id = "id_" + to_string(r.Get());
    auto blob_meta = s_GetBlobMetaArgs(1, blob_id, 2);
    blob_meta.emplace_back("status=503");
    error_503_chunks.emplace_back();
    s_OutputArgs(error_503_chunks.back(), r, blob_meta);

    auto message_size = r.Get(kMessageSizeMin, kMessageSizeMax);
    auto blob_message = s_GetBlobMessageArgs(1, blob_id, "error", message_size);
    blob_message.emplace_back("status=503&code=310");
    error_503_chunks.emplace_back();
    s_OutputArgs(error_503_chunks.back(), r, blob_message);
    error_503_chunks.back() += r.GetString(message_size);

    r.Shuffle(error_503_chunks.begin(), error_503_chunks.end());

    const auto prefix_part_size = r.Get(kPrefixPartMin, kPrefixPartMax);
    error_503_chunks.front().erase(0, prefix_part_size);
    error_503_chunks.emplace_front(src_chunks.front().substr(0, prefix_part_size));

    error_503_chunks.emplace_back();
    s_OutputArgs(error_503_chunks.back(), r, s_GetReplyMetaArgs(error_503_chunks.size()));
}

template <class TReadImpl>
void SFixture::MtReading()
{
    const unsigned kReadingDeadline = 300;

    const SPSG_Params params;


    // Test receiving unparsable data only

    auto unparsable = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    SReceivers(params, unparsable, unparsable_chunks);
    BOOST_REQUIRE_MESSAGE_MT_SAFE(!unparsable->GetNextItem(CDeadline::eNoWait), "Got an item from only unparsable data");


    // Test receiving "error 503" data only

    auto error_503 = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    SReceivers(params, error_503, error_503_chunks);
    BOOST_REQUIRE_MESSAGE_MT_SAFE(!error_503->GetNextItem(CDeadline::eNoWait), "Got an item from only \"error 503\" data");


    // Reading

    auto reply = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    map<SPSG_Reply::SItem::TTS*, thread> readers;

    auto reader_impl = [&](const TData& src, SPSG_Reply::SItem::TTS& dst) {
        TReadImpl read_impl(dst);
        vector<char> received(kSizeMax);
        auto expected = src.first.data();
        size_t expected_to_read = src.first.size();
        const auto expect_terminal_error = any_of(src.second.begin(), src.second.end(), [](const auto& message) {
            return eDiag_Error <= message.severity && message.severity <= eDiag_Fatal;
        });
        CDeadline deadline(kReadingDeadline, 0);

        auto check_messages = [&] {
            const auto& src_messages = src.second;
            size_t messages = 0;

            while (auto message = dst.GetLock()->state.GetMessage(eDiag_Trace)) {
                ++messages;
                auto it = find(src_messages.begin(), src_messages.end(), message);
                BOOST_CHECK_MESSAGE_MT_SAFE(it != src_messages.end(), "Received message does not match expected");
            }

            BOOST_CHECK_MESSAGE_MT_SAFE(messages >= src_messages.size(), "Received less messages than expected");
            BOOST_CHECK_MESSAGE_MT_SAFE(messages <= src_messages.size(), "Received more messages than expected");
        };

        while (!deadline.IsExpired()) {
            size_t read = 0;
            auto reading_result = read_impl(r, received.data(), received.size(), expected_to_read, &read);

            if (reading_result < 0) {
                if (reading_result != kExpectedTerminalError || !expect_terminal_error) {
                    BOOST_ERROR_MT_SAFE("Reader stopped unexpectedly");
                }
                if (reading_result == kExpectedTerminalError) {
                    check_messages();
                }
                return;
            }

            BOOST_REQUIRE_MESSAGE_MT_SAFE(read <= expected_to_read, "Received more data than expected");
            BOOST_REQUIRE_MESSAGE_MT_SAFE(equal(&received[0], &received[read], expected), "Received data does not match expected");

            expected += read;
            expected_to_read -= read;

            if (reading_result == 0) {
                if (expect_terminal_error) {
                    BOOST_ERROR_MT_SAFE("Reader reached EOF without the expected terminal error");
                }

                check_messages();
                break;
            }

            auto ms = chrono::milliseconds(r.Get(kSleepMin, kSleepMax));
            this_thread::sleep_for(ms);
        }

        BOOST_REQUIRE_MESSAGE_MT_SAFE(!expected_to_read, "Got less data that expected");
    };

    auto dispatcher_impl = [&]() {
        CDeadline deadline(kReadingDeadline, 0);

        while (auto new_item = reply->GetNextItem(deadline)) {
            // No more reply items
            if (auto item_ts = new_item.value(); !item_ts) {
                break;

            } else if (auto reader = readers.find(item_ts); reader == readers.end()) {
                auto item_locked = item_ts->GetLock();
                auto blob_id = item_locked->args.GetValue<string>("blob_id");
                auto src_blob = src_blobs.find(blob_id);

                BOOST_REQUIRE_MESSAGE_MT_SAFE(src_blob != src_blobs.end(), "Unknown blob received");

                thread t = thread(reader_impl, src_blob->second, ref(*item_ts));
                readers.emplace(item_ts, std::move(t));
            }
        }

        BOOST_REQUIRE_MESSAGE_MT_SAFE(readers.size() >= src_blobs.size(), "Got less blobs that expected");

        for (auto& reader : readers) {
            if (reader.second.joinable()) reader.second.join();
        }
    };

    thread dispatcher(dispatcher_impl);


    // Receiving normal data after unparsable and "error 503" data

    SReceivers receivers(params, reply, unparsable_chunks, error_503_chunks, src_chunks);
    receivers.Complete();


    // Waiting

    dispatcher.join();
}

BOOST_FIXTURE_TEST_SUITE(PSG, SFixture)

BOOST_AUTO_TEST_CASE(Request)
{
    const SPSG_Params params;


    // Test receiving unparsable data only

    auto unparsable = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    SReceivers(params, unparsable, unparsable_chunks);
    BOOST_REQUIRE_MESSAGE_MT_SAFE(!unparsable->GetNextItem(CDeadline::eNoWait), "Got an item from only unparsable data");


    // Test receiving "error 503" data only

    auto error_503 = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    SReceivers(params, error_503, error_503_chunks);
    BOOST_REQUIRE_MESSAGE_MT_SAFE(!error_503->GetNextItem(CDeadline::eNoWait), "Got an item from only \"error 503\" data");


    // Receiving normal data after unparsable and "error 503" data

    auto reply = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
    SReceivers receivers(params, reply, unparsable_chunks, error_503_chunks, src_chunks);
    receivers.Complete();


    // Checking

    auto items_locked = reply->items.GetLock();
    auto& items = *items_locked;

    for (auto& item_ts : items) {
        auto item_locked = item_ts.GetLock();
        auto& item = *item_locked;
        auto& expected = item.expected;
        auto& received = item.received;

        BOOST_REQUIRE_MESSAGE(!expected.Cmp<greater>(received), "Expected is greater than received");
        BOOST_REQUIRE_MESSAGE(!expected.Cmp<less>(received), "Expected is less than received");

        auto& chunks = item.chunks;
        auto blob_id = item.args.GetValue<string>("blob_id");

        auto src_blob = src_blobs.find(blob_id);

        BOOST_REQUIRE_MESSAGE(src_blob != src_blobs.end(), "Unknown blob received");

        {
            auto src_current = src_blob->second.first.begin();
            auto src_end = src_blob->second.first.end();

            for (auto& chunk : chunks) {
                auto dst_current = chunk.begin();
                auto dst_end = chunk.end();

                auto src_to_compare = distance(src_current, src_end);
                auto dst_to_compare = distance(dst_current, dst_end);

                BOOST_REQUIRE_MESSAGE(dst_to_compare <= src_to_compare, "Received more data than sent");
                BOOST_REQUIRE_MESSAGE(equal(dst_current, dst_end, src_current), "Received data does not match expected");

                advance(src_current, dst_to_compare);
            }

            BOOST_REQUIRE_MESSAGE(src_current == src_end, "Received less data than sent");
        }
    }
}

bool s_IsExpectedCompletedError(SPSG_Reply::SItem::TTS& dst)
{
    auto dst_locked = dst.GetLock();
    return !dst_locked->state.InProgress() && dst_locked->state.GetStatus() == EPSG_Status::eError;
}

struct SBlobReader
{
    SBlobReader(SPSG_Reply::SItem::TTS& dst) : reader(dst), m_Dst(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t expected, size_t* read)
    {
        assert(buf);
        assert(read);

        auto pending_result = reader.PendingCount(read);

        if (pending_result == eRW_Error && s_IsExpectedCompletedError(m_Dst)) return kExpectedTerminalError;
        BOOST_REQUIRE_MESSAGE_MT_SAFE((pending_result == eRW_Success) || (pending_result == eRW_Eof), "PendingCount() failed");
        BOOST_REQUIRE_MESSAGE_MT_SAFE(*read <= expected, "Pending data is more than expected");

        auto to_read = r.Get(1, buf_size);
        auto reading_result = eRW_Success;

        try {
            reading_result = reader.Read(buf, to_read, read);
        }
        catch (CPSG_Exception& ex) {
            BOOST_ERROR_MT_SAFE("Read() exception: " << ex.GetErrCodeString());
            return -1;
        }
        catch (...) {
            BOOST_ERROR_MT_SAFE("Read() exception: Unknown");
            return -1;
        }

        if (reading_result == eRW_Eof)     return 0;
        if (reading_result == eRW_Success) return 1;
        if (reading_result == eRW_Error && s_IsExpectedCompletedError(m_Dst)) return kExpectedTerminalError;

        BOOST_ERROR_MT_SAFE("Read() failed: " << g_RW_ResultToString(reading_result));
        return -1;
    }

private:
    SPSG_BlobReader reader;
    SPSG_Reply::SItem::TTS& m_Dst;
};

BOOST_AUTO_TEST_CASE(BlobReader)
{
    auto valgrind = getenv("NCBI_RUN_UNDER_VALGRIND");

    if (valgrind && !NStr::strcasecmp(valgrind, "yes")) {
        TPSG_ReaderTimeout::SetDefault(60);
    }

    MtReading<SBlobReader>();
}

struct SStreamReadsome
{
    SStreamReadsome(SPSG_Reply::SItem::TTS& dst) : is(dst), m_Dst(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t expected, size_t* read)
    {
        auto to_read = r.Get(1, buf_size);
        *read = is.readsome(buf, to_read);

        if (*read) {
            return 1;
        } else if (s_IsExpectedCompletedError(m_Dst)) {
            return kExpectedTerminalError;
        } else if (is.eof()) {
            return 0;
        } else if (is.fail()) {
            return -1;
        } else if (!m_Dst.GetLock()->state.InProgress() && expected == 0) {
            return 0;
        } else {
            return 1;
        }
    }

private:
    SPSG_RStream is;
    SPSG_Reply::SItem::TTS& m_Dst;
};

BOOST_AUTO_TEST_CASE(StreamReadsome)
{
    MtReading<SStreamReadsome>();
}

struct SStreamRead
{
    SStreamRead(SPSG_Reply::SItem::TTS& dst) : is(dst), m_Dst(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t, size_t* read)
    {
        auto to_read = r.Get(1, buf_size);
        const auto good = static_cast<bool>(is.read(buf, to_read));

        *read = is.gcount();

        if (good || *read) {
            return 1;
        } else if (s_IsExpectedCompletedError(m_Dst)) {
            return kExpectedTerminalError;
        } else if (is.eof()) {
            return 0;
        } else {
            return -1;
        }
    }

private:
    SPSG_RStream is;
    SPSG_Reply::SItem::TTS& m_Dst;
};

BOOST_AUTO_TEST_CASE(StreamRead)
{
    MtReading<SStreamRead>();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PSG)

void s_CompilationTest()
{
    string seq_id;
    CPSG_BioId bio_id(seq_id);
    CPSG_BioIds bio_ids{bio_id};
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;

    auto user_context = make_shared<string>();
    auto request_context = CRef<CRequestContext>();

    CPSG_Request_Biodata biodata[] =
    {
        { seq_id },
        { seq_id, user_context },
        { seq_id, user_context, request_context },
        { bio_id },
        { bio_id, user_context },
        { bio_id, user_context, request_context },
        { seq_id, EPSG_BioIdResolution::NoResolve },
        { seq_id, EPSG_BioIdResolution::NoResolve, user_context },
        { seq_id, EPSG_BioIdResolution::NoResolve, user_context, request_context },
        { bio_id, EPSG_BioIdResolution::NoResolve },
        { bio_id, EPSG_BioIdResolution::NoResolve, user_context },
        { bio_id, EPSG_BioIdResolution::NoResolve, user_context, request_context },
    };

    CPSG_Request_Resolve resolve[] =
    {
        { seq_id },
        { seq_id, user_context },
        { seq_id, user_context, request_context },
        { bio_id },
        { bio_id, user_context },
        { bio_id, user_context, request_context },
        { seq_id, EPSG_BioIdResolution::NoResolve },
        { seq_id, EPSG_BioIdResolution::NoResolve, user_context },
        { seq_id, EPSG_BioIdResolution::NoResolve, user_context, request_context },
        { bio_id, EPSG_BioIdResolution::NoResolve },
        { bio_id, EPSG_BioIdResolution::NoResolve, user_context },
        { bio_id, EPSG_BioIdResolution::NoResolve, user_context, request_context },
    };

    CPSG_Request_NamedAnnotInfo named_annot[] =
    {
        { seq_id,  annot_names },
        { seq_id,  annot_names, user_context },
        { seq_id,  annot_names, user_context, request_context },
        { bio_id,  annot_names },
        { bio_id,  annot_names, user_context },
        { bio_id,  annot_names, user_context, request_context },
        { bio_ids, annot_names },
        { bio_ids, annot_names, user_context },
        { bio_ids, annot_names, user_context, request_context },
        { seq_id,  annot_names, EPSG_BioIdResolution::NoResolve },
        { seq_id,  annot_names, EPSG_BioIdResolution::NoResolve, user_context },
        { seq_id,  annot_names, EPSG_BioIdResolution::NoResolve, user_context, request_context },
        { bio_id,  annot_names, EPSG_BioIdResolution::NoResolve },
        { bio_id,  annot_names, EPSG_BioIdResolution::NoResolve, user_context },
        { bio_id,  annot_names, EPSG_BioIdResolution::NoResolve, user_context, request_context },
        { bio_ids, annot_names, EPSG_BioIdResolution::NoResolve },
        { bio_ids, annot_names, EPSG_BioIdResolution::NoResolve, user_context },
        { bio_ids, annot_names, EPSG_BioIdResolution::NoResolve, user_context, request_context },
    };
}

SPSG_UserArgs s_Build(SPSG_UserArgsBuilder& builder, const SPSG_UserArgs& request_args)
{
    ostringstream os;
    builder.Build(os, request_args);
    SPSG_UserArgs rv(os.str());
    rv.erase("client_id");
    return rv;
}

BOOST_AUTO_TEST_CASE(UserArgsBuilder)
{
    s_CompilationTest();

    TPSG_RequestUserArgs::SetDefault("enable_processor=cdd&enable_processor=osg&hops=3");
    SPSG_UserArgsBuilder builder;
    SPSG_UserArgs request_user_args("enable_processor=snp&disable_processor=cdd&hops=2&use_cache=no");

    BOOST_CHECK_EQUAL(s_Build(builder, {}), SPSG_UserArgs("&enable_processor=cdd&enable_processor=osg&hops=3"));
    BOOST_CHECK_EQUAL(s_Build(builder, request_user_args), SPSG_UserArgs("&enable_processor=cdd&enable_processor=osg&enable_processor=snp&hops=3&use_cache=no"));

    builder.SetQueueArgs({{"enable_processor", {"wgs"}}, {"disable_processor", {"snp", "cdd"}}, {"hops", {"1"}}});

    BOOST_CHECK_EQUAL(s_Build(builder, {}), SPSG_UserArgs("&disable_processor=snp&enable_processor=cdd&enable_processor=osg&enable_processor=wgs&hops=3"));
    BOOST_CHECK_EQUAL(s_Build(builder, request_user_args), SPSG_UserArgs("&enable_processor=cdd&enable_processor=osg&enable_processor=snp&enable_processor=wgs&hops=3&use_cache=no"));
}

// Clang 20+ is required to support jthread by default (18+ with -fexperimental-library)
struct SJThreads : vector<thread>
{
    SJThreads() = default;
    SJThreads(SJThreads&&) = default;
    using vector<thread>::emplace_back;

    ~SJThreads() { for (auto& t : *this) t.join(); }
};

auto s_CreateThreads(auto n, auto l, auto& b)
{
    SJThreads rv;

    while (n-- > 0) {
        rv.emplace_back(l, ref(b));
    }

    return rv;
}

auto s_CreateThreadsAndWait(auto n, auto l)
{
    barrier b(n + 1);
    auto rv = s_CreateThreads(n, l, b);
    b.arrive_and_wait();
    return rv;
}

BOOST_AUTO_TEST_CASE(SyncThreadSafe)
{
    const auto kTimeout = CTimeout(0.5);
    const auto kSleep = chrono::milliseconds(100);
    const auto kIterations = 10;
    const auto kThreads = 10;
    SRandom r;
    SSyncThreadSafe<bool> sts;
    atomic_size_t notified = 0;

    auto thread_impl = [&](barrier<>& b) {
        [[maybe_unused]] auto u = b.arrive();

        auto locked = sts.GetLock();

        if (sts.WaitUntil(locked, kTimeout, [&]() { return !*locked; } )) {
            ++notified;
        }
    };

    for (auto i = kIterations; i > 0; --i) {
        auto n = r.Get(2, kThreads);
        {
            *sts.GetLock() = true;
            notified = 0;
            auto threads = s_CreateThreadsAndWait(n, thread_impl);
            *sts.GetLock() = false;
            sts.NotifyOne();
            this_thread::sleep_for(kSleep);
            BOOST_CHECK_MT_SAFE(notified >= 1);
            sts.NotifyAll();
        }

        {
            *sts.GetLock() = true;
            notified = 0;
            auto threads = s_CreateThreadsAndWait(n, thread_impl);
            *sts.GetLock() = false;
            sts.NotifyAll();
            this_thread::sleep_for(kSleep);
            BOOST_CHECK_MT_SAFE(notified == n);
            sts.NotifyAll();
        }
    }
}

BOOST_AUTO_TEST_CASE(WaitingQueue)
{
    const auto kTimeout = CTimeout(0.5);
    const auto kIterations = 10;
    const auto kThreads = 10;
    const auto kItems = 10000;
    SRandom r;
    CPSG_WaitingQueue<int> q;
    atomic_int to_send;
    atomic_int to_receive;

    auto receiver_impl = [&](barrier<>& b) {
        [[maybe_unused]] auto u = b.arrive();
        int v;
        while (to_receive-- > 0) {
            if (!q.Pop(v, kTimeout)) {
                to_receive++;
            }
        }
        to_receive++;
    };

    auto producer_impl = [&](barrier<>& b) {
        b.arrive_and_wait();
        while (to_send-- > 0) {
            q.Push(1);
        }
        to_send++;
    };

    for (auto i = kIterations; i > 0; --i) {
        to_send = to_receive = (int)r.Get(1, kItems);
        {
            auto n = r.Get(1, kThreads), m = r.Get(1, kThreads);
            barrier b(n + m);
            auto receivers = s_CreateThreads(n, receiver_impl, b);
            auto producers = s_CreateThreads(m, producer_impl, b);
        }
        BOOST_CHECK_EQUAL_MT_SAFE(to_send.exchange(0), 0);
        BOOST_CHECK_EQUAL_MT_SAFE(to_receive.exchange(0), 0);
    }
}

void s_TestArgsImpl(const char* impl_name)
{
    // Test basic GetValue
    {
        SPSG_Args args("key1=value1&key2=value2&empty_key=&key3=value3");

        BOOST_CHECK_MESSAGE(args.GetValue("key1") == "value1"sv, impl_name << ": GetValue key1");
        BOOST_CHECK_MESSAGE(args.GetValue("key2") == "value2"sv, impl_name << ": GetValue key2");
        BOOST_CHECK_MESSAGE(args.GetValue("key3") == "value3"sv, impl_name << ": GetValue key3");
        BOOST_CHECK_MESSAGE(args.GetValue("empty_key") == ""sv, impl_name << ": GetValue empty_key");
        BOOST_CHECK_MESSAGE(args.GetValue("nonexistent") == ""sv, impl_name << ": GetValue nonexistent");
    }

    // Test URL decoding
    {
        SPSG_Args args("encoded=%2F%3D%26&space=%20&plus=a%2Bb");

        BOOST_CHECK_MESSAGE(args.GetValue("encoded") == "/=&"sv, impl_name << ": URL decode special chars");
        BOOST_CHECK_MESSAGE(args.GetValue("space") == " "sv, impl_name << ": URL decode space");
        BOOST_CHECK_MESSAGE(args.GetValue("plus") == "a+b"sv, impl_name << ": URL decode plus sign");
    }

    // Test case-insensitive argument names
    {
        SPSG_Args args("MiXeD_Key=value");

        BOOST_CHECK_MESSAGE(args.GetValue("mixed_key") == "value"sv,
                impl_name << ": case-insensitive argument name");
    }

    // Test valueless segments match CUrlArgs behavior
    {
        SPSG_Args args("foo&bar=baz&empty=");

        BOOST_CHECK_MESSAGE(args.GetValue("foo") == ""sv, impl_name << ": valueless segment yields empty string");
        BOOST_CHECK_MESSAGE(args.GetValue("bar") == "baz"sv, impl_name << ": mixed valueless and keyed segments");
        BOOST_CHECK_MESSAGE(args.GetValue("empty") == ""sv, impl_name << ": explicit empty value");
    }

    // Test malformed trailing segment without '='
    {
        SPSG_Args args("foo=bar&baz");

        BOOST_CHECK_MESSAGE(args.GetValue("foo") == "bar"sv, impl_name << ": keyed segment before trailing valueless");
        BOOST_CHECK_MESSAGE(args.GetValue("baz") == ""sv, impl_name << ": trailing valueless segment yields empty string");
    }

    // Test empty-name segments do not hide following arguments
    {
        SPSG_Args args_leading("&key=value");
        SPSG_Args args_repeated("first=1&&second=2");
        SPSG_Args args_empty_name("=ignored&key=value");

        BOOST_CHECK_MESSAGE(args_leading.GetValue("key") == "value"sv, impl_name << ": leading empty segment is ignored");
        BOOST_CHECK_MESSAGE(args_repeated.GetValue("second") == "2"sv, impl_name << ": repeated separator is ignored");
        BOOST_CHECK_MESSAGE(args_empty_name.GetValue("key") == "value"sv, impl_name << ": empty-name segment is ignored");
    }

    // Test GetValue
    {
        SPSG_Args args("str_key=string_value&empty=");

        BOOST_CHECK_EQUAL(args.GetValue("str_key"), string("string_value"));
        BOOST_CHECK_EQUAL(args.GetValue("empty"), string(""));
        BOOST_CHECK_EQUAL(args.GetValue("missing"), string(""));
    }

    // Test GetValue with various types
    {
        SPSG_Args args("int_val=42&negative=-123&zero=0&double_val=3.14&large=9876543210&empty=&invalid=abc");

        // Basic integer types
        BOOST_CHECK_MESSAGE(args.GetValue<int>("int_val") == 42, impl_name << ": GetValue int");
        BOOST_CHECK_MESSAGE(args.GetValue<int>("negative") == -123, impl_name << ": GetValue negative");
        BOOST_CHECK_MESSAGE(args.GetValue<int>("zero") == 0, impl_name << ": GetValue zero");
        BOOST_CHECK_MESSAGE(args.GetValue<size_t>("int_val") == 42, impl_name << ": GetValue size_t");
        BOOST_CHECK_MESSAGE(args.GetValue<Int8>("large") == 9876543210, impl_name << ": GetValue Int8");

        // Double
        BOOST_CHECK_CLOSE(args.GetValue<double>("double_val"), 3.14, 0.001);

        // Empty/missing returns default (0)
        BOOST_CHECK_MESSAGE(args.GetValue<int>("empty") == 0, impl_name << ": GetValue empty");
        BOOST_CHECK_MESSAGE(args.GetValue<int>("missing") == 0, impl_name << ": GetValue missing");
        auto invalid = args.GetValue<int, nothrow>("invalid");
        BOOST_CHECK_MESSAGE(invalid == 0, impl_name << ": GetValue invalid");

        // Optional types
        BOOST_CHECK_MESSAGE(args.GetValue<optional<int>>("int_val") == 42, impl_name << ": GetValue optional<int>");
        BOOST_CHECK_MESSAGE(!args.GetValue<optional<int>>("empty").has_value(), impl_name << ": GetValue optional empty");
        BOOST_CHECK_MESSAGE(!args.GetValue<optional<int>>("missing").has_value(), impl_name << ": GetValue optional missing");
        auto optional_invalid = args.GetValue<optional<int>, nothrow>("invalid");
        BOOST_CHECK_MESSAGE(!optional_invalid.has_value(), impl_name << ": GetValue optional invalid");

        // CNullable types
        BOOST_CHECK_MESSAGE(!args.GetValue<CNullable<int>>("int_val").IsNull(), impl_name << ": GetValue CNullable has value");
        BOOST_CHECK_MESSAGE(args.GetValue<CNullable<int>>("int_val") == 42, impl_name << ": GetValue CNullable<int>");
        BOOST_CHECK_MESSAGE(args.GetValue<CNullable<int>>("empty").IsNull(), impl_name << ": GetValue CNullable empty");
        BOOST_CHECK_MESSAGE(args.GetValue<CNullable<int>>("missing").IsNull(), impl_name << ": GetValue CNullable missing");
    }

    // Test GetValue<EValue> for item_type
    {
        SPSG_Args args_blob("item_type=blob&chunk_type=data");
        BOOST_CHECK_MESSAGE(args_blob.GetValue<SPSG_Args::eItemType>().first == SPSG_Args::eBlob, impl_name << ": item_type blob");
        BOOST_CHECK_MESSAGE(args_blob.GetValue<SPSG_Args::eItemType>().second == "blob"sv, impl_name << ": item_type blob string");

        SPSG_Args args_bioseq("item_type=bioseq_info");
        BOOST_CHECK_MESSAGE(args_bioseq.GetValue<SPSG_Args::eItemType>().first == SPSG_Args::eBioseqInfo, impl_name << ": item_type bioseq_info");

        SPSG_Args args_reply("item_type=reply");
        BOOST_CHECK_MESSAGE(args_reply.GetValue<SPSG_Args::eItemType>().first == SPSG_Args::eReply, impl_name << ": item_type reply");

        SPSG_Args args_empty("");
        BOOST_CHECK_MESSAGE(args_empty.GetValue<SPSG_Args::eItemType>().first == SPSG_Args::eReply, impl_name << ": item_type empty defaults to reply");

        SPSG_Args args_unknown("item_type=unknown_type");
        BOOST_CHECK_MESSAGE(args_unknown.GetValue<SPSG_Args::eItemType>().first == SPSG_Args::eUnknownItem, impl_name << ": item_type unknown");
    }

    // Test GetValue<EValue> for chunk_type
    {
        SPSG_Args args_data("chunk_type=data");
        BOOST_CHECK_MESSAGE(args_data.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eData, impl_name << ": chunk_type data");

        SPSG_Args args_meta("chunk_type=meta");
        BOOST_CHECK_MESSAGE(args_meta.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eMeta, impl_name << ": chunk_type meta");

        SPSG_Args args_message("chunk_type=message");
        BOOST_CHECK_MESSAGE(args_message.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eMessage, impl_name << ": chunk_type message");

        SPSG_Args args_data_meta("chunk_type=data_and_meta");
        BOOST_CHECK_MESSAGE(args_data_meta.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eDataAndMeta, impl_name << ": chunk_type data_and_meta");

        SPSG_Args args_unknown("chunk_type=unknown");
        BOOST_CHECK_MESSAGE(args_unknown.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eUnknownChunk, impl_name << ": chunk_type unknown");
    }

    // Test GetValue<EValue> for blob_id
    {
        SPSG_Args args("blob_id=123.456.789&other=value");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eBlobId>() == "123.456.789"sv, impl_name << ": blob_id");

        SPSG_Args args_encoded("blob_id=id%2Fwith%2Fslashes");
        BOOST_CHECK_MESSAGE(args_encoded.GetValue<SPSG_Args::eBlobId>() == "id/with/slashes"sv, impl_name << ": blob_id URL decoded");
    }

    // Test GetValue<EValue> for id2_chunk
    {
        SPSG_Args args("id2_chunk=42&id2_info=some_info");
        BOOST_CHECK_MESSAGE(!args.GetValue<SPSG_Args::eId2Chunk>().empty(), impl_name << ": id2_chunk has value");
        auto id2_chunk = args.GetValue<SPSG_Args::eId2Chunk, int>();
        BOOST_CHECK_MESSAGE(id2_chunk == 42, impl_name << ": id2_chunk value");

        SPSG_Args args_empty("id2_info=some_info");
        BOOST_CHECK_MESSAGE(args_empty.GetValue<SPSG_Args::eId2Chunk>().empty(), impl_name << ": id2_chunk empty");
    }

    // Test ConvertToRaw
    {
        const string original = "item_type=unknown_item&chunk_type=data&blob_id=old_blob_id&id2_chunk=7&item_id=1";
        SPSG_Args args(original);

        const auto item_type = args.GetValue<SPSG_Args::eItemType>();
        BOOST_CHECK_MESSAGE(item_type.first == SPSG_Args::eUnknownItem, impl_name << ": cached item_type before ConvertToRaw");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eData, impl_name << ": cached chunk_type before ConvertToRaw");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eBlobId>() == "old_blob_id"sv, impl_name << ": cached blob_id before ConvertToRaw");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eId2Chunk>() == "7"sv, impl_name << ": cached id2_chunk before ConvertToRaw");

        args.ConvertToRaw(item_type.second, 12345);

        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eItemType>().second == "unknown_item"sv, impl_name << ": ConvertToRaw refreshes cached item_type");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eChunkType>().first == SPSG_Args::eData, impl_name << ": ConvertToRaw refreshes cached chunk_type");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eBlobId>() == "unknown_item"sv, impl_name << ": ConvertToRaw refreshes cached blob_id");
        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eId2Chunk>() == "7"sv, impl_name << ": ConvertToRaw refreshes cached id2_chunk");
        BOOST_CHECK_MESSAGE(args.GetValue("last_modified") == "12345"sv, impl_name << ": ConvertToRaw sets last_modified");

        ostringstream os;
        os << args;
        BOOST_CHECK_MESSAGE(os.str() == original, impl_name << ": ConvertToRaw preserves original output");
    }

    // Test ConvertToRaw overrides existing values
    {
        SPSG_Args args("blob_id=old_blob_id&last_modified=1");

        args.ConvertToRaw("new_blob_id", 12345);

        BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eBlobId>() == "new_blob_id"sv,
                impl_name << ": ConvertToRaw overrides blob_id");
        BOOST_CHECK_MESSAGE(args.GetValue("last_modified") == "12345"sv,
                impl_name << ": ConvertToRaw overrides last_modified");
    }

    // Test output stream operator
    {
        SPSG_Args args("key1=value1&key2=value2");
        ostringstream os;
        os << args;
        string output = os.str();
        BOOST_CHECK_MESSAGE(output.find("key1=value1") != string::npos, impl_name << ": output contains key1=value1");
        BOOST_CHECK_MESSAGE(output.find("key2=value2") != string::npos, impl_name << ": output contains key2=value2");
    }

    // Test all item_type values
    {
        vector<pair<SPSG_Args::EItemType, string_view>> item_types{
            { SPSG_Args::eBioseqInfo,    "bioseq_info"sv      },
            { SPSG_Args::eBlobProp,      "blob_prop"sv        },
            { SPSG_Args::eBlob,          "blob"sv             },
            { SPSG_Args::eReply,         "reply"sv            },
            { SPSG_Args::eBioseqNa,      "bioseq_na"sv        },
            { SPSG_Args::eNaStatus,      "na_status"sv        },
            { SPSG_Args::ePublicComment, "public_comment"sv   },
            { SPSG_Args::eProcessor,     "processor"sv        },
            { SPSG_Args::eIpgInfo,       "ipg_info"sv         },
            { SPSG_Args::eAccVerHistory, "acc_ver_history"sv  },
        };

        for (const auto& item : item_types) {
            string query = string("item_type=") + item.second;
            SPSG_Args args(query);
            BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eItemType>().first == item.first,
                impl_name << ": item_type " << item.second);
        }
    }

    // Test all chunk_type values
    {
        vector<pair<SPSG_Args::EChunkType, string_view>> chunk_types{
            { SPSG_Args::eMeta,           "meta"sv             },
            { SPSG_Args::eData,           "data"sv             },
            { SPSG_Args::eMessage,        "message"sv          },
            { SPSG_Args::eDataAndMeta,    "data_and_meta"sv    },
            { SPSG_Args::eMessageAndMeta, "message_and_meta"sv },
        };

        for (const auto& chunk : chunk_types) {
            string query = string("chunk_type=") + chunk.second;
            SPSG_Args args(query);
            BOOST_CHECK_MESSAGE(args.GetValue<SPSG_Args::eChunkType>().first == chunk.first,
                impl_name << ": chunk_type " << chunk.second);
        }
    }
}

BOOST_AUTO_TEST_CASE(Args)
{
    // Test with SPSG_ArgsVectorImpl
    SPSG_ArgsImpl::Set(false);
    s_TestArgsImpl("VectorImpl");

    // Test with SPSG_ArgsCUrlArgsImpl
    SPSG_ArgsImpl::Set(true);
    s_TestArgsImpl("CUrlArgsImpl");
}

BOOST_AUTO_TEST_CASE(BlobReaderTreatsCanceledItemAsError)
{
    SPSG_Reply::SItem::TTS item_ts;

    {
        auto item_locked = item_ts.GetLock();
        item_locked->state.AddError("Reply canceled by blob reader test", EPSG_Status::eCanceled);
        item_locked->state.SetComplete();
    }

    SPSG_BlobReader reader(item_ts, {});
    size_t pending = numeric_limits<size_t>::max();
    size_t read = numeric_limits<size_t>::max();
    char c = '\0';

    BOOST_CHECK(reader.PendingCount(&pending) == eRW_Error);
    BOOST_CHECK_EQUAL(pending, static_cast<size_t>(0));
    BOOST_CHECK(reader.Read(&c, 1, &read) == eRW_Error);
    BOOST_CHECK_EQUAL(read, static_cast<size_t>(0));
}

BOOST_AUTO_TEST_CASE(ReplyCancelNotifiesItemWaiters)
{
    auto internal_reply = make_shared<SPSG_Reply>("", SPSG_Params{}, make_shared<TPSG_Queue>());

    SPSG_Reply::SItem::TTS* item_ts = nullptr;

    if (auto items_locked = internal_reply->items.GetLock()) {
        items_locked->emplace_back();
        item_ts = &items_locked->back();
    }

    BOOST_REQUIRE(internal_reply);
    BOOST_REQUIRE(item_ts);

    {
        auto item_locked = item_ts->GetLock();
        item_locked->state.AddError("Error preceding cancellation");
    }

    auto wait_started = make_shared<promise<void>>();
    auto wait_started_future = wait_started->get_future();

    auto wait_future = async(launch::async, [item_ts, wait_started]
    {
        auto item_locked = item_ts->GetLock();
        wait_started->set_value();
        return item_ts->WaitUntil(item_locked, CDeadline(1, 0), [&] {
            return !item_locked->state.InProgress();
        });
    });

    BOOST_REQUIRE_MESSAGE(wait_started_future.wait_for(chrono::seconds(1)) == future_status::ready,
        "Item waiter did not start");

    // WaitUntil() atomically releases the item lock while starting to wait.
    // Acquiring the same lock here proves that the worker reached that point.
    {
        auto item_locked = item_ts->GetLock();
    }

    internal_reply->Cancel("Reply canceled by test");

    BOOST_REQUIRE_MESSAGE(wait_future.wait_for(chrono::milliseconds(100)) == future_status::ready,
        "Cancel() did not notify the item waiter promptly");
    BOOST_CHECK(wait_future.get());
    BOOST_CHECK(internal_reply->reply_item.GetLock()->state.GetStatus() == EPSG_Status::eCanceled);
    BOOST_CHECK(item_ts->GetLock()->state.GetStatus() == EPSG_Status::eError);
}

BOOST_AUTO_TEST_SUITE_END()

struct STransportTestEnv
{
    SUv_Loop loop;
    SPSG_Params params;
    SPSG_AsyncQueues queues;
    SPSG_AsyncQueue& queue;
    SPSG_Servers::TTS servers;
    uv_async_t handle = {};
    atomic_int queue_signals = 0;
    bool queue_initialized = false;

    STransportTestEnv() : queue(queues.emplace_back(queues)) { handle.loop = &loop; }

    ~STransportTestEnv()
    {
        if (queue_initialized) {
            queue_initialized = false;
            queue.Close();
        }

        auto servers_locked = servers.GetLock();

        for (auto& server : *servers_locked) {
            server.throttling.StartClose();
        }

        for (auto& server : *servers_locked) {
            server.throttling.FinishClose();
        }

        loop.Run();
    }

    SPSG_Server& AddServer(const string& address, double rate, int available_streams = TPSG_MaxConcurrentRequestsPerServer::GetDefault())
    {
        auto servers_locked = servers.GetLock();
        auto a = SSocketAddress::Parse(address, SSocketAddress::SHost::EName::eOriginal);
        auto l = [&g = servers_locked->server_eligibility_generation, &i = queue_initialized, &q = queues] { ++g; if (i) q.SignalAll(); };
        servers_locked->emplace_back(std::move(a), rate, available_streams, SPSG_ThrottleParams(), &loop, l);
        return servers_locked->operator[](servers_locked->size() - 1);
    }

    void InitQueue()
    {
        if (queue_initialized) {
            return;
        }

        queue.Init(&queue_signals, &loop, [](uv_async_t* handle) { ++*static_cast<atomic_int*>(handle->data); });
        queue_initialized = true;
    }

    void DrainLoopNowait(unsigned times = 4)
    {
        for (unsigned i = 0; i < times; ++i) {
            loop.Run(UV_RUN_NOWAIT);
        }
    }

    shared_ptr<SPSG_Request> MakeRequest()
    {
        auto reply = make_shared<SPSG_Reply>("", params, make_shared<TPSG_Queue>());
        return make_shared<SPSG_Request>(string(), reply, CDiagContext::GetRequestContext().Clone(), params);
    }
};

BEGIN_NCBI_SCOPE

struct SPSG_TestAccess
{
    static void CheckForServerEligibilityChanges(SPSG_IoImpl& io, uv_async_t* handle)
    {
        io.CheckForServerEligibilityChanges();
    }

    static void RunQueue(SPSG_IoImpl& io, uv_async_t* handle)
    {
        io.OnQueue(handle);
    }

    static SPSG_IoSession& CreateSession(SPSG_IoImpl& io, size_t server_index, uv_async_t* handle, const char* reason)
    {
        auto& server_sessions = io.m_Sessions[server_index];
        auto* session = io.ReuseOrCreateSession(server_sessions, handle->loop, nullptr);
        BOOST_REQUIRE_MESSAGE(session, reason);
        return *session;
    }

    static size_t GetLocalSessionCount(const SPSG_IoImpl& io, size_t server_index)
    {
        return io.m_Sessions[server_index].sessions.size();
    }

    static size_t GetAllocatedSessionCount(const SPSG_IoImpl& io, size_t server_index)
    {
        return x_GetAllocatedSessionCount(io.m_Sessions[server_index]);
    }

    static bool HasDrainingSession(const SPSG_IoImpl& io, size_t server_index)
    {
        return x_HasDrainingSession(io.m_Sessions[server_index]);
    }

    static SPSG_IoSession& GetSession(SPSG_IoImpl& io, size_t server_index, size_t session_index)
    {
        return io.m_Sessions[server_index].sessions[session_index];
    }

    static void SetAllocationState(SPSG_IoImpl& io, SPSG_IoSession& session,
            SPSG_IoSession::EAllocationState allocation_state)
    {
        session.SetAllocationState(allocation_state, io.m_AllocatedSessionCount);
    }

    static void SetThrottlingActive(SPSG_Server& server, bool value)
    {
        server.throttling.m_Active.store(value ? SPSG_Throttling::eOnTimer : SPSG_Throttling::eOff);
    }

    static void ConfigureThrottling(SPSG_Server& server, uint64_t period, unsigned max_failures, bool until_discovery = false)
    {
        auto stats_locked = server.throttling.m_Stats.GetLock();
        const_cast<volatile uint64_t&>(stats_locked->params.period) = period;
        stats_locked->params.max_failures = TPSG_ThrottleMaxFailures([max_failures](auto) { return max_failures; });
        stats_locked->params.until_discovery = TPSG_ThrottleUntilDiscovery([until_discovery](auto) { return until_discovery; });
        server.throttling.m_Timer.SetRepeat(period);
    }

    static size_t AcquireSessionIndex(SPSG_IoImpl& io, size_t server_index, uv_async_t* handle)
    {
        auto& server = io.m_Sessions[server_index];
        auto session = io.AcquireSession(server, handle);

        if (!session) {
            return numeric_limits<size_t>::max();
        }

        for (size_t i = 0; i < server.sessions.size(); ++i) {
            if (&server.sessions[i] == session) {
                return i;
            }
        }

        return numeric_limits<size_t>::max();
    }

    static void AddInFlightRequest(SPSG_IoSession& session, int32_t stream_id, SPSG_TimedRequest request)
    {
        session.m_Requests.emplace(stream_id, std::move(request));
    }

    static void FailRequest(SPSG_IoSession& session, shared_ptr<SPSG_Request> req)
    {
        CDiagCollectGuard suppress_expected_warning(eDiag_Error, eDiag_Warning);
        session.Fail(0, std::move(req), SUvNgHttp2_Error("Test failure"));
    }

    static bool ActivateTransport(SPSG_IoSession& session)
    {
        vector<char> buffer;
        return (session.m_Session.Send(buffer) >= 0) && session.HasActiveTransport();
    }

    static void ResetSession(SPSG_IoSession& session, STransportTestEnv& env)
    {
        if (!session.HasActiveTransport()) {
            return;
        }

        session.Reset("Test cleanup", SUv_Tcp::eNormalClose);
        env.DrainLoopNowait();
    }

    static void CompleteRequest(SPSG_IoSession& session, int32_t stream_id)
    {
        session.OnStreamClose(nullptr, stream_id, 0);
    }

    static void RunTimer(SPSG_IoImpl& io)
    {
        io.OnTimer(nullptr);
    }

    static void InvalidateServerEligibility(SPSG_IoImpl& io)
    {
        ++io.m_Servers->server_eligibility_generation;
    }

    static size_t x_GetAllocatedSessionCount(const SPSG_ServerSessions& server_sessions);
    static bool x_HasDrainingSession(const SPSG_ServerSessions& server_sessions);
};

size_t SPSG_TestAccess::x_GetAllocatedSessionCount(const SPSG_ServerSessions& server_sessions)
{
    size_t count = 0;

    for (const auto& session : server_sessions.sessions) {
        if (session.GetAllocationState() == SPSG_IoSession::eAllocated) {
            ++count;
        }
    }

    return count;
}

bool SPSG_TestAccess::x_HasDrainingSession(const SPSG_ServerSessions& server_sessions)
{
    for (const auto& session : server_sessions.sessions) {
        if (session.GetAllocationState() == SPSG_IoSession::eDraining) {
            return true;
        }
    }

    return false;
}

END_NCBI_SCOPE

BOOST_AUTO_TEST_SUITE(PSG)
BOOST_AUTO_TEST_SUITE(IoSessionAllocation)

BOOST_AUTO_TEST_CASE(IdleQueueDoesNotCreateSessions)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10021", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 0U);

    SPSG_TestAccess::RunQueue(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
}

BOOST_AUTO_TEST_CASE(FullSessionSignalsQueueWhenStreamBecomesAvailable)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10003", 1.0);
    env.InitQueue();

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    auto& session = SPSG_TestAccess::CreateSession(io, 0, &env.handle, "session for stream availability test");

    for (int32_t stream_id = 1; !session.IsFull(); ++stream_id) {
        SPSG_TestAccess::AddInFlightRequest(session, stream_id, SPSG_TimedRequest(env.MakeRequest()));
    }

    BOOST_REQUIRE_EQUAL(env.queue_signals.load(), 0);

    session.AddStreams(1);
    env.DrainLoopNowait();

    BOOST_CHECK_EQUAL(env.queue_signals.load(), 1);
}

BOOST_AUTO_TEST_CASE(UnchangedEligibilityDoesNotRebalanceParkableSessions)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10035", 1.0);
    env.AddServer("127.0.0.1:10036", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "first allocated session");
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "second allocated session");

    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 2U);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 2U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);
}

BOOST_AUTO_TEST_CASE(ParksIdleSessionToFreeSlotOnDemand)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10027", 1.0);
    env.AddServer("127.0.0.1:10028", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    const auto max_sessions = env.params.max_sessions.Get();

    for (unsigned i = 0; i < max_sessions; ++i) {
        SPSG_TestAccess::CreateSession(io, 0, &env.handle, "allocated session");
    }

    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), max_sessions);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), size_t{0});
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), max_sessions);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), max_sessions - 1);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 1U);
}

BOOST_AUTO_TEST_CASE(DoesNotParkSessionWithActiveTransportToFreeSlot)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10037", 1.0);
    env.AddServer("127.0.0.1:10038", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    const auto max_sessions = env.params.max_sessions.Get();

    for (unsigned i = 0; i < max_sessions; ++i) {
        SPSG_TestAccess::CreateSession(io, 0, &env.handle, "active allocated session");
        auto& active_session = SPSG_TestAccess::GetSession(io, 0, i);
        BOOST_REQUIRE(active_session.GetAllocationState() == SPSG_IoSession::eAllocated);
        BOOST_REQUIRE(active_session.CanBeParked());
        BOOST_REQUIRE(SPSG_TestAccess::ActivateTransport(active_session));
        BOOST_REQUIRE(active_session.HasActiveTransport());
    }

    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), numeric_limits<size_t>::max());

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), max_sessions);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), max_sessions);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    for (unsigned i = 0; i < max_sessions; ++i) {
        auto& active_session = SPSG_TestAccess::GetSession(io, 0, i);
        BOOST_CHECK(active_session.GetAllocationState() == SPSG_IoSession::eAllocated);
        BOOST_CHECK(!active_session.CanBeParked());
        BOOST_CHECK(active_session.HasActiveTransport());
        SPSG_TestAccess::ResetSession(active_session, env);
    }
}

BOOST_AUTO_TEST_CASE(ParksSessionWithoutActiveTransportToFreeSlot)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10039", 1.0);
    env.AddServer("127.0.0.1:10040", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    const auto max_sessions = env.params.max_sessions.Get();

    for (unsigned i = 0; i < max_sessions; ++i) {
        SPSG_TestAccess::CreateSession(io, 0, &env.handle, "inactive allocated session");
    }

    auto& reclaimable_session = SPSG_TestAccess::GetSession(io, 0, 0);
    BOOST_REQUIRE(reclaimable_session.GetAllocationState() == SPSG_IoSession::eAllocated);
    BOOST_REQUIRE(reclaimable_session.CanBeParked());
    BOOST_REQUIRE(!reclaimable_session.HasActiveTransport());

    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), size_t{0});

    BOOST_CHECK(reclaimable_session.GetAllocationState() == SPSG_IoSession::eParked);
    BOOST_CHECK(!reclaimable_session.HasActiveTransport());
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), max_sessions);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), max_sessions - 1);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 1U);
}

BOOST_AUTO_TEST_CASE(ReenabledServerKeepsBusySessionDrainingUntilCompletion)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10011", 1.0);
    env.AddServer("127.0.0.1:10012", 1.0);
    env.InitQueue();

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");

    auto& session = SPSG_TestAccess::GetSession(io, 0, 0);
    SPSG_TestAccess::AddInFlightRequest(session, 1, SPSG_TimedRequest(env.MakeRequest()));

    first.rate = 0.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_REQUIRE(SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    first.rate = 1.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK(SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 0U);

    SPSG_TestAccess::CompleteRequest(session, 1);
    SPSG_TestAccess::RunTimer(io);
    env.DrainLoopNowait();
    SPSG_TestAccess::RunTimer(io);

    BOOST_CHECK(!SPSG_TestAccess::HasDrainingSession(io, 0));
}

BOOST_AUTO_TEST_CASE(ReenabledServerKeepsParkedSessionUnallocatedUntilNeeded)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10022", 1.0);
    env.AddServer("127.0.0.1:10023", 1.0);
    env.InitQueue();

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");

    auto& session = SPSG_TestAccess::GetSession(io, 0, 0);
    SPSG_TestAccess::AddInFlightRequest(session, 1, SPSG_TimedRequest(env.MakeRequest()));

    first.rate = 0.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    BOOST_REQUIRE(SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);

    SPSG_TestAccess::CompleteRequest(session, 1);
    SPSG_TestAccess::RunTimer(io);
    env.DrainLoopNowait();
    SPSG_TestAccess::RunTimer(io);

    BOOST_REQUIRE(!SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);

    first.rate = 1.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK(!SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
}

BOOST_AUTO_TEST_CASE(ReenabledServerReusesParkedSessionWhenNeeded)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10024", 1.0);
    env.AddServer("127.0.0.1:10025", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");

    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);

    first.rate = 0.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_REQUIRE(!SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);

    first.rate = 1.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 0, &env.handle), size_t{0});
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
}

BOOST_AUTO_TEST_CASE(IneligibleServerMarksBusySessionDraining)
{
    STransportTestEnv env;
    auto& server = env.AddServer("127.0.0.1:10013", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");
    auto& session = SPSG_TestAccess::GetSession(io, 0, 0);
    SPSG_TestAccess::AddInFlightRequest(session, 1, SPSG_TimedRequest(env.MakeRequest()));

    server.rate = 0.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK(SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
}

BOOST_AUTO_TEST_CASE(ThrottledServerParksIdleSessionAndFreesSlot)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10019", 1.0);
    env.AddServer("127.0.0.1:10020", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");

    SPSG_TestAccess::SetThrottlingActive(first, true);
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK(!SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), size_t{0});
}

BOOST_AUTO_TEST_CASE(RequestFailureInvalidatesEligibilityWhenThrottlingActivates)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10041", 1.0);
    env.AddServer("127.0.0.1:10042", 1.0);
    env.InitQueue();

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");
    SPSG_TestAccess::ConfigureThrottling(first, 60000, 1, true);

    auto& session = SPSG_TestAccess::GetSession(io, 0, 0);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);
    BOOST_REQUIRE_EQUAL(env.queue_signals.load(), 0);

    SPSG_TestAccess::FailRequest(session, env.MakeRequest());
    env.DrainLoopNowait();
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK_LT(0, env.queue_signals.load());
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), size_t{0});
}

BOOST_AUTO_TEST_CASE(ReenabledServerReusesSessionAfterDrainCompletes)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10014", 1.0);
    env.AddServer("127.0.0.1:10015", 0.0);
    env.InitQueue();

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "Test");

    auto& session = SPSG_TestAccess::GetSession(io, 0, 0);
    SPSG_TestAccess::AddInFlightRequest(session, 1, SPSG_TimedRequest(env.MakeRequest()));

    first.rate = 0.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    BOOST_REQUIRE(SPSG_TestAccess::HasDrainingSession(io, 0));

    SPSG_TestAccess::CompleteRequest(session, 1);
    SPSG_TestAccess::RunTimer(io);
    env.DrainLoopNowait();
    SPSG_TestAccess::RunTimer(io);

    BOOST_REQUIRE(!SPSG_TestAccess::HasDrainingSession(io, 0));
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);

    first.rate = 1.0;
    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 0, &env.handle), size_t{0});
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
}

BOOST_AUTO_TEST_CASE(PrefersAllocatedSessionOverParkedSession)
{
    STransportTestEnv env;
    env.AddServer("127.0.0.1:10016", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "parked session");
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "allocated session");

    auto& parked_session = SPSG_TestAccess::GetSession(io, 0, 0);
    auto& allocated_session = SPSG_TestAccess::GetSession(io, 0, 1);
    SPSG_TestAccess::SetAllocationState(io, parked_session, SPSG_IoSession::eParked);
    SPSG_TestAccess::SetAllocationState(io, allocated_session, SPSG_IoSession::eAllocated);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 0, &env.handle), size_t{1});
}

BOOST_AUTO_TEST_CASE(OnQueueAppliesEligibilityChangesOnlyAfterInvalidation)
{
    STransportTestEnv env;
    auto& first = env.AddServer("127.0.0.1:10017", 1.0);
    env.AddServer("127.0.0.1:10018", 1.0);

    SPSG_IoImpl io(env.params, env.servers, env.queue);
    SPSG_TestAccess::CheckForServerEligibilityChanges(io, &env.handle);
    SPSG_TestAccess::CreateSession(io, 0, &env.handle, "allocated session");

    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
    BOOST_REQUIRE_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    first.rate = 0.0;
    SPSG_TestAccess::RunQueue(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);

    SPSG_TestAccess::InvalidateServerEligibility(io);
    SPSG_TestAccess::RunQueue(io, &env.handle);

    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 0), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 0), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 0U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::AcquireSessionIndex(io, 1, &env.handle), size_t{0});
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetLocalSessionCount(io, 1), 1U);
    BOOST_CHECK_EQUAL(SPSG_TestAccess::GetAllocatedSessionCount(io, 1), 1U);

    SPSG_TestAccess::GetSession(io, 1, 0).Shutdown();
    env.DrainLoopNowait();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

#endif
