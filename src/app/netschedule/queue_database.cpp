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
#include "ns_db_dump.hpp"
#include "ns_types.hpp"


BEGIN_NCBI_SCOPE


#define GetUIntNoErr(name, dflt) \
    (unsigned) bdb_conf.GetInt("netschedule", name, \
                               CConfig::eErr_NoThrow, dflt)
#define GetSizeNoErr(name, dflt) \
    (unsigned) bdb_conf.GetDataSize("netschedule", name, \
                                    CConfig::eErr_NoThrow, dflt)
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



/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation

CQueueDataBase::CQueueDataBase(CNetScheduleServer *            server,
                               const SNSDBEnvironmentParams &  params,
                               bool                            reinit)
: m_Host(server->GetBackgroundHost()),
  m_Executor(server->GetRequestExecutor()),
  m_Env(NULL),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_Server(server),
  m_PurgeQueue(""),
  m_PurgeStatusIndex(0),
  m_PurgeJobScanned(0)
{
    m_DataPath = CDirEntry::AddTrailingPathSeparator(params.db_path);
    m_DumpPath = CDirEntry::AddTrailingPathSeparator(m_DataPath +
                                                     kDumpSubdirName);

    // First, load the previous session start job IDs if file existed
    m_Server->LoadJobsStartIDs();

    // Old instance bdb files are not needed (even if they survived)
    x_RemoveBDBFiles();

    // Creates the queues from the ini file and loads jobs from the dump
    x_Open(params, reinit);
}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (...) {}
}


void  CQueueDataBase::x_Open(const SNSDBEnvironmentParams &  params,
                             bool                            reinit)
{
    // Checks preconditions and provides the final reinit value
    // It sets alerts and throws exceptions if needed.
    reinit = x_CheckOpenPreconditions(reinit);

    CDir    data_dir(m_DataPath);
    if (reinit)
        data_dir.Remove();
    if (!data_dir.Exists())
        data_dir.Create();

    // The initialization must be done before the queues are created but after
    // the directory is possibly re-created
    m_Server->InitNodeID(m_DataPath);

    m_Env = x_CreateBDBEnvironment(params);

    // Detect what queues need to be loaded. It depends on the configuration
    // file and on the dumped queues. It might be that the saved queues +
    // the config file queues excced the configured max number of queues.
    set<string, PNocase>    dump_static_queues;
    map<string, string,
        PNocase>            dump_dynamic_queues;    // qname -> qclass
    TQueueParams            dump_queue_classes;
    x_ReadDumpQueueDesrc(dump_static_queues, dump_dynamic_queues,
                         dump_queue_classes);
    set<string, PNocase>    config_static_queues = x_GetConfigQueues();
    string                  last_queue_load_error;
    size_t                  queue_load_error_count = 0;

    // Exclude number of queues will be the static queues from the config
    // plus the dumped dynamic queues
    size_t      final_dynamic_count = 0;
    for (map<string, string, PNocase>::const_iterator
            k = dump_dynamic_queues.begin();
            k != dump_dynamic_queues.end(); ++k)
        if (config_static_queues.find(k->first) == config_static_queues.end())
            ++final_dynamic_count;

    size_t      total_queues = final_dynamic_count +
                               config_static_queues.size();
    size_t      queues_limit = total_queues;
    if (total_queues > params.max_queues) {
        string  msg = "The initial number of queues on the server exceeds the "
                      "configured max number of queues. Configured: " +
                      NStr::NumericToString(params.max_queues) + ". Real: " +
                      NStr::NumericToString(total_queues) + ". The limit will "
                      "be extended to accomodate all the queues.";
        LOG_POST(Note << msg);
        m_Server->RegisterAlert(eMaxQueues, msg);
    } else {
        // The configuration file limit has not been exceeded
        queues_limit = params.max_queues;
    }

    // Allocate SQueueDbBlock's here, open/create corresponding databases
    m_QueueDbBlockArray.Init(*m_Env, m_DataPath, queues_limit);

    try {
        // Here: we can start restoring what was saved. The first step is
        //       to get the linked sections. Linked sections have two sources:
        //       - config file
        //       - dumped sections
        // The config file sections must override the dumped ones
        CJsonNode       unused_diff = CJsonNode::NewObjectNode();
        x_ReadLinkedSections(CNcbiApplication::Instance()->GetConfig(),
                             unused_diff);
        x_AppendDumpLinkedSections();

        // Read the queue classes from the config file and append those which
        // come from the dump
        m_QueueClasses = x_ReadIniFileQueueClassDescriptions(
                                    CNcbiApplication::Instance()->GetConfig());
        for (TQueueParams::const_iterator  k = dump_queue_classes.begin();
                k != dump_queue_classes.end(); ++k) {
            if (m_QueueClasses.find(k->first) == m_QueueClasses.end())
                m_QueueClasses[k->first] = k->second;
        }

        // Read the queues from the config file
        TQueueParams    queues_from_ini =
                            x_ReadIniFileQueueDescriptions(
                                    CNcbiApplication::Instance()->GetConfig(),
                                    m_QueueClasses);
        x_ConfigureQueues(queues_from_ini, unused_diff);

        // Add the queues from the dump
        for (map<string, string, PNocase>::const_iterator
                k = dump_dynamic_queues.begin();
                k != dump_dynamic_queues.end(); ++k) {
            string      qname = k->first;
            if (config_static_queues.find(qname) != config_static_queues.end())
                continue;

            // Here: the dumped queue has not been changed from a dynamic one
            //       to a static. So, it needs to be added.
            string      qclass = k->second;
            int         new_position = m_QueueDbBlockArray.Allocate();

            SQueueParameters    params = m_QueueClasses[qclass];

            params.kind = CQueue::eKindDynamic;
            params.position = new_position;
            params.delete_request = false;
            params.qclass = qclass;

            // Lost parameter: description. The only dynamic queue classes are
            // dumped so the description is lost.
            // params.description = ...

            x_CreateAndMountQueue(qname, params,
                                  m_QueueDbBlockArray.Get(new_position));
        }

        // All the structures are ready to upload the jobs from the dump
        for (TQueueInfo::iterator  k = m_Queues.begin();
                k != m_Queues.end(); ++k) {
            try {
                unsigned int   records =
                                    k->second.second->LoadFromDump(m_DumpPath);
                GetDiagContext().Extra()
                    .Print("_type", "startup")
                    .Print("_queue", k->first)
                    .Print("info", "load_from_dump")
                    .Print("records", records);
            } catch (const exception &  ex) {
                ERR_POST(ex.what());
                last_queue_load_error = ex.what();
                ++queue_load_error_count;
            } catch (...) {
                last_queue_load_error = "Unknown error loading queue " +
                                        k->first + " from dump";
                ERR_POST(last_queue_load_error);
                ++queue_load_error_count;
            }
        }
    } catch (const exception &  ex) {
        ERR_POST(ex.what());
        last_queue_load_error = ex.what();
        ++queue_load_error_count;
    } catch (...) {
        last_queue_load_error = "Unknown error loading queues from dump";
        ERR_POST(last_queue_load_error);
        ++queue_load_error_count;
    }

    x_CreateCrashFlagFile();
    x_CreateDumpErrorFlagFile();
    x_CreateStorageVersionFile();

    // Serialize the start job IDs file if it was deleted during the database
    // initialization. Even if it is overwriting an existing file there is no
    // performance issue at this point (it's done once anyway).
    m_Server->SerializeJobsStartIDs();

    if (queue_load_error_count > 0) {
        m_Server->RegisterAlert(eDumpLoadError,
                                "There were error(s) loading the previous "
                                "instance dump. Number of errors: " +
                                NStr::NumericToString(queue_load_error_count) +
                                ". See log for all the loading errors. "
                                "Last error: " + last_queue_load_error);
        x_BackupDump();
    } else {
        x_RemoveDump();
    }

    x_CreateSpaceReserveFile();
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
        if (NStr::CompareNocase(prefix, "qclass") != 0)
           continue;
        if (queue_class.empty())
            continue;
        if (queue_class.size() > kMaxQueueNameSize - 1)
            continue;

        // Warnings are ignored here. At this point they are not of interest
        // because they have already been collected at the startup (allowed)
        // or at RECO - a file with warnings is not allowed
        SQueueParameters    params;
        vector<string>      warnings;
        if (params.ReadQueueClass(reg, section_name, warnings)) {
            // false => problems with linked sections; see CXX-2617
            // The same sections cannot appear twice
            queue_classes[queue_class] = params;
        }
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
        if (NStr::CompareNocase(prefix, "queue") != 0)
           continue;
        if (queue_name.empty())
            continue;
        if (queue_name.size() > kMaxQueueNameSize - 1)
            continue;

        // Warnings are ignored here. At this point they are not of interest
        // because they have already been collected at the startup (allowed)
        // or at RECO - a file with warnings is not allowed
        SQueueParameters    params;
        vector<string>      warnings;

        if (params.ReadQueue(reg, section_name, classes, warnings)) {
            // false => problems with linked sections; see CXX-2617
            queues[queue_name] = params;
        }
    }

    return queues;
}


void  CQueueDataBase::x_ReadLinkedSections(const IRegistry &  reg,
                                           CJsonNode &        diff)
{
    // Read the new content
    typedef map< string, map< string, string > >    section_container;
    section_container   new_values;
    list<string>        sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string              queue_or_class;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_or_class);
        if (queue_or_class.empty())
            continue;
        if (NStr::CompareNocase(prefix, "qclass") != 0 &&
            NStr::CompareNocase(prefix, "queue") != 0)
            continue;
        if (queue_or_class.size() > kMaxQueueNameSize - 1)
            continue;

        list<string>    entries;
        reg.EnumerateEntries(section_name, &entries);

        ITERATE(list<string>, k, entries) {
            const string &  entry = *k;
            string          ref_section = reg.GetString(section_name,
                                                        entry, kEmptyStr);

            if (!NStr::StartsWith(entry, "linked_section_", NStr::eCase))
                continue;

            if (entry == "linked_section_")
                continue;   // Malformed values prefix

            if (ref_section.empty())
                continue;   // Malformed section name

            if (find(sections.begin(), sections.end(), ref_section) ==
                                                            sections.end())
                continue;   // Non-existing section

            if (new_values.find(ref_section) != new_values.end())
                continue;   // Has already been read

            // Read the linked section values
            list<string>    linked_section_entries;
            reg.EnumerateEntries(ref_section, &linked_section_entries);
            map<string, string> values;
            for (list<string>::const_iterator j = linked_section_entries.begin();
                 j != linked_section_entries.end(); ++j)
                values[*j] = reg.GetString(ref_section, *j, kEmptyStr);

            new_values[ref_section] = values;
        }
    }

    CFastMutexGuard     guard(m_LinkedSectionsGuard);

    // Identify those sections which were deleted
    vector<string>  deleted;
    for (section_container::const_iterator  k(m_LinkedSections.begin());
         k != m_LinkedSections.end(); ++k)
        if (new_values.find(k->first) == new_values.end())
            deleted.push_back(k->first);

    if (!deleted.empty()) {
        CJsonNode   deletedSections = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k(deleted.begin());
                k != deleted.end(); ++k)
            deletedSections.AppendString( *k );
        diff.SetByKey( "linked_section_deleted", deletedSections );
    }

    // Identify those sections which were added
    vector<string>  added;
    for (section_container::const_iterator  k(new_values.begin());
         k != new_values.end(); ++k)
        if (m_LinkedSections.find(k->first) == m_LinkedSections.end())
            added.push_back(k->first);

    if (!added.empty()) {
        CJsonNode   addedSections = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k(added.begin());
                k != added.end(); ++k)
            addedSections.AppendString( *k );
        diff.SetByKey( "linked_section_added", addedSections );
    }

    // Deal with changed sections: what was added/deleted/modified
    vector<string>  changed;
    for (section_container::const_iterator  k(new_values.begin());
        k != new_values.end(); ++k) {
        if (find(added.begin(), added.end(), k->first) != added.end())
            continue;
        if (new_values[k->first] == m_LinkedSections[k->first])
            continue;
        changed.push_back(k->first);
    }

    if (!changed.empty()) {
        CJsonNode       changedSections = CJsonNode::NewObjectNode();
        for (vector<string>::const_iterator  k(changed.begin());
                k != changed.end(); ++k)
            changedSections.SetByKey( *k,
                        x_DetectChangesInLinkedSection( m_LinkedSections[*k],
                                                        new_values[*k]) );
        diff.SetByKey( "linked_section_changed", changedSections );
    }

    // Finally, save the new configuration
    m_LinkedSections = new_values;
}


CJsonNode
CQueueDataBase::x_DetectChangesInLinkedSection(
                        const map<string, string> &  old_values,
                        const map<string, string> &  new_values)
{
    CJsonNode       diff = CJsonNode::NewObjectNode();

    // Deal with deleted items
    vector<string>  deleted;
    for (map<string, string>::const_iterator  k(old_values.begin());
         k != old_values.end(); ++k)
        if (new_values.find(k->first) == new_values.end())
            deleted.push_back(k->first);
    if (!deleted.empty()) {
        CJsonNode   deletedValues = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k(deleted.begin());
                k != deleted.end(); ++k)
            deletedValues.AppendString( *k );
        diff.SetByKey( "deleted", deletedValues );
    }

    // Deal with added items
    vector<string>  added;
    for (map<string, string>::const_iterator  k(new_values.begin());
         k != new_values.end(); ++k)
        if (old_values.find(k->first) == old_values.end())
            added.push_back(k->first);
    if (!added.empty()) {
        CJsonNode   addedValues = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k(added.begin());
                k != added.end(); ++k)
            addedValues.AppendString( *k );
        diff.SetByKey( "added", addedValues );
    }

    // Deal with changed values
    vector<string>  changed;
    for (map<string, string>::const_iterator  k(new_values.begin());
         k != new_values.end(); ++k) {
        if (old_values.find(k->first) == old_values.end())
            continue;
        if (old_values.find(k->first)->second ==
            new_values.find(k->first)->second)
            continue;
        changed.push_back(k->first);
    }
    if (!changed.empty()) {
        CJsonNode   changedValues = CJsonNode::NewObjectNode();
        for (vector<string>::const_iterator  k(changed.begin());
                k != changed.end(); ++k) {
            CJsonNode       values = CJsonNode::NewArrayNode();
            values.AppendString( old_values.find(*k)->second );
            values.AppendString( new_values.find(*k)->second );
            changedValues.SetByKey( *k, values );
        }
        diff.SetByKey( "changed", changedValues );
    }

    return diff;
}


// Validates the config from an ini file for the following:
// - a static queue redefines existed dynamic queue
void
CQueueDataBase::x_ValidateConfiguration(
                        const TQueueParams &  queues_from_ini) const
{
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
                                        CJsonNode &           diff)
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
        CJsonNode       deleted_classes = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = classes.begin();
                k != classes.end(); ++k)
            deleted_classes.AppendString( *k );
        diff.SetByKey( "deleted_queue_classes", deleted_classes );
    }


    // Check the updates in the classes
    classes.clear();

    CJsonNode           class_changes = CJsonNode::NewObjectNode();
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
        CJsonNode   class_diff = k->second.Diff(new_class->second,
                                                false, true);

        if (class_diff.GetSize() > 0) {
            // There is a difference, update the class info
            k->second = new_class->second;

            class_changes.SetByKey(queue_class, class_diff);
            has_changes = true;
        }
    }
    if (class_changes.GetSize() > 0)
        diff.SetByKey("queue_class_changes", class_changes);

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
        CJsonNode       added_classes = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = classes.begin();
                k != classes.end(); ++k)
            added_classes.AppendString(*k);
        diff.SetByKey("added_queue_classes", added_classes);
    }

    return has_changes;
}


// Updates the queue info in memory and creates/marks for deletion
// queues if necessary.
bool
CQueueDataBase::x_ConfigureQueues(const TQueueParams &  queues_from_ini,
                                  CJsonNode &           diff)
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
        CJsonNode       deleted = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = deleted_queues.begin();
                k != deleted_queues.end(); ++k)
            deleted.AppendString(*k);
        diff.SetByKey("deleted_queues", deleted);
    }


    // Check the updates in the queue parameters
    vector< pair<string,
                 string> >  added_queues;
    CJsonNode               section_changes = CJsonNode::NewObjectNode();

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
            added_queues.push_back(make_pair(queue_name,
                                             k->second.first.qclass));
            continue;
        }


        // That's the same queue which possibly was updated
        // Class name should also be compared here
        // Description should be compared here
        CJsonNode   queue_diff = k->second.first.Diff(new_queue->second,
                                                      true, true);

        if (queue_diff.GetSize() > 0) {
            // There is a difference, update the queue info and the queue
            CRef<CQueue>    queue = k->second.second;
            queue->SetParameters(new_queue->second);

            // The queue position must be preserved.
            // The queue kind could not be changed here.
            // The queue delete request could not be changed here.
            int     pos = k->second.first.position;

            k->second.first = new_queue->second;
            k->second.first.position = pos;

            section_changes.SetByKey(queue_name, queue_diff);
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
        CJsonNode   class_diff = k->second.first.Diff(queue_class->second,
                                                      false, false);
        if (class_diff.GetSize() > 0) {
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
            k->second.first.kind = CQueue::eKindDynamic;

            section_changes.SetByKey(k->first, class_diff);
            has_changes = true;
        }
    }

    if (section_changes.GetSize() > 0)
        diff.SetByKey("queue_changes", section_changes);


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
        CJsonNode       added = CJsonNode::NewObjectNode();
        for (vector< pair<string, string> >::const_iterator
                k = added_queues.begin();
                k != added_queues.end(); ++k)
            added.SetByKey(k->first, CJsonNode::NewStringNode(k->second));
        diff.SetByKey("added_queues", added);
    }

    return has_changes;
}


time_t  CQueueDataBase::Configure(const IRegistry &  reg,
                                  CJsonNode &        diff)
{
    CFastMutexGuard     guard(m_ConfigureLock);

    // Read the configured queues and classes from the ini file
    TQueueParams        classes_from_ini =
                            x_ReadIniFileQueueClassDescriptions(reg);
    TQueueParams        queues_from_ini =
                            x_ReadIniFileQueueDescriptions(reg,
                                                           classes_from_ini);

    // Validate basic consistency of the incoming configuration
    x_ValidateConfiguration(queues_from_ini);

    x_ReadLinkedSections(reg, diff);

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
    x_ConfigureQueueClasses(classes_from_ini, diff);
    x_ConfigureQueues(queues_from_ini, diff);
    return CalculateRuntimePrecision();
}


CNSPreciseTime CQueueDataBase::CalculateRuntimePrecision(void) const
{
    // Calculate the new min_run_timeout: required at the time of loading
    // NetSchedule and not used while reconfiguring on the fly
    CNSPreciseTime      min_precision = kTimeNever;
    for (TQueueParams::const_iterator  k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k)
        min_precision = std::min(min_precision,
                                 k->second.CalculateRuntimePrecision());
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        min_precision = std::min(min_precision,
                                 k->second.first.CalculateRuntimePrecision());
    return min_precision;
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
                                     params.kind, m_Server, *this));

    q->Attach(queue_db_block);
    q->SetParameters(params);

    m_Queues[qname] = make_pair(params, q.release());

    GetDiagContext().Extra()
        .Print("_type", "startup")
        .Print("_queue", qname)
        .Print("qclass", params.qclass)
        .Print("info", "mount");
}


bool CQueueDataBase::QueueExists(const string &  qname) const
{
    CFastMutexGuard     guard(m_ConfigureLock);
    return m_Queues.find(qname) != m_Queues.end();
}


void CQueueDataBase::CreateDynamicQueue(const CNSClientId &  client,
                                        const string &  qname,
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

    // submitter and program restrictions must be checked
    // for the class
    if (!client.IsAdmin()) {
        if (!queue_class->second.subm_hosts.empty()) {
            CNetScheduleAccessList  acl;
            acl.SetHosts(queue_class->second.subm_hosts);
            if (!acl.IsAllowed(client.GetAddress())) {
                m_Server->RegisterAlert(eAccess, "submitter privileges required"
                                        " to create a dynamic queue");
                NCBI_THROW(CNetScheduleException, eAccessDenied,
                           "Access denied: submitter privileges required");
            }
        }
        if (!queue_class->second.program_name.empty()) {
            CQueueClientInfoList    acl;
            bool                    ok = false;

            acl.AddClientInfo(queue_class->second.program_name);
            try {
                CQueueClientInfo    auth_prog_info;
                ParseVersionString(client.GetProgramName(),
                                   &auth_prog_info.client_name,
                                   &auth_prog_info.version_info);
                ok = acl.IsMatchingClient(auth_prog_info);
            } catch (...) {
                // Parsing errors
                ok = false;
            }

            if (!ok) {
                m_Server->RegisterAlert(eAccess, "program privileges required "
                                        "to create a dynamic queue");
                NCBI_THROW(CNetScheduleException, eAccessDenied,
                           "Access denied: program privileges required");
            }
        }
    }


    // All the preconditions are met. Create the queue
    int     new_position = m_QueueDbBlockArray.Allocate();

    SQueueParameters    params = queue_class->second;

    params.kind = CQueue::eKindDynamic;
    params.position = new_position;
    params.delete_request = false;
    params.qclass = qclass;
    params.description = description;

    x_CreateAndMountQueue(qname, params, m_QueueDbBlockArray.Get(new_position));
}


void  CQueueDataBase::DeleteDynamicQueue(const CNSClientId &  client,
                                         const string &  qname)
{
    CFastMutexGuard         guard(m_ConfigureLock);
    TQueueInfo::iterator    found_queue = m_Queues.find(qname);

    if (found_queue == m_Queues.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Queue '" + qname + "' is not found." );

    if (found_queue->second.first.kind != CQueue::eKindDynamic)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "Queue '" + qname + "' is static and cannot be deleted.");

    // submitter and program restrictions must be checked
    CRef<CQueue>    queue = found_queue->second.second;
    if (!client.IsAdmin()) {
        if (!queue->IsSubmitAllowed(client.GetAddress()))
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: submitter privileges required");
        if (!queue->IsProgramAllowed(client.GetProgramName()))
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: program privileges required");
    }

    found_queue->second.first.delete_request = true;
    queue->SetRefuseSubmits(true);
}


SQueueParameters CQueueDataBase::QueueInfo(const string &  qname) const
{
    CFastMutexGuard             guard(m_ConfigureLock);
    TQueueInfo::const_iterator  found_queue = m_Queues.find(qname);

    if (found_queue == m_Queues.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Queue '" + qname + "' is not found." );

    return x_SingleQueueInfo(found_queue);
}


/* Note: this member must be called under a lock, it's not thread safe */
SQueueParameters
CQueueDataBase::x_SingleQueueInfo(TQueueInfo::const_iterator  found) const
{
    SQueueParameters    params = found->second.first;

    // The fields below are used as a transport.
    // Usually used by QINF2 and STAT QUEUES
    params.refuse_submits = found->second.second->GetRefuseSubmits();
    params.pause_status = found->second.second->GetPauseStatus();
    params.max_aff_slots = m_Server->GetMaxAffinities();
    params.aff_slots_used = found->second.second->GetAffSlotsUsed();
    params.clients = found->second.second->GetClientsCount();
    params.groups = found->second.second->GetGroupsCount();
    params.gc_backlog = found->second.second->GetGCBacklogCount();
    params.notif_count = found->second.second->GetNotifCount();
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


void CQueueDataBase::Close(void)
{
    // Check that we're still open
    if (!m_Env)
        return;

    StopNotifThread();
    StopPurgeThread();
    StopServiceThread();
    StopExecutionWatcherThread();

    // Print the statistics counters last time
    if (m_Server->IsLogStatisticsThread()) {
        size_t      aff_count = 0;

        PrintStatistics(aff_count);
        CStatisticsCounters::PrintServerWide(aff_count);
    }

    if (m_Server->IsDrainShutdown() && m_Server->WasDBDrained()) {
        // That was a not interrupted drain shutdown so there is no
        // need to dump anything
        LOG_POST("Drained shutdown: the DB has been successfully drained");
        x_RemoveDumpErrorFlagFile();
    } else {
        // That was either:
        // - hard shutdown
        // - hard shutdown when drained shutdown is in process
        if (m_Server->IsDrainShutdown())
            ERR_POST(Warning <<
                     "Drained shutdown: DB draining has not been completed "
                     "when a hard shutdown is received. "
                     "Shutting down immediately.");

        // Dump all the queues/queue classes/queue parameters to flat files
        x_Dump();

        // A Dump is created, so we may avoid calling
        // - m_Env->ForceTransactionCheckpoint();
        // - m_Env->CleanLog();
        // which may take very long time to complete

        m_QueueClasses.clear();

        // CQueue objects destructors are called from here because the last
        // reference to the object has gone
        m_Queues.clear();

        // The call below may also cause a very long shutdown. At this point
        // we are not interested in proper BDB file closing because the jobs
        // have already been dumped. Thus we can ommit calling the DBD files
        // close and just remove them a few statements later.
        // The only caveat is that valgrind will complain on memory leaks. This
        // is however minor in comparison to the shutdown time consumption.
        // With the close() commented out it only depends on the current number
        // of jobs but not on the number of job since start.
        // Close pre-allocated databases
        // m_QueueDbBlockArray.Close();
    }

    delete m_Env;
    m_Env = 0;

    // BDB files are not needed anymore. They could be safely deleted.
    x_RemoveBDBFiles();
    x_RemoveCrashFlagFile();
}


void CQueueDataBase::TransactionCheckPoint(bool clean_log)
{
    m_Env->TransactionCheckpoint();
    if (clean_log)
        m_Env->CleanLog();
}


string CQueueDataBase::PrintTransitionCounters(void)
{
    string                      result;
    CFastMutexGuard             guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        result += "OK:[queue " + k->first + "]\n" +
                  k->second.second->PrintTransitionCounters();
    return result;
}


string CQueueDataBase::PrintJobsStat(void)
{
    string                      result;
    vector<string>              warnings;
    CFastMutexGuard             guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        // Group and affinity tokens make no sense for the server,
        // so they are both "".
        result += "OK:[queue " + k->first + "]\n" +
                  k->second.second->PrintJobsStat("", "", warnings);
    return result;
}


string CQueueDataBase::GetQueueClassesInfo(void) const
{
    string                  output;
    CFastMutexGuard         guard(m_ConfigureLock);

    for (TQueueParams::const_iterator  k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k) {
        if (!output.empty())
            output += "\n";

        // false - not to include qclass
        // false - not URL encoded format
        output += "OK:[qclass " + k->first + "]\n" +
                  k->second.GetPrintableParameters(false, false);

        for (map<string, string>::const_iterator
             j = k->second.linked_sections.begin();
             j != k->second.linked_sections.end(); ++j) {
            string  prefix((j->first).c_str() + strlen("linked_section_"));
            string  section_name = j->second;

            map<string, string> values = GetLinkedSection(section_name);
            for (map<string, string>::const_iterator m = values.begin();
                 m != values.end(); ++m)
                output += "\nOK:" + prefix + "." + m->first + ": " + m->second;
        }
    }
    return output;
}


string CQueueDataBase::GetQueueClassesConfig(void) const
{
    string              output;
    CFastMutexGuard     guard(m_ConfigureLock);
    for (TQueueParams::const_iterator  k = m_QueueClasses.begin();
         k != m_QueueClasses.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += "[qclass_" + k->first + "]\n" +
                  k->second.ConfigSection(true);
    }
    return output;
}


string CQueueDataBase::GetQueueInfo(void) const
{
    string                  output;
    CFastMutexGuard         guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        if (!output.empty())
            output += "\n";

        // true - include qclass
        // false - not URL encoded format
        output += "OK:[queue " + k->first + "]\n" +
                  x_SingleQueueInfo(k).GetPrintableParameters(true, false);

        for (map<string, string>::const_iterator
             j = k->second.first.linked_sections.begin();
             j != k->second.first.linked_sections.end(); ++j) {
            string  prefix((j->first).c_str() + strlen("linked_section_"));
            string  section_name = j->second;

            map<string, string> values = GetLinkedSection(section_name);
            for (map<string, string>::const_iterator m = values.begin();
                 m != values.end(); ++m)
                output += "\nOK:" + prefix + "." + m->first + ": " + m->second;
        }
    }
    return output;
}

string CQueueDataBase::GetQueueConfig(void) const
{
    string              output;
    CFastMutexGuard     guard(m_ConfigureLock);
    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += "[queue_" + k->first + "]\n" +
                  k->second.first.ConfigSection(false);
    }
    return output;
}


string CQueueDataBase::GetLinkedSectionConfig(void) const
{
    string              output;
    CFastMutexGuard     guard(m_LinkedSectionsGuard);

    for (map< string, map< string, string > >::const_iterator
            k = m_LinkedSections.begin();
            k != m_LinkedSections.end(); ++k) {
        if (!output.empty())
            output += "\n";
        output += "[" + k->first + "]\n";
        for (map< string, string >::const_iterator j = k->second.begin();
             j != k->second.end(); ++j) {
            output += j->first + "=\"" + j->second + "\"\n";
        }
    }
    return output;
}


map< string, string >
CQueueDataBase::GetLinkedSection(const string &  section_name) const
{
    CFastMutexGuard     guard(m_LinkedSectionsGuard);
    map< string, map< string, string > >::const_iterator  found =
        m_LinkedSections.find(section_name);
    if (found == m_LinkedSections.end())
        return map< string, string >();
    return found->second;
}


void CQueueDataBase::NotifyListeners(void)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->NotifyListenersPeriodically(current_time);
    }
}


void CQueueDataBase::PrintStatistics(size_t &  aff_count)
{
    CFastMutexGuard             guard(m_ConfigureLock);

    for (TQueueInfo::const_iterator  k = m_Queues.begin();
         k != m_Queues.end(); ++k)
        k->second.second->PrintStatistics(aff_count);
}


void CQueueDataBase::CheckExecutionTimeout(bool  logging)
{
    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->CheckExecutionTimeout(logging);
    }
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
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
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
bool  CQueueDataBase::x_PurgeQueue(CQueue &                queue,
                                   size_t                  status,
                                   size_t                  status_to_end,
                                   unsigned int            start_job_id,
                                   unsigned int            end_job_id,
                                   size_t                  max_scanned,
                                   size_t                  max_mark_deleted,
                                   const CNSPreciseTime &  current_time,
                                   size_t &                total_scanned,
                                   size_t &                total_mark_deleted)
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


void  CQueueDataBase::x_CreateCrashFlagFile(void)
{
    try {
        CFile       crash_file(CFile::MakePath(m_DataPath,
                                               kCrashFlagFileName));
        if (!crash_file.Exists()) {
            CFileIO     f;
            f.Open(CFile::MakePath(m_DataPath, kCrashFlagFileName),
                   CFileIO_Base::eCreate,
                   CFileIO_Base::eReadWrite);
            f.Close();
        }
    }
    catch (...) {
        ERR_POST("Error creating crash detection file.");
    }
}


void  CQueueDataBase::x_RemoveCrashFlagFile(void)
{
    try {
        CFile       crash_file(CFile::MakePath(m_DataPath,
                                               kCrashFlagFileName));
        if (crash_file.Exists())
            crash_file.Remove();
    }
    catch (...) {
        ERR_POST("Error removing crash detection file. When the server "
                 "restarts it will re-initialize the database.");
    }
}


bool  CQueueDataBase::x_DoesCrashFlagFileExist(void) const
{
    return CFile(CFile::MakePath(m_DataPath, kCrashFlagFileName)).Exists();
}


void  CQueueDataBase::x_CreateDumpErrorFlagFile(void)
{
    try {
        CFile       crash_file(CFile::MakePath(m_DataPath,
                                               kDumpErrorFlagFileName));
        if (!crash_file.Exists()) {
            CFileIO     f;
            f.Open(CFile::MakePath(m_DataPath, kDumpErrorFlagFileName),
                   CFileIO_Base::eCreate,
                   CFileIO_Base::eReadWrite);
            f.Close();
        }
    }
    catch (...) {
        ERR_POST("Error creating dump error  detection file.");
    }
}


bool  CQueueDataBase::x_DoesDumpErrorFlagFileExist(void) const
{
    return CFile(CFile::MakePath(m_DataPath, kDumpErrorFlagFileName)).Exists();
}


void  CQueueDataBase::x_RemoveDumpErrorFlagFile(void)
{
    try {
        CFile       crash_file(CFile::MakePath(m_DataPath,
                                               kDumpErrorFlagFileName));
        if (crash_file.Exists())
            crash_file.Remove();
    }
    catch (...) {
        ERR_POST("Error removing dump error detection file. When the server "
                 "restarts it will set an alert.");
    }
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


void
CQueueDataBase::x_OptimizeStatusMatrix(const CNSPreciseTime &  current_time)
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
}


void CQueueDataBase::StaleWNodes(void)
{
    // Worker nodes have the last access time in seconds since 1970
    // so there is no need to purge them more often than once a second
    static CNSPreciseTime   last_purge(0, 0);
    CNSPreciseTime          current_time = CNSPreciseTime::Current();

    if (current_time.Sec() == last_purge.Sec())
        return;
    last_purge = current_time;

    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->StaleNodes(current_time);
        if (x_CheckStopPurge())
            break;
    }
}


void CQueueDataBase::PurgeBlacklistedJobs(void)
{
    static CNSPreciseTime   period(30, 0);
    static CNSPreciseTime   last_time(0, 0);
    CNSPreciseTime          current_time = CNSPreciseTime::Current();

    // Run this check once in ten seconds
    if (current_time - last_time < period)
        return;

    last_time = current_time;

    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->PurgeBlacklistedJobs();
        if (x_CheckStopPurge())
            break;
    }
}


void CQueueDataBase::PurgeClientRegistry(void)
{
    static CNSPreciseTime   period(5, 0);
    static CNSPreciseTime   last_time(0, 0);
    CNSPreciseTime          current_time = CNSPreciseTime::Current();

    // Run this check once in five seconds
    if (current_time - last_time < period)
        return;

    last_time = current_time;

    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        queue->PurgeClientRegistry(current_time);
        if (x_CheckStopPurge())
            break;
    }
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


void CQueueDataBase::WakeupNotifThread(void)
{
    if (!m_NotifThread.Empty())
        m_NotifThread->WakeUp();
}


CNSPreciseTime
CQueueDataBase::SendExactNotifications(void)
{
    CNSPreciseTime      next = kTimeNever;
    CNSPreciseTime      from_queue;

    for (unsigned int  index = 0; ; ++index) {
        CRef<CQueue>  queue = x_GetQueueAt(index);
        if (queue.IsNull())
            break;
        from_queue = queue->NotifyExactListeners();
        if (from_queue < next )
            next = from_queue;
    }
    return next;
}


void CQueueDataBase::RunServiceThread(void)
{
    // Once in 100 seconds
    m_ServiceThread.Reset(new CServiceThread(
                                *m_Server, m_Host, *this,
                                m_Server->IsLogStatisticsThread(),
                                m_Server->GetStatInterval()));
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


void CQueueDataBase::RunExecutionWatcherThread(const CNSPreciseTime & run_delay)
{
    m_ExeWatchThread.Reset(new CJobQueueExecutionWatcherThread(
                                    m_Host, *this,
                                    run_delay.Sec(), run_delay.NSec(),
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


void CQueueDataBase::x_Dump()
{
    LOG_POST(Note << "Start dumping jobs");

    // Create the directory if needed
    CDir        dump_dir(m_DumpPath);
    if (!dump_dir.Exists())
        dump_dir.Create();

    // Remove the file which reserves the disk space
    x_RemoveSpaceReserveFile();

    // Walk all the queues and dump them
    // Note: no need for a lock because it is a shutdown time
    bool                dump_error = false;
    set<string>         dumped_queues;
    const string        lbsm_test_queue("LBSMDTestQueue");
    for (TQueueInfo::iterator  k = m_Queues.begin();
            k != m_Queues.end(); ++k) {
        if (NStr::CompareNocase(k->first, lbsm_test_queue) != 0) {
            try {
                k->second.second->Dump(m_DumpPath);
                dumped_queues.insert(k->first);
            } catch (const exception &  ex) {
                dump_error = true;
                ERR_POST("Error dumping queue " << k->first << ": " <<
                         ex.what());
            }
        }
    }

    // Dump the required queue classes. The only classes required are those
    // which were used by dynamic queues. The dynamic queue classes may also
    // use linked sections
    set<string>     classes_to_dump;
    set<string>     linked_sections_to_dump;
    set<string>     dynamic_queues_to_dump;
    for (TQueueInfo::iterator  k = m_Queues.begin();
            k != m_Queues.end(); ++k) {
        if (NStr::CompareNocase(k->first, lbsm_test_queue) == 0)
            continue;
        if (k->second.second->GetQueueKind() == CQueue::eKindStatic)
            continue;
        if (dumped_queues.find(k->first) == dumped_queues.end())
            continue;   // There was a dumping error

        classes_to_dump.insert(k->second.first.qclass);
        for (map<string, string>::const_iterator
                j = k->second.first.linked_sections.begin();
                j != k->second.first.linked_sections.end(); ++j)
            linked_sections_to_dump.insert(j->second);
        dynamic_queues_to_dump.insert(k->first);
    }


    // Dump classes if so and linked sections if so
    if (!classes_to_dump.empty()) {
        string      qclasses_dump_file_name = m_DumpPath +
                                            kQClassDescriptionFileName;
        string      linked_sections_dump_file_name = m_DumpPath +
                                            kLinkedSectionsFileName;
        FILE *      qclasses_dump_file = NULL;
        FILE *      linked_sections_dump_file = NULL;
        try {
            qclasses_dump_file = fopen(qclasses_dump_file_name.c_str(), "wb");
            if (qclasses_dump_file == NULL)
                throw runtime_error("Cannot open file " +
                                    qclasses_dump_file_name);
            setbuf(qclasses_dump_file, NULL);

            // Dump dynamic queue classes
            for (set<string>::const_iterator  k = classes_to_dump.begin();
                    k != classes_to_dump.end(); ++k) {
                TQueueParams::const_iterator  queue_class =
                                                    m_QueueClasses.find(*k);
                x_DumpQueueOrClass(qclasses_dump_file, "", *k, false,
                                   queue_class->second);
            }

            // Dump dynamic queues: qname and its class.
            for (set<string>::const_iterator
                    k = dynamic_queues_to_dump.begin();
                    k != dynamic_queues_to_dump.end(); ++k) {
                TQueueInfo::const_iterator  q = m_Queues.find(*k);
                x_DumpQueueOrClass(qclasses_dump_file, *k,
                                   q->second.first.qclass, true,
                                   q->second.first);
            }

            fclose(qclasses_dump_file);
            qclasses_dump_file = NULL;

            // Dump linked sections if so
            if (!linked_sections_to_dump.empty()) {
                linked_sections_dump_file = fopen(
                                linked_sections_dump_file_name.c_str(), "wb");
                if (linked_sections_dump_file == NULL)
                    throw runtime_error("Cannot open file " +
                                         linked_sections_dump_file_name);
                setbuf(linked_sections_dump_file, NULL);

                for (set<string>::const_iterator
                        k = linked_sections_to_dump.begin();
                        k != linked_sections_to_dump.end(); ++k) {
                    map<string, map<string, string> >::const_iterator
                        j = m_LinkedSections.find(*k);
                    x_DumpLinkedSection(linked_sections_dump_file, *k,
                                        j->second);
                }
                fclose(linked_sections_dump_file);
                linked_sections_dump_file = NULL;
            }
        } catch (const exception &  ex) {
            dump_error = true;
            ERR_POST("Error dumping dynamic queue classes and "
                     "their linked sections. Dynamic queue dumps are lost.");
            if (qclasses_dump_file != NULL)
                fclose(qclasses_dump_file);
            if (linked_sections_dump_file != NULL)
                fclose(linked_sections_dump_file);

            // Remove the classes and linked sections files
            if (access(qclasses_dump_file_name.c_str(), F_OK) != -1)
                remove(qclasses_dump_file_name.c_str());
            if (access(linked_sections_dump_file_name.c_str(), F_OK) != -1)
                remove(linked_sections_dump_file_name.c_str());

            // Remove dynamic queues dumps
            for (set<string>::const_iterator
                    k = dynamic_queues_to_dump.begin();
                    k != dynamic_queues_to_dump.end(); ++k) {
                m_Queues[*k].second->RemoveDump(m_DumpPath);
            }
        }
    }

    if (!dump_error)
        x_RemoveDumpErrorFlagFile();

    LOG_POST(Note << "Dumping jobs finished");
}


void CQueueDataBase::x_DumpQueueOrClass(FILE *  f,
                                        const string &  qname,
                                        const string &  qclass,
                                        bool  is_queue,
                                        const SQueueParameters &  params)
{
    SQueueDescriptionDump       descr_dump;

    descr_dump.is_queue = is_queue;
    descr_dump.qname_size = qname.size();
    memcpy(descr_dump.qname, qname.data(), qname.size());
    descr_dump.qclass_size = qclass.size();
    memcpy(descr_dump.qclass, qclass.data(), qclass.size());

    if (!is_queue) {
        // The other parameters are required for the queue classes only
        descr_dump.timeout = (double)params.timeout;
        descr_dump.notif_hifreq_interval = (double)params.notif_hifreq_interval;
        descr_dump.notif_hifreq_period = (double)params.notif_hifreq_period;
        descr_dump.notif_lofreq_mult = params.notif_lofreq_mult;
        descr_dump.notif_handicap = (double)params.notif_handicap;
        descr_dump.dump_buffer_size = params.dump_buffer_size;
        descr_dump.dump_client_buffer_size = params.dump_client_buffer_size;
        descr_dump.dump_aff_buffer_size = params.dump_aff_buffer_size;
        descr_dump.dump_group_buffer_size = params.dump_group_buffer_size;
        descr_dump.run_timeout = (double)params.run_timeout;
        descr_dump.read_timeout = (double)params.read_timeout;
        descr_dump.program_name_size = params.program_name.size();
        memcpy(descr_dump.program_name, params.program_name.data(),
                                        params.program_name.size());
        descr_dump.failed_retries = params.failed_retries;
        descr_dump.read_failed_retries = params.read_failed_retries;
        descr_dump.blacklist_time = (double)params.blacklist_time;
        descr_dump.read_blacklist_time = (double)params.read_blacklist_time;
        descr_dump.max_input_size = params.max_input_size;
        descr_dump.max_output_size = params.max_output_size;
        descr_dump.subm_hosts_size = params.subm_hosts.size();
        memcpy(descr_dump.subm_hosts, params.subm_hosts.data(),
                                      params.subm_hosts.size());
        descr_dump.wnode_hosts_size = params.wnode_hosts.size();
        memcpy(descr_dump.wnode_hosts, params.wnode_hosts.data(),
                                       params.wnode_hosts.size());
        descr_dump.reader_hosts_size = params.reader_hosts.size();
        memcpy(descr_dump.reader_hosts, params.reader_hosts.data(),
                                        params.reader_hosts.size());
        descr_dump.wnode_timeout = (double)params.wnode_timeout;
        descr_dump.reader_timeout = (double)params.reader_timeout;
        descr_dump.pending_timeout = (double)params.pending_timeout;
        descr_dump.max_pending_wait_timeout =
                                (double)params.max_pending_wait_timeout;
        descr_dump.max_pending_read_wait_timeout =
                                (double)params.max_pending_read_wait_timeout;
        descr_dump.description_size = params.description.size();
        memcpy(descr_dump.description, params.description.data(),
                                       params.description.size());
        descr_dump.scramble_job_keys = params.scramble_job_keys;
        descr_dump.client_registry_timeout_worker_node =
                        (double)params.client_registry_timeout_worker_node;
        descr_dump.client_registry_min_worker_nodes =
                        params.client_registry_min_worker_nodes;
        descr_dump.client_registry_timeout_admin =
                        (double)params.client_registry_timeout_admin;
        descr_dump.client_registry_min_admins =
                        params.client_registry_min_admins;
        descr_dump.client_registry_timeout_submitter =
                        (double)params.client_registry_timeout_submitter;
        descr_dump.client_registry_min_submitters =
                        params.client_registry_min_submitters;
        descr_dump.client_registry_timeout_reader =
                        (double)params.client_registry_timeout_reader;
        descr_dump.client_registry_min_readers =
                        params.client_registry_min_readers;
        descr_dump.client_registry_timeout_unknown =
                        (double)params.client_registry_timeout_unknown;
        descr_dump.client_registry_min_unknowns =
                        params.client_registry_min_unknowns;

        // Dump the linked sections prefixes and names in the same order
        string      prefixes;
        string      names;
        for (map<string, string>::const_iterator
                k = params.linked_sections.begin();
                k != params.linked_sections.end(); ++k) {
            if (!prefixes.empty()) {
                prefixes += ",";
                names += ",";
            }
            prefixes += k->first;
            names += k->second;
        }
        descr_dump.linked_section_prefixes_size = prefixes.size();
        memcpy(descr_dump.linked_section_prefixes, prefixes.data(),
                                                   prefixes.size());
        descr_dump.linked_section_names_size = names.size();
        memcpy(descr_dump.linked_section_names, names.data(), names.size());
    }

    try {
        descr_dump.Write(f);
    } catch (const exception &  ex) {
        string      msg = "Writing error while dumping queue ";
        if (is_queue)
            msg += qname;
        else
            msg += "class " + qclass;
        msg += string(": ") + ex.what();
        throw runtime_error(msg);
    }
}


void CQueueDataBase::x_DumpLinkedSection(FILE *  f, const string &  sname,
                                         const map<string, string> &  values)
{
    for (map<string, string>::const_iterator  k = values.begin();
            k != values.end(); ++k) {
        SLinkedSectionDump      section_dump;

        section_dump.section_size = sname.size();
        memcpy(section_dump.section, sname.data(), sname.size());
        section_dump.value_name_size = k->first.size();
        memcpy(section_dump.value_name, k->first.data(), k->first.size());
        section_dump.value_size = k->second.size();
        memcpy(section_dump.value, k->second.data(), k->second.size());

        try {
            section_dump.Write(f);
        } catch (const exception &  ex) {
            throw runtime_error("Writing error while dumping linked section " +
                                sname + " values: " + ex.what());
        }
    }
}


void CQueueDataBase::x_RemoveDump(void)
{
    try {
        CDir    dump_dir(m_DumpPath);
        if (dump_dir.Exists())
            dump_dir.Remove();
    } catch (const exception &  ex) {
        ERR_POST("Error removing the dump directory: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error removing the dump directory");
    }
}


void CQueueDataBase::x_RemoveBDBFiles(void)
{
    CDir        data_dir(m_DataPath);
    if (!data_dir.Exists())
        return;

    CDir::TEntries      entries = data_dir.GetEntries(
                                    kEmptyStr, CDir::fIgnoreRecursive);
    for (CDir::TEntries::const_iterator  k = entries.begin();
            k != entries.end(); ++k) {
        if ((*k)->IsDir())
            continue;
        if ((*k)->IsLink())
            continue;
        string      entryName = (*k)->GetName();
        if (entryName == kDBStorageVersionFileName ||
            entryName == kNodeIDFileName ||
            entryName == kStartJobIDsFileName ||
            entryName == kCrashFlagFileName ||
            entryName == kDumpErrorFlagFileName)
            continue;

        CFile   f(m_DataPath + entryName);
        try {
            f.Remove();
        } catch (...) {}
    }
}


// Logs the corresponding message if needed and provides the overall reinit
// status.
bool CQueueDataBase::x_CheckOpenPreconditions(bool  reinit)
{
    if (x_DoesCrashFlagFileExist()) {
        ERR_POST("Reinitialization due to the server "
                 "did not stop gracefully last time. "
                 << m_DataPath << " removed.");
        m_Server->RegisterAlert(eStartAfterCrash, "Database has been "
                                "reinitialized due to the server did not "
                                "stop gracefully last time");
        return true;
    }

    if (reinit) {
        LOG_POST(Note << "Reinitialization due to a command line option. "
                      << m_DataPath << " removed.");
        m_Server->RegisterAlert(eReinit, "Database has been reinitialized due "
                                         "to a command line option");
        return true;
    }

    if (CDir(m_DataPath).Exists()) {
        bool    ver_file_exists = CFile(m_DataPath +
                                        kDBStorageVersionFileName).Exists();
        bool    dump_dir_exists = CDir(m_DumpPath).Exists();

        if (dump_dir_exists && !ver_file_exists) {
            // Strange. Some service file exist while the storage version
            // does not. It might be that the data dir has been altered
            // manually. Let's not start.
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error detected: Storage version file is not found "
                       "while the dump directory exists.");
        }

        if (dump_dir_exists && ver_file_exists) {
            CFileIO     f;
            char        buf[32];
            size_t      read_count = 0;
            string      storage_ver;

            f.Open(m_DataPath + kDBStorageVersionFileName,
                   CFileIO_Base::eOpen, CFileIO_Base::eRead);

            read_count = f.Read(buf, sizeof(buf));
            storage_ver.append(buf, read_count);
            NStr::TruncateSpacesInPlace(storage_ver, NStr::eTrunc_End);
            f.Close();

            if (storage_ver != NETSCHEDULED_STORAGE_VERSION)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "Error detected: Storage version mismatch, "
                           "required: " NETSCHEDULED_STORAGE_VERSION
                           ", found: " + storage_ver);
        }

        if (!dump_dir_exists && ver_file_exists) {
            LOG_POST(Note << "Non-empty data directory exists however the "
                          << kDumpSubdirName
                          << " subdirectory is not found");
            m_Server->RegisterAlert(eNoDump,
                                    "Non-empty data directory exists "
                                    "however the " + kDumpSubdirName +
                                    " subdirectory is not found");
        }
    }

    if (x_DoesDumpErrorFlagFileExist()) {
        string  msg = "The previous instance of the server had problems with "
                      "dumping the information on the disk. Some queues may be "
                      "not restored. "
                      "See the previous instance log for details.";
        LOG_POST(Note << msg);
        m_Server->RegisterAlert(eDumpError, msg);
    }

    return false;
}


CBDB_Env *
CQueueDataBase::x_CreateBDBEnvironment(const SNSDBEnvironmentParams &  params)
{
    string      trailed_log_path =
                        CDirEntry::AddTrailingPathSeparator(params.db_log_path);
    string      err_file = m_DataPath + "errjsqueue.log";
    CBDB_Env *  env = new CBDB_Env();

    if (!trailed_log_path.empty())
        env->SetLogDir(trailed_log_path);

    env->OpenErrFile(err_file.c_str());

    env->SetLogRegionMax(512 * 1024);
    if (params.log_mem_size) {
        env->SetLogInMemory(true);
        env->SetLogBSize(params.log_mem_size);
    } else {
        env->SetLogFileMax(200 * 1024 * 1024);
        env->SetLogAutoRemove(true);
    }

    CBDB_Env::TEnvOpenFlags opt = CBDB_Env::eThreaded;
    if (params.private_env)
        opt |= CBDB_Env::ePrivate;

    if (params.cache_ram_size)
        env->SetCacheSize(params.cache_ram_size);
    if (params.mutex_max)
        env->MutexSetMax(params.mutex_max);
    if (params.max_locks)
        env->SetMaxLocks(params.max_locks);
    if (params.max_lockers)
        env->SetMaxLockers(params.max_lockers);
    if (params.max_lockobjects)
        env->SetMaxLockObjects(params.max_lockobjects);
    if (params.max_trans)
        env->SetTransactionMax(params.max_trans);
    env->SetTransactionSync(params.sync_transactions ?
                                  CBDB_Transaction::eTransSync :
                                  CBDB_Transaction::eTransASync);

    env->OpenWithTrans(m_DataPath.c_str(), opt);
    GetDiagContext().Extra()
        .Print("_type", "startup")
        .Print("info", "opened BDB environment")
        .Print("private", params.private_env ? "true" : "false")
        .Print("max_locks", env->GetMaxLocks())
        .Print("transactions",
               env->GetTransactionSync() == CBDB_Transaction::eTransSync ?
                    "syncronous" : "asyncronous")
        .Print("max_mutexes", env->MutexGetMax());

    env->SetDirectDB(params.direct_db);
    env->SetDirectLog(params.direct_log);

    env->SetCheckPointKB(params.checkpoint_kb);
    env->SetCheckPointMin(params.checkpoint_min);

    env->SetLockTimeout(10 * 1000000); // 10 sec

    env->SetTasSpins(5);

    if (env->IsTransactional()) {
        env->SetTransactionTimeout(10 * 1000000); // 10 sec
        env->ForceTransactionCheckpoint();
        env->CleanLog();
    }

    return env;
}


void CQueueDataBase::x_CreateStorageVersionFile(void)
{
    CFileIO     f;
    f.Open(m_DataPath + kDBStorageVersionFileName, CFileIO_Base::eCreate,
                                                   CFileIO_Base::eReadWrite);
    f.Write(NETSCHEDULED_STORAGE_VERSION, strlen(NETSCHEDULED_STORAGE_VERSION));
    f.Close();
}


void
CQueueDataBase::x_ReadDumpQueueDesrc(set<string, PNocase> &  dump_static_queues,
                                     map<string, string,
                                         PNocase> &  dump_dynamic_queues,
                                     TQueueParams &  dump_queue_classes)
{
    CDir        dump_dir(m_DumpPath);
    if (!dump_dir.Exists())
        return;

    // The file contains dynamic queue classes and dynamic queue names
    string      queue_desrc_file_name = m_DumpPath + kQClassDescriptionFileName;
    CFile       queue_desrc_file(queue_desrc_file_name);
    if (queue_desrc_file.Exists()) {
        FILE *      f = fopen(queue_desrc_file_name.c_str(), "rb");
        if (f == NULL)
            throw runtime_error("Cannot open the existing dump file "
                                "for reading: " + queue_desrc_file_name);
        SQueueDescriptionDump   dump_struct;
        try {
            while (dump_struct.Read(f) == 0) {
                if (dump_struct.is_queue) {
                    string  qname(dump_struct.qname, dump_struct.qname_size);
                    string  qclass(dump_struct.qclass, dump_struct.qclass_size);
                    dump_dynamic_queues[qname] = qclass;
                } else {
                    SQueueParameters    p;
                    p.qclass = "";
                    p.timeout = CNSPreciseTime(dump_struct.timeout);
                    p.notif_hifreq_interval =
                        CNSPreciseTime(dump_struct.notif_hifreq_interval);
                    p.notif_hifreq_period =
                        CNSPreciseTime(dump_struct.notif_hifreq_period);
                    p.notif_lofreq_mult = dump_struct.notif_lofreq_mult;
                    p.notif_handicap =
                        CNSPreciseTime(dump_struct.notif_handicap);
                    p.dump_buffer_size = dump_struct.dump_buffer_size;
                    p.dump_client_buffer_size =
                        dump_struct.dump_client_buffer_size;
                    p.dump_aff_buffer_size = dump_struct.dump_aff_buffer_size;
                    p.dump_group_buffer_size =
                        dump_struct.dump_group_buffer_size;
                    p.run_timeout = CNSPreciseTime(dump_struct.run_timeout);
                    p.read_timeout = CNSPreciseTime(dump_struct.read_timeout);
                    p.program_name = string(dump_struct.program_name,
                                            dump_struct.program_name_size);
                    p.failed_retries = dump_struct.failed_retries;
                    p.read_failed_retries = dump_struct.read_failed_retries;
                    p.blacklist_time =
                        CNSPreciseTime(dump_struct.blacklist_time);
                    p.read_blacklist_time =
                        CNSPreciseTime(dump_struct.read_blacklist_time);
                    p.max_input_size = dump_struct.max_input_size;
                    p.max_output_size = dump_struct.max_output_size;
                    p.subm_hosts = string(dump_struct.subm_hosts,
                                          dump_struct.subm_hosts_size);
                    p.wnode_hosts = string(dump_struct.wnode_hosts,
                                           dump_struct.wnode_hosts_size);
                    p.reader_hosts = string(dump_struct.reader_hosts,
                                            dump_struct.reader_hosts_size);
                    p.wnode_timeout =
                        CNSPreciseTime(dump_struct.wnode_timeout);
                    p.reader_timeout =
                        CNSPreciseTime(dump_struct.reader_timeout);
                    p.pending_timeout =
                        CNSPreciseTime(dump_struct.pending_timeout);
                    p.max_pending_wait_timeout =
                        CNSPreciseTime(dump_struct.max_pending_wait_timeout);
                    p.max_pending_read_wait_timeout =
                        CNSPreciseTime(dump_struct.
                                max_pending_read_wait_timeout);
                    p.description = string(dump_struct.description,
                                           dump_struct.description_size);
                    p.scramble_job_keys = dump_struct.scramble_job_keys;
                    p.client_registry_timeout_worker_node =
                        CNSPreciseTime(dump_struct.
                                client_registry_timeout_worker_node);
                    p.client_registry_min_worker_nodes =
                        dump_struct.client_registry_min_worker_nodes;
                    p.client_registry_timeout_admin =
                        CNSPreciseTime(dump_struct.
                                client_registry_timeout_admin);
                    p.client_registry_min_admins =
                        dump_struct.client_registry_min_admins;
                    p.client_registry_timeout_submitter =
                        CNSPreciseTime(dump_struct.
                                client_registry_timeout_submitter);
                    p.client_registry_min_submitters =
                        dump_struct.client_registry_min_submitters;
                    p.client_registry_timeout_reader =
                        CNSPreciseTime(dump_struct.
                                client_registry_timeout_reader);
                    p.client_registry_min_readers =
                        dump_struct.client_registry_min_readers;
                    p.client_registry_timeout_unknown =
                        CNSPreciseTime(dump_struct.
                                client_registry_timeout_unknown);
                    p.client_registry_min_unknowns =
                        dump_struct.client_registry_min_unknowns;;

                    // Unpack linked sections
                    string          dump_prefs(dump_struct.
                                                linked_section_prefixes,
                                               dump_struct.
                                                linked_section_prefixes_size);
                    string          dump_names(dump_struct.
                                                linked_section_names,
                                               dump_struct.
                                                linked_section_names_size);
                    list<string>    prefixes;
                    list<string>    names;
                    NStr::Split(dump_prefs, ",", prefixes);
                    NStr::Split(dump_names, ",", names);
                    list<string>::const_iterator pref_it = prefixes.begin();
                    list<string>::const_iterator names_it = names.begin();
                    for ( ; pref_it != prefixes.end() &&
                            names_it != names.end(); ++pref_it, ++names_it)
                        p.linked_sections[*pref_it] = *names_it;

                    string  qclass(dump_struct.qclass, dump_struct.qclass_size);
                    dump_queue_classes[qclass] = p;
                }
            }
        } catch (const exception &  ex) {
            fclose(f);
            throw;
        }
        fclose(f);
    }


    CDir::TEntries      entries = dump_dir.GetEntries(
                                    kEmptyStr, CDir::fIgnoreRecursive);
    for (CDir::TEntries::const_iterator  k = entries.begin();
            k != entries.end(); ++k) {
        if ((*k)->IsDir())
            continue;
        if ((*k)->IsLink())
            continue;
        string      entry_name = (*k)->GetName();
        if (!NStr::StartsWith(entry_name, "db_dump."))
            continue;

        string  prefix;
        string  qname;
        NStr::SplitInTwo(entry_name, ".", prefix, qname);
        if (dump_dynamic_queues.find(qname) == dump_dynamic_queues.end())
            dump_static_queues.insert(qname);
    }
}


set<string, PNocase> CQueueDataBase::x_GetConfigQueues(void)
{
    const CNcbiRegistry &   reg = CNcbiApplication::Instance()->GetConfig();
    set<string, PNocase>    queues;
    list<string>            sections;

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
            k != sections.end(); ++k) {
        string              queue_name;
        string              prefix;
        const string &      section_name = *k;

        NStr::SplitInTwo(section_name, "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, "queue") != 0)
            continue;
        if (queue_name.empty())
            continue;
        if (queue_name.size() > kMaxQueueNameSize - 1)
            continue;
        queues.insert(queue_name);
    }

    return queues;
}


void CQueueDataBase::x_AppendDumpLinkedSections(void)
{
    // Here: the m_LinkedSections has already been read from the configuration
    //       file. Let's append the sections from the dump.
    CDir        dump_dir(m_DumpPath);
    if (!dump_dir.Exists())
        return;

    string  linked_sections_file_name = m_DumpPath + kLinkedSectionsFileName;
    CFile   linked_sections_file(linked_sections_file_name);

    if (linked_sections_file.Exists()) {
        FILE *      f = fopen(linked_sections_file_name.c_str(), "rb");
        if (f == NULL)
            throw runtime_error("Cannot open the existing dump file "
                                "for reading: " + linked_sections_file_name);
        SLinkedSectionDump                  dump_struct;
        map<string, map<string, string> >   dump_sections;
        try {
            while (dump_struct.Read(f) == 0) {
                if (m_LinkedSections.find(dump_struct.section) !=
                        m_LinkedSections.end())
                    continue;
                string  sname(dump_struct.section, dump_struct.section_size);
                string  vname(dump_struct.value_name,
                              dump_struct.value_name_size);
                string  val(dump_struct.value, dump_struct.value_size);
                dump_sections[sname][vname] = val;
            }
        } catch (const exception &  ex) {
            fclose(f);
            throw;
        }
        fclose(f);

        m_LinkedSections.insert(dump_sections.begin(), dump_sections.end());
    }
}


void CQueueDataBase::x_BackupDump(void)
{
    CDir    dump_dir(m_DumpPath);
    if (!dump_dir.Exists())
        return;

    size_t      backup_number = 0;
    string      backup_dir_name;
    for ( ; ; ) {
        backup_dir_name = CDirEntry::DeleteTrailingPathSeparator(m_DumpPath) +
                          "." +
                          NStr::NumericToString(backup_number);
        if (!CDir(backup_dir_name).Exists())
            break;
        ++backup_number;
    }

    try {
        dump_dir.Rename(backup_dir_name);
    } catch (const exception &  ex) {
        ERR_POST("Error renaming the dump directory: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error renaming the dump directory");
    }
}


string CQueueDataBase::x_GetDumpSpaceFileName(void) const
{
    return m_DumpPath + kDumpReservedSpaceFileName;
}


bool CQueueDataBase::x_RemoveSpaceReserveFile(void)
{
    CFile       space_file(x_GetDumpSpaceFileName());
    if (space_file.Exists()) {
        try {
            space_file.Remove();
        } catch (const exception &  ex) {
            string  msg = "Error removing reserving dump space file: " +
                          string(ex.what());
            ERR_POST(msg);
            m_Server->RegisterAlert(eDumpSpaceError, msg);
            return false;
        }
    }
    return true;
}


void CQueueDataBase::x_CreateSpaceReserveFile(void)
{
    unsigned int        space = m_Server->GetReserveDumpSpace();
    if (space == 0)
        return;

    CDir    dump_dir(m_DumpPath);
    if (!dump_dir.Exists()) {
        try {
            dump_dir.Create();
        } catch (const exception &  ex) {
            string  msg = "Error creating dump directory: " + string(ex.what());
            ERR_POST(msg);
            m_Server->RegisterAlert(eDumpSpaceError, msg);
            return;
        }
    }

    // This will truncate the file if it existed
    FILE *      space_file = fopen(x_GetDumpSpaceFileName().c_str(), "w");
    if (space_file == NULL) {
        string  msg = "Error opening reserving dump space file " +
                      x_GetDumpSpaceFileName();
        ERR_POST(msg);
        m_Server->RegisterAlert(eDumpSpaceError, msg);
        return;
    }

    void *      buffer = malloc(kDumpReservedSpaceFileBuffer);
    if (buffer == NULL) {
        fclose(space_file);
        string  msg = "Error creating a memory buffer to write into the "
                      "reserving dump space file";
        ERR_POST(msg);
        m_Server->RegisterAlert(eDumpSpaceError, msg);
        return;
    }

    memset(buffer, 0, kDumpReservedSpaceFileBuffer);
    while (space > kDumpReservedSpaceFileBuffer) {
        errno = 0;
        if (fwrite(buffer, kDumpReservedSpaceFileBuffer, 1, space_file) != 1) {
            free(buffer);
            fclose(space_file);
            string  msg = "Error writing into the reserving dump space file: " +
                          string(strerror(errno));
            ERR_POST(msg);
            m_Server->RegisterAlert(eDumpSpaceError, msg);
            return;
        }
        space -= kDumpReservedSpaceFileBuffer;
    }

    if (space > 0) {
        errno = 0;
        if (fwrite(buffer, space, 1, space_file) != 1) {
            string  msg = "Error writing into the reserving dump space file: " +
                          string(strerror(errno));
            ERR_POST(msg);
            m_Server->RegisterAlert(eDumpSpaceError, msg);
        }
    }

    free(buffer);
    fclose(space_file);
}


END_NCBI_SCOPE

