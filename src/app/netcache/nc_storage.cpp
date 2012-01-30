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
static const char* kNCStorage_MetaFileSuffix  = "";
static const char* kNCStorage_DataFileSuffix  = "";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_RegSection        = "storage";
static const char* kNCStorage_PathParam         = "path";
static const char* kNCStorage_FilePrefixParam   = "prefix";
static const char* kNCStorage_FileSizeParam     = "each_file_size";
static const char* kNCStorage_GarbagePctParam   = "max_garbage_pct";
static const char* kNCStorage_MinDBSizeParam    = "min_storage_size";
//static const char* kNCStorage_MoveLifeParam     = "min_lifetime_to_move";
static const char* kNCStorage_MaxIOWaitParam    = "max_io_wait_time";
static const char* kNCStorage_GCBatchParam      = "gc_batch_size";
static const char* kNCStorage_FlushTimeParam    = "sync_time_period";
static const char* kNCStorage_ExtraGCOnParam    = "db_limit_del_old_on";
static const char* kNCStorage_ExtraGCOffParam   = "db_limit_del_old_off";
static const char* kNCStorage_StopWriteOnParam  = "db_limit_stop_write_on";
static const char* kNCStorage_StopWriteOffParam = "db_limit_stop_write_off";
static const char* kNCStorage_MinFreeDiskParam  = "disk_free_limit";
static const char* kNCStorage_DiskCriticalParam = "critical_disk_free_limit";
static const char* kNCStorage_MinRecNoSaveParam = "min_rec_no_save_period";


static const Uint8 kMetaSignature = NCBI_CONST_UINT8(0xeed5be66cdafbfa3);
static const Uint8 kDataSignature = NCBI_CONST_UINT8(0xaf9bedf24cfa05ed);
static const Uint1 kSignatureSize = Uint1(sizeof(kMetaSignature));


/// Size of memory page that is a granularity of all allocations from OS.
static const size_t kMemPageSize  = 4 * 1024;
/// Mask that can move pointer address or memory size to the memory page
/// boundary.
static const size_t kMemPageAlignMask = ~(kMemPageSize - 1);




static char*
s_MapFile(TFileHandle fd, size_t file_size)
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
    /*int res = madvise(mem_ptr, file_size, MADV_SEQUENTIAL);
    if (res) {
        ERR_POST(Critical << "madvise failed, errno=" << errno);
    }*/
#endif
    return mem_ptr;
}

static void
s_UnmapFile(char* mem_ptr, size_t file_size)
{
#ifdef NCBI_OS_LINUX
    file_size = (file_size + kMemPageSize - 1) & kMemPageAlignMask;
    munmap(mem_ptr, file_size);
#endif
}



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

    string str = reg.GetString(kNCStorage_RegSection, kNCStorage_FileSizeParam, "100Mb");
    m_NewFileSize = Uint4(NStr::StringToUInt8_DataSize(str));
    m_MaxGarbagePct = reg.GetInt(kNCStorage_RegSection, kNCStorage_GarbagePctParam, 50);
    str = reg.GetString(kNCStorage_RegSection, kNCStorage_MinDBSizeParam, "1Gb");
    m_MinDBSize = NStr::StringToUInt8_DataSize(str);
    //m_MinMoveLife = reg.GetInt(kNCStorage_RegSection, kNCStorage_MoveLifeParam, 600);
    m_MinMoveLife = 0;

    m_MaxIOWaitTime = reg.GetInt(kNCStorage_RegSection, kNCStorage_MaxIOWaitParam, 10);

    m_MinRecNoSavePeriod = reg.GetInt(kNCStorage_RegSection, kNCStorage_MinRecNoSaveParam, 30);
    m_FlushTimePeriod = reg.GetInt(kNCStorage_RegSection, kNCStorage_FlushTimeParam, 0);

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
    m_DiskCritical   = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_DiskCriticalParam, "1Gb"));

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
CNCBlobStorage::x_GetFileName(Uint4 file_id, ENCDBFileType file_type)
{
    string file_name(m_Prefix);
    switch (file_type) {
    case eDBFileMeta:
        file_name += kNCStorage_MetaFileSuffix;
        break;
    case eDBFileData:
        file_name += kNCStorage_DataFileSuffix;
        break;
    default:
        abort();
    }
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
        string pid(NStr::UInt8ToString(CProcess::GetCurrentPid()));
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
                CRef<SNCDBFileInfo> info = it->second;
#ifdef NCBI_OS_LINUX
                info->fd = open(info->file_name.c_str(), O_RDWR | O_NOATIME);
                if (info->fd == -1) {
                    ERR_POST(Critical << "Cannot open storage file, errno="
                                      << errno);
delete_file:
                    try {
                        m_IndexDB->DeleteDBFile(info->file_id);
                    }
                    catch (CSQLITE_Exception& ex) {
                        ERR_POST(Critical << "Error cleaning index file: " << ex);
                    }
                    m_DBFiles.erase(it);
                    continue;
                }
                Int8 file_size = CFile(info->file_name).GetLength();
                if (file_size == -1) {
                    ERR_POST(Critical << "Cannot read file size (errno=" << errno
                                      << "). File " << info->file_name
                                      << " will be deleted.");
                    goto delete_file;
                }
                info->file_size = Uint4(file_size);
                info->file_map = s_MapFile(info->fd, info->file_size);
                if (!info->file_map)
                    goto delete_file;
                if (*(Uint8*)info->file_map == kMetaSignature)
                    info->file_type = eDBFileMeta;
                else if (*(Uint8*)info->file_map == kDataSignature)
                    info->file_type = eDBFileData;
                else {
                    ERR_POST(Critical << "Unknown file signature: " << *(Uint8*)info->file_map);
                    goto delete_file;
                }
                info->index_head = (SFileIndexRec*)(info->file_map + file_size);
                --info->index_head;
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

inline SFileIndexRec*
CNCBlobStorage::x_DeleteIndexRec(SFileIndexRec* index_head, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = index_head - rec_num;
    SFileIndexRec* prev_rec = index_head - ind_rec->prev_idx;
    SFileIndexRec* next_rec = index_head - ind_rec->next_idx;
    prev_rec->next_idx = ind_rec->next_idx;
    next_rec->prev_idx = ind_rec->prev_idx;
    ind_rec->next_idx = ind_rec->prev_idx = rec_num;
    return prev_rec;
}

void
CNCBlobStorage::x_MoveRecToGarbage(SNCDBFileInfo* file_info,
                                   SFileRecHeader* rec_head)
{
    Uint4 rec_num = rec_head->rec_num;
    Uint4 size = rec_head->rec_size;
    if (size & 7)
        size += 8 - (size & 7);
    size += sizeof(SFileIndexRec);

    file_info->info_lock.Lock();
    x_DeleteIndexRec(file_info->index_head, rec_num);
    if (file_info->used_size < size)
        abort();
    file_info->used_size -= size;
    file_info->garb_size += size;
    if (file_info->garb_size > file_info->file_size)
        abort();
    if (file_info->index_head->next_idx == 0  &&  file_info->used_size != 0)
        abort();
    file_info->info_lock.Unlock();

    m_GarbageLock.Lock();
    m_GarbageSize += size;
    m_GarbageLock.Unlock();
}

CRef<SNCDBFileInfo>
CNCBlobStorage::x_GetFileForCoord(Uint8 coord)
{
    Uint4 file_id = Uint4(coord >> 32);

    m_DBFilesLock.Lock();
    TNCDBFilesMap::const_iterator it = m_DBFiles.find(file_id);
    if (it == m_DBFiles.end()) {
        m_DBFilesLock.Unlock();
        return null;
    }
    CRef<SNCDBFileInfo> file_info = it->second;
    m_DBFilesLock.Unlock();

    return file_info;
}

bool
CNCBlobStorage::x_CreateNewFile(ENCDBFileType file_type)
{
    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    Uint4 file_id;
    if (m_LastFileId == kNCMaxDBFileId)
        file_id = 1;
    else
        file_id = m_LastFileId + 1;
    //ENCDBFileType true_file_type = ENCDBFileType(file_type & ~fDBFileForMove);
    ENCDBFileType true_file_type = file_type;
    string file_name = x_GetFileName(file_id, true_file_type);
    SNCDBFileInfo* file_info = new SNCDBFileInfo();
    file_info->file_id      = file_id;
    file_info->file_name    = file_name;
    file_info->create_time  = int(time(NULL));
    file_info->file_size    = m_NewFileSize;
    file_info->file_type    = true_file_type;

#ifdef NCBI_OS_LINUX
    file_info->fd = open(file_name.c_str(),
                         O_RDWR | O_CREAT | O_TRUNC | O_NOATIME,
                         S_IRUSR | S_IWUSR);
    if (file_info->fd == -1) {
        ERR_POST(Critical << "Cannot create new storage file, errno=" << errno);
        delete file_info;
        return false;
    }
    int trunc_res = ftruncate(file_info->fd, m_NewFileSize);
    if (trunc_res != 0) {
        ERR_POST(Critical << "Cannot truncate new file, errno=" << errno);
        delete file_info;
        return false;
    }
    file_info->file_map = s_MapFile(file_info->fd, m_NewFileSize);
    if (!file_info->file_map) {
        delete file_info;
        return false;
    }
#endif

    file_info->index_head = (SFileIndexRec*)(file_info->file_map + file_info->file_size);
    --file_info->index_head;
    file_info->index_head->next_idx = file_info->index_head->prev_idx = 0;

    try {
        CFastMutexGuard guard(m_IndexLock);
        m_IndexDB->NewDBFile(file_id, file_name);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Error while adding new storage file: " << ex);
        delete file_info;
        return false;
    }

    m_LastFileId = file_id;
    switch (true_file_type) {
    case eDBFileMeta:
        *(Uint8*)file_info->file_map = kMetaSignature;
        break;
    case eDBFileData:
        *(Uint8*)file_info->file_map = kDataSignature;
        break;
    default:
        abort();
    }

    m_DBFilesLock.Lock();
    m_DBFiles[file_id] = file_info;

    m_NextWriteLock.Lock();
    switch (file_type) {
    case eDBFileMeta:
        m_MetaWriting.next_file = file_info;
        break;
    case eDBFileData:
        m_DataWriting.next_file = file_info;
        break;
    /*case eDBFileMoveMeta:
        m_MetaMoving.next_file = file_info;
        break;
    case eDBFileMoveData:
        m_DataMoving.next_file = file_info;
        break;*/
    default:
        abort();
    }
    m_CurDBSize += kSignatureSize + sizeof(SFileIndexRec);
    m_NextWriteLock.Unlock();

    m_DBFilesLock.Unlock();

    m_NextWaitLock.Lock();
    if (m_NextWaiters != 0)
        m_NextWait.SignalAll();
    m_NextWaitLock.Unlock();

    return true;
}

void
CNCBlobStorage::x_DeleteDBFile(CRef<SNCDBFileInfo> file_info)
{
    try {
        CFastMutexGuard guard(m_IndexLock);
        m_IndexDB->DeleteDBFile(file_info->file_id);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Index database does not delete rows: " << ex);
    }

    m_DBFilesLock.Lock();
    m_DBFiles.erase(file_info->file_id);
    m_DBFilesLock.Unlock();

    m_NextWriteLock.Lock();
    m_CurDBSize -= file_info->file_size;
    m_NextWriteLock.Unlock();
    m_GarbageLock.Lock();
    m_GarbageSize -= file_info->garb_size;
    m_GarbageLock.Unlock();
}

SNCDBFileInfo::SNCDBFileInfo(void)
    : file_map(NULL),
      file_id(0),
      file_size(0),
      garb_size(0),
      used_size(0),
      index_head(NULL),
      fd(0),
      create_time(0),
      last_shrink_time(0)
{}

SNCDBFileInfo::~SNCDBFileInfo(void)
{
    if (file_map)
        s_UnmapFile(file_map, file_size);
#ifdef NCBI_OS_LINUX
    if (fd  &&  fd != -1  &&  close(fd)) {
        ERR_POST(Critical << "Error closing file " << file_name
                          << ", errno=" << errno);
    }
    if (unlink(file_name.c_str())) {
        ERR_POST(Critical << "Error deleting file " << file_name
                          << ", errno=" << errno);
    }
#endif
}

CNCBlobStorage::CNCBlobStorage(void)
    : m_Stopped(false),
      m_LastFileId(0),
      m_NextWaiters(0),
      m_CurBlobsCnt(0),
      m_CurDBSize(0),
      m_GarbageSize(0),
      m_CntMoveTasks(0),
      m_CntFailedMoves(0),
      m_MovedSize(0),
      m_LastFlushTime(0),
      m_FreeAccessors(NULL),
      m_UsedAccessors(NULL),
      m_CntUsedHolders(0),
      m_IsStopWrite(eNoStop),
      m_DoExtraGC(false),
      m_LastRecNoSaveTime(0),
      m_ExtraGCTime(0)
{
    m_MetaWriting.cur_file = m_MetaWriting.next_file = NULL;
    m_DataWriting.cur_file = m_DataWriting.next_file = NULL;
}

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
    if (!m_DBFiles.empty()) {
        int max_create_time = 0;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CRef<SNCDBFileInfo> file_info = it->second;
            if (file_info->create_time >= max_create_time) {
                max_create_time = file_info->create_time;
                m_LastFileId = file_info->file_id;
            }
        }
    }

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
    m_StopCond.SignalAll();
    m_NextFileSwitchLock.Lock();
    m_NextFileSwitch.SignalAll();
    m_NextFileSwitchLock.Unlock();

    if (m_FlushThread) {
        try {
            m_FlushThread->Join();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
        }
    }
    if (m_NewFileThread) {
        try {
            m_NewFileThread->Join();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
        }
    }
    if (m_GCThread) {
        try {
            m_GCThread->Join();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
        }
    }
    if (m_BGThread) {
        try {
            m_BGThread->Join();
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
        }
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

bool
CNCBlobStorage::DeleteBlobKey(Uint2 slot, const string& key)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    cache->lock.WriteLock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (data->coord != 0) {
        cache->lock.WriteUnlock();
        return false;
    }
    data->key_deleted = true;
    data->key_del_time = int(time(NULL));
    cache->lock.WriteUnlock();
    cache->deleter.AddElement(key);
    return true;
}

void
CNCBlobStorage::RestoreBlobKey(Uint2 slot,
                               const string& key,
                               SNCCacheData* cache_data)
{
    SSlotCache* cache = x_GetSlotCache(slot);
    cache->lock.WriteLock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (data == cache_data)
        data->key_deleted = false;
    /*else
        abort();*/
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

inline void
CNCBlobStorage::x_SwitchToNextFile(SWritingInfo* w_info)
{
    w_info->cur_file = w_info->next_file;
    w_info->next_file = NULL;
    w_info->next_rec_num = 1;
    w_info->next_coord = (Uint8(w_info->cur_file->file_id) << 32) + kSignatureSize;
    w_info->left_file_size = w_info->cur_file->file_size
                             - (kSignatureSize + sizeof(SFileIndexRec));
}

bool
CNCBlobStorage::x_GetNextWriteCoord(ENCDBFileType file_type,
                                    Uint4 rec_size,
                                    Uint8& coord,
                                    SFileRecHeader*& write_head)
{
    Uint4 true_rec_size = rec_size;
    if (true_rec_size & 7)
        true_rec_size += 8 - (true_rec_size & 7);
    Uint4 reserve_size = true_rec_size + sizeof(SFileIndexRec);

    m_NextWriteLock.Lock();

    bool need_signal_switch = false;
    SWritingInfo* w_info;
    switch (file_type) {
    case eDBFileMeta:
        w_info = &m_MetaWriting;
        break;
    case eDBFileData:
        w_info = &m_DataWriting;
        break;
    /*case eDBFileMoveMeta:
        w_info = &m_MetaMoving;
        break;
    case eDBFileMoveData:
        w_info = &m_DataMoving;
        break;*/
    default:
        abort();
    }

    Uint4 offset;
    CAbsTimeout timeout(m_MaxIOWaitTime);
    while ((offset = Uint4(w_info->next_coord)) > w_info->left_file_size - reserve_size
           &&  w_info->next_file == NULL)
    {
        m_NextWriteLock.Unlock();
        m_NextWaitLock.Lock();
        ++m_NextWaiters;
        while (w_info->next_file == NULL) {
            if (!m_NextWait.WaitForSignal(m_NextWaitLock, timeout)) {
                --m_NextWaiters;
                m_NextWaitLock.Unlock();
                return false;
            }
        }
        --m_NextWaiters;
        m_NextWaitLock.Unlock();
        m_NextWriteLock.Lock();
    }
    if (w_info->left_file_size < reserve_size) {
        m_CurDBSize += w_info->left_file_size;
        m_GarbageSize += w_info->left_file_size;
        w_info->cur_file->info_lock.Lock();
        w_info->cur_file->garb_size += w_info->left_file_size;
        if (w_info->cur_file->used_size
                + w_info->cur_file->garb_size >= w_info->cur_file->file_size)
        {
            abort();
        }
        w_info->cur_file->info_lock.Unlock();

        x_SwitchToNextFile(w_info);
        need_signal_switch = true;
    }
    coord = w_info->next_coord;
    offset = Uint4(coord);
    w_info->next_coord += true_rec_size;
    write_head = (SFileRecHeader*)(w_info->cur_file->file_map + offset);
    write_head->rec_num = w_info->next_rec_num++;
    write_head->rec_size = rec_size;

    CRef<SNCDBFileInfo> write_file = w_info->cur_file;
    SFileIndexRec* write_index = w_info->cur_file->index_head - write_head->rec_num;
    w_info->left_file_size -= reserve_size;
    m_CurDBSize += reserve_size;

    write_file->info_lock.Lock();

    write_file->used_size += reserve_size;
    write_index->offset = offset;
    write_index->dead_time = 0;
    write_index->next_idx = 0;
    SFileIndexRec* prev_index = write_file->index_head
                                    - write_file->index_head->prev_idx;
    if (prev_index->next_idx != 0)
        abort();
    write_index->prev_idx = write_file->index_head->prev_idx;
    prev_index->next_idx = write_head->rec_num;
    write_file->index_head->prev_idx = write_head->rec_num;

    write_file->info_lock.Unlock();
    m_NextWriteLock.Unlock();

    if (need_signal_switch)
        m_NextFileSwitch.SignalAll();

    return true;
}

inline SFileRecHeader*
CNCBlobStorage::x_GetRecordForCoord(SNCDBFileInfo* file_info, Uint8 coord)
{
    Uint4 offset = Uint4(coord);
    /*if (offset > file_info->file_size - sizeof(SFileRecHeader))
        return NULL;*/
    SFileRecHeader* rec_head = (SFileRecHeader*)(file_info->file_map + offset);
    /*if (offset > file_info->file_size - rec_head->rec_size)
        return NULL;*/

    return rec_head;
}

inline SFileRecHeader*
CNCBlobStorage::x_GetRecordForCoord(Uint8 coord)
{
    CRef<SNCDBFileInfo> file_info = x_GetFileForCoord(coord);
    if (!file_info)
        return NULL;
    return x_GetRecordForCoord(file_info, coord);
}

inline Uint1
CNCBlobStorage::x_CalcMapDepth(Uint8 size, Uint4 chunk_size, Uint2 map_size)
{
    Uint1 map_depth = 0;
    Uint8 cnt_chunks = (size + chunk_size - 1) / chunk_size;
    while (cnt_chunks > 1) {
        ++map_depth;
        cnt_chunks = (cnt_chunks + map_size - 1) / map_size;
    }
    return map_depth;
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(ver_data->coord != 0);
    SFileRecHeader* header = x_GetRecordForCoord(ver_data->coord);
    SFileMetaRec* meta_rec = (SFileMetaRec*)header;
    if (!meta_rec  ||  meta_rec->rec_type != eFileRecMeta) {
        ERR_POST(Critical << "Storage data is corrupted at coord " << ver_data->coord);
        //abort();
        // This shouldn't happen
        return false;
    }

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
    ver_data->data_coord = meta_rec->down_coord;
    ver_data->chunk_size = meta_rec->chunk_size;
    ver_data->map_size = meta_rec->map_size;

    ver_data->map_depth = x_CalcMapDepth(ver_data->size,
                                         ver_data->chunk_size,
                                         ver_data->map_size);
    return true;
}

bool
CNCBlobStorage::x_UpdateUpCoords(SFileChunkMapRec* map_rec, Uint8 coord)
{
    Uint1 cnt_downs = Uint1((map_rec->rec_size
                             - ((char*)map_rec->down_coords - (char*)map_rec))
                            / sizeof(map_rec->down_coords[0]));
    for (Uint1 i = 0; i < cnt_downs; ++i) {
        SFileRecHeader* header = x_GetRecordForCoord(map_rec->down_coords[i]);
        if (!header) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << map_rec->down_coords[i]);
            //abort();
            return false;
        }
        switch (header->rec_type) {
        case eFileRecChunkMap:
            ((SFileChunkMapRec*)header)->up_coord = coord;
            break;
        case eFileRecChunkData:
            ((SFileChunkDataRec*)header)->up_coord = coord;
            break;
        default:
            ERR_POST(Critical << "Storage data is corrupted at coord " << map_rec->down_coords[i]);
            //abort();
            return false;
        }
    }
    return true;
}

bool
CNCBlobStorage::x_SaveChunkMap(SNCBlobVerData* ver_data,
                               SNCChunkMapInfo** maps,
                               Uint2 cnt_chunks,
                               bool save_all_deps)
{
    SFileRecHeader* map_head = NULL;
    Uint8 coord = 0;
    SFileChunkMapRec* map_rec = (SFileChunkMapRec*)map_head;
    size_t coords_size = cnt_chunks * sizeof((*maps)->coords[0]);
    Uint4 rec_size = Uint4((char*)map_rec->down_coords - (char*)map_rec
                           + coords_size);
    if (!x_GetNextWriteCoord(eDBFileMeta, rec_size, coord, map_head))
        return false;

    map_rec = (SFileChunkMapRec*)map_head;
    map_rec->rec_type = eFileRecChunkMap;
    map_rec->map_idx = maps[0]->map_idx;
    map_rec->up_coord = 0;
    memcpy(map_rec->down_coords, maps[0]->coords, coords_size);

    maps[1]->coords[maps[0]->map_idx] = coord;
    ++maps[0]->map_idx;
    memset(maps[0]->coords, 0, ver_data->map_size * sizeof(maps[0]->coords[0]));
    if (!x_UpdateUpCoords(map_rec, coord))
        return false;

    Uint1 cur_level = 0;
    while (cur_level < kNCMaxBlobMapsDepth
           &&  (maps[cur_level]->map_idx == ver_data->map_size
                ||  (maps[cur_level]->map_idx > 1  &&  save_all_deps)))
    {
        cnt_chunks = maps[cur_level]->map_idx;
        ++cur_level;
        coords_size = cnt_chunks * sizeof((*maps)->coords[0]);
        rec_size = Uint4((char*)map_rec->down_coords - (char*)map_rec
                         + coords_size);
        if (!x_GetNextWriteCoord(eDBFileMeta, rec_size, coord, map_head))
            return false;

        map_rec = (SFileChunkMapRec*)map_head;
        map_rec->rec_type = eFileRecChunkMap;
        map_rec->map_idx = maps[cur_level]->map_idx;
        map_rec->up_coord = 0;
        memcpy(map_rec->down_coords, maps[cur_level]->coords, coords_size);

        maps[cur_level + 1]->coords[maps[cur_level]->map_idx] = coord;
        ++maps[cur_level]->map_idx;
        memset(maps[cur_level]->coords, 0,
               ver_data->map_size * sizeof(maps[cur_level]->coords[0]));
        if (!x_UpdateUpCoords(map_rec, coord))
            return false;
    }

    return true;
}

void
CNCBlobStorage::x_UpdateDeadTime(Uint8 coord, Uint1 map_depth, int dead_time)
{
    CRef<SNCDBFileInfo> file_info = x_GetFileForCoord(coord);
    SFileRecHeader* rec_head = (SFileRecHeader*)(file_info->file_map + Uint4(coord));
    SFileIndexRec* ind_rec = file_info->index_head - rec_head->rec_num;
    ind_rec->dead_time = dead_time;
    if (map_depth != 0) {
        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)rec_head;
        Uint2 cnt_chunks = Uint2((map_rec->rec_size
                                   - ((char*)map_rec->down_coords - (char*)map_rec))
                                 / sizeof(map_rec->down_coords[0]));
        for (Uint2 i = 0; i < cnt_chunks; ++i) {
            x_UpdateDeadTime(map_rec->down_coords[i], map_depth - 1, dead_time);
        }
    }
}

bool
CNCBlobStorage::WriteBlobInfo(const string& blob_key,
                              SNCBlobVerData* ver_data,
                              SNCChunkMapInfo** maps,
                              Uint8 cnt_chunks)
{
    Uint8 old_coord = ver_data->coord;
    Uint8 down_coord = ver_data->data_coord;
    if (old_coord == 0) {
        if (ver_data->size > ver_data->chunk_size) {
            Uint2 last_chunks_cnt = Uint2((cnt_chunks - 1) % ver_data->map_size) + 1;
            if (!x_SaveChunkMap(ver_data, maps, last_chunks_cnt, true))
                return false;

            for (Uint1 i = kNCMaxBlobMapsDepth; i > 0; --i) {
                if (maps[i]->coords[0] != 0) {
                    down_coord = maps[i]->coords[0];
                    maps[i]->coords[0] = 0;
                    ver_data->map_depth = i;
                    break;
                }
            }
            if (down_coord == 0)
                abort();
        }
        else {
            down_coord = maps[0]->coords[0];
            maps[0]->coords[0] = 0;
            ver_data->map_depth = 0;
        }
    }

    SFileRecHeader* meta_head = NULL;
    Uint8 coord = 0;
    SFileMetaRec* meta_rec = (SFileMetaRec*)meta_head;
    Uint2 key_size = Uint2(blob_key.size());
    if (!ver_data->password.empty())
        key_size += 16;
    Uint4 rec_size = Uint4((char*)meta_rec->key_data - (char*)meta_rec + key_size);
    if (!x_GetNextWriteCoord(eDBFileMeta, rec_size, coord, meta_head))
        return false;

    meta_rec = (SFileMetaRec*)meta_head;
    meta_rec->rec_type = eFileRecMeta;
    meta_rec->deleted = 0;
    meta_rec->slot = ver_data->slot;
    meta_rec->key_size = Uint2(blob_key.size());
    meta_rec->down_coord = down_coord;
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
    meta_rec->map_size = ver_data->map_size;
    meta_rec->chunk_size = ver_data->chunk_size;
    char* key_data = meta_rec->key_data;
    if (ver_data->password.empty()) {
        meta_rec->has_password = 0;
    }
    else {
        meta_rec->has_password = 1;
        memcpy(key_data, ver_data->password.data(), 16);
        key_data += 16;
        meta_rec->key_size += 16;
    }
    memcpy(key_data, blob_key.data(), blob_key.size());

    CRef<SNCDBFileInfo> file_info = x_GetFileForCoord(coord);
    SFileIndexRec* ind_rec = file_info->index_head - meta_rec->rec_num;
    ind_rec->dead_time = ver_data->dead_time;

    ver_data->coord = coord;
    ver_data->data_coord = down_coord;

    if (down_coord != 0) {
        SFileRecHeader* down_head = x_GetRecordForCoord(down_coord);
        if (!down_head) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << down_coord);
            //abort();
            // This shouldn't happen
            return false;
        }
        if (ver_data->map_depth == 0) {
            if (down_head->rec_type != eFileRecChunkData)
                abort();
            ((SFileChunkDataRec*)down_head)->up_coord = coord;
        }
        else {
            if (down_head->rec_type != eFileRecChunkMap)
                abort();
            ((SFileChunkMapRec*)down_head)->up_coord = coord;
        }
        x_UpdateDeadTime(down_coord, ver_data->map_depth, ver_data->dead_time);
    }

    if (old_coord != 0) {
        CRef<SNCDBFileInfo> old_file = x_GetFileForCoord(old_coord);
        if (!old_file) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << old_coord);
            //abort();
            // This shouldn't happen
            return false;
        }
        SFileRecHeader* old_head = x_GetRecordForCoord(old_file, old_coord);
        SFileMetaRec* old_rec = (SFileMetaRec*)old_head;
        if (!old_rec  ||  old_rec->rec_type != eFileRecMeta) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << old_coord);
            //abort();
            // This shouldn't happen
            return false;
        }
        old_rec->deleted = 1;
        x_MoveRecToGarbage(old_file, old_rec);
    }

    return true;
}

void
CNCBlobStorage::x_MoveDataToGarbage(Uint8 coord,
                                    Uint1 map_depth)
{
    CRef<SNCDBFileInfo> file_info = x_GetFileForCoord(coord);
    SFileRecHeader* rec_head = x_GetRecordForCoord(file_info, coord);
    if (!rec_head) {
        ERR_POST(Critical << "Storage data is corrupted at coord " << coord);
        //abort();
        // This shouldn't happen
        return;
    }
    if (map_depth == 0) {
        x_MoveRecToGarbage(file_info, rec_head);
    }
    else {
        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)rec_head;
        if (map_rec->rec_type != eFileRecChunkMap) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << coord);
            //abort();
            // This shouldn't happen
            return;
        }
        Uint2 cnt_chunks = Uint2((map_rec->rec_size
                                  - ((char*)map_rec->down_coords - (char*)map_rec))
                                 / sizeof(map_rec->down_coords[0]));
        for (Uint2 i = 0; i < cnt_chunks; ++i) {
            x_MoveDataToGarbage(map_rec->down_coords[i], map_depth - 1);
        }
        x_MoveRecToGarbage(file_info, map_rec);
    }
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data,
                               SNCChunkMapInfo** maps)
{
    if (ver_data->coord != 0) {
        CRef<SNCDBFileInfo> meta_file = x_GetFileForCoord(ver_data->coord);
        SFileRecHeader* meta_head = x_GetRecordForCoord(meta_file, ver_data->coord);
        SFileMetaRec* meta_rec = (SFileMetaRec*)meta_head;
        if (!meta_rec  ||  meta_rec->rec_type != eFileRecMeta) {
            ERR_POST(Critical << "Storage data is corrupted at coord " << ver_data->coord);
            //abort();
            // This shouldn't happen
            return;
        }
        meta_rec->deleted = 1;
        if (ver_data->size != 0) {
            x_MoveDataToGarbage(ver_data->data_coord, ver_data->map_depth);
        }
        x_MoveRecToGarbage(meta_file, meta_rec);
    }
    else if (maps != NULL) {
        for (Uint1 depth = 0; depth <= kNCMaxBlobMapsDepth; ++depth) {
            for (Uint2 idx = 0; maps[depth]->coords[idx] != 0; ++idx) {
                x_MoveDataToGarbage(maps[depth]->coords[idx], depth);
            }
        }
    }
}

bool
CNCBlobStorage::ReadChunkData(SNCBlobVerData* ver_data,
                              SNCChunkMapInfo** maps,
                              Uint8 chunk_num,
                              CNCBlobBuffer* buffer)
{
    Uint2 map_idx[kNCMaxBlobMapsDepth] = {0};
    Uint1 cur_index = 0;
    Uint8 level_num = chunk_num;
    do {
        map_idx[cur_index] = Uint2(level_num % ver_data->map_size);
        level_num /= ver_data->map_size;
        ++cur_index;
    }
    while (level_num != 0);

    for (Uint1 depth = ver_data->map_depth; depth > 0; ) {
        Uint2 this_index = map_idx[depth];
        --depth;
        if (maps[depth]->map_idx == this_index  &&  maps[depth]->coords[0] != 0)
            continue;

        Uint8 coord;
        if (depth + 1 == ver_data->map_depth)
            coord = ver_data->data_coord;
        else
            coord = maps[depth + 1]->coords[this_index];
        SFileRecHeader* map_head = x_GetRecordForCoord(coord);
        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)map_head;
        if (!map_rec  ||  map_rec->rec_type != eFileRecChunkMap
            ||  map_rec->map_idx != this_index)
        {
            ERR_POST(Critical << "Storage data is corrupted at coord " << coord);
            //abort();
            // This shouldn't happen
            return false;
        }
        maps[depth]->map_idx = this_index;
        memcpy(maps[depth]->coords, map_rec->down_coords,
               map_rec->rec_size - ((char*)map_rec->down_coords - (char*)map_rec));
        if (maps[depth]->coords[0] == 0)
            abort();

        while (depth > 0) {
            this_index = map_idx[depth];
            coord = maps[depth]->coords[this_index];
            --depth;
            map_head = x_GetRecordForCoord(coord);
            map_rec = (SFileChunkMapRec*)map_head;
            if (!map_rec  ||  map_rec->rec_type != eFileRecChunkMap
                ||  map_rec->map_idx != this_index)
            {
                ERR_POST(Critical << "Storage data is corrupted at coord " << coord);
                //abort();
                // This shouldn't happen
                return false;
            }
            maps[depth]->map_idx = this_index;
            memcpy(maps[depth]->coords, map_rec->down_coords,
                   map_rec->rec_size - ((char*)map_rec->down_coords - (char*)map_rec));
            if (maps[depth]->coords[0] == 0)
                abort();
        }
    }

    Uint8 chunk_coord;
    if (ver_data->map_depth == 0) {
        if (chunk_num != 0)
            abort();
        chunk_coord = ver_data->data_coord;
    }
    else
        chunk_coord = maps[0]->coords[map_idx[0]];

    SFileRecHeader* data_head = x_GetRecordForCoord(chunk_coord);
    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)data_head;
    if (!data_rec) {
        ERR_POST(Critical << "Storage data is corrupted at coord " << chunk_coord);
        //abort();
        return false;
    }
    if (data_rec->rec_type != eFileRecChunkData
        ||  data_rec->chunk_num != chunk_num)
    {
        ERR_POST(Critical << "Storage data is corrupted at coord " << chunk_coord);
        //abort();
        return false;
    }

    size_t data_size = (char*)data_rec + data_rec->rec_size
                        - (char*)data_rec->chunk_data;
    memcpy(buffer->GetData(), data_rec->chunk_data, data_size);
    buffer->Resize(data_size);
    CNCStat::AddChunkRead(data_size);

    return true;
}

bool
CNCBlobStorage::WriteChunkData(SNCBlobVerData* ver_data,
                               SNCChunkMapInfo** maps,
                               Uint8 chunk_num,
                               const CNCBlobBuffer* data)
{
    Uint2 map_idx[kNCMaxBlobMapsDepth] = {0};
    Uint1 cur_index = 0;
    Uint8 level_num = chunk_num;
    do {
        map_idx[cur_index] = Uint2(level_num % ver_data->map_size);
        level_num /= ver_data->map_size;
        ++cur_index;
    }
    while (level_num != 0);

    if (map_idx[1] != maps[0]->map_idx) {
        if (!x_SaveChunkMap(ver_data, maps, ver_data->map_size, false))
            return false;
        for (Uint1 i = 0; i < kNCMaxBlobMapsDepth - 1; ++i)
            maps[i]->map_idx = map_idx[i + 1];
    }

    SFileRecHeader* data_head = NULL;
    Uint8 coord = 0;
    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)data_head;
    Uint4 rec_size = Uint4((char*)data_rec->chunk_data - (char*)data_rec
                           + data->GetSize());
    if (!x_GetNextWriteCoord(eDBFileData, rec_size, coord, data_head))
        return false;

    data_rec = (SFileChunkDataRec*)data_head;
    data_rec->rec_type = eFileRecChunkData;
    data_rec->up_coord = 0;
    data_rec->chunk_num = chunk_num;
    data_rec->chunk_idx = map_idx[0];
    memcpy(data_rec->chunk_data, data->GetData(), data->GetSize());

    maps[0]->coords[map_idx[0]] = coord;
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
    m_DBFilesLock.Lock();
    Uint4 num_files = Uint4(m_DBFiles.size());
    m_DBFilesLock.Unlock();
    m_TimeTableLock.Lock();
    Uint8 cnt_blobs = m_CurBlobsCnt;
    int oldest_dead = (m_TimeTable.empty()? 0: m_TimeTable.begin()->dead_time);
    m_TimeTableLock.Unlock();
    //m_GarbageLock.Lock();
    Uint8 garbage = m_GarbageSize;
    //m_GarbageLock.Unlock();
    Uint8 db_size = m_CurDBSize;

    proxy << "Now on disk - "
                    << num_files << " files of "
                    << g_ToSizeStr(db_size) << " (garbage - "
                    << g_ToSizeStr(garbage) << ", "
                    << (db_size == 0? 0: garbage * 100 / db_size) << "%)" << endl
          << "Now in memory - "
                    << g_ToSmartStr(cnt_blobs) << " blobs" << endl
          << "Total moves - "
                    << g_ToSmartStr(m_CntMoveTasks) << " tasks ("
                    << m_CntFailedMoves << " failed), "
                    << g_ToSizeStr(m_MovedSize) << " in size" << endl;
    if (oldest_dead != 0) {
        proxy << "Dead times - ";
        CTime tmp(CTime::eEmpty, CTime::eLocal);
        tmp.SetTimeT(oldest_dead);
        proxy << tmp.AsString() << " (oldest)" << endl;
    }
}

void
CNCBlobStorage::x_CollectStorageStats(void)
{
    m_DBFilesLock.Lock();
    Uint4 num_files = Uint4(m_DBFiles.size());
    m_DBFilesLock.Unlock();
    //m_GarbageLock.Lock();
    Uint8 garbage = m_GarbageSize;
    //m_GarbageLock.Unlock();
    Uint8 db_size = m_CurDBSize;
    Uint8 cnt_blobs = m_CurBlobsCnt;

    CNCStat::AddDBMeasurement(num_files, db_size, garbage, cnt_blobs);
}

void
CNCBlobStorage::OnBlockedOpFinish(void)
{
    m_GCBlockWaiter.SignalAll();
}

bool
CNCBlobStorage::x_CacheMapRecs(Uint8 map_coord,
                               Uint1 map_depth,
                               Uint8 up_coord,
                               Uint4 chunk_size,
                               Uint4 last_chunk_size,
                               map<Uint4, Uint4>& sizes_map,
                               TFileRecsMap& recs_map)
{
    Uint4 file_id = Uint4(map_coord >> 32);
    Uint4 offset = Uint4(map_coord);
    TNCDBFilesMap::const_iterator it = m_DBFiles.find(file_id);
    if (it == m_DBFiles.end())
        return false;

    CRef<SNCDBFileInfo> file_info = it->second;
    if (offset > file_info->file_size - sizeof(SFileRecHeader))
        return false;
    SFileRecHeader* rec_head = (SFileRecHeader*)(file_info->file_map + offset);

    if (map_depth == 0) {
        TOffToNumMap& off_map = recs_map[file_id];
        TOffToNumMap::iterator it = off_map.find(offset);
        if (it == off_map.end())
            return false;
        off_map.erase(it);

        SFileChunkDataRec* data_rec = (SFileChunkDataRec*)rec_head;
        if (data_rec->up_coord != up_coord)
            data_rec->up_coord = up_coord;

        Uint4 rec_size = last_chunk_size
                         + Uint4((char*)data_rec->chunk_data - (char*)data_rec)
                         + sizeof(SFileIndexRec);
        if (rec_size & 7)
            rec_size += 8 - (rec_size & 7);
        sizes_map[file_id] += rec_size;
    }
    else {
        SFileChunkMapRec* map_rec = (SFileChunkMapRec*)rec_head;
        //if (offset > file_info->file_size - map_rec->rec_size)
        //    return false;
        if (map_rec->up_coord != up_coord)
            map_rec->up_coord = up_coord;

        Uint2 cnt_chunks = Uint2((map_rec->rec_size
                                  - ((char*)map_rec->down_coords - (char*)map_rec))
                                 / sizeof(map_rec->down_coords[0]));
        if (cnt_chunks > kNCMaxChunksInMap)
            return false;

        --cnt_chunks;
        for (Uint2 i = 0; i < cnt_chunks; ++i) {
            if (!x_CacheMapRecs(map_rec->down_coords[i], map_depth - 1, map_coord,
                                chunk_size, chunk_size, sizes_map, recs_map))
            {
                return false;
            }
        }
        if (!x_CacheMapRecs(map_rec->down_coords[cnt_chunks], map_depth - 1, map_coord,
                            chunk_size, last_chunk_size, sizes_map, recs_map))
        {
            return false;
        }
        sizes_map[file_id] += map_rec->rec_size + sizeof(SFileIndexRec);
    }

    return true;
}

bool
CNCBlobStorage::x_CacheMetaRec(SFileRecHeader* header,
                               SNCDBFileInfo* file_info,
                               size_t offset,
                               int cur_time,
                               TFileRecsMap& recs_map)
{
    SFileMetaRec* meta_rec = (SFileMetaRec*)header;
    if (meta_rec->dead_time <= cur_time)
        meta_rec->deleted = 1;
    if (meta_rec->deleted)
        return false;

    Uint8 coord = (Uint8(file_info->file_id) << 32) + Uint4(offset);
    if (meta_rec->size != 0) {
        Uint1 map_depth = x_CalcMapDepth(meta_rec->size,
                                         meta_rec->chunk_size,
                                         meta_rec->map_size);
        Uint4 last_chunk_size = Uint4(meta_rec->size % meta_rec->chunk_size);
        if (last_chunk_size == 0)
            last_chunk_size = meta_rec->chunk_size;
        typedef map<Uint4, Uint4> TSizesMap;
        TSizesMap sizes_map;
        if (!x_CacheMapRecs(meta_rec->down_coord, map_depth, coord,
                            meta_rec->chunk_size, last_chunk_size,
                            sizes_map, recs_map))
        {
            meta_rec->deleted = 1;
            return false;
        }
        ITERATE(TSizesMap, it, sizes_map) {
            SNCDBFileInfo* info = m_DBFiles[it->first];
            info->used_size += it->second;
            if (info->garb_size < it->second)
                abort();
            info->garb_size -= it->second;
            m_GarbageSize -= it->second;
        }
    }
    Uint4 rec_size = meta_rec->rec_size + sizeof(SFileIndexRec);
    if (rec_size & 7)
        rec_size += 8 - (rec_size & 7);
    file_info->used_size += rec_size;
    if (file_info->garb_size < rec_size)
        abort();
    file_info->garb_size -= rec_size;
    m_GarbageSize -= rec_size;

    char* key_data = meta_rec->key_data;
    Uint2 key_size = meta_rec->key_size;
    if (meta_rec->has_password) {
        key_data += 16;
        key_size -= 16;
    }
    string key(key_data, key_size);

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
        SNCDBFileInfo* old_file = m_DBFiles[Uint4(old_data->coord >> 32)];
        SFileMetaRec* old_rec = (SFileMetaRec*)x_GetRecordForCoord(old_file, old_data->coord);
        if (!old_rec  ||  old_rec->rec_type != eFileRecMeta)
            abort();
        old_rec->deleted = 1;
        m_GarbageSize += old_rec->rec_size;
        x_MoveRecToGarbage(old_file, old_rec);
        if (old_rec->size != 0  &&  old_rec->down_coord != meta_rec->down_coord)
        {
            Uint1 map_depth = x_CalcMapDepth(old_rec->size,
                                             old_rec->chunk_size,
                                             old_rec->map_size);
            x_MoveDataToGarbage(old_rec->down_coord, map_depth);
        }
        delete old_data;
    }
    m_TimeTable.insert_equal(*cache_data);
    ++m_CurBlobsCnt;

    return true;
}

void
CNCBlobStorage::x_CacheDatabase(void)
{
    CRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    CDiagContext::SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart().Print("_type", "caching");
        ctx->SetRequestStatus(CNCMessageHandler::eStatus_OK);
    }

    TFileRecsMap recs_map;
    int cur_time = int(time(NULL));
    ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        CRef<SNCDBFileInfo> file_info = it_file->second;
        m_CurDBSize += file_info->file_size;
        file_info->garb_size += file_info->file_size;
        m_GarbageSize += file_info->file_size;
        if (file_info->file_type == eDBFileMeta)
            continue;
        TOffToNumMap& off_map = recs_map[file_info->file_id];
        SFileIndexRec* ind_rec = file_info->index_head;
        while (ind_rec->next_idx != 0) {
            Uint4 rec_idx = ind_rec->next_idx;
            ind_rec = file_info->index_head - rec_idx;
            off_map[ind_rec->offset] = rec_idx;
        }
    }

    set<Uint4> new_file_ids;
    while (!m_Stopped  &&  new_file_ids.size() != 4/*8*/) {
        if (new_file_ids.size() == 0) {
            if (!x_CreateNewFile(eDBFileMeta))
                goto del_file_and_retry;
            new_file_ids.insert(m_MetaWriting.next_file->file_id);
        }
        if (new_file_ids.size() == 1) {
            if (!x_CreateNewFile(eDBFileData))
                goto del_file_and_retry;
            new_file_ids.insert(m_DataWriting.next_file->file_id);
        }
        /*if (new_file_ids.size() == 2) {
            if (!x_CreateNewFile(eDBFileMoveMeta))
                goto del_file_and_retry;
            new_file_ids.insert(m_MetaMoving.next_file->file_id);
        }
        if (new_file_ids.size() == 3) {
            if (!x_CreateNewFile(eDBFileMoveData))
                goto del_file_and_retry;
            new_file_ids.insert(m_DataMoving.next_file->file_id);
        }*/
        if (new_file_ids.size() == 2/*4*/) {
            if (m_MetaWriting.next_file) {
                x_SwitchToNextFile(&m_MetaWriting);
                x_SwitchToNextFile(&m_DataWriting);
                //x_SwitchToNextFile(&m_MetaMoving);
                //x_SwitchToNextFile(&m_DataMoving);
            }
            if (!x_CreateNewFile(eDBFileMeta))
                goto del_file_and_retry;
            new_file_ids.insert(m_MetaWriting.next_file->file_id);
        }
        if (new_file_ids.size() == 3/*5*/) {
            if (!x_CreateNewFile(eDBFileData))
                goto del_file_and_retry;
            new_file_ids.insert(m_DataWriting.next_file->file_id);
        }
        /*if (new_file_ids.size() == 6) {
            if (!x_CreateNewFile(eDBFileMoveMeta))
                goto del_file_and_retry;
            new_file_ids.insert(m_MetaMoving.next_file->file_id);
        }
        if (new_file_ids.size() == 7) {
            if (!x_CreateNewFile(eDBFileMoveData))
                goto del_file_and_retry;
            new_file_ids.insert(m_DataMoving.next_file->file_id);
        }*/
        break;

del_file_and_retry:
        if (m_DBFiles.empty()) {
            ctx->SetRequestStatus(CNCMessageHandler::eStatus_ServerError);
            g_NetcacheServer->RequestShutdown();
            goto clean_and_exit;
        }
        CRef<SNCDBFileInfo> file_to_del;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CRef<SNCDBFileInfo> file_info = it->second;
            if (file_info->file_type == eDBFileData) {
                file_to_del = file_info;
                break;
            }
        }
        if (!file_to_del)
            file_to_del = m_DBFiles.begin()->second;
        x_DeleteDBFile(file_to_del);
    }

    ERASE_ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        if (m_Stopped)
            break;

        SNCDBFileInfo* file_info = it_file->second;
        if (file_info->file_type != eDBFileMeta
            ||  new_file_ids.find(file_info->file_id) != new_file_ids.end())
        {
            continue;
        }

        SFileIndexRec* ind_rec = file_info->index_head;
        while (ind_rec->next_idx != 0  &&  !m_Stopped) {
            Uint4 rec_num = ind_rec->next_idx;
            ind_rec = file_info->index_head - rec_num;
            if (ind_rec->dead_time <= cur_time) {
                ind_rec = x_DeleteIndexRec(file_info->index_head, rec_num);
                continue;
            }
            SFileRecHeader* header = (SFileRecHeader*)(file_info->file_map
                                                       + ind_rec->offset);
            if (header->rec_num == 0  &&  header->rec_type == eFileRecNone)
                continue;
            if (header->rec_num != rec_num) {
header_error:
                ERR_POST(Critical << "Incorrect data met in "
                                  << file_info->file_name << ", offset "
                                  << ind_rec->offset << ", rec_num "
                                  << header->rec_num << ", next_rec_num "
                                  << rec_num);
                continue;
            }
            switch (header->rec_type) {
            case eFileRecMeta:
                if (!x_CacheMetaRec(header, file_info, ind_rec->offset,
                                    cur_time, recs_map))
                {
                    ind_rec = x_DeleteIndexRec(file_info->index_head, rec_num);
                }
                break;
            case eFileRecChunkMap:
                break;
            case eFileRecChunkData:
                ind_rec = x_DeleteIndexRec(file_info->index_head, rec_num);
                break;
            default:
                goto header_error;
            }
        }
    }

    ITERATE(TFileRecsMap, it_id, recs_map) {
        Uint4 file_id = it_id->first;
        const TOffToNumMap& off_map = it_id->second;
        SNCDBFileInfo* file_info = m_DBFiles[file_id];
        ITERATE(TOffToNumMap, it_off, off_map) {
            Uint4 rec_num = it_off->second;
            x_DeleteIndexRec(file_info->index_head, rec_num);
        }
    }

    CNetCacheServer::CachingCompleted();

clean_and_exit:
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

    if (m_IsStopWrite == eNoStop) {
        if (m_StopWriteOnSize != 0  &&  m_CurDBSize >= m_StopWriteOnSize) {
            m_IsStopWrite = eStopDBSize;
            ERR_POST(Critical << "Database size exceeded limit. "
                                 "Will no longer accept any writes from clients.");
        }
    }
    else if (m_IsStopWrite ==  eStopDBSize  &&  m_CurDBSize <= m_StopWriteOffSize)
    {
        m_IsStopWrite = eNoStop;
    }
    try {
        Uint8 free_space = CFileUtil::GetFreeDiskSpace(m_Path);
        if (free_space <= m_DiskCritical) {
            m_IsStopWrite = eStopDiskCritical;
            ERR_POST(Critical << "Free disk space is below CRITICAL threshold. "
                                 "Will no longer accept any writes.");
        }
        else if (free_space <= m_DiskFreeLimit) {
            m_IsStopWrite = eStopDiskSpace;
            ERR_POST(Critical << "Free disk space is below threshold. "
                                 "Will no longer accept any writes from clients.");
        }
        else if (m_IsStopWrite == eStopDiskSpace
                 ||  m_IsStopWrite == eStopDiskCritical)
        {
            m_IsStopWrite = eNoStop;
        }
    }
    catch (CFileErrnoException& ex) {
        ERR_POST(Critical << "Cannot read free disk space: " << ex);
    }
}

void
CNCBlobStorage::x_GC_DeleteExpired(const string& key,
                                   Uint2 slot,
                                   int dead_before)
{
    m_GCAccessor->Prepare(key, kEmptyStr, slot, eNCGCDelete);
    x_InitializeAccessor(m_GCAccessor);
    m_GCBlockLock.Lock();
    while (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock) {
        m_GCBlockWaiter.WaitForSignal(m_GCBlockLock);
    }
    m_GCBlockLock.Unlock();
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

inline void
CNCBlobStorage::x_ReadRecDeadFromMeta(Uint8 /* coord */,
                                      SFileRecHeader* header,
                                      SFileRecHeader*& /* up_head */,
                                      SFileMetaRec*& meta_rec)
{
    meta_rec = (SFileMetaRec*)header;
}

void
CNCBlobStorage::x_ReadRecDeadFromMap(Uint8 coord,
                                     SFileRecHeader* header,
                                     Uint1 max_map_depth,
                                     SFileRecHeader*& up_head,
                                     SFileMetaRec*& meta_rec)
{
    SFileChunkMapRec* map_rec = (SFileChunkMapRec*)header;
    if (map_rec->up_coord == 0)
        return;
    up_head = x_GetRecordForCoord(map_rec->up_coord);
    if (!up_head)
        return;

    SFileRecHeader* dummy_up = NULL;
    if (up_head->rec_type == eFileRecMeta) {
        x_ReadRecDeadFromMeta(map_rec->up_coord, up_head, dummy_up, meta_rec);
        if (meta_rec->down_coord != coord) {
            map_rec->up_coord = 0;
            meta_rec = NULL;
        }
    }
    else if (max_map_depth > 1  &&  up_head->rec_type == eFileRecChunkMap) {
        SFileChunkMapRec* up_map = (SFileChunkMapRec*)up_head;
        if (up_map->down_coords[map_rec->map_idx] == coord) {
            x_ReadRecDeadFromMap(map_rec->up_coord, up_head,
                                 max_map_depth - 1, dummy_up, meta_rec);
        }
        else {
            map_rec->up_coord = 0;
        }
    }
    else {
        ERR_POST(Critical << "Storage is corrupted. Record will be deleted.");
        //abort();
        up_head = NULL;
    }
}

void
CNCBlobStorage::x_ReadRecDeadFromData(Uint8 coord,
                                      SFileRecHeader* header,
                                      Uint1 max_map_depth,
                                      SFileRecHeader*& up_head,
                                      SFileMetaRec*& meta_rec)
{
    SFileChunkDataRec* data_rec = (SFileChunkDataRec*)header;
    if (data_rec->up_coord == 0)
        return;
    up_head = x_GetRecordForCoord(data_rec->up_coord);
    if (!up_head)
        return;

    SFileRecHeader* dummy_up = NULL;
    if (up_head->rec_type == eFileRecMeta) {
        x_ReadRecDeadFromMeta(data_rec->up_coord, up_head, dummy_up, meta_rec);
        if (meta_rec->down_coord != coord) {
            data_rec->up_coord = 0;
            meta_rec = NULL;
        }
    }
    else if (up_head->rec_type == eFileRecChunkMap) {
        SFileChunkMapRec* up_map = (SFileChunkMapRec*)up_head;
        if (up_map->down_coords[data_rec->chunk_idx] == coord) {
            x_ReadRecDeadFromMap(data_rec->up_coord, up_head,
                                 max_map_depth, dummy_up, meta_rec);
        }
        else {
            data_rec->up_coord = 0;
        }
    }
    else {
        ERR_POST(Critical << "Storage is corrupted. Record will be deleted.");
        //abort();
        up_head = NULL;
    }
}

void
CNCBlobStorage::x_ReadRecDeadTime(Uint8 coord,
                                  SFileRecHeader* header,
                                  Uint1 max_map_depth,
                                  SFileRecHeader*& up_head,
                                  SFileMetaRec*& meta_rec)
{
    if (header->rec_type == eFileRecMeta)
        x_ReadRecDeadFromMeta(coord, header, up_head, meta_rec);
    else if (header->rec_type == eFileRecChunkMap)
        x_ReadRecDeadFromMap(coord, header, max_map_depth, up_head, meta_rec);
    else if (header->rec_type == eFileRecChunkData)
        x_ReadRecDeadFromData(coord, header, max_map_depth, up_head, meta_rec);
    else  if (header->rec_type != eFileRecNone) {
        ERR_POST(Critical << "Storage is corrupted. Record will be deleted.");
        //abort();
    }
}

bool
CNCBlobStorage::x_MoveRecord(ENCDBFileType file_type,
                             SFileRecHeader* header,
                             SFileRecHeader* up_head,
                             SFileMetaRec* meta_rec,
                             bool& move_done)
{
    bool result = true;
    Uint8 new_coord = 0;
    SFileRecHeader* new_head = NULL;
    if (!x_GetNextWriteCoord(file_type, //ENCDBFileType(file_type | fDBFileForMove),
                             header->rec_size, new_coord, new_head))
    {
        move_done = false;
        return false;
    }
    CRef<SNCDBFileInfo> new_file = x_GetFileForCoord(new_coord);

    char* key_data = meta_rec->key_data;
    Uint2 key_size = meta_rec->key_size;
    if (meta_rec->has_password) {
        key_data += 16;
        key_size -= 16;
    }
    string key(key_data, key_size);
    SNCCacheData* key_cache = x_GetKeyCacheData(meta_rec->slot, key, false);
    if (!key_cache) {
        if (!meta_rec->deleted) {
            abort();
            // TODO: delete blob
        }
        goto wipe_new_mem;
    }

    key_cache->lock.Lock();
    if (key_cache->ver_mgr) {
        result = false;
        goto unlock_and_wipe;
    }
    if (meta_rec->deleted)
        goto unlock_and_wipe;

    Uint4 tmp_rec_num;
    tmp_rec_num = new_head->rec_num;
    memcpy(new_head, header, header->rec_size);
    new_head->rec_num = tmp_rec_num;

    SFileIndexRec* ind_rec;
    ind_rec = new_file->index_head - new_head->rec_num;
    ind_rec->dead_time = meta_rec->dead_time;

    SFileChunkMapRec* map_rec;
    SFileChunkDataRec* data_rec;
    switch (header->rec_type) {
    case eFileRecMeta:
        key_cache->coord = new_coord;
        if (meta_rec->size != 0) {
            SFileRecHeader* down_head = x_GetRecordForCoord(meta_rec->down_coord);
            if (down_head) {
                if (down_head->rec_type == eFileRecChunkData) {
                    SFileChunkDataRec* down_data = (SFileChunkDataRec*)down_head;
                    down_data->up_coord = new_coord;
                }
                else {
                    SFileChunkMapRec* down_map = (SFileChunkMapRec*)down_head;
                    down_map->up_coord = new_coord;
                }
            }
        }
        meta_rec->deleted = 1;
        break;
    case eFileRecChunkMap:
        map_rec = (SFileChunkMapRec*)header;
        if (up_head == meta_rec)
            meta_rec->down_coord = new_coord;
        else {
            SFileChunkMapRec* up_map = (SFileChunkMapRec*)up_head;
            up_map->down_coords[map_rec->map_idx] = new_coord;
        }
        x_UpdateUpCoords(map_rec, new_coord);
        map_rec->up_coord = 0;
        break;
    case eFileRecChunkData:
        data_rec = (SFileChunkDataRec*)header;
        if (up_head == meta_rec)
            meta_rec->down_coord = new_coord;
        else {
            SFileChunkMapRec* up_map = (SFileChunkMapRec*)up_head;
            up_map->down_coords[data_rec->chunk_idx] = new_coord;
        }
        data_rec->up_coord = 0;
        break;
    }

    key_cache->lock.Unlock();
    move_done = true;
    return true;

unlock_and_wipe:
    key_cache->lock.Unlock();
wipe_new_mem:
    SFileChunkDataRec* new_chunk = (SFileChunkDataRec*)new_head;
    new_chunk->rec_type = eFileRecChunkData;
    new_chunk->up_coord = 0;
    x_MoveRecToGarbage(new_file, new_chunk);
    move_done = false;

    return result;
}

bool
CNCBlobStorage::x_ShrinkDiskStorage(void)
{
    if (m_Stopped)
        return false;

    int cur_time = int(time(NULL));
    bool need_move = m_CurDBSize >= m_MinDBSize
                     &&  m_GarbageSize * 100 > m_CurDBSize * m_MaxGarbagePct;
    double max_pct = 0;
    double total_pct = double(m_GarbageSize) / m_CurDBSize;
    CRef<SNCDBFileInfo> max_file;
    vector<CRef<SNCDBFileInfo> > files_to_del;

    m_DBFilesLock.Lock();
    ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        CRef<SNCDBFileInfo> this_file = it_file->second;
        m_NextWriteLock.Lock();
        bool is_current = this_file == m_MetaWriting.cur_file
                          ||  this_file == m_DataWriting.cur_file
                          ||  this_file == m_MetaWriting.next_file
                          ||  this_file == m_DataWriting.next_file
                          /*||  this_file == m_MetaMoving.cur_file
                          ||  this_file == m_DataMoving.cur_file
                          ||  this_file == m_MetaMoving.next_file
                          ||  this_file == m_DataMoving.next_file*/;
        m_NextWriteLock.Unlock();
        if (is_current)
            continue;

        if (this_file->used_size == 0) {
            files_to_del.push_back(this_file);
        }
        else if (need_move) {
            if (cur_time - this_file->last_shrink_time > m_MinMoveLife) {
                this_file->info_lock.Lock();
                if (this_file->garb_size + this_file->used_size != 0) {
                    double this_pct = double(this_file->garb_size)
                                      / (this_file->garb_size + this_file->used_size);
                    if (this_pct > max_pct) {
                        max_pct = this_pct;
                        max_file = this_file;
                    }
                }
                this_file->info_lock.Unlock();
            }
        }
    }
    m_DBFilesLock.Unlock();

    NON_CONST_ITERATE(vector<CRef<SNCDBFileInfo> >, it, files_to_del) {
        x_DeleteDBFile(*it);
    }
    files_to_del.clear();

    if (need_move) {
        need_move = m_CurDBSize >= m_MinDBSize
                    &&  m_GarbageSize * 100 > m_CurDBSize * m_MaxGarbagePct;
    }
    if (max_file == NULL  ||  !need_move  ||  max_pct < total_pct)
        return false;

    bool failed = false;
    Uint4 cnt_moved = 0;
    while (!m_Stopped) {
        max_file->info_lock.Lock();
        SFileIndexRec* ind_rec = max_file->index_head;
        do {
            if (ind_rec->next_idx == 0) {
                if (max_file->index_head->next_idx == 0  &&  max_file->used_size != 0)
                    abort();
                ind_rec = NULL;
                break;
            }
            ind_rec = max_file->index_head - ind_rec->next_idx;
        }
        while (ind_rec->dead_time < cur_time + m_MinMoveLife);
        max_file->info_lock.Unlock();
        if (!ind_rec)
            break;

        Uint8 coord = (Uint8(max_file->file_id) << 32) + Uint4(ind_rec->offset);
        SFileRecHeader* header = (SFileRecHeader*)(max_file->file_map
                                                        + ind_rec->offset);
        SFileRecHeader* up_head = NULL;
        SFileMetaRec* meta_rec = NULL;
        x_ReadRecDeadTime(coord, header, kNCMaxBlobMapsDepth, up_head, meta_rec);

        if (meta_rec  &&  !meta_rec->deleted
            &&  meta_rec->dead_time > cur_time + m_MinMoveLife)
        {
            bool move_done = false;
            if (!x_MoveRecord(max_file->file_type,
                              header, up_head, meta_rec, move_done))
            {
                ++m_CntFailedMoves;
                failed = true;
                break;
            }
            if (move_done) {
                ++cnt_moved;
                ++m_CntMoveTasks;
                m_MovedSize += header->rec_size;
                x_MoveRecToGarbage(max_file, header);
            }
        }
    }
    if (!failed) {
        if (max_file->used_size == 0)
            x_DeleteDBFile(max_file);
        else if (cnt_moved == 0)
            max_file->last_shrink_time = cur_time;
    }

    return true;
}

void
CNCBlobStorage::x_SaveLogRecNo(void)
{
    int cur_time = int(time(NULL));
    if (!m_NeedSaveLogRecNo
        ||  cur_time < m_LastRecNoSaveTime + m_MinRecNoSavePeriod)
    {
        return;
    }

    Uint8 log_rec_no = CNCSyncLog::GetLastRecNo();
    m_IndexLock.Lock();
    try {
        m_IndexDB->SetMaxSyncLogRecNo(log_rec_no);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST(Critical << "Cannot save sync log record number: " << ex);
    }
    m_IndexLock.Unlock();

    m_LastRecNoSaveTime = cur_time;
}

void
CNCBlobStorage::x_FlushStorage(void)
{
    int cur_time = int(time(NULL));
    if (!m_FlushTimePeriod  ||  cur_time < m_LastFlushTime + m_FlushTimePeriod)
        return;

    Uint4 last_id = 0;
    for (;;) {
        m_DBFilesLock.Lock();
        TNCDBFilesMap::iterator file_it = m_DBFiles.upper_bound(last_id);
        if (file_it == m_DBFiles.end()) {
            m_DBFilesLock.Unlock();
            break;
        }
        CRef<SNCDBFileInfo> file_info = file_it->second;
        m_DBFilesLock.Unlock();

#ifdef NCBI_OS_LINUX
        if (msync(file_info->file_map, file_info->file_size, MS_SYNC)) {
            ERR_POST(Critical << "Unable to sync file" << file_info->file_name
                              << ", errno=" << errno);
        }
#endif

        last_id = file_info->file_id;
    }

    m_LastFlushTime = cur_time;
}

void
CNCBlobStorage::x_DoBackgroundWork(void)
{
    x_CacheDatabase();

    m_NewFileThread = NewBGThread(this, &CNCBlobStorage::x_DoNewFileWork);
    try {
        m_NewFileThread->Run();
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
        m_NewFileThread.Reset();
        g_NetcacheServer->RequestShutdown();
        return;
    }
    m_GCThread = NewBGThread(this, &CNCBlobStorage::x_DoGCWork);
    try {
        m_GCThread->Run();
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
        m_GCThread.Reset();
        g_NetcacheServer->RequestShutdown();
        return;
    }
    m_FlushThread = NewBGThread(this, &CNCBlobStorage::x_DoFlushWork);
    try {
        m_FlushThread->Run();
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
        m_FlushThread.Reset();
        g_NetcacheServer->RequestShutdown();
        return;
    }

    while (!m_Stopped) {
        Uint8 start_time = CNetCacheServer::GetPreciseTime();

        x_HeartBeat();
        x_CollectStorageStats();
        x_ShrinkDiskStorage();

        Uint8 end_time = CNetCacheServer::GetPreciseTime();
        if (end_time - start_time < kNCTimeTicksInSec) {
            m_StopLock.Lock();
            if (!m_Stopped)
                m_StopCond.WaitForSignal(m_StopLock, CTimeout(1, 0));
            m_StopLock.Unlock();
        }
    }
}

void
CNCBlobStorage::x_DoFlushWork(void)
{
    while (!m_Stopped) {
        int start_time = int(time(NULL));
        x_FlushStorage();
        x_SaveLogRecNo();
        int end_time = int(time(NULL));

        int to_wait = m_FlushTimePeriod;
        if (!to_wait)
            to_wait = m_MinRecNoSavePeriod;
        if (end_time - start_time < to_wait * kNCTimeTicksInSec) {
            m_StopLock.Lock();
            if (!m_Stopped)
                m_StopCond.WaitForSignal(m_StopLock, CTimeout(to_wait, 0));
            m_StopLock.Unlock();
        }
    }
}

void
CNCBlobStorage::x_DoGCWork(void)
{
    while (!m_Stopped) {
        Uint8 start_time = CNetCacheServer::GetPreciseTime();
        x_RunGC();
        Uint8 end_time = CNetCacheServer::GetPreciseTime();
        if (end_time - start_time < kNCTimeTicksInSec) {
            m_StopLock.Lock();
            if (!m_Stopped)
                m_StopCond.WaitForSignal(m_StopLock, CTimeout(1, 0));
            m_StopLock.Unlock();
        }
    }
}

void
CNCBlobStorage::x_DoNewFileWork(void)
{
    while (!m_Stopped) {
        if (m_MetaWriting.next_file == NULL)
            x_CreateNewFile(eDBFileMeta);
        if (m_DataWriting.next_file == NULL)
            x_CreateNewFile(eDBFileData);
        /*if (m_MetaMoving.next_file == NULL)
            x_CreateNewFile(eDBFileMoveMeta);
        if (m_DataMoving.next_file == NULL)
            x_CreateNewFile(eDBFileMoveData);*/
        m_NextWaitLock.Lock();
        if (m_NextWaiters != 0)
            m_NextWait.SignalAll();
        m_NextWaitLock.Unlock();

        m_NextFileSwitchLock.Lock();
        if (!m_Stopped
            &&  m_MetaWriting.next_file != NULL
            &&  m_DataWriting.next_file != NULL
            /*&&  m_MetaMoving.next_file != NULL
            &&  m_DataMoving.next_file != NULL*/)
        {
            m_NextFileSwitch.WaitForSignal(m_NextFileSwitchLock);
        }
        m_NextFileSwitchLock.Unlock();
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
