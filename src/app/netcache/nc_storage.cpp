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

#include "nc_pch.hpp"

#include <corelib/ncbireg.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_process.hpp>

#include "nc_storage.hpp"
#include "storage_types.hpp"
#include "nc_db_files.hpp"
#include "distribution_conf.hpp"
#include "netcached.hpp"
#include "nc_storage_blob.hpp"
#include "sync_log.hpp"
#include "nc_stat.hpp"
#include "logging.hpp"


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
static const char* kNCStorage_MapsFileSuffix  = "";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_RegSection        = "storage";
static const char* kNCStorage_PathParam         = "path";
static const char* kNCStorage_FilePrefixParam   = "prefix";
static const char* kNCStorage_GuardNameParam    = "guard_file_name";
static const char* kNCStorage_FileSizeParam     = "each_file_size";
static const char* kNCStorage_GarbagePctParam   = "max_garbage_pct";
static const char* kNCStorage_MinDBSizeParam    = "min_storage_size";
static const char* kNCStorage_MoveLifeParam     = "min_lifetime_to_move";
static const char* kNCStorage_FailedMoveParam   = "failed_move_delay";
static const char* kNCStorage_GCBatchParam      = "gc_batch_size";
static const char* kNCStorage_FlushTimeParam    = "sync_time_period";
static const char* kNCStorage_ExtraGCOnParam    = "db_limit_del_old_on";
static const char* kNCStorage_ExtraGCOffParam   = "db_limit_del_old_off";
static const char* kNCStorage_StopWriteOnParam  = "db_limit_stop_write_on";
static const char* kNCStorage_StopWriteOffParam = "db_limit_stop_write_off";
static const char* kNCStorage_MinFreeDiskParam  = "disk_free_limit";
static const char* kNCStorage_DiskCriticalParam = "critical_disk_free_limit";
static const char* kNCStorage_MinRecNoSaveParam = "min_rec_no_save_period";


// storage file type signatures
static const Uint8 kMetaSignature = NCBI_CONST_UINT8(0xeed5be66cdafbfa3);
static const Uint8 kDataSignature = NCBI_CONST_UINT8(0xaf9bedf24cfa05ed);
static const Uint8 kMapsSignature = NCBI_CONST_UINT8(0xba6efd7b61fdff6c);
static const Uint1 kSignatureSize = sizeof(kMetaSignature);


/// Size of memory page that is a granularity of all allocations from OS.
static const size_t kMemPageSize  = 4 * 1024;
/// Mask that can move pointer address or memory size to the memory page
/// boundary.
static const size_t kMemPageAlignMask = ~(kMemPageSize - 1);


enum EStopCause {
    eNoStop,
    eStopWarning,
    eStopDBSize,
    eStopDiskSpace,
    eStopDiskCritical
};

struct SBucketCache
{
    CMiniMutex   lock;
    TKeyMap      key_map;
};
typedef map<Uint2, SBucketCache*> TBucketCacheMap;

struct STimeTable
{
    CMiniMutex lock;
    TTimeTableMap time_map;
};
typedef map<Uint2, STimeTable*> TTimeBuckets;



/// Directory for all database files of the storage
static string s_Path;
/// Name of the storage
static string s_Prefix;
/// Number of blobs treated by GC and by caching mechanism in one batch
static Uint2 s_GCBatchSize;
static int s_FlushTimePeriod;
static Uint1 s_MaxGarbagePct;
static int s_MinMoveLife;
static int s_FailedMoveDelay;
static Uint8 s_MinDBSize;
/// Name of guard file excluding several instances to run on the same
/// database.
static string s_GuardName;
/// Lock for guard file representing this instance running.
/// It will hold exclusive lock all the time while NetCache is
/// working, so that other instance of NetCache on the same database will
/// be unable to start.
static auto_ptr<CFileLock> s_GuardLock;
static CMiniMutex s_IndexLock;
/// Index database file
static auto_ptr<CNCDBIndexFile> s_IndexDB;
/// Read-write lock to work with m_DBParts
static CMiniMutex s_DBFilesLock;
/// List of all database parts in the storage
static TNCDBFilesMap* s_DBFiles = NULL;
static Uint4 s_LastFileId = 0;
static CMiniMutex s_NextWriteLock;
static SWritingInfo s_AllWritings[s_CntAllFiles];
static Uint4 s_NewFileSize;
static TTimeBuckets s_TimeTables;
static Uint8 s_CurBlobsCnt = 0;
static Uint8 s_CurKeysCnt = 0;
static bool s_Draining = false;
/// Current size of storage database. Kept here for printing statistics.
static Uint8 s_CurDBSize = 0;
static CMiniMutex s_GarbageLock;
static Uint8 s_GarbageSize = 0;
static int s_LastFlushTime = 0;
/// Internal cache of blobs identification information sorted to be able
/// to search by key, subkey and version.
static TBucketCacheMap s_BucketsCache;
static EStopCause s_IsStopWrite = eNoStop;
static bool s_CleanStart;
static bool s_NeedSaveLogRecNo;
static bool s_NeedSavePurgeData;
static Uint1 s_WarnLimitOnPct;
static Uint1 s_WarnLimitOffPct;
static int s_MinRecNoSavePeriod;
static int s_LastRecNoSaveTime = 0;
static CAtomicCounter s_BlobCounter;
static Uint8 s_ExtraGCOnSize;
static Uint8 s_ExtraGCOffSize;
static Uint8 s_StopWriteOnSize;
static Uint8 s_StopWriteOffSize;
static Uint8 s_DiskFreeLimit;
static Uint8 s_DiskCritical;
static CNewFileCreator* s_NewFileCreator;
static CDiskFlusher* s_DiskFlusher;
static CRecNoSaver* s_RecNoSaver;
static CSpaceShrinker* s_SpaceShrinker;
static CExpiredCleaner* s_ExpiredCleaner;



#define DB_CORRUPTED(msg)                                                   \
    do {                                                                    \
        SRV_LOG(Fatal, "Database is corrupted. " << msg                     \
                << " Bug, faulty disk or somebody writes to database?");    \
        abort();                                                            \
    } while (0)                                                             \
/**/



static inline char*
s_MapFile(TFileHandle fd, size_t file_size)
{
    char* mem_ptr = NULL;
#ifdef NCBI_OS_LINUX
    file_size = (file_size + kMemPageSize - 1) & kMemPageAlignMask;
    mem_ptr = (char*)mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (mem_ptr == MAP_FAILED) {
        SRV_LOG(Critical, "Cannot map file into memory, errno=" << errno);
        return NULL;
    }
#endif
    return mem_ptr;
}

static inline void
s_UnmapFile(char* mem_ptr, size_t file_size)
{
#ifdef NCBI_OS_LINUX
    file_size = (file_size + kMemPageSize - 1) & kMemPageAlignMask;
    munmap(mem_ptr, file_size);
#endif
}

static inline void
s_LockFileMem(const void* mem_ptr, size_t mem_size)
{
// mlock is not allowed on NCBI servers (limit 32 KB)
#if 0
//#ifdef NCBI_OS_LINUX
    int res = mlock(mem_ptr, mem_size);
    if (res) {
        SRV_LOG(Critical, "mlock finished with error, errno=" << errno);
    }
#endif
}



static bool
s_EnsureDirExist(const string& dir_name)
{
    CDir dir(dir_name);
    if (!dir.Exists()) {
        return dir.Create();
    }
    return true;
}

/// Read from registry only those parameters that can be changed on the
/// fly, without re-starting the application.
static bool
s_ReadVariableParams(void)
{
    const CNcbiRegistry& reg = CTaskServer::GetConfRegistry();

    s_GCBatchSize = Uint2(reg.GetInt(kNCStorage_RegSection, kNCStorage_GCBatchParam, 500));

    string str = reg.GetString(kNCStorage_RegSection, kNCStorage_FileSizeParam, "100 MB");
    s_NewFileSize = Uint4(NStr::StringToUInt8_DataSize(str));
    s_MaxGarbagePct = reg.GetInt(kNCStorage_RegSection, kNCStorage_GarbagePctParam, 20);
    str = reg.GetString(kNCStorage_RegSection, kNCStorage_MinDBSizeParam, "1 GB");
    s_MinDBSize = NStr::StringToUInt8_DataSize(str);
    s_MinMoveLife = reg.GetInt(kNCStorage_RegSection, kNCStorage_MoveLifeParam, 1000);
    s_FailedMoveDelay = reg.GetInt(kNCStorage_RegSection, kNCStorage_FailedMoveParam, 10);

    s_MinRecNoSavePeriod = reg.GetInt(kNCStorage_RegSection, kNCStorage_MinRecNoSaveParam, 30);
    s_FlushTimePeriod = reg.GetInt(kNCStorage_RegSection, kNCStorage_FlushTimeParam, 0);

    s_ExtraGCOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOnParam, "0"));
    s_ExtraGCOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_ExtraGCOffParam, "0"));
    s_StopWriteOnSize  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOnParam, "0"));
    s_StopWriteOffSize = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_StopWriteOffParam, "0"));
    s_DiskFreeLimit  = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_MinFreeDiskParam, "5 GB"));
    s_DiskCritical   = NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, kNCStorage_DiskCriticalParam, "1 GB"));

    int warn_pct = reg.GetInt(kNCStorage_RegSection, "db_limit_percentage_alert", 65);
    if (warn_pct <= 0  ||  warn_pct >= 100) {
        SRV_LOG(Error, "Parameter db_limit_percentage_alert has wrong value "
                       << warn_pct << ". Assuming it's 65.");
        warn_pct = 65;
    }
    s_WarnLimitOnPct = Uint1(warn_pct);
    warn_pct = reg.GetInt(kNCStorage_RegSection, "db_limit_percentage_alert_delta", 5);
    if (warn_pct <= 0  ||  warn_pct >= s_WarnLimitOnPct) {
        SRV_LOG(Error, "Parameter db_limit_percentage_alert_delta has wrong value "
                       << warn_pct << ". Assuming it's 5.");
        warn_pct = 5;
    }
    s_WarnLimitOffPct = s_WarnLimitOnPct - Uint1(warn_pct);

    SetWBSoftSizeLimit(NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, "write_back_soft_size_limit", "4 GB")));
    SetWBHardSizeLimit(NStr::StringToUInt8_DataSize(reg.GetString(
                       kNCStorage_RegSection, "write_back_hard_size_limit", "6 GB")));

    int to2 = reg.GetInt(kNCStorage_RegSection, "write_back_timeout", 1000);
    int to1 = reg.GetInt(kNCStorage_RegSection, "write_back_timeout_startup", to2);
    SetWBWriteTimeout(to1, to2);
    SetWBFailedWriteDelay(reg.GetInt(kNCStorage_RegSection, "write_back_failed_delay", 2));

    return true;
}

/// Read all storage parameters from registry
static bool
s_ReadStorageParams(void)
{
    const CNcbiRegistry& reg = CTaskServer::GetConfRegistry();

    s_Path = reg.Get(kNCStorage_RegSection, kNCStorage_PathParam);
    s_Prefix = reg.Get(kNCStorage_RegSection, kNCStorage_FilePrefixParam);
    if (s_Path.empty()  ||  s_Prefix.empty()) {
        SRV_LOG(Critical, "Incorrect parameters for " << kNCStorage_PathParam
                          << " and " << kNCStorage_FilePrefixParam
                          << " in the section '" << kNCStorage_RegSection << "': '"
                          << s_Path << "' and '" << s_Prefix << "'");
        return false;
    }
    if (!s_EnsureDirExist(s_Path)) {
        SRV_LOG(Critical, "Cannot create directory " << s_Path);
        return false;
    }
    s_GuardName = reg.Get(kNCStorage_RegSection, kNCStorage_GuardNameParam);
    if (s_GuardName.empty()) {
        s_GuardName = CDirEntry::MakePath(s_Path,
                                          kNCStorage_StartedFileName,
                                          s_Prefix);
    }
    try {
        return s_ReadVariableParams();
    }
    catch (CStringException& ex) {
        SRV_LOG(Critical, "Bad configuration: " << ex);
        return false;
    }
}

/// Make name of the index file in the storage
static string
s_GetIndexFileName(void)
{
    string file_name(s_Prefix);
    file_name += kNCStorage_IndexFileSuffix;
    file_name = CDirEntry::MakePath(s_Path, file_name, kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

/// Make name of file with meta-information in given database part
static string
s_GetFileName(Uint4 file_id, ENCDBFileType file_type)
{
    string file_name(s_Prefix);
    switch (file_type) {
    case eDBFileMeta:
        file_name += kNCStorage_MetaFileSuffix;
        break;
    case eDBFileData:
        file_name += kNCStorage_DataFileSuffix;
        break;
    case eDBFileMaps:
        file_name += kNCStorage_MapsFileSuffix;
        break;
    default:
        abort();
    }
    file_name += NStr::UIntToString(file_id);
    file_name = CDirEntry::MakePath(s_Path, file_name, kNCStorage_DBFileExt);
    return CDirEntry::CreateAbsolutePath(file_name);
}

/// Make sure that current storage database is used only with one instance
/// of NetCache. Lock specially named file exclusively and hold the lock
/// for all time while process is working. This file also is used for
/// determining if previous instance of NetCache was closed gracefully.
/// If another instance of NetCache using the database method will throw
/// an exception.
///
/// @return
///   TRUE if guard file existed and was unlocked meaning that previous
///   instance of NetCache was terminated inappropriately. FALSE if file
///   didn't exist so that this storage instance is a clean start.
static bool
s_LockInstanceGuard(void)
{
    s_CleanStart = !CFile(s_GuardName).Exists();

    try {
        if (s_CleanStart) {
            // Just create the file with read and write permissions
            CFileWriter tmp_writer(s_GuardName, CFileWriter::eOpenAlways);
            CFile guard_file(s_GuardName);
            CFile::TMode user_mode, grp_mode, oth_mode;

            guard_file.GetMode(&user_mode, &grp_mode, &oth_mode);
            user_mode |= CFile::fRead;
            guard_file.SetMode(user_mode, grp_mode, oth_mode);
        }

        s_GuardLock.reset(new CFileLock(s_GuardName,
                                        CFileLock::fDefault,
                                        CFileLock::eExclusive));
        if (!s_CleanStart  &&  CFile(s_GuardName).GetLength() == 0)
            s_CleanStart = true;
        if (!s_CleanStart) {
            INFO("NetCache wasn't finished cleanly in previous run. "
                 "Will try to work with storage as is.");
        }

        CFileIO writer;
        writer.SetFileHandle(s_GuardLock->GetFileHandle());
        writer.SetFileSize(0, CFileIO::eBegin);
        string pid(NStr::UInt8ToString(CProcess::GetCurrentPid()));
        writer.Write(pid.c_str(), pid.size());
    }
    catch (CFileErrnoException& ex) {
        SRV_LOG(Critical, "Can't lock the database (other instance is running?): " << ex);
        return false;
    }
    return true;
}

/// Unlock and delete specially named file used to ensure only one
/// instance working with database.
/// Small race exists here now which cannot be resolved for Windows
/// without re-writing of the mechanism using low-level functions. Race
/// can appear like this: if another instance will start right when this
/// instance stops then other instance can be thinking that this instance
/// terminated incorrectly.
static void
s_UnlockInstanceGuard(void)
{
    if (s_GuardLock.get()) {
        s_GuardLock.reset();
        CFile(s_GuardName).Remove();
    }
}

/// Open and read index database file
static bool
s_OpenIndexDB(void)
{
    string index_name = s_GetIndexFileName();
    for (int i = 0; i < 2; ++i) {
        try {
            s_IndexDB.reset(new CNCDBIndexFile(index_name));
            s_IndexDB->CreateDatabase();
            s_IndexDB->GetAllDBFiles(s_DBFiles);
            ERASE_ITERATE(TNCDBFilesMap, it, (*s_DBFiles)) {
                CSrvRef<SNCDBFileInfo> info = it->second;
                Int8 file_size = -1;
#ifdef NCBI_OS_LINUX
                info->fd = open(info->file_name.c_str(), O_RDWR | O_NOATIME);
                if (info->fd == -1) {
                    SRV_LOG(Critical, "Cannot open storage file, errno=" << errno);
delete_file:
                    try {
                        s_IndexDB->DeleteDBFile(info->file_id);
                    }
                    catch (CSQLITE_Exception& ex) {
                        SRV_LOG(Critical, "Error cleaning index file: " << ex);
                    }
                    s_DBFiles->erase(it);
                    continue;
                }
                file_size = CFile(info->file_name).GetLength();
                if (file_size == -1) {
                    SRV_LOG(Critical, "Cannot read file size (errno=" << errno
                                      << "). File " << info->file_name
                                      << " will be deleted.");
                    goto delete_file;
                }
                info->file_size = Uint4(file_size);
                info->file_map = s_MapFile(info->fd, info->file_size);
                if (!info->file_map)
                    goto delete_file;
                if (*(Uint8*)info->file_map == kMetaSignature) {
                    info->file_type = eDBFileMeta;
                    info->type_index = eFileIndexMeta;
                }
                else if (*(Uint8*)info->file_map == kDataSignature) {
                    info->file_type = eDBFileData;
                    info->type_index = eFileIndexData;
                }
                else if (*(Uint8*)info->file_map == kMapsSignature) {
                    info->file_type = eDBFileMaps;
                    info->type_index = eFileIndexMaps;
                }
                else {
                    SRV_LOG(Critical, "Unknown file signature: " << *(Uint8*)info->file_map);
                    goto delete_file;
                }
                info->index_head = (SFileIndexRec*)(info->file_map + file_size);
                --info->index_head;
#endif
            }
            return true;
        }
        catch (CSQLITE_Exception& ex) {
            s_IndexDB.reset();
            SRV_LOG(Error, "Index file is broken, reinitializing storage. " << ex);
            CFile(index_name).Remove();
        }
    }
    SRV_LOG(Critical, "Cannot open or create index file for the storage.");
    return false;
}

/// Reinitialize database cleaning all data from it.
/// Only database is cleaned, internal cache is left intact.
static void
s_CleanDatabase(void)
{
    INFO("Reinitializing storage " << s_Prefix << " at " << s_Path);

    s_DBFiles->clear();
    s_IndexDB->DeleteAllDBFiles();
}

/// Check if database need re-initialization depending on different
/// parameters and state of the guard file protecting from several
/// instances running on the same database.
static bool
s_ReinitializeStorage(void)
{
    try {
        s_CleanDatabase();
        return true;
    }
    catch (CSQLITE_Exception& ex) {
        SRV_LOG(Error, "Error in soft reinitialization, trying harder. " << ex);
        s_IndexDB.reset();
        CFile(s_GetIndexFileName()).Remove();
        return s_OpenIndexDB();
    }
}

static inline bool
s_IsIndexDeleted(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    Uint4 rec_num = Uint4(file_info->index_head - ind_rec);
    return ind_rec->next_num == rec_num  ||  ind_rec->prev_num == rec_num;
}

static SFileIndexRec*
s_GetIndOrDeleted(SNCDBFileInfo* file_info, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = file_info->index_head - rec_num;
    char* min_ptr = file_info->file_map + kSignatureSize;
    if ((char*)ind_rec < min_ptr  ||  ind_rec >= file_info->index_head) {
        DB_CORRUPTED("Bad record number requested, rec_num=" << rec_num
                     << " in file " << file_info->file_name
                     << ". It produces pointer " << (void*)ind_rec
                     << " which is not in the range between " << (void*)min_ptr
                     << " and " << (void*)file_info->index_head << ".");
    }
    Uint4 next_num = ACCESS_ONCE(ind_rec->next_num);
    if ((next_num != 0  &&  next_num < rec_num)  ||  ind_rec->prev_num > rec_num)
    {
        DB_CORRUPTED("Index record " << rec_num
                     << " in file " << file_info->file_name
                     << " contains bad next_num " << next_num
                     << " and/or prev_num " << ind_rec->prev_num << ".");
    }
    return ind_rec;
}

static SFileIndexRec*
s_GetIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = s_GetIndOrDeleted(file_info, rec_num);
    if (s_IsIndexDeleted(file_info, ind_rec)) {
        DB_CORRUPTED("Index record " << rec_num
                     << " in file " << file_info->file_name
                     << " has been deleted.");
    }
    return ind_rec;
}

static void
s_DeleteIndexRec(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    SFileIndexRec* prev_rec;
    SFileIndexRec* next_rec;
    if (ind_rec->prev_num == 0)
        prev_rec = file_info->index_head;
    else
        prev_rec = s_GetIndexRec(file_info, ind_rec->prev_num);
    if (ind_rec->next_num == 0)
        next_rec = file_info->index_head;
    else
        next_rec = s_GetIndexRec(file_info, ind_rec->next_num);
    // These should be in the exactly this order to prevent unrecoverable
    // corruption.
    ACCESS_ONCE(prev_rec->next_num) = ind_rec->next_num;
    ACCESS_ONCE(next_rec->prev_num) = ind_rec->prev_num;
    ind_rec->next_num = ind_rec->prev_num = Uint4(file_info->index_head - ind_rec);
}

static inline void
s_DeleteIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = s_GetIndexRec(file_info, rec_num);
    s_DeleteIndexRec(file_info, ind_rec);
}

static void
s_MoveRecToGarbage(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    Uint4 size = ind_rec->rec_size;
    if (size & 7)
        size += 8 - (size & 7);
    size += sizeof(SFileIndexRec);

    file_info->info_lock.Lock();
    s_DeleteIndexRec(file_info, ind_rec);
    if (file_info->used_size < size)
        abort();
    file_info->used_size -= size;
    file_info->garb_size += size;
    if (file_info->garb_size > file_info->file_size)
        abort();
    if (file_info->index_head->next_num == 0  &&  file_info->used_size != 0)
        abort();
    file_info->info_lock.Unlock();

    s_GarbageLock.Lock();
    s_GarbageSize += size;
    s_GarbageLock.Unlock();
}

static CSrvRef<SNCDBFileInfo>
s_GetDBFile(Uint4 file_id)
{
    s_DBFilesLock.Lock();
    TNCDBFilesMap::const_iterator it = s_DBFiles->find(file_id);
    if (it == s_DBFiles->end()) {
        DB_CORRUPTED("Cannot find file " << file_id);
    }
    CSrvRef<SNCDBFileInfo> file_info = it->second;
    s_DBFilesLock.Unlock();

    return file_info;
}

static inline Uint1
s_CalcMapDepthImpl(Uint8 size, Uint4 chunk_size, Uint2 map_size)
{
    Uint1 map_depth = 0;
    Uint8 cnt_chunks = (size + chunk_size - 1) / chunk_size;
    while (cnt_chunks > 1  &&  map_depth <= kNCMaxBlobMapsDepth) {
        ++map_depth;
        cnt_chunks = (cnt_chunks + map_size - 1) / map_size;
    }
    return map_depth;
}

static Uint1
s_CalcMapDepth(Uint8 size, Uint4 chunk_size, Uint2 map_size)
{
    Uint1 map_depth = s_CalcMapDepthImpl(size, chunk_size, map_size);
    if (map_depth > kNCMaxBlobMapsDepth) {
        DB_CORRUPTED("Size parameters are resulted in bad map_depth"
                     << ", size=" << size
                     << ", chunk_size=" << chunk_size
                     << ", map_size=" << map_size);
    }
    return map_depth;
}

static inline Uint2
s_CalcCntMapDowns(Uint4 rec_size)
{
    SFileChunkMapRec map_rec;
    return Uint2((SNCDataCoord*)((char*)&map_rec + rec_size) - map_rec.down_coords);
}

static inline Uint4
s_CalcChunkDataSize(Uint4 rec_size)
{
    SFileChunkDataRec data_rec;
    return Uint4((Uint1*)((char*)&data_rec + rec_size) - data_rec.chunk_data);
}

static inline Uint4
s_CalcMetaRecSize(Uint2 key_size)
{
    SFileMetaRec meta_rec;
    return Uint4((char*)&meta_rec.key_data[key_size] - (char*)&meta_rec);
}

static inline Uint4
s_CalcMapRecSize(Uint2 cnt_downs)
{
    SFileChunkMapRec map_rec;
    return Uint4((char*)&map_rec.down_coords[cnt_downs] - (char*)&map_rec);
}

static inline Uint4
s_CalcChunkRecSize(size_t data_size)
{
    SFileChunkDataRec data_rec;
    return Uint4((char*)&data_rec.chunk_data[data_size] - (char*)&data_rec);
}

static char*
s_CalcRecordAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    char* rec_ptr = file_info->file_map + ind_rec->offset;
    char* rec_end = rec_ptr + ind_rec->rec_size;
    char* min_ptr = file_info->file_map + kSignatureSize;
    if (rec_ptr < min_ptr  ||  rec_ptr >= (char*)ind_rec
        ||  rec_end < rec_ptr  ||  rec_end > (char*)ind_rec
        ||  (file_info->index_head - ind_rec == 1  &&  (char*)rec_ptr != min_ptr))
    {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong offset " << ind_rec->offset
                     << " and/or size " << ind_rec->rec_size
                     << ". It results in address " << (void*)rec_ptr
                     << " that doesn't fit between " << (void*)min_ptr
                     << " and " << (void*)ind_rec << ".");
    }
    return rec_ptr;
}

static SFileMetaRec*
s_CalcMetaAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecMeta) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecMeta) << ".");
    }
    SFileMetaRec* meta_rec = (SFileMetaRec*)s_CalcRecordAddress(file_info, ind_rec);
    Uint4 min_size = sizeof(SFileMetaRec) + 1;
    if (meta_rec->has_password)
        min_size += 16;
    if (ind_rec->rec_size < min_size) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong rec_size " << ind_rec->rec_size
                     << " for meta record."
                     << " It's less than minimum " << min_size << ".");
    }
    return meta_rec;
}

static SFileChunkMapRec*
s_CalcMapAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecChunkMap) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecChunkMap) << ".");
    }
    char* rec_ptr = s_CalcRecordAddress(file_info, ind_rec);
    Uint4 min_size = sizeof(SFileChunkMapRec);
    if (ind_rec->rec_size < min_size) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong rec_size " << ind_rec->rec_size
                     << " for map record."
                     << " It's less than minimum " << min_size << ".");
    }
    return (SFileChunkMapRec*)rec_ptr;
}

static SFileChunkDataRec*
s_CalcChunkAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecChunkData) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecChunkData) << ".");
    }
    char* rec_ptr = s_CalcRecordAddress(file_info, ind_rec);
    Uint4 min_size = sizeof(SFileChunkDataRec);
    if (ind_rec->rec_size < min_size) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong rec_size " << ind_rec->rec_size
                     << " for data record."
                     << " It's less than minimum " << min_size << ".");
    }
    return (SFileChunkDataRec*)rec_ptr;
}

/// Do set of procedures creating and initializing new database part and
/// switching storage to using new database part as current one.
static bool
s_CreateNewFile(size_t type_idx)
{
    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    Uint4 file_id;
    if (s_LastFileId == kNCMaxDBFileId)
        file_id = 1;
    else
        file_id = s_LastFileId + 1;
    //size_t true_type_idx = type_idx - eFileIndexMoveShift;
    size_t true_type_idx = type_idx;
    string file_name = s_GetFileName(file_id, s_AllFileTypes[true_type_idx]);
    SNCDBFileInfo* file_info = new SNCDBFileInfo();
    file_info->file_id      = file_id;
    file_info->file_name    = file_name;
    file_info->create_time  = CSrvTime::CurSecs();
    file_info->file_size    = s_NewFileSize;
    file_info->file_type    = s_AllFileTypes[true_type_idx];
    file_info->type_index   = EDBFileIndex(true_type_idx);

#ifdef NCBI_OS_LINUX
    file_info->fd = open(file_name.c_str(),
                         O_RDWR | O_CREAT | O_TRUNC | O_NOATIME,
                         S_IRUSR | S_IWUSR);
    if (file_info->fd == -1) {
        SRV_LOG(Critical, "Cannot create new storage file, errno=" << errno);
        delete file_info;
        return false;
    }
    int trunc_res = ftruncate(file_info->fd, s_NewFileSize);
    if (trunc_res != 0) {
        SRV_LOG(Critical, "Cannot truncate new file, errno=" << errno);
        delete file_info;
        return false;
    }
    file_info->file_map = s_MapFile(file_info->fd, s_NewFileSize);
    if (!file_info->file_map) {
        delete file_info;
        return false;
    }
#endif

    file_info->index_head = (SFileIndexRec*)(file_info->file_map + file_info->file_size);
    --file_info->index_head;
    file_info->index_head->next_num = file_info->index_head->prev_num = 0;

    s_IndexLock.Lock();
    try {
        s_IndexDB->NewDBFile(file_id, file_name);
    }
    catch (CSQLITE_Exception& ex) {
        s_IndexLock.Unlock();
        SRV_LOG(Critical, "Error while adding new storage file: " << ex);
        delete file_info;
        return false;
    }
    s_IndexLock.Unlock();

    s_LastFileId = file_id;
    switch (true_type_idx) {
    case eFileIndexMeta:
        *(Uint8*)file_info->file_map = kMetaSignature;
        break;
    case eFileIndexData:
        *(Uint8*)file_info->file_map = kDataSignature;
        break;
    case eFileIndexMaps:
        *(Uint8*)file_info->file_map = kMapsSignature;
        break;
    default:
        abort();
    }

    s_DBFilesLock.Lock();
    (*s_DBFiles)[file_id] = file_info;

    s_NextWriteLock.Lock();
    s_AllWritings[type_idx].next_file = file_info;
    s_CurDBSize += kSignatureSize + sizeof(SFileIndexRec);
    s_NextWriteLock.Unlock();

    s_DBFilesLock.Unlock();

    return true;
}

static void
s_DeleteDBFile(const CSrvRef<SNCDBFileInfo>& file_info)
{
    s_IndexLock.Lock();
    try {
        s_IndexDB->DeleteDBFile(file_info->file_id);
    }
    catch (CSQLITE_Exception& ex) {
        SRV_LOG(Critical, "Index database does not delete rows: " << ex);
    }
    s_IndexLock.Unlock();

    s_DBFilesLock.Lock();
    s_DBFiles->erase(file_info->file_id);
    s_DBFilesLock.Unlock();

    s_NextWriteLock.Lock();
    s_CurDBSize -= file_info->file_size;
    s_NextWriteLock.Unlock();
    s_GarbageLock.Lock();
    s_GarbageSize -= file_info->garb_size;
    s_GarbageLock.Unlock();
}

SNCDBFileInfo::SNCDBFileInfo(void)
    : file_map(NULL),
      file_id(0),
      file_size(0),
      garb_size(0),
      used_size(0),
      index_head(NULL),
      is_releasing(false),
      fd(0),
      create_time(0),
      next_shrink_time(0)
{
    cnt_unfinished.Set(0);
}

SNCDBFileInfo::~SNCDBFileInfo(void)
{
    if (file_map)
        s_UnmapFile(file_map, file_size);
#ifdef NCBI_OS_LINUX
    if (fd  &&  fd != -1  &&  close(fd)) {
        SRV_LOG(Critical, "Error closing file " << file_name
                          << ", errno=" << errno);
    }
    if (unlink(file_name.c_str())) {
// if file not found, it is not error
        if (errno != ENOENT) {
            SRV_LOG(Critical, "Error deleting file " << file_name
                              << ", errno=" << errno);
        }
    }
#endif
}

bool
CNCBlobStorage::Initialize(bool do_reinit)
{
    for (size_t i = 0; i < s_CntAllFiles; ++i) {
        s_AllWritings[i].cur_file = s_AllWritings[i].next_file = NULL;
    }
    s_DBFiles = new TNCDBFilesMap();

    if (!s_ReadStorageParams())
        return false;

    if (!s_LockInstanceGuard())
        return false;
    if (!s_CleanStart) {
        do_reinit = true;
    }
    if (!s_OpenIndexDB())
        return false;
    if (do_reinit) {
        if (!s_ReinitializeStorage())
            return false;
        s_CleanStart = false;
    }

    for (Uint2 i = 1; i <= CNCDistributionConf::GetCntTimeBuckets(); ++i) {
        s_BucketsCache[i] = new SBucketCache();
        s_TimeTables[i] = new STimeTable();
    }

    s_BlobCounter.Set(0);
    if (!s_DBFiles->empty()) {
        int max_create_time = 0;
        ITERATE(TNCDBFilesMap, it, (*s_DBFiles)) {
            CSrvRef<SNCDBFileInfo> file_info = it->second;
            if (file_info->create_time >= max_create_time) {
                max_create_time = file_info->create_time;
                s_LastFileId = file_info->file_id;
            }
        }
    }

    s_NewFileCreator = new CNewFileCreator();
    s_DiskFlusher = new CDiskFlusher();
    s_RecNoSaver = new CRecNoSaver();
    s_SpaceShrinker = new CSpaceShrinker();
    s_ExpiredCleaner = new CExpiredCleaner();
    CBlobCacher* cacher = new CBlobCacher();
    cacher->SetRunnable();

    return true;
}

void
CNCBlobStorage::Finalize(void)
{
    s_IndexDB.reset();

    s_UnlockInstanceGuard();
}

void CNCBlobStorage::WriteSetup(CSrvSocketTask& task)
{
    string is("\": "),iss("\": \""), eol(",\n\""), str("_str"), eos("\"");
    task.WriteText(eol).WriteText(kNCStorage_PathParam        ).WriteText(iss).WriteText(   s_Path).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_FilePrefixParam  ).WriteText(iss).WriteText(   s_Prefix).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_GuardNameParam   ).WriteText(iss).WriteText(   s_GuardName).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_GCBatchParam     ).WriteText(is ).WriteNumber( s_GCBatchSize);
    task.WriteText(eol).WriteText(kNCStorage_FileSizeParam    ).WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( s_NewFileSize)).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_FileSizeParam    ).WriteText(is ).WriteNumber( s_NewFileSize);
    task.WriteText(eol).WriteText(kNCStorage_GarbagePctParam  ).WriteText(is ).WriteNumber( s_MaxGarbagePct);
    task.WriteText(eol).WriteText(kNCStorage_MinDBSizeParam   ).WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( s_MinDBSize)).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_MinDBSizeParam   ).WriteText(is ).WriteNumber( s_MinDBSize);
    task.WriteText(eol).WriteText(kNCStorage_MoveLifeParam    ).WriteText(is ).WriteNumber( s_MinMoveLife);
    task.WriteText(eol).WriteText(kNCStorage_FailedMoveParam  ).WriteText(is ).WriteNumber( s_FailedMoveDelay);
    task.WriteText(eol).WriteText(kNCStorage_MinRecNoSaveParam).WriteText(is ).WriteNumber( s_MinRecNoSavePeriod);
    task.WriteText(eol).WriteText(kNCStorage_FlushTimeParam   ).WriteText(is ).WriteNumber( s_FlushTimePeriod);
    task.WriteText(eol).WriteText(kNCStorage_ExtraGCOnParam   ).WriteText(is ).WriteNumber( s_ExtraGCOnSize);
    task.WriteText(eol).WriteText(kNCStorage_ExtraGCOffParam  ).WriteText(is ).WriteNumber( s_ExtraGCOffSize);
    task.WriteText(eol).WriteText(kNCStorage_StopWriteOnParam ).WriteText(is ).WriteNumber( s_StopWriteOnSize);
    task.WriteText(eol).WriteText(kNCStorage_StopWriteOffParam).WriteText(is ).WriteNumber( s_StopWriteOffSize);
    task.WriteText(eol).WriteText(kNCStorage_MinFreeDiskParam ).WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( s_DiskFreeLimit)).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_MinFreeDiskParam ).WriteText(is ).WriteNumber( s_DiskFreeLimit);
    task.WriteText(eol).WriteText(kNCStorage_DiskCriticalParam).WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( s_DiskCritical)).WriteText(eos);
    task.WriteText(eol).WriteText(kNCStorage_DiskCriticalParam).WriteText(is ).WriteNumber( s_DiskCritical);
    task.WriteText(eol).WriteText("db_limit_percentage_alert" ).WriteText(is ).WriteNumber( s_WarnLimitOnPct);
    task.WriteText(eol).WriteText("db_limit_percentage_alert_delta").WriteText(is).WriteNumber(s_WarnLimitOffPct);
    task.WriteText(eol).WriteText("write_back_soft_size_limit").WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( GetWBSoftSizeLimit())).WriteText(eos);
    task.WriteText(eol).WriteText("write_back_soft_size_limit").WriteText(is ).WriteNumber( GetWBSoftSizeLimit());
    task.WriteText(eol).WriteText("write_back_hard_size_limit").WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( GetWBHardSizeLimit())).WriteText(eos);
    task.WriteText(eol).WriteText("write_back_hard_size_limit").WriteText(is ).WriteNumber( GetWBHardSizeLimit());
    task.WriteText(eol).WriteText("write_back_timeout"        ).WriteText(is ).WriteNumber( GetWBWriteTimeout());
    task.WriteText(eol).WriteText("write_back_failed_delay"   ).WriteText(is ).WriteNumber( GetWBFailedWriteDelay());
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

static SBucketCache*
s_GetBucketCache(Uint2 bucket)
{
    TBucketCacheMap::const_iterator it = s_BucketsCache.find(bucket);
    if (it == s_BucketsCache.end())
        abort();
    return it->second;
}

static SNCCacheData*
s_GetKeyCacheData(Uint2 time_bucket, const string& key, bool need_create)
{
    SBucketCache* cache = s_GetBucketCache(time_bucket);
    SNCCacheData* data = NULL;
    cache->lock.Lock();
    if (need_create) {
        TKeyMap::insert_commit_data commit_data;
        pair<TKeyMap::iterator, bool> ins_res
            = cache->key_map.insert_unique_check(key, SCacheKeyCompare(), commit_data);
        if (ins_res.second) {
            data = new SNCCacheData();
            data->key = key;
            data->time_bucket = time_bucket;
            cache->key_map.insert_unique_commit(*data, commit_data);
            AtomicAdd(s_CurKeysCnt, 1);
        }
        else {
            data = &*ins_res.first;
        }
    }
    else {
        TKeyMap::iterator it = cache->key_map.find(key, SCacheKeyCompare());
        if (it == cache->key_map.end()) {
            data = NULL;
        }
        else {
            data = &*it;
        }
    }
    if (data)
        data->ref_cnt.Add(1);
    cache->lock.Unlock();
    return data;
}

void
CNCBlobStorage::ReferenceCacheData(SNCCacheData* data)
{
    data->ref_cnt.Add(1);
}

void
CNCBlobStorage::ReleaseCacheData(SNCCacheData* data)
{
    if (data->ref_cnt.Add(-1) != 0)
        return;

    SBucketCache* cache = s_GetBucketCache(data->time_bucket);
    cache->lock.Lock();
    if (data->ref_cnt.Get() != 0  ||  !data->coord.empty()
        ||  !((TKeyMapHook*)data)->is_linked())
    {
        cache->lock.Unlock();
        return;
    }
    cache->key_map.erase(*data);
    AtomicSub(s_CurKeysCnt, 1);
    cache->lock.Unlock();

    data->CallRCU();
}

static void
s_InitializeAccessor(CNCBlobAccessor* acessor)
{
    const string& key = acessor->GetBlobKey();
    Uint2 time_bucket = acessor->GetTimeBucket();
    bool need_create = acessor->GetAccessType() == eNCCreate
                       ||  acessor->GetAccessType() == eNCCopyCreate;
    SNCCacheData* data = s_GetKeyCacheData(time_bucket, key, need_create);
    acessor->Initialize(data);
    if (data)
        CNCBlobStorage::ReleaseCacheData(data);
}

CNCBlobAccessor*
CNCBlobStorage::GetBlobAccess(ENCAccessType access,
                              const string& key,
                              const string& password,
                              Uint2         time_bucket)
{
    CNCBlobAccessor* accessor = new CNCBlobAccessor();
    accessor->Prepare(key, password, time_bucket, access);
    s_InitializeAccessor(accessor);
    return accessor;
}

static inline void
s_SwitchToNextFile(SWritingInfo& w_info)
{
    w_info.cur_file = w_info.next_file;
    w_info.next_file = NULL;
    w_info.next_rec_num = 1;
    w_info.next_offset = kSignatureSize;
    w_info.left_file_size = w_info.cur_file->file_size
                             - (kSignatureSize + sizeof(SFileIndexRec));
}

static bool
s_GetNextWriteCoord(EDBFileIndex file_index,
                    Uint4 rec_size,
                    SNCDataCoord& coord,
                    CSrvRef<SNCDBFileInfo>& file_info,
                    SFileIndexRec*& ind_rec)
{
    Uint4 true_rec_size = rec_size;
    if (true_rec_size & 7)
        true_rec_size += 8 - (true_rec_size & 7);
    Uint4 reserve_size = true_rec_size + sizeof(SFileIndexRec);

    bool need_signal_switch = false;
    SWritingInfo& w_info = s_AllWritings[file_index];

    s_NextWriteLock.Lock();
    if (w_info.left_file_size < reserve_size  &&  w_info.next_file == NULL) {
        s_NextWriteLock.Unlock();
        return false;
    }
    if (w_info.left_file_size < reserve_size) {
        s_CurDBSize += w_info.left_file_size;
        s_GarbageSize += w_info.left_file_size;
        w_info.cur_file->info_lock.Lock();
        w_info.cur_file->garb_size += w_info.left_file_size;
        if (w_info.cur_file->used_size
                + w_info.cur_file->garb_size >= w_info.cur_file->file_size)
        {
            abort();
        }
        w_info.cur_file->info_lock.Unlock();

        Uint4 last_rec_num = w_info.next_rec_num - 1;
        SFileIndexRec* last_ind = w_info.cur_file->index_head - last_rec_num;
        s_LockFileMem(last_ind, (last_rec_num + 1) * sizeof(SFileIndexRec));

        s_SwitchToNextFile(w_info);
        need_signal_switch = true;
    }
    file_info = w_info.cur_file;
    coord.file_id = file_info->file_id;
    coord.rec_num = w_info.next_rec_num++;
    ind_rec = file_info->index_head - coord.rec_num;
    ind_rec->offset = w_info.next_offset;
    ind_rec->rec_size = rec_size;
    ind_rec->rec_type = eFileRecNone;
    ind_rec->cache_data = NULL;
    ind_rec->chain_coord.clear();

    w_info.next_offset += true_rec_size;
    w_info.left_file_size -= reserve_size;
    s_CurDBSize += reserve_size;

    file_info->cnt_unfinished.Add(1);

    file_info->info_lock.Lock();

    file_info->used_size += reserve_size;
    SFileIndexRec* index_head = file_info->index_head;
    Uint4 prev_rec_num = index_head->prev_num;
    SFileIndexRec* prev_ind_rec = index_head - prev_rec_num;
    if (prev_ind_rec->next_num != 0)
        abort();
    ind_rec->prev_num = prev_rec_num;
    ind_rec->next_num = 0;
    prev_ind_rec->next_num = coord.rec_num;
    index_head->prev_num = coord.rec_num;

    file_info->info_lock.Unlock();
    s_NextWriteLock.Unlock();

    if (need_signal_switch)
        s_NewFileCreator->SetRunnable();

    return true;
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(!ver_data->coord.empty());
    CSrvRef<SNCDBFileInfo> file_info = s_GetDBFile(ver_data->coord.file_id);
    SFileIndexRec* ind_rec = s_GetIndexRec(file_info, ver_data->coord.rec_num);
    if (ind_rec->cache_data != ver_data->manager->GetCacheData()) {
        DB_CORRUPTED("Index record " << ver_data->coord.rec_num
                     << " in file " << file_info->file_name
                     << " has wrong cache_data pointer " << ind_rec->cache_data
                     << " when should be " << ver_data->manager->GetCacheData() << ".");
    }
    SFileMetaRec* meta_rec = s_CalcMetaAddress(file_info, ind_rec);
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
    ver_data->data_coord = ind_rec->chain_coord;

    ver_data->map_depth = s_CalcMapDepth(ver_data->size,
                                         ver_data->chunk_size,
                                         ver_data->map_size);
    return true;
}

static void
s_UpdateUpCoords(SFileChunkMapRec* map_rec,
                 SFileIndexRec* map_ind,
                 SNCDataCoord coord)
{
    Uint2 cnt_downs = s_CalcCntMapDowns(map_ind->rec_size);
    for (Uint2 i = 0; i < cnt_downs; ++i) {
        SNCDataCoord down_coord = map_rec->down_coords[i];
        CSrvRef<SNCDBFileInfo> file_info = s_GetDBFile(down_coord.file_id);
        SFileIndexRec* ind_rec = s_GetIndexRec(file_info, down_coord.rec_num);
        ind_rec->chain_coord = coord;
    }
}

static bool
s_SaveOneMapImpl(SNCChunkMapInfo* save_map,
                 SNCChunkMapInfo* up_map,
                 Uint2 cnt_downs,
                 Uint2 map_size,
                 Uint1 map_depth,
                 SNCCacheData* cache_data)
{
    SNCDataCoord map_coord;
    CSrvRef<SNCDBFileInfo> map_file;
    SFileIndexRec* map_ind;
    Uint4 rec_size = s_CalcMapRecSize(cnt_downs);
    if (!s_GetNextWriteCoord(eFileIndexMaps, rec_size, map_coord, map_file, map_ind))
        return false;

    map_ind->rec_type = eFileRecChunkMap;
    map_ind->cache_data = cache_data;
    SFileChunkMapRec* map_rec = s_CalcMapAddress(map_file, map_ind);
    map_rec->map_idx = save_map->map_idx;
    map_rec->map_depth = map_depth;
    size_t coords_size = cnt_downs * sizeof(map_rec->down_coords[0]);
    memcpy(map_rec->down_coords, save_map->coords, coords_size);

    up_map->coords[save_map->map_idx] = map_coord;
    ++save_map->map_idx;
    memset(save_map->coords, 0, map_size * sizeof(save_map->coords[0]));
    s_UpdateUpCoords(map_rec, map_ind, map_coord);

    map_file->cnt_unfinished.Add(-1);

    return true;
}

static bool
s_SaveChunkMap(SNCBlobVerData* ver_data,
               SNCCacheData* cache_data,
               SNCChunkMapInfo** maps,
               Uint2 cnt_downs,
               bool save_all_deps)
{
    if (!s_SaveOneMapImpl(maps[0], maps[1], cnt_downs, ver_data->map_size, 1, cache_data))
        return false;

    Uint1 cur_level = 0;
    while (cur_level < kNCMaxBlobMapsDepth
           &&  (maps[cur_level]->map_idx == ver_data->map_size
                ||  (maps[cur_level]->map_idx > 1  &&  save_all_deps)))
    {
        cnt_downs = maps[cur_level]->map_idx;
        ++cur_level;
        if (!s_SaveOneMapImpl(maps[cur_level], maps[cur_level + 1], cnt_downs,
                              ver_data->map_size, cur_level + 1, cache_data))
        {
            return false;
        }
    }

    return true;
}

bool
CNCBlobStorage::WriteBlobInfo(const string& blob_key,
                              SNCBlobVerData* ver_data,
                              SNCChunkMaps* maps,
                              Uint8 cnt_chunks,
                              SNCCacheData* cache_data)
{
    SNCDataCoord old_coord = ver_data->coord;
    SNCDataCoord down_coord = ver_data->data_coord;
    if (old_coord.empty()) {
        if (ver_data->size > ver_data->chunk_size) {
            Uint2 last_chunks_cnt = Uint2((cnt_chunks - 1) % ver_data->map_size) + 1;
            if (!s_SaveChunkMap(ver_data, cache_data, maps->maps, last_chunks_cnt, true))
                return false;

            for (Uint1 i = 1; i <= kNCMaxBlobMapsDepth; ++i) {
                if (!maps->maps[i]->coords[0].empty()) {
                    down_coord = maps->maps[i]->coords[0];
                    maps->maps[i]->coords[0].clear();
                    ver_data->map_depth = i;
                    break;
                }
            }
            if (down_coord.empty())
                abort();
        }
        else if (ver_data->size != 0) {
            down_coord = maps->maps[0]->coords[0];
            maps->maps[0]->coords[0].clear();
            ver_data->map_depth = 0;
        }
    }

    Uint2 key_size = Uint2(blob_key.size());
    if (!ver_data->password.empty())
        key_size += 16;
    Uint4 rec_size = s_CalcMetaRecSize(key_size);
    SNCDataCoord meta_coord;
    CSrvRef<SNCDBFileInfo> meta_file;
    SFileIndexRec* meta_ind;
    if (!s_GetNextWriteCoord(eFileIndexMeta, rec_size, meta_coord, meta_file, meta_ind))
        return false;

    meta_ind->rec_type = eFileRecMeta;
    meta_ind->cache_data = cache_data;
    meta_ind->chain_coord = down_coord;

    SFileMetaRec* meta_rec = s_CalcMetaAddress(meta_file, meta_ind);
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
    }
    memcpy(key_data, blob_key.data(), blob_key.size());

    ver_data->coord = meta_coord;
    ver_data->data_coord = down_coord;

    if (!down_coord.empty()) {
        CSrvRef<SNCDBFileInfo> down_file = s_GetDBFile(down_coord.file_id);
        SFileIndexRec* down_ind = s_GetIndexRec(down_file, down_coord.rec_num);
        down_ind->chain_coord = meta_coord;
    }

    if (!old_coord.empty()) {
        CSrvRef<SNCDBFileInfo> old_file = s_GetDBFile(old_coord.file_id);
        SFileIndexRec* old_ind = s_GetIndexRec(old_file, old_coord.rec_num);
        // Check that meta-data wasn't corrupted.
        s_CalcMetaAddress(old_file, old_ind);
        s_MoveRecToGarbage(old_file, old_ind);
    }

    meta_file->cnt_unfinished.Add(-1);

    return true;
}

static void
s_MoveDataToGarbage(SNCDataCoord coord, Uint1 map_depth, SNCDataCoord up_coord)
{
    CSrvRef<SNCDBFileInfo> file_info = s_GetDBFile(coord.file_id);
    SFileIndexRec* ind_rec = s_GetIndexRec(file_info, coord.rec_num);
    if (ind_rec->chain_coord != up_coord) {
        DB_CORRUPTED("Index record " << coord.rec_num
                     << " in file " << file_info->file_name
                     << " has wrong up_coord " << ind_rec->chain_coord
                     << " when should be " << up_coord << ".");
    }
    if (map_depth == 0) {
        s_CalcChunkAddress(file_info, ind_rec);
        s_MoveRecToGarbage(file_info, ind_rec);
    }
    else {
        SFileChunkMapRec* map_rec = s_CalcMapAddress(file_info, ind_rec);
        Uint2 cnt_downs = s_CalcCntMapDowns(ind_rec->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            s_MoveDataToGarbage(map_rec->down_coords[i], map_depth - 1, coord);
        }
        s_MoveRecToGarbage(file_info, ind_rec);
    }
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data,
                               SNCChunkMaps* maps)
{
    if (!ver_data->coord.empty()) {
        CSrvRef<SNCDBFileInfo> meta_file = s_GetDBFile(ver_data->coord.file_id);
        SFileIndexRec* meta_ind = s_GetIndexRec(meta_file, ver_data->coord.rec_num);
        s_CalcMetaAddress(meta_file, meta_ind);
        if (ver_data->size != 0) {
            s_MoveDataToGarbage(ver_data->data_coord,
                                ver_data->map_depth,
                                ver_data->coord);
        }
        s_MoveRecToGarbage(meta_file, meta_ind);
    }
    else if (maps) {
        SNCDataCoord empty_coord;
        empty_coord.clear();
        for (Uint1 depth = 0; depth <= kNCMaxBlobMapsDepth; ++depth) {
            Uint2 idx = 0;
            while (idx < ver_data->map_size
                   &&  !maps->maps[depth]->coords[idx].empty())
            {
                s_MoveDataToGarbage(maps->maps[depth]->coords[idx], depth, empty_coord);
                ++idx;
            }
        }
    }
}

static bool
s_ReadMapInfo(SNCDataCoord map_coord,
              SNCChunkMapInfo* map_info,
              Uint1 map_depth/*,
              SNCDataCoord up_coord*/)
{
    CSrvRef<SNCDBFileInfo> map_file = s_GetDBFile(map_coord.file_id);
    SFileIndexRec* map_ind = s_GetIndexRec(map_file, map_coord.rec_num);
    /*
    This is an incorrect check because it can race with
    CNCBlobVerManager::x_UpdateVersion

    if (map_ind->chain_coord != up_coord) {
        DB_CORRUPTED("Index record " << map_coord.rec_num
                     << " in file " << map_file->file_name
                     << " has wrong up_coord " << map_ind->chain_coord
                     << " when should be " << up_coord << ".");
    }
    */
    SFileChunkMapRec* map_rec = s_CalcMapAddress(map_file, map_ind);
    if (map_rec->map_idx != map_info->map_idx  ||  map_rec->map_depth != map_depth)
    {
        DB_CORRUPTED("Map at coord " << map_coord
                     << " has wrong index " << map_rec->map_idx
                     << " and/or depth " << map_rec->map_depth
                     << " when it should be " << map_info->map_idx
                     << " and " << map_depth << ".");
    }
    memcpy(map_info->coords, map_rec->down_coords,
           s_CalcCntMapDowns(map_ind->rec_size) * sizeof(map_rec->down_coords[0]));
    return true;
}

bool
CNCBlobStorage::ReadChunkData(SNCBlobVerData* ver_data,
                              SNCChunkMaps* maps,
                              Uint8 chunk_num,
                              char*& buffer,
                              Uint4& buf_size)
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

    SNCDataCoord map_coord/*, up_coord*/;
    for (Uint1 depth = ver_data->map_depth; depth > 0; ) {
        Uint2 this_index = map_idx[depth];
        --depth;
        SNCChunkMapInfo* this_map = maps->maps[depth];
        if (this_map->map_idx == this_index
            &&  !this_map->coords[0].empty()
            &&  this_map->map_counter == ver_data->map_move_counter)
        {
            continue;
        }

        this_map->map_counter = ver_data->map_move_counter;
        this_map->map_idx = this_index;

        if (depth + 1 == ver_data->map_depth) {
            //up_coord = ver_data->coord;
            map_coord = ver_data->data_coord;
        }
        else {
            /*if (depth + 2 == ver_data->map_depth)
                up_coord = ver_data->data_coord;
            else
                up_coord = maps->maps[depth + 2]->coords[map_idx[depth + 2]];*/
            map_coord = maps->maps[depth + 1]->coords[this_index];
        }

        s_ReadMapInfo(map_coord, this_map, depth + 1/*, up_coord*/);

        while (depth > 0) {
            this_index = map_idx[depth];
            --depth;
            this_map = maps->maps[depth];
            this_map->map_counter = ver_data->map_move_counter;
            this_map->map_idx = this_index;
            //up_coord = map_coord;
            map_coord = maps->maps[depth + 1]->coords[this_index];
            s_ReadMapInfo(map_coord, this_map, depth + 1/*, up_coord*/);
        }
    }

    SNCDataCoord chunk_coord;
    if (ver_data->map_depth == 0) {
        if (chunk_num != 0)
            abort();
        //up_coord = ver_data->coord;
        chunk_coord = ver_data->data_coord;
    }
    else {
        /*if (ver_data->map_depth == 1)
            up_coord = ver_data->data_coord;
        else
            up_coord = maps->maps[1]->coords[map_idx[1]];*/
        chunk_coord = maps->maps[0]->coords[map_idx[0]];
    }

    CSrvRef<SNCDBFileInfo> data_file = s_GetDBFile(chunk_coord.file_id);
    SFileIndexRec* data_ind = s_GetIndexRec(data_file, chunk_coord.rec_num);
    /*
    This is an incorrect check because it can race with
    CNCBlobVerManager::x_UpdateVersion

    if (data_ind->chain_coord != up_coord) {
        DB_CORRUPTED("Index record " << chunk_coord.rec_num
                     << " in file " << data_file->file_name
                     << " has wrong up_coord " << data_ind->chain_coord
                     << " when should be " << up_coord << ".");
    }
    */
    SFileChunkDataRec* data_rec = s_CalcChunkAddress(data_file, data_ind);
    if (data_rec->chunk_num != chunk_num  ||  data_rec->chunk_idx != map_idx[0])
    {
        SRV_LOG(Critical, "File " << data_file->file_name
                          << " in chunk record " << chunk_coord.rec_num
                          << " has wrong chunk number " << data_rec->chunk_num
                          << " and/or chunk index " << data_rec->chunk_idx
                          << ". Deleting blob.");
        return false;
    }

    buf_size = s_CalcChunkDataSize(data_ind->rec_size);
    buffer = (char*)data_rec->chunk_data;

    return true;
}

char*
CNCBlobStorage::WriteChunkData(SNCBlobVerData* ver_data,
                               SNCChunkMaps* maps,
                               SNCCacheData* cache_data,
                               Uint8 chunk_num,
                               char* buffer,
                               Uint4 buf_size)
{
    Uint2 map_idx[kNCMaxBlobMapsDepth] = {0};
    Uint1 cur_index = 0;
    Uint8 level_num = chunk_num;
    do {
        map_idx[cur_index] = Uint2(level_num % ver_data->map_size);
        level_num /= ver_data->map_size;
        ++cur_index;
    }
    while (level_num != 0  &&  cur_index < kNCMaxBlobMapsDepth);
    if (level_num != 0  &&  cur_index >= kNCMaxBlobMapsDepth) {
        SRV_LOG(Critical, "Chunk number " << chunk_num
                          << " exceeded maximum map depth " << kNCMaxBlobMapsDepth
                          << " with map_size=" << ver_data->map_size << ".");
        return NULL;
    }

    if (map_idx[1] != maps->maps[0]->map_idx) {
        if (!s_SaveChunkMap(ver_data, cache_data, maps->maps, ver_data->map_size, false))
            return NULL;
        for (Uint1 i = 0; i < kNCMaxBlobMapsDepth - 1; ++i)
            maps->maps[i]->map_idx = map_idx[i + 1];
    }

    SNCDataCoord data_coord;
    CSrvRef<SNCDBFileInfo> data_file;
    SFileIndexRec* data_ind;
    Uint4 rec_size = s_CalcChunkRecSize(buf_size);
    if (!s_GetNextWriteCoord(eFileIndexData, rec_size, data_coord, data_file, data_ind))
        return NULL;

    data_ind->rec_type = eFileRecChunkData;
    data_ind->cache_data = cache_data;
    SFileChunkDataRec* data_rec = s_CalcChunkAddress(data_file, data_ind);
    data_rec->chunk_num = chunk_num;
    data_rec->chunk_idx = map_idx[0];
    memcpy(data_rec->chunk_data, buffer, buf_size);

    maps->maps[0]->coords[map_idx[0]] = data_coord;

    data_file->cnt_unfinished.Add(-1);

    return (char*)data_rec->chunk_data;
}

void
CNCBlobStorage::ChangeCacheDeadTime(SNCCacheData* cache_data)
{
    STimeTable* table = s_TimeTables[cache_data->time_bucket];
    table->lock.Lock();
    if (cache_data->saved_dead_time != 0) {
        table->time_map.erase(table->time_map.iterator_to(*cache_data));
        AtomicSub(s_CurBlobsCnt, 1);
        if (s_Draining && s_CurBlobsCnt == 0) {
            CTaskServer::RequestShutdown(eSrvSlowShutdown);
        }
    }
    cache_data->saved_dead_time = cache_data->dead_time;
    if (cache_data->saved_dead_time != 0) {
        table->time_map.insert_equal(*cache_data);
        AtomicAdd(s_CurBlobsCnt, 1);
    }
    table->lock.Unlock();
}

Uint8
CNCBlobStorage::GetMaxSyncLogRecNo(void)
{
    Uint8 result = 0;
    s_IndexLock.Lock();
    try {
        result = s_IndexDB->GetMaxSyncLogRecNo();
    }
    catch (CSQLITE_Exception& ex) {
        SRV_LOG(Critical, "Cannot read max_sync_log_rec_no: " << ex);
    }
    s_IndexLock.Unlock();
    return result;
}

void
CNCBlobStorage::SaveMaxSyncLogRecNo(void)
{
    s_NeedSaveLogRecNo = true;
}

string
CNCBlobStorage::GetPurgeData(void)
{
    string result;
    s_IndexLock.Lock();
    try {
        result = s_IndexDB->GetPurgeData();
    }
    catch (CSQLITE_Exception&) {
    }
    s_IndexLock.Unlock();
    return result;
}

void CNCBlobStorage::SavePurgeData(void)
{
    s_NeedSavePurgeData = true;
    s_RecNoSaver->SetRunnable();
}

Uint8
CNCBlobStorage::GetDiskFree(void)
{
    try {
        return CFileUtil::GetFreeDiskSpace(s_Path);
    }
    catch (CFileErrnoException& ex) {
        SRV_LOG(Critical, "Cannot read free disk space: " << ex);
        return 0;
    }
}

Uint8
CNCBlobStorage::GetAllowedDBSize(Uint8 free_space)
{
    Uint8 total_space = s_CurDBSize + free_space;
    Uint8 allowed_db_size = total_space;

    if (total_space < s_DiskCritical)
        allowed_db_size = 0;
    else
        allowed_db_size = min(allowed_db_size, total_space - s_DiskCritical);
    if (total_space < s_DiskFreeLimit)
        allowed_db_size = 0;
    else
        allowed_db_size = min(allowed_db_size, total_space - s_DiskFreeLimit);
    if (s_StopWriteOnSize != 0  &&  s_StopWriteOnSize < allowed_db_size)
        allowed_db_size = s_StopWriteOnSize;

    return allowed_db_size;
}

Uint8
CNCBlobStorage::GetDBSize(void)
{
    return s_CurDBSize;
}

int
CNCBlobStorage::GetNDBFiles(void)
{
    return int(s_DBFiles->size());
}

bool
CNCBlobStorage::IsCleanStart(void)
{
    return s_CleanStart;
}

bool
CNCBlobStorage::NeedStopWrite(void)
{
    return s_Draining || (s_IsStopWrite != eNoStop  &&  s_IsStopWrite != eStopWarning);
}

bool
CNCBlobStorage::AcceptWritesFromPeers(void)
{
    return !s_Draining && s_IsStopWrite != eStopDiskCritical;
}

void
CNCBlobStorage::SetDraining(bool draining)
{
    s_Draining = draining;
    if (draining) {
        if (s_CurBlobsCnt == 0) {
            CTaskServer::RequestShutdown(eSrvSlowShutdown);
        }
    }
}

bool
CNCBlobStorage::IsDraining(void)
{
    return s_Draining;
}

bool
CNCBlobStorage::IsDBSizeAlert(void)
{
    return s_IsStopWrite != eNoStop;
}

Uint4
CNCBlobStorage::GetNewBlobId(void)
{
    return Uint4(s_BlobCounter.Add(1));
}

void
CNCBlobStorage::GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst)
{
    blobs_lst.clear();
    Uint2 slot_buckets = CNCDistributionConf::GetCntSlotBuckets();
    Uint2 bucket_num = (slot - 1) * slot_buckets + 1;
    for (Uint2 i = 0; i < slot_buckets; ++i, ++bucket_num) {
        SBucketCache* cache = s_GetBucketCache(bucket_num);

        cache->lock.Lock();
        Uint8 cnt_blobs = cache->key_map.size();
        void* big_block = malloc(size_t(cnt_blobs * sizeof(SNCTempBlobInfo)));
        if (!big_block) {
            return;
        }
        SNCTempBlobInfo* info_ptr = (SNCTempBlobInfo*)big_block;

        ITERATE(TKeyMap, it, cache->key_map) {
            new (info_ptr) SNCTempBlobInfo(*it);
            ++info_ptr;
        }
        cache->lock.Unlock();

        info_ptr = (SNCTempBlobInfo*)big_block;
        for (Uint8 i = 0; i < cnt_blobs; ++i) {
            Uint2 key_slot = 0, key_bucket = 0;
            if (!CNCDistributionConf::GetSlotByKey(info_ptr->key,
                            key_slot, key_bucket) ||
                    key_slot != slot ||
                    key_bucket != bucket_num)
                abort();

            SNCBlobSummary* blob_sum = new SNCBlobSummary();
            blob_sum->create_id = info_ptr->create_id;
            blob_sum->create_server = info_ptr->create_server;
            blob_sum->create_time = info_ptr->create_time;
            blob_sum->dead_time = info_ptr->dead_time;
            blob_sum->expire = info_ptr->expire;
            blob_sum->ver_expire = info_ptr->ver_expire;
            blobs_lst[info_ptr->key] = blob_sum;

            info_ptr->~SNCTempBlobInfo();
            ++info_ptr;
        }

        free(big_block);
    }
}

void
CNCBlobStorage::MeasureDB(SNCStateStat& state)
{
    s_DBFilesLock.Lock();
    state.db_files = Uint4(s_DBFiles->size());
    s_DBFilesLock.Unlock();

    state.db_size = s_CurDBSize;
    state.db_garb = s_GarbageSize;
    state.cnt_blobs = s_CurBlobsCnt;
    state.cnt_keys = s_CurKeysCnt;
    state.min_dead_time = numeric_limits<int>::max();
    ITERATE(TTimeBuckets, it, s_TimeTables) {
        STimeTable* table = it->second;
        table->lock.Lock();
        if (!table->time_map.empty()) {
            state.min_dead_time = min(state.min_dead_time,
                                      table->time_map.begin()->dead_time);
        }
        table->lock.Unlock();
    }
    if (state.min_dead_time == numeric_limits<int>::max())
        state.min_dead_time = 0;
}

void
CBlobCacher::x_DeleteIndexes(SNCDataCoord map_coord, Uint1 map_depth)
{
    SNCDBFileInfo* file_info = (*s_DBFiles)[map_coord.file_id];
    SFileIndexRec* map_ind = s_GetIndexRec(file_info, map_coord.rec_num);
    s_DeleteIndexRec(file_info, map_ind);
    if (map_depth != 0) {
        SFileChunkMapRec* map_rec = s_CalcMapAddress(file_info, map_ind);
        Uint2 cnt_downs = s_CalcCntMapDowns(map_ind->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            x_DeleteIndexes(map_rec->down_coords[i], map_depth - 1);
        }
    }
}

bool
CBlobCacher::x_CacheMapRecs(SNCDataCoord map_coord,
                            Uint1 map_depth,
                            SNCDataCoord up_coord,
                            Uint2 up_index,
                            SNCCacheData* cache_data,
                            Uint8 cnt_chunks,
                            Uint8& chunk_num,
                            map<Uint4, Uint4>& sizes_map)
{
    TNCDBFilesMap::const_iterator it_file = s_DBFiles->find(map_coord.file_id);
    if (it_file == s_DBFiles->end()) {
        SRV_LOG(Critical, "Cannot find file for the coord " << map_coord
                          << " which is referenced by blob " << cache_data->key
                          << ". Deleting blob.");
        return false;
    }

    SNCDBFileInfo* file_info = it_file->second.GetNCPointerOrNull();
    TRecNumsSet& recs_set = m_RecsMap[map_coord.file_id];
    TRecNumsSet::iterator it_recs = recs_set.find(map_coord.rec_num);
    if (it_recs == recs_set.end()) {
        SRV_LOG(Critical, "Blob " << cache_data->key
                          << " references record with coord " << map_coord
                          << ", but its index wasn't in live chain. Deleting blob.");
        return false;
    }

    SFileIndexRec* map_ind = s_GetIndexRec(file_info, map_coord.rec_num);
    if (map_ind->chain_coord != up_coord) {
        SRV_LOG(Critical, "Up_coord for map/data in blob " << cache_data->key
                          << " is incorrect: " << map_ind->chain_coord
                          << " when should be " << up_coord
                          << ". Correcting it.");
        map_ind->chain_coord = up_coord;
    }
    map_ind->cache_data = cache_data;

    Uint4 rec_size = map_ind->rec_size;
    if (rec_size & 7)
        rec_size += 8 - (rec_size & 7);
    sizes_map[map_coord.file_id] += rec_size + sizeof(SFileIndexRec);

    if (map_depth == 0) {
        if (map_ind->rec_type != eFileRecChunkData) {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " references record with coord " << map_coord
                              << " that has type " << int(map_ind->rec_type)
                              << " when it should be " << int(eFileRecChunkData)
                              << ". Deleting blob.");
            return false;
        }
        ++chunk_num;
        if (chunk_num > cnt_chunks) {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " has too many data chunks, should be " << cnt_chunks
                              << ". Deleting blob.");
            return false;
        }
        Uint4 data_size = s_CalcChunkDataSize(map_ind->rec_size);
        Uint4 need_size;
        if (chunk_num < cnt_chunks)
            need_size = cache_data->chunk_size;
        else
            need_size = (cache_data->size - 1) % cache_data->chunk_size + 1;
        if (data_size != need_size) {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " references data record with coord " << map_coord
                              << " that has data size " << data_size
                              << " when it should be " << need_size
                              << ". Deleting blob.");
            return false;
        }
    }
    else {
        if (map_ind->rec_type != eFileRecChunkMap) {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " references record with coord " << map_coord
                              << " at map_depth=" << map_depth
                              << ". Record has type " << int(map_ind->rec_type)
                              << " when it should be " << int(eFileRecChunkMap)
                              << ". Deleting blob.");
            return false;
        }
        SFileChunkMapRec* map_rec = s_CalcMapAddress(file_info, map_ind);
        if (map_rec->map_idx != up_index  ||  map_rec->map_depth != map_depth)
        {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " references map with coord " << map_coord
                              << " that has wrong index " << map_rec->map_idx
                              << " and/or map depth " << map_rec->map_depth
                              << ", they should be " << up_index
                              << " and " << map_depth
                              << ". Deleting blob.");
            return false;
        }
        Uint2 cnt_downs = s_CalcCntMapDowns(map_ind->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            if (!x_CacheMapRecs(map_rec->down_coords[i], map_depth - 1,
                                map_coord, i, cache_data, cnt_chunks,
                                chunk_num, sizes_map))
            {
                for (Uint2 j = 0; j < i; ++j) {
                    x_DeleteIndexes(map_rec->down_coords[j], map_depth - 1);
                }
                return false;
            }
        }
        if (chunk_num < cnt_chunks  &&  cnt_downs != cache_data->map_size) {
            SRV_LOG(Critical, "Blob " << cache_data->key
                              << " with size " << cache_data->size
                              << " references map record with coord " << map_coord
                              << " that has " << cnt_downs
                              << " children when it should be " << cache_data->map_size
                              << " because accumulated chunk_num=" << chunk_num
                              << " is less than total " << cnt_chunks
                              << ". Deleting blob.");
            return false;
        }
    }

    recs_set.erase(it_recs);
    return true;
}

bool
CBlobCacher::x_CacheMetaRec(SNCDBFileInfo* file_info,
                            SFileIndexRec* ind_rec,
                            SNCDataCoord coord)
{
    SFileMetaRec* meta_rec = s_CalcMetaAddress(file_info, ind_rec);
    if (meta_rec->has_password > 1  ||  meta_rec->dead_time < meta_rec->expire
        ||  meta_rec->dead_time < meta_rec->ver_expire
        ||  meta_rec->chunk_size == 0  ||  meta_rec->map_size == 0)
    {
        SRV_LOG(Critical, "Meta record in file " << file_info->file_name
                          << " at offset " << ind_rec->offset << " was corrupted."
                          << " passwd=" << meta_rec->has_password
                          << ", expire=" << meta_rec->expire
                          << ", ver_expire=" << meta_rec->ver_expire
                          << ", dead=" << meta_rec->dead_time
                          << ", chunk_size=" << meta_rec->chunk_size
                          << ", map_size=" << meta_rec->map_size
                          << ". Deleting it.");
        return false;
    }
    if (meta_rec->dead_time <= CSrvTime::CurSecs())
        return false;

    char* key_data = meta_rec->key_data;
    char* key_end = (char*)meta_rec + ind_rec->rec_size;
    if (meta_rec->has_password)
        key_data += 16;
    string key(key_data, key_end - key_data);
    int cnt = 0;
    ITERATE(string, it, key) {
        if (*it == '\1')
            ++cnt;
    }
    if (cnt != 2) {
        SRV_LOG(Critical, "Meta record in file " << file_info->file_name
                          << " at offset " << ind_rec->offset << " was corrupted."
                          << " key=" << key
                          << ", cnt_special=" << cnt
                          << ". Deleting it.");
        return false;
    }
    Uint2 slot = 0, time_bucket = 0;
    if (!CNCDistributionConf::GetSlotByKey(key, slot, time_bucket)) {
        SRV_LOG(Critical, "Could not extract slot number from key: " << key);
        return false;
    }

    SNCCacheData* cache_data = new SNCCacheData();
    cache_data->key = key;
    cache_data->coord = coord;
    cache_data->time_bucket = time_bucket;
    cache_data->size = meta_rec->size;
    cache_data->chunk_size = meta_rec->chunk_size;
    cache_data->map_size = meta_rec->map_size;
    cache_data->create_time = meta_rec->create_time;
    cache_data->create_server = meta_rec->create_server;
    cache_data->create_id = meta_rec->create_id;
    cache_data->dead_time = meta_rec->dead_time;
    cache_data->saved_dead_time = meta_rec->dead_time;
    cache_data->expire = meta_rec->expire;
    cache_data->ver_expire = meta_rec->ver_expire;

    if (meta_rec->size == 0) {
        if (!ind_rec->chain_coord.empty()) {
            SRV_LOG(Critical, "Index record " << (file_info->index_head - ind_rec)
                              << " in file " << file_info->file_name
                              << " was corrupted. size=0 but chain_coord="
                              << ind_rec->chain_coord << ". Ignoring that.");
            ind_rec->chain_coord.clear();
        }
    }
    else {
        Uint1 map_depth = s_CalcMapDepthImpl(meta_rec->size,
                                             meta_rec->chunk_size,
                                             meta_rec->map_size);
        if (map_depth > kNCMaxBlobMapsDepth) {
            SRV_LOG(Critical, "Meta record in file " << file_info->file_name
                              << " at offset " << ind_rec->offset << " was corrupted."
                              << ", size=" << meta_rec->size
                              << ", chunk_size=" << meta_rec->chunk_size
                              << ", map_size=" << meta_rec->map_size
                              << " (map depth is more than " << kNCMaxBlobMapsDepth
                              << "). Deleting it.");
            return false;
        }
        Uint8 cnt_chunks = (meta_rec->size - 1) / meta_rec->chunk_size + 1;
        Uint8 chunk_num = 0;
        typedef map<Uint4, Uint4> TSizesMap;
        TSizesMap sizes_map;
        if (!x_CacheMapRecs(ind_rec->chain_coord, map_depth, coord, 0, cache_data,
                            cnt_chunks, chunk_num, sizes_map))
        {
            delete cache_data;
            return false;
        }
        ITERATE(TSizesMap, it, sizes_map) {
            SNCDBFileInfo* info = (*s_DBFiles)[it->first];
            info->used_size += it->second;
            if (info->garb_size < it->second)
                abort();
            info->garb_size -= it->second;
            s_GarbageSize -= it->second;
        }
    }
    ind_rec->cache_data = cache_data;
    Uint4 rec_size = ind_rec->rec_size;
    if (rec_size & 7)
        rec_size += 8 - (rec_size & 7);
    rec_size += sizeof(SFileIndexRec);
    file_info->used_size += rec_size;
    if (file_info->garb_size < rec_size)
        abort();
    file_info->garb_size -= rec_size;
    s_GarbageSize -= rec_size;

    TBucketCacheMap::iterator it_bucket = s_BucketsCache.lower_bound(time_bucket);
    SBucketCache* bucket_cache;
    if (it_bucket == s_BucketsCache.end()  ||  it_bucket->first != time_bucket) {
        bucket_cache = new SBucketCache();
        s_BucketsCache.insert(it_bucket, TBucketCacheMap::value_type(time_bucket, bucket_cache));
    }
    else {
        bucket_cache = it_bucket->second;
    }
    STimeTable* time_table = s_TimeTables[time_bucket];
    TKeyMap::insert_commit_data commit_data;
    pair<TKeyMap::iterator, bool> ins_res =
        bucket_cache->key_map.insert_unique_check(key, SCacheKeyCompare(), commit_data);
    if (ins_res.second) {
        bucket_cache->key_map.insert_unique_commit(*cache_data, commit_data);
        ++s_CurKeysCnt;
    }
    else {
        SNCCacheData* old_data = &*ins_res.first;
        bucket_cache->key_map.erase(ins_res.first);
        bucket_cache->key_map.insert_equal(*cache_data);
        --s_CurBlobsCnt;
        time_table->time_map.erase(time_table->time_map.iterator_to(*old_data));
        SNCDBFileInfo* old_file = (*s_DBFiles)[old_data->coord.file_id];
        SFileIndexRec* old_ind = s_GetIndexRec(old_file, old_data->coord.rec_num);
        SFileMetaRec* old_rec = s_CalcMetaAddress(old_file, old_ind);
        if (old_rec->size != 0  &&  old_ind->chain_coord != ind_rec->chain_coord) {
            Uint1 map_depth = s_CalcMapDepth(old_rec->size,
                                             old_rec->chunk_size,
                                             old_rec->map_size);
            s_MoveDataToGarbage(old_ind->chain_coord, map_depth, old_data->coord);
        }
        s_MoveRecToGarbage(old_file, old_ind);
        delete old_data;
    }
    time_table->time_map.insert_equal(*cache_data);
    ++s_CurBlobsCnt;

    return true;
}

CBlobCacher::State
CBlobCacher::x_StartCreateFiles(void)
{
    m_CurCreatePass = 0;
    m_CurCreateFile = 0;
    return &Me::x_CreateInitialFile;
}

CBlobCacher::State
CBlobCacher::x_CreateInitialFile(void)
{
    if (CTaskServer::IsInShutdown())
        return &Me::x_CancelCaching;
    if (m_CurCreatePass == 1  &&  m_CurCreateFile >= s_CntAllFiles)
        return &Me::x_StartCacheBlobs;

    if (m_CurCreateFile < s_CntAllFiles) {
        if (!s_CreateNewFile(m_CurCreateFile))
            return &Me::x_DelFileAndRetryCreate;

        m_NewFileIds.insert(s_AllWritings[m_CurCreateFile].next_file->file_id);
        ++m_CurCreateFile;
    }
    else {
        for (Uint1 i = 0; i < s_CntAllFiles; ++i) {
            s_SwitchToNextFile(s_AllWritings[i]);
        }
        m_CurCreateFile = 0;
        ++m_CurCreatePass;
    }

    // Yield execution to other tasks
    SetRunnable();
    return NULL;
}

CBlobCacher::State
CBlobCacher::x_DelFileAndRetryCreate(void)
{
    if (s_DBFiles->empty()) {
        SRV_LOG(Critical, "Cannot create initial database files.");
        GetDiagCtx()->SetRequestStatus(eStatus_ServerError);
        CTaskServer::RequestShutdown(eSrvFastShutdown);
        return &Me::x_CancelCaching;
    }

    CSrvRef<SNCDBFileInfo> file_to_del;
    ITERATE(TNCDBFilesMap, it, (*s_DBFiles)) {
        CSrvRef<SNCDBFileInfo> file_info = it->second;
        if (file_info->file_type == eDBFileData) {
            file_to_del = file_info;
            break;
        }
    }
    if (!file_to_del)
        file_to_del = s_DBFiles->begin()->second;
    s_DeleteDBFile(file_to_del);

    // Yield execution to other tasks
    SetState(&Me::x_CreateInitialFile);
// file creation failed; probably, makes sense to wait longer
// instead of failing again and again, like thousands times per sec
//    SetRunnable();
    RunAfter(1);

    return NULL;
}

CBlobCacher::State
CBlobCacher::x_PreCacheRecNums(void)
{
    if (CTaskServer::IsInShutdown())
        return &Me::x_CancelCaching;
    if (m_CurFile == s_DBFiles->end())
        return &Me::x_StartCreateFiles;

    SNCDBFileInfo* file_info = m_CurFile->second.GetNCPointerOrNull();
    TRecNumsSet& recs_set = m_RecsMap[file_info->file_id];

    s_CurDBSize += file_info->file_size;
    // Non-garbage is left the same as in x_CreateNewFile
    Uint4 garb_size = file_info->file_size
                      - (kSignatureSize + sizeof(SFileIndexRec));
    file_info->garb_size += garb_size;
    s_GarbageSize += garb_size;

    SFileIndexRec* ind_rec = file_info->index_head;
    Uint4 prev_rec_num = 0;
    char* min_ptr = file_info->file_map + kSignatureSize;
    while (!CTaskServer::IsInShutdown()  &&  ind_rec->next_num != 0) {
        Uint4 rec_num = ind_rec->next_num;
        if (rec_num <= prev_rec_num) {
            SRV_LOG(Critical, "File " << file_info->file_name
                              << " contains wrong next_num=" << rec_num
                              << " (it's not greater than current " << prev_rec_num
                              << "). Won't cache anything else from this file.");
            goto wrap_index_and_return;
        }
        SFileIndexRec* next_ind;
        next_ind = file_info->index_head - rec_num;
        if ((char*)next_ind < min_ptr  ||  next_ind >= ind_rec) {
            SRV_LOG(Critical, "File " << file_info->file_name
                              << " contains wrong next_num=" << rec_num
                              << " in index record " << (file_info->index_head - ind_rec)
                              << ". It produces pointer " << (void*)next_ind
                              << " which is not in the range between " << (void*)min_ptr
                              << " and " << (void*)ind_rec
                              << ". Won't cache anything else from this file.");
            goto wrap_index_and_return;
        }
        char* next_rec_start = file_info->file_map + next_ind->offset;
        if (next_rec_start < min_ptr  ||  next_rec_start > (char*)next_ind
            ||  (rec_num == 1  &&  next_rec_start != min_ptr))
        {
            SRV_LOG(Critical, "File " << file_info->file_name
                              << " contains wrong offset=" << ind_rec->offset
                              << " in index record " << rec_num
                              << ", resulting ptr " << (void*)next_rec_start
                              << " which is not in the range " << (void*)min_ptr
                              << " and " << (void*)next_ind
                              << ". This record will be ignored.");
            goto ignore_rec_and_continue;
        }
        char* next_rec_end;
        next_rec_end = next_rec_start + next_ind->rec_size;
        if (next_rec_end < next_rec_start  ||  next_rec_end > (char*)next_ind) {
            SRV_LOG(Critical, "File " << file_info->file_name
                              << " contains wrong rec_size=" << next_ind->rec_size
                              << " for offset " << next_ind->offset
                              << " in index record " << rec_num
                              << " (resulting end ptr " << (void*)next_rec_end
                              << " is greater than index record " << (void*)next_ind
                              << "). This record will be ignored.");
            goto ignore_rec_and_continue;
        }
        if (next_ind->prev_num != prev_rec_num)
            next_ind->prev_num = prev_rec_num;
        switch (next_ind->rec_type) {
        case eFileRecChunkData:
            if (file_info->file_type != eDBFileData) {
                // This is not an error. It can be a result of failed move
                // attempt.
                goto ignore_rec_and_continue;
            }
            if (next_ind->rec_size < sizeof(SFileChunkDataRec)) {
                SRV_LOG(Critical, "File " << file_info->file_name
                                  << " contains wrong rec_size=" << next_ind->rec_size
                                  << " for offset " << next_ind->offset
                                  << " in index record " << rec_num
                                  << ", rec_type=" << int(next_ind->rec_type)
                                  << ", min_size=" << sizeof(SFileChunkDataRec)
                                  << ". This record will be ignored.");
                goto ignore_rec_and_continue;
            }
            break;
        case eFileRecChunkMap:
            if (file_info->file_type != eDBFileMaps)
                goto bad_rec_type;
            if (next_ind->rec_size < sizeof(SFileChunkMapRec)) {
                SRV_LOG(Critical, "File " << file_info->file_name
                                  << " contains wrong rec_size=" << next_ind->rec_size
                                  << " for offset " << next_ind->offset
                                  << " in index record " << rec_num
                                  << ", rec_type=" << int(next_ind->rec_type)
                                  << ", min_size=" << sizeof(SFileChunkMapRec)
                                  << ". This record will be ignored.");
                goto ignore_rec_and_continue;
            }
            break;
        case eFileRecMeta:
            if (file_info->file_type != eDBFileMeta)
                goto bad_rec_type;
            SFileMetaRec* meta_rec;
            meta_rec = (SFileMetaRec*)next_rec_start;
            Uint4 min_rec_size;
            // Minimum key length is 2, so we are adding 1 below
            min_rec_size = sizeof(SFileChunkMapRec) + 1;
            if (meta_rec->has_password)
                min_rec_size += 16;
            if (next_ind->rec_size < min_rec_size) {
                SRV_LOG(Critical, "File " << file_info->file_name
                                  << " contains wrong rec_size=" << next_ind->rec_size
                                  << " for offset " << next_ind->offset
                                  << " in index record " << rec_num
                                  << ", rec_type=" << int(next_ind->rec_type)
                                  << ", min_size=" << min_rec_size
                                  << ". This record will be ignored.");
                goto ignore_rec_and_continue;
            }
            break;
        default:
bad_rec_type:
            SRV_LOG(Critical, "File " << file_info->file_name
                              << " with type " << int(file_info->file_type)
                              << " contains wrong rec_type=" << next_ind->rec_type
                              << " in index record " << rec_num
                              << "). This record will be ignored.");
            goto ignore_rec_and_continue;
        }

        recs_set.insert(rec_num);
        min_ptr = next_rec_end;
        ind_rec = next_ind;
        prev_rec_num = rec_num;
        continue;

ignore_rec_and_continue:
        ind_rec->next_num = next_ind->next_num;
    }
    goto finalize_and_return;

wrap_index_and_return:
    ind_rec->next_num = 0;

finalize_and_return:
    s_LockFileMem(ind_rec, (prev_rec_num + 1) * sizeof(SFileIndexRec));
// to next file
    ++m_CurFile;
    SetRunnable();
    return NULL;
}

CBlobCacher::State
CBlobCacher::x_StartCaching(void)
{
    CreateNewDiagCtx();
    CSrvDiagMsg().StartRequest().PrintParam("_type", "caching");

    m_CurFile = s_DBFiles->begin();
    return &Me::x_PreCacheRecNums;
}

CBlobCacher::State
CBlobCacher::x_CancelCaching(void)
{
    if (GetDiagCtx()->GetRequestStatus() == eStatus_OK)
        GetDiagCtx()->SetRequestStatus(eStatus_OperationCanceled);
    CSrvDiagMsg().StopRequest();
    ReleaseDiagCtx();

    Terminate();
    return NULL;
}

CBlobCacher::State
CBlobCacher::x_StartCacheBlobs(void)
{
    m_CurFile = s_DBFiles->begin();
    return &Me::x_CacheNextFile;
}

CBlobCacher::State
CBlobCacher::x_CacheNextFile(void)
{
try_next_file:
    if (CTaskServer::IsInShutdown())
        return &Me::x_CancelCaching;
    if (m_CurFile == s_DBFiles->end())
        return &Me::x_CleanOrphanRecs;

    SNCDBFileInfo* file_info = m_CurFile->second.GetNCPointerOrNull();
    if (file_info->file_type != eDBFileMeta
        ||  m_NewFileIds.find(file_info->file_id) != m_NewFileIds.end())
    {
        ++m_CurFile;
        goto try_next_file;
    }
    m_CurRecsSet = &m_RecsMap[file_info->file_id];
    m_CurRecIt = m_CurRecsSet->begin();
    return &Me::x_CacheNextRecord;
}

CBlobCacher::State
CBlobCacher::x_CacheNextRecord(void)
{
    if (CTaskServer::IsInShutdown())
        return &Me::x_CancelCaching;
    if (m_CurRecIt == m_CurRecsSet->end()) {
        m_CurRecsSet->clear();
        ++m_CurFile;
        return &Me::x_CacheNextFile;
    }

    Uint4 rec_num = *m_CurRecIt;
    SNCDBFileInfo* file_info = m_CurFile->second.GetNCPointerOrNull();
    SNCDataCoord coord;
    coord.file_id = file_info->file_id;
    coord.rec_num = rec_num;
    SFileIndexRec* ind_rec = s_GetIndexRec(file_info, rec_num);
    if (!x_CacheMetaRec(file_info, ind_rec, coord))
        s_DeleteIndexRec(file_info, ind_rec);

    ++m_CurRecIt;
    SetRunnable();
    return NULL;
}

CBlobCacher::State
CBlobCacher::x_CleanOrphanRecs(void)
{
    ITERATE(TFileRecsMap, it_id, m_RecsMap) {
        Uint4 file_id = it_id->first;
        const TRecNumsSet& recs_set = it_id->second;
        TNCDBFilesMap::iterator it_file = s_DBFiles->find(file_id);
        if (it_file == s_DBFiles->end()) {
            // File could be deleted when we tried to create initial files.
            continue;
        }
        SNCDBFileInfo* file_info = it_file->second;
        TRecNumsSet::iterator it_rec = recs_set.begin();
        for (; it_rec != recs_set.end(); ++it_rec) {
            s_DeleteIndexRec(file_info, *it_rec);
        }
    }

    return &Me::x_FinishCaching;
}

CBlobCacher::State
CBlobCacher::x_FinishCaching(void)
{
    CSrvDiagMsg().StopRequest();
    ReleaseDiagCtx();

    s_DiskFlusher->SetRunnable();
    s_RecNoSaver->SetRunnable();
    s_SpaceShrinker->SetRunnable();
    s_ExpiredCleaner->SetRunnable();

    CNCServer::CachingCompleted();

    Terminate();
    return NULL;
}

CBlobCacher::CBlobCacher(void)
{
    SetState(&Me::x_StartCaching);
}

CBlobCacher::~CBlobCacher(void)
{}


void
CNCBlobStorage::CheckDiskSpace(void)
{
    Uint8 free_space = GetDiskFree();
    Uint8 allowed_db_size = GetAllowedDBSize(free_space);
    Uint8 cur_db_size = s_CurDBSize;

    if (s_IsStopWrite == eNoStop
        &&  cur_db_size * 100 >= allowed_db_size * s_WarnLimitOnPct)
    {
        ERR_POST(Critical << "ALERT! Database is too large. "
                 << "Current db size is " << g_ToSizeStr(cur_db_size)
                 << ", allowed db size is " << g_ToSizeStr(allowed_db_size) << ".");
        s_IsStopWrite = eStopWarning;
        Logging_DiskSpaceAlert();
    }

    if (s_IsStopWrite == eStopWarning) {
        if (s_StopWriteOnSize != 0  &&  cur_db_size >= s_StopWriteOnSize) {
            s_IsStopWrite = eStopDBSize;
            ERR_POST(Critical << "Database size exceeded its limit. "
                                 "Will no longer accept any writes from clients.");
        }
    }
    else if (s_IsStopWrite ==  eStopDBSize  &&  cur_db_size <= s_StopWriteOffSize)
    {
        s_IsStopWrite = eStopWarning;
    }
    if (free_space <= s_DiskCritical) {
        s_IsStopWrite = eStopDiskCritical;
        ERR_POST(Critical << "Free disk space is below CRITICAL threshold. "
                             "Will no longer accept any writes.");
    }
    else if (free_space <= s_DiskFreeLimit) {
        s_IsStopWrite = eStopDiskSpace;
        ERR_POST(Critical << "Free disk space is below threshold. "
                             "Will no longer accept any writes from clients.");
    }
    else if (s_IsStopWrite == eStopDiskSpace
             ||  s_IsStopWrite == eStopDiskCritical)
    {
        s_IsStopWrite = eStopWarning;
    }

    if (s_IsStopWrite == eStopWarning
        &&  cur_db_size * 100 < allowed_db_size * s_WarnLimitOffPct)
    {
        ERR_POST(Critical << "OK. Database is back to normal size. "
                 << "Current db size is " << g_ToSizeStr(cur_db_size)
                 << ", allowed db size is " << g_ToSizeStr(allowed_db_size) << ".");
        s_IsStopWrite = eNoStop;
    }
}


CExpiredCleaner::State
CExpiredCleaner::x_DeleteNextData(void)
{
    if (CTaskServer::IsInShutdown())
        return NULL;

    if (m_CurDelData >= m_CacheDatas.size()) {
        if (m_BatchSize < s_GCBatchSize)
            ++m_CurBucket;
        m_CacheDatas.clear();
        return &Me::x_CleanNextBucket;
    }

    SNCCacheData* cache_data = m_CacheDatas[m_CurDelData];

    cache_data->lock.Lock();
    CNCBlobVerManager* mgr = cache_data->ver_mgr;
    if (mgr) {
        mgr->ObtainReference();
        cache_data->lock.Unlock();

        mgr->DeleteDeadVersion(m_NextDead);
        mgr->Release();
    }
    else if (!cache_data->coord.empty()  &&  cache_data->dead_time < m_NextDead) {
        SNCDataCoord coord = cache_data->coord;
        Uint8 size = cache_data->size;
        Uint4 chunk_size = cache_data->chunk_size;
        Uint2 map_size = cache_data->map_size;
        cache_data->coord.clear();
        cache_data->dead_time = 0;
        CNCBlobStorage::ChangeCacheDeadTime(cache_data);
        cache_data->lock.Unlock();

        CSrvRef<SNCDBFileInfo> meta_file = s_GetDBFile(coord.file_id);
        SFileIndexRec* meta_ind = s_GetIndexRec(meta_file, coord.rec_num);
        s_CalcMetaAddress(meta_file, meta_ind);
        if (!meta_ind->chain_coord.empty()) {
            Uint1 map_depth = s_CalcMapDepth(size, chunk_size, map_size);
            s_MoveDataToGarbage(meta_ind->chain_coord, map_depth, coord);
        }
        s_MoveRecToGarbage(meta_file, meta_ind);
    }
    else {
        cache_data->lock.Unlock();
    }

    CNCBlobStorage::ReleaseCacheData(cache_data);

    ++m_CurDelData;
    SetRunnable();
    return NULL;
}

CExpiredCleaner::State
CExpiredCleaner::x_StartSession(void)
{
    m_StartTime = CSrvTime::CurSecs();
    m_NextDead = m_StartTime;
    if (m_DoExtraGC) {
        if (s_CurDBSize <= s_ExtraGCOffSize) {
            m_DoExtraGC = false;
        }
        else {
            ++m_ExtraGCTime;
            m_NextDead += m_ExtraGCTime;
        }
    }
    else if (s_ExtraGCOnSize != 0
             &&  s_CurDBSize >= s_ExtraGCOnSize)
    {
        m_DoExtraGC = true;
        m_ExtraGCTime = 1;
        m_NextDead += m_ExtraGCTime;
    }

    m_CurBucket = 1;
    return &Me::x_CleanNextBucket;
}

CExpiredCleaner::State
CExpiredCleaner::x_CleanNextBucket(void)
{
    if (CTaskServer::IsInShutdown())
        return NULL;
    // if all buckets clean, finish
    if (m_CurBucket > CNCDistributionConf::GetCntTimeBuckets())
        return &Me::x_FinishSession;

    m_BatchSize = 0;
    // s_TimeTables has blob info sorted by dead_time
    STimeTable* table = s_TimeTables[m_CurBucket];
    TTimeTableMap& time_map = table->time_map;
    table->lock.Lock();
    TTimeTableMap::iterator it = time_map.begin();
    SNCCacheData* last_data = NULL;
    for (; it != time_map.end(); ++m_BatchSize) {
        if (m_BatchSize >= s_GCBatchSize)
            break;

        SNCCacheData* cache_data = &*it;
        if (cache_data->saved_dead_time >= m_NextDead)
            break;

        int dead_time = ACCESS_ONCE(cache_data->dead_time);
        if (dead_time != 0) {
            // dead_time has changed, put blob back
            if (dead_time != cache_data->saved_dead_time) {
                time_map.erase(time_map.iterator_to(*cache_data));
                cache_data->saved_dead_time = dead_time;
                time_map.insert_equal(*cache_data);
                if (last_data) {
                    it = time_map.iterator_to(*last_data);
                    ++it;
                }
                else
                    it = time_map.begin();
                continue;
            }
            // increment ref counter
            CNCBlobStorage::ReferenceCacheData(cache_data);
            m_CacheDatas.push_back(cache_data);
        }
        last_data = cache_data;
        ++it;
    }
    table->lock.Unlock();

    if (m_BatchSize == 0) {
        // goto next bucket
        ++m_CurBucket;
        SetRunnable();
        return NULL;
    }

    m_CurDelData = 0;
    return &Me::x_DeleteNextData;
}

CExpiredCleaner::State
CExpiredCleaner::x_FinishSession(void)
{
    SetState(&Me::x_StartSession);
    if (!CTaskServer::IsInShutdown()) {
        if (CSrvTime::CurSecs() == m_StartTime)
            RunAfter(1);
        else
            SetRunnable();
    }
    return NULL;
}

CExpiredCleaner::CExpiredCleaner(void)
    : m_DoExtraGC(false)
{
    SetState(&Me::x_StartSession);
}

CExpiredCleaner::~CExpiredCleaner(void)
{}


CSpaceShrinker::State
CSpaceShrinker::x_MoveRecord(void)
{
    CSrvRef<SNCDBFileInfo> chain_file;
    SFileIndexRec* chain_ind = NULL;
    SFileChunkMapRec* map_rec = NULL;
    SFileChunkMapRec* up_map = NULL;
    Uint2 up_index = Uint2(-1);

    SNCDataCoord old_coord;
    old_coord.file_id = m_MaxFile->file_id;
    old_coord.rec_num = m_RecNum;

    SNCDataCoord new_coord;
    CSrvRef<SNCDBFileInfo> new_file;
    SFileIndexRec* new_ind;
    if (!s_GetNextWriteCoord(m_MaxFile->type_index, //ENCDBFileType(m_MaxFile->type_index + eFileIndexMoveShift),
                             m_IndRec->rec_size, new_coord, new_file, new_ind))
    {
        m_Failed = true;
        return &Me::x_FinishMoveRecord;
    }

    memcpy(new_file->file_map + new_ind->offset,
           m_MaxFile->file_map + m_IndRec->offset,
           m_IndRec->rec_size);

    new_ind->cache_data = m_CacheData;
    new_ind->rec_type = m_IndRec->rec_type;
    SNCDataCoord chain_coord = m_IndRec->chain_coord;
    new_ind->chain_coord = chain_coord;

    if (!chain_coord.empty()) {
        chain_file = s_GetDBFile(chain_coord.file_id);
        chain_ind = s_GetIndOrDeleted(chain_file, chain_coord.rec_num);
        if (s_IsIndexDeleted(chain_file, chain_ind)
            ||  chain_file->cnt_unfinished.Get() != 0)
        {
            goto wipe_new_record;
        }

        // Checks below are not just checks for corruption but also
        // an opportunity to fault in all database pages that will be necessary
        // to make all changes. This way we minimize the time that cache_data
        // lock will be held.
        switch (m_IndRec->rec_type) {
        case eFileRecMeta:
            if (chain_ind->chain_coord != old_coord
                &&  !s_IsIndexDeleted(m_MaxFile, m_IndRec))
            {
                DB_CORRUPTED("Meta with coord " << old_coord
                             << " links down to record with coord " << new_ind->chain_coord
                             << " but it has up coord " << chain_ind->chain_coord);
            }
            break;
        case eFileRecChunkMap:
            map_rec = s_CalcMapAddress(new_file, new_ind);
            up_index = map_rec->map_idx;
            Uint2 cnt_downs;
            cnt_downs = s_CalcCntMapDowns(new_ind->rec_size);
            for (Uint2 i = 0; i < cnt_downs; ++i) {
                SNCDataCoord down_coord = map_rec->down_coords[i];
                CSrvRef<SNCDBFileInfo> down_file = s_GetDBFile(down_coord.file_id);
                SFileIndexRec* down_ind = s_GetIndOrDeleted(down_file, down_coord.rec_num);
                if (s_IsIndexDeleted(down_file, down_ind))
                    goto wipe_new_record;
                if (down_ind->chain_coord != old_coord) {
                    DB_CORRUPTED("Map with coord " << old_coord
                                 << " links down to record with coord " << down_coord
                                 << " at index " << i
                                 << " but it has up coord " << down_ind->chain_coord);
                }
            }
            goto check_up_index;

        case eFileRecChunkData:
            SFileChunkDataRec* data_rec;
            data_rec = s_CalcChunkAddress(new_file, new_ind);
            up_index = data_rec->chunk_idx;
        check_up_index:
            if (chain_ind->rec_type == eFileRecChunkMap) {
                up_map = s_CalcMapAddress(chain_file, chain_ind);
                if (up_map->down_coords[up_index] != old_coord) {
                    DB_CORRUPTED("Record with coord " << old_coord
                                 << " links up to map with coord " << new_ind->chain_coord
                                 << " but at the index " << up_index
                                 << " it has coord " << up_map->down_coords[up_index]);
                }
            }
            else if (chain_ind->chain_coord != old_coord) {
                DB_CORRUPTED("Record with coord " << old_coord
                             << " links up to meta with coord " << new_ind->chain_coord
                             << " but it has down coord " << chain_ind->chain_coord);
            }
            break;
        }
    }

    if (!m_CurVer) {
        m_CacheData->lock.Lock();
        if (m_CacheData->ver_mgr
            ||  m_IndRec->chain_coord != chain_coord
            ||  (!chain_coord.empty()  &&  s_IsIndexDeleted(chain_file, chain_ind)
                 &&  !s_IsIndexDeleted(m_MaxFile, m_IndRec)))
        {
            m_Failed = true;
            goto unlock_and_wipe;
        }
        if (m_CacheData->coord.empty()
            ||  m_CacheData->dead_time <= CSrvTime::CurSecs() + s_MinMoveLife
            ||  s_IsIndexDeleted(m_MaxFile, m_IndRec))
        {
            goto unlock_and_wipe;
        }
    }

    switch (m_IndRec->rec_type) {
    case eFileRecMeta:
        if (m_CurVer) {
            if (m_CurVer->coord != old_coord) {
                // If coord for the version changed under us it's a bug.
                DB_CORRUPTED("Coord " << old_coord << " of the current version for blob "
                             << m_CacheData->key << " changed while move was in progress.");
            }
            m_CurVer->coord = new_coord;
        }
        else {
            if (m_CacheData->coord != old_coord) {
                // If there is no VerManager and still coord is something different
                // it's a bug.
                DB_CORRUPTED("Meta record with coord " << old_coord
                             << " points to cache_data with key " << m_CacheData->key
                             << " which has different coord " << m_CacheData->coord << ".");
            }
            m_CacheData->coord = new_coord;
        }
        if (chain_ind)
            chain_ind->chain_coord = new_coord;
        break;
    case eFileRecChunkMap:
        s_UpdateUpCoords(map_rec, new_ind, new_coord);
        if (m_CurVer)
            ++m_CurVer->map_move_counter;
        goto update_up_map;

    case eFileRecChunkData:
        if (m_CurVer) {
            SFileChunkDataRec* new_data = s_CalcChunkAddress(new_file, new_ind);
            m_CurVer->chunks[new_data->chunk_num] = (char*)new_data->chunk_data;
        }
    update_up_map:
        if (up_map) {
            up_map->down_coords[up_index] = new_coord;
        }
        else {
            chain_ind->chain_coord = new_coord;
            if (m_CurVer)
                m_CurVer->data_coord = new_coord;
        }
        break;
    }

    if (m_CurVer) {
        if (m_MovingMeta) {
            s_MoveRecToGarbage(m_MaxFile, m_IndRec);
        }
        else {
            CMovedRecDeleter* deleter = new CMovedRecDeleter(m_MaxFile, m_IndRec);
            deleter->CallRCU();
        }
    }
    else {
        m_CacheData->lock.Unlock();
        s_MoveRecToGarbage(m_MaxFile, m_IndRec);
    }
    new_file->cnt_unfinished.Add(-1);

    ++m_CntMoved;
    m_SizeMoved += m_IndRec->rec_size + sizeof(SFileIndexRec);

    return &Me::x_FinishMoveRecord;

unlock_and_wipe:
    m_CacheData->lock.Unlock();
wipe_new_record:
    new_ind->rec_type = eFileRecChunkData;
    new_ind->chain_coord.clear();
    s_MoveRecToGarbage(new_file, new_ind);
    new_file->cnt_unfinished.Add(-1);

    return &Me::x_FinishMoveRecord;
}

CSpaceShrinker::State
CSpaceShrinker::x_PrepareToShrink(void)
{
    if (CTaskServer::IsInShutdown())
        return NULL;

    int cur_time = CSrvTime::CurSecs();
    bool need_move = s_CurDBSize >= s_MinDBSize
                     &&  s_GarbageSize * 100 > s_CurDBSize * s_MaxGarbagePct;
    double max_pct = 0;
    Uint8 total_rel_used = 0;
    Uint8 total_rel_garb = 0;

    s_DBFilesLock.Lock();
    ITERATE(TNCDBFilesMap, it_file, (*s_DBFiles)) {
        SNCDBFileInfo* this_file = it_file->second.GetNCPointerOrNull();
        s_NextWriteLock.Lock();
        bool is_current = false;
        for (size_t i = 0; i < s_CntAllFiles; ++i) {
            if (this_file == s_AllWritings[i].cur_file
                ||  this_file == s_AllWritings[i].next_file)
            {
                is_current = true;
                break;
            }
        }
        s_NextWriteLock.Unlock();
        if (is_current  ||  this_file->cnt_unfinished.Get() != 0)
            continue;

        if (this_file->used_size == 0) {
            m_FilesToDel.push_back(SrvRef(this_file));
        }
        else if (need_move) {
            if (cur_time >= this_file->next_shrink_time) {
                this_file->info_lock.Lock();
                this_file->is_releasing = false;
                if (this_file->garb_size + this_file->used_size != 0) {
                    double this_pct = double(this_file->garb_size)
                                      / (this_file->garb_size + this_file->used_size);
                    if (this_pct > max_pct) {
                        max_pct = this_pct;
                        m_MaxFile = this_file;
                    }
                }
                this_file->info_lock.Unlock();
            }
            else if (this_file->is_releasing) {
                this_file->info_lock.Lock();
                total_rel_used += this_file->used_size;
                total_rel_garb += this_file->garb_size;
                this_file->info_lock.Unlock();
            }
        }
    }
    s_DBFilesLock.Unlock();

    Uint8 proj_garbage = s_GarbageSize - total_rel_garb;
    Uint8 proj_size = s_CurDBSize - (total_rel_garb + total_rel_used);
    if (need_move
        &&  (proj_garbage * 100 <= proj_size * s_MaxGarbagePct
             ||  max_pct * 100 <= s_MaxGarbagePct))
    {
        m_MaxFile = NULL;
    }

    m_CurDelFile = m_FilesToDel.begin();
    SetState(&Me::x_DeleteNextFile);
    SetRunnable();
    return NULL;
}

CSpaceShrinker::State
CSpaceShrinker::x_DeleteNextFile(void)
{
    if (CTaskServer::IsInShutdown())
        return NULL;

    if (m_CurDelFile == m_FilesToDel.end()) {
        m_FilesToDel.clear();
        return &Me::x_StartMoves;
    }

    s_DeleteDBFile(*m_CurDelFile);
    m_CurDelFile->Reset();
    ++m_CurDelFile;
    SetRunnable();
    return NULL;
}

CSpaceShrinker::State
CSpaceShrinker::x_StartMoves(void)
{
    m_StartTime = CSrvTime::CurSecs();
    bool need_move = s_CurDBSize >= s_MinDBSize
                     &&  s_GarbageSize * 100 > s_CurDBSize * s_MaxGarbagePct;
    if (!m_MaxFile  ||  !need_move)
        return &Me::x_FinishSession;

    CreateNewDiagCtx();
    CSrvDiagMsg().StartRequest()
                 .PrintParam("_type", "move")
                 .PrintParam("file_id", m_MaxFile->file_id);

    m_Failed = false;
    m_PrevRecNum = 0;
    m_LastAlive = 0;
    m_CntProcessed = 0;
    m_CntMoved = 0;
    m_SizeMoved = 0;

    return &Me::x_MoveNextRecord;
}

CSpaceShrinker::State
CSpaceShrinker::x_MoveNextRecord(void)
{
    if (CTaskServer::IsInShutdown())
        return &Me::x_FinishMoves;

    int cur_time = CSrvTime::CurSecs();
    m_MaxFile->info_lock.Lock();
    m_RecNum = m_LastAlive;
    m_IndRec = m_MaxFile->index_head - m_RecNum;
    if (m_RecNum != 0  &&  s_IsIndexDeleted(m_MaxFile, m_IndRec)) {
        m_LastAlive = m_RecNum = 0;
        m_IndRec = m_MaxFile->index_head;
    }
    do {
        if (m_RecNum > m_PrevRecNum)
            ++m_CntProcessed;
        if (m_IndRec->next_num == 0) {
            if (m_MaxFile->index_head->next_num == 0  &&  m_MaxFile->used_size != 0)
                abort();
            m_IndRec = NULL;
            break;
        }
        m_LastAlive = m_RecNum;
        m_RecNum = m_IndRec->next_num;
        m_IndRec = m_MaxFile->index_head - m_RecNum;
    }
    while (m_RecNum <= m_PrevRecNum
           ||  m_IndRec->cache_data->dead_time <= cur_time + s_MinMoveLife);
    m_MaxFile->info_lock.Unlock();
    if (!m_IndRec)
        return &Me::x_FinishMoves;

    m_CacheData = m_IndRec->cache_data;
    m_CacheData->lock.Lock();
    m_VerMgr = m_CacheData->ver_mgr;
    if (m_VerMgr) {
        m_VerMgr->ObtainReference();
        m_CacheData->lock.Unlock();

        m_VerMgr->RequestCurVersion(this);
        return &Me::x_CheckCurVersion;
    }
    m_CacheData->lock.Unlock();

    if (s_IsIndexDeleted(m_MaxFile, m_IndRec)) {
        SetRunnable();
        return NULL;
    }
    return &Me::x_MoveRecord;
}

SNCDataCoord
CSpaceShrinker::x_FindMetaCoord(SNCDataCoord coord, Uint1 max_map_depth)
{
    if (max_map_depth == 0  ||  coord.empty())
        return coord;

    CSrvRef<SNCDBFileInfo> file_info = s_GetDBFile(coord.file_id);
    SFileIndexRec* ind_rec = s_GetIndOrDeleted(file_info, coord.rec_num);
    if (s_IsIndexDeleted(file_info, ind_rec)  ||  ind_rec->rec_type == eFileRecMeta)
        return coord;

    return x_FindMetaCoord(ind_rec->chain_coord, max_map_depth - 1);
}

CSpaceShrinker::State
CSpaceShrinker::x_CheckCurVersion(void)
{
    if (!IsTransFinished())
        return NULL;
    if (CTaskServer::IsInShutdown())
        return &Me::x_FinishMoves;

    if (s_IsIndexDeleted(m_MaxFile, m_IndRec))
        return &Me::x_FinishMoveRecord;

    m_CurVer = m_VerMgr->GetCurVersion();
    if (!m_CurVer)
        return &Me::x_FinishMoveRecord;

    if (m_IndRec->rec_type == eFileRecChunkData) {
        SFileChunkDataRec* data_rec = s_CalcChunkAddress(m_MaxFile, m_IndRec);
        Uint8 chunk_num = data_rec->chunk_num;
        char* cur_chunk_ptr = NULL;
        if (m_CurVer->chunks.size() > chunk_num)
            cur_chunk_ptr = ACCESS_ONCE(m_CurVer->chunks[chunk_num]);
        if (cur_chunk_ptr) {
            if (cur_chunk_ptr != (char*)data_rec->chunk_data)
                return &Me::x_FinishMoveRecord;
            if (m_CurVer->coord.empty()) {
                m_Failed = true;
                return &Me::x_FinishMoveRecord;
            }
        }
        else {
            SNCDataCoord meta_coord = x_FindMetaCoord(m_IndRec->chain_coord,
                                                      m_CurVer->map_depth);
            if (meta_coord.empty()  ||  meta_coord != m_CurVer->coord)
                return &Me::x_FinishMoveRecord;
        }
    }
    else if (m_IndRec->rec_type == eFileRecChunkMap) {
        if (m_CurVer->map_depth == 0)
            return &Me::x_FinishMoveRecord;
        SNCDataCoord meta_coord = x_FindMetaCoord(m_IndRec->chain_coord,
                                                  m_CurVer->map_depth - 1);
        if (meta_coord.empty()  &&  m_CurVer->coord.empty()) {
            m_Failed = true;
            return &Me::x_FinishMoveRecord;
        }
        if (meta_coord.empty()  ||  meta_coord != m_CurVer->coord)
            return &Me::x_FinishMoveRecord;
    }
    else if (m_CurVer->coord.file_id != m_MaxFile->file_id
             ||  m_CurVer->coord.rec_num != m_RecNum)
    {
        return &Me::x_FinishMoveRecord;
    }
    else if (m_CurVer->move_or_rewrite
             ||  !AtomicCAS(m_CurVer->move_or_rewrite, false, true))
    {
        m_Failed = true;
        return &Me::x_FinishMoveRecord;
    }

    m_MovingMeta = m_IndRec->rec_type == eFileRecMeta;
    if (m_CurVer->dead_time < CSrvTime::CurSecs() + s_MinMoveLife)
        return &Me::x_FinishMoveRecord;
    else
        return &Me::x_MoveRecord;
}

CSpaceShrinker::State
CSpaceShrinker::x_FinishMoveRecord(void)
{
    if (m_VerMgr) {
        if (m_CurVer) {
            if (m_MovingMeta)
                m_CurVer->move_or_rewrite = false;
            m_CurVer.Reset();
        }
        m_VerMgr->Release();
        m_VerMgr = NULL;
    }

    ++m_CntProcessed;
    m_PrevRecNum = m_RecNum;

    if (m_Failed)
        SetState(&Me::x_FinishMoves);
    else
        SetState(&Me::x_MoveNextRecord);
    SetRunnable();
    return NULL;
}

CSpaceShrinker::State
CSpaceShrinker::x_FinishMoves(void)
{
    if (!m_Failed) {
        m_MaxFile->is_releasing = true;
        if (m_MaxFile->used_size == 0)
            s_DeleteDBFile(m_MaxFile);
        else if (m_CntProcessed == 0) {
            SRV_LOG(Warning, "Didn't find anything to process in the file");
            m_MaxFile->next_shrink_time = CSrvTime::CurSecs() + max(s_MinMoveLife, 300);
        }
        else
            m_MaxFile->next_shrink_time = CSrvTime::CurSecs() + s_MinMoveLife;
    }
    else {
        GetDiagCtx()->SetRequestStatus(eStatus_CmdAborted);
        m_MaxFile->next_shrink_time = CSrvTime::CurSecs() + s_FailedMoveDelay;
    }
    m_MaxFile.Reset();

    CSrvDiagMsg().PrintExtra()
                 .PrintParam("cnt_processed", m_CntProcessed)
                 .PrintParam("cnt_moved", m_CntMoved)
                 .PrintParam("size_moved", m_SizeMoved);
    CSrvDiagMsg().StopRequest();
    ReleaseDiagCtx();
    CNCStat::DBFileCleaned(!m_Failed, m_CntProcessed, m_CntMoved, m_SizeMoved);

    return &Me::x_FinishSession;
}

CSpaceShrinker::State
CSpaceShrinker::x_FinishSession(void)
{
    SetState(&Me::x_PrepareToShrink);
    if (!CTaskServer::IsInShutdown()) {
        if (CSrvTime::CurSecs() != m_StartTime)
            SetRunnable();
        else
            RunAfter(1);
    }
    return NULL;
}

CSpaceShrinker::CSpaceShrinker(void)
{
    SetState(&Me::x_PrepareToShrink);
}

CSpaceShrinker::~CSpaceShrinker(void)
{}


CMovedRecDeleter::CMovedRecDeleter(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
    : m_FileInfo(file_info),
      m_IndRec(ind_rec)
{}

CMovedRecDeleter::~CMovedRecDeleter(void)
{}

void
CMovedRecDeleter::ExecuteRCU(void)
{
    s_MoveRecToGarbage(m_FileInfo, m_IndRec);
    delete this;
}


CRecNoSaver::~CRecNoSaver(void)
{}

void
CRecNoSaver::ExecuteSlice(TSrvThreadNum /* thr_num */)
{

    if (s_NeedSavePurgeData) {
        s_NeedSavePurgeData = false;
        s_IndexLock.Lock();
        try {
            string forget = CNCBlobAccessor::GetPurgeData(';');
            INFO("Updated Purge data: " << forget);
            s_IndexDB->UpdatePurgeData(forget);
        }
        catch (CSQLITE_Exception&) {
        }
        s_IndexLock.Unlock();
    }

// max record number used in sync logs
    int cur_time = CSrvTime::CurSecs();
    int next_save = s_LastRecNoSaveTime + s_MinRecNoSavePeriod;
    if (!s_NeedSaveLogRecNo)
        next_save = cur_time + s_MinRecNoSavePeriod;
    else if (CTaskServer::IsInShutdown())
        next_save = cur_time;

    if (cur_time < next_save) {
        if (!CTaskServer::IsInShutdown())
            RunAfter(next_save - cur_time);
        return;
    }

    s_NeedSaveLogRecNo = false;
    Uint8 log_rec_no = CNCSyncLog::GetLastRecNo();
    s_IndexLock.Lock();
    try {
        s_IndexDB->SetMaxSyncLogRecNo(log_rec_no);
    }
    catch (CSQLITE_Exception& ex) {
        SRV_LOG(Critical, "Cannot save sync log record number: " << ex);
    }
    s_IndexLock.Unlock();

    s_LastRecNoSaveTime = cur_time;
    RunAfter(s_MinRecNoSavePeriod);
}


CDiskFlusher::CDiskFlusher(void)
{
    SetState(&Me::x_CheckFlushTime);
}

CDiskFlusher::~CDiskFlusher(void)
{}

CDiskFlusher::State
CDiskFlusher::x_CheckFlushTime(void)
{
    if (!s_FlushTimePeriod  ||  CTaskServer::IsInShutdown())
        return NULL;

    int cur_time = CSrvTime::CurSecs();
    if (cur_time < s_LastFlushTime + s_FlushTimePeriod) {
        RunAfter(s_LastFlushTime + s_FlushTimePeriod - cur_time);
        return NULL;
    }

    m_LastId = 0;
    return &Me::x_FlushNextFile;
}

CDiskFlusher::State
CDiskFlusher::x_FlushNextFile(void)
{
    if (CTaskServer::IsInShutdown())
        return NULL;

    s_DBFilesLock.Lock();
    TNCDBFilesMap::iterator file_it = s_DBFiles->upper_bound(m_LastId);
    if (file_it == s_DBFiles->end()) {
        s_DBFilesLock.Unlock();
        s_LastFlushTime = CSrvTime::CurSecs();
        return &Me::x_CheckFlushTime;
    }
    CSrvRef<SNCDBFileInfo> file_info = file_it->second;
    s_DBFilesLock.Unlock();

#ifdef NCBI_OS_LINUX
    if (msync(file_info->file_map, file_info->file_size, MS_SYNC)) {
        SRV_LOG(Critical, "Unable to sync file" << file_info->file_name
                          << ", errno=" << errno);
    }
#endif

    m_LastId = file_info->file_id;
    SetRunnable();
    return NULL;
}


CNewFileCreator::~CNewFileCreator(void)
{}

void
CNewFileCreator::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// create new (next) db file
    for (Uint1 i = 0; i < s_CntAllFiles; ++i) {
        if (s_AllWritings[i].next_file == NULL) {
            s_CreateNewFile(i);
            SetRunnable();
            break;
        }
    }
}


void
SNCCacheData::ExecuteRCU(void)
{
    delete this;
}

END_NCBI_SCOPE
