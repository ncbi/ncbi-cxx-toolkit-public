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


BEGIN_NCBI_SCOPE


void CRateMonitor::Mark(Uint8 pos, double time)
{
    if (!m_Data.empty()) {
        if (m_Data.front().first  > pos  ||
            m_Data.front().second > time) {
            return;  // invalid input silently ignored
        }
        while (m_Data.front().second > m_Data.back().second + kMaxSpan)
            m_Data.pop_back();
        if (m_Data.size() > 1) {
            list<TMark>::const_iterator it = m_Data.begin();
            if ((++it)->second + kMinSpan < time) {
                // update only
                m_Data.front().first  = pos;
                m_Data.front().second = time;
                m_Rate = 0.0;
                return;
            }
        }
    }
    // new mark
    m_Data.push_front(make_pair(pos, time));
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

    if (n == 2) {
        double dt = m_Data.front().second - m_Data.back().second;
        if (dt < kMinSpan)
            return GetPace();
        gaps.push_back(make_pair(m_Data.front().first -
                                 m_Data.back ().first, dt));
    } else {
        TMark prev = m_Data.front();
        _ASSERT(prev.first - m_Data.back().first > kMinSpan);
        for (list<TMark>::const_iterator it = ++m_Data.begin();
             it != m_Data.end();  ++it) {
            TMark next = *it;
            double dt = prev.second - next.second;
            if (dt < kMinSpan) {
                _ASSERT(it == ++m_Data.begin());
                continue;
            }
            gaps.push_back(make_pair(prev.first - next.first, dt));
            prev = next;
        }
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
        double w = weight * 0.9;
        m_Rate  += rate * w;
        weight  -= w;
    }

    _ASSERT(m_Rate);
    return m_Rate;
}


double CRateMonitor::GetETA(void) const
{
    if (!m_Size)
        return  0.0;
    Uint8 pos = GetPos();
    if (!pos)  // NB: This check ensures rate != 0.0 later
        return -1.0;
    if (pos < m_Size) {
        double eta = (m_Size - pos) / GetRate();
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
        // NB: Essentially, this is the same formula as in GetETA(),
        //     if to note that rate = pos / time in this case.
        time = time * m_Size / pos - time;
        if (time < kMinSpan)
            time = 0.0;
        return time;
    }
    return 0.0;
}


END_NCBI_SCOPE
