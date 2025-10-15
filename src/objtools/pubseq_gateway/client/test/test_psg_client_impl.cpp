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

#include <corelib/test_boost.hpp>

#include <algorithm>
#include <barrier>
#include <deque>
#include <thread>
#include <random>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

const char kAllowedChars[] = "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

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
        CDeadline deadline(kReadingDeadline, 0);

        while (!deadline.IsExpired()) {
            size_t read = 0;
            auto reading_result = read_impl(r, received.data(), received.size(), expected_to_read, &read);

            if (reading_result < 0) return;

            BOOST_REQUIRE_MESSAGE_MT_SAFE(read <= expected_to_read, "Received more data than expected");
            BOOST_REQUIRE_MESSAGE_MT_SAFE(equal(&received[0], &received[read], expected), "Received data does not match expected");

            expected += read;
            expected_to_read -= read;

            if (reading_result == 0) {
                const auto& src_messages = src.second;
                size_t messages = 0;

                while (auto message = dst.GetLock()->state.GetMessage(eDiag_Trace)) {
                    ++messages;
                    auto it = find(src_messages.begin(), src_messages.end(), message);
                    BOOST_REQUIRE_MESSAGE_MT_SAFE(it != src_messages.end(), "Received message does not match expected");
                }

                BOOST_REQUIRE_MESSAGE_MT_SAFE(messages >= src_messages.size(), "Received less messages than expected");
                BOOST_REQUIRE_MESSAGE_MT_SAFE(messages <= src_messages.size(), "Received more messages than expected");
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
                auto blob_id = item_locked->args.GetValue("blob_id");
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
        auto blob_id = item.args.GetValue("blob_id");

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

struct SBlobReader
{
    SBlobReader(SPSG_Reply::SItem::TTS& dst) : reader(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t expected, size_t* read)
    {
        assert(buf);
        assert(read);

        auto pending_result = reader.PendingCount(read);

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

        BOOST_ERROR_MT_SAFE("Read() failed: " << g_RW_ResultToString(reading_result));
        return -1;
    }

private:
    SPSG_BlobReader reader;
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
    SStreamReadsome(SPSG_Reply::SItem::TTS& dst) : is(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t, size_t* read)
    {
        auto to_read = r.Get(1, buf_size);
        *read = is.readsome(buf, to_read);

        if (*read) {
            return 1;
        } else if (is.eof()) {
            return 0;
        } else {
            return -1;
        }
    }

private:
    SPSG_RStream is;
};

BOOST_AUTO_TEST_CASE(StreamReadsome)
{
    MtReading<SStreamReadsome>();
}

struct SStreamRead
{
    SStreamRead(SPSG_Reply::SItem::TTS& dst) : is(dst) {}

    int operator()(SRandom& r, char* buf, size_t buf_size, size_t, size_t* read)
    {
        auto to_read = r.Get(1, buf_size);

        if (is.read(buf, to_read)) {
            *read = is.gcount();
            return 1;
        } else if (is.eof()) {
            *read = is.gcount();
            return 0;
        } else {
            return -1;
        }
    }

private:
    SPSG_RStream is;
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
    atomic_int notified = 0;

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

BOOST_AUTO_TEST_CASE(CV)
{
    const auto kTimeout = CTimeout(0.5);
    const auto kIterations = 10;
    const auto kThreads = 10;
    SRandom r;
    SPSG_CV<void> cv;
    atomic_int notified = 0;
    atomic_bool stopped = false;

    auto thread_impl1 = [&](barrier<>& b) { [[maybe_unused]] auto u = b.arrive(); if (cv.WaitUntil(kTimeout))          ++notified; };
    auto thread_impl2 = [&](barrier<>& b) { [[maybe_unused]] auto u = b.arrive(); if (cv.WaitUntil(stopped, kTimeout)) ++notified; };

    for (auto i = kIterations; i > 0; --i) {
        int n = r.Get(2, kThreads);
        {
            auto threads = s_CreateThreadsAndWait(n, thread_impl1);
            cv.NotifyOne();
        }
        BOOST_CHECK_EQUAL_MT_SAFE(notified.exchange(0), 1);

        {
            auto threads = s_CreateThreadsAndWait(n, thread_impl2);
            cv.NotifyOne();
        }
        BOOST_CHECK_EQUAL_MT_SAFE(notified.exchange(0), 1);
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

BOOST_AUTO_TEST_SUITE_END()

#endif
