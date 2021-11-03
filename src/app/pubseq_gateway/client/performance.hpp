#ifndef APP__PUBSEQ_GATEWAY__CLIENT__PERFORMANCE_HPP
#define APP__PUBSEQ_GATEWAY__CLIENT__PERFORMANCE_HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <array>
#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

#include <corelib/ncbidbg.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

BEGIN_NCBI_SCOPE

struct SMetricType
{
    enum EType : size_t
    {
        // External metrics (collected by SMetrics)
        eStart,
        eSubmit,
        eReply,
        eDone,
        eSize,
        eError = eSize,

        // Internal metrics (must correspond to values of SDebugPrintout::EType)
        eSend = 1000,
        eReceive,
        eClose,
        eRetry,
        eFail,

        // Should always be the last
        eLastType
    };

    static const char* Name(EType t);
};

struct SMetrics : string, private SMetricType
{
    using time_point = chrono::steady_clock::time_point;
    using duration = chrono::duration<double, milli>;

    SMetrics(string id) : string(move(id)) {}
    ~SMetrics() { cout << *this; }

    void Set(EType t)
    {
        _ASSERT(t < eSize);
        m_Data[t] = chrono::steady_clock::now();
    }

    using TItem = pair<CPSG_ReplyItem::EType, EPSG_Status>;
    void AddItem(TItem item) { m_Items.emplace_back(move(item)); }

private:
    duration::rep Get(EType t) const { return duration(m_Data[t].time_since_epoch()).count(); }
    void OutputItems(ostream& os) const;

    array<time_point, eSize> m_Data;
    vector<TItem> m_Items;

    friend ostream& operator<<(ostream& os, const SMetrics& metrics)
    {
        os << fixed << boolalpha;

        for (auto t : { eStart, eSubmit, eReply, eDone}) {
            os << string(metrics) << '\t' <<
                metrics.Get(t) << '\t' << t << '\t' << this_thread::get_id();
           
            if (t == eDone) {
                metrics.OutputItems(os);
            }

            os << '\n';
        }

        return os.flush();
    }
};

struct SMessage
{
    double milliseconds;
    SMetricType::EType type = SMetricType::eError;
    string thread_id;
    string rest;

    bool operator<(const SMessage& rhs) const
    {
        return milliseconds < rhs.milliseconds;
    }

    template <SMetricType::EType type>
    static bool IsSameType(const SMessage& message) { return type == message.type; }

    friend istream& operator>>(istream& is, SMessage& message)
    {
        size_t type;
        is >> message.milliseconds >> type >> message.thread_id;

        if (!is) return is;

        switch (type) {
            case SMetricType::eStart:
            case SMetricType::eSubmit:
            case SMetricType::eReply:
            case SMetricType::eDone:
            case SMetricType::eSend:
            case SMetricType::eReceive:
            case SMetricType::eClose:
            case SMetricType::eRetry:
            case SMetricType::eFail:
                message.type = static_cast<SMetricType::EType>(type);
                break;

            default:
                message.type = SMetricType::eError;
                break;
        }

        // Read the rest of the line if the converion above has failed
        is.clear();
        return getline(is, message.rest);
    }

    friend ostream& operator<<(ostream& os, const SMessage& message)
    {
        return os << message.milliseconds << '\t' <<
            SMetricType::Name(message.type) << '\t' <<
            message.thread_id <<
            message.rest;
    }
};

struct SIoRedirector
{
    SIoRedirector(ios& what, ios& to) :
        m_What(what),
        m_To(to),
        m_Buf(m_What.rdbuf())
    {
        m_What.rdbuf(m_To.rdbuf());
    }

    ~SIoRedirector() { Reset(); }
    void Reset() { m_What.rdbuf(m_Buf); }

private:
    ios& m_What;
    ios& m_To;
    streambuf* m_Buf;
};

END_NCBI_SCOPE

#endif
