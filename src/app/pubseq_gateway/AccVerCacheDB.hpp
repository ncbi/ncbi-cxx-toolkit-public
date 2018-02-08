#ifndef ACC_VER_CACHE_DB__HPP
#define ACC_VER_CACHE_DB__HPP

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
 * File Description:
 *
 */

#include <string>
#include <vector>
#include <memory>

#include <util/lmdbxx/lmdb++.h>

#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

#include "AccVerCacheStorage.hpp"

using namespace std;


class CAccVerCacheDB
{
public:
    CAccVerCacheDB() :
        m_Env(lmdb::env::create()),
        m_IsReadOnly(true),
        m_MetaDbi({0})
    {}

    CAccVerCacheDB(CAccVerCacheDB&&) = default;
    CAccVerCacheDB(const CAccVerCacheDB&) = delete;
    CAccVerCacheDB& operator= (const CAccVerCacheDB&) = delete;

    void Open(const string &  db_path, bool  initialize, bool  readonly);
    void Close();

    CAccVerCacheStorage &  Storage(void)
    {
        if (!m_Storage)
            EAccVerException::raise("DB is not open");
        return *m_Storage.get();
    }

    DDRPC::DataColumns Columns();
    void SaveColumns(const DDRPC::DataColumns &  clms);

    lmdb::env& Env(void)
    {
        return m_Env;
    }

    bool IsReadOnly(void) const
    {
        return m_IsReadOnly;
    }

    static void s_InitializeDb(const lmdb::env &  env);
    static constexpr const char* const META_DB         = "#META";
    static constexpr const char* const STORAGE_COLUMNS = "DATA_COLUMNS";

private:
    string                              m_DbPath;
    lmdb::env                           m_Env;
    unique_ptr<CAccVerCacheStorage>     m_Storage;
    bool                                m_IsReadOnly;
    lmdb::dbi                           m_MetaDbi;
};


#endif
