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
 *  Db Cassandra: class generating execution plans for cassandra table fullscans.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__PLAN_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__PLAN_HPP

#include <corelib/ncbistl.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

#include <memory>
#include <vector>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class ICassandraFullscanPlan
{
 public:
    using TQueryPtr = shared_ptr<CCassQuery>;

    virtual TQueryPtr GetNextQuery() = 0;
    virtual size_t    GetQueryCount() const = 0;
    virtual void      Generate() = 0;
    virtual ~ICassandraFullscanPlan() = default;
};

class CCassandraFullscanPlan
    : public ICassandraFullscanPlan
{
    static const size_t kMinPartitionsForSubrangeScanDefault = 100000;
 public:
    using TQueryPtr = shared_ptr<CCassQuery>;

    CCassandraFullscanPlan();
    CCassandraFullscanPlan(const CCassandraFullscanPlan&) = default;
    CCassandraFullscanPlan(CCassandraFullscanPlan&&) = default;
    CCassandraFullscanPlan& operator=(const CCassandraFullscanPlan&) = default;
    CCassandraFullscanPlan& operator=(CCassandraFullscanPlan&&) = default;

    virtual ~CCassandraFullscanPlan() = default;

    CCassandraFullscanPlan& SetConnection(shared_ptr<CCassConnection> connection);
    CCassandraFullscanPlan& SetFieldList(vector<string> fields);
    CCassandraFullscanPlan& SetWhereFilter(string const & where_filter);
    CCassandraFullscanPlan& SetMinPartitionsForSubrangeScan(size_t value);
    CCassandraFullscanPlan& SetKeyspace(string const & keyspace);
    CCassandraFullscanPlan& SetTable(string const & table);
    size_t GetMinPartitionsForSubrangeScan();

    virtual void Generate();
    virtual TQueryPtr GetNextQuery();
    virtual size_t    GetQueryCount() const;
    
 protected:
    CCassConnection::TTokenRanges& GetTokenRanges();

 private:
    size_t GetPartitionCountEstimate();

    shared_ptr<CCassConnection> m_Connection;
    vector<string> m_FieldList;
    string m_Keyspace;
    string m_Table;
    string m_WhereFilter;
    string m_SqlTemplate;
    CCassConnection::TTokenRanges m_TokenRanges;
    size_t m_MinPartitionsForSubrangeScan;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__PLAN_HPP
