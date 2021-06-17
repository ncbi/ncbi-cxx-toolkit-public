/*****************************************************************************
 *  $Id$
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
 *  Db Cassandra: class generating execution plans for cassandra table scans with filtered token ranges.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__FILTERED_PLAN_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__FILTERED_PLAN_HPP

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassandraFilteredPlan: public CCassandraFullscanPlan
{
 public:
    CCassandraFilteredPlan() = default;
    CCassandraFilteredPlan& SetFilterRanges(CCassConnection::TTokenRanges ranges)
    {
        if (ranges.size()) {
            pair<CCassConnection::TTokenValue, CCassConnection::TTokenValue> prev = {0,0};
            for (auto range : ranges) {
                if (range.first >= range.second) {
                    NCBI_USER_THROW("Illegal filter range - " + to_string(range.first) + ":" + to_string(range.second));
                }
                if (prev.first != 0 || prev.second != 0) {
                    if (range.first < prev.second) {
                        NCBI_USER_THROW("Unsorted filter range - " + to_string(range.first) + ":" + to_string(range.second));
                    }
                }
                prev = range;
            }
        }
        m_Filter = ranges;
        return *this;
    }

    void Generate() override
    {
        // Subrange scan should be used for filtered plans
        SetMinPartitionsForSubrangeScan(0);
        CCassandraFullscanPlan::Generate();
        if (m_Filter.size()) {
            CCassConnection::TTokenRanges filtered_ranges;
            CCassConnection::TTokenRanges& ranges = GetTokenRanges();
            for (auto range : ranges) {
                auto range_begin = range.first, range_end = range.second;
                for (auto filter : m_Filter) {
                    auto filter_begin = filter.first, filter_end = filter.second;
                    if (range_begin < filter_end && range_end > filter_begin) {
                        filtered_ranges.push_back({
                            max(range_begin, filter_begin),
                            min(range_end, filter_end)
                        });
                    }
                }
            }

            /*for (auto range : filtered_ranges) {
                cout << "Filtered range: " << range.first << ":" << range.second << endl;
            }*/

            swap(ranges, filtered_ranges);
        }
    }

    /*TQueryPtr GetNextQuery() override
    {
        auto query = CCassandraFullscanPlan::GetNextQuery();
        if (query) {
            cout << query->ToString() << endl;
        }
        return query;
    }*/

 private:
    CCassConnection::TTokenRanges m_Filter;
};

END_IDBLOB_SCOPE

#endif
