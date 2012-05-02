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
static const Uint8 kMapsSignature = NCBI_CONST_UINT8(0xba6efd7b61fdff6c);
static const Uint1 kSignatureSize = sizeof(kMetaSignature);


/// Size of memory page that is a granularity of all allocations from OS.
static const size_t kMemPageSize  = 4 * 1024;
/// Mask that can move pointer address or memory size to the memory page
/// boundary.
static const size_t kMemPageAlignMask = ~(kMemPageSize - 1);



#define DB_CORRUPTED(msg)                                                   \
    do {                                                                    \
        ERR_POST(Fatal << "Database is corrupted. " << msg                  \
                 << " Bug, faulty disk or somebody writes to database?");   \
        abort();                                                            \
    } while (0)                                                             \
/**/



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
    m_MaxGarbagePct = reg.GetInt(kNCStorage_RegSection, kNCStorage_GarbagePctParam, 80);
    str = reg.GetString(kNCStorage_RegSection, kNCStorage_MinDBSizeParam, "1Gb");
    m_MinDBSize = NStr::StringToUInt8_DataSize(str);
    m_MinMoveLife = reg.GetInt(kNCStorage_RegSection, kNCStorage_MoveLifeParam, 0);
    m_FailedMoveDelay = reg.GetInt(kNCStorage_RegSection, kNCStorage_FailedMoveParam, 10);

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

    int warn_pct = reg.GetInt(kNCStorage_RegSection, "db_limit_percentage_alert", 65);
    if (warn_pct <= 0  ||  warn_pct >= 100) {
        ERR_POST("Parameter db_limit_percentage_alert has wrong value " << warn_pct
                 << ". Assuming it's 65.");
        warn_pct = 65;
    }
    m_WarnLimitOnPct = Uint1(warn_pct);
    warn_pct = reg.GetInt(kNCStorage_RegSection, "db_limit_percentage_alert_delta", 5);
    if (warn_pct <= 0  ||  warn_pct >= m_WarnLimitOnPct) {
        ERR_POST("Parameter db_limit_percentage_alert_delta has wrong value "
                 << warn_pct << ". Assuming it's 5.");
        warn_pct = 5;
    }
    m_WarnLimitOffPct = m_WarnLimitOnPct - Uint1(warn_pct);

    return true;
}

bool
CNCBlobStorage::x_ReadStorageParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_Path = reg.Get(kNCStorage_RegSection, kNCStorage_PathParam);
    m_Prefix = reg.Get(kNCStorage_RegSection, kNCStorage_FilePrefixParam);
    if (m_Path.empty()  ||  m_Prefix.empty()) {
        ERR_POST(Critical <<  "Incorrect parameters for " << kNCStorage_PathParam
                          << " and " << kNCStorage_FilePrefixParam
                          << " in the section '" << kNCStorage_RegSection << "': '"
                          << m_Path << "' and '" << m_Prefix << "'");
        return false;
    }
    if (!x_EnsureDirExist(m_Path))
        return false;
    m_GuardName = reg.Get(kNCStorage_RegSection, kNCStorage_GuardNameParam);
    if (m_GuardName.empty()) {
        m_GuardName = CDirEntry::MakePath(m_Path,
                                          kNCStorage_StartedFileName,
                                          m_Prefix);
    }
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
    case eDBFileMaps:
        file_name += kNCStorage_MapsFileSuffix;
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
                CNCRef<SNCDBFileInfo> info = it->second;
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

inline bool
CNCBlobStorage::x_IsIndexDeleted(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    Uint4 rec_num = Uint4(file_info->index_head - ind_rec);
    return ind_rec->next_num == rec_num  ||  ind_rec->prev_num == rec_num;
}

SFileIndexRec*
CNCBlobStorage::x_GetIndOrDeleted(SNCDBFileInfo* file_info, Uint4 rec_num)
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
    Uint4 next_num = *(volatile Uint4*)(&ind_rec->next_num);
    if ((next_num != 0  &&  next_num < rec_num)  ||  ind_rec->prev_num > rec_num)
    {
        DB_CORRUPTED("Index record " << rec_num
                     << " in file " << file_info->file_name
                     << " contains bad next_num " << next_num
                     << " and/or prev_num " << ind_rec->prev_num << ".");
    }
    return ind_rec;
}

SFileIndexRec*
CNCBlobStorage::x_GetIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = x_GetIndOrDeleted(file_info, rec_num);
    if (x_IsIndexDeleted(file_info, ind_rec)) {
        DB_CORRUPTED("Index record " << rec_num
                     << " in file " << file_info->file_name
                     << " has been deleted.");
    }
    return ind_rec;
}

void
CNCBlobStorage::x_DeleteIndexRec(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    SFileIndexRec* prev_rec;
    SFileIndexRec* next_rec;
    if (ind_rec->prev_num == 0)
        prev_rec = file_info->index_head;
    else
        prev_rec = x_GetIndexRec(file_info, ind_rec->prev_num);
    if (ind_rec->next_num == 0)
        next_rec = file_info->index_head;
    else
        next_rec = x_GetIndexRec(file_info, ind_rec->next_num);
    // These should be in the exactly this order to prevent unrecoverable
    // corruption.
    *(volatile Uint4*)&prev_rec->next_num = ind_rec->next_num;
    *(volatile Uint4*)&next_rec->prev_num = ind_rec->prev_num;
    ind_rec->next_num = ind_rec->prev_num = Uint4(file_info->index_head - ind_rec);
}

inline void
CNCBlobStorage::x_DeleteIndexRec(SNCDBFileInfo* file_info, Uint4 rec_num)
{
    SFileIndexRec* ind_rec = x_GetIndexRec(file_info, rec_num);
    x_DeleteIndexRec(file_info, ind_rec);
}

void
CNCBlobStorage::x_MoveRecToGarbage(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    Uint4 size = ind_rec->rec_size;
    if (size & 7)
        size += 8 - (size & 7);
    size += sizeof(SFileIndexRec);

    file_info->info_lock.Lock();
    x_DeleteIndexRec(file_info, ind_rec);
    if (file_info->used_size < size)
        abort();
    file_info->used_size -= size;
    file_info->garb_size += size;
    if (file_info->garb_size > file_info->file_size)
        abort();
    if (file_info->index_head->next_num == 0  &&  file_info->used_size != 0)
        abort();
    file_info->info_lock.Unlock();

    m_GarbageLock.Lock();
    m_GarbageSize += size;
    m_GarbageLock.Unlock();
}

CNCRef<SNCDBFileInfo>
CNCBlobStorage::x_GetDBFile(Uint4 file_id)
{
    m_DBFilesLock.Lock();
    TNCDBFilesMap::const_iterator it = m_DBFiles.find(file_id);
    if (it == m_DBFiles.end()) {
        DB_CORRUPTED("Cannot find file " << file_id);
    }
    CNCRef<SNCDBFileInfo> file_info = it->second;
    m_DBFilesLock.Unlock();

    return file_info;
}

inline Uint1
CNCBlobStorage::x_CalcMapDepthImpl(Uint8 size, Uint4 chunk_size, Uint2 map_size)
{
    Uint1 map_depth = 0;
    Uint8 cnt_chunks = (size + chunk_size - 1) / chunk_size;
    while (cnt_chunks > 1  &&  map_depth <= kNCMaxBlobMapsDepth) {
        ++map_depth;
        cnt_chunks = (cnt_chunks + map_size - 1) / map_size;
    }
    return map_depth;
}

Uint1
CNCBlobStorage::x_CalcMapDepth(Uint8 size, Uint4 chunk_size, Uint2 map_size)
{
    Uint1 map_depth = x_CalcMapDepthImpl(size, chunk_size, map_size);
    if (map_depth > kNCMaxBlobMapsDepth) {
        DB_CORRUPTED("Size parameters are resulted in bad map_depth"
                     << ", size=" << size
                     << ", chunk_size=" << chunk_size
                     << ", map_size=" << map_size);
    }
    return map_depth;
}

inline Uint2
CNCBlobStorage::x_CalcCntMapDowns(Uint4 rec_size)
{
    SFileChunkMapRec map_rec;
    return Uint2((SNCDataCoord*)((char*)&map_rec + rec_size) - map_rec.down_coords);
}

inline size_t
CNCBlobStorage::x_CalcChunkDataSize(Uint4 rec_size)
{
    SFileChunkDataRec data_rec;
    return (Uint1*)((char*)&data_rec + rec_size) - data_rec.chunk_data;
}

inline Uint4
CNCBlobStorage::x_CalcMetaRecSize(Uint2 key_size)
{
    SFileMetaRec meta_rec;
    return Uint4((char*)&meta_rec.key_data[key_size] - (char*)&meta_rec);
}

inline Uint4
CNCBlobStorage::x_CalcMapRecSize(Uint2 cnt_downs)
{
    SFileChunkMapRec map_rec;
    return Uint4((char*)&map_rec.down_coords[cnt_downs] - (char*)&map_rec);
}

inline Uint4
CNCBlobStorage::x_CalcChunkRecSize(size_t data_size)
{
    SFileChunkDataRec data_rec;
    return Uint4((char*)&data_rec.chunk_data[data_size] - (char*)&data_rec);
}

char*
CNCBlobStorage::x_CalcRecordAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
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

SFileMetaRec*
CNCBlobStorage::x_CalcMetaAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecMeta) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecMeta) << ".");
    }
    SFileMetaRec* meta_rec = (SFileMetaRec*)x_CalcRecordAddress(file_info, ind_rec);
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

SFileChunkMapRec*
CNCBlobStorage::x_CalcMapAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecChunkMap) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecChunkMap) << ".");
    }
    char* rec_ptr = x_CalcRecordAddress(file_info, ind_rec);
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

SFileChunkDataRec*
CNCBlobStorage::x_CalcChunkAddress(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec)
{
    if (ind_rec->rec_type != eFileRecChunkData) {
        DB_CORRUPTED("Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " has wrong type " << int(ind_rec->rec_type)
                     << " when should be " << int(eFileRecChunkData) << ".");
    }
    char* rec_ptr = x_CalcRecordAddress(file_info, ind_rec);
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

bool
CNCBlobStorage::x_CreateNewFile(size_t type_idx)
{
    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    Uint4 file_id;
    if (m_LastFileId == kNCMaxDBFileId)
        file_id = 1;
    else
        file_id = m_LastFileId + 1;
    //size_t true_type_idx = type_idx - eFileIndexMoveShift;
    size_t true_type_idx = type_idx;
    string file_name = x_GetFileName(file_id, s_AllFileTypes[true_type_idx]);
    SNCDBFileInfo* file_info = new SNCDBFileInfo();
    file_info->file_id      = file_id;
    file_info->file_name    = file_name;
    file_info->create_time  = int(time(NULL));
    file_info->file_size    = m_NewFileSize;
    file_info->file_type    = s_AllFileTypes[true_type_idx];
    file_info->type_index   = EDBFileIndex(true_type_idx);

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
    file_info->index_head->next_num = file_info->index_head->prev_num = 0;

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

    m_DBFilesLock.Lock();
    m_DBFiles[file_id] = file_info;

    m_NextWriteLock.Lock();
    m_AllWritings[type_idx].next_file = file_info;
    m_CurDBSize += kSignatureSize + sizeof(SFileIndexRec);
    if (m_NextWaiters != 0)
        m_NextWait.SignalAll();
    m_NextWriteLock.Unlock();

    m_DBFilesLock.Unlock();

    return true;
}

void
CNCBlobStorage::x_DeleteDBFile(const CNCRef<SNCDBFileInfo>& file_info)
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
      m_CntMoveFiles(0),
      m_CntFailedMoves(0),
      m_MovedSize(0),
      m_LastFlushTime(0),
      m_FreeAccessors(NULL),
      m_CntUsedHolders(0),
      m_IsStopWrite(eNoStop),
      m_DoExtraGC(false),
      m_LastRecNoSaveTime(0),
      m_ExtraGCTime(0)
{
    for (size_t i = 0; i < s_CntAllFiles; ++i) {
        m_AllWritings[i].cur_file = m_AllWritings[i].next_file = NULL;
    }
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

    for (Uint2 i = 1; i <= CNCDistributionConf::GetCntTimeBuckets(); ++i) {
        m_BucketsCache[i] = new SBucketCache(i);
        m_TimeTables[i] = new STimeTable();
    }

    m_BlobCounter.Set(0);
    if (!m_DBFiles.empty()) {
        int max_create_time = 0;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CNCRef<SNCDBFileInfo> file_info = it->second;
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
    m_NextFileSwitch.SignalAll();

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
CNCBlobStorage::x_GetAccessor(void)
{
    m_HoldersPoolLock.Lock();
    CNCBlobAccessor* holder = m_FreeAccessors;
    if (holder) {
        holder->RemoveFromList(m_FreeAccessors);
    }
    else {
        holder = new CNCBlobAccessor();
    }
    ++m_CntUsedHolders;
    m_HoldersPoolLock.Unlock();

    return holder;
}

void
CNCBlobStorage::ReturnAccessor(CNCBlobAccessor* holder)
{
    m_HoldersPoolLock.Lock();
    holder->AddToList(m_FreeAccessors);
    --m_CntUsedHolders;
    m_HoldersPoolLock.Unlock();
}

CNCBlobStorage::SBucketCache*
CNCBlobStorage::x_GetBucketCache(Uint2 bucket)
{
    TBucketCacheMap::const_iterator it = m_BucketsCache.find(bucket);
    if (it == m_BucketsCache.end())
        abort();
    return it->second;
}

SNCCacheData*
CNCBlobStorage::x_GetKeyCacheData(Uint2 time_bucket,
                                  const string& key,
                                  bool need_create)
{
    SBucketCache* cache = x_GetBucketCache(time_bucket);
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
        }
        else {
            data = &*ins_res.first;
            data->key_del_time = 0;
        }
    }
    else {
        TKeyMap::iterator it = cache->key_map.find(key, SCacheKeyCompare());
        if (it == cache->key_map.end()) {
            data = NULL;
        }
        else {
            data = &*it;
            if (data->key_del_time != 0)
                data = NULL;
        }
    }
    cache->lock.Unlock();
    return data;
}

void
CNCBlobStorage::x_InitializeAccessor(CNCBlobAccessor* acessor)
{
    const string& key = acessor->GetBlobKey();
    Uint2 time_bucket = acessor->GetTimeBucket();
    bool need_create = acessor->GetAccessType() == eNCCreate
                       ||  acessor->GetAccessType() == eNCCopyCreate;
    SNCCacheData* data = x_GetKeyCacheData(time_bucket, key, need_create);
    acessor->Initialize(data);
}

CNCBlobAccessor*
CNCBlobStorage::GetBlobAccess(ENCAccessType access,
                              const string& key,
                              const string& password,
                              Uint2         time_bucket)
{
    CNCBlobAccessor* accessor = x_GetAccessor();
    accessor->Prepare(key, password, time_bucket, access);
    x_InitializeAccessor(accessor);
    return accessor;
}

void
CNCBlobStorage::DeleteBlobKey(Uint2 time_bucket, const string& key)
{
    SBucketCache* cache = x_GetBucketCache(time_bucket);
    cache->lock.Lock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (!data->coord.empty()) {
        cache->lock.Unlock();
        return;
    }
    data->key_del_time = int(time(NULL));
    cache->lock.Unlock();
    cache->deleter.AddElement(key);
}

void
CNCBlobStorage::RestoreBlobKey(Uint2 time_bucket,
                               const string& key,
                               SNCCacheData* cache_data)
{
    SBucketCache* cache = x_GetBucketCache(time_bucket);
    cache->lock.Lock();
    SNCCacheData* data = &*cache->key_map.find(key, SCacheKeyCompare());
    if (data == cache_data)
        data->key_del_time = 0;
    /*else
        abort();*/
    cache->lock.Unlock();
}

bool
CNCBlobStorage::IsBlobExists(Uint2 time_bucket, const string& key)
{
    SBucketCache* cache = x_GetBucketCache(time_bucket);
    cache->lock.Lock();
    TKeyMap::const_iterator it = cache->key_map.find(key, SCacheKeyCompare());
    bool result = it != cache->key_map.end()  &&  !it->coord.empty()
                  &&  it->expire > int(time(NULL));
    cache->lock.Unlock();
    return result;
}

inline void
CNCBlobStorage::x_SwitchToNextFile(SWritingInfo& w_info)
{
    w_info.cur_file = w_info.next_file;
    w_info.next_file = NULL;
    w_info.next_rec_num = 1;
    w_info.next_offset = kSignatureSize;
    w_info.left_file_size = w_info.cur_file->file_size
                             - (kSignatureSize + sizeof(SFileIndexRec));
}

bool
CNCBlobStorage::x_GetNextWriteCoord(EDBFileIndex file_index,
                                    Uint4 rec_size,
                                    SNCDataCoord& coord,
                                    CNCRef<SNCDBFileInfo>& file_info,
                                    SFileIndexRec*& ind_rec)
{
    Uint4 true_rec_size = rec_size;
    if (true_rec_size & 7)
        true_rec_size += 8 - (true_rec_size & 7);
    Uint4 reserve_size = true_rec_size + sizeof(SFileIndexRec);

    bool need_signal_switch = false;
    SWritingInfo& w_info = m_AllWritings[file_index];
    CAbsTimeout timeout(m_MaxIOWaitTime);

    m_NextWriteLock.Lock();
    while (w_info.left_file_size < reserve_size  &&  w_info.next_file == NULL)
    {
        ++m_NextWaiters;
        while (w_info.next_file == NULL) {
            if (!m_NextWait.WaitForSignal(m_NextWriteLock, timeout)) {
                --m_NextWaiters;
                m_NextWriteLock.Unlock();
                return false;
            }
        }
        --m_NextWaiters;
    }
    if (w_info.left_file_size < reserve_size) {
        m_CurDBSize += w_info.left_file_size;
        m_GarbageSize += w_info.left_file_size;
        w_info.cur_file->info_lock.Lock();
        w_info.cur_file->garb_size += w_info.left_file_size;
        if (w_info.cur_file->used_size
                + w_info.cur_file->garb_size >= w_info.cur_file->file_size)
        {
            abort();
        }
        w_info.cur_file->info_lock.Unlock();

        x_SwitchToNextFile(w_info);
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
    m_CurDBSize += reserve_size;

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
    m_NextWriteLock.Unlock();

    if (need_signal_switch)
        m_NextFileSwitch.SignalAll();

    return true;
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(!ver_data->coord.empty());
    CNCRef<SNCDBFileInfo> file_info = x_GetDBFile(ver_data->coord.file_id);
    SFileIndexRec* ind_rec = x_GetIndexRec(file_info, ver_data->coord.rec_num);
    SFileMetaRec* meta_rec = x_CalcMetaAddress(file_info, ind_rec);
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

    ver_data->map_depth = x_CalcMapDepth(ver_data->size,
                                         ver_data->chunk_size,
                                         ver_data->map_size);
    return true;
}

void
CNCBlobStorage::x_UpdateUpCoords(SFileChunkMapRec* map_rec,
                                 SFileIndexRec* map_ind,
                                 SNCDataCoord coord)
{
    Uint2 cnt_downs = x_CalcCntMapDowns(map_ind->rec_size);
    for (Uint2 i = 0; i < cnt_downs; ++i) {
        SNCDataCoord down_coord = map_rec->down_coords[i];
        CNCRef<SNCDBFileInfo> file_info = x_GetDBFile(down_coord.file_id);
        SFileIndexRec* ind_rec = x_GetIndexRec(file_info, down_coord.rec_num);
        ind_rec->chain_coord = coord;
    }
}

bool
CNCBlobStorage::x_SaveOneMapImpl(SNCChunkMapInfo* save_map,
                                 SNCChunkMapInfo* up_map,
                                 Uint2 cnt_downs,
                                 Uint2 map_size,
                                 Uint1 map_depth,
                                 SNCCacheData* cache_data)
{
    SNCDataCoord map_coord;
    CNCRef<SNCDBFileInfo> map_file;
    SFileIndexRec* map_ind;
    Uint4 rec_size = x_CalcMapRecSize(cnt_downs);
    if (!x_GetNextWriteCoord(eFileIndexMaps, rec_size, map_coord, map_file, map_ind))
        return false;

    map_ind->rec_type = eFileRecChunkMap;
    map_ind->cache_data = cache_data;
    SFileChunkMapRec* map_rec = x_CalcMapAddress(map_file, map_ind);
    map_rec->map_idx = save_map->map_idx;
    map_rec->map_depth = map_depth;
    size_t coords_size = cnt_downs * sizeof(map_rec->down_coords[0]);
    memcpy(map_rec->down_coords, save_map->coords, coords_size);

    up_map->coords[save_map->map_idx] = map_coord;
    ++save_map->map_idx;
    memset(save_map->coords, 0, map_size * sizeof(save_map->coords[0]));
    x_UpdateUpCoords(map_rec, map_ind, map_coord);

    map_file->cnt_unfinished.Add(-1);

    return true;
}

bool
CNCBlobStorage::x_SaveChunkMap(SNCBlobVerData* ver_data,
                               SNCChunkMapInfo** maps,
                               Uint2 cnt_downs,
                               bool save_all_deps)
{
    SNCCacheData* cache_data = ver_data->manager->GetCacheData();
    if (!x_SaveOneMapImpl(maps[0], maps[1], cnt_downs, ver_data->map_size, 1, cache_data))
        return false;

    Uint1 cur_level = 0;
    while (cur_level < kNCMaxBlobMapsDepth
           &&  (maps[cur_level]->map_idx == ver_data->map_size
                ||  (maps[cur_level]->map_idx > 1  &&  save_all_deps)))
    {
        cnt_downs = maps[cur_level]->map_idx;
        ++cur_level;
        if (!x_SaveOneMapImpl(maps[cur_level], maps[cur_level + 1], cnt_downs,
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
                              SNCChunkMapInfo** maps,
                              Uint8 cnt_chunks)
{
    SNCDataCoord old_coord = ver_data->coord;
    SNCDataCoord down_coord = ver_data->data_coord;
    if (old_coord.empty()) {
        if (ver_data->size > ver_data->chunk_size) {
            Uint2 last_chunks_cnt = Uint2((cnt_chunks - 1) % ver_data->map_size) + 1;
            if (!x_SaveChunkMap(ver_data, maps, last_chunks_cnt, true))
                return false;

            for (Uint1 i = kNCMaxBlobMapsDepth; i > 0; --i) {
                if (!maps[i]->coords[0].empty()) {
                    down_coord = maps[i]->coords[0];
                    maps[i]->coords[0].clear();
                    ver_data->map_depth = i;
                    break;
                }
            }
            if (down_coord.empty())
                abort();
        }
        else {
            down_coord = maps[0]->coords[0];
            maps[0]->coords[0].clear();
            ver_data->map_depth = 0;
        }
    }

    Uint2 key_size = Uint2(blob_key.size());
    if (!ver_data->password.empty())
        key_size += 16;
    Uint4 rec_size = x_CalcMetaRecSize(key_size);
    SNCDataCoord meta_coord;
    CNCRef<SNCDBFileInfo> meta_file;
    SFileIndexRec* meta_ind;
    if (!x_GetNextWriteCoord(eFileIndexMeta, rec_size, meta_coord, meta_file, meta_ind))
        return false;

    meta_ind->rec_type = eFileRecMeta;
    meta_ind->cache_data = ver_data->manager->GetCacheData();
    meta_ind->chain_coord = down_coord;

    SFileMetaRec* meta_rec = x_CalcMetaAddress(meta_file, meta_ind);
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
        CNCRef<SNCDBFileInfo> down_file = x_GetDBFile(down_coord.file_id);
        SFileIndexRec* down_ind = x_GetIndexRec(down_file, down_coord.rec_num);
        down_ind->chain_coord = meta_coord;
    }

    if (!old_coord.empty()) {
        CNCRef<SNCDBFileInfo> old_file = x_GetDBFile(old_coord.file_id);
        SFileIndexRec* old_ind = x_GetIndexRec(old_file, old_coord.rec_num);
        // Check that meta-data wasn't corrupted.
        x_CalcMetaAddress(old_file, old_ind);
        x_MoveRecToGarbage(old_file, old_ind);
    }

    meta_file->cnt_unfinished.Add(-1);

    return true;
}

void
CNCBlobStorage::x_MoveDataToGarbage(SNCDataCoord coord, Uint1 map_depth)
{
    CNCRef<SNCDBFileInfo> file_info = x_GetDBFile(coord.file_id);
    SFileIndexRec* ind_rec = x_GetIndexRec(file_info, coord.rec_num);
    if (map_depth == 0) {
        x_CalcChunkAddress(file_info, ind_rec);
        x_MoveRecToGarbage(file_info, ind_rec);
    }
    else {
        SFileChunkMapRec* map_rec = x_CalcMapAddress(file_info, ind_rec);
        Uint2 cnt_downs = x_CalcCntMapDowns(ind_rec->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            x_MoveDataToGarbage(map_rec->down_coords[i], map_depth - 1);
        }
        x_MoveRecToGarbage(file_info, ind_rec);
    }
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data,
                               SNCChunkMapInfo** maps)
{
    if (!ver_data->coord.empty()) {
        CNCRef<SNCDBFileInfo> meta_file = x_GetDBFile(ver_data->coord.file_id);
        SFileIndexRec* meta_ind = x_GetIndexRec(meta_file, ver_data->coord.rec_num);
        x_CalcMetaAddress(meta_file, meta_ind);
        if (ver_data->size != 0)
            x_MoveDataToGarbage(ver_data->data_coord, ver_data->map_depth);
        x_MoveRecToGarbage(meta_file, meta_ind);
    }
    else if (maps != NULL) {
        for (Uint1 depth = 0; depth <= kNCMaxBlobMapsDepth; ++depth) {
            Uint2 idx = 0;
            while (idx < ver_data->map_size
                   &&  !maps[depth]->coords[idx].empty())
            {
                x_MoveDataToGarbage(maps[depth]->coords[idx], depth);
                ++idx;
            }
        }
    }
}

bool
CNCBlobStorage::x_ReadMapInfo(SNCDataCoord map_coord,
                              SNCChunkMapInfo* map_info,
                              Uint1 map_depth)
{
    CNCRef<SNCDBFileInfo> map_file = x_GetDBFile(map_coord.file_id);
    SFileIndexRec* map_ind = x_GetIndexRec(map_file, map_coord.rec_num);
    SFileChunkMapRec* map_rec = x_CalcMapAddress(map_file, map_ind);
    if (map_rec->map_idx != map_info->map_idx  ||  map_rec->map_depth != map_depth)
    {
        DB_CORRUPTED("Map at coord " << map_coord
                     << " has wrong index " << map_rec->map_idx
                     << " and/or depth " << map_rec->map_depth
                     << " when it should be " << map_info->map_idx
                     << " and " << map_depth << ".");
    }
    memcpy(map_info->coords, map_rec->down_coords,
           x_CalcCntMapDowns(map_ind->rec_size) * sizeof(map_rec->down_coords[0]));
    return true;
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
        if (maps[depth]->map_idx == this_index  &&  !maps[depth]->coords[0].empty())
            continue;

        SNCDataCoord map_coord;
        if (depth + 1 == ver_data->map_depth)
            map_coord = ver_data->data_coord;
        else
            map_coord = maps[depth + 1]->coords[this_index];

        maps[depth]->map_idx = this_index;
        x_ReadMapInfo(map_coord, maps[depth], depth + 1);

        while (depth > 0) {
            this_index = map_idx[depth];
            map_coord = maps[depth]->coords[this_index];
            --depth;
            maps[depth]->map_idx = this_index;
            x_ReadMapInfo(map_coord, maps[depth], depth + 1);
        }
    }

    SNCDataCoord chunk_coord;
    if (ver_data->map_depth == 0) {
        if (chunk_num != 0)
            abort();
        chunk_coord = ver_data->data_coord;
    }
    else
        chunk_coord = maps[0]->coords[map_idx[0]];

    CNCRef<SNCDBFileInfo> data_file = x_GetDBFile(chunk_coord.file_id);
    SFileIndexRec* data_ind = x_GetIndexRec(data_file, chunk_coord.rec_num);
    SFileChunkDataRec* data_rec = x_CalcChunkAddress(data_file, data_ind);
    if (data_rec->chunk_num != chunk_num  ||  data_rec->chunk_idx != map_idx[0])
    {
        ERR_POST(Critical << "File " << data_file->file_name
                 << " in chunk record " << chunk_coord.rec_num
                 << " has wrong chunk number " << data_rec->chunk_num
                 << " and/or chunk index " << data_rec->chunk_idx
                 << ". Deleting blob.");
        return false;
    }

    size_t data_size = x_CalcChunkDataSize(data_ind->rec_size);
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
    while (level_num != 0  &&  cur_index < kNCMaxBlobMapsDepth);
    if (level_num != 0  &&  cur_index >= kNCMaxBlobMapsDepth) {
        ERR_POST("Chunk number " << chunk_num
                 << " exceeded maximum map depth " << kNCMaxBlobMapsDepth
                 << " with map_size=" << ver_data->map_size << ".");
        return false;
    }

    if (map_idx[1] != maps[0]->map_idx) {
        if (!x_SaveChunkMap(ver_data, maps, ver_data->map_size, false))
            return false;
        for (Uint1 i = 0; i < kNCMaxBlobMapsDepth - 1; ++i)
            maps[i]->map_idx = map_idx[i + 1];
    }

    SNCDataCoord data_coord;
    CNCRef<SNCDBFileInfo> data_file;
    SFileIndexRec* data_ind;
    Uint4 rec_size = x_CalcChunkRecSize(data->GetSize());
    if (!x_GetNextWriteCoord(eFileIndexData, rec_size, data_coord, data_file, data_ind))
        return false;

    data_ind->rec_type = eFileRecChunkData;
    data_ind->cache_data = ver_data->manager->GetCacheData();
    SFileChunkDataRec* data_rec = x_CalcChunkAddress(data_file, data_ind);
    data_rec->chunk_num = chunk_num;
    data_rec->chunk_idx = map_idx[0];
    memcpy(data_rec->chunk_data, data->GetData(), data->GetSize());

    maps[0]->coords[map_idx[0]] = data_coord;
    CNCStat::AddChunkWritten(data->GetSize());

    data_file->cnt_unfinished.Add(-1);

    return true;
}

void
CNCBlobStorage::ChangeCacheDeadTime(SNCCacheData* cache_data,
                                    SNCDataCoord new_coord,
                                    int new_dead_time)
{
    STimeTable* table = m_TimeTables[cache_data->time_bucket];
    table->lock.Lock();
    if (cache_data->dead_time != 0) {
        table->time_map.erase(*cache_data);
        AtomicSub(m_CurBlobsCnt, 1);
    }
    cache_data->coord = new_coord;
    cache_data->dead_time = new_dead_time;
    if (new_dead_time != 0) {
        cache_data->dead_time = new_dead_time;
        table->time_map.insert_equal(*cache_data);
        AtomicAdd(m_CurBlobsCnt, 1);
    }
    table->lock.Unlock();
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
    SNCDataCoord coord;
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
    Uint2 slot_buckets = CNCDistributionConf::GetCntSlotBuckets();
    Uint2 bucket_num = (slot - 1) * slot_buckets + 1;
    for (Uint2 i = 0; i < slot_buckets; ++i, ++bucket_num) {
        SBucketCache* cache = x_GetBucketCache(bucket_num);

        cache->lock.Lock();
        Uint8 cnt_blobs = cache->key_map.size();
        void* big_block = malloc(size_t(cnt_blobs * sizeof(SNCTempBlobInfo)));
        SNCTempBlobInfo* info_ptr = (SNCTempBlobInfo*)big_block;

        ITERATE(TKeyMap, it, cache->key_map) {
            new (info_ptr) SNCTempBlobInfo(*it);
            ++info_ptr;
        }
        cache->lock.Unlock();

        info_ptr = (SNCTempBlobInfo*)big_block;
        for (Uint8 i = 0; i < cnt_blobs; ++i) {
            if (!info_ptr->coord.empty()) {
                Uint2 key_slot = 0, key_bucket = 0;
                CNCDistributionConf::GetSlotByKey(info_ptr->key, key_slot, key_bucket);
                if (key_slot != slot  ||  key_bucket != bucket_num)
                    abort();

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
}

void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    m_DBFilesLock.Lock();
    Uint4 num_files = Uint4(m_DBFiles.size());
    m_DBFilesLock.Unlock();
    Uint8 cnt_blobs = m_CurBlobsCnt;
    int oldest_dead = numeric_limits<int>::max();
    for (Uint2 i = 1; i <= CNCDistributionConf::GetCntTimeBuckets(); ++i) {
        STimeTable* table = m_TimeTables[i];
        table->lock.Lock();
        if (!table->time_map.empty())
            oldest_dead = min(oldest_dead, table->time_map.begin()->dead_time);
        table->lock.Unlock();
    }
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
                    << g_ToSizeStr(m_MovedSize) << " in size, cleaned "
                    << g_ToSmartStr(m_CntMoveFiles) << " files" << endl;
    if (oldest_dead != numeric_limits<int>::max()) {
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

void
CNCBlobStorage::x_CacheDeleteIndexes(SNCDataCoord map_coord, Uint1 map_depth)
{
    SNCDBFileInfo* file_info = m_DBFiles[map_coord.file_id];
    SFileIndexRec* map_ind = x_GetIndexRec(file_info, map_coord.rec_num);
    x_DeleteIndexRec(file_info, map_ind);
    if (map_depth != 0) {
        SFileChunkMapRec* map_rec = x_CalcMapAddress(file_info, map_ind);
        Uint2 cnt_downs = x_CalcCntMapDowns(map_ind->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            x_CacheDeleteIndexes(map_rec->down_coords[i], map_depth - 1);
        }
    }
}

bool
CNCBlobStorage::x_CacheMapRecs(SNCDataCoord map_coord,
                               Uint1 map_depth,
                               SNCDataCoord up_coord,
                               Uint2 up_index,
                               SNCCacheData* cache_data,
                               Uint8 cnt_chunks,
                               Uint8& chunk_num,
                               map<Uint4, Uint4>& sizes_map,
                               TFileRecsMap& recs_map)
{
    TNCDBFilesMap::const_iterator it_file = m_DBFiles.find(map_coord.file_id);
    if (it_file == m_DBFiles.end()) {
        ERR_POST(Critical << "Cannot find file for the coord " << map_coord
                 << " which is referenced by blob " << cache_data->key
                 << ". Deleting blob.");
        return false;
    }

    SNCDBFileInfo* file_info = it_file->second.GetNCPointerOrNull();
    TRecNumsSet& recs_set = recs_map[map_coord.file_id];
    TRecNumsSet::iterator it_recs = recs_set.find(map_coord.rec_num);
    if (it_recs == recs_set.end()) {
        ERR_POST(Critical << "Blob " << cache_data->key
                 << " references record with coord " << map_coord
                 << ", but its index wasn't in live chain. Deleting blob.");
        return false;
    }

    SFileIndexRec* map_ind = x_GetIndexRec(file_info, map_coord.rec_num);
    if (map_ind->chain_coord != up_coord) {
        ERR_POST(Critical << "Up_coord for map/data in blob " << cache_data->key
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
            ERR_POST(Critical << "Blob " << cache_data->key
                     << " with size " << cache_data->size
                     << " references record with coord " << map_coord
                     << " that has type " << int(map_ind->rec_type)
                     << " when it should be " << int(eFileRecChunkData)
                     << ". Deleting blob.");
            return false;
        }
        ++chunk_num;
        if (chunk_num > cnt_chunks) {
            ERR_POST(Critical << "Blob " << cache_data->key
                     << " with size " << cache_data->size
                     << " has too many data chunks, should be " << cnt_chunks
                     << ". Deleting blob.");
            return false;
        }
        size_t data_size = x_CalcChunkDataSize(map_ind->rec_size);
        Uint4 need_size;
        if (chunk_num < cnt_chunks)
            need_size = cache_data->chunk_size;
        else
            need_size = (cache_data->size - 1) % cache_data->chunk_size + 1;
        if (data_size != need_size) {
            ERR_POST(Critical << "Blob " << cache_data->key
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
            ERR_POST(Critical << "Blob " << cache_data->key
                     << " with size " << cache_data->size
                     << " references record with coord " << map_coord
                     << " at map_depth=" << map_depth
                     << ". Record has type " << int(map_ind->rec_type)
                     << " when it should be " << int(eFileRecChunkMap)
                     << ". Deleting blob.");
            return false;
        }
        SFileChunkMapRec* map_rec = x_CalcMapAddress(file_info, map_ind);
        if (map_rec->map_idx != up_index  ||  map_rec->map_depth != map_depth)
        {
            ERR_POST(Critical << "Blob " << cache_data->key
                     << " with size " << cache_data->size
                     << " references map with coord " << map_coord
                     << " that has wrong index " << map_rec->map_idx
                     << " and/or map depth " << map_rec->map_depth
                     << ", they should be " << up_index
                     << " and " << map_depth
                     << ". Deleting blob.");
            return false;
        }
        Uint2 cnt_downs = x_CalcCntMapDowns(map_ind->rec_size);
        for (Uint2 i = 0; i < cnt_downs; ++i) {
            if (!x_CacheMapRecs(map_rec->down_coords[i], map_depth - 1,
                                map_coord, i, cache_data, cnt_chunks,
                                chunk_num, sizes_map, recs_map))
            {
                for (Uint2 j = 0; j < i; ++j) {
                    x_CacheDeleteIndexes(map_rec->down_coords[j], map_depth - 1);
                }
                return false;
            }
        }
        if (chunk_num < cnt_chunks  &&  cnt_downs != cache_data->map_size) {
            ERR_POST(Critical << "Blob " << cache_data->key
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
CNCBlobStorage::x_CacheMetaRec(SNCDBFileInfo* file_info,
                               SFileIndexRec* ind_rec,
                               SNCDataCoord coord,
                               int cur_time,
                               TFileRecsMap& recs_map)
{
    SFileMetaRec* meta_rec = x_CalcMetaAddress(file_info, ind_rec);
    if (meta_rec->has_password > 1  ||  meta_rec->dead_time < meta_rec->expire
        ||  meta_rec->dead_time < meta_rec->ver_expire
        ||  meta_rec->chunk_size == 0  ||  meta_rec->map_size == 0)
    {
        ERR_POST(Critical << "Meta record in file " << file_info->file_name
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
    if (meta_rec->dead_time <= cur_time)
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
        ERR_POST(Critical << "Meta record in file " << file_info->file_name
                 << " at offset " << ind_rec->offset << " was corrupted."
                 << " key=" << key
                 << ", cnt_special=" << cnt
                 << ". Deleting it.");
        return false;
    }
    Uint2 slot = 0, time_bucket = 0;
    CNCDistributionConf::GetSlotByKey(key, slot, time_bucket);

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
    cache_data->expire = meta_rec->expire;
    cache_data->ver_expire = meta_rec->ver_expire;

    if (meta_rec->size == 0) {
        if (!ind_rec->chain_coord.empty()) {
            ERR_POST(Critical << "Index record " << (file_info->index_head - ind_rec)
                     << " in file " << file_info->file_name
                     << " was corrupted. size=0 but chain_coord="
                     << ind_rec->chain_coord << ". Ignoring that.");
            ind_rec->chain_coord.clear();
        }
    }
    else {
        Uint1 map_depth = x_CalcMapDepthImpl(meta_rec->size,
                                             meta_rec->chunk_size,
                                             meta_rec->map_size);
        if (map_depth > kNCMaxBlobMapsDepth) {
            ERR_POST(Critical << "Meta record in file " << file_info->file_name
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
                            cnt_chunks, chunk_num, sizes_map, recs_map))
        {
            delete cache_data;
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
    ind_rec->cache_data = cache_data;
    Uint4 rec_size = ind_rec->rec_size;
    if (rec_size & 7)
        rec_size += 8 - (rec_size & 7);
    rec_size += sizeof(SFileIndexRec);
    file_info->used_size += rec_size;
    if (file_info->garb_size < rec_size)
        abort();
    file_info->garb_size -= rec_size;
    m_GarbageSize -= rec_size;

    TBucketCacheMap::iterator it_bucket = m_BucketsCache.lower_bound(time_bucket);
    SBucketCache* bucket_cache;
    if (it_bucket == m_BucketsCache.end()  ||  it_bucket->first != time_bucket) {
        bucket_cache = new SBucketCache(time_bucket);
        m_BucketsCache.insert(it_bucket, TBucketCacheMap::value_type(time_bucket, bucket_cache));
    }
    else {
        bucket_cache = it_bucket->second;
    }
    STimeTable* time_table = m_TimeTables[time_bucket];
    TKeyMap::insert_commit_data commit_data;
    pair<TKeyMap::iterator, bool> ins_res =
        bucket_cache->key_map.insert_unique_check(key, SCacheKeyCompare(), commit_data);
    if (ins_res.second) {
        bucket_cache->key_map.insert_unique_commit(*cache_data, commit_data);
    }
    else {
        SNCCacheData* old_data = &*ins_res.first;
        bucket_cache->key_map.erase(ins_res.first);
        bucket_cache->key_map.insert_equal(*cache_data);
        --m_CurBlobsCnt;
        time_table->time_map.erase(*old_data);
        SNCDBFileInfo* old_file = m_DBFiles[old_data->coord.file_id];
        SFileIndexRec* old_ind = x_GetIndexRec(old_file, old_data->coord.rec_num);
        SFileMetaRec* old_rec = x_CalcMetaAddress(old_file, old_ind);
        if (old_rec->size != 0  &&  old_ind->chain_coord != ind_rec->chain_coord) {
            Uint1 map_depth = x_CalcMapDepth(old_rec->size,
                                             old_rec->chunk_size,
                                             old_rec->map_size);
            x_MoveDataToGarbage(old_ind->chain_coord, map_depth);
        }
        x_MoveRecToGarbage(old_file, old_ind);
        delete old_data;
    }
    time_table->time_map.insert_equal(*cache_data);
    ++m_CurBlobsCnt;

    return true;
}

bool
CNCBlobStorage::x_CreateInitialFiles(set<Uint4>& new_file_ids)
{
    int cur_pass = 0;
    size_t cur_file = 0;
    while (!m_Stopped  &&  (cur_pass != 1  ||  cur_file < s_CntAllFiles)) {
        if (cur_file < s_CntAllFiles) {
            if (!x_CreateNewFile(cur_file))
                goto del_file_and_retry;
            new_file_ids.insert(m_AllWritings[cur_file].next_file->file_id);
            ++cur_file;
        }
        else {
            for (size_t i = 0; i < s_CntAllFiles; ++i) {
                x_SwitchToNextFile(m_AllWritings[i]);
            }
            cur_file = 0;
            ++cur_pass;
        }
        continue;

del_file_and_retry:
        if (m_DBFiles.empty()) {
            ERR_POST(Critical << "Cannot create initial database files.");
            return false;
        }

        CNCRef<SNCDBFileInfo> file_to_del;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CNCRef<SNCDBFileInfo> file_info = it->second;
            if (file_info->file_type == eDBFileData) {
                file_to_del = file_info;
                break;
            }
        }
        if (!file_to_del)
            file_to_del = m_DBFiles.begin()->second;
        x_DeleteDBFile(file_to_del);
    }

    return true;
}

void
CNCBlobStorage::x_PreCacheFileRecNums(SNCDBFileInfo* file_info,
                                      TRecNumsSet& recs_set)
{
    m_CurDBSize += file_info->file_size;
    // Non-garbage is left the same as in x_CreateNewFile
    Uint4 garb_size = file_info->file_size
                      - (kSignatureSize + sizeof(SFileIndexRec));
    file_info->garb_size += garb_size;
    m_GarbageSize += garb_size;

    SFileIndexRec* ind_rec = file_info->index_head;
    Uint4 prev_rec_num = 0;
    char* min_ptr = file_info->file_map + kSignatureSize;
    while (!m_Stopped  &&  ind_rec->next_num != 0) {
        Uint4 rec_num = ind_rec->next_num;
        if (rec_num <= prev_rec_num) {
            ERR_POST(Critical << "File " << file_info->file_name
                     << " contains wrong next_num=" << rec_num
                     << " (it's not greater than current " << prev_rec_num
                     << "). Won't cache anything else from this file.");
            goto wrap_index_and_return;
        }
        SFileIndexRec* next_ind;
        next_ind = file_info->index_head - rec_num;
        if ((char*)next_ind < min_ptr  ||  next_ind >= ind_rec) {
            ERR_POST(Critical << "File " << file_info->file_name
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
            ERR_POST(Critical << "File " << file_info->file_name
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
            ERR_POST(Critical << "File " << file_info->file_name
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
                ERR_POST(Critical << "File " << file_info->file_name
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
                ERR_POST(Critical << "File " << file_info->file_name
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
                ERR_POST(Critical << "File " << file_info->file_name
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
            ERR_POST(Critical << "File " << file_info->file_name
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
    return;

wrap_index_and_return:
    ind_rec->next_num = 0;
}

void
CNCBlobStorage::x_CacheDatabase(void)
{
    CNCRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    CDiagContext::SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart().Print("_type", "caching");
        ctx->SetRequestStatus(CNCMessageHandler::eStatus_OK);
    }

    TFileRecsMap recs_map;
    ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        SNCDBFileInfo* file_info = it_file->second.GetNCPointerOrNull();
        x_PreCacheFileRecNums(file_info, recs_map[file_info->file_id]);
    }

    set<Uint4> new_file_ids;
    if (!x_CreateInitialFiles(new_file_ids)) {
        ctx->SetRequestStatus(CNCMessageHandler::eStatus_ServerError);
        g_NetcacheServer->RequestShutdown();
        goto clean_and_exit;
    }

    int cur_time;
    cur_time = int(time(NULL));
    ERASE_ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        if (m_Stopped)
            break;

        SNCDBFileInfo* file_info = it_file->second;
        if (file_info->file_type != eDBFileMeta
            ||  new_file_ids.find(file_info->file_id) != new_file_ids.end())
        {
            continue;
        }
        TRecNumsSet& recs_set = recs_map[file_info->file_id];
        TRecNumsSet::iterator it_rec = recs_set.begin();
        for (; it_rec != recs_set.end()  &&  !m_Stopped; ++it_rec) {
            Uint4 rec_num = *it_rec;
            SNCDataCoord coord;
            coord.file_id = file_info->file_id;
            coord.rec_num = rec_num;
            SFileIndexRec* ind_rec = x_GetIndexRec(file_info, rec_num);
            if (!x_CacheMetaRec(file_info, ind_rec, coord, cur_time, recs_map))
                x_DeleteIndexRec(file_info, ind_rec);
        }
        recs_set.clear();
    }

    ITERATE(TFileRecsMap, it_id, recs_map) {
        if (m_Stopped)
            break;

        Uint4 file_id = it_id->first;
        const TRecNumsSet& recs_set = it_id->second;
        TNCDBFilesMap::iterator it_file = m_DBFiles.find(file_id);
        if (it_file == m_DBFiles.end()) {
            // File could be deleted when we tried to create initial files.
            continue;
        }
        SNCDBFileInfo* file_info = it_file->second;
        TRecNumsSet::iterator it_rec = recs_set.begin();
        for (; it_rec != recs_set.end()  &&  !m_Stopped; ++it_rec) {
            x_DeleteIndexRec(file_info, *it_rec);
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
    ITERATE(TBucketCacheMap, it, m_BucketsCache) {
        SBucketCache* cache = it->second;
        cache->deleter.HeartBeat();
    }

    try {
        Uint8 free_space = CFileUtil::GetFreeDiskSpace(m_Path);
        Uint8 cur_db_size = m_CurDBSize;
        Uint8 total_space = cur_db_size + free_space;
        Uint8 allowed_db_size = total_space;

        if (total_space < m_DiskCritical)
            allowed_db_size = 0;
        else
            allowed_db_size = min(allowed_db_size, total_space - m_DiskCritical);
        if (total_space < m_DiskFreeLimit)
            allowed_db_size = 0;
        else
            allowed_db_size = min(allowed_db_size, total_space - m_DiskFreeLimit);
        if (m_StopWriteOnSize != 0  &&  m_StopWriteOnSize < allowed_db_size)
            allowed_db_size = m_StopWriteOnSize;

        if (m_IsStopWrite == eNoStop
            &&  cur_db_size * 100 >= allowed_db_size * m_WarnLimitOnPct)
        {
            ERR_POST(Critical << "ALERT! Database is too large. "
                     << "Current db size is " << g_ToSizeStr(cur_db_size)
                     << ", allowed db size is " << g_ToSizeStr(allowed_db_size) << ".");
            m_IsStopWrite = eStopWarning;
        }

        if (m_IsStopWrite == eStopWarning) {
            if (m_StopWriteOnSize != 0  &&  cur_db_size >= m_StopWriteOnSize) {
                m_IsStopWrite = eStopDBSize;
                ERR_POST(Critical << "Database size exceeded its limit. "
                                     "Will no longer accept any writes from clients.");
            }
        }
        else if (m_IsStopWrite == eStopDBSize  &&  cur_db_size <= m_StopWriteOffSize)
        {
            m_IsStopWrite = eStopWarning;
        }
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
            m_IsStopWrite = eStopWarning;
        }

        if (m_IsStopWrite == eStopWarning
            &&  cur_db_size * 100 < allowed_db_size * m_WarnLimitOffPct)
        {
            ERR_POST(Critical << "OK. Database is back to normal size. "
                     << "Current db size is " << g_ToSizeStr(cur_db_size)
                     << ", allowed db size is " << g_ToSizeStr(allowed_db_size) << ".");
            m_IsStopWrite = eNoStop;
        }
    }
    catch (CFileErrnoException& ex) {
        ERR_POST(Critical << "Cannot read free disk space: " << ex);
    }
}

void
CNCBlobStorage::x_GC_DeleteExpired(SNCCacheData* cache_data, int dead_before)
{
    cache_data->lock.Lock();
    if (!cache_data->ver_mgr) {
        if (!cache_data->coord.empty()  &&  cache_data->dead_time < dead_before) {
            SNCDataCoord coord = cache_data->coord;
            Uint8 size = cache_data->size;
            Uint4 chunk_size = cache_data->chunk_size;
            Uint2 map_size = cache_data->map_size;
            SNCDataCoord new_coord;
            new_coord.clear();
            ChangeCacheDeadTime(cache_data, new_coord, 0);
            cache_data->key_del_time = int(time(NULL));
            SBucketCache* bucket = x_GetBucketCache(cache_data->time_bucket);
            bucket->deleter.AddElement(cache_data->key);
            cache_data->lock.Unlock();

            CNCRef<SNCDBFileInfo> meta_file = x_GetDBFile(coord.file_id);
            SFileIndexRec* meta_ind = x_GetIndexRec(meta_file, coord.rec_num);
            if (!meta_ind->chain_coord.empty()) {
                Uint1 map_depth = x_CalcMapDepth(size, chunk_size, map_size);
                x_MoveDataToGarbage(meta_ind->chain_coord, map_depth);
            }
            x_MoveRecToGarbage(meta_file, meta_ind);
        }
        else {
            cache_data->lock.Unlock();
        }
    }
    else {
        cache_data->lock.Unlock();

        m_GCAccessor->Prepare(cache_data->key, kEmptyStr,
                              cache_data->time_bucket, eNCGCDelete);
        x_InitializeAccessor(m_GCAccessor);
        m_GCBlockLock.Lock();
        while (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock) {
            m_GCBlockWaiter.WaitForSignal(m_GCBlockLock);
        }
        m_GCBlockLock.Unlock();
        if (m_GCAccessor->IsBlobExists()
            &&  m_GCAccessor->GetCurBlobDeadTime() < dead_before)
        {
            m_GCAccessor->DeleteBlob();
        }
        m_GCAccessor->Deinitialize();
    }
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

    Uint2 cnt_buckets = CNCDistributionConf::GetCntTimeBuckets();
    int cnt_read = 1;
    Uint2 bucket = 1;
    while (cnt_read != 0  &&  !m_Stopped) {
        m_GCDatas.clear();
        cnt_read = 0;
        for (; bucket <= cnt_buckets  &&  cnt_read < m_GCBatchSize; ++bucket) {
            STimeTable* table = m_TimeTables[bucket];
            table->lock.Lock();
            NON_CONST_ITERATE(TTimeTableMap, it, table->time_map) {
                SNCCacheData* cache_data = &*it;
                if (cache_data->dead_time >= next_dead  ||  cnt_read >= m_GCBatchSize)
                {
                    break;
                }
                m_GCDatas.push_back(cache_data);
                ++cnt_read;
            }
            table->lock.Unlock();
        }
        for (size_t i = 0; i < m_GCDatas.size()  &&  !m_Stopped; ++i) {
            x_GC_DeleteExpired(m_GCDatas[i], next_dead);
        }
    }
}

bool
CNCBlobStorage::x_MoveRecord(SNCDBFileInfo* rec_file,
                             SFileIndexRec* rec_ind,
                             int cur_time,
                             bool& move_done)
{
    bool result = true;
    CNCRef<SNCDBFileInfo> chain_file;
    SFileIndexRec* chain_ind = NULL;
    SFileChunkMapRec* map_rec = NULL;
    SFileChunkMapRec* up_map = NULL;
    Uint2 up_index;

    SNCDataCoord new_coord;
    CNCRef<SNCDBFileInfo> new_file;
    SFileIndexRec* new_ind;
    if (!x_GetNextWriteCoord(rec_file->type_index, //ENCDBFileType(rec_file->type_index + eFileIndexMoveShift),
                             rec_ind->rec_size, new_coord, new_file, new_ind))
    {
        move_done = false;
        return false;
    }

    memcpy(new_file->file_map + new_ind->offset,
           rec_file->file_map + rec_ind->offset,
           rec_ind->rec_size);

    SNCCacheData* cache_data = rec_ind->cache_data;
    new_ind->cache_data = cache_data;
    new_ind->rec_type = rec_ind->rec_type;
    SNCDataCoord chain_coord = rec_ind->chain_coord;
    new_ind->chain_coord = chain_coord;

    if (!chain_coord.empty()) {
        chain_file = x_GetDBFile(chain_coord.file_id);
        chain_ind = x_GetIndOrDeleted(chain_file, chain_coord.rec_num);
        if (x_IsIndexDeleted(chain_file, chain_ind)
            ||  chain_file->cnt_unfinished.Get() != 0)
        {
            goto wipe_new_record;
        }

        SNCDataCoord old_coord;
        old_coord.file_id = rec_file->file_id;
        old_coord.rec_num = Uint4(rec_file->index_head - rec_ind);

        // Checks below are not just checks for corruption but also
        // an opportunity to fault in all database pages that will be necessary
        // to make all changes. This way we minimize the time that cache_data
        // lock will be held.
        switch (rec_ind->rec_type) {
        case eFileRecMeta:
            if (chain_ind->chain_coord != old_coord
                &&  !x_IsIndexDeleted(rec_file, rec_ind))
            {
                DB_CORRUPTED("Meta with coord " << old_coord
                             << " links down to record with coord " << new_ind->chain_coord
                             << " but it has up coord " << chain_ind->chain_coord);
            }
            break;
        case eFileRecChunkMap:
            map_rec = x_CalcMapAddress(new_file, new_ind);
            up_index = map_rec->map_idx;
            Uint2 cnt_downs;
            cnt_downs = x_CalcCntMapDowns(new_ind->rec_size);
            for (Uint2 i = 0; i < cnt_downs; ++i) {
                SNCDataCoord down_coord = map_rec->down_coords[i];
                CNCRef<SNCDBFileInfo> down_file = x_GetDBFile(down_coord.file_id);
                SFileIndexRec* down_ind = x_GetIndOrDeleted(down_file, down_coord.rec_num);
                if (x_IsIndexDeleted(down_file, down_ind))
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
            data_rec = x_CalcChunkAddress(new_file, new_ind);
            up_index = data_rec->chunk_idx;
        check_up_index:
            if (chain_ind->rec_type == eFileRecChunkMap) {
                up_map = x_CalcMapAddress(chain_file, chain_ind);
                if (up_map->down_coords[up_index] != old_coord) {
                    DB_CORRUPTED("Record with coord " << old_coord
                                 << " links up to map with coord " << new_ind->chain_coord
                                 << " but at the index " << up_index
                                 << " it has coord " << up_map->down_coords[up_index]);
                }
            }
            else {
                if (chain_ind->chain_coord != old_coord) {
                    DB_CORRUPTED("Record with coord " << old_coord
                                 << " links up to meta with coord " << new_ind->chain_coord
                                 << " but it has down coord " << chain_ind->chain_coord);
                }
            }
            break;
        }
    }

    cache_data->lock.Lock();
    if (cache_data->ver_mgr
        ||  rec_ind->chain_coord != chain_coord
        ||  (!chain_coord.empty()  &&  x_IsIndexDeleted(chain_file, chain_ind)
             &&  !x_IsIndexDeleted(rec_file, rec_ind)))
    {
        result = false;
        goto unlock_and_wipe;
    }
    if (cache_data->coord.empty()
        ||  cache_data->dead_time <= cur_time + m_MinMoveLife
        ||  x_IsIndexDeleted(rec_file, rec_ind))
    {
        goto unlock_and_wipe;
    }

    switch (rec_ind->rec_type) {
    case eFileRecMeta:
        cache_data->coord = new_coord;
        if (chain_ind)
            chain_ind->chain_coord = new_coord;
        break;
    case eFileRecChunkMap:
        x_UpdateUpCoords(map_rec, new_ind, new_coord);
        // fall through
    case eFileRecChunkData:
        if (up_map)
            up_map->down_coords[up_index] = new_coord;
        else
            chain_ind->chain_coord = new_coord;
        break;
    }

    cache_data->lock.Unlock();
    move_done = true;
    new_file->cnt_unfinished.Add(-1);
    return true;

unlock_and_wipe:
    cache_data->lock.Unlock();
wipe_new_record:
    new_ind->rec_type = eFileRecChunkData;
    x_MoveRecToGarbage(new_file, new_ind);
    move_done = false;
    new_file->cnt_unfinished.Add(-1);

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
    CNCRef<SNCDBFileInfo> max_file;
    vector<CNCRef<SNCDBFileInfo> > files_to_del;

    m_DBFilesLock.Lock();
    ITERATE(TNCDBFilesMap, it_file, m_DBFiles) {
        CNCRef<SNCDBFileInfo> this_file = it_file->second;
        m_NextWriteLock.Lock();
        bool is_current = false;
        for (size_t i = 0; i < s_CntAllFiles; ++i) {
            if (this_file == m_AllWritings[i].cur_file
                ||  this_file == m_AllWritings[i].next_file)
            {
                is_current = true;
                break;
            }
        }
        m_NextWriteLock.Unlock();
        if (is_current  ||  this_file->cnt_unfinished.Get() != 0)
            continue;

        if (this_file->used_size == 0) {
            files_to_del.push_back(this_file);
        }
        else if (need_move) {
            if (cur_time >= this_file->next_shrink_time) {
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

    NON_CONST_ITERATE(vector<CNCRef<SNCDBFileInfo> >, it, files_to_del) {
        x_DeleteDBFile(*it);
    }
    files_to_del.clear();

    if (need_move) {
        need_move = m_CurDBSize >= m_MinDBSize
                    &&  m_GarbageSize * 100 > m_CurDBSize * m_MaxGarbagePct;
    }
    if (max_file == NULL  ||  !need_move  ||  max_pct < total_pct)
        return false;

    CNCRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    GetDiagContext().SetRequestContext(ctx);
    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "move")
                        .Print("file_id", max_file->file_id);
    }
    ctx->SetRequestStatus(CNCMessageHandler::eStatus_OK);

    bool failed = false;
    Uint4 prev_rec_num = 0;
    Uint4 cnt_processed = 0, cnt_moved = 0, size_moved = 0;
    while (!m_Stopped) {
        max_file->info_lock.Lock();
        Uint4 rec_num = 0;
        SFileIndexRec* ind_rec = max_file->index_head;
        do {
            if (ind_rec->next_num == 0) {
                if (max_file->index_head->next_num == 0  &&  max_file->used_size != 0)
                    abort();
                ind_rec = NULL;
                break;
            }
            rec_num = ind_rec->next_num;
            ind_rec = max_file->index_head - rec_num;
        }
        while (rec_num <= prev_rec_num
               ||  ind_rec->cache_data->dead_time <= cur_time + m_MinMoveLife);
        max_file->info_lock.Unlock();
        if (!ind_rec)
            break;
        if (x_IsIndexDeleted(max_file, ind_rec))
            continue;
        if (ind_rec->cache_data->ver_mgr) {
            failed = true;
            break;
        }

        bool move_done = false;
        if (!x_MoveRecord(max_file, ind_rec, cur_time, move_done)) {
            failed = true;
            break;
        }
        ++cnt_processed;
        if (move_done) {
            ++cnt_moved;
            ++m_CntMoveTasks;
            size_moved += ind_rec->rec_size + sizeof(SFileIndexRec);
            m_MovedSize += ind_rec->rec_size + sizeof(SFileIndexRec);
            x_MoveRecToGarbage(max_file, ind_rec);
        }
        prev_rec_num = rec_num;
    }
    if (!failed) {
        ++m_CntMoveFiles;
        if (max_file->used_size == 0)
            x_DeleteDBFile(max_file);
        else if (cnt_processed == 0) {
            ERR_POST(Warning << "Didn't find anything to process in the file");
            max_file->next_shrink_time = cur_time + max(m_MinMoveLife, 300);
        }
        else
            max_file->next_shrink_time = cur_time + m_MinMoveLife;
    }
    else {
        ctx->SetRequestStatus(CNCMessageHandler::eStatus_CmdAborted);
        ++m_CntFailedMoves;
        max_file->next_shrink_time = cur_time + m_FailedMoveDelay;
    }

    if (g_NetcacheServer->IsLogCmds()) {
        GetDiagContext().Extra().Print("cnt_processed", cnt_processed)
                                .Print("cnt_moved", cnt_moved)
                                .Print("size_moved", size_moved);
        GetDiagContext().PrintRequestStop();
    }
    ctx.Reset();
    GetDiagContext().SetRequestContext(NULL);

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
        CNCRef<SNCDBFileInfo> file_info = file_it->second;
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
        for (size_t i = 0; i < s_CntAllFiles; ++i) {
            if (m_AllWritings[i].next_file == NULL)
                x_CreateNewFile(i);
        }

        m_NextWriteLock.Lock();
        bool has_null = false;
        for (size_t i = 0; i < s_CntAllFiles; ++i) {
            if (m_AllWritings[i].next_file == NULL) {
                has_null = true;
                break;
            }
        }
        if (!m_Stopped  &&  !has_null)
            m_NextFileSwitch.WaitForSignal(m_NextWriteLock);
        m_NextWriteLock.Unlock();
    }
}


CNCBlobStorage::SBucketCache::SBucketCache(Uint2 bucket)
    : deleter(CKeysCleaner(bucket))
{}


CNCBlobStorage::CKeysCleaner::CKeysCleaner(Uint2 bucket)
    : m_Bucket(bucket)
{}

void
CNCBlobStorage::CKeysCleaner::Delete(const string& key) const
{
    SBucketCache* cache = g_NCStorage->x_GetBucketCache(m_Bucket);
    cache->lock.Lock();
    TKeyMap::iterator it = cache->key_map.find(key, SCacheKeyCompare());
    if (it != cache->key_map.end()) {
        SNCCacheData* cache_data = &*it;
        if (cache_data->key_del_time != 0) {
            if (cache_data->key_del_time >= int(time(NULL)) - 2) {
                cache->deleter.AddElement(key);
            }
            else {
                cache->key_map.erase(it);
                delete cache_data;
            }
        }
    }
    cache->lock.Unlock();
}

END_NCBI_SCOPE
