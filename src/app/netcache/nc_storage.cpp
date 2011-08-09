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
 * Author: Pavel Ivanov
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/request_ctx.hpp>

#include "nc_storage.hpp"
#include "netcached.hpp"
#include "nc_stat.hpp"
#include "message_handler.hpp"
#include "nc_memory.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/mman.h>
#endif


BEGIN_NCBI_SCOPE


static const char* kNCStorage_DBFileExt       = ".db";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_RegSection        = "storage";
static const char* kNCStorage_PathParam         = "path";
static const char* kNCStorage_FilePrefixParam   = "prefix";
static const char* kNCStorage_FileSizeParam     = "each_file_size";
static const char* kNCStorage_GarbagePctParam   = "max_garbage_pct";
static const char* kNCStorage_MinDBSizeParam    = "min_storage_size";
static const char* kNCStorage_MoveLifeParam     = "min_lifetime_to_move";
static const char* kNCStorage_MoveSizeParam     = "max_move_operation_size";
static const char* kNCStorage_GCBatchParam      = "gc_batch_size";
static const char* kNCStorage_FlushSizeParam    = "sync_size_period";
static const char* kNCStorage_FlushTimeParam    = "sync_time_period";
static const char* kNCStorage_ExtraGCOnParam    = "db_limit_del_old_on";
static const char* kNCStorage_ExtraGCOffParam   = "db_limit_del_old_off";
static const char* kNCStorage_StopWriteOnParam  = "db_limit_stop_write_on";
static const char* kNCStorage_StopWriteOffParam = "db_limit_stop_write_off";
static const char* kNCStorage_MinFreeDiskParam  = "disk_free_limit";


/// Size of memory page that is a granularity of all allocations from OS.
static const size_t kMemPageSize  = 4 * 1024;
/// Mask that can move pointer address or memory size to the memory page
/// boundary.
static const size_t kMemPageAlignMask = ~(kMemPageSize - 1);
static const size_t kMemSizeTolerance = 1 * 1024 * 1024;

static const int kDeadTimeDead = 1;
static const int kDeadTimeUnfinished = numeric_limits<int>::max();




bool
CNCBlobStorage::x_EnsureDirExist(const string& dir_name)
{
    CDir dir(dir_name);
    if (!dir.Exists()) {
        return dir.Create();
    }
    return true;
}

bool
CNCBlobStorage::x_ReadVariableParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_GCBatchSize = reg.GetInt(kNCStorage_RegSection, kNCStorage_GCBatchParam, 500);

    string str = reg.GetString(kNCStorage_RegSection, kNCStorage_FileSizeParam, "1Gb");
    m_NewFileSize = Uint4(NStr::StringToUInt8_DataSize(str));
    m_MaxGarbagePct = reg.GetInt(kNCStorage_RegSection, kNCStorage_GarbagePctParam, 50);
    str = reg.GetString(kNCStorage_RegSection, kNCStorage_MinDBSizeParam, "10Gb");
    m_MinDBSize = NStr::StringToUInt8_DataSize(str);
    m_MinMoveLife = reg.GetInt(kNCStorage_RegSection, kNCStorage_MoveLifeParam, 600);
    str = reg.GetString(kNCStorage_RegSection, kNCStorage_MoveSizeParam, "1Mb");
    m_MaxMoveSize = Uint4(NStr::StringToUInt8_DataSize(str));

    str = reg.GetString(kNCStorage_RegSection, kNCStorage_FlushSizeParam, "100Mb");
    m_FlushSizePeriod = Uint4(NStr::StringToUInt8_DataSize(str));
    m_FlushTimePeriod = reg.GetInt(kNCStorage_RegSection, kNCStorage_FlushTimeParam, 15);

    m_ExtraGCOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOnParam, "0"));
    m_ExtraGCOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOffParam, "0"));
    m_StopWriteOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOnParam, "0"));
    m_StopWriteOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOffParam, "0"));
    m_DiskFreeLimit  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_MinFreeDiskParam, "5Gb"));

    return true;
}

bool
CNCBlobStorage::x_ReadStorageParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_Path = reg.Get(kNCStorage_RegSection, kNCStorage_PathParam);
    m_Prefix   = reg.Get(kNCStorage_RegSection, kNCStorage_FilePrefixParam);
    if (m_Path.empty()  ||  m_Prefix.empty()) {
        ERR_POST(Critical <<  "Incorrect parameters for " << kNCStorage_PathParam
                          << " and " << kNCStorage_FilePrefixParam
                          << " in the section '" << kNCStorage_RegSection << "': '"
                          << m_Path << "' and '" << m_Prefix << "'");
        return false;
    }
    if (!x_EnsureDirExist(m_Path))
        return false;
    m_GuardName = CDirEntry::MakePath(m_Path,
                                      kNCStorage_StartedFileName,
                                      m_Prefix);
    try {
        return x_ReadVariableParams();
    }
    catch (CStringException& ex) {
        ERR_POST(Critical << "Bad configuration: " << ex);
        return false;
    }
}

string
CNCBlobStorage::x_GetIndexFileName(void)
{
    string file_name(m_Prefix);
    file_name += kNCStorage_IndexFileSuffix;
    file_name = CDirEntry::MakePath(m_Path, file_name, kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

string
CNCBlobStorage::x_GetFileName(Uint4 file_id)
{
    string file_name(m_Prefix);
    file_name += NStr::UIntToString(file_id);
    file_name = CDirEntry::MakePath(m_Path, file_name, kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

bool
CNCBlobStorage::x_LockInstanceGuard(void)
{
    m_CleanStart = !CFile(m_GuardName).Exists();

    try {
        if (m_CleanStart) {
            // Just create the file with read and write permissions
            CFileWriter tmp_writer(m_GuardName, CFileWriter::eOpenAlways);
            CFile guard_file(m_GuardName);
            CFile::TMode user_mode, grp_mode, oth_mode;

            guard_file.GetMode(&user_mode, &grp_mode, &oth_mode);
            user_mode |= CFile::fRead;
            guard_file.SetMode(user_mode, grp_mode, oth_mode);
        }

        m_GuardLock = new CFileLock(m_GuardName,
                                    CFileLock::fDefault,
                                    CFileLock::eExclusive);
        if (!m_CleanStart  &&  CFile(m_GuardName).GetLength() == 0)
            m_CleanStart = true;
        if (!m_CleanStart) {
            INFO_POST("NetCache wasn't finished cleanly in previous run. "
                      "Will try to work with storage as is.");
        }

        CFileIO writer;
        writer.SetFileHandle(m_GuardLock->GetFileHandle());
        writer.SetFileSize(0, CFileIO::eBegin);
        string pid(NStr::UIntToString(CProcess::GetCurrentPid()));
        writer.Write(pid.c_str(), pid.size());
    }
    catch (CFileErrnoException& ex) {
        ERR_POST(Critical << "Can't lock the database (other instance is running?): " << ex);
        return false;
    }
    return true;
}

inline void
CNCBlobStorage::x_UnlockInstanceGuard(void)
{
    _ASSERT(m_GuardLock.get());

    m_GuardLock.reset();
    CFile(m_GuardName).Remove();
}

bool
CNCBlobStorage::x_OpenIndexDB(void)
{
    string index_name = x_GetIndexFileName();
    for (int i = 0; i < 2; ++i) {
        try {
            m_IndexDB = new CNCDBIndexFile(index_name);
            m_IndexDB->CreateDatabase();
            m_IndexDB->GetAllDBFiles(&m_DBFiles);
            ERASE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
                SNCDBFileInfo* info = it->second;
#ifdef NCBI_OS_LINUX
                info->fd = open(info->file_name.c_str(), O_RDWR);
                if (info->fd == -1) {
                    ERR_POST(Critical << "Cannot open storage file, errno="
                                      << errno);
                    delete info;
                    m_DBFiles.erase(it);
                }
#endif
            }
            return true;
        }
        catch (CSQLITE_Exception& ex) {
            m_IndexDB.reset();
            ERR_POST("Index file is broken, reinitializing storage. " << ex);
            CFile(index_name).Remove();
        }
    }
    ERR_POST(Critical << "Cannot open or create index file for the storage.");
    return false;
}

void
CNCBlobStorage::x_CleanDatabase(void)
{
    INFO_POST("Reinitializing storage " << m_Prefix << " at " << m_Path);

    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CFile file(it->second->file_name);
        file.Remove();
        delete it->second;
    }
    m_DBFiles.clear();
    m_IndexDB->DeleteAllDBFiles();
}

bool
CNCBlobStorage::x_ReinitializeStorage(void)
{
    try {
        x_CleanDatabase();
        return true;
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST("Error in soft reinitialization, trying harder. " << ex);
        m_IndexDB.reset();
        // GCC 3.0.4 doesn't allow to merge the following 2 lines
        CFile file_obj(x_GetIndexFileName());
        file_obj.Remove();
        return x_OpenIndexDB();
    }
}

char*
CNCBlobStorage::x_MapFile(TFileHandle fd, size_t file_size)
{
    char* mem_ptr = NULL;
#ifdef NCBI_OS_LINUX
    file_size = (file_size + kMemPageSize - 1) & kMemPageAlignMask;
    mem_ptr = (char*)mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (mem_ptr == MAP_FAILED) {
        ERR_POST(Critical << "Cannot map file into memory, errno=" << errno);
        return NULL;
    }
    int res = madvise(mem_ptr, file_size, MADV_SEQUENTIAL);
    if (res) {
        ERR_POST(Critical << "madvise failed, errno=" << errno);
    }
#endif
    return mem_ptr;
}

void
CNCBlobStorage::x_UnmapFile(char* mem_ptr, size_t file_size)
{
#ifdef NCBI_OS_LINUX
    file_size = (file_size + kMemPageSize - 1) & kMemPageAlignMask;
    munmap(mem_ptr, file_size);
#endif
}

bool
CNCBlobStorage::x_FlushCurFile(bool force_flush)
{
    int cur_time = int(time(NULL));
    if (!force_flush
        &&  m_TotalDataWritten < m_LastFlushTotal + m_FlushSizePeriod
        &&  cur_time < m_LastFlushTime + m_FlushTimePeriod)
    {
        return true;
    }
#ifdef NCBI_OS_LINUX
    if (msync(m_CurFileMem, m_CurFileOff, MS_SYNC)) {
        ERR_POST(Critical << "Unable to sync storage file, errno=" << errno);
        return false;
    }
#endif
    m_LastFlushTime = cur_time;
    m_LastFlushTotal = m_TotalDataWritten;
    return true;
}

bool
CNCBlobStorage::x_CreateNewFile(void)
{
    if (m_CurFileFD != 0  &&  !x_FlushCurFile(true))
        return false;

    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    Uint4 file_id;
    if (m_CurFileId == kNCMaxDBFileId)
        file_id = 1;
    else
        file_id = m_CurFileId + 1;
    string file_name = x_GetFileName(file_id);
    SNCDBFileInfo* file_info = new SNCDBFileInfo;
    file_info->file_id      = file_id;
    file_info->file_name    = file_name;
    file_info->create_time  = int(time(NULL));
    file_info->file_size    = m_NewFileSize;

    char* mem_ptr = NULL;
#ifdef NCBI_OS_LINUX
    file_info->fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC,
                                            S_IRUSR | S_IWUSR);
    if (file_info->fd == -1) {
        ERR_POST(Critical << "Cannot create new storage file, errno=" << errno);
        delete file_info;
        return false;
    }
    int trunc_res = ftruncate(file_info->fd, m_NewFileSize);
    if (trunc_res != 0) {
        ERR_POST(Critical << "Cannot truncate new file, errno=" << errno);
        close(file_info->fd);
        delete file_info;
        return false;
    }
    mem_ptr = x_MapFile(file_info->fd, m_NewFileSize);
    if (!mem_ptr) {
        close(file_info->fd);
        delete file_info;
        return false;
    }
#endif

    try {
        CFastMutexGuard guard(m_IndexLock);
        m_IndexDB->NewDBFile(file_id, file_name);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Error while adding new storage file: " << ex);
#ifdef NCBI_OS_LINUX
        x_UnmapFile(mem_ptr, m_NewFileSize);
        close(file_info->fd);
#endif
        delete file_info;
        return false;
    }

    m_DBFilesLock.WriteLock();
    m_DBFiles[file_id] = file_info;
    m_DBFilesLock.WriteUnlock();

    m_CurFileLock.WriteLock();
    char* prev_mem_ptr = m_CurFileMem;
    m_CurFileMem = mem_ptr;
    m_CurFileId = file_id;
    m_CurFileFD = file_info->fd;
    m_CurFileOff = 0;
    m_CurFileInfo = file_info;
    m_CurFileLock.WriteUnlock();

    x_UnmapFile(prev_mem_ptr, m_NewFileSize);
    return true;
}

void
CNCBlobStorage::x_DeleteDBFile(TNCDBFilesMap::iterator files_it)
{
    Uint4 file_id = files_it->first;
    SNCDBFileInfo* file_info = files_it->second;
#ifdef NCBI_OS_LINUX
    if (close(file_info->fd)) {
        ERR_POST(Critical << "Error closing file " << file_info->file_name
                          << ", errno=" << errno);
    }
    if (unlink(file_info->file_name.c_str())) {
        ERR_POST(Critical << "Error deleting file " << file_info->file_name
                          << ", errno=" << errno);
    }
#endif
    try {
        CFastMutexGuard guard(m_IndexLock);
        m_IndexDB->DeleteDBFile(file_id);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Index database does not delete rows: " << ex);
    }

    m_DBFilesLock.WriteLock();
    m_DBFiles.erase(files_it);
    m_DBFilesLock.WriteUnlock();

    delete file_info;
}

CNCBlobStorage::CNCBlobStorage(void)
    : m_Stopped(false),
      m_StopTrigger(0, 100),
      m_CurFileId(0),
      m_CurFileFD(0),
      m_FirstRecCoord(0),
      m_FirstRecDead(0),
      m_CntWriteTasks(0),
      m_DataCacheSize(0),
      m_CntDataCacheRecs(0),
      m_CurBlobsCnt(0),
      m_CurDBSize(0),
      m_GarbageSize(0),
      m_TotalDataWritten(0),
      m_CntMoveTasks(0),
      m_CntFailedMoves(0),
      m_MovedSize(0),
      m_LastFlushTotal(0),
      m_LastFlushTime(0),
      m_FreeAccessors(NULL),
      m_UsedAccessors(NULL),
      m_CntUsedHolders(0),
      m_GCBlockWaiter(0, 1),
      m_IsStopWrite(false),
      m_DoExtraGC(false),
      m_ExtraGCTime(0)
{}

CNCBlobStorage::~CNCBlobStorage(void)
{}

bool
CNCBlobStorage::Initialize(bool do_reinit)
{
    if (!x_ReadStorageParams())
        return false;

    if (!x_LockInstanceGuard())
        return false;
    if (!x_OpenIndexDB())
        return false;
    if (do_reinit) {
        if (!x_ReinitializeStorage())
            return false;
        m_CleanStart = false;
    }

    m_BlobCounter.Set(0);
    m_NextRecNum = 1;
    if (!m_DBFiles.empty())
        m_CurFileId = (--m_DBFiles.end())->first;
    if (!x_CreateNewFile())
        return false;
    m_FirstRecCoord = Uint8(m_DBFiles.begin()->first) << 32;
    m_NextWriteCoord = Uint8(m_CurFileId) << 32;

    m_GCAccessor = new CNCBlobAccessor();

    m_BGThread = NewBGThread(this, &CNCBlobStorage::x_DoBackgroundWork);
    try {
        m_BGThread->Run();
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
        m_BGThread.Reset();
        return false;
    }
    return true;
}

void
CNCBlobStorage::Finalize(void)
{
    m_Stopped = true;
    m_StopTrigger.Post();
    if (m_BGThread) {
        try {
            m_BGThread->Join();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
        }
    }

    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        delete it->second;
    }
    m_IndexDB.reset();

    x_UnlockInstanceGuard();
}

void
CNCBlobStorage::PackBlobKey(string*      packed_key,
                            CTempString  cache_name,
                            CTempString  blob_key,
                            CTempString  blob_subkey)
{
    packed_key->clear();
    packed_key->reserve(cache_name.size() + blob_key.size()
                        + blob_subkey.size() + 2);
    packed_key->append(cache_name.data(), cache_name.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_key.data(), blob_key.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_subkey.data(), blob_subkey.size());
}

void
CNCBlobStorage::UnpackBlobKey(const string& packed_key,
                              string&       cache_name,
                              string&       key,
                              string&       subkey)
{
    // cache
    const char* cache_str = packed_key.data();
    size_t cache_size = packed_key.find('\1', 0);
    cache_name.assign(cache_str, cache_size);
    // key
    const char* key_str = cache_str + cache_size + 1;
    size_t key_size = packed_key.find('\1', cache_size + 1)
                      - cache_size - 1;
    key.assign(key_str, key_size);
    // subkey
    const char* subkey_str = key_str + key_size + 1;
    size_t subkey_size = packed_key.size() - cache_size - key_size - 2;
    subkey.assign(subkey_str, subkey_size);
}

string
CNCBlobStorage::UnpackKeyForLogs(const string& packed_key)
{
    // cache
    const char* cache_name = packed_key.data();
    size_t cache_size = packed_key.find('\1', 0);
    // key
    const char* key = cache_name + cache_size + 1;
    size_t key_size = packed_key.find('\1', cache_size + 1) - cache_size - 1;
    // subkey
    const char* subkey = key + key_size + 1;
    size_t subkey_size = packed_key.size() - cache_size - key_size - 2;

    string result;
    result.append("'");
    result.append(key, key_size);
    result.append("'");
    if (cache_size != 0  ||  subkey_size != 0) {
        result.append(", '");
        result.append(subkey, subkey_size);
        result.append("' from cache '");
        result.append(cache_name, cache_size);
        result.append("'");
    }
    return result;
}

string
CNCBlobStorage::PrintablePassword(const string& pass)
{
    static const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    string result(32, '\0');
    for (int i = 0, j = 0; i < 16; ++i) {
        Uint1 c = Uint1(pass[i]);
        result[j++] = digits[c >> 4];
        result[j++] = digits[c & 0xF];
    }
    return result;
}

CNCBlobAccessor*
CNCBlobStorage::x_GetAccessor(bool* need_initialize)
{
    CSpinGuard guard(m_HoldersPoolLock);

    CNCBlobAccessor* holder = m_FreeAccessors;
    if (holder) {
        holder->RemoveFromList(m_FreeAccessors);
    }
    else {
        holder = new CNCBlobAccessor();
    }
    ++m_CntUsedHolders;

    *need_initialize = true;
    return holder;
}

void
CNCBlobStorage::ReturnAccessor(CNCBlobAccessor* holder)
{
    CSpinGuard guard(m_HoldersPoolLock);

    holder->AddToList(m_FreeAccessors);
    --m_CntUsedHolders;
}

CNCBlobStorage::SSlotCache*
CNCBlobStorage::x_GetSlotCache(Uint2 slot)
{
    m_BigCacheLock.ReadLock();
    TSlotCacheMap::const_iterator it = m_SlotsCache.find(slot);
    if (it != m_SlotsCache.end()) {
        SSlotCache* cache = it->second;
        m_BigCacheLock.ReadUnlock();
        return cache;
    }
    m_BigCacheLock.ReadUnlock();
    m_BigCacheLock.WriteLock();
    SSlotCache* cache = m_SlotsCache[slot];
    if (cache == NULL) {
        cache = new SSlotCache(slot);
        m_SlotsCache[slot] = cache;
    }
    m_BigCacheLock.WriteUnlock();
    return cache;
}

SNCCacheData*
CNCBlobStorage::x_GetKeyCacheData(Uint2 slot, const string& key, bool need_create)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    SNCCacheData* data = NULL;
    if (need_create) {
        cache->lock.WriteLock();
        TKeyMap::insert_commit_data commit_data;
        pair<TKeyMap::iterator, bool> ins_res
            = cache->key_map.insert_unique_check(key, SCacheKeyCompare(), commit_data);
        if (ins_res.second) {
            data = new SNCCacheData();
            data->key = key;
            data->slot = slot;
            cache->key_map.insert_unique_commit(*data, commit_data);
        }
        else {
            data = &*ins_res.first;
            data->key_deleted = false;
        }
        cache->lock.WriteUnlock();
    }
    else {
        cache->lock.ReadLock();
        TKeyMap::iterator it = cache->key_map.find(key, SCacheKeyCompare());
        if (it == cache->key_map.end()) {
            data = NULL;
        }
        else {
            data = &*it;
            if (data->key_deleted)
                data = NULL;
        }
        cache->lock.ReadUnlock();
    }
    return data;
}

void
CNCBlobStorage::x_InitializeAccessor(CNCBlobAccessor* acessor)
{
    const string& key = acessor->GetBlobKey();
    Uint2 slot = acessor->GetBlobSlot();
    bool need_create = acessor->GetAccessType() == eNCCreate
                       ||  acessor->GetAccessType() == eNCCopyCreate;
    SNCCacheData* data = x_GetKeyCacheData(slot, key, need_create);
    acessor->Initialize(data);
}

CNCBlobAccessor*
CNCBlobStorage::GetBlobAccess(ENCAccessType access,
                              const string& key,
                              const string& password,
                              Uint2         slot)
{
    bool need_initialize;
    CNCBlobAccessor* accessor(x_GetAccessor(&need_initialize));
    accessor->Prepare(key, password, slot, access);
    if (need_initialize)
        x_InitializeAccessor(accessor);
    else {
        CSpinGuard guard(m_HoldersPoolLock);
        accessor->AddToList(m_UsedAccessors);
    }
    return accessor;
}

void
CNCBlobStorage::DeleteBlobKey(Uint2 slot, const string& key)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    cache->lock.WriteLock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (data->coord != 0)
        abort();
    data->key_deleted = true;
    data->key_del_time = int(time(NULL));
    cache->lock.WriteUnlock();
    cache->deleter.AddElement(key);
}

void
CNCBlobStorage::RestoreBlobKey(Uint2 slot,
                               const string& key,
                               SNCCacheData* cache_data)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    cache->lock.WriteLock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (data != cache_data)
        abort();
    data->key_deleted = false;
    cache->lock.WriteUnlock();
}

bool
CNCBlobStorage::IsBlobExists(Uint2 slot, const string& key)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    CFastReadGuard guard(cache->lock);
    TKeyMap::const_iterator it = cache->key_map.find(key, SCacheKeyCompare());
    return it != cache->key_map.end()  &&  it->coord != 0
           &&  it->expire > int(time(NULL));
}

void
CNCBlobStorage::x_RemoveDataFromLRU(SCacheDataRec* cache_data)
{
    if (((TLRUHook*)cache_data)->is_linked()) {
        m_DataLRU.erase(m_DataLRU.iterator_to(*cache_data));
    }
    else {
        ++cache_data->lock_lru_refs;
    }
}

void
CNCBlobStorage::x_PutDataToLRU(SCacheDataRec* cache_data)
{
    m_DataCacheLock.Lock();
    if (cache_data->lock_lru_refs == 0) {
        m_DataLRU.push_back(*cache_data);
    }
    else {
        --cache_data->lock_lru_refs;
    }
    m_DataCacheLock.Unlock();
}

SCacheDataRec*
CNCBlobStorage::x_GetCacheData(Uint8 coord)
{
    CSpinGuard guard(m_DataCacheLock);
    TDataRecMap::iterator it = m_DataCache.find(coord, SCacheRecCompare());
    if (it == m_DataCache.end()) {
        return NULL;
    }
    else {
        SCacheDataRec* cache_data = &*it;
        x_RemoveDataFromLRU(cache_data);
        return cache_data;
    }
}

SCacheDataRec*
CNCBlobStorage::x_ReadCacheDataImpl(Uint8 coord)
{
    void* cache_mem;
    Uint4 file_id = Uint4(coord >> 32);
    Uint4 offset = Uint4(coord);
    m_CurFileLock.ReadLock();
    if (m_CurFileId == file_id) {
        if (offset > m_NewFileSize - sizeof(SFileRecHeader)) {
            abort();
            m_CurFileLock.ReadUnlock();
            ERR_POST(Critical << "Bad coordinates met: " << coord);
            return NULL;
        }
        SFileRecHeader* header = (SFileRecHeader*)(m_CurFileMem + offset);
        Uint2 rec_size = header->rec_size;
        if (offset > m_NewFileSize - rec_size) {
            abort();
            m_CurFileLock.ReadUnlock();
            ERR_POST(Critical << "Bad coordinates met: " << coord);
            return NULL;
        }
        cache_mem = malloc(rec_size);
        memcpy(cache_mem, header, rec_size);
        m_CurFileLock.ReadUnlock();
    }
    else {
        m_CurFileLock.ReadUnlock();
        m_DBFilesLock.ReadLock();
        TNCDBFilesMap::iterator it = m_DBFiles.find(file_id);
        if (it == m_DBFiles.end()) {
            abort();
            m_DBFilesLock.ReadUnlock();
            ERR_POST(Critical << "Can't find file id " << file_id
                              << " from coordinate " << coord);
            return NULL;
        }
        TFileHandle fd = it->second->fd;
        m_DBFilesLock.ReadUnlock();

#ifdef NCBI_OS_LINUX
        SFileRecHeader header;
retry_header:
        ssize_t n_read = pread(fd, &header, sizeof(header), offset);
        int x_errno = errno;
process_read_error:
        if (n_read == -1) {
            if (x_errno == EINTR)
                goto retry_header;
            abort();
            ERR_POST(Critical << "Error reading file " << file_id
                              << ", errno=" << x_errno);
            return NULL;
        }
        else if (size_t(n_read) != sizeof(header)) {
            abort();
            ERR_POST(Critical << "Can't read record from file " << file_id
                              << ", offset " << offset
                              << ", read " << n_read << " bytes");
            return NULL;
        }

        cache_mem = malloc(header.rec_size);
retry_record:
        n_read = pread(fd, cache_mem, header.rec_size, offset);
        x_errno = errno;
        if (n_read == -1  &&  x_errno == EINTR)
            goto retry_record;
        else if (size_t(n_read) != header.rec_size) {
            free(cache_mem);
            goto process_read_error;
        }
#endif
    }

    SCacheDataRec* cache_data = new SCacheDataRec();
    cache_data->coord = coord;
    cache_data->data = (SFileRecHeader*)cache_mem;
    m_DataCacheLock.Lock();
    TDataRecMap::insert_commit_data commit_data;
    pair<TDataRecMap::iterator, bool> ins_res =
        m_DataCache.insert_unique_check(coord, SCacheRecCompare(), commit_data);
    if (ins_res.second) {
        m_DataCache.insert_unique_commit(*cache_data, commit_data);
        m_DataCacheSize += sizeof(*cache_data) + cache_data->data->rec_size;
        ++m_CntDataCacheRecs;
        m_DataCacheLock.Unlock();
    }
    else {
        SCacheDataRec* new_data = &*ins_res.first;
        x_RemoveDataFromLRU(new_data);
        m_DataCacheLock.Unlock();

        free(cache_mem);
        delete cache_data;
        cache_data = new_data;
    }

    return cache_data;
}

SCacheDataRec*
CNCBlobStorage::x_ReadCacheData(Uint8 coord, EFileRecType rec_type)
{
    SCacheDataRec* cache_data = x_GetCacheData(coord);
    if (!cache_data) {
        cache_data = x_ReadCacheDataImpl(coord);
        if (!cache_data)
            return NULL;
    }
    if (cache_data->data->rec_type == rec_type  ||  rec_type == eFileRecAny)
        return cache_data;

    abort();
    ERR_POST(Critical << "Incorrect record type found - "
                      << cache_data->data->rec_type << " (need "
                      << rec_type << ")");
    x_PutDataToLRU(cache_data);
    return NULL;
}

void
CNCBlobStorage::x_DeleteCacheData(SCacheDataRec* cache_data)
{
    free(cache_data->data);
    delete cache_data;
}

void
CNCBlobStorage::x_AddWriteRecordTask(SCacheDataRec* cache_data,
                                     EWriteTask task_type)
{
    SWriteTask* task = new SWriteTask;
    task->task_type = task_type;
    task->cache_data = cache_data;

    m_WriteQueueLock.Lock();
    Uint4 offset = Uint4(m_NextWriteCoord);
    if (offset > m_NewFileSize - cache_data->data->rec_size) {
        Uint4 file_id = Uint4(m_NextWriteCoord >> 32);
        if (file_id == kNCMaxDBFileId)
            file_id = 1;
        else
            ++file_id;
        m_NextWriteCoord = Uint8(file_id) << 32;
    }
    task->coord = m_NextWriteCoord;
    m_NextWriteCoord += cache_data->data->rec_size;
    cache_data->data->rec_num = m_NextRecNum++;
    m_WriteQueue.push_back(*task);
    ++m_CntWriteTasks;
    m_WriteQueueLock.Unlock();

    m_DataCacheLock.Lock();
    // Nobody else will be able to insert the same coordinate, so I'm using
    // insert_equal.
    cache_data->coord = task->coord;
    m_DataCache.insert_equal(*cache_data);
    m_DataCacheSize += sizeof(*cache_data) + cache_data->data->rec_size;
    ++m_CntDataCacheRecs;
    m_DataCacheLock.Unlock();
}

void
CNCBlobStorage::x_AddWriteTask(EWriteTask task_type, Uint8 coord)
{
    SWriteTask* task = new SWriteTask;
    task->task_type = task_type;
    task->coord = coord;

    m_WriteQueueLock.Lock();
    m_WriteQueue.push_back(*task);
    ++m_CntWriteTasks;
    m_WriteQueueLock.Unlock();
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(ver_data->coord != 0);
    SCacheDataRec* cache_data = x_ReadCacheData(ver_data->coord, eFileRecMeta);
    if (!cache_data) {
        abort();
        ERR_POST(Critical << "Cannot read blob meta info");
        return false;
    }

    SFileMetaRec* meta_rec = (SFileMetaRec*)cache_data->data;
    ver_data->create_time = meta_rec->create_time;
    ver_data->ttl = meta_rec->ttl;
    ver_data->expire = meta_rec->expire;
    ver_data->dead_time = meta_rec->dead_time;
    ver_data->size = meta_rec->size;
    if (meta_rec->has_password)
        ver_data->password.assign(meta_rec->key_data, 16);
    else
        ver_data->password.clear();
    ver_data->blob_ver = meta_rec->blob_ver;
    ver_data->ver_ttl = meta_rec->ver_ttl;
    ver_data->ver_expire = meta_rec->ver_expire;
    ver_data->create_id = meta_rec->create_id;
    ver_data->create_server = meta_rec->create_server;
    ver_data->slot = meta_rec->slot;
    ver_data->first_map_coord = meta_rec->map_coord;
    ver_data->first_chunk_coord = meta_rec->data_coord;

    x_PutDataToLRU(cache_data);
    return true;
}

bool
CNCBlobStorage::x_SaveChunkMap(SNCBlobVerData* ver_data,
                               SNCChunkMapInfo* chunk_map)
{
    SCacheDataRec* cache_data = new SCacheDataRec();
    SFileChunkMapRec* map_rec = (SFileChunkMapRec*)cache_data->data;
    size_t coords_size = chunk_map->cur_chunk_idx * sizeof(chunk_map->chunks[0]);
    size_t rec_size = (char*)map_rec->chunk_coords - (char*)map_rec + coords_size;
    map_rec = (SFileChunkMapRec*)malloc(rec_size);
    map_rec->rec_type = eFileRecChunkMap;
    map_rec->rec_size = Uint2(rec_size);
    map_rec->map_num = chunk_map->map_num;
    map_rec->prev_coord = ver_data->last_map_coord;
    map_rec->next_coord = 0;
    map_rec->meta_coord = 0;
    memcpy(map_rec->chunk_coords, chunk_map->chunks, coords_size);

    ver_data->disk_size += map_rec->rec_size;
    cache_data->data = map_rec;
    x_AddWriteRecordTask(cache_data, eWriteRecord);

    Uint8 prev_map_coord = ver_data->last_map_coord;
    ver_data->last_map_coord = cache_data->coord;
    if (chunk_map->map_num == 0) {
        ver_data->first_map_coord = cache_data->coord;
    }
    else {
        SCacheDataRec* prev_cache = x_ReadCacheData(prev_map_coord, eFileRecChunkMap);
        if (!prev_cache) {
            ERR_POST(Critical << "Cannot read previous map");
            return false;
        }
        SFileChunkMapRec* prev_rec = (SFileChunkMapRec*)prev_cache->data;
        prev_rec->next_coord = ver_data->last_map_coord;
        x_PutDataToLRU(prev_cache);
    }

    return true;
}

bool
CNCBlobStorage::WriteBlobInfo(const string& blob_key,
                              SNCBlobVerData* ver_data,
                              SNCChunkMapInfo* chunk_map)
{
    Uint8 old_coord = ver_data->coord;
    if (old_coord == 0  &&  ver_data->size > kNCMaxBlobChunkSize) {
        if (!x_SaveChunkMap(ver_data, chunk_map))
            return false;
    }

    SCacheDataRec* cache_data = new SCacheDataRec();
    SFileMetaRec* meta_rec = (SFileMetaRec*)cache_data->data;
    Uint2 key_size = Uint2(blob_key.size());
    if (!ver_data->password.empty())
        key_size += 16;
    size_t rec_size = (char*)meta_rec->key_data - (char*)meta_rec + key_size;
    meta_rec = (SFileMetaRec*)malloc(rec_size);
    meta_rec->rec_type = eFileRecMeta;
    meta_rec->rec_size = Uint2(rec_size);
    meta_rec->deleted = 0;
    meta_rec->slot = ver_data->slot;
    meta_rec->key_size = Uint2(blob_key.size());
    meta_rec->map_coord = ver_data->first_map_coord;
    meta_rec->data_coord = ver_data->first_chunk_coord;
    meta_rec->size = ver_data->size;
    meta_rec->create_time = ver_data->create_time;
    meta_rec->create_server = ver_data->create_server;
    meta_rec->create_id = ver_data->create_id;
    meta_rec->dead_time = ver_data->dead_time;
    meta_rec->ttl = ver_data->ttl;
    meta_rec->expire = ver_data->expire;
    meta_rec->blob_ver = ver_data->blob_ver;
    meta_rec->ver_ttl = ver_data->ver_ttl;
    meta_rec->ver_expire = ver_data->ver_expire;
    char* key_data = meta_rec->key_data;
    if (ver_data->password.empty()) {
        meta_rec->has_password = 0;
    }
    else {
        meta_rec->has_password = 1;
        memcpy(key_data, ver_data->password.data(), 16);
        key_data += 16;
    }
    memcpy(key_data, blob_key.data(), blob_key.size());

    ver_data->disk_size += meta_rec->rec_size;
    cache_data->data = meta_rec;
    x_AddWriteRecordTask(cache_data, eWriteRecord);
    ver_data->coord = cache_data->coord;

    if (old_coord != 0)
        x_AddWriteTask(eDelMetaInfo, old_coord);

    return true;
}

inline void
CNCBlobStorage::x_AddGarbageSize(Int8 size)
{
    m_GarbageLock.Lock();
    m_GarbageSize += Uint8(size);
    m_GarbageLock.Unlock();
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data)
{
    if (ver_data->coord != 0) {
        x_AddWriteTask(eDelBlob, ver_data->coord);
    }
    else {
        m_GarbageLock.Lock();
        m_GarbageSize += ver_data->disk_size;
        m_UnfinishedBlobs.erase(ver_data->first_chunk_coord);
        m_GarbageLock.Unlock();
    }
}

SCacheDataRec*
CNCBlobStorage::x_FindChunkMap(Uint8 first_map_coord, Uint4 map_num)
{
    Uint8 next_coord = first_map_coord;
    for (Uint4 cur_map = 0; cur_map <= map_num; ++cur_map) {
        SCacheDataRec* map_cache = x_ReadCacheData(next_coord, eFileRecChunkMap);
        if (!map_cache) {
            abort();
            ERR_POST(Critical << "Cannot read chunk map");
            return NULL;
        }

        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)map_cache->data;
        if (map_rec->map_num != cur_map) {
            abort();
            ERR_POST(Critical << "Corrupted data in the storage");
            x_PutDataToLRU(map_cache);
            return NULL;
        }

        if (cur_map == map_num)
            return map_cache;
        next_coord = map_rec->next_coord;
        x_PutDataToLRU(map_cache);
    }
    abort();
    return NULL;
}

bool
CNCBlobStorage::ReadChunkData(SNCBlobVerData* ver_data,
                              SNCChunkMapInfo* chunk_map,
                              Uint8 chunk_num,
                              CNCBlobBuffer* buffer)
{
    Uint4 map_num = Uint4(chunk_num / kNCMaxChunksInMap);
    if (ver_data->size > kNCMaxBlobChunkSize  &&  chunk_map->map_num != map_num)
    {
        SCacheDataRec* map_cache;
        if (map_num  == 0)
            map_cache = x_ReadCacheData(ver_data->first_map_coord, eFileRecChunkMap);
        else if (chunk_map->map_num == map_num - 1)
            map_cache = x_ReadCacheData(chunk_map->next_coord, eFileRecChunkMap);
        else
            map_cache = x_FindChunkMap(ver_data->first_map_coord, map_num);

        if (!map_cache) {
            abort();
            ERR_POST(Critical << "Cannot locate blob's chunk map");
            return false;
        }

        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)map_cache->data;
        if (map_rec->map_num != map_num) {
            abort();
            ERR_POST(Critical << "Corrupted data in the storage");
            x_PutDataToLRU(map_cache);
            return false;
        }
        size_t map_size = (char*)map_rec + map_rec->rec_size
                            - (char*)map_rec->chunk_coords;

        memcpy(chunk_map->chunks, map_rec->chunk_coords, map_size);
        chunk_map->map_num = map_num;
        chunk_map->next_coord = map_rec->next_coord;

        x_PutDataToLRU(map_cache);
    }
    Uint8 chunk_coord;
    if (ver_data->size <= kNCMaxBlobChunkSize) {
        if (chunk_num != 0)
            abort();
        chunk_coord = ver_data->first_chunk_coord;
    }
    else
        chunk_coord = chunk_map->chunks[chunk_num % kNCMaxChunksInMap];

    SCacheDataRec* cache_data = x_ReadCacheData(chunk_coord, eFileRecChunkData);
    if (!cache_data) {
        abort();
        ERR_POST(Critical << "Cannot read chunk data");
        return false;
    }

    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)cache_data->data;
    if (data_rec->chunk_num != chunk_num) {
        abort();
        ERR_POST(Critical << "Corrupted data in the storage");
        x_PutDataToLRU(cache_data);
        return false;
    }
    size_t data_size = (char*)data_rec + data_rec->rec_size
                        - (char*)data_rec->chunk_data;
    memcpy(buffer->GetData(), data_rec->chunk_data, data_size);
    buffer->Resize(data_size);
    CNCStat::AddChunkRead(data_size);

    x_PutDataToLRU(cache_data);
    return true;
}

bool
CNCBlobStorage::WriteChunkData(SNCBlobVerData* ver_data,
                               SNCChunkMapInfo* chunk_map,
                               Uint8 chunk_num,
                               const CNCBlobBuffer* data)
{
    Uint8 prev_chunk_coord;
    if (chunk_num == 0)
        prev_chunk_coord = 0;
    else
        prev_chunk_coord = chunk_map->chunks[chunk_map->cur_chunk_idx - 1];

    Uint4 map_num = Uint4(chunk_num / kNCMaxChunksInMap);
    if (map_num != chunk_map->map_num) {
        if (!x_SaveChunkMap(ver_data, chunk_map))
            return false;
        chunk_map->map_num = map_num;
        chunk_map->cur_chunk_idx = 0;
    }

    SCacheDataRec* cache_data = new SCacheDataRec();
    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)cache_data->data;
    size_t rec_size = (char*)data_rec->chunk_data - (char*)data_rec
                      + data->GetSize();
    data_rec = (SFileChunkDataRec*)malloc(rec_size);
    data_rec->rec_type = eFileRecChunkData;
    data_rec->rec_size = Uint2(rec_size);
    data_rec->prev_coord = prev_chunk_coord;
    data_rec->next_coord = 0;
    data_rec->map_coord = 0;
    data_rec->chunk_num = chunk_num;
    memcpy(data_rec->chunk_data, data->GetData(), data->GetSize());

    ver_data->disk_size += data_rec->rec_size;
    cache_data->data = data_rec;
    x_AddWriteRecordTask(cache_data, eWriteRecord);
    chunk_map->chunks[chunk_map->cur_chunk_idx++] = cache_data->coord;

    if (chunk_num == 0)
        ver_data->first_chunk_coord = cache_data->coord;
    CNCStat::AddChunkWritten(data->GetSize());

    return true;
}

void
CNCBlobStorage::ChangeCacheDeadTime(SNCCacheData* cache_data,
                                    Uint8 new_coord,
                                    int new_dead_time)
{
    m_TimeTableLock.Lock();
    if (cache_data->dead_time != 0) {
        m_TimeTable.erase(*cache_data);
        --m_CurBlobsCnt;
    }
    cache_data->coord = new_coord;
    cache_data->dead_time = new_dead_time;
    if (new_dead_time != 0) {
        cache_data->dead_time = new_dead_time;
        m_TimeTable.insert_equal(*cache_data);
        ++m_CurBlobsCnt;
    }
    m_TimeTableLock.Unlock();
}

Uint8
CNCBlobStorage::GetMaxSyncLogRecNo(void)
{
    try {
        CFastMutexGuard guard(m_IndexLock);
        return m_IndexDB->GetMaxSyncLogRecNo();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Cannot read max_sync_log_rec_no: " << ex);
        return 0;
    }
}

void
CNCBlobStorage::SaveMaxSyncLogRecNo(void)
{
    m_NeedSaveLogRecNo = true;
}


struct SNCTempBlobInfo
{
    string  key;
    Uint8   create_time;
    Uint8   create_server;
    Uint8   coord;
    Uint4   create_id;
    int     dead_time;
    int     expire;
    int     ver_expire;

    SNCTempBlobInfo(const SNCCacheData& cache_info)
        : key(cache_info.key),
          create_time(cache_info.create_time),
          create_server(cache_info.create_server),
          coord(cache_info.coord),
          create_id(cache_info.create_id),
          dead_time(cache_info.dead_time),
          expire(cache_info.expire),
          ver_expire(cache_info.ver_expire)
    {
    }
};

void
CNCBlobStorage::GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst)
{
    blobs_lst.clear();
    SSlotCache* cache = x_GetSlotCache(slot);

    cache->lock.WriteLock();

    Uint8 cnt_blobs = cache->key_map.size();
    void* big_block = malloc(size_t(cnt_blobs * sizeof(SNCTempBlobInfo)));
    SNCTempBlobInfo* info_ptr = (SNCTempBlobInfo*)big_block;

    ITERATE(TKeyMap, it, cache->key_map) {
        new (info_ptr) SNCTempBlobInfo(*it);
        ++info_ptr;
    }

    cache->lock.WriteUnlock();

    info_ptr = (SNCTempBlobInfo*)big_block;
    for (Uint8 i = 0; i < cnt_blobs; ++i) {
        if (info_ptr->coord != 0) {
            SNCCacheData* blob_sum = new SNCCacheData();
            blob_sum->create_id = info_ptr->create_id;
            blob_sum->create_server = info_ptr->create_server;
            blob_sum->create_time = info_ptr->create_time;
            blob_sum->dead_time = info_ptr->dead_time;
            blob_sum->expire = info_ptr->expire;
            blob_sum->ver_expire = info_ptr->ver_expire;
            blobs_lst[info_ptr->key] = blob_sum;
        }
        info_ptr->~SNCTempBlobInfo();
        ++info_ptr;
    }

    free(big_block);
}

void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    m_DBFilesLock.ReadLock();
    Uint4 num_files = Uint4(m_DBFiles.size());
    m_DBFilesLock.ReadUnlock();
    m_TimeTableLock.Lock();
    Uint8 cnt_blobs = m_CurBlobsCnt;
    int oldest_dead = (m_TimeTable.empty()? 0: m_TimeTable.begin()->dead_time);
    int first_dead = m_FirstRecDead;
    m_TimeTableLock.Unlock();
    m_GarbageLock.Lock();
    Uint8 garbage = m_GarbageSize;
    Uint8 unfinished = m_UnfinishedBlobs.size();
    m_GarbageLock.Unlock();
    m_DataCacheLock.Lock();
    size_t cache_size = m_DataCacheSize;
    size_t cache_cnt = m_CntDataCacheRecs;
    m_DataCacheLock.Unlock();
    Uint8 db_size = m_CurDBSize;
    Uint4 write_tasks = m_CntWriteTasks;

    proxy << "Now on disk - "
                    << num_files << " files of "
                    << g_ToSizeStr(db_size) << " (garbage - "
                    << g_ToSizeStr(garbage) << ", "
                    << (db_size == 0? 0: garbage * 100 / db_size) << "%)" << endl
          << "Now in memory - "
                    << g_ToSmartStr(cnt_blobs) << " blobs ("
                    << unfinished << " unfinished), write queue "
                    << write_tasks << endl
          << "Now in cache - "
                    << g_ToSizeStr(cache_size) << ", "
                    << g_ToSmartStr(cache_cnt) << " records" << endl
          << "Total moves - "
                    << g_ToSmartStr(m_CntMoveTasks) << " tasks ("
                    << m_CntFailedMoves << " failed), "
                    << g_ToSizeStr(m_MovedSize) << " in size" << endl;
    if (oldest_dead != 0) {
        proxy << "Dead times - ";
        CTime tmp(CTime::eEmpty, CTime::eLocal);
        tmp.SetTimeT(oldest_dead);
        proxy << tmp.AsString() << " (oldest), ";
        if (first_dead == 0)
            proxy << "unknown";
        else if (first_dead == kDeadTimeDead)
            proxy << "expired";
        else if (first_dead == kDeadTimeUnfinished)
            proxy << "unfinished";
        else {
            tmp.SetTimeT(first_dead);
            proxy << tmp.AsString();
        }
        proxy << " (first rec)" << endl;
    }
}

void
CNCBlobStorage::x_CollectStorageStats(void)
{
    m_DBFilesLock.ReadLock();
    Uint4 num_files = Uint4(m_DBFiles.size());
    m_DBFilesLock.ReadUnlock();
    m_GarbageLock.Lock();
    Uint8 garbage = m_GarbageSize;
    Uint8 unfinished = m_UnfinishedBlobs.size();
    m_GarbageLock.Unlock();
    m_DataCacheLock.Lock();
    size_t cache_size = m_DataCacheSize;
    size_t cache_cnt = m_CntDataCacheRecs;
    m_DataCacheLock.Unlock();
    Uint8 db_size = m_CurDBSize;
    Uint8 cnt_blobs = m_CurBlobsCnt;
    Uint4 write_tasks = m_CntWriteTasks;

    CNCStat::AddDBMeasurement(num_files, db_size, garbage,
                              cache_size, cache_cnt, cnt_blobs,
                              unfinished, write_tasks);
}

void
CNCBlobStorage::OnBlockedOpFinish(void)
{
    m_GCBlockWaiter.Post();
}

Uint8
CNCBlobStorage::x_CalcBlobDiskSize(Uint2 key_size, Uint8 blob_size)
{
    SFileMetaRec meta_rec;
    SFileChunkMapRec map_rec;
    SFileChunkDataRec data_rec;
    Uint8 meta_disk_size = meta_rec.key_data - (char*)&meta_rec + key_size;
    Uint8 cnt_chunks = (blob_size + kNCMaxBlobChunkSize - 1) / kNCMaxBlobChunkSize;
    Uint8 data_disk_size =
                // Blob data size
                blob_size
                // Size of meta information in chunks
                + cnt_chunks * ((char*)data_rec.chunk_data - (char*)&data_rec);
    Uint8 cnt_maps = (cnt_chunks + kNCMaxChunksInMap - 1) / kNCMaxChunksInMap;
    Uint8 map_disk_size =
                // Size of chunk pointers in chunk maps
                cnt_chunks * sizeof(map_rec.chunk_coords[0])
                // Size of meta information in chunk maps
                + cnt_maps * ((char*)map_rec.chunk_coords - (char*)&map_rec);

    if (blob_size == 0)
        return meta_disk_size;
    else if (cnt_chunks == 1)
        return meta_disk_size + data_disk_size;
    else
        return meta_disk_size + map_disk_size + data_disk_size;
}

void
CNCBlobStorage::x_CacheMetaRec(SFileRecHeader* header,
                               Uint4 file_id,
                               size_t offset,
                               int cur_time)
{
    SFileMetaRec* meta_rec = (SFileMetaRec*)header;
    Uint8 coord = (Uint8(file_id) << 32) + Uint4(offset);
    char* key_data = meta_rec->key_data;
    Uint2 key_size = meta_rec->key_size;
    if (meta_rec->has_password) {
        key_data += 16;
        key_size -= 16;
    }
    string key(key_data, key_size);

    if (meta_rec->deleted) {
        m_GarbageSize += meta_rec->rec_size;
        return;
    }
    if (meta_rec->dead_time <= cur_time) {
        x_AddWriteTask(eDelMetaInfo, coord);
        // m_GarbageSize is increased in x_DoDelBlob
        return;
    }

    m_GarbageSize -= x_CalcBlobDiskSize(meta_rec->key_size, meta_rec->size)
                     - meta_rec->rec_size;

    SNCCacheData* cache_data = new SNCCacheData();
    cache_data->key = key;
    cache_data->coord = coord;
    cache_data->slot = meta_rec->slot;
    cache_data->size = meta_rec->size;
    cache_data->create_time = meta_rec->create_time;
    cache_data->create_server = meta_rec->create_server;
    cache_data->create_id = meta_rec->create_id;
    cache_data->dead_time = meta_rec->dead_time;
    cache_data->expire = meta_rec->expire;
    cache_data->ver_expire = meta_rec->ver_expire;

    TSlotCacheMap::iterator it_slot = m_SlotsCache.lower_bound(meta_rec->slot);
    SSlotCache* slot_cache;
    if (it_slot == m_SlotsCache.end()
        ||  it_slot->first != meta_rec->slot)
    {
        slot_cache = new SSlotCache(meta_rec->slot);
        m_SlotsCache.insert(it_slot, TSlotCacheMap::value_type(
                                                  meta_rec->slot, slot_cache));
    }
    else {
        slot_cache = it_slot->second;
    }
    TKeyMap::insert_commit_data commit_data;
    pair<TKeyMap::iterator, bool> ins_res =
        slot_cache->key_map.insert_unique_check(key, SCacheKeyCompare(), commit_data);
    if (ins_res.second) {
        slot_cache->key_map.insert_unique_commit(*cache_data, commit_data);
    }
    else {
        SNCCacheData* old_data = &*ins_res.first;
        slot_cache->key_map.erase(ins_res.first);
        slot_cache->key_map.insert_equal(*cache_data);
        --m_CurBlobsCnt;
        m_TimeTable.erase(*old_data);
        SCacheDataRec* old_cache = x_ReadCacheData(old_data->coord, eFileRecMeta);
        if (!old_cache)
            abort();
        SFileMetaRec* old_meta = (SFileMetaRec*)old_cache->data;
        if (old_meta->map_coord == meta_rec->map_coord
            &&  old_meta->data_coord == meta_rec->data_coord)
        {
            x_AddWriteTask(eDelMetaInfo, old_data->coord);
        }
        else {
            x_AddWriteTask(eDelBlob, old_data->coord);
        }
        x_PutDataToLRU(old_cache);
        delete old_data;
    }
    m_TimeTable.insert_equal(*cache_data);
    ++m_CurBlobsCnt;
}

void
CNCBlobStorage::x_CacheDatabase(void)
{
    CRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    CDiagContext::SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart().Print("_type", "caching");
    }

    int cur_time = int(time(NULL));
    ERASE_ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        if (m_Stopped)
            break;

        SNCDBFileInfo* file_info = it_file->second;
        Int8 file_size = CFile(file_info->file_name).GetLength();
        if (file_size == -1) {
            ERR_POST(Critical << "Cannot read file size (errno=" << errno
                              << "). File " << file_info->file_name
                              << " will be deleted");
file_error:
            x_DeleteDBFile(it_file);
            continue;
        }
        char* file_mem = x_MapFile(file_info->fd, size_t(file_size));
        if (!file_mem) {
            ERR_POST(Critical << "File " << file_info->file_name
                              << " will be deleted");
            goto file_error;
        }
#ifdef NCBI_OS_LINUX
        int res = madvise(file_mem, file_size, MADV_WILLNEED);
        if (res) {
            ERR_POST(Critical << "madvise failed, errno=" << errno);
        }
#endif

        char* data = file_mem;
        char* end_data = data + size_t(file_size);
        while (data + sizeof(SFileRecHeader) < end_data) {
            if (m_Stopped)
                break;

            SFileRecHeader* header = (SFileRecHeader*)data;
            if (header->rec_num == 0  &&  header->rec_type == eFileRecNone) {
                // File finished, some zeros left in the end.
                break;
            }
            if (header->rec_num < m_NextRecNum
                ||  end_data - data < header->rec_size)
            {
header_error:
                ERR_POST(Critical << "Incorrect data met in "
                                  << file_info->file_name << ", offset "
                                  << (data - file_mem)
                                  << ". Cannot parse file further.");
                break;
            }
            switch (header->rec_type) {
            case eFileRecMeta:
                x_CacheMetaRec(header, file_info->file_id, data - file_mem, cur_time);
                break;
            case eFileRecChunkData:
            case eFileRecChunkMap:
                m_GarbageSize += header->rec_size;
                break;
            default:
                goto header_error;
            }
            data += header->rec_size;
            m_NextRecNum = header->rec_num + 1;
        }
        file_info->file_size = Uint4(data - file_mem);
        m_CurDBSize += file_info->file_size;
        LOG_POST("Cached file size " << file_info->file_size << ", total " << m_CurDBSize);
        x_UnmapFile(file_mem, size_t(file_size));
    }

    CNetCacheServer::CachingCompleted();
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStop();
    }
    CDiagContext::SetRequestContext(NULL);
}

void
CNCBlobStorage::x_HeartBeat(void)
{
    m_BigCacheLock.ReadLock();
    ITERATE(TSlotCacheMap, it, m_SlotsCache) {
        SSlotCache* cache = it->second;
        cache->deleter.HeartBeat();
    }
    m_BigCacheLock.ReadUnlock();

    if (!m_IsStopWrite) {
        if (m_StopWriteOnSize != 0  &&  m_CurDBSize >= m_StopWriteOnSize) {
            m_IsStopWrite = true;
            ERR_POST(Critical << "Database size exceeded limit. "
                                 "Will no longer accept any writes.");
        }
    }
    else if (m_CurDBSize <= m_StopWriteOffSize) {
        m_IsStopWrite = false;
    }
    try {
        if (CFileUtil::GetFreeDiskSpace(m_Path) <= m_DiskFreeLimit) {
            m_IsStopWrite = true;
            ERR_POST(Critical << "Free disk space is below threshold. "
                                 "Will no longer accept any writes.");
        }
    }
    catch (CFileErrnoException& ex) {
        ERR_POST(Critical << "Cannot read free disk space: " << ex);
    }
}

bool
CNCBlobStorage::x_WriteToDB(Uint8 coord, void* buf, size_t size)
{
    Uint4 file_id = Uint4(coord >> 32);
    Uint4 offset = Uint4(coord);
    if (file_id == m_CurFileId) {
        memcpy(m_CurFileMem + offset, buf, size);
    }
    else {
        TNCDBFilesMap::const_iterator file_it = m_DBFiles.find(file_id);
        if (file_it != m_DBFiles.end()) {
            TFileHandle fd = file_it->second->fd;
#ifdef NCBI_OS_LINUX
retry_write:
            ssize_t n_written = pwrite(fd, buf, size, offset);
            if (size_t(n_written) != size) {
                if (n_written == -1) {
                    int x_errno = errno;
                    if (x_errno == EINTR)
                        goto retry_write;
                    ERR_POST(Critical << "Unable to write to file, errno=" << x_errno);
                }
                else {
                    ERR_POST(Critical << "Unable to write to file, written only "
                                      << n_written << " bytes");
                }
                return false;
            }
#endif
        }
    }
    return true;
}

void
CNCBlobStorage::x_ChangeCacheDataCoord(SCacheDataRec* cache_data,
                                       Uint8 new_coord)
{
    m_DataCacheLock.Lock();
    m_DataCache.erase(*cache_data);
    cache_data->coord = new_coord;
    // Save some unnecessary checks and do not call insert_unique
    m_DataCache.insert_equal(*cache_data);
    m_DataCacheLock.Unlock();
}

bool
CNCBlobStorage::x_AdjustPrevChunkData(SWriteTask* task, bool& move_done)
{
    SCacheDataRec* cache_data = task->cache_data;
    Uint8 new_coord = task->coord;
    SFileChunkDataRec* chunk_rec = (SFileChunkDataRec*)cache_data->data;
    Uint1 chunk_idx = chunk_rec->chunk_num % kNCMaxChunksInMap;
    if (chunk_rec->chunk_num == 0  &&  task->task_type == eWriteRecord) {
        m_GarbageLock.Lock();
        m_UnfinishedBlobs[new_coord] = new_coord;
        m_GarbageLock.Unlock();
    }

    Uint8 prev_coord = chunk_rec->prev_coord;
    Uint8 next_coord = chunk_rec->next_coord;
    Uint8 map_coord = chunk_rec->map_coord;
    Uint8 meta_coord = 0;
    SCacheDataRec* prev_cache = NULL;
    SCacheDataRec* next_cache = NULL;
    SCacheDataRec* map_cache = NULL;
    SCacheDataRec* meta_cache = NULL;
    SFileChunkDataRec* prev_rec = NULL;
    SFileChunkDataRec* next_rec = NULL;
    SFileChunkMapRec* map_rec = NULL;
    SFileMetaRec* meta_rec = NULL;
    if (prev_coord != 0) {
        prev_cache = x_ReadCacheData(prev_coord, eFileRecChunkData);
        if (!prev_cache) {
            ERR_POST(Critical << "Cannot read previous chunk data");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(prev_cache);

        prev_rec = (SFileChunkDataRec*)prev_cache->data;
    }
    if (next_coord != 0) {
        next_cache = x_ReadCacheData(next_coord, eFileRecChunkData);
        if (!next_cache) {
            ERR_POST(Critical << "Cannot read next chunk data");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(next_cache);

        next_rec = (SFileChunkDataRec*)next_cache->data;
    }
    if (map_coord != 0) {
        map_cache = x_ReadCacheData(map_coord, eFileRecAny);
        if (!map_cache) {
            ERR_POST(Critical << "Cannot read chunk map");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(map_cache);

        if (map_cache->data->rec_type == eFileRecMeta) {
            meta_coord = map_coord;
            meta_cache = map_cache;
            map_cache = NULL;
            meta_rec = (SFileMetaRec*)meta_cache->data;
        }
        else if (map_cache->data->rec_type == eFileRecChunkMap) {
            map_rec = (SFileChunkMapRec*)map_cache->data;
            meta_coord = map_rec->meta_coord;
            meta_cache = x_ReadCacheData(meta_coord, eFileRecMeta);
            if (!meta_cache) {
                ERR_POST(Critical << "Cannot read meta info");
                return false;
            }
            // LRU is cleaned by this thread only, so we can return to LRU right away
            x_PutDataToLRU(meta_cache);

            meta_rec = (SFileMetaRec*)meta_cache->data;
        }
    }

    if (task->task_type == eMoveRecord  &&  cache_data->coord != new_coord) {
        string key(meta_rec->key_data, meta_rec->key_size);
        SNCCacheData* key_cache = x_GetKeyCacheData(meta_rec->slot, key, false);
        key_cache->lock.Lock();
        if (key_cache->ver_mgr) {
            key_cache->lock.Unlock();

            new_coord += (char*)&chunk_rec->map_coord - (char*)chunk_rec;
            *(Uint8*)(m_CurFileMem + Uint4(new_coord)) = 0;
            move_done = false;
            return true;
        }
        else {
            if (prev_rec)
                prev_rec->next_coord = new_coord;
            if (next_rec)
                next_rec->prev_coord = new_coord;
            if (map_rec)
                map_rec->chunk_coords[chunk_idx] = next_coord;
            if (chunk_rec->chunk_num == 0)
                meta_rec->data_coord = new_coord;

            Uint8 old_coord = cache_data->coord;
            x_ChangeCacheDataCoord(cache_data, new_coord);
            key_cache->lock.Unlock();

            old_coord += (char*)&chunk_rec->map_coord - (char*)chunk_rec;
            Uint8 map_coord = 0;
            x_WriteToDB(old_coord, &map_coord, sizeof(map_coord));
            // Ignore possible error as we cannot do anything about it
            // at this point.
        }
    }

    if (prev_coord != 0) {
        prev_rec->next_coord = new_coord;
        prev_coord += (char*)&chunk_rec->next_coord - (char*)chunk_rec;
        if (!x_WriteToDB(prev_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    if (next_coord != 0) {
        next_rec->prev_coord = new_coord;
        next_coord += (char*)&chunk_rec->prev_coord - (char*)chunk_rec;
        if (!x_WriteToDB(next_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    if (map_coord != 0) {
        map_rec->chunk_coords[chunk_idx] = new_coord;
        map_coord += (char*)&map_rec->chunk_coords[chunk_idx] - (char*)map_rec;
        if (!x_WriteToDB(map_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    if (meta_coord != 0  &&  chunk_rec->chunk_num == 0) {
        meta_rec->data_coord = new_coord;
        meta_coord += (char*)&meta_rec->data_coord - (char*)meta_rec;
        if (!x_WriteToDB(meta_coord, &new_coord, sizeof(new_coord)))
            return false;
    }

    return true;
}

bool
CNCBlobStorage::x_AdjustMapChunks(SWriteTask* task, bool& move_done)
{
    SCacheDataRec* cache_data = task->cache_data;
    Uint8 new_coord = task->coord;
    SFileChunkMapRec* map_rec = (SFileChunkMapRec*)cache_data->data;
    Uint8 prev_coord = map_rec->prev_coord;
    Uint8 next_coord = map_rec->next_coord;
    Uint8 meta_coord = map_rec->meta_coord;
    SCacheDataRec* prev_cache = NULL;
    SCacheDataRec* next_cache = NULL;
    SCacheDataRec* meta_cache = NULL;
    SFileChunkMapRec* prev_rec = NULL;
    SFileChunkMapRec* next_rec = NULL;
    SFileMetaRec* meta_rec = NULL;
    if (prev_coord != 0) {
        prev_cache = x_ReadCacheData(prev_coord, eFileRecChunkMap);
        if (!prev_cache) {
            ERR_POST(Critical << "Cannot read previous chunk map");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(prev_cache);

        prev_rec = (SFileChunkMapRec*)prev_cache->data;
    }
    if (next_coord != 0) {
        next_cache = x_ReadCacheData(next_coord, eFileRecChunkMap);
        if (!next_cache) {
            ERR_POST(Critical << "Cannot read next chunk map");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(next_cache);

        next_rec = (SFileChunkMapRec*)next_cache->data;
    }
    if (meta_coord != 0) {
        meta_cache = x_ReadCacheData(next_coord, eFileRecMeta);
        if (!meta_cache) {
            ERR_POST(Critical << "Cannot read meta info");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(meta_cache);

        meta_rec = (SFileMetaRec*)meta_cache->data;
    }

    if (task->task_type == eMoveRecord  &&  cache_data->coord != new_coord) {
        string key(meta_rec->key_data, meta_rec->key_size);
        SNCCacheData* key_cache = x_GetKeyCacheData(meta_rec->slot, key, false);
        key_cache->lock.Lock();
        if (key_cache->ver_mgr) {
            key_cache->lock.Unlock();

            new_coord += (char*)&map_rec->meta_coord - (char*)map_rec;
            *(Uint8*)(m_CurFileMem + Uint4(new_coord)) = 0;
            move_done = false;
            return true;
        }
        else {
            if (prev_rec)
                prev_rec->next_coord = new_coord;
            if (next_rec)
                next_rec->prev_coord = new_coord;
            if (map_rec->map_num == 0)
                meta_rec->map_coord = new_coord;

            Uint8 old_coord = cache_data->coord;
            x_ChangeCacheDataCoord(cache_data, new_coord);
            key_cache->lock.Unlock();

            old_coord += (char*)&map_rec->meta_coord - (char*)map_rec;
            Uint8 meta_coord = 0;
            x_WriteToDB(old_coord, &meta_coord, sizeof(meta_coord));
            // Ignore possible error as we cannot do anything about it
            // at this point.
        }
    }

    if (prev_coord != 0) {
        prev_rec->next_coord = new_coord;
        prev_coord += (char*)&map_rec->next_coord - (char*)map_rec;
        if (!x_WriteToDB(prev_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    if (next_coord != 0) {
        next_rec->prev_coord = new_coord;
        next_coord += (char*)&map_rec->prev_coord - (char*)map_rec;
        if (!x_WriteToDB(next_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    if (meta_coord != 0) {
        meta_rec->map_coord = new_coord;
        meta_coord += (char*)&meta_rec->map_coord - (char*)meta_rec;
        if (!x_WriteToDB(meta_coord, &new_coord, sizeof(new_coord)))
            return false;
    }

    Uint8* chunk_coords = map_rec->chunk_coords;
    Uint8* last_coord = (Uint8*)((char*)map_rec + map_rec->rec_size);
    for (; chunk_coords != last_coord; ++chunk_coords) {
        Uint8 coord = *chunk_coords;
        SCacheDataRec* chunk_data = x_ReadCacheData(coord, eFileRecChunkData);
        if (!chunk_data) {
            ERR_POST(Critical << "Cannot read blob chunk");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(chunk_data);

        SFileChunkDataRec* chunk_rec = (SFileChunkDataRec*)chunk_data->data;
        chunk_rec->map_coord = new_coord;
        coord += (char*)&chunk_rec->map_coord - (char*)chunk_rec;
        if (!x_WriteToDB(coord, &new_coord, sizeof(new_coord)))
            return false;
    }

    return true;
}

bool
CNCBlobStorage::x_AdjustBlobMaps(SWriteTask* task, bool& move_done)
{
    SCacheDataRec* cache_data = task->cache_data;
    Uint8 new_coord = task->coord;
    SFileMetaRec* meta_rec = (SFileMetaRec*)cache_data->data;
    if (meta_rec->size != 0) {
        m_GarbageLock.Lock();
        m_UnfinishedBlobs.erase(meta_rec->data_coord);
        m_GarbageLock.Unlock();
    }

    if (task->task_type == eMoveRecord  &&  cache_data->coord != new_coord) {
        string key(meta_rec->key_data, meta_rec->key_size);
        SNCCacheData* key_cache = x_GetKeyCacheData(meta_rec->slot, key, false);
        key_cache->lock.Lock();
        if (key_cache->ver_mgr) {
            key_cache->lock.Unlock();

            new_coord += (char*)&meta_rec->deleted - (char*)meta_rec;
            *(Uint1*)(m_CurFileMem + Uint4(new_coord)) = 1;
            move_done = false;
            return true;
        }
        else {
            key_cache->coord = new_coord;
            Uint8 old_coord = cache_data->coord;
            x_ChangeCacheDataCoord(cache_data, new_coord);
            key_cache->lock.Unlock();

            old_coord += (char*)&meta_rec->deleted - (char*)meta_rec;
            Uint1 deleted = 1;
            x_WriteToDB(old_coord, &deleted, sizeof(deleted));
            // Ignore possible error as we cannot do anything about it
            // at this point.
        }
    }

    Uint8 map_coord = meta_rec->map_coord;
    if (meta_rec->size == 0) {
        // do nothing
    }
    else if (map_coord == 0) {
        Uint8 chunk_coord = meta_rec->data_coord;
        SCacheDataRec* chunk_data = x_ReadCacheData(chunk_coord, eFileRecChunkData);
        if (!chunk_data) {
            ERR_POST(Critical << "Cannot read the only blob chunk");
            return false;
        }
        // LRU is cleaned by this thread only, so we can return to LRU right away
        x_PutDataToLRU(chunk_data);

        SFileChunkDataRec* chunk_rec = (SFileChunkDataRec*)chunk_data->data;
        chunk_rec->map_coord = new_coord;
        chunk_coord += (char*)&chunk_rec->map_coord - (char*)chunk_rec;
        if (!x_WriteToDB(chunk_coord, &new_coord, sizeof(new_coord)))
            return false;
    }
    else {
        while (map_coord != 0) {
            SCacheDataRec* map_data = x_ReadCacheData(map_coord, eFileRecChunkMap);
            if (!map_data) {
                ERR_POST(Critical << "Cannot read chunk map for blob");
                return false;
            }
            // LRU is cleaned by this thread only, so we can return to LRU right away
            x_PutDataToLRU(map_data);

            SFileChunkMapRec* map_rec = (SFileChunkMapRec*)map_data->data;
            map_rec->meta_coord = new_coord;
            map_coord += (char*)&map_rec->meta_coord - (char*)map_rec;
            if (!x_WriteToDB(map_coord, &new_coord, sizeof(new_coord)))
                return false;

            map_coord = map_rec->next_coord;
        }
    }
    return true;
}

bool
CNCBlobStorage::x_DoWriteRecord(SWriteTask* task)
{
    SCacheDataRec* cache_data = task->cache_data;
    Uint4 file_id = Uint4(task->coord >> 32);
    if (file_id != m_CurFileId) {
        m_CurFileInfo->file_size = m_CurFileOff;
        if (!x_CreateNewFile())
            return false;
    }
    if (file_id != m_CurFileId)
        abort();

    Uint4 offset = Uint4(task->coord);
    memcpy(m_CurFileMem + offset, cache_data->data, cache_data->data->rec_size);
    bool move_done = true;
    switch (cache_data->data->rec_type) {
    case eFileRecMeta:
        if (!x_AdjustBlobMaps(task, move_done))
            return false;
        break;
    case eFileRecChunkMap:
        if (!x_AdjustMapChunks(task, move_done))
            return false;
        break;
    case eFileRecChunkData:
        if (!x_AdjustPrevChunkData(task, move_done))
            return false;
        break;
    default:
        abort();
    }
    m_CurFileOff = offset + cache_data->data->rec_size;
    m_CurDBSize += cache_data->data->rec_size;
    m_TotalDataWritten += cache_data->data->rec_size;
    if (task->task_type == eMoveRecord) {
        // Either old record or new one became garbage
        x_AddGarbageSize(cache_data->data->rec_size);
        if (move_done)
            m_MovedSize += cache_data->data->rec_size;
        else
            ++m_CntFailedMoves;
    }
    x_PutDataToLRU(cache_data);
    return true;
}

inline void
CNCBlobStorage::x_EraseFromDataCache(SCacheDataRec* cache_data)
{
    m_DataCache.erase(*cache_data);
    m_DataCacheSize -= sizeof(*cache_data) + cache_data->data->rec_size;
    --m_CntDataCacheRecs;
}

bool
CNCBlobStorage::x_DoDelBlob(SWriteTask* task)
{
    Uint8 coord = task->coord;
    SCacheDataRec* cache_data = x_ReadCacheData(coord, eFileRecMeta);
    if (!cache_data) {
        ERR_POST(Critical << "Cannot read blob meta data");
        return false;
    }
    SFileMetaRec* meta_rec = (SFileMetaRec*)cache_data->data;
    meta_rec->deleted = 1;
    if (meta_rec->dead_time == m_FirstRecDead)
        m_FirstRecDead = 0;
    coord += ((char*)&meta_rec->deleted - (char*)meta_rec);
    if (!x_WriteToDB(coord, &meta_rec->deleted, sizeof(meta_rec->deleted)))
        return false;

    if (task->task_type == eDelMetaInfo) {
        x_AddGarbageSize(meta_rec->rec_size);
    }
    else {
        Uint8 disk_size = x_CalcBlobDiskSize(meta_rec->key_size, meta_rec->size);
        x_AddGarbageSize(Int8(disk_size));
    }

    m_DataCacheLock.Lock();
    x_EraseFromDataCache(cache_data);
    m_DataCacheLock.Unlock();
    x_DeleteCacheData(cache_data);

    return true;
}

bool
CNCBlobStorage::x_ExecuteWriteTasks(void)
{
    if (m_NeedSaveLogRecNo) {
        m_NeedSaveLogRecNo = false;
        try {
            CFastMutexGuard guard(m_IndexLock);
            m_IndexDB->SetMaxSyncLogRecNo(CNCSyncLog::GetLastRecNo());
        }
        catch (CSQLITE_Exception& ex) {
            ERR_POST(Critical << "Error saving max_sync_log_rec_no: " << ex);
        }
    }

    TWriteQueue q;
    m_WriteQueueLock.Lock();
    q.swap(m_WriteQueue);
    Uint4 cnt_tasks = m_CntWriteTasks;
    m_CntWriteTasks = 0;
    m_WriteQueueLock.Unlock();

    SWriteTask* task = NULL;
    bool success = true;
    Uint4 cnt_executed = 0;
    while (success  &&  !q.empty()) {
        task = &q.front();
        q.pop_front();

        switch (task->task_type) {
        case eMoveRecord:
            if (task->cache_data->coord == m_FirstRecCoord)
                m_FirstRecDead = 0;
            // fall through
        case eWriteRecord:
            if (!x_DoWriteRecord(task))
                success = false;
            break;
        case eDelMetaInfo:
        case eDelBlob:
            if (!x_DoDelBlob(task))
                success = false;
            break;
        default:
            abort();
        }
        delete task;
        if (success)
            ++cnt_executed;
    }

    if (success) {
        x_FlushCurFile(false);
    }
    else {
        m_WriteQueueLock.Lock();
        m_WriteQueue.splice(m_WriteQueue.begin(), q, q.begin(), q.end());
        m_WriteQueue.push_front(*task);
        m_CntWriteTasks += cnt_tasks - cnt_executed;
        m_WriteQueueLock.Unlock();
        m_IsStopWrite = true;
    }
    return true;
}

void
CNCBlobStorage::x_GC_DeleteExpired(const string& key,
                                   Uint2 slot,
                                   int dead_before)
{
    m_GCAccessor->Prepare(key, kEmptyStr, slot, eNCGCDelete);
    x_InitializeAccessor(m_GCAccessor);
    while (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock) {
        m_GCBlockWaiter.Wait();
    }
    if (m_GCAccessor->IsBlobExists()
        &&  m_GCAccessor->GetCurBlobDeadTime() < dead_before)
    {
        m_GCAccessor->DeleteBlob(dead_before);
        ++m_GCDeleted;
    }
    m_GCAccessor->Deinitialize();
}

void
CNCBlobStorage::x_RunGC(void)
{
    int cur_time = int(time(NULL));
    int next_dead = cur_time;
    if (m_DoExtraGC) {
        if (m_CurDBSize <= m_ExtraGCOffSize) {
            m_DoExtraGC = false;
        }
        else {
            ++m_ExtraGCTime;
            next_dead += m_ExtraGCTime;
        }
    }
    else if (m_ExtraGCOnSize != 0
             &&  m_CurDBSize >= m_ExtraGCOnSize)
    {
        m_DoExtraGC = true;
        m_ExtraGCTime = 1;
        next_dead += m_ExtraGCTime;
    }

    int cnt_read = 1;
    while (cnt_read != 0  &&  !m_Stopped) {
        m_GCKeys.clear();
        m_GCSlots.clear();
        cnt_read = 0;
        m_TimeTableLock.Lock();
        ITERATE(TTimeTableMap, it, m_TimeTable) {
            const SNCCacheData* cache_data = &*it;
            if (cache_data->dead_time >= next_dead
                ||  cnt_read >= m_GCBatchSize)
            {
                break;
            }
            m_GCKeys.push_back(cache_data->key);
            m_GCSlots.push_back(cache_data->slot);
            ++cnt_read;
        }
        m_TimeTableLock.Unlock();
        for (size_t i = 0; i < m_GCKeys.size()  &&  !m_Stopped; ++i) {
            x_GC_DeleteExpired(m_GCKeys[i], m_GCSlots[i], next_dead);
        }
    }
}

void
CNCBlobStorage::x_CleanDataCache(void)
{
    size_t limit = CNCMemManager::GetMemoryLimit();
    size_t used = CNCMemManager::GetMemoryUsed();
    size_t cache = m_DataCacheSize + m_CurFileOff;
    size_t other = used - cache;
    size_t need_cache = max(limit / 2, limit - other);
    if (cache <= need_cache + kMemSizeTolerance)
        return;

    size_t to_clean = cache - need_cache + kMemSizeTolerance;
    size_t cleaned = 0;
    m_DataCacheLock.Lock();
    TDataRecLRU::iterator next_it = m_DataLRU.begin();
    for (; cleaned < to_clean  &&  next_it != m_DataLRU.end(); ++next_it) {
        SCacheDataRec* cache_data = &*next_it;
        cleaned += sizeof(*cache_data) + cache_data->data->rec_size;
        x_EraseFromDataCache(cache_data);
    }
    TDataRecLRU to_delete;
    to_delete.splice(to_delete.begin(), m_DataLRU, m_DataLRU.begin(), next_it);
    m_DataCacheLock.Unlock();

    while (!to_delete.empty()  &&  !m_Stopped) {
        SCacheDataRec* cache_data = &to_delete.front();
        to_delete.pop_front();
        x_DeleteCacheData(cache_data);
    }
}

inline void
CNCBlobStorage::x_ReadRecDeadFromMeta(SCacheDataRec* cache_data, int& dead_time)
{
    SFileMetaRec* meta_rec = (SFileMetaRec*)cache_data->data;
    if (meta_rec->deleted)
        dead_time = kDeadTimeDead;
    else
        dead_time = meta_rec->dead_time;
}

inline void
CNCBlobStorage::x_ReadRecDeadFromUnfinished(Uint8 data_coord, int& dead_time)
{
    m_GarbageLock.Lock();
    if (m_UnfinishedBlobs.find(data_coord) != m_UnfinishedBlobs.end())
        dead_time = kDeadTimeUnfinished;
    else
        dead_time = kDeadTimeDead;
    m_GarbageLock.Unlock();
}

void
CNCBlobStorage::x_ReadRecDeadFromMap(SCacheDataRec* cache_data, int& dead_time)
{
    SFileChunkMapRec* map_rec = (SFileChunkMapRec*)cache_data->data;
    if (map_rec->meta_coord == 0) {
        if (map_rec->map_num == 0)
            x_ReadRecDeadFromUnfinished(map_rec->chunk_coords[0], dead_time);
        else
            dead_time = kDeadTimeDead;
        return;
    }

    SCacheDataRec* meta_cache = x_ReadCacheData(map_rec->meta_coord, eFileRecMeta);
    if (meta_cache) {
        x_ReadRecDeadFromMeta(meta_cache, dead_time);
        if (dead_time == kDeadTimeDead)
            map_rec->meta_coord = 0;
        x_PutDataToLRU(meta_cache);
    }
    else {
        ERR_POST(Critical << "Cannot understand expiration time of "
                             "record. Record will be deleted.");
        dead_time = kDeadTimeDead;
    }
}

void
CNCBlobStorage::x_ReadRecDeadFromData(SCacheDataRec* cache_data, int& dead_time)
{
    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)cache_data->data;
    if (data_rec->map_coord == 0) {
        if (data_rec->chunk_num == 0)
            x_ReadRecDeadFromUnfinished(cache_data->coord, dead_time);
        else
            dead_time = kDeadTimeDead;
        return;
    }

    SCacheDataRec* map_cache = x_ReadCacheData(data_rec->map_coord, eFileRecAny);
    if (map_cache) {
        if (map_cache->data->rec_type == eFileRecMeta) {
            x_ReadRecDeadFromMeta(map_cache, dead_time);
            x_PutDataToLRU(map_cache);
        }
        else if (map_cache->data->rec_type == eFileRecChunkMap) {
            x_ReadRecDeadFromMap(map_cache, dead_time);
            x_PutDataToLRU(map_cache);
        }
        else {
            x_PutDataToLRU(map_cache);
            goto cannot_find_map;
        }
        if (dead_time == kDeadTimeDead)
            data_rec->map_coord = 0;
    }
    else {
cannot_find_map:
        ERR_POST(Critical << "Cannot find map record for chunk data. "
                             "Record will be deleted.");
        dead_time = kDeadTimeDead;
    }
}

bool
CNCBlobStorage::x_ReadRecDeadTime(Uint8 coord, int& dead_time, Uint2& rec_size)
{
    SCacheDataRec* cache_data = x_ReadCacheData(coord, eFileRecAny);
    if (!cache_data) {
        ERR_POST(Critical << "Cannot read record in storage.");
        return false;
    }
    rec_size = cache_data->data->rec_size;
    if (cache_data->data->rec_type == eFileRecMeta)
        x_ReadRecDeadFromMeta(cache_data, dead_time);
    else if (cache_data->data->rec_type == eFileRecChunkMap)
        x_ReadRecDeadFromMap(cache_data, dead_time);
    else if (cache_data->data->rec_type == eFileRecChunkData)
        x_ReadRecDeadFromData(cache_data, dead_time);
    else  {
        ERR_POST(Critical << "Storage is corrupted. Record will be deleted.");
        dead_time = kDeadTimeDead;
    }
    x_PutDataToLRU(cache_data);

    return true;
}

void
CNCBlobStorage::x_AdvanceFirstRecPtr(void)
{
    int cur_time = int(time(NULL));
    TNCDBFilesMap::iterator it_file = m_DBFiles.begin();
    SNCDBFileInfo* file_info = it_file->second;
    for (;;) {
        if (m_FirstRecCoord == m_NextWriteCoord
            ||  Uint4(m_FirstRecCoord >> 32) == m_CurFileId
            ||  (m_FirstRecDead != kDeadTimeUnfinished
                 &&  m_FirstRecDead >= cur_time)
            ||  m_Stopped)
        {
            break;
        }
        Uint2 rec_size = 0;
        if (Uint4(m_FirstRecCoord) >= file_info->file_size
            ||  !x_ReadRecDeadTime(m_FirstRecCoord, m_FirstRecDead, rec_size))
        {
            x_DeleteDBFile(it_file);
            m_FirstRecCoord = Uint8(m_DBFiles.begin()->second->file_id) << 32;
            m_FirstRecDead = 0;
            break;
        }
        if (m_FirstRecDead != kDeadTimeDead)
            break;

        m_DataCacheLock.Lock();
        TDataRecMap::iterator it = m_DataCache.find(m_FirstRecCoord, SCacheRecCompare());
        if (it == m_DataCache.end())
            abort();
        SCacheDataRec* cache_data = &*it;
        x_EraseFromDataCache(cache_data);
        x_RemoveDataFromLRU(cache_data);
        m_DataCacheLock.Unlock();

        x_DeleteCacheData(cache_data);

        m_CurDBSize -= rec_size;
        x_AddGarbageSize(-Int8(rec_size));
        m_FirstRecCoord += rec_size;
        m_FirstRecDead = 0;
    }
    LOG_POST("Currently db size " << m_CurDBSize);
}

void
CNCBlobStorage::x_ShrinkDiskStorage(void)
{
    if (m_CurDBSize < m_MinDBSize
        ||  m_GarbageSize * 100 <= m_CurDBSize * m_MaxGarbagePct
        ||  m_Stopped)
    {
        return;
    }

    int cur_time = int(time(NULL));
    TNCDBFilesMap::iterator it_file = m_DBFiles.begin();
    SNCDBFileInfo* file_info = it_file->second;
    Uint8 cur_rec_coord = m_FirstRecCoord;
    Uint4 moved_size = 0;
    while (Uint4(cur_rec_coord) < file_info->file_size
           &&  moved_size < m_MaxMoveSize)
    {
        int dead_time = kDeadTimeDead;
        Uint2 rec_size = 0;
        if (!x_ReadRecDeadTime(cur_rec_coord, dead_time, rec_size))
            break;
        if (dead_time == kDeadTimeUnfinished
            ||  dead_time < cur_time + m_MinMoveLife)
        {
            break;
        }
        if (dead_time != kDeadTimeDead) {
            SCacheDataRec* cache_data = x_GetCacheData(cur_rec_coord);
            x_AddWriteRecordTask(cache_data, eMoveRecord);
            ++m_CntMoveTasks;
        }
        moved_size += rec_size;
        cur_rec_coord += rec_size;
    }
}

void
CNCBlobStorage::x_DoBackgroundWork(void)
{
    x_CacheDatabase();

    for (;;) {
        if (m_Stopped)
            break;

        x_HeartBeat();
        x_CollectStorageStats();
        bool had_tasks = x_ExecuteWriteTasks();
        if (m_Stopped) {
            if (had_tasks)
                continue;
            break;
        }
        x_RunGC();
        x_CleanDataCache();
        x_AdvanceFirstRecPtr();
        x_ShrinkDiskStorage();

        m_StopTrigger.TryWait(1, 0);
    }
}


CNCBlobStorage::SSlotCache::SSlotCache(Uint2 slot)
    : deleter(CKeysCleaner(slot))
{}


CNCBlobStorage::CKeysCleaner::CKeysCleaner(Uint2 slot)
    : m_Slot(slot)
{}

void
CNCBlobStorage::CKeysCleaner::Delete(const string& key) const
{
    SSlotCache* cache = g_NCStorage->x_GetSlotCache(m_Slot);
    cache->lock.WriteLock();
    TKeyMap::iterator it = cache->key_map.find(key, SCacheKeyCompare());
    if (it != cache->key_map.end()) {
        SNCCacheData* cache_data = &*it;
        if (cache_data->key_deleted) {
            if (cache_data->key_del_time >= int(time(NULL)) - 2) {
                cache->deleter.AddElement(key);
            }
            else {
                cache->key_map.erase(it);
                delete cache_data;
            }
        }
    }
    cache->lock.WriteUnlock();
}

END_NCBI_SCOPE
