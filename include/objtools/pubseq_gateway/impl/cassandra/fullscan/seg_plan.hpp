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
 *  Db Cassandra: class generating execution plans for cassandra table segmented scans.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__SEG_PLAN_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__SEG_PLAN_HPP

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassandraSegscanPlan
    : public CCassandraFullscanPlan
{
public:
    CCassandraSegscanPlan();

    // by setting Segment we want to run sub-set of the token ranges
    // for example if we have to distribute a task among 5 segments, we run them
    // with segments of {0,5}, {1,5}, {2,5}, {3,5} and {4,5}
    //    where .first is segment index and .second is total segment count
    //
    // In this method we fetch full range of tokens from Cassandra, then remove the token ranges
    // that the other segments will perform
    // in particular, seg #0 will remove tokens that will be handled by seg #1, 2, 3 and 4
    //                seg #1 will remove tokens intended for seg #0, 2, 3 and 4
    //                and so forth
    CCassandraSegscanPlan& SetSegment(pair<size_t, size_t> segment);

    void Generate() override;
private:
    pair<size_t, size_t> m_Segment{make_pair(0, 1)};
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__SEG_PLAN_HPP
