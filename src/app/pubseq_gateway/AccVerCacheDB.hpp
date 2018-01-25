
#ifndef _ACC_VER_CACHE_DB_HPP_
#define _ACC_VER_CACHE_DB_HPP_

#include <string>
#include <vector>
#include <memory>

#include "lmdbxx/lmdb++.h"

#include "UtilException.hpp"
#include "DdRpcDataPacker.hpp"

#include "AccVerCacheStorage.hpp"

class CAccVerCacheDB {
private:
    std::string m_DbPath;
    lmdb::env m_Env;
    std::unique_ptr<CAccVerCacheStorage> m_Storage;
    bool m_IsReadOnly;
    lmdb::dbi m_MetaDbi;
public:
    CAccVerCacheDB() : 
        m_Env(lmdb::env::create()),
        m_IsReadOnly(true),
        m_MetaDbi({0})
    {}
    CAccVerCacheDB(CAccVerCacheDB&&) = default;
    CAccVerCacheDB(const CAccVerCacheDB&) = delete;
    CAccVerCacheDB& operator= (const CAccVerCacheDB&) = delete;
    
    void Open(const std::string& DbPath, bool Initialize, bool Readonly);
    void Close();
    
    CAccVerCacheStorage& Storage() {
        if (!m_Storage)
            EAccVerException::raise("DB is not open");
        return *m_Storage.get();
    }

    DDRPC::DataColumns Columns();
    void SaveColumns(const DDRPC::DataColumns& Clms);

    lmdb::env& Env() {
        return m_Env;
    }
    bool IsReadOnly() const {
        return m_IsReadOnly;
    }

    static void InitializeDb(const lmdb::env& Env);
    static constexpr const char* const META_DB         = "#META";
    static constexpr const char* const STORAGE_COLUMNS = "DATA_COLUMNS";
};


#endif
