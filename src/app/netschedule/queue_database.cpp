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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: NetSchedule queue collection and database managenement.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/ncbireg.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_util.hpp>

#include <util/time_line.hpp>

#include "queue_database.hpp"
#include "ns_util.hpp"
#include "netschedule_version.hpp"
#include "ns_server.hpp"
#include "ns_handler.hpp"


BEGIN_NCBI_SCOPE


#define GetUIntNoErr(name, dflt) \
    (unsigned) bdb_conf.GetInt("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetSizeNoErr(name, dflt) \
    (unsigned) bdb_conf.GetDataSize("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetBoolNoErr(name, dflt) \
    bdb_conf.GetBool("netschedule", name, CConfig::eErr_NoThrow, dflt)


/////////////////////////////////////////////////////////////////////////////
// SNSDBEnvironmentParams implementation

bool SNSDBEnvironmentParams::Read(const IRegistry& reg, const string& sname)
{
    CConfig                         conf(reg);
    const CConfig::TParamTree*      param_tree = conf.GetTree();
    const TPluginManagerParamTree*  bdb_tree = param_tree->FindSubNode(sname);

    if (!bdb_tree)
        return false;

    CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
    db_storage_ver = bdb_conf.GetString("netschedule", "force_storage_version",
        CConfig::eErr_NoThrow, NETSCHEDULED_STORAGE_VERSION);

    db_path = bdb_conf.GetString("netschedule", "path", CConfig::eErr_Throw);
    db_log_path = ""; // doesn't work yet
        // bdb_conf.GetString("netschedule", "transaction_log_path",
        // CConfig::eErr_NoThrow, "");

    max_queues        = GetUIntNoErr("max_queues", 50);

    cache_ram_size    = GetSizeNoErr("mem_size", 0);
    mutex_max         = GetUIntNoErr("mutex_max", 0);
    max_locks         = GetUIntNoErr("max_locks", 0);
    max_lockers       = GetUIntNoErr("max_lockers", 0);
    max_lockobjects   = GetUIntNoErr("max_lockobjects", 0);
    log_mem_size      = GetSizeNoErr("log_mem_size", 0);
    // max_trans is derivative, so we do not read it here

    checkpoint_kb     = GetUIntNoErr("checkpoint_kb", 5000);
    checkpoint_min    = GetUIntNoErr("checkpoint_min", 5);

    sync_transactions = GetBoolNoErr("sync_transactions", false);
    direct_db         = GetBoolNoErr("direct_db", false);
    direct_log        = GetBoolNoErr("direct_log", false);
    private_env       = GetBoolNoErr("private_env", false);
    return true;
}


static string  s_DBEnvParameters[] =
{
    "path",                     // 0
    "transaction_log_path",     // 1
    "max_queues",               // 2
    "mem_size",                 // 3
    "mutex_max",                // 4
    "max_locks",                // 5
    "max_lockers",              // 6
    "max_lockobjects",          // 7
    "log_mem_size",             // 8
    "checkpoint_kb",            // 9
    "checkpoint_min",           // 10
    "sync_transactions",        // 11
    "direct_db",                // 12
    "direct_log",               // 13
    "private_env"               // 14
};
static unsigned s_NumDBEnvParameters = sizeof(s_DBEnvParameters) /
                                       sizeof(string);


unsigned SNSDBEnvironmentParams::GetNumParams() const
{
    return s_NumDBEnvParameters;
}


string SNSDBEnvironmentParams::GetParamName(unsigned n) const
{
    if (n >= s_NumDBEnvParameters)
        return kEmptyStr;
    return s_DBEnvParameters[n];
}


string SNSDBEnvironmentParams::GetParamValue(unsigned n) const
{
    switch (n) {
    case 0:  return db_path;
    case 1:  return db_log_path;
    case 2:  return NStr::UIntToString(max_queues);
    case 3:  return NStr::UIntToString(cache_ram_size);
    case 4:  return NStr::UIntToString(mutex_max);
    case 5:  return NStr::UIntToString(max_locks);
    case 6:  return NStr::UIntToString(max_lockers);
    case 7:  return NStr::UIntToString(max_lockobjects);
    case 8:  return NStr::UIntToString(log_mem_size);
    case 9:  return NStr::UIntToString(checkpoint_kb);
    case 10: return NStr::UIntToString(checkpoint_min);
    case 11: return NStr::BoolToString(sync_transactions);
    case 12: return NStr::BoolToString(direct_db);
    case 13: return NStr::BoolToString(direct_log);
    case 14: return NStr::BoolToString(private_env);
    default: return kEmptyStr;
    }
}

/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation

CQueueDataBase::CQueueDataBase(CNetScheduleServer *            server,
                               const SNSDBEnvironmentParams &  params,
                               bool                            reinit)
: m_Host(server->GetBackgroundHost()),
  m_Executor(server->GetRequestExecutor()),
  m_Env(0),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_Server(server),
  m_PurgeQueue(""),
  m_PurgeStatusIndex(0),
  m_PurgeJobScanned(0)
{
    // Creates/re-creates if needed and opens DB tables
    x_Open(params, reinit);

    // Read queue classes and queues descriptions from the DB
    m_QueueClasses = x_ReadDBQueueDescriptions("qclass");
    TQueueParams    queues_from_db = x_ReadDBQueueDescriptions("queue");

    // Mount all the queues in accordance with the DB info
    for (TQueueParams::const_iterator  k = queues_from_db.begin();
         k != queues_from_db.end(); ++k ) {
         SQueueDbBlock *   block = m_QueueDbBlockArray.Get(k->second.position);

        if (block == NULL) {
            // That means the DB has more a queue allocated in a slot which
            // index exceeds the max tables configured in .ini file
            try {
                // No drain closing
                Close(false);
            } catch (...) {}

            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Error detected: the DB has a queue allocated in a "
                       "slot (" + NStr::NumericToString(k->second.position) +
                       ") which index exceeds the max configured number "
                       "of queues. Consider increasing the "
                       "[bdb]/max_queues value.");
        }

        // OK, the block is available
        m_QueueDbBlockArray.Allocate(k->second.position);

        // This will actually create CQueue and insert it to m_Queues
        x_CreateAndMountQueue(k->first, k->second, block);
    }
    return;
}


CQueueDataBase::~CQueueDataBase()
{
    try {
        // No drain closing
        Close(false);
    } catch (...) {}
}


void  CQueueDataBase::x_Open(const SNSDBEnvironmentParams &  params,
                             bool                            reinit)
{
    const string &   db_path     = params.db_path;
    const string &   db_log_path = params.db_log_path;
    if (reinit) {
        CDir    dir(db_path);

        dir.Remove();
        LOG_POST(Message << Warning
                         << "Reinitialization. " << db_path << " removed.");
    }

    m_Path = CDirEntry::AddTrailingPathSeparator(db_path);

    if (x_IsDBDrained()) {
        CDir    dir(db_path);

        dir.Remove();
        LOG_POST(Message << Warning
                         << "Reinitialization due to the DB was drained "
                            "on last shutdown. " << db_path << " removed.");
    }

    string trailed_log_path = CDirEntry::AddTrailingPathSeparator(db_log_path);

    const string* effective_log_path = trailed_log_path.empty() ?
                                       &m_Path :
                                       &trailed_log_path;

    bool        fresh_db(false);
    {{
        CDir    dir(m_Path);
        if ( !dir.Exists() ) {
            fresh_db = true;
            dir.Create();
        }
    }}
    x_SetSignallingFile(false);

    string      db_storage_ver;
    CFile       ver_file(CFile::MakePath(m_Path, "DB_STORAGE_VER"));
    if (fresh_db) {
        db_storage_ver = NETSCHEDULED_STORAGE_VERSION;
        CFileIO f;
        f.Open(ver_file.GetPath(), CFileIO_Base::eCreate,
                                   CFileIO_Base::eReadWrite);
        f.Write(db_storage_ver.data(), db_storage_ver.size());
        f.Close();
    } else {
        if (!ver_file.Exists()) {
            // Last storage version which does not have DB_STORAGE_VER file
            db_storage_ver = "4.0.0";
        } else {
            CFileIO f;
            char buf[32];
            f.Open(ver_file.GetPath(), CFileIO_Base::eOpen, CFileIO_Base::eRead);
            size_t n = f.Read(buf, sizeof(buf));
            db_storage_ver.append(buf, n);
            NStr::TruncateSpacesInPlace(db_storage_ver, NStr::eTrunc_End);
            f.Close();
        }
        if (db_storage_ver != params.db_storage_ver)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Error detected: Storage version mismatch, required: " +
                       params.db_storage_ver + ", found: " + db_storage_ver);
    }

    delete m_Env;
    m_Env = new CBDB_Env();

    if (!trailed_log_path.empty()) {
        m_Env->SetLogDir(trailed_log_path);
    }

    // memory log option. we probably need to reset LSN
    // numbers
/*
    if (log_mem_size) {
    }

    // Private environment for LSN recovery
    {{
        m_Name = "jsqueue";
        string err_file = m_Path + "err" + string(m_Name) + ".log";
        m_Env->OpenErrFile(err_file.c_str());

        m_Env->SetCacheSize(1024*1024);
        m_Env->OpenPrivate(path.c_str());

        m_Env->LsnReset("jsq_test.db");

        delete m_Env;
        m_Env = new CBDB_Env();
    }}
*/

    m_Name = "jsqueue";
    string err_file = m_Path + "err" + string(m_Name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    m_Env->SetLogRegionMax(512 * 1024);
    if (params.log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(params.log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    // Check if bdb env. files are in place and try to join
    CDir            dir(*effective_log_path);
    CDir::TEntries  fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (!params.private_env && !fl.empty()) {
        // Opening db with recover flags is unreliable.
        BDB_RecoverEnv(m_Path, false);
        LOG_POST(Message << Warning << "Running recovery...");
    }

    CBDB_Env::TEnvOpenFlags opt = CBDB_Env::eThreaded;
    if (params.private_env)
        opt |= CBDB_Env::ePrivate;

    if (params.cache_ram_size)
        m_Env->SetCacheSize(params.cache_ram_size);
    if (params.mutex_max)
        m_Env->MutexSetMax(params.mutex_max);
    if (params.max_locks)
        m_Env->SetMaxLocks(params.max_locks);
    if (params.max_lockers)
        m_Env->SetMaxLockers(params.max_lockers);
    if (params.max_lockobjects)
        m_Env->SetMaxLockObjects(params.max_lockobjects);
    if (params.max_trans)
        m_Env->SetTransactionMax(params.max_trans);
    m_Env->SetTransactionSync(params.sync_transactions ?
                                  CBDB_Transaction::eTransSync :
                                  CBDB_Transaction::eTransASync);

    m_Env->OpenWithTrans(m_Path.c_str(), opt);
    LOG_POST(Message << Warning
                     << "Opened " << (params.private_env ? "private " : "")
                     << "BDB environment with "
                     << m_Env->GetMaxLocks() << " max locks, "
//                    << m_Env->GetMaxLockers() << " max lockers, "
//                    << m_Env->GetMaxLockObjects() << " max lock objects, "
                     << (m_Env->GetTransactionSync() == CBDB_Transaction::eTransSync ?
                                          "" : "a") << "syncronous transactions, "
                     << m_Env->MutexGetMax() <<  " max mutexes");


    m_Env->SetDirectDB(params.direct_db);
    m_Env->SetDirectLog(params.direct_log);

    m_Env->SetCheckPointKB(params.checkpoint_kb);
    m_Env->SetCheckPointMin(params.checkpoint_min);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->ForceTransactionCheckpoint();
        m_Env->CleanLog();
    }

    m_QueueDescriptionDB.SetEnv(*m_Env);
    m_QueueDescriptionDB.Open("sys_qdescr.db", CBDB_RawFile::eReadWriteCreate);

    // Allocate SQueueDbBlock's here, open/create corresponding databases
    m_QueueDbBlockArray.Init(*m_Env, m_Path, params.max_queues);
    return;
}


TQueueParams
CQueueDataBase::x_ReadDBQueueDescriptions(const string &  expected_prefix)
{
    // Reads what is stored in the DB about currently served queues
    m_QueueDescriptionDB.SetTransaction(NULL);

    CBDB_FileCursor     cur(m_QueueDescriptionDB);
    TQueueParams        queues;

    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        string      prefix;
        string      queue_name;

        NStr::SplitInTwo(m_QueueDescriptionDB.queue.GetString(),
                         "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, expected_prefix) != 0)
            continue;

        SQueueParameters    params;
        params.kind = m_QueueDescriptionDB.kind;
        params.position = m_QueueDescriptionDB.pos;
        params.delete_request = m_QueueDescriptionDB.delete_request;
        params.qclass = m_QueueDescriptionDB.qclass;
        params.timeout = m_QueueDescriptionDB.timeout;
        params.notif_hifreq_interval = m_QueueDescriptionDB.notif_hifreq_interval;
        params.notif_hifreq_period = m_QueueDescriptionDB.notif_hifreq_period;
        params.notif_lofreq_mult = m_QueueDescriptionDB.notif_lofreq_mult;
        params.dump_buffer_size = m_QueueDescriptionDB.dump_buffer_size;
        params.run_timeout = m_QueueDescriptionDB.run_timeout;
        params.program_name = m_QueueDescriptionDB.program_name;
        params.failed_retries = m_QueueDescriptionDB.failed_retries;
        params.blacklist_time = m_QueueDescriptionDB.blacklist_time;
        params.max_input_size = m_QueueDescriptionDB.max_input_size;
        params.max_output_size = m_QueueDescriptionDB.max_output_size;
        params.subm_hosts = m_QueueDescriptionDB.subm_hosts;
        params.wnode_hosts = m_QueueDescriptionDB.wnode_hosts;
        params.wnode_timeout = m_QueueDescriptionDB.wnode_timeout;
        params.pending_timeout = m_QueueDescriptionDB.pending_timeout;
        params.max_pending_wait_timeout = m_QueueDescriptionDB.max_pending_wait_timeout;
        params.description = m_QueueDescriptionDB.description;
        params.run_timeout_precision = m_QueueDescriptionDB.run_timeout_precision;

        // It is impossible to have the same entries twice in the DB
        queues[queue_name] = params;
    }
    return queues;
}


void
CQueueDataBase::x_DeleteDBRecordsWithPrefix(const string &  prefix)
{
    // Transaction must be created outside

    vector<string>      to_delete;
    CBDB_FileCursor     cur(m_QueueDescriptionDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        string      qprefix;
        string      queue_name;

        NStr::SplitInTwo(m_QueueDescriptionDB.queue.GetString(),
                         "_", qprefix, queue_name);
        if (NStr::CompareNocase(qprefix, prefix) == 0)
            to_delete.push_back(m_QueueDescriptionDB.queue);
    }

    for (vector<string>::const_iterator  k = to_delete.begin();
         k != to_delete.end(); ++k) {
        m_QueueDescriptionDB.queue = *k;
        m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    }

    return;
}


void
CQueueDataBase::x_InsertParamRecord(const string &            key,
                                    const SQueueParameters &  params)
{
    // Transaction must be created outside

    m_QueueDescriptionDB.queue = key;

    m_QueueDescriptionDB.kind = params.kind;
    m_QueueDescriptionDB.pos = params.position;
    m_QueueDescriptionDB.delete_request = params.delete_request;
    m_QueueDescriptionDB.qclass = params.qclass;
    m_QueueDescriptionDB.timeout = params.timeout;
    m_QueueDescriptionDB.notif_hifreq_interval = params.notif_hifreq_interval;
    m_QueueDescriptionDB.notif_hifreq_period = params.notif_hifreq_period;
    m_QueueDescriptionDB.notif_lofreq_mult = params.notif_lofreq_mult;
    m_QueueDescriptionDB.dump_buffer_size = params.dump_buffer_size;
    m_QueueDescriptionDB.run_timeout = params.run_timeout;
    m_QueueDescriptionDB.program_name = params.program_name;
    m_QueueDescriptionDB.failed_retries = params.failed_retries;
    m_QueueDescriptionDB.blacklist_time = params.blacklist_time;
    m_QueueDescriptionDB.max_input_size = params.max_input_size;
    m_QueueDescriptionDB.max_output_size = params.max_output_size;
    m_QueueDescriptionDB.subm_hosts = params.subm_hosts;
    m_QueueDescriptionDB.wnode_hosts = params.wnode_hosts;
    m_QueueDescriptionDB.wnode_timeout = params.wnode_timeout;
    m_QueueDescriptionDB.pending_timeout = params.pending_timeout;
    m_QueueDescriptionDB.max_pending_wait_timeout = params.max_pending_wait_timeout;
    m_QueueDescriptionDB.description = params.description;
    m_QueueDescriptionDB.run_timeout_precision = params.run_timeout_precision;

    m_QueueDescriptionDB.UpdateInsert();
    return;
}


void
CQueueDataBase::x_WriteDBQueueDescriptions(const TQueueParams &   queue_classes)
{
    CBDB_Transaction        trans(*m_Env, CBDB_Transaction::eEnvDefault,
                                          CBDB_Transaction::eNoAssociation);

    m_QueueDescriptionDB.SetTransaction(&trans);

    // First, delete all the records with the appropriate prefix
    x_DeleteDBRecordsWithPrefix("qclass");

    // Second, write down the new records
    for (TQueueParams::const_iterator  k = queue_classes.begin();
         k != queue_classes.end(); ++k )
        x_InsertParamRecord("qclass_" + k->first, k->second);

    trans.Commit();
    return;
}


void
CQueueDataBase::x_WriteDBQueueDescriptions(const TQueueInfo &  queues)
{
    CBDB_Transaction        trans(*m_Env, CBDB_Transaction::eEnvDefault,
                                          CBDB_Transaction::eNoAssociation);

    m_QueueDescriptionDB.SetTransaction(&trans);

    // First, delete all the records with the appropriate prefix
    x_DeleteDBRecordsWithPrefix("queue");

    // Second, write down the new records
    for (TQueueInfo::const_iterator  k = queues.begin();
         k != queues.end(); ++k )
        x_InsertParamRecord("queue_" + k->first, k->second.first);

    trans.Commit();
    return;
}



TQueueParams
CQueueDataBase::x_ReadIniFileQueueClassDescriptions(const IRegistry &  reg)
{
    TQueueParams        queue_classes;
    list<string>        sections;

    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string              queue_class;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_class);
        if (NStr::CompareNocase(prefix, "qclass") != 0 ||
            queue_class.empty())
            continue;

        SQueueParameters    params;
        params.Read(reg, section_name);

        // The same sections cannot appear twice
        queue_classes[queue_class] = params;
    }

    return queue_classes;
}


// Reads the queues from ini file and respects inheriting queue classes
// parameters
TQueueParams
CQueueDataBase::x_ReadIniFileQueueDescriptions(const IRegistry &     reg,
                                               const TQueueParams &  classes)
{
    TQueueParams        queues;
    list<string>        sections;

    reg.EnumerateSections(&sections);
    ITERATE(list<string>, it, sections) {
        string              queue_name;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, "queue") != 0 ||
            queue_name.empty())
            continue;

        string  qclass = reg.GetString(section_name, "class", kEmptyStr);
        if (qclass.empty()) {
            // This queue does not have a class so read it with defaults
            SQueueParameters    params;
            params.Read(reg, section_name);

            queues[queue_name] = params;
            continue;
        }

        // This queue derives from a class
        TQueueParams::const_iterator  found = classes.find(qclass);
        if (found == classes.end())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Configuration error. The queue '" +
                       queue_name +
                       "' refers to undefined queue class '" +
                       qclass + "'.");

        // Take the class parameters
        SQueueParameters    params = found->second;

        // Override the class with what found in the section
        list<string>        values;
        reg.EnumerateEntries(section_name, &values);

        for (list<string>::const_iterator  val = values.begin();
             val != values.end(); ++val) {
            if (*val == "timeout")
                params.timeout =
                        params.ReadTimeout(reg, section_name);
            else if (*val == "notif_hifreq_interval")
                params.notif_hifreq_interval =
                        params.ReadNotifHifreqInterval(reg, section_name);
            else if (*val == "notif_hifreq_period")
                params.notif_hifreq_period =
                        params.ReadNotifHifreqPeriod(reg, section_name);
            else if (*val == "notif_lofreq_mult")
                params.notif_lofreq_mult =
                        params.ReadNotifLofreqMult(reg, section_name);
            else if (*val == "dump_buffer_size")
                params.dump_buffer_size =
                        params.ReadDumpBufferSize(reg, section_name);
            else if (*val == "run_timeout")
                params.run_timeout =
                        params.ReadRunTimeout(reg, section_name);
            else if (*val == "program")
                params.program_name = params.ReadProgram(reg, section_name);
            else if (*val == "failed_retries")
                params.failed_retries =
                        params.ReadFailedRetries(reg, section_name);
            else if (*val == "blacklist_time")
                params.blacklist_time =
                        params.ReadBlacklistTime(reg, section_name);
            else if (*val == "max_input_size")
                params.max_input_size =
                        params.ReadMaxInputSize(reg, section_name);
            else if (*val == "max_output_size")
                params.max_output_size =
                        params.ReadMaxOutputSize(reg, section_name);
            else if (*val == "subm_host")
                params.subm_hosts =
                        params.ReadSubmHosts(reg, section_name);
            else if (*val == "wnode_host")
                params.wnode_hosts =
                        params.ReadWnodeHosts(reg, section_name);
            else if (*val == "wnode_timeout")
                params.wnode_timeout =
                        params.ReadWnodeTimeout(reg, section_name);
            else if (*val == "pending_timeout")
                params.pending_timeout =
                        params.ReadPendingTimeout(reg, section_name);
            else if (*val == "max_pending_wait_timeout")
                params.max_pending_wait_timeout = 
                        params.ReadMaxPendingWaitTimeout(reg, section_name);
            else if (*val == "description")
                params.description =
                        params.ReadDescription(reg, section_name);
            else if (*val == "run_timeout_precision")
                params.run_timeout_precision =
                        params.ReadRunTimeoutPrecision(reg, section_name);
        }
        params.qclass = qclass;
        queues[queue_name] = params;
    }

    return queues;
}


// Validates the config from an ini file for the following:
// - a queue references a class which is not defined
// - a static queue redefines existed dynamic queue
void
CQueueDataBase::x_ValidateConfiguration(
                        const TQueueParams &  queues_from_ini,
                        const TQueueParams &  classes_from_ini) const
{
    // Check that all queues refer to defined classes
    for (TQueueParams::const_iterator  k = queues_from_ini.begin();
         k != queues_from_ini.end(); ++k) {
        if (k->second.qclass.empty())
            continue;
        if (classes_from_ini.find(k->second.qclass) == classes_from_ini.end())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Configuration error. The queue '" +
                       k->first +
                       "' refers to undefined queue class '" +
                       k->second.qclass + "'.");
    }

    // Check that static queues do not mess with existing dynamic queues
    for (TQueueParams::const_iterator  k = queues_from_ini.begin();
         k != queues_from_ini.end(); ++k) {
        TQueueInfo::const_iterator  existing = m_Queues.find(k->first);

        if (existing == m_Queues.end())
            continue;
        if (existing->second.first.kind == CQueue::eKindDynamic)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Configuration error. The queue '" + k->first +
                       "' clashes with a currently existing "
                       "dynamic queue of the same name.");
    }

    // Config file is OK for the current configuration
    return;
}


unsigned int
CQueueDataBase::x_CountQueuesToAdd(const TQueueParams &  queues_from_ini) const
{
    unsigned int        add_count = 0;

    for (TQueueParams::const_iterator  k = queues_from_ini.begin();
         k != queues_from_ini.end(); ++k) {

        if (m_Queues.find(k->first) == m_Queues.end())
            ++add_count;
    }

    return add_count;
}


// Updates what is stored in memory.
// Forms the diff string. Tells if there were changes.
bool
CQueueDataBase::x_ConfigureQueueClasses(const TQueueParams &  classes_from_ini,
                                        string &              diff)
{
    bool            has_changes = false;
    vector<string>  classes;    // Used to store added and deleted classes

    // Delete from existed what was not found in the new classes
    for (TQueueParams::iterator    k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k) {
        string      old_queue_class = k->first;

        if (classes_from_ini.find(old_queue_class) != classes_from_ini.end())
            continue;

        // The queue class is not in the configuration any more however it
        // still could be in use for a dynamic queue. Leave it for the GC
        // to check if the class has no reference to it and delete it
        // accordingly.
        // So, just mark it as for removal.

        if (k->second.delete_request)
            continue;   // Has already been marked for deletion

        k->second.delete_request = true;
        classes.push_back(old_queue_class);
    }

    if (!classes.empty()) {
        has_changes = true;
        if (!diff.empty())
            diff += ", ";
        diff += "\"deleted_queue_classes\" [";
        for (vector<string>::const_iterator  k = classes.begin();
             k != classes.end(); ++k) {
                if (k != classes.begin())
                    diff += ", ";
                diff += "\"" + *k + "\"";
        }
        diff += "]";
    }


    // Check the updates in the classes
    classes.clear();

    bool        section_started = false;
    for (TQueueParams::iterator    k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k) {

        string                        queue_class = k->first;
        TQueueParams::const_iterator  new_class =
                                classes_from_ini.find(queue_class);

        if (new_class == classes_from_ini.end())
            continue;   // It is a candidate for deletion, so no diff

        // The same class found in the new configuration
        if (k->second.delete_request) {
            // The class was restored before GC deleted it. Update the flag
            // and parameters
            k->second = new_class->second;
            classes.push_back(queue_class);
            continue;
        }

        // That's the same class which possibly was updated
        // Do not compare class name here, this is a class itself
        // Description should be compared
        string      class_diff = k->second.Diff(new_class->second,
                                                false, true);

        if (!class_diff.empty()) {
            // There is a difference, update the class info
            k->second = new_class->second;

            if (section_started == false) {
                section_started = true;
                if (!diff.empty())
                    diff += ", ";
                diff += "\"queue_class_changes\" {";
            } else {
                diff += ", ";
            }

            diff += "\"" + queue_class + "\" {" + class_diff + "}";
            has_changes = true;
        }
    }

    if (section_started)
        diff += "}";

    // Check what was added
    for (TQueueParams::const_iterator  k = classes_from_ini.begin();
         k != classes_from_ini.end(); ++k) {
        string      new_queue_class = k->first;

        if (m_QueueClasses.find(new_queue_class) == m_QueueClasses.end()) {
            m_QueueClasses[new_queue_class] = k->second;
            classes.push_back(new_queue_class);
        }
    }

    if (!classes.empty()) {
        has_changes = true;
        if (!diff.empty())
            diff += ", ";
        diff += "\"added_queue_classes\" [";
        for (vector<string>::const_iterator  k = classes.begin();
             k != classes.end(); ++k) {
                if (k != classes.begin())
                    diff += ", ";
                diff += "\"" + *k + "\"";
        }
        diff += "]";
    }

    return has_changes;
}


// Updates the queue info in memory and creates/marks for deletion
// queues if necessary.
bool
CQueueDataBase::x_ConfigureQueues(const TQueueParams &  queues_from_ini,
                                  string &              diff)
{
    bool            has_changes = false;
    vector<string>  deleted_queues;

    // Mark for deletion what disappeared
    for (TQueueInfo::iterator    k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        if (k->second.first.kind == CQueue::eKindDynamic)
            continue;   // It's not the config business to deal
                        // with dynamic queues

        string      old_queue = k->first;
        if (queues_from_ini.find(old_queue) != queues_from_ini.end())
            continue;

        // The queue is not in the configuration any more. It could
        // still be in use or jobs could be still there. So mark it
        // for deletion and forbid submits for the queue.
        // GC will later delete it.

        if (k->second.first.delete_request)
            continue;   // Has already been marked for deletion

        CRef<CQueue>    queue = k->second.second;
        queue->SetRefuseSubmits(true);

        k->second.first.delete_request = true;
        deleted_queues.push_back(k->first);
    }

    if (!deleted_queues.empty()) {
        has_changes = true;
        if (!diff.empty())
            diff += ", ";
        diff += "\"deleted_queues\" [";
        for (vector<string>::const_iterator  k = deleted_queues.begin();
             k != deleted_queues.end(); ++k) {
                if (k != deleted_queues.begin())
                    diff += ", ";
                diff += "\"" + *k + "\"";
        }
        diff += "]";
    }

    // Check the updates in the queue parameters
    vector< pair<string, string> >      added_queues;
    bool                                section_started = false;

    for (TQueueInfo::iterator    k = m_Queues.begin();
         k != m_Queues.end(); ++k) {

        if (k->second.first.kind == CQueue::eKindDynamic)
            continue;   // It's not the config business to deal
                        // with dynamic queues

        string                        queue_name = k->first;
        TQueueParams::const_iterator  new_queue =
                                        queues_from_ini.find(queue_name);

        if (new_queue == queues_from_ini.end())
            continue;   // It is a candidate for deletion, or a dynamic queue;
                        // So no diff

        // The same queue is in the new configuration
        if (k->second.first.delete_request) {
            // The queue was restored before GC deleted it. Update the flag,
            // parameters and allows submits and update parameters if so.
            CRef<CQueue>    queue = k->second.second;
            queue->SetParameters(new_queue->second);
            queue->SetRefuseSubmits(false);

            // The queue position must be preserved.
            // The queue kind could not be changed here.
            // The delete request is just checked.
            int     pos = k->second.first.position;

            k->second.first = new_queue->second;
            k->second.first.position = pos;
            added_queues.push_back(make_pair(queue_name, k->second.first.qclass));
            continue;
        }


        // That's the same queue which possibly was updated
        // Class name should also be compared here
        // Description should be compared here
        string      queue_diff = k->second.first.Diff(new_queue->second,
                                                      true, true);

        if (!queue_diff.empty()) {
            // There is a difference, update the queue info and the queue
            CRef<CQueue>    queue = k->second.second;
            queue->SetParameters(new_queue->second);

            // The queue position must be preserved.
            // The queue kind could not be changed here.
            // The queue delete request could not be changed here.
            int     pos = k->second.first.position;

            k->second.first = new_queue->second;
            k->second.first.position = pos;

            if (section_started == false) {
                section_started = true;
                if (!diff.empty())
                    diff += ", ";
                diff += "\"queue_changes\" {";
            } else {
                diff += ", ";
            }

            diff += "\"" + queue_name + "\" {" + queue_diff + "}";
            has_changes = true;
        }
    }

    // Check dynamic queues classes. They might be updated.
    for (TQueueInfo::iterator    k = m_Queues.begin();
         k != m_Queues.end(); ++k) {

        if (k->second.first.kind != CQueue::eKindDynamic)
            continue;
        if (k->second.first.delete_request == true)
            continue;

        // OK, this is dynamic queue, alive and not set for deletion
        // Check if its class parameters have been  updated/
        TQueueParams::const_iterator    queue_class =
                            m_QueueClasses.find(k->second.first.qclass);
        if (queue_class == m_QueueClasses.end()) {
            ERR_POST("Cannot find class '" + k->second.first.qclass +
                     "' for dynamic queue '" + k->first +
                     "'. Unexpected internal data inconsistency.");
            continue;
        }

        // Do not compare classes
        // Do not compare description
        // They do not make sense for dynamic queues because they are created
        // with their own descriptions and the class does not have the 'class'
        // field
        string  class_diff = k->second.first.Diff(queue_class->second,
                                                  false, false);
        if (!class_diff.empty()) {
            // There is a difference in the queue class - update the
            // parameters.
            string      old_class = k->second.first.qclass;
            int         old_pos = k->second.first.position;
            string      old_description = k->second.first.description;

            CRef<CQueue>    queue = k->second.second;
            queue->SetParameters(queue_class->second);

            k->second.first = queue_class->second;
            k->second.first.qclass = old_class;
            k->second.first.position = old_pos;
            k->second.first.description = old_description;

            if (section_started == false) {
                section_started = true;
                if (!diff.empty())
                    diff += ", ";
                diff += "\"queue_changes\" {";
            } else {
                diff += ", ";
            }

            diff += "\"" + k->first + "\" {" + class_diff + "}";
            has_changes = true;
        }
    }

    if (section_started)
        diff += "}";

    // Check what was added
    for (TQueueParams::const_iterator  k = queues_from_ini.begin();
         k != queues_from_ini.end(); ++k) {
        string      new_queue_name = k->first;

        if (m_Queues.find(new_queue_name) == m_Queues.end()) {
            // No need to check the allocation success here. It was checked
            // before that the server has enough resources for the new
            // configuration.
            int     new_position = m_QueueDbBlockArray.Allocate();

            x_CreateAndMountQueue(new_queue_name, k->second,
                                  m_QueueDbBlockArray.Get(new_position));

            m_Queues[new_queue_name].first.position = new_position;

            added_queues.push_back(make_pair(new_queue_name, k->second.qclass));
        }
    }

    if (!added_queues.empty()) {
        has_changes = true;
        if (!diff.empty())
            diff += ", ";
        diff += "\"added_queues\" {";
        for (vector< pair<string, string> >::const_iterator  k = added_queues.begin();
             k != added_queues.end(); ++k) {
                if (k != added_queues.begin())
                    diff += ", ";
                diff += "\"" + k->first + "\" \"" + k->second + "\"";
        }
        diff += "}";
    }

    return has_changes;
}


unsigned int  CQueueDataBase::Configure(const IRegistry &  reg,
                                        string &           diff)
{
    CFastMutexGuard     guard(m_ConfigureLock);

    // Read the configured queues and classes from the ini file
    TQueueParams        classes_from_ini =
                            x_ReadIniFileQueueClassDescriptions(reg);
    TQueueParams        queues_from_ini =
                            x_ReadIniFileQueueDescriptions(reg,
                                                           classes_from_ini);

    // Validate basic consistency of the incoming configuration
    x_ValidateConfiguration(queues_from_ini, classes_from_ini);

    // Check that the there are enough slots for the new queues if so
    // configured
    unsigned int        to_add_count = x_CountQueuesToAdd(queues_from_ini);
    unsigned int        available_count = m_QueueDbBlockArray.CountAvailable();

    if (to_add_count > available_count)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "New configuration slots requirement: " +
                   NStr::NumericToString(to_add_count) +
                   ". Number of available slots: " +
                   NStr::NumericToString(available_count) + ".");

    // Here: validation is finished. There is enough resources for the new
    // configuration.
    bool    has_changes = false;

    has_changes = x_ConfigureQueueClasses(classes_from_ini, diff);
    if (has_changes)
        x_WriteDBQueueDescriptions(m_QueueClasses);


    has_changes = x_ConfigureQueues(queues_from_ini, diff);
    if (has_changes)
        x_WriteDBQueueDescriptions(m_Queues);


    // Calculate the new min_run_timeout: required at the time of loading
    // NetSchedule and not used while reconfiguring on the fly
    unsigned int        min_run_timeout_precision = 3600;
    for (TQueueParams::const_iterator  k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k)
        min_run_timeout_precision = std::min(min_run_timeout_precision,
                                             k->second.run_timeout_precision);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        min_run_timeout_precision = std::min(min_run_timeout_precision,
                                             k->second.first.run_timeout_precision);
    return min_run_timeout_precision;
}


unsigned int  CQueueDataBase::CountActiveJobs(void) const
{
    unsigned int        cnt = 0;
    CFastMutexGuard     guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        cnt += k->second.second->CountActiveJobs();

    return cnt;
}


unsigned int  CQueueDataBase::CountAllJobs(void) const
{
    unsigned int        cnt = 0;
    CFastMutexGuard     guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        cnt += k->second.second->CountAllJobs();

    return cnt;
}


bool  CQueueDataBase::AnyJobs(void) const
{
    CFastMutexGuard     guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        if (k->second.second->AnyJobs())
            return true;

    return false;
}


CRef<CQueue> CQueueDataBase::OpenQueue(const string &  name)
{
    CFastMutexGuard             guard(m_ConfigureLock);
    TQueueInfo::const_iterator  found = m_Queues.find(name);

    if (found == m_Queues.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Queue '" + name + "' is not found.");

    return found->second.second;
}


void
CQueueDataBase::x_CreateAndMountQueue(const string &            qname,
                                      const SQueueParameters &  params,
                                      SQueueDbBlock *           queue_db_block)
{
    auto_ptr<CQueue>    q(new CQueue(m_Executor, qname,
                                     params.kind, m_Server));

    q->Attach(queue_db_block);
    q->SetParameters(params);

    unsigned int        recs = q->LoadStatusMatrix();
    m_Queues[qname] = make_pair(params, q.release());

    if (params.qclass.empty())
        LOG_POST(Message << Warning << "Queue '" << qname
                                    << "' (without any class) mounted. "
                                       "Number of records: " << recs);

    else
        LOG_POST(Message << Warning << "Queue '" << qname << "' of class '"
                                    << params.qclass
                                    << "' mounted. Number of records: "
                                    << recs);
    return;
}


bool CQueueDataBase::QueueExists(const string &  qname) const
{
    CFastMutexGuard     guard(m_ConfigureLock);
    return m_Queues.find(qname) != m_Queues.end();
}


void CQueueDataBase::CreateDynamicQueue(const string &  qname,
                                        const string &  qclass,
                                        const string &  description)
{
    CFastMutexGuard     guard(m_ConfigureLock);

    // Queue name clashes
    if (m_Queues.find(qname) != m_Queues.end())
        NCBI_THROW(CNetScheduleException, eDuplicateName,
                   "Queue '" + qname + "' already exists.");

    // Queue class existance
    TQueueParams::const_iterator  queue_class = m_QueueClasses.find(qclass);
    if (queue_class == m_QueueClasses.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass,
                   "Queue class '" + qclass +
                   "' for queue '" + qname + "' is not found.");

    // And class is not marked for deletion
    if (queue_class->second.delete_request)
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass,
                   "Queue class '" + qclass +
                   "' for queue '" + qname + "' is marked for deletion.");

    // Slot availability
    if (m_QueueDbBlockArray.CountAvailable() <= 0)
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Cannot allocate queue '" + qname +
                   "'. max_queues limit reached.");


    // All the preconditions are met. Create the queue
    int     new_position = m_QueueDbBlockArray.Allocate();

    SQueueParameters    params = queue_class->second;

    params.kind = CQueue::eKindDynamic;
    params.position = new_position;
    params.delete_request = false;
    params.qclass = qclass;
    params.description = description;

    x_CreateAndMountQueue(qname, params, m_QueueDbBlockArray.Get(new_position));

    x_WriteDBQueueDescriptions(m_Queues);
    return;
}


void  CQueueDataBase::DeleteDynamicQueue(const string &  qname)
{
    CFastMutexGuard         guard(m_ConfigureLock);
    TQueueInfo::iterator    found_queue = m_Queues.find(qname);

    if (found_queue == m_Queues.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Queue '" + qname + "' is not found." );

    if (found_queue->second.first.kind != CQueue::eKindDynamic)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Queue '" + qname + "' is static and cannot be deleted.");

    found_queue->second.first.delete_request = true;
    x_WriteDBQueueDescriptions(m_Queues);


    CRef<CQueue>    queue = found_queue->second.second;
    queue->SetRefuseSubmits(true);
    return;
}


SQueueParameters CQueueDataBase::QueueInfo(const string &  qname) const
{
    CFastMutexGuard             guard(m_ConfigureLock);
    TQueueInfo::const_iterator  found_queue = m_Queues.find(qname);

    if (found_queue == m_Queues.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Queue '" + qname + "' is not found." );

    SQueueParameters    params = found_queue->second.first;

    // refuse_submits field is used as a transport.
    // Usually used by QINF2
    params.refuse_submits = found_queue->second.second->GetRefuseSubmits();
    return params;
}


string CQueueDataBase::GetQueueNames(const string &  sep) const
{
    string                      names;
    CFastMutexGuard             guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        names += k->first + sep;

    return names;
}


void CQueueDataBase::Close(bool  drained_shutdown)
{
    // Check that we're still open
    if (!m_Env)
        return;

    StopNotifThread();
    StopPurgeThread();
    StopServiceThread();
    StopExecutionWatcherThread();

    if (drained_shutdown) {
        LOG_POST(Message << Warning <<
                 "Drained shutdown: DB is not closed gracefully.");
        m_QueueDescriptionDB.Close();
        x_SetSignallingFile(true);  // Create/update signalling file
    } else {
        m_Env->ForceTransactionCheckpoint();
        m_Env->CleanLog();

        m_QueueClasses.clear();
        m_Queues.clear();

        // Close pre-allocated databases
        m_QueueDbBlockArray.Close();

        m_QueueDescriptionDB.Close();
        try {
            if (m_Env->CheckRemove())
                LOG_POST(Message << Warning << "JS: '" << m_Name
                                 << "' Unmounted. BDB ENV deleted.");
            else
                LOG_POST(Message << Warning << "JS: '" << m_Name
                                 << "' environment still in use.");
        }
        catch (exception &  ex) {
            ERR_POST("JS: '" << m_Name << "' Exception in Close() " <<
                     ex.what() << " (ignored.)");
        }
    }

    delete m_Env;
    m_Env = 0;
    return;
}


void CQueueDataBase::TransactionCheckPoint(bool clean_log)
{
    m_Env->TransactionCheckpoint();
    if (clean_log)
        m_Env->CleanLog();
}


void CQueueDataBase::PrintTransitionCounters(CNetScheduleHandler &  handler)
{
    CFastMutexGuard             guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        handler.WriteMessage("OK:[queue " + k->first + "]");
        k->second.second->PrintTransitionCounters(handler);
    }
    return;
}


void CQueueDataBase::PrintJobsStat(CNetScheduleHandler &  handler)
{
    CFastMutexGuard             guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        handler.WriteMessage("OK:[queue " + k->first + "]");
        // Group and affinity tokens make no sense for the server,
        // so they are both "".
        k->second.second->PrintJobsStat(handler, "", "");
    }
    return;
}


string CQueueDataBase::GetQueueClassesInfo(void)
{
    string              output;
    CFastMutexGuard     guard(m_ConfigureLock);
    for (TQueueParams::const_iterator  k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += "OK:[qclass " + k->first + "]\n";
        // false - not to include qclass
        output += k->second.GetPrintableParameters(false);
    }
    return output;
}


string CQueueDataBase::GetQueueInfo(void)
{
    string              output;
    CFastMutexGuard     guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += "OK:[queue " + k->first + "]\n";
        // true - include qclass
        output += k->second.first.GetPrintableParameters(true);
    }
    return output;
}


void CQueueDataBase::NotifyListeners(void)
{
    time_t      current_time = time(0);
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->NotifyListenersPeriodically(current_time);
    }
    return;
}


void CQueueDataBase::PrintStatistics(size_t &  aff_count)
{
    CFastMutexGuard             guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        k->second.second->PrintStatistics(aff_count);

    return;
}


void CQueueDataBase::CheckExecutionTimeout(bool  logging)
{
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->CheckExecutionTimeout(logging);
    }
    return;
}


// Checks if the queues marked for deletion could be really deleted
// and deletes them if so. Deletes queue classes which are marked
// for deletion if there are no links to them.
void  CQueueDataBase::x_DeleteQueuesAndClasses(void)
{
    // It's better to avoid quering the queues under a lock so
    // let's first build a list of CRefs to the candidate queues.
    list< pair< string, CRef< CQueue > > >    candidates;

    {{
        CFastMutexGuard             guard(m_ConfigureLock);
        for (TQueueInfo::const_iterator  k = m_Queues.begin();
             k != m_Queues.end(); ++k)
            if (k->second.first.delete_request)
                candidates.push_back(make_pair(k->first, k->second.second));
    }}

    // Now the queues are queiried if they are empty without a lock
    list< pair< string, CRef< CQueue > > >::iterator
                                            k = candidates.begin();
    while (k != candidates.end()) {
        if (k->second->IsEmpty() == false)
            k = candidates.erase(k);
        else
            ++k;
    }

    if (candidates.empty())
        return;

    // Here we have a list of the queues which can be deleted
    // Take a lock and delete the queues plus check queue classes
    CFastMutexGuard             guard(m_ConfigureLock);
    for (k = candidates.begin(); k != candidates.end(); ++k) {
        // It's only here where a queue can be deleted so it's safe not
        // to check the iterator
        TQueueInfo::iterator    queue = m_Queues.find(k->first);

        // Deallocation of the DB block will be done later when the queue
        // is actually deleted
        queue->second.second->MarkForTruncating();
        m_Queues.erase(queue);
    }

    // Now, while still holding the lock, let's check queue classes
    vector< string >    classes_to_delete;
    for (TQueueParams::const_iterator  j = m_QueueClasses.begin();
         j != m_QueueClasses.end(); ++j) {
        if (j->second.delete_request) {
            bool    in_use = false;
            for (TQueueInfo::const_iterator m = m_Queues.begin();
                m != m_Queues.end(); ++m) {
                if (m->second.first.qclass == j->first) {
                    in_use = true;
                    break;
                }
            }
            if (in_use == false)
                classes_to_delete.push_back(j->first);
        }
    }

    for (vector< string >::const_iterator  k = classes_to_delete.begin();
         k != classes_to_delete.end(); ++k) {
        // It's only here where a queue class can be deleted so
        // it's safe not  to check the iterator
        m_QueueClasses.erase(m_QueueClasses.find(*k));
    }

    return;
}


/* Data used in CQueueDataBase::Purge() only */
static CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
    CNetScheduleAPI::eFailed,
    CNetScheduleAPI::eCanceled,
    CNetScheduleAPI::eDone,
    CNetScheduleAPI::ePending,
    CNetScheduleAPI::eReadFailed,
    CNetScheduleAPI::eConfirmed
};
const size_t kStatusesSize = sizeof(statuses_to_delete_from) /
                             sizeof(CNetScheduleAPI::EJobStatus);

void CQueueDataBase::Purge(void)
{
    size_t              max_mark_deleted = m_Server->GetMarkdelBatchSize();
    size_t              max_scanned = m_Server->GetScanBatchSize();
    size_t              total_scanned = 0;
    size_t              total_mark_deleted = 0;
    time_t              current_time = time(0);
    bool                limit_reached = false;

    // Cleanup the queues and classes if possible
    x_DeleteQueuesAndClasses();

    // Part I: from the last end till the end of the list
    CRef<CQueue>        start_queue = x_GetLastPurged();
    CRef<CQueue>        current_queue = start_queue;
    size_t              start_status_index = m_PurgeStatusIndex;
    unsigned int        start_job_id = m_PurgeJobScanned;

    while (current_queue.IsNull() == false) {
        m_PurgeQueue = current_queue->GetQueueName();
        if (x_PurgeQueue(current_queue.GetObject(),
                         m_PurgeStatusIndex, kStatusesSize - 1,
                         m_PurgeJobScanned, 0,
                         max_scanned, max_mark_deleted,
                         current_time,
                         total_scanned, total_mark_deleted) == true)
            return;

        if (total_mark_deleted >= max_mark_deleted ||
            total_scanned >= max_scanned) {
            limit_reached = true;
            break;
        }
        current_queue = x_GetNext(m_PurgeQueue);
    }


    // Part II: from the beginning of the list till the last end
    if (limit_reached == false) {
        current_queue = x_GetFirst();
        while (current_queue.IsNull() == false) {
            if (current_queue->GetQueueName() == start_queue->GetQueueName())
                break;

            m_PurgeQueue = current_queue->GetQueueName();
            if (x_PurgeQueue(current_queue.GetObject(),
                             m_PurgeStatusIndex, kStatusesSize - 1,
                             m_PurgeJobScanned, 0,
                             max_scanned, max_mark_deleted,
                             current_time,
                             total_scanned, total_mark_deleted) == true)
                return;

            if (total_mark_deleted >= max_mark_deleted ||
                total_scanned >= max_scanned) {
                limit_reached = true;
                break;
            }
            current_queue = x_GetNext(m_PurgeQueue);
        }
    }

    // Part III: it might need to check the statuses in the queue we started
    // with if the start status was not the first one.
    if (limit_reached == false) {
        if (start_queue.IsNull() == false) {
            m_PurgeQueue = start_queue->GetQueueName();
            if (start_status_index > 0) {
                if (x_PurgeQueue(start_queue.GetObject(),
                                 0, start_status_index - 1,
                                 m_PurgeJobScanned, 0,
                                 max_scanned, max_mark_deleted,
                                 current_time,
                                 total_scanned, total_mark_deleted) == true)
                    return;
            }
        }
    }

    if (limit_reached == false) {
        if (start_queue.IsNull() == false) {
            m_PurgeQueue = start_queue->GetQueueName();
            if (x_PurgeQueue(start_queue.GetObject(),
                             start_status_index, start_status_index,
                             0, start_job_id,
                             max_scanned, max_mark_deleted,
                             current_time,
                             total_scanned, total_mark_deleted) == true)
                return;
        }
    }


    // Part IV: purge the found candidates and optimize the memory if required
    m_FreeStatusMemCnt += x_PurgeUnconditional();
    TransactionCheckPoint();

    x_OptimizeStatusMatrix(current_time);

    return;
}


CRef<CQueue>  CQueueDataBase::x_GetLastPurged(void)
{
    CFastMutexGuard             guard(m_ConfigureLock);

    if (m_PurgeQueue.empty()) {
        if (m_Queues.empty())
            return CRef<CQueue>(NULL);
        return m_Queues.begin()->second.second;
    }

    for (TQueueInfo::iterator  it = m_Queues.begin();
         it != m_Queues.end(); ++it)
        if (it->first == m_PurgeQueue)
            return it->second.second;

    // Not found, which means the queue was deleted. Pick a random one
    m_PurgeStatusIndex = 0;
    m_PurgeJobScanned = 0;

    int     queue_num = ((rand() * 1.0) / RAND_MAX) * m_Queues.size();
    int     k = 1;
    for (TQueueInfo::iterator  it = m_Queues.begin();
         it != m_Queues.end(); ++it) {
        if (k >= queue_num)
            return it->second.second;
        ++k;
    }

    // Cannot happened, so just be consistent with the return value
    return m_Queues.begin()->second.second;
}


CRef<CQueue>  CQueueDataBase::x_GetFirst(void)
{
    CFastMutexGuard             guard(m_ConfigureLock);

    if (m_Queues.empty())
        return CRef<CQueue>(NULL);
    return m_Queues.begin()->second.second;
}


CRef<CQueue>  CQueueDataBase::x_GetNext(const string &  current_name)
{
    CFastMutexGuard             guard(m_ConfigureLock);

    if (m_Queues.empty())
        return CRef<CQueue>(NULL);

    for (TQueueInfo::iterator  it = m_Queues.begin();
         it != m_Queues.end(); ++it) {
        if (it->first == current_name) {
            ++it;
            if (it == m_Queues.end())
                return CRef<CQueue>(NULL);
            return it->second.second;
        }
    }

    // May not really happen. Let's have just in case.
    return CRef<CQueue>(NULL);
}


// Purges jobs from a queue starting from the given status.
// Returns true if the purge should be stopped.
// The status argument is a status to start from
bool  CQueueDataBase::x_PurgeQueue(CQueue &      queue,
                                   size_t        status,
                                   size_t        status_to_end,
                                   unsigned int  start_job_id,
                                   unsigned int  end_job_id,
                                   size_t        max_scanned,
                                   size_t        max_mark_deleted,
                                   time_t        current_time,
                                   size_t &      total_scanned,
                                   size_t &      total_mark_deleted)
{
    SPurgeAttributes    purge_io;

    for (; status <= status_to_end; ++status) {
        purge_io.scans = max_scanned - total_scanned;
        purge_io.deleted = max_mark_deleted - total_mark_deleted;
        purge_io.job_id = start_job_id;

        purge_io = queue.CheckJobsExpiry(current_time, purge_io,
                                         end_job_id,
                                         statuses_to_delete_from[status]);
        total_scanned += purge_io.scans;
        total_mark_deleted += purge_io.deleted;
        m_PurgeJobScanned = purge_io.job_id;

        if (x_CheckStopPurge())
            return true;

        if (total_mark_deleted >= max_mark_deleted ||
            total_scanned >= max_scanned) {
            m_PurgeStatusIndex = status;
            return false;
        }
    }
    m_PurgeStatusIndex = 0;
    m_PurgeJobScanned = 0;
    return false;
}


static const char *     drained_file_name =    "DB_DRAINED_Y";
static const char *     nondrained_file_name = "DB_DRAINED_N";

void  CQueueDataBase::x_SetSignallingFile(bool  drained)
{
    try {
        const char *    src = NULL;
        const char *    dst = NULL;

        if (drained) {
            src = nondrained_file_name;
            dst = drained_file_name;
        } else {
            src = drained_file_name;
            dst = nondrained_file_name;
        }

        CFile       src_file(CFile::MakePath(m_Path, src));
        if (src_file.Exists())
            src_file.Rename(CFile::MakePath(m_Path, dst));
        else {
            CFileIO     f;
            f.Open(CFile::MakePath(m_Path, dst),
                   CFileIO_Base::eCreate,
                   CFileIO_Base::eReadWrite);
            f.Close();
        }
    }
    catch (...) {
        ERR_POST("Error creating signalling file. "
                 "The server might not start correct with this DB.");
    }
    return;
}


bool  CQueueDataBase::x_IsDBDrained(void) const
{
    CFile       drained_file(CFile::MakePath(m_Path, drained_file_name));
    return drained_file.Exists();
}


unsigned int  CQueueDataBase::x_PurgeUnconditional(void) {
    // Purge unconditional jobs
    unsigned int        del_rec = 0;
    unsigned int        max_deleted = m_Server->GetDeleteBatchSize();


    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        del_rec += queue->DeleteBatch(max_deleted - del_rec);
        if (del_rec >= max_deleted)
            break;
    }
    return del_rec;
}


void CQueueDataBase::x_OptimizeStatusMatrix(time_t  current_time)
{
    // optimize memory every 15 min. or after 1 million of deleted records
    static const int        kMemFree_Delay = 15 * 60;
    static const unsigned   kRecordThreshold = 1000000;

    if ((m_FreeStatusMemCnt > kRecordThreshold) ||
        (m_LastFreeMem + kMemFree_Delay <= current_time)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = current_time;

        for (unsigned int  index = 0; ; ++index) {
            CRef<CQueue>  queue = x_GetQueueAt(index);
            if (queue.IsNull())
                break;
            queue->OptimizeMem();
            if (x_CheckStopPurge())
                break;
        }
    }
}


void CQueueDataBase::StopPurge(void)
{
    // No need to guard, this operation is thread safe
    m_StopPurge = true;
}


bool CQueueDataBase::x_CheckStopPurge(void)
{
    CFastMutexGuard     guard(m_PurgeLock);
    bool                stop_purge = m_StopPurge;

    m_StopPurge = false;
    return stop_purge;
}


void CQueueDataBase::RunPurgeThread(void)
{
    double              purge_timeout = m_Server->GetPurgeTimeout();
    unsigned int        sec_delay = purge_timeout;
    unsigned int        nanosec_delay = (purge_timeout - sec_delay)*1000000000;

    m_PurgeThread.Reset(new CJobQueueCleanerThread(
                                m_Host, *this, sec_delay, nanosec_delay,
                                m_Server->IsLogCleaningThread()));
    m_PurgeThread->Run();
}


void CQueueDataBase::StopPurgeThread(void)
{
    if (!m_PurgeThread.Empty()) {
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        m_PurgeThread.Reset(0);
    }
}


void CQueueDataBase::PurgeAffinities(void)
{
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->PurgeAffinities();
        if (x_CheckStopPurge())
            break;
    }
    return;
}


void CQueueDataBase::PurgeGroups(void)
{
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->PurgeGroups();
        if (x_CheckStopPurge())
            break;
    }
    return;
}


void CQueueDataBase::PurgeWNodes(void)
{
    // Worker nodes have the last access time in seconds since 1970
    // so there is no need to purge them more often than once a second
    static time_t       last_purge = 0;
    time_t              current_time = time(0);

    if (current_time == last_purge)
        return;
    last_purge = current_time;

    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->PurgeWNodes(current_time);
        if (x_CheckStopPurge())
            break;
    }
    return;
}


// Safely provides a queue at the given index
CRef<CQueue>  CQueueDataBase::x_GetQueueAt(unsigned int  index)
{
    unsigned int                current_index = 0;
    CFastMutexGuard             guard(m_ConfigureLock);

    for (TQueueInfo::iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        if (current_index == index)
            return k->second.second;
        ++current_index;
    }
    return CRef<CQueue>(NULL);
}


void CQueueDataBase::RunNotifThread(void)
{
    // 10 times per second
    m_NotifThread.Reset(new CGetJobNotificationThread(
                                *this, 0, 100000000,
                                m_Server->IsLogNotificationThread()));
    m_NotifThread->Run();
}


void CQueueDataBase::StopNotifThread(void)
{
    if (!m_NotifThread.Empty()) {
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        m_NotifThread.Reset(0);
    }
}


void CQueueDataBase::RunServiceThread(void)
{
    // Once in 100 seconds
    m_ServiceThread.Reset(new CServiceThread(
                                *m_Server, m_Host, *this,
                                m_Server->IsLogStatisticsThread()));
    m_ServiceThread->Run();
}


void CQueueDataBase::StopServiceThread(void)
{
    if (!m_ServiceThread.Empty()) {
        m_ServiceThread->RequestStop();
        m_ServiceThread->Join();
        m_ServiceThread.Reset(0);
    }
}


void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    m_ExeWatchThread.Reset(new CJobQueueExecutionWatcherThread(
                                    m_Host, *this, run_delay,
                                    m_Server->IsLogExecutionWatcherThread()));
    m_ExeWatchThread->Run();
}


void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        m_ExeWatchThread.Reset(0);
    }
}


END_NCBI_SCOPE

