#include <ncbi_pch.hpp>

#include <sys/stat.h>
#include <sstream>

#include "AccVerCacheDB.hpp"
#include <objtools/pubseq_gateway/rpc/UtilException.hpp>

using namespace std;

#define DBI_COUNT       6
#define THREAD_COUNT    1024

#define MAP_SIZE_INIT	(256L * 1024 * 1024 * 1024)
#define MAP_SIZE_DELTA	(16L * 1024 * 1024 * 1024)

constexpr const char* const CAccVerCacheDB::META_DB;

/** CAccVerCacheDB */
    
void CAccVerCacheDB::InitializeDb(const lmdb::env& Env) {
    unsigned int flags;
    lmdb::env_get_flags(Env, &flags);
    if (flags & MDB_RDONLY)
        return;
    auto wtxn = lmdb::txn::begin(Env);
    auto meta_dbi = lmdb::dbi::open(wtxn, CAccVerCacheDB::META_DB, MDB_CREATE);
    wtxn.commit();    

    CAccVerCacheStorage::InitializeDb(Env);
}

void CAccVerCacheDB::Open(const string& DbPath, bool Initialize, bool Readonly) {
    m_DbPath = DbPath;
    if (m_DbPath.empty())
        EAccVerException::raise("DB path is not specified");
    if (Initialize && Readonly)
        EAccVerException::raise("DB create & readonly flags are mutually exclusive");

	int stat_rv;
    bool need_sync = !Initialize;
	struct stat st;
	stat_rv = stat(m_DbPath.c_str(), &st);
	if (stat_rv != 0 || st.st_size == 0) {
		/* file does not exist */
		need_sync = false;
		if (Readonly || !Initialize)
            EAccVerException::raise("DB file is not initialized. Run full update first");
	}

    m_Env.set_max_dbs(DBI_COUNT);
    m_Env.set_max_readers(THREAD_COUNT);

    int64_t mapsize = MAP_SIZE_INIT;
	if (stat_rv == 0 && st.st_size + MAP_SIZE_DELTA >  mapsize)
		mapsize = st.st_size + MAP_SIZE_DELTA;

    m_Env.set_mapsize(mapsize);
    m_Env.open(DbPath.c_str(), (Readonly ?  MDB_RDONLY : 0) | MDB_NOSUBDIR | (need_sync ? 0 : (MDB_NOSYNC | MDB_NOMETASYNC)), 0664);
    m_IsReadOnly = Readonly;

    if (Initialize) {
        InitializeDb(m_Env);        
    }

    auto wtxn = lmdb::txn::begin(m_Env, nullptr, Readonly ? MDB_RDONLY : 0);
    m_MetaDbi = lmdb::dbi::open(wtxn, META_DB, 0);
    wtxn.commit();    

    m_Storage.reset(new CAccVerCacheStorage(m_Env));

}

void CAccVerCacheDB::Close() {
    m_Env.close();
}

void CAccVerCacheDB::SaveColumns(const DDRPC::DataColumns& Clms) {
    stringstream os;
    Clms.GetAsBin(os);
    
    auto wtxn = lmdb::txn::begin(m_Env);
    m_MetaDbi.put(wtxn, STORAGE_COLUMNS, os.str());
    wtxn.commit();
}

DDRPC::DataColumns CAccVerCacheDB::Columns() {
    auto rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
    string Data;
    bool found = m_MetaDbi.get(rtxn, STORAGE_COLUMNS, Data);
    rtxn.commit();

    if (!found)
        EAccVerException::raise("DB file is not updated. Run full update first");

    stringstream ss(Data);
    DDRPC::DataColumns rv(ss);
    return rv;
}
