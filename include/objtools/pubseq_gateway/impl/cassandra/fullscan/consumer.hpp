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
 *  Db Cassandra: interface of cassandra fullscan record consumer.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__CONSUMER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__CONSUMER_HPP

#include <corelib/ncbistl.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

#include <memory>
#include <functional>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class ICassandraFullscanConsumer
{
 public:
    // Called for every iteration
    // @return false to stop processing
    virtual bool Tick()
    {
        return true;
    };

    // Called for every row
    // @return false to stop processing
    virtual bool ReadRow(CCassQuery const & query) = 0;

    // Called once before consumer destruction
    virtual void Finalize() = 0;

    virtual ~ICassandraFullscanConsumer() = default;
};

using TCassandraFullscanConsumerFactory = function<unique_ptr<ICassandraFullscanConsumer>()>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__CONSUMER_HPP
