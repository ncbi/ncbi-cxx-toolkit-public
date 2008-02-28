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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: BDB library utilities. 
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <bdb/bdb_util.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/error_codes.hpp>
#include <util/strsearch.hpp>


#define NCBI_USE_ERRCODE_X   Bdb_Util

BEGIN_NCBI_SCOPE


/// Check if field is a variant of string, returns pointer on the
/// internal buffer (or NULL).
///
/// @internal
static
const char* BDB_GetStringFieldBuf(const CBDB_Field& fld)
{
    const CBDB_FieldString* str_fld = 
          dynamic_cast<const CBDB_FieldString*>(&fld);
    if (str_fld) {
        return (const char*)fld.GetBuffer();
    } else {
        const CBDB_FieldLString* lstr_fld = 
              dynamic_cast<const CBDB_FieldLString*>(&fld);
        if (lstr_fld) {
            return (const char*)fld.GetBuffer();
        }
    }
    return 0;
}

/// Search for value in the field buffer
///
/// @return -1 if not found, otherwise field number
///
/// @internal
static
int BDB_find_field(const CBDB_BufferManager& buffer_man,
                   const CBoyerMooreMatcher& matcher,
                   string*                   tmp_str)
{
    int fidx = -1;
    unsigned int fcount = buffer_man.FieldCount();
    for (unsigned i = 0; i < fcount; ++i) {
        const CBDB_Field& fld = buffer_man.GetField(i);

        if (fld.IsNull()) {
            continue;
        }

        unsigned str_buf_len;
        const char* str_buf = BDB_GetStringFieldBuf(fld);
        
        // for string based fields it should be non-0
        
        if (str_buf) {
            str_buf_len = fld.GetDataLength(str_buf);
            match:
            if (str_buf_len) {
                int pos = matcher.Search(str_buf, 0, str_buf_len);
                if (pos >= 0) {
                    fidx = i;
                    break;
                }
            }
        } else {
            if (tmp_str) {
                fld.ToString(*tmp_str);
                str_buf = tmp_str->c_str();
                str_buf_len = tmp_str->length();
                goto match;
            }
        }
        

    } // for i
    return fidx;
}

CBDB_File::TUnifiedFieldIndex BDB_find_field(const CBDB_File& dbf, 
                                             const CBoyerMooreMatcher& matcher,
                                             string* tmp_str)
{
    CBDB_File::TUnifiedFieldIndex fidx = 0;
    const CBDB_BufferManager* buffer_man;
    buffer_man =  dbf.GetKeyBuffer();
    if (buffer_man) {
        fidx = BDB_find_field(*buffer_man, matcher, tmp_str);
        if (fidx >= 0) {
            fidx = BDB_GetUFieldIdx(fidx, true /* key */);
            return fidx;
        } else {
            fidx = 0;
        }
    }

    buffer_man =  dbf.GetDataBuffer();
    if (buffer_man) {
        fidx = BDB_find_field(*buffer_man, matcher, tmp_str);
        if (fidx >= 0) {
            fidx = BDB_GetUFieldIdx(fidx, false /* key */);
            return fidx;        
        } else {
            fidx = 0;
        }
    }

    return fidx;
}

int BDB_get_rowid(const CBDB_File& dbf)
{
    const CBDB_BufferManager* buffer_man;
    buffer_man =  dbf.GetKeyBuffer();
    if (!buffer_man) {
        return 0;
    }
    const CBDB_Field& fld = buffer_man->GetField(0);
    {{
    const CBDB_FieldInt2* fi = dynamic_cast<const CBDB_FieldInt2*>(&fld);
    if (fi)
        return fi->Get();
    }}
    {{
    const CBDB_FieldInt4* fi = dynamic_cast<const CBDB_FieldInt4*>(&fld);
    if (fi)
        return fi->Get();
    }}
    {{
    const CBDB_FieldUint4* fi = dynamic_cast<const CBDB_FieldUint4*>(&fld);
    if (fi)
        return fi->Get();
    }}

    return 0;
}




//////////////////////////////////////////////////////////////////////////////
///
/// Log a parameter in a human-readable format
///
/// @internal
///
template <typename T>
void s_LogEnvParam(const string& param_name,
                   const T& param_value,
                   const string& units = kEmptyStr)
{
    LOG_POST_X(1, Info
               << setw(20) << param_name
               << " : "
               << param_value << units);
}

/// Log a parameter in a human-readable format
///
/// @internal
static
void s_LogEnvParam(const string& param_name,
                   const bool& param_value)
{
    LOG_POST_X(2, Info
               << setw(20) << param_name
               << " : "
               << (param_value ? "true" : "false"));
}



static const char* kEnvParam_type                 = "env_type";
static const char* kEnvParam_path                 = "path";
static const char* kEnvParam_reinit               = "reinit";
static const char* kEnvParam_errfile              = "error_file";

static const char* kEnvParam_mem_size             = "mem_size";
static const char* kEnvParam_cache_size           = "cache_size";
static const char* kEnvParam_cache_num            = "cache_num";
static const char* kEnvParam_log_mem_size         = "log_mem_size";
static const char* kEnvParam_write_sync           = "write_sync";
static const char* kEnvParam_direct_db            = "direct_db";
static const char* kEnvParam_direct_log           = "direct_log";
static const char* kEnvParam_transaction_log_path = "transaction_log_path";
static const char* kEnvParam_log_file_max         = "log_file_max";
static const char* kEnvParam_log_autoremove       = "log_autoremove";
static const char* kEnvParam_lock_timeout         = "lock_timeout";
static const char* kEnvParam_TAS_spins            = "tas_spins";
static const char* kEnvParam_max_locks            = "max_locks";
static const char* kEnvParam_max_lock_objects     = "max_lock_objects";
static const char* kEnvParam_mp_maxwrite          = "mp_maxwrite";
static const char* kEnvParam_mp_maxwrite_sleep    = "mp_maxwrite_sleep";

static const char* kEnvParam_checkpoint_interval  = "checkpoint_interval";
static const char* kEnvParam_enable_checkpoint    = "enable_checkpoint";
static const char* kEnvParam_checkpoint_kb        = "checkpoint_kb";
static const char* kEnvParam_checkpoint_min       = "checkpoint_min";
static const char* kEnvParam_memp_trickle_percent = "memp_trickle_percent";


CBDB_Env* BDB_CreateEnv(const CNcbiRegistry& reg, 
                        const string& section_name)
{
    auto_ptr<CBDB_Env> env;

    CBDB_Env::EEnvType e_type = CBDB_Env::eEnvTransactional;

    string env_type = 
        reg.GetString(section_name, kEnvParam_type, "transactional");
    NStr::ToLower(env_type);
    if (NStr::EqualNocase(env_type, "locking")) {
        e_type = CBDB_Env::eEnvLocking;
    } else if (NStr::EqualNocase(env_type, "transactional")) {
        e_type = CBDB_Env::eEnvTransactional;
    } else if (NStr::EqualNocase(env_type, "concurrent")) {
        e_type = CBDB_Env::eEnvConcurrent;
    }

    // path to the environment, re-initialization options
    
    string path = 
        reg.GetString(section_name, kEnvParam_path, ".");
    path = CDirEntry::AddTrailingPathSeparator(path);
    s_LogEnvParam("BDB env.path", path);

    bool reinit =
        reg.GetBool(section_name, kEnvParam_reinit, false, 0, 
                    IRegistry::eReturn);


    // Make sure our directory exists
    {{
        CDir dir(path);
        if ( !dir.Exists() ) {
            dir.Create();
        } else {
            if (reinit) {
                s_LogEnvParam("BDB re-initialization", reinit);

                dir.Remove();
                dir.Create();
            }
        }
    }}

    // the error file for BDB errors
    string err_path = reg.GetString(section_name, kEnvParam_errfile,
                                    path + "/bdb_err.log");
    
    if (!err_path.empty()) {
        string dir, base, ext;
        CDirEntry::SplitPath(err_path, &dir, &base, &ext);
        if (dir.empty()) {
            err_path = CDirEntry::MakePath(path, base, ext);
        }
    }

    // cache RAM size
    string cache_size_str;
    if (reg.HasEntry(section_name, kEnvParam_cache_size)) {
        cache_size_str = 
            reg.GetString(section_name, kEnvParam_cache_size, "");
    } else {
        cache_size_str = 
            reg.GetString(section_name, kEnvParam_mem_size, "");
    }
    Uint8 cache_size = 
        NStr::StringToUInt8_DataSize(cache_size_str, NStr::fConvErr_NoThrow);

    int cache_num = 
        reg.GetInt(section_name, kEnvParam_cache_num, 1, 0,
                   IRegistry::eReturn);

    // in-memory log size
    string log_mem_size_str;
    log_mem_size_str = 
        reg.GetString(section_name, kEnvParam_log_mem_size, "");
    Uint8 log_mem_size = 
        NStr::StringToUInt8_DataSize(log_mem_size_str, NStr::fConvErr_NoThrow);

    // transaction log path
    string log_path = 
        reg.GetString(section_name, kEnvParam_transaction_log_path,
                      path);

    // transaction log file max
    string log_max_str;
    log_max_str = 
        reg.GetString(section_name, kEnvParam_log_file_max, "200M");
    Uint8 log_max = NStr::StringToUInt8_DataSize(log_max_str);
    if (log_max < 1024 * 1024) {
        log_max = 1024 * 1024;
    }

    // automatic log truncation
    bool log_autoremove =
        reg.GetBool(section_name, kEnvParam_log_autoremove, true, 0, 
                    IRegistry::eReturn);

    // transaction sync
    bool write_sync =
        reg.GetBool(section_name, kEnvParam_write_sync, false, 0, 
                    IRegistry::eReturn);

    // direct IO
    bool direct_db =
        reg.GetBool(section_name, kEnvParam_direct_db, false, 0, 
                    IRegistry::eReturn);

    bool direct_log =
        reg.GetBool(section_name, kEnvParam_direct_log, false, 0, 
                    IRegistry::eReturn);

    // deadlock detection interval
    int lock_timeout =
        reg.GetInt(section_name, kEnvParam_lock_timeout, 0, 0,
                   IRegistry::eReturn);

    // BDB spin lock spin count
    int tas_spins =
        reg.GetInt(section_name, kEnvParam_TAS_spins, 0, 0,
                   IRegistry::eReturn);

    // maximum locks and locked objects
    // lock starvation is a critical flaw in a transactional situation,
    // and will lead to deadlocks or worse
    //
    int max_locks =
        reg.GetInt(section_name, kEnvParam_max_locks, 0, 0,
                   IRegistry::eReturn);
    int max_lock_objects =
        reg.GetInt(section_name, kEnvParam_max_lock_objects, 0, 0,
                   IRegistry::eReturn);

    // mempool write governors
    // we can limit the amount written in any sync operation, as well as
    // determine a sleep time between successive writes
    //
    int mp_maxwrite = 
        reg.GetInt(section_name, kEnvParam_mp_maxwrite, 0, 0,
                   IRegistry::eReturn);
    int mp_maxwrite_sleep = 
        reg.GetInt(section_name, kEnvParam_mp_maxwrite_sleep, 0, 0,
                   IRegistry::eReturn);

    // checkpointing parameters
    //
    int checkpoint_interval = 
        reg.GetInt(section_name, kEnvParam_checkpoint_interval, 30, 0,
                   IRegistry::eReturn);

    bool enable_checkpoint =
        reg.GetBool(section_name, kEnvParam_enable_checkpoint, true, 0, 
                    IRegistry::eReturn);
    
    int checkpoint_kb = 
        reg.GetInt(section_name, kEnvParam_checkpoint_kb, 256, 0,
                   IRegistry::eReturn);

    int checkpoint_min = 
        reg.GetInt(section_name, kEnvParam_checkpoint_min, 5, 0,
                   IRegistry::eReturn);

    int memp_trickle_percent = 
        reg.GetInt(section_name, kEnvParam_memp_trickle_percent, 60, 0,
                   IRegistry::eReturn);


    //
    // establish our BDB environment
    //
    try {
        env.reset(new CBDB_Env());
        s_LogEnvParam("BDB error file)",  err_path);
        env->OpenErrFile(err_path);
        if (cache_size) {
            s_LogEnvParam("BDB cache size)",  cache_size);
            env->SetCacheSize(cache_size, cache_num);
        }
        if (log_mem_size) {
            env->SetLogInMemory(true);
            s_LogEnvParam("BDB log mem size (in-memory log)",  log_mem_size);
            env->SetLogBSize((unsigned)log_mem_size);        
        }

        if (!log_path.empty() && log_path != path) {
            s_LogEnvParam("BDB log path",  log_path);
            env->SetLogDir(log_path);
        }

        if (log_max) {
            s_LogEnvParam("BDB log file max size",  log_max);
            env->SetLogFileMax((unsigned)log_max);
        }
        s_LogEnvParam("BDB log auto remove",  log_autoremove);
        env->SetLogAutoRemove(log_autoremove);

        s_LogEnvParam("BDB direct db",  direct_db);
        s_LogEnvParam("BDB direct log", direct_log);
        env->SetDirectDB(direct_db);
        env->SetDirectLog(direct_log);


        if (lock_timeout) {
            s_LogEnvParam("BDB lock timeout", lock_timeout);
            env->SetLockTimeout(lock_timeout);
        }        
        if (tas_spins) {
            s_LogEnvParam("BDB TAS spins", tas_spins);
            env->SetTasSpins(tas_spins);
        }
        if (max_locks) {
            s_LogEnvParam("BDB max locks", max_locks);
            env->SetMaxLocks(max_locks);
        }
        if (max_lock_objects) {
            s_LogEnvParam("BDB max lock objects", max_lock_objects);
            env->SetMaxLockObjects(max_lock_objects);
        }
        s_LogEnvParam("BDB mempool max write", mp_maxwrite);
        s_LogEnvParam("BDB mempool max write sleep", mp_maxwrite_sleep);
        env->SetMpMaxWrite(mp_maxwrite, mp_maxwrite_sleep);

        if (checkpoint_kb) {
            s_LogEnvParam("BDB checkpoint KB", checkpoint_kb);
            env->SetCheckPointKB(checkpoint_kb);
        }
        if (checkpoint_min) {
            s_LogEnvParam("BDB checkpoint minitues", checkpoint_min);
            env->SetCheckPointKB(checkpoint_min);
        }
        env->EnableCheckPoint(enable_checkpoint);


        switch (e_type) {
        case CBDB_Env::eEnvTransactional:
            env->OpenWithTrans(path, 
                               CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
            break;
        case CBDB_Env::eEnvLocking:
            env->OpenWithLocks(path);
            break;
        case CBDB_Env::eEnvConcurrent:
            env->OpenConcurrentDB(path);
            break;
        default:
            _ASSERT(0);
        }


        if (env->IsTransactional()) {
            if (write_sync) {
                s_LogEnvParam("BDB transaction sync", "Sync");
                env->SetTransactionSync(CBDB_Transaction::eTransSync);
            } else {
                s_LogEnvParam("BDB transaction sync", "Async");
                env->SetTransactionSync(CBDB_Transaction::eTransASync);
            }
            // TODO: another parameter to add
            s_LogEnvParam("BDB deadlock detection", "Default");
            env->SetLkDetect(CBDB_Env::eDeadLock_Default);
        }


        if (checkpoint_interval) {
            s_LogEnvParam("BDB checkpoint interval (sec)", checkpoint_interval);
            s_LogEnvParam("BDB trickle percent", memp_trickle_percent);
            env->RunCheckpointThread(checkpoint_interval, memp_trickle_percent);
        }
    }
    catch (CBDB_ErrnoException& e) {
        if (e.IsRecovery()) {
            env.reset();
            BDB_RecoverEnv(path, true);
        }
        throw;
    }

    return env.release();
}




END_NCBI_SCOPE
