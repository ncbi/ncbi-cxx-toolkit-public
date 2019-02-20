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

#include <ncbi_pch.hpp>

#include <numeric>

#include "performance.hpp"

BEGIN_NCBI_SCOPE

size_t SMetrics::CurrentRequestId = 0;

const vector<SRule> SRule::Rules =
{
    { "StartToDone",      { SMetricType::eStart,   SPoint::eFirst }, { SMetricType::eDone,    SPoint::eFirst } },
    { "SendToLastChunk",  { SMetricType::eSend,    SPoint::eFirst }, { SMetricType::eReceive, SPoint::eLast  } },

    { "StartToSend",      { SMetricType::eStart,   SPoint::eFirst }, { SMetricType::eSend,    SPoint::eFirst } },
    { "SendToFirstChunk", { SMetricType::eSend,    SPoint::eFirst }, { SMetricType::eReceive, SPoint::eFirst } },
    { "FirstToLastChunk", { SMetricType::eReceive, SPoint::eFirst }, { SMetricType::eReceive, SPoint::eLast  } },
    { "LastChunkToDone",  { SMetricType::eReceive, SPoint::eLast  }, { SMetricType::eDone,    SPoint::eFirst } },

    { "StartToSubmit",    { SMetricType::eStart,   SPoint::eFirst }, { SMetricType::eSubmit,  SPoint::eFirst } },
    { "SubmitToReply",    { SMetricType::eSubmit,  SPoint::eFirst }, { SMetricType::eReply,   SPoint::eFirst } },
    { "ReplyToDone",      { SMetricType::eReply,   SPoint::eFirst }, { SMetricType::eDone,    SPoint::eFirst } },

    { "PushToPop",        { SMetricType::ePush,    SPoint::eFirst }, { SMetricType::ePop,     SPoint::eFirst } },
};

const vector<pair<size_t, string>> SPercentiles::PercentileTypes =
{
    {   0, "Min"    },
    {  25, "25th"   },
    {  50, "Median" },
    {  75, "75th"   },
    {  90, "90th"   },
    {  95, "95th"   },
    { 100, "Max"    }
};

const char* SMetricType::Name(EType t)
{
    switch (t) {
        case eStart:   return "Start";
        case eSubmit:  return "Submit";
        case eReply:   return "Reply";
        case eDone:    return "Done";
        case eSend:    return "Send";
        case eReceive: return "Receive";
        case ePush:    return "Push";
        case ePop:     return "Pop";

        case eLastType:
        case eError:   break;
    }

    _TROUBLE;
    return "ERROR";
}

void SComplexMetrics::Add(double milliseconds, SMetricType::EType type)
{
    for (size_t i = 0; i < SRule::Rules.size(); ++i) {
        Set(SRule::Rules[i].start, type, milliseconds, m_Data[i].first);
        Set(SRule::Rules[i].stop, type, milliseconds, m_Data[i].second);
    }
};

void SComplexMetrics::Set(const SPoint& point, SMetricType::EType type, double milliseconds, double& value)
{
    // If this rule point is for a different type
    if (point.type != type) return;

    // If it's for the first of the type but first value is already set
    if ((point.index == SPoint::eFirst) && (value > 0.0)) return;

    value = milliseconds;
}

ostream& operator<<(ostream& os, SComplexMetrics::SHeader)
{
    os.setf(ios_base::fixed | ios_base::boolalpha);
    os.precision(3);

    os << "Request";

    for (const auto& rule : SRule::Rules) {
        os << '\t' << rule.name;
    }

    return os << '\t' << "Success";
}

ostream& operator<<(ostream& os, const SComplexMetrics& complex_metrics)
{
    _ASSERT(complex_metrics.m_Data.size() == SRule::Rules.size());

    os << complex_metrics.m_Request;

    for (const auto& metric : complex_metrics.m_Data) {
        auto diff = metric.second - metric.first;
        os << '\t' << setw(7) << diff;
    }

    return os << '\t' << complex_metrics.m_Success;
}

void SPercentiles::Add(const SComplexMetrics& complex_metrics)
{
    for (size_t i = 0; i < SRule::Rules.size(); ++i) {
        m_Data[i].emplace(complex_metrics.Get(i));
    }
}

ostream& operator<<(ostream& os, SPercentiles& percentiles)
{
    for (const auto& type : percentiles.PercentileTypes) {
        os << type.second;

        for (size_t i = 0; i < SRule::Rules.size(); ++i) {
            const auto& data = percentiles.m_Data[i];
            auto it = data.begin();
            if (auto index = data.size() * (type.first / 100.0)) advance(it, index - 1);
            os << '\t' << setw(7) << *it;
        }

        os << '\n';
    }

    os << "Average";

    for (size_t i = 0; i < SRule::Rules.size(); ++i) {
        const auto& data = percentiles.m_Data[i];
        os << '\t' << setw(7) << accumulate(data.begin(), data.end(), 0.0) / data.size();
    }

    return os;
}

SPostProcessing::SPostProcessing(bool raw_metrics) :
    m_RawMetrics(raw_metrics),
    m_StreamBuf(cout.rdbuf())
{
    cout.rdbuf(m_Stream.rdbuf());
}

SPostProcessing::~SPostProcessing()
{
    cerr << "Reading raw metrics: ";

    cout.rdbuf(m_StreamBuf);
    m_Stream.seekg(0);

    map<size_t, vector<SMessage>> raw_data;

    while (m_Stream) {
        size_t request;
        SMessage message;

        if ((m_Stream >> request >> message) && (message.type != SMetricType::eError)) {
            auto old_size = raw_data.size();
            raw_data[request].emplace_back(move(message));
            auto new_size = raw_data.size();
            if ((old_size < new_size) && (new_size % 2000 == 0)) cerr << '.';
        }
    }

    cerr << "\nProcessing raw metrics: ";
    size_t processed = 0;

    vector<SComplexMetrics> complex_metrics;
    SPercentiles percentiles;
    double min = numeric_limits<double>::max();
    double max = numeric_limits<double>::min();

    const string& prefix(CNcbiApplication::Instance()->GetProgramDisplayName());
    ofstream raw_file;

    if (m_RawMetrics) {
        raw_file.open(prefix + ".raw.txt");
        raw_file.setf(ios_base::fixed);
    }

    for (auto& node : raw_data) {
        auto& request = node.first;
        auto& messages = node.second;

        sort(messages.begin(), messages.end());

        auto l = [](const SMessage& m) { return m.type == SMetricType::eDone; };
        auto done = find_if(messages.rbegin(), messages.rend(), l);
        auto success = done != messages.rend() && SMetrics::GetSuccess(done->rest);

        complex_metrics.emplace_back(request, success);
        auto& request_metrics = complex_metrics.back();

        for (const auto& message : messages) {
            auto current = message.milliseconds;

            if (m_RawMetrics) {
                raw_file << request << '\t' << message << '\n';
            }

            if (current < min) min = current;
            if (current > max) max = current;

            request_metrics.Add(current, message.type);
        }

        percentiles.Add(request_metrics);

        if (++processed % 2000 == 0) cerr << '.';
    }

    cerr << "\nPrinting complex metrics: ";
    size_t printed = 0;

    ofstream complex_file(prefix + ".table.txt");
    complex_file << SComplexMetrics::SHeader() << '\n';

    for (const auto& request_metrics : complex_metrics) {
        complex_file << request_metrics << '\n';
        if (++printed % 2000 == 0) cerr << '.';
    }

    complex_file << '\n' << percentiles << "\n\nOverall\t" << setw(7) << (max - min) / milli::den << "\tseconds" << endl;
    cerr << '\n';
}

END_NCBI_SCOPE
