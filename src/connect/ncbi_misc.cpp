/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Miscellaneous C++ connect stuff
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <connect/ncbi_misc.hpp>

#define DEBUG_RATE_MONITOR 1

#ifdef DEBUG_RATE_MONITOR
#  include <iterator>  // distance()
#endif //DEBUG_RATE_MONITOR


BEGIN_NCBI_SCOPE


#ifdef DEBUG_RATE_MONITOR
static void x_PrintList(const list<CRateMonitor::TMark>& data)
{
    CRateMonitor::TMark prev;
    list<CRateMonitor::TMark>::const_iterator it = data.begin();
    cout << data.size() << ':' << endl;
    for (size_t n = 0;  n < data.size();  n++) {
        CRateMonitor::TMark next = *it;
        cout << n << ":\t"
            "p = " << next.first << ",\t"
            "t = " << next.second;
        if (n) {
            cout << ",\t"
                "dp = " << prev.first  - next.first  << ",\t"
                "dt = " << prev.second - next.second << endl;
        } else
            cout << endl;
        prev = next;
        ++it;
    }
}
#endif //DEBUG_RATE_MONITOR


void CRateMonitor::Mark(Uint8 pos, double time)
{
    if (!m_Data.empty()) {
        if (m_Data.front().first  > pos  ||
            m_Data.front().second > time) {
            return;  // invalid input silently ignored
        }
        while (m_Data.front().second > m_Data.back().second + kMaxSpan) {
            m_Data.pop_back();
        }
        if (m_Data.size() > 1) {
            list<TMark>::const_iterator it;
            if (m_Data.front().first == pos  ||  m_Data.front().second == time
                ||  time - (++(it = m_Data.begin()))->second < kSpan
                ||  m_Data.front().second -       it->second < kSpan) {
                // update only
                m_Data.front().first  = pos;
                m_Data.front().second = time;
#ifdef DEBUG_RATE_MONITOR
                cout << "UPDATED" << endl;
                x_PrintList(m_Data);
#endif //DEBUG_RATE_MONITOR
                m_Rate = 0.0;
                return;
            }
        }
    }
    // new mark
    m_Data.push_front(make_pair(pos, time));
#ifdef DEBUG_RATE_MONITOR
    cout << "ADDED" << endl;
    x_PrintList(m_Data);
#endif //DEBUG_RATE_MONITOR
    m_Rate = 0.0;
}


double CRateMonitor::GetRate(void) const
{
    if (m_Rate > 0.0)
        return m_Rate;
    size_t n = m_Data.size();
    if (n < 2)
        return GetPace();

    list<TMark> gaps;

    if (n > 2) {
        TMark prev = m_Data.front();
        list<TMark>::const_iterator it = m_Data.begin();
        _ASSERT(prev.first - m_Data.back().first > kSpan);
        for (++it;  it != m_Data.end();  ++it) {
            TMark next = *it;
            double dt = prev.second - next.second;
            if (dt < kSpan) {
#ifdef DEBUG_RATE_MONITOR
                cout << "dt = " << dt << ",\td =" << (kSpan - dt)
                     << ",\tn = " << distance(m_Data.begin(), it) << endl;
#endif //DEBUG_RATE_MONITOR
                _DEBUG_ARG(list<TMark>::const_iterator beg = m_Data.begin());
                _ASSERT(it == ++beg);
                continue;
            }
            gaps.push_back(make_pair(prev.first - next.first, dt));
            prev = next;
        }
    } else {
        double dt = m_Data.front().second - m_Data.back().second;
        if (dt < kSpan)
            return GetPace();
        gaps.push_back(make_pair(m_Data.front().first -
                                 m_Data.back ().first, dt));
    }

    _ASSERT(!gaps.empty()  &&  !m_Rate);

    double weight = 1.0;
    for (;;) {
        double rate = gaps.front().first / gaps.front().second;
        gaps.pop_front();
        if (gaps.empty()) {
            m_Rate += rate * weight;
            break;
        }
        double w = weight * kWeight;
        m_Rate  += rate * w;
        weight  -= w;
    }
    return m_Rate;
}


double CRateMonitor::GetETA(void) const
{
    if (!m_Size)
        return  0.0;
    Uint8 pos = GetPos();
    if (pos < m_Size) {
        double rate = GetRate();
        if (!rate)
            return -1.0;
        double eta = (m_Size - pos) / rate;
        if (eta < kMinSpan)
            eta = 0.0;
        return eta;
    }
    return 0.0;
}


double CRateMonitor::GetTimeRemaining(void) const
{
    if (!m_Size)
        return  0.0;
    Uint8 pos = GetPos();
    if (!pos)
        return -1.0;
    if (pos < m_Size) {
        double time = m_Data.front().second;
        // NB: Essentially, there is the same formula as in GetETA(),
        //     if to notice that rate = pos / time in this case.
        time = time * m_Size / pos - time;
        if (time < kMinSpan)
            time = 0.0;
        return time;
    }
    return 0.0;
}


END_NCBI_SCOPE
