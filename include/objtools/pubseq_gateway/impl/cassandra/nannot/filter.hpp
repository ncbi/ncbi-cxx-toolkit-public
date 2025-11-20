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
 *  Blob storage: named annotation filter to remove duplicates during data migration
 *  @temporary
 *  @see ID-7327
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__FILTER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__FILTER_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>

#include <functional>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>

#include "../IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class NCBI_STD_DEPRECATED("CNAnnotFilter is deprecated and will be removed as soon as CXX-13570 is closed")
CNAnnotFilter {
 public:
    using TConsume = function<void(int32_t, CNAnnotRecord&&)>;

    CNAnnotFilter() = default;
    CNAnnotFilter(CNAnnotFilter const &) = delete;
    CNAnnotFilter(CNAnnotFilter&&) = default;
    CNAnnotFilter& operator=(CNAnnotFilter const &) = delete;
    CNAnnotFilter& operator=(CNAnnotFilter&&) = default;

    void Store(int32_t sat, CNAnnotRecord&& nannot)
    {
        CFastMutexGuard guard(m_UpdateMutex);
        m_NAnnotItems[sat].emplace_back(move(nannot));
    }

    void Consume(TConsume fn)
    {
        auto cmp = [](CNAnnotRecord const* a, CNAnnotRecord const* b)
        {
            return (a->GetAccession() == b->GetAccession())
                ? (a->GetVersion() == b->GetVersion())
                    ? (a->GetSeqIdType() == b->GetSeqIdType())
                        ? (a->GetAnnotName() > b->GetAnnotName())
                        : (a->GetSeqIdType() > b->GetSeqIdType())
                    : (a->GetVersion() > b->GetVersion())
                : (a->GetAccession() > b->GetAccession());

        };
        set<CNAnnotRecord*, decltype(cmp)> consumed_items(cmp);
        for (auto itr = m_NAnnotItems.begin(); itr != m_NAnnotItems.end(); ++itr) {
            set<CNAnnotRecord*, decltype(cmp)> consumed_items_sat(cmp);
            for (auto & annot : itr->second) {
                if (consumed_items.find(&annot) == consumed_items.end()) {
                    fn(itr->first, move(annot));
                    consumed_items_sat.emplace(&annot);
                }
            }
            if (itr != m_NAnnotItems.rbegin().base()) {
                consumed_items.insert(consumed_items_sat.begin(), consumed_items_sat.end());
            }
        }
    }

 private:
    CFastMutex m_UpdateMutex;
    map<int32_t, vector<CNAnnotRecord>, greater<int32_t>> m_NAnnotItems;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__FILTER_HPP
