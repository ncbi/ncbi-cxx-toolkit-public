#ifndef PSG_CACHE_BASE__HPP
#define PSG_CACHE_BASE__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description: Base class for all cache subclasses
 *
 */

#include <memory>
#include <string>
#include <utility>

#include <util/lmdbxx/lmdb++.h>

#include <corelib/ncbistd.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CLMDBReadOnlyTxn
{
 public:
    CLMDBReadOnlyTxn()
        : m_Txn(nullptr)
    {
    }

    explicit CLMDBReadOnlyTxn(lmdb::txn&& txn)
        : m_Txn(move(txn))
    {
    }

    ~CLMDBReadOnlyTxn()
    {
        m_Txn.commit();
    }

    CLMDBReadOnlyTxn(CLMDBReadOnlyTxn&&) = default;
    CLMDBReadOnlyTxn(const CLMDBReadOnlyTxn&) = delete;
    CLMDBReadOnlyTxn& operator=(CLMDBReadOnlyTxn&&) = default;
    CLMDBReadOnlyTxn& operator=(const CLMDBReadOnlyTxn&) = delete;

    operator MDB_txn*() const
    {
        return m_Txn.handle();
    }

 private:
    lmdb::txn m_Txn;
};

class CPubseqGatewayCacheBase
{
 public:
    explicit CPubseqGatewayCacheBase(const string& file_name);
    virtual ~CPubseqGatewayCacheBase();
    // @throws lmdb::error
    void Open();

 protected:
    CLMDBReadOnlyTxn BeginReadTxn();
    string m_FileName;
    unique_ptr<lmdb::env> m_Env;
};

END_IDBLOB_SCOPE

#endif  // PSG_CACHE_BASE__HPP
