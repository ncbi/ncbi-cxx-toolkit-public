#include <ncbi_pch.hpp>

#include <string>

#include "AccVerCacheStorage.hpp"
#include "AccVerCacheDB.hpp"


/** CAccVerCacheStorage */

constexpr const char* const CAccVerCacheStorage::DATA_DB;

CAccVerCacheStorage::CAccVerCacheStorage(const lmdb::env& Env) :
    m_Env(Env),
    m_Dbi({0}),
    m_IsReadOnly(true)
{
    unsigned int flags = 0;
    unsigned int env_flags = 0;
    
    lmdb::env_get_flags(m_Env, &env_flags);
    m_IsReadOnly = env_flags & MDB_RDONLY;

    auto wtxn = lmdb::txn::begin(Env, nullptr, env_flags & MDB_RDONLY);
    m_Dbi = lmdb::dbi::open(wtxn, DATA_DB, flags);
    wtxn.commit();    
}

void CAccVerCacheStorage::InitializeDb(const lmdb::env& Env) {
    unsigned int flags = MDB_CREATE;
    
    auto wtxn = lmdb::txn::begin(Env);
    lmdb::dbi::open(wtxn, DATA_DB, flags);
    wtxn.commit();    
}

bool CAccVerCacheStorage::Update(const std::string& Key, const std::string& Data, bool CheckIfExists) {
    if (m_IsReadOnly)
        EAccVerException::raise("Can not update: DB open in readonly mode");
    if (CheckIfExists) {
        auto rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
        std::string ExData;
        bool found = m_Dbi.get(rtxn, Key, ExData);
        rtxn.commit();
        if (found && ExData == Data)
            return false;
    }
    auto wtxn = lmdb::txn::begin(m_Env);
    m_Dbi.put(wtxn, Key, Data);
    wtxn.commit();
    return true;
}

bool CAccVerCacheStorage::Get(const std::string& Key, std::string& Data) const {
    auto rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
    bool found = m_Dbi.get(rtxn, Key, Data);
    rtxn.commit();
    return found;
}
