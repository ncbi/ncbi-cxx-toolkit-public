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
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  NetCache_Storage


static const char* kNCStorage_DBFileExt       = ".db";
static const char* kNCStorage_IndexFileSuffix = ".index";
static const char* kNCStorage_MetaFileSuffix  = ".meta.";
static const char* kNCStorage_DataFileSuffix  = ".data.";
static const char* kNCStorage_StartedFileName = "__ncbi_netcache_started__";

static const char* kNCStorage_RegSection        = "storage";
static const char* kNCStorage_PathParam         = "path";
static const char* kNCStorage_FilePrefixParam   = "prefix";
static const char* kNCStorage_PartsCntParam     = "parts_cnt";
static const char* kNCStorage_GCDelayParam      = "gc_delay";
static const char* kNCStorage_GCBatchParam      = "gc_batch_size";
static const char* kNCStorage_MaxMetaSizeParam  = "max_metafile_size";
static const char* kNCStorage_MaxDataSizeParam  = "max_datafile_size";
static const char* kNCStorage_MinMetaSizeParam  = "min_metafile_size";
static const char* kNCStorage_MinDataSizeParam  = "min_datafile_size";
static const char* kNCStorage_MinUsfulMetaParam = "min_useful_meta_pct";
static const char* kNCStorage_MinUsfulDataParam = "min_useful_data_pct";



/// Special exception to terminate background thread when storage is
/// destroying. Class intentionally made without inheritance from CException
/// or std::exception.
class CNCStorage_StoppedException
{};

///
class CNCStorage_BlockedException
{};

///
class CNCStorage_RecacheException
{};



const char*
CNCBlobStorageException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eUnknown:      return "eUnknown";
    case eWrongConfig:  return "eWrongConfig";
    case eCorruptedDB:  return "eCorruptedDB";
    case eWrongBlock:   return "eWrongBlock";
    default:            return CException::GetErrCodeString();
    }
}



inline void
CNCBlobStorage::x_EnsureDirExist(const string& dir_name)
{
    CDir dir(dir_name);
    if (!dir.Exists()) {
        dir.Create();
    }
}

void
CNCBlobStorage::x_ReadVariableParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_GCRunDelay  = reg.GetInt(kNCStorage_RegSection, kNCStorage_GCDelayParam,
                               30,  0, IRegistry::eErrPost);
    m_GCBatchSize = reg.GetInt(kNCStorage_RegSection, kNCStorage_GCBatchParam,
                               500, 0, IRegistry::eErrPost);

    string max_size_str_m = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MaxMetaSizeParam, "3Mb");
    string max_size_str_d = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MaxDataSizeParam, "300Mb");
    string min_size_str_m = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MinMetaSizeParam, "1Mb");
    string min_size_str_d = reg.GetString(kNCStorage_RegSection,
                                          kNCStorage_MinDataSizeParam, "100Mb");
    try {
        m_MaxFileSize[eNCMeta] = NStr::StringToUInt8_DataSize(max_size_str_m);
        m_MaxFileSize[eNCData] = NStr::StringToUInt8_DataSize(max_size_str_d);
        m_MinFileSize[eNCMeta] = NStr::StringToUInt8_DataSize(min_size_str_m);
        m_MinFileSize[eNCData] = NStr::StringToUInt8_DataSize(min_size_str_d);
    }
    catch (CStringException& ex) {
        ERR_POST_X(21, "Error in meta/data min/max size parameters: " << ex);
        m_MaxFileSize[eNCMeta] = 3000000;
        m_MaxFileSize[eNCData] = 300000000;
        m_MinFileSize[eNCMeta] = 1000000;
        m_MinFileSize[eNCData] = 100000000;
    }
    m_MinUsefulPct[eNCMeta] = double(reg.GetInt(kNCStorage_RegSection,
                                                kNCStorage_MinUsfulMetaParam,
                                                50, 0, IRegistry::eErrPost)) / 100;
    m_MinUsefulPct[eNCData] = double(reg.GetInt(kNCStorage_RegSection,
                                                kNCStorage_MinUsfulDataParam,
                                                50, 0, IRegistry::eErrPost)) / 100;

    int parts_cnt = reg.GetInt(kNCStorage_RegSection, kNCStorage_PartsCntParam,
                               5, 0, IRegistry::eErrPost);
    if (parts_cnt <= 0) {
        NCBI_THROW(CNCBlobStorageException, eWrongConfig,
                   "Number of database parts cannot be less or equal to 0");
    }
    m_PartsPaths.resize(parts_cnt);
    string path_param_prefix(kNCStorage_PathParam);
    path_param_prefix += "_";
    for (int i = 0; i < parts_cnt; ++i) {
        string param(path_param_prefix);
        param += NStr::IntToString(i + 1);
        string this_path(reg.GetString(kNCStorage_RegSection, param, m_MainPath));
        m_PartsPaths[i] = this_path;
        x_EnsureDirExist(this_path);
    }
    m_CurFiles[eNCMeta].resize(parts_cnt);
    m_CurFiles[eNCData].resize(parts_cnt);
}

void
CNCBlobStorage::x_ReadStorageParams(void)
{
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();

    m_MainPath = reg.Get(kNCStorage_RegSection, kNCStorage_PathParam);
    m_Prefix   = reg.Get(kNCStorage_RegSection, kNCStorage_FilePrefixParam);
    if (m_MainPath.empty()  ||  m_Prefix.empty()) {
        NCBI_THROW_FMT(CNCBlobStorageException, eWrongConfig,
                       "Incorrect parameters for " << kNCStorage_PathParam
                       << " and " << kNCStorage_FilePrefixParam
                       << " in the section '" << kNCStorage_RegSection << "': '"
                       << m_MainPath << "' and '" << m_Prefix << "'");
    }
    x_EnsureDirExist(m_MainPath);
    m_GuardName = CDirEntry::MakePath(m_MainPath,
                                      kNCStorage_StartedFileName,
                                      m_Prefix);

    x_ReadVariableParams();
}

inline string
CNCBlobStorage::x_GetIndexFileName(void)
{
    string file_name(m_Prefix);
    file_name += kNCStorage_IndexFileSuffix;
    return CDirEntry::MakePath(m_MainPath, file_name, kNCStorage_DBFileExt);
}

inline string
CNCBlobStorage::x_GetFileName(ENCDBFileType file_type,
                              unsigned int  part_num,
                              TNCDBFileId   file_id)
{
    string file_name(m_Prefix);
    file_name += (file_type == eNCMeta? kNCStorage_MetaFileSuffix
                                      : kNCStorage_DataFileSuffix);
    file_name += NStr::UIntToString(file_id);
    return CDirEntry::MakePath(m_PartsPaths[part_num], file_name,
                               kNCStorage_DBFileExt);
}

void
CNCBlobStorage::Block(void)
{
    CSpinGuard guard(m_HoldersPoolLock);

    if (m_Blocked) {
        NCBI_THROW(CNCBlobStorageException, eWrongBlock,
                   "Only one blocking can occur at a time");
    }
    if (m_NotCachedFileId != -1) {
        NCBI_THROW(CNCBlobStorageException, eWrongBlock,
                   "Cannot do exclusivity when just started");
    }

    _ASSERT(m_CntLocksToWait == 0);
    m_CntLocksToWait = m_CntUsedHolders;
    m_Blocked = true;
}

void
CNCBlobStorage::Unblock(void)
{
    _ASSERT(m_Blocked);

    CSpinGuard guard(m_HoldersPoolLock);

    if (m_NotCachedFileId != -1)
        m_NeedRecache = true;
    m_Blocked = false;
    while (m_UsedAccessors) {
        CNCBlobAccessor* holder = m_UsedAccessors;
        _ASSERT(!holder->IsInitialized());
        holder->RemoveFromList(m_UsedAccessors);
        x_InitializeAccessor(holder);
    }
}

inline void
CNCBlobStorage::x_LockInstanceGuard(void)
{
    bool guard_existed = CFile(m_GuardName).Exists();

    if (!guard_existed) {
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
    if (guard_existed  &&  CFile(m_GuardName).GetLength() == 0)
        guard_existed = false;
    if (guard_existed) {
        INFO_POST("NetCache wasn't finished cleanly in previous run. "
                  "Will try to work with storage as is.");
    }

    CFileWriter writer(m_GuardLock->GetFileHandle());
    string pid(NStr::UIntToString(CProcess::GetCurrentPid()));
    writer.Write(pid.c_str(), pid.size());
}

inline void
CNCBlobStorage::x_UnlockInstanceGuard(void)
{
    _ASSERT(m_GuardLock.get());

    m_GuardLock.reset();
    CFile(m_GuardName).Remove();
}

void
CNCBlobStorage::x_OpenIndexDB(void)
{
    string index_name = x_GetIndexFileName();
    for (int i = 0; i < 2; ++i) {
        try {
            CNCFileSystem::OpenNewDBFile(index_name, true);
            m_IndexDB = new CNCDBIndexFile(index_name);
            m_IndexDB->CreateDatabase(eNCIndex);
            m_IndexDB->GetAllDBFiles(&m_DBFiles);
            return;
        }
        catch (CSQLITE_Exception& ex) {
            m_IndexDB.reset();
            ERR_POST_X(1, "Index file is broken, reinitializing storage. " << ex);
            CFile(index_name).Remove();
        }
    }
    NCBI_THROW(CNCBlobStorageException, eCorruptedDB,
               "Cannot open or create index file for the storage");
}

void
CNCBlobStorage::x_CleanDatabase(void)
{
    INFO_POST("Reinitializing storage " << m_Prefix << " at " << m_MainPath);

    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCFileSystem::DeleteFileOnClose(it->second.file_name);
    }
    m_DBFiles.clear();
    m_IndexDB->DeleteAllDBFiles();
}

inline void
CNCBlobStorage::x_ReinitializeStorage(void)
{
    try {
        x_CleanDatabase();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error in soft reinitialization, trying harder. " << ex);
        m_IndexDB.reset();
        // GCC 3.0.4 doesn't allow to merge the following 2 lines
        CFile file_obj(x_GetIndexFileName());
        file_obj.Remove();
        x_OpenIndexDB();
    }
}

void
CNCBlobStorage::x_CreateNewFile(ENCDBFileType file_type, unsigned int part_num)
{
    // No need in mutex because m_LastBlob is changed only here and will be
    // executed only from one thread.
    TNCDBFileId file_id = m_LastFileId + 1;
    string file_name = x_GetFileName(file_type, part_num, file_id);

    try {
        // Make sure that there's no such files from some previous runs.
        // As long as we didn't make any connections to databases we can do
        // the deletion safely.
        CFile file_obj(file_name);
        file_obj.Remove();

        TNCDBFilePtr file;
        if (file_type == eNCMeta) {
            file.reset(new CNCDBMetaFile(file_name));
        }
        else {
            file.reset(new CNCDBDataFile(file_name));
        }
        file->CreateDatabase(file_type);
        CNCFileSystem::SetFileInitialized(file_name);

        // No need of mutex on using index database because it is used only
        // in one background thread (after using in constructor).
        m_IndexDB->NewDBFile(file_type, file_id, file_name);
        SNCDBFileInfo* file_info;
        {{
            CSpinWriteGuard guard(m_DBFilesLock);
            file_info    = &m_DBFiles[file_id];
            m_LastFileId = file_id;
        }}
        file_info->file_id     = file_id;
        file_info->file_name   = file_name;
        file_info->create_time = int(time(NULL));
        file_info->file_obj    = file;

        m_CurFiles[file_type][part_num] = file_id;
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Error while creating new database part: " << ex);
        CNCFileSystem::DeleteFileOnClose(file_name);

        // Re-throw exception after cleanup to break application if storage
        // cannot create database files during initialization.
        throw;
    }
}

inline void
CNCBlobStorage::x_CreateNewCurFiles(void)
{
    for (unsigned int i = 0; i < m_CurFiles[0].size(); ++i) {
        x_CreateNewFile(eNCMeta, i);
        x_CreateNewFile(eNCData, i);
    }
}

inline void
CNCBlobStorage::x_DeleteDBFile(TNCDBFilesMap::iterator files_it)
{
    TNCDBFileId  file_id  = files_it->first;
    TNCDBFilePtr file_ptr = files_it->second.file_obj;
    try {
        m_IndexDB->DeleteDBFile(file_id);
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Index database does not delete rows: " << ex);
    }
    CNCFileSystem::DeleteFileOnClose(files_it->second.file_name);

    CSpinWriteGuard guard(m_DBFilesLock);
    m_DBFiles.erase(files_it);
}

inline void
CNCBlobStorage::x_ReadLastBlobCoords(void)
{
    TNCBlobId last_blob = 0;
    ERASE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCDBFile* file = it->second.file_obj.get();
        if (file->GetType() != eNCMeta)
            continue;

        try {
            last_blob = max(last_blob,
                            static_cast<CNCDBMetaFile*>(file)->GetLastBlobId());
        }
        catch (CException& ex) {
            ERR_POST_X(1, "Error reading last blob id from file '"
                          << file->GetFileName() << "'. File will be deleted. "
                          << ex);
            x_DeleteDBFile(it);
        }
    }
    m_LastBlob.blob_id = last_blob;
}

CNCBlobStorage::CNCBlobStorage(bool do_reinit)
    : m_Stopped(false),
      m_StopTrigger(0, 100),
      m_KeysDeleter(CKeysCleaner()),
      m_FreeAccessors(NULL),
      m_UsedAccessors(NULL),
      m_CntUsedHolders(0),
      m_GCBlockWaiter(0, 1),
      m_Blocked(false),
      m_CntLocksToWait(0),
      m_NeedRecache(false)
{
    x_ReadStorageParams();

    x_LockInstanceGuard();
    x_OpenIndexDB();
    if (do_reinit) {
        x_ReinitializeStorage();
    }

    m_LastDeadTime = int(time(NULL));
    m_LastBlob.meta_id = m_LastBlob.data_id = 0;
    m_LastBlob.blob_id = 0;
    m_BlobGeneration = 1;
    m_LastFileId = 0;
    m_LastChunkId = kNCMinChunkId;
    if (m_DBFiles.empty()) {
        m_NotCachedFileId = -1;
    }
    else {
        m_LastFileId = m_DBFiles.rbegin()->first;
        m_NotCachedFileId = m_LastFileId;
        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CNCFileSystem::SetFileInitialized(it->second.file_name);
        }
        x_ReadLastBlobCoords();
    }
    x_CreateNewCurFiles();

    m_GCAccessor = new CNCBlobAccessor();
    _VERIFY(m_CachingTrigger.StartWorking(NULL) == eNCSuccessNoBlock);
    _ASSERT(m_CachingTrigger.GetState() == eNCOpInProgress);

    m_BGThread = NewBGThread(this, &CNCBlobStorage::x_DoBackgroundWork);
    m_BGThread->Run();
}

CNCBlobStorage::~CNCBlobStorage(void)
{
    m_Stopped = true;
    // To be on a safe side let's post 100
    m_StopTrigger.Post(100);
    m_BGThread->Join();

    m_DBFiles.clear();
    m_IndexDB.reset();

    x_UnlockInstanceGuard();
}

void
CNCBlobStorage::PackBlobKey(string*      packed_key,
                            CTempString  cache_name,
                            CTempString  blob_key,
                            CTempString  blob_subkey,
                            unsigned int blob_version)
{
    packed_key->clear();
    packed_key->append(cache_name.data(), cache_name.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_key.data(), blob_key.size());
    packed_key->append(1, '\1');
    packed_key->append(blob_subkey.data(), blob_subkey.size());
    packed_key->append(1, '\1');
#ifdef WORDS_BIGENDIAN
    packed_key->append(1,  blob_version        & 0xFF);
    packed_key->append(1, (blob_version >>  8) & 0xFF);
    packed_key->append(1, (blob_version >> 16) & 0xFF);
    packed_key->append(1, (blob_version >> 24) & 0xFF);
#else
    packed_key->append(reinterpret_cast<const char*>(&blob_version), 4);
#endif
}

string
CNCBlobStorage::UnpackKeyForLogs(const string& packed_key)
{
    // cache
    const char* cache_name = packed_key.data();
    //size_t cache_size = strlen(cache_name);
    size_t cache_size = packed_key.find('\1', 0);
    // key
    const char* key = cache_name + cache_size + 1;
    //size_t key_size = strlen(key);
    size_t key_size = packed_key.find('\1', cache_size + 1) - cache_size - 1;
    // subkey
    const char* subkey = key + key_size + 1;
    //size_t subkey_size = strlen(subkey);
    size_t subkey_size = packed_key.find('\1', cache_size + key_size + 2) - cache_size - key_size - 2;
    // version
    const char* ver_str  = subkey + subkey_size + 1;
    unsigned int version;
#ifdef WORDS_BIGENDIAN
    version =  static_cast<unsigned char>(ver_str[0])
            + (static_cast<unsigned char>(ver_str[1]) <<  8)
            + (static_cast<unsigned char>(ver_str[2]) << 16)
            + (static_cast<unsigned char>(ver_str[3]) << 24);
#else
    memcpy(&version, ver_str, 4);
#endif

    string result;
    result.append("'");
    result.append(key, key_size);
    result.append("'");
    if (cache_size != 0  ||  subkey_size != 0  ||  version != 0) {
        result.append(", '");
        result.append(subkey, subkey_size);
        result.append("', ");
        result.append(NStr::UIntToString(version));
        result.append(" from cache '");
        result.append(cache_name, cache_size);
        result.append("'");
    }
    return result;
}

inline void
CNCBlobStorage::x_WaitForGC(void)
{
    while (m_GCInWork) {
        NCBI_SCHED_YIELD();
    }
}

void
CNCBlobStorage::Reconfigure(void)
{
    x_WaitForGC();

    CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();
    // Non-changeable parameters
    reg.Set(kNCStorage_RegSection, kNCStorage_PathParam, m_MainPath,
            IRegistry::fPersistent | IRegistry::fOverride,
            reg.GetComment(kNCStorage_RegSection, kNCStorage_PathParam));
    reg.Set(kNCStorage_RegSection, kNCStorage_FilePrefixParam, m_Prefix,
            IRegistry::fPersistent | IRegistry::fOverride,
            reg.GetComment(kNCStorage_RegSection, kNCStorage_FilePrefixParam));
    // Change all others
    x_ReadVariableParams();
}

void
CNCBlobStorage::ReinitializeCache(const string& cache_key)
{
    abort();

    _ASSERT(IsBlockActive());

    x_WaitForGC();
    m_KeysCache.Clear();

    string max_key(cache_key);
    size_t ind = max_key.find('\1');
    max_key.resize(ind + 1);
    max_key[ind] = '\2';
    ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCDBFile* file = it->second.file_obj.get();
        if (file->GetType() == eNCMeta) {
            CNCDBMetaFile* metafile = static_cast<CNCDBMetaFile*>(file);
            try {
                metafile->DeleteAllBlobInfos(cache_key, max_key);
            }
            catch (CSQLITE_Exception& ex) {
                ERR_POST("Error cleaning cache in the file '" << file->GetFileName()
                         << "': " << ex);
            }
        }
        file->ResetBlobsCounts();
    }

    m_NotCachedFileId = m_LastFileId;
    m_CachingTrigger.Reset();
    _VERIFY(m_CachingTrigger.StartWorking(NULL) == eNCSuccessNoBlock);
    _ASSERT(m_CachingTrigger.GetState() == eNCOpInProgress);
}

inline void
CNCBlobStorage::x_CheckStopped(void)
{
    if (m_Stopped) {
        throw CNCStorage_StoppedException();
    }
    else if (m_Blocked) {
        throw CNCStorage_BlockedException();
    }
    else if (m_NeedRecache) {
        m_NeedRecache = false;
        throw CNCStorage_RecacheException();
    }
}

inline bool
CNCBlobStorage::x_IsBlobFamilyExistsInCache(const string& key)
{
    if (key[0] == 0) {
        CNCCacheData* cache_data = m_KeysCache.GetPtr(key,
                                                TKeysCacheMap::eGetOnlyActive);
        return cache_data  &&  cache_data->GetBlobId() != 0;
    }
    else {
        string search_key(key);
        search_key.resize(search_key.size() - 4);
        string stored_key;
        return m_KeysCache.GetLowerBound(search_key, stored_key,
                                         TKeysCacheMap::eGetOnlyActive)
               &&  stored_key.size() == key.size()
               &&  NStr::CompareCase(stored_key, 0, search_key.size(), search_key) == 0;
    }
}

bool
CNCBlobStorage::IsBlobFamilyExists(const string& key)
{
    // dead_time should be read before check_part_id to avoid races when
    // caching has already finished and dead_time is already changing but we
    // don't know about it yet
    int dead_time = m_LastDeadTime;
    TNCDBFileId check_file_id = m_NotCachedFileId;
    if (x_IsBlobFamilyExistsInCache(key))
        return true;
    if (check_file_id != -1) {
        CSpinReadGuard guard(m_DBFilesLock);

        string max_key(key);
        // last 4 bytes are version, 5th byte from end is always 0, so if we
        // put 1 we will get key that is greater than any version of given
        // key/subkey
        max_key[max_key.size() - 4] = '\2';
        // With REVERSE_ITERATE code should look more ugly to be compilable
        // in WorkShop.
        NON_CONST_REVERSE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            TNCDBFileId file_id = it->first;
            CNCDBFile*  file    = it->second.file_obj.get();
            if (file_id > check_file_id  ||  file->GetType() != eNCMeta)
                continue;

            try {
                TMetaFileLock metafile(file);
                if (metafile->IsBlobFamilyExists(key, max_key, dead_time))
                    return true;
            }
            catch (CException& ex) {
                ERR_POST_X(1, "Error checking blob existence in file "
                              << it->second.file_name << ": " << ex);
            }
        }
    }
    return false;
}

void
CNCBlobStorage::x_FindBlobInFiles(const string& key)
{
    int dead_time = m_LastDeadTime;
    TNCDBFileId check_file_id = m_NotCachedFileId;
    if (check_file_id == -1)
        return;

    CSpinReadGuard guard(m_DBFilesLock);
    REVERSE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        TNCDBFileId file_id = it->first;
        CNCDBFile*  file    = it->second.file_obj.get();
        if (file_id > check_file_id  ||  file->GetType() != eNCMeta)
            continue;

        SNCBlobShortInfo blob_info;
        bool found = false;
        try {
            TMetaFileLock metafile(file);
            found = metafile->GetBlobShortInfo(key, dead_time, &blob_info);
        }
        catch (CException& ex) {
            ERR_POST_X(1, "Error reading blob id in file " << it->second.file_name
                          << ": " << ex);
        }
        if (found) {
            CNCCacheData new_data(blob_info.blob_id, file_id);
            new_data.SetCreatedCaching(true);
            CNCCacheData* data_in_cache;
            if (m_KeysCache.InsertOrGetPtr(key, new_data, TKeysCacheMap::eGetActiveAndPassive, &data_in_cache))
            {
                file->AddUsefulBlobs(1);
                m_DBFiles[blob_info.data_id].file_obj.get()->AddUsefulBlobs(1, blob_info.size);
                CNCStat::AddBlobCached(blob_info.cnt_reads);
            }
            break;
        }
    }
}

bool
CNCBlobStorage::ReadBlobInfo(SNCBlobVerData* ver_data)
{
    _ASSERT(ver_data->coords.blob_id != 0);
    try {
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        if (metafile->ReadBlobInfo(ver_data))
            return true;
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading blob information: " << ex);
    }
    return false;
}

bool
CNCBlobStorage::UpdateBlobInfo(const string&   blob_key,
                               SNCBlobVerData* ver_data)
{
    if (m_NotCachedFileId == -1
        &&  ver_data->old_dead_time != ver_data->dead_time
        &&  ver_data->data
        &&  m_BlobGeneration - ver_data->generation > m_CurFiles[eNCMeta].size())
    {
        ver_data->generation = m_BlobGeneration;
        SNCBlobVerData new_ver(ver_data);
        GetNewBlobCoords(&new_ver.coords);
        try {
            {{
                TDataFileLock datafile(this, new_ver.coords.data_id);
                datafile->WriteChunkData(new_ver.coords.blob_id, ver_data->data);
                datafile->AddUsefulBlobs(1, ver_data->data->GetSize());
            }}
            {{
                TMetaFileLock metafile(this, new_ver.coords.meta_id);
                metafile->WriteBlobInfo(blob_key, &new_ver);
                metafile->AddUsefulBlobs(1);
            }}
        }
        catch (CSQLITE_Exception& ex) {
            ERR_POST_X(1, "Cannot create new blob's meta-information: " << ex);
            return false;
        }
        swap(ver_data->coords, new_ver.coords);
        try {
            {{
                TMetaFileLock metafile(this, new_ver.coords.meta_id);
                metafile->DeleteBlobInfo(new_ver.coords.blob_id);
                metafile->DelUsefulBlobs(1);
            }}
            {{
                CSpinReadGuard guard(m_DBFilesLock);
                CNCDBFile* file = m_DBFiles[new_ver.coords.data_id].file_obj.get();
                file->SetBlobsGarbaged(1, new_ver.size);
            }}
        }
        catch (CSQLITE_Exception& ex) {
            ERR_POST_X(1, "Cannot delete old blob's meta-information: " << ex);
        }
        return true;
    }
    else {
        try {
            TMetaFileLock metafile(this, ver_data->coords.meta_id);
            metafile->UpdateBlobInfo(ver_data);
        }
        catch (CSQLITE_Exception& ex) {
            ERR_POST_X(1, "Cannot update blob's meta-information: " << ex);
        }
        return false;
    }
}

void
CNCBlobStorage::DeleteBlobInfo(const SNCBlobVerData* ver_data)
{
    if (ver_data->create_time != 0) {
        TMetaFileLock metafile(this, ver_data->coords.meta_id);
        try {
            metafile->DeleteBlobInfo(ver_data->coords.blob_id);
        }
        catch (CException& ex) {
            ERR_POST_X(1, "Error deleting blob from file "
                          << metafile->GetFileName() << ": " << ex);
        }
        metafile->DelUsefulBlobs(1);
    }
    if (ver_data->disk_size != 0) {
        TDataFileLock datafile(this, ver_data->coords.data_id);
        datafile->SetBlobsGarbaged(1, ver_data->disk_size);
    }
}

void
CNCBlobStorage::ReadChunkIds(SNCBlobVerData* ver_data)
{
    TMetaFileLock metafile(this, ver_data->coords.meta_id);
    try {
        metafile->GetChunkIds(ver_data->coords.blob_id, &ver_data->chunks);
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading chunk ids from file "
                      << metafile->GetFileName() << ": " << ex);
    }
}

bool
CNCBlobStorage::ReadChunkData(TNCDBFileId    file_id,
                              TNCChunkId     chunk_id,
                              CNCBlobBuffer* buffer)
{
    TDataFileLock datafile(this, file_id);
    try {
        if (datafile->ReadChunkData(chunk_id, buffer)) {
            CNCStat::AddChunkRead(buffer->GetSize());
            return true;
        }
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(1, "Error reading chunk data from file "
                      << datafile->GetFileName() << ": " << ex);
    }
    return false;
}


void
CNCBlobStorage::PrintStat(CPrintTextProxy& proxy)
{
    Int8 size_meta = 0, size_data = 0;
    unsigned int num_meta = 0, num_data = 0;
    Uint8 useful_cnt_meta = 0, garbage_cnt_meta = 0;
    Uint8 useful_cnt_data = 0, garbage_cnt_data = 0;
    Int8 useful_size_data = 0, garbage_size_data = 0;
    {{
        CSpinReadGuard guard(m_DBFilesLock);

        ITERATE(TNCDBFilesMap, it, m_DBFiles) {
            CNCDBFile* file = it->second.file_obj.get();
            Int8 size = file->GetFileSize();
            Uint8 useful_cnt, garbage_cnt;
            file->GetBlobsCounts(&useful_cnt, &garbage_cnt);
            if (file->GetType() == eNCMeta) {
                size_meta += size;
                useful_cnt_meta += useful_cnt;
                garbage_cnt_meta += garbage_cnt;
                ++num_meta;
            }
            else {
                Int8 useful_size, garbage_size;
                file->GetBlobsSizes(&useful_size, &garbage_size);
                size_data += size;
                useful_cnt_data += useful_cnt;
                garbage_cnt_data += garbage_cnt;
                useful_size_data += useful_size;
                garbage_size_data += garbage_size;
                ++num_data;
            }
        }
    }}

    proxy << "Now in cache - " << m_KeysCache.CountValues() << " values, "
                               << m_KeysCache.CountNodes()  << " nodes, "
                               << m_KeysCache.TreeHeight()  << " height" << endl;
    proxy << "Now in db    - "
                    << (size_meta + size_data) << " bytes, "
                    << num_meta << " metas of "
                    << size_meta << " bytes ("
                    << size_meta / num_meta << " avg), "
                    << num_data << " datas of "
                    << size_data << " bytes ("
                    << size_data / num_data << " avg)" << endl
          << "Metas blobs  - useful: "
                    << useful_cnt_meta << " cnt ("
                    << useful_cnt_meta / num_meta << " avg); garbage: "
                    << garbage_cnt_meta << " cnt ("
                    << garbage_cnt_meta / num_meta << " avg)" << endl
          << "Datas blobs  - useful: "
                    << useful_cnt_data << " cnt ("
                    << useful_cnt_data / num_data << " avg) of "
                    << useful_size_data << " bytes ("
                    << useful_size_data / num_data << " avg); garbage: "
                    << garbage_cnt_data << " cnt ("
                    << garbage_cnt_data / num_data << " avg) of "
                    << garbage_size_data << " bytes ("
                    << garbage_size_data / num_data << " avg)" << endl;
}

void
CNCBlobStorage::OnBlockedOpFinish(void)
{
    m_GCBlockWaiter.Post();
}

inline void
CNCBlobStorage::x_GC_DeleteExpired(const SNCBlobShortInfo& blob_info)
{
    m_GCAccessor->Prepare(blob_info.key, kEmptyStr, eNCGCDelete);
    x_InitializeAccessor(m_GCAccessor);
    if (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock) {
        m_GCBlockWaiter.Wait();
    }
    if (m_GCAccessor->IsBlobExists()  &&  m_GCAccessor->IsBlobExpired()) {
        m_GCAccessor->DeleteBlob();
    }
    m_GCAccessor->Deinitialize();
}

inline void
CNCBlobStorage::x_GC_CleanDBFile(CNCDBFile* metafile, int dead_before)
{
    try {
        TNCBlobsList blobs_list;
        int dead_after = m_LastDeadTime;
        TNCBlobId last_id = 0;
        do {
            x_CheckStopped();
            INFO_POST("Starting next GC batch");
            {{
                TMetaFileLock file_lock(metafile);
                file_lock->GetBlobsList(dead_after, last_id, dead_before,
                                        m_GCBatchSize, &blobs_list);
            }}
            ITERATE(TNCBlobsList, it, blobs_list) {
                x_CheckStopped();
                x_GC_DeleteExpired(*it);
                // Avoid this thread to be the most foreground task to execute,
                // let's give others a chance to work.
                NCBI_SCHED_YIELD();
            }
            INFO_POST("Batch processing is finished");
        }
        while (!blobs_list.empty());
    }
    catch (CException& ex) {
        ERR_POST_X(1, "Error processing file '" << metafile->GetFileName()
                       << "': " << ex);
    }
}

inline void
CNCBlobStorage::x_GC_CollectFilesStats(void)
{
    CNCStat::AddCacheMeasurement(m_KeysCache.CountValues(),
                                 m_KeysCache.CountNodes(),
                                 m_KeysCache.TreeHeight());

    // Nobody else changes m_DBParts, so working with it right away without
    // any mutexes.
    Int8 size_meta = 0, size_data = 0;
    unsigned int num_meta = 0, num_data = 0;
    Uint8 useful_cnt_meta = 0, garbage_cnt_meta = 0;
    Uint8 useful_cnt_data = 0, garbage_cnt_data = 0;
    Int8 useful_size_data = 0, garbage_size_data = 0;
    // With ITERATE code should look more ugly to be
    // compilable in WorkShop.
    NON_CONST_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
        CNCDBFile* file = it->second.file_obj.get();
        Int8 size = file->GetFileSize();
        Uint8 useful_cnt, garbage_cnt;
        file->GetBlobsCounts(&useful_cnt, &garbage_cnt);
        if (file->GetType() == eNCMeta) {
            size_meta += size;
            useful_cnt_meta += useful_cnt;
            garbage_cnt_meta += garbage_cnt;
            ++num_meta;
            CNCStat::AddMetaFileMeasurement(size, useful_cnt, garbage_cnt);
        }
        else {
            Int8 useful_size, garbage_size;
            file->GetBlobsSizes(&useful_size, &garbage_size);
            size_data += size;
            useful_cnt_data += useful_cnt;
            garbage_cnt_data += garbage_cnt;
            useful_size_data += useful_size;
            garbage_size_data += garbage_size;
            ++num_data;
            CNCStat::AddDataFileMeasurement(size, useful_cnt, garbage_cnt,
                                            useful_size, garbage_size);
        }
    }
    CNCStat::AddDBMeasurement(num_meta, num_data, size_meta, size_data,
                              useful_cnt_meta, garbage_cnt_meta,
                              useful_cnt_data, garbage_cnt_data,
                              useful_size_data, garbage_size_data);
}

inline void
CNCBlobStorage::x_GC_HeartBeat(void)
{
    m_KeysCache.HeartBeat();
    m_KeysDeleter.HeartBeat();

    bool next_gen = false;
    for (unsigned int i = 0; i < m_CurFiles[eNCMeta].size(); ++i) {
        TNCDBFileId file_id = m_CurFiles[eNCMeta][i];
        CNCDBFile*  file    = m_DBFiles[file_id].file_obj.get();
        Uint8 useful, garbage;
        file->GetBlobsCounts(&useful, &garbage);
        Uint8 total = useful + garbage;
        Int8  size  = file->GetFileSize();
        if ((size >= m_MaxFileSize[eNCMeta]
             ||  (size >= m_MinFileSize[eNCMeta]
                  &&  useful / total <= m_MinUsefulPct[eNCMeta])))
        {
            x_CreateNewFile(eNCMeta, i);
            next_gen = true;
        }
    }
    for (unsigned int i = 0; i < m_CurFiles[eNCData].size(); ++i) {
        TNCDBFileId file_id = m_CurFiles[eNCData][i];
        CNCDBFile*  file    = m_DBFiles[file_id].file_obj.get();
        Int8  useful_size, garbage_size;
        file->GetBlobsSizes(&useful_size, &garbage_size);
        Int8 total_blobs_size = useful_size + garbage_size;
        Int8 file_size  = file->GetFileSize();
        if ((file_size >= m_MaxFileSize[eNCData]
             ||  (file_size >= m_MinFileSize[eNCData]
                  &&  useful_size / total_blobs_size <= m_MinUsefulPct[eNCData])))
        {
            x_CreateNewFile(eNCData, i);
            next_gen = true;
        }
    }
    if (next_gen)
        ++m_BlobGeneration;
}

inline bool
CNCBlobStorage::x_CheckFileAging(TNCDBFileId file_id, CNCDBFile* file)
{
    ENCDBFileType file_type = file->GetType();
    const vector<TNCDBFileId>& files_list = m_CurFiles[file_type];
    unsigned int part_num = 0;
    for (; part_num < files_list.size(); ++part_num) {
        if (files_list[part_num] == file_id)
            break;
    }
    if (part_num != files_list.size()) {
        return false;
    }
    Uint8 useful_cnt, garbage_cnt;
    file->GetBlobsCounts(&useful_cnt, &garbage_cnt);
    return useful_cnt == 0;
}

void
CNCBlobStorage::x_DoBackgroundWork(void)
{
    // Make background postings to be all in the same request id
    CRef<CRequestContext> diag_context(new CRequestContext());
    diag_context->SetRequestID();
    CDiagContext::SetRequestContext(diag_context);

    try {
        for (;;) {
            try {
                INFO_POST("Caching of blobs info is started");
                TUsefulCntMap useful_sizes, useful_blobs;
                // Nobody else changes m_DBFiles, so iterating right over it.
                // With REVERSE_ITERATE code should look more ugly to be
                // compilable in WorkShop.
                NON_CONST_REVERSE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
                    CNCDBFile* file = it->second.file_obj.get();
                    if (file->GetType() != eNCMeta)
                        continue;
                    TNCDBFileId file_id = it->first;
                    m_NotCachedFileId = file_id;

                    Uint8 cnt_blobs = 0;
                    try {
                        TNCBlobsList blobs_list;
                        TNCBlobId last_id = 0;
                        int dead_after = m_LastDeadTime;
                        do {
                            x_CheckStopped();
                            {{
                                TMetaFileLock file_lock(file);
                                file_lock->GetBlobsList(dead_after, last_id,
                                                        numeric_limits<int>::max(),
                                                        m_GCBatchSize, &blobs_list);
                            }}
                            ITERATE(TNCBlobsList, it, blobs_list) {
                                const SNCBlobShortInfo& blob_info = *it;
                                CNCCacheData new_data(blob_info.blob_id, file_id);
                                new_data.SetCreatedCaching(true);
                                CNCCacheData* data_in_cache;
                                bool was_added = m_KeysCache.InsertOrGetPtr(blob_info.key, new_data, TKeysCacheMap::eGetActiveAndPassive, &data_in_cache);
                                if (!was_added) {
                                    m_GCAccessor->Prepare(blob_info.key, kEmptyStr, eNCRead);
                                    m_GCAccessor->Initialize(data_in_cache);
                                    if (m_GCAccessor->ObtainMetaInfo(this) == eNCWouldBlock)
                                        m_GCBlockWaiter.Wait();
                                    if (!m_GCAccessor->IsBlobExists()) {
                                        // I.e. somebody already deleted it, so we should ignore what we found anyway
                                        TMetaFileLock metafile(file);
                                        try {
                                            metafile->DeleteBlobInfo(blob_info.blob_id);
                                        }
                                        catch (CException& ex) {
                                            ERR_POST_X(1, "Error deleting blob from file "
                                                          << file->GetFileName() << ": " << ex);
                                        }
                                    }
                                    else if (m_GCAccessor->GetBlobId() != blob_info.blob_id) {
                                        CRef<SNCBlobVerData> new_ver(m_GCAccessor->GetNewVersion());
                                        new_ver->coords.blob_id = blob_info.blob_id;
                                        new_ver->coords.meta_id = file_id;
                                        if (ReadBlobInfo(new_ver)) {
                                            if (m_GCAccessor->SetCurVerIfNewer(new_ver)) {
                                                was_added = true;
                                            }
                                            // If our version is not newer then it will be deleted on destruction of new_ver.
                                            // If our was newer then previous will be destroyed on deinitialization of accessor
                                            // and thus will be deleted from database.
                                        }
                                    }
                                    m_GCAccessor->Deinitialize();
                                }
                                if (was_added) {
                                    ++cnt_blobs;
                                    useful_sizes[blob_info.data_id] += blob_info.size;
                                    ++useful_blobs[blob_info.data_id];
                                    CNCStat::AddBlobCached(blob_info.cnt_reads);
                                }
                            }
                        }
                        while (!blobs_list.empty());
                    }
                    catch (CException& ex) {
                        // Try to recover from any database errors by just
                        // ignoring it and trying to work further.
                        ERR_POST_X(1, "Database file " << it->second.file_name
                                      << " was not cached properly: " << ex);
                    }
                    file->AddUsefulBlobs(cnt_blobs);
                }
                m_NotCachedFileId = -1;

                ITERATE(TUsefulCntMap, it, useful_sizes) {
                    CNCDBFile* datafile = m_DBFiles[it->first].file_obj.get();
                    datafile->AddUsefulBlobs(useful_blobs[it->first], it->second);
                }
                m_CachingTrigger.OperationCompleted();

                INFO_POST("Caching of blobs info is finished");

                for (;;) {
                    for (int i = 0; i < m_GCRunDelay; ++i) {
                        m_StopTrigger.TryWait(1, 0);
                        x_GC_HeartBeat();
                        try {
                            x_CheckStopped();
                        }
                        catch (CNCStorage_BlockedException&) {
                            // If storage is blocked heart beat still can work
                        }
                    }

                    m_GCInWork = true;
                    try {
                        x_CheckStopped();

                        INFO_POST("Starting next GC cycle");
                        x_GC_CollectFilesStats();

                        int next_dead = int(time(NULL));
                        ERASE_ITERATE(TNCDBFilesMap, it, m_DBFiles) {
                            CNCDBFile* file = it->second.file_obj.get();
                            if (file->GetType() == eNCMeta) {
                                x_GC_CleanDBFile(file, next_dead);
                            }
                            if (x_CheckFileAging(it->second.file_id, file)) {
                                x_DeleteDBFile(it);
                            }
                        }
                        m_LastDeadTime = next_dead;
                        INFO_POST("GC cycle ended");
                    }
                    catch (CNCStorage_BlockedException&) {
                        // Do nothing - we're already in place where we want to be
                        INFO_POST("Storage block is in progress");
                    }
                    m_GCInWork = false;
                }
            }
            catch (CNCStorage_RecacheException&) {
                // Do nothing - we're already in place where we want to be
            }
        }
    }
    catch (CNCStorage_StoppedException&) {
        // Do nothing - we're already in place where we want to be
    }
    INFO_POST("Background thread exits normally");
}

END_NCBI_SCOPE
