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
 * Author: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: API to support blobs synchronization
 *
 */

#include "nc_pch.hpp"

#include <corelib/ncbidbg.hpp>

#include "sync_log.hpp"
#include "distribution_conf.hpp"
#include "task_server.hpp"



BEGIN_NCBI_SCOPE


struct SSlotData
{
    CMiniMutex  lock;
    TSyncEvents events;
    Uint8       rec_number;

    SSlotData(void) : rec_number(0)
    {}

    SSlotData(const SSlotData& other) : rec_number(0)
    {
        if (other.rec_number != 0  ||  !other.events.empty())
            abort();
    }
};


struct SSrvSyncedData
{
    Uint8 local_rec_no;
    Uint8 remote_rec_no;

    SSrvSyncedData(void) : local_rec_no(0), remote_rec_no(0)
    {}
};


static CMiniMutex       s_GlobalLock;
typedef map<Uint2, SSlotData> TLog;
static TLog             s_Log;
typedef map<Uint2, SSrvSyncedData>  TSrvSyncedMap;
typedef map<Uint8, TSrvSyncedMap>   TSyncedRecsMap;
static TSyncedRecsMap   s_SyncedData;
static CAtomicCounter   s_TotalRecords;
static Uint8            s_LastWrittenRecord;


// File IO supporting structures
static const size_t kMaxKeyLength = 1024;

struct SFixedPart
{
    Uint8         rec_no;       //< Local event sequential number.
    ENCSyncEvent  event_type;   //< Event type (write, remove, prolong).
    Uint2         slot;         //< Key slot number.
    Uint8         orig_time;    //< Timestamp of the event when
                                //< it originated by client.
    Uint8         orig_server;  //< The server where event has
                                //< been originated.
    Uint8         orig_rec_no;  //< Record number on the host where the
                                //< event was originated.
    Uint8         local_time;   //< Timestamp when the record was
                                //< recorded locally.
};

struct SFileServRecord
{
    Uint8 key_server;
    Uint2 key_slot;
    Uint8 local_rec_no;
    Uint8 remote_rec_no;
};


// Reads the beginning of the file where the information about last synced
// records per pair server <--> slot is saved.
// Returns true if this information is read successfully.
static bool
s_ReadHeader(FILE* file)
{
    size_t count = 0;

    // Read the number of pairs server <--> slot
    if (fread(&count, sizeof(count), 1, file) != 1) {
        SRV_LOG(Critical, "Cannot read the number of saved pairs(server <--> slot) "
                          "with the last synced record numbers. Invalid file?");
        return false;
    }

    for (; count > 0; --count) {
        SFileServRecord record;

        if (fread(&record, sizeof(record), 1, file) != 1) {
            SRV_LOG(Critical, "Cannot read last synced record numbers. Invalid file?");
            return false;
        }
        SSrvSyncedData& sync_data = s_SyncedData[record.key_server]
                                                [record.key_slot];
        sync_data.local_rec_no = record.local_rec_no;
        sync_data.remote_rec_no = record.remote_rec_no;
    }

    typedef map<Uint8, string> TPeerMap;
    const TPeerMap& peers = CNCDistributionConf::GetPeers();
    ERASE_ITERATE(TSyncedRecsMap, it_srv, s_SyncedData) {
        bool valid = false;
        if (peers.find(it_srv->first) != peers.end()) {
            const vector<Uint2>& slots
                            = CNCDistributionConf::GetCommonSlots(it_srv->first);
            if (!slots.empty()) {
                valid = true;
                TSrvSyncedMap& srv_map = it_srv->second;
                ERASE_ITERATE(TSrvSyncedMap, it_slot, srv_map) {
                    if (find(slots.begin(), slots.end(), it_slot->first)
                                                                == slots.end())
                    {
                        srv_map.erase(it_slot);
                    }
                }
            }
        }
        if (!valid)
            s_SyncedData.erase(it_srv);
    }

    return true;
}


// Reads and saves a single saved event record.
// Returns true if the event was read successfully.
static bool
s_ReadRecord(FILE* file)
{
    // Read the fixed part
    SFixedPart record;
    if (fread(&record, sizeof(record), 1, file) != 1)
        return false;

    // Read the key size
    size_t key_size;
    if (fread(&key_size, sizeof(key_size), 1, file) != 1)
        return false;

    // Read the key
    char key[kMaxKeyLength];
    if (fread(key, key_size, 1, file) != 1)
        return false;

    // Insert the corresponding record
    SNCSyncEvent* new_record = new SNCSyncEvent;

    new_record->rec_no      = record.rec_no;
    new_record->event_type  = record.event_type;
    new_record->orig_time   = record.orig_time;
    new_record->orig_server = record.orig_server;
    new_record->orig_rec_no = record.orig_rec_no;
    new_record->local_time  = record.local_time;
    new_record->key         = string(key, key_size);

    s_Log[record.slot].events.push_back(new_record);
    return true;
}


// Writes the beginning of the file where the information about last synced
// records per pair server <--> slot is saved.
// Returns true if this information is written successfully.
static bool
s_WriteHeader(FILE* file)
{
    size_t count = 0;
    ITERATE(TSyncedRecsMap, it_srv, s_SyncedData) {
        count += it_srv->second.size();
    }

    if (fwrite(&count, sizeof(count), 1, file) != 1)
        return false;

    ITERATE(TSyncedRecsMap, it_srv, s_SyncedData) {
        ITERATE(TSrvSyncedMap, it_slot, it_srv->second) {
            SFileServRecord record;
            record.key_server = it_srv->first;
            record.key_slot = it_slot->first;
            record.local_rec_no = it_slot->second.local_rec_no;
            record.remote_rec_no = it_slot->second.remote_rec_no;

            if (fwrite(&record, sizeof(record), 1, file) != 1)
                return false;
        }
    }
    return true;
}


// Writes a single event record.
// Returns true if the event is written successfully.
static bool
s_WriteRecord(FILE* file, Uint2 slot, const SNCSyncEvent* event)
{
    SFixedPart record;
    memset(&record, 0, sizeof(record));
    record.rec_no       = event->rec_no;
    record.event_type   = event->event_type;
    record.slot         = slot;
    record.orig_time    = event->orig_time;
    record.orig_server  = event->orig_server;
    record.orig_rec_no  = event->orig_rec_no;
    record.local_time   = event->local_time;

    // Write fixed size fields
    if (fwrite(&record, sizeof(record), 1, file) != 1)
        return false;

    // Write the key size
    size_t key_size = event->key.size();
    if (fwrite(&key_size, sizeof(key_size), 1, file) != 1)
        return false;

    // Write the key
    if (fwrite(event->key.data(), key_size, 1, file) != 1)
        return false;

    return true;
}

static inline SSlotData&
s_GetSlotData(Uint2 slot)
{
    CMiniMutexGuard guard(s_GlobalLock);
    return s_Log[slot];
}


// Provides the minimum record number till which the synchronization is done
// with all the servers
static Uint8
s_GetMinLocalSyncedRecordNo(Uint2 slot, const SSlotData& data)
{
    CMiniMutexGuard guard(s_GlobalLock);

    Uint8 min_rec_no = (data.events.empty()? s_LastWrittenRecord
                                           : data.events.front()->rec_no);
    Uint8 result = s_LastWrittenRecord;
    NON_CONST_ITERATE(TSyncedRecsMap, it_srv, s_SyncedData) {
        SSrvSyncedData& sync_data = it_srv->second[slot];
        if (sync_data.local_rec_no >= min_rec_no
            &&  sync_data.local_rec_no < result)
        {
            result = sync_data.local_rec_no;
        }
    }
    return result;
}


// The prolong event is found in the src interval
static void
s_ProcessProlong(const SBlobEvent& src_event,
                 const SBlobEvent& other_event,
                 TSyncEvents*      diff)
{
    if (other_event.prolong_event != NULL) {
        if (other_event.prolong_event->orig_server == src_event.prolong_event->orig_server
            &&  other_event.prolong_event->orig_rec_no == src_event.prolong_event->orig_rec_no)
        {
            return;
        }

        if (other_event.prolong_event->isOlder(*src_event.prolong_event)
            &&  ((src_event.wr_or_rm_event == NULL)
                 ||  (src_event.wr_or_rm_event != NULL
                      &&  other_event.wr_or_rm_event != NULL
                      &&  other_event.wr_or_rm_event->orig_server == src_event.wr_or_rm_event->orig_server
                      &&  other_event.wr_or_rm_event->orig_rec_no == src_event.wr_or_rm_event->orig_rec_no)
                 ||  (src_event.wr_or_rm_event != NULL
                      &&  other_event.wr_or_rm_event != NULL
                      &&  src_event.wr_or_rm_event->isOlder(*other_event.wr_or_rm_event))))
        {
            diff->push_back(src_event.prolong_event);
        }
    }
    else if (other_event.wr_or_rm_event != NULL) {
        if (other_event.wr_or_rm_event->event_type == eSyncWrite) {
            if (other_event.wr_or_rm_event->isOlder(*src_event.prolong_event)
                &&  ((src_event.wr_or_rm_event == NULL)
                     ||  (src_event.wr_or_rm_event != NULL
                          &&  other_event.wr_or_rm_event->orig_server == src_event.wr_or_rm_event->orig_server
                          &&  other_event.wr_or_rm_event->orig_rec_no == src_event.wr_or_rm_event->orig_rec_no)
                     ||  (src_event.wr_or_rm_event != NULL
                          &&  src_event.wr_or_rm_event->isOlder(*other_event.wr_or_rm_event))))
            {
                diff->push_back(src_event.prolong_event);
            }
        }
    }
}

static void
s_ProcessWrite(SNCSyncEvent*     src_event,
               const SBlobEvent& other_event,
               TSyncEvents*      diff)
{
    if (other_event.wr_or_rm_event != NULL) {
        // If there is write or remove it does not matter if there was
        // prolong or not
        if ((other_event.wr_or_rm_event->orig_server != src_event->orig_server
             ||  other_event.wr_or_rm_event->orig_rec_no != src_event->orig_rec_no)
            &&  other_event.wr_or_rm_event->isOlder(*src_event))
        {
            // Take the most recent event
            diff->push_back(src_event);
            return;
        }
    }
    else {
        // This is lone prolong
        diff->push_back(src_event);
    }
}


static bool
s_SpecialFind(const TReducedSyncEvents& container,
              TReducedSyncEvents::const_iterator& current_iterator,
              const string& key)
{
    // It is known that the container is sorted by keys, so
    for (; current_iterator != container.end(); ++current_iterator) {
        if (current_iterator->first == key)
            return true;
        if (current_iterator->first > key)
            return false;
    }
    return false;
}


// Here is how the decision is made:
//         |  User RM    | WR             | Prolong  <-- found in src interval
// ----------------------------------------------------------
// User RM | none        | if WR later    | none
//         |             | => take WR     |
// WR      | if RM later | if WRsrc later | if PR later
//         | => take RM  | => take WRsrc  | => take PR
// Prolong | *           | **             | if PRsrc later
//         |             |                | => take PRsrc
// ^- found in other
//
// * - if it was a lonely prolong => take User RM
//     if there was WRother => take User RM if it is later than
//     WRother
// ** - if it was a lonely prolong => take WR
//      if there was WRother => take WRsrc if it is later
static void
s_CompareEvents(const TReducedSyncEvents& src,
                Uint8 start_rec_no,
                Uint8 now,
                const TReducedSyncEvents& other,
                Uint8* synced_rec_no,
                TSyncEvents* diff)
{
    TReducedSyncEvents::const_iterator k = src.begin();
    TReducedSyncEvents::const_iterator src_end = src.end();
    Uint8 max_rec_no = 0;
    Uint8 time_limit = now - CNCDistributionConf::GetPeriodicSyncHeadTime();
    TReducedSyncEvents::const_iterator other_event = other.begin();

    for (; k != src_end; ++k) {
        Uint8 op_rec_no = k->second.getMaxRecNoWithinTimeLimit(time_limit);
        if (op_rec_no <= start_rec_no)
            continue;

        // Update the synced record #
        if (op_rec_no > max_rec_no)
            max_rec_no = op_rec_no;

        if (s_SpecialFind(other, other_event, k->first) == false) {
            // No operations found with this blob - take the ops
            if (k->second.wr_or_rm_event != NULL
                &&  (k->second.wr_or_rm_event->rec_no > start_rec_no))
            {
                diff->push_back(k->second.wr_or_rm_event);
            }
            else if (k->second.prolong_event != NULL
                     &&  (k->second.prolong_event->local_time < time_limit))
            {
                diff->push_back(k->second.prolong_event);
            }
            continue;
        }

        // Found on the other side, let's make the decision
        if (k->second.wr_or_rm_event != NULL
            &&  (k->second.wr_or_rm_event->rec_no > start_rec_no))
        {
            s_ProcessWrite(k->second.wr_or_rm_event, other_event->second, diff);
        }
        if (k->second.prolong_event != NULL
            &&  (k->second.prolong_event->local_time < time_limit))
        {
            s_ProcessProlong(k->second, other_event->second, diff);
        }
    }

    if (max_rec_no != 0)
        *synced_rec_no = max_rec_no;
    else
        *synced_rec_no = start_rec_no;
}


// This call is guaranteed to be done once in one thread only.
// So there is no need to bother about thread safety.
void
CNCSyncLog::Initialize(bool need_read_saved, Uint8 start_log_rec_no)
{
    s_TotalRecords.Set(0);
    s_LastWrittenRecord = 0;

    string file_name = CNCDistributionConf::GetSyncLogFileName();
    if (!need_read_saved  ||  file_name.empty()) {
        // No need to load the log from the file.
        // Start from the given record number.
        s_LastWrittenRecord = start_log_rec_no;
        return;
    }

    // Need to load the log from the file and identify the record number to
    // start with.
    FILE* log_file = fopen(file_name.c_str(), "r");

    if (!log_file) {
        SRV_LOG(Warning, "Cannot open file: " << file_name);

        // Could not read the file and therefore could not identify the
        // start record number. So use the given
        s_LastWrittenRecord = start_log_rec_no;
        return;
    }

    // Read the server records
    if (!s_ReadHeader(log_file)) {
        // Error occurred, the file is broken
        s_SyncedData.clear();
        s_LastWrittenRecord = start_log_rec_no;
        return;
    }

    // Read the log records
    Uint4 recs_count = 0;
    while (s_ReadRecord(log_file))
        ++recs_count;
    s_TotalRecords.Set(recs_count);

    // Check for the errors
    if (!feof(log_file)) {
        SRV_LOG(Critical, "Cannot read records from " << file_name
                          << ". Invalid file?");

        // Error occurred, the file is broken:
        // Reset the counter, clean the log container, set the start number.
        s_LastWrittenRecord = start_log_rec_no;
        s_TotalRecords.Set(0);
        s_SyncedData.clear();
        s_Log.clear();

        fclose(log_file);
        return;
    }
    fclose(log_file);

    // Set the last written record number
    NON_CONST_ITERATE(TLog, slot, s_Log) {
        SSlotData& data = slot->second;
        data.rec_number = data.events.size();
        if (data.rec_number != 0) {
            Uint8 last_rec_no = data.events.back()->rec_no;
            if (last_rec_no > s_LastWrittenRecord)
                s_LastWrittenRecord = last_rec_no;
        }
    }
}

// This call is guaranteed to be done once in one thread only.
// So there is no need to bother about thread safety.
bool
CNCSyncLog::Finalize(void)
{
    const string& file_name = CNCDistributionConf::GetSyncLogFileName();
    FILE* log_file = fopen(file_name.c_str(), "w");

    if (!log_file)
        return false;

    // Avoid saving excessive data
    ITERATE(TLog, it, s_Log) {
        Clean(it->first);
    }

    // Save the last synced records
    if (!s_WriteHeader(log_file)) {
        fclose(log_file);
        return false;
    }

    // Save the log entries
    ITERATE(TLog, it_slot, s_Log) {
        const TSyncEvents& events = it_slot->second.events;
        ITERATE(TSyncEvents, item, events) {
            if (!s_WriteRecord(log_file, it_slot->first, (*item))) {
                fclose(log_file);
                return false;
            }
        }
    }
    fclose(log_file);
    return true;
}

Uint8
CNCSyncLog::AddEvent(Uint2 slot, SNCSyncEvent* event)
{
    Uint2 real_slot = 0;
    Uint2 time_bucket = 0;
    if (!CNCDistributionConf::GetSlotByKey(event->key,
            real_slot, time_bucket) || real_slot != slot)
        abort();

    SSlotData& data = s_GetSlotData(slot);
    CMiniMutexGuard guard(data.lock);

    event->local_time = CSrvTime::Current().AsUSec();
    s_GlobalLock.Lock();
    event->rec_no = ++s_LastWrittenRecord;
    s_GlobalLock.Unlock();
    // Avoid race condition:
    // - user blob comes
    // - event is written
    // - sync started in another thread while orig_rec_no is updated
    if (event->orig_server == CNCDistributionConf::GetSelfID())
        event->orig_rec_no = event->rec_no;

    data.events.push_back(event);
    ++data.rec_number;
    s_TotalRecords.Add(1);
    return event->rec_no;
}

void
CNCSyncLog::GetLastSyncedRecNo(Uint8  server,
                               Uint2  slot,
                               Uint8* local_synced_rec_no,
                               Uint8* remote_synced_rec_no)
{
    CMiniMutexGuard guard(s_GlobalLock);

    SSrvSyncedData& sync_data = s_SyncedData[server][slot];
    *local_synced_rec_no = sync_data.local_rec_no;
    *remote_synced_rec_no = sync_data.remote_rec_no;
}

void
CNCSyncLog::SetLastSyncRecNo(Uint8 server,
                             Uint2 slot,
                             Uint8 local_synced_rec_no,
                             Uint8 remote_synced_rec_no)
{
    CMiniMutexGuard guard(s_GlobalLock);

    SSrvSyncedData& sync_data = s_SyncedData[server][slot];
    sync_data.local_rec_no = local_synced_rec_no;
    sync_data.remote_rec_no = remote_synced_rec_no;
}

Uint8
CNCSyncLog::GetCurrentRecNo(Uint2 slot)
{
    SSlotData& data = s_GetSlotData(slot);
    CMiniMutexGuard guard(data.lock);
    if (!data.events.empty())
        return data.events.back()->rec_no;
    else
        return s_LastWrittenRecord;
}

Uint8
CNCSyncLog::GetLastRecNo(void)
{
    return s_LastWrittenRecord;
}

bool
CNCSyncLog::GetEventsList(Uint8  server,
                          Uint2  slot,
                          Uint8* local_start_rec_no,
                          Uint8* remote_start_rec_no,
                          TReducedSyncEvents* events)
{
    {{
        CMiniMutexGuard guard(s_GlobalLock);
        SSrvSyncedData& sync_data = s_SyncedData[server][slot];
        if (sync_data.local_rec_no > *local_start_rec_no)
            *local_start_rec_no = sync_data.local_rec_no;
        else
            sync_data.local_rec_no = *local_start_rec_no;
        if (sync_data.remote_rec_no > *remote_start_rec_no)
            *remote_start_rec_no = sync_data.remote_rec_no;
        else
            sync_data.remote_rec_no = *remote_start_rec_no;
    }}

    SSlotData& data = s_GetSlotData(slot);
    CMiniMutexGuard guard(data.lock);

    // Check the presence of the local record
    if (data.events.empty()
        ||  *local_start_rec_no < data.events.front()->rec_no
        ||  s_LastWrittenRecord < *local_start_rec_no)
    {
        return false;   // The required record is not available,
                        // all the blobs will be exchanged
    }

    //Uint8 time_limit = 0;
    //bool  time_limit_set = false;
    // Walk the container from the end with copying all the records which
    // matched: rec_no > local_rec_no or timestamp within 10 sec
    NON_CONST_REVERSE_ITERATE(TSyncEvents, record, data.events) {
        SNCSyncEvent* evt = *record;
        // local_time is used here because the records are ordered by
        // local_time but not the origin time
        //if (time_limit_set  &&  evt->local_time < time_limit)
        //    break;
        if (/*!time_limit_set  &&  */evt->rec_no < *local_start_rec_no) {
            //time_limit_set = true;
            //time_limit = evt->local_time
            //             - CNCDistributionConf::GetPeriodicSyncTailTime();
            break;
        }

        // Now make the decision if the record should be memorized
        //          | rm         | wr       | pr          <-- was saved
        // ---------------------------------------------
        // rm (any) | impossible | nothing  | impossible
        // wr       | nothing    | nothing  | memorize
        // pr       | nothing    | nothing  | nothing
        // ^- found
        SBlobEvent& blob_event = (*events)[evt->key];
        switch (evt->event_type) {
        case eSyncWrite:
            if (!blob_event.wr_or_rm_event)
                blob_event.wr_or_rm_event = evt;
            break;
        case eSyncProlong:
            if (!blob_event.wr_or_rm_event  &&  !blob_event.prolong_event)
                blob_event.prolong_event = evt;
            break;
        }
    }
    return true;
}


bool
CNCSyncLog::GetSyncOperations(Uint8 server,
                              Uint2 slot,
                              Uint8 local_start_rec_no,
                              Uint8 remote_start_rec_no,
                              const TReducedSyncEvents& remote_events,
                              TSyncEvents* events_to_get,
                              TSyncEvents* events_to_send,
                              Uint8* local_synced_rec_no,
                              Uint8* remote_synced_rec_no)
{
    // Get the local events
    TReducedSyncEvents local_events;
    if (!GetEventsList(server, slot, &local_start_rec_no,
                       &remote_start_rec_no, &local_events))
    {
        return false;
    }

    Uint8 now = CSrvTime::Current().AsUSec();
    s_CompareEvents(local_events, local_start_rec_no,
                    now, remote_events,
                    local_synced_rec_no, events_to_send);
    s_CompareEvents(remote_events, remote_start_rec_no,
                    now, local_events,
                    remote_synced_rec_no, events_to_get);
    return true;
}


Uint8
CNCSyncLog::Clean(Uint2 slot)
{
    SSlotData& data = s_GetSlotData(slot);
    CMiniMutexGuard guard(data.lock);

    Uint4 max_recs = CNCDistributionConf::GetMaxSlotLogEvents();
    Uint4 clean_to_recs = max_recs - CNCDistributionConf::GetCleanLogReserve();
    Uint4 max_clean_cnt = CNCDistributionConf::GetMaxCleanLogBatch();
    Uint4 cleaned_cnt = 0;
    Uint8 limit = s_GetMinLocalSyncedRecordNo(slot, data);
    while (cleaned_cnt < max_clean_cnt) {
        if (data.events.empty()  ||  data.events.front()->rec_no >= limit)
            break;       // Records are younger that should be deleted

        // Delete the record, it is not required any more
        delete data.events.front();
        data.events.pop_front();
        s_TotalRecords.Add(-1);
        --data.rec_number;
        ++cleaned_cnt;
    }
    if (data.rec_number > max_recs) {
        while (data.rec_number > clean_to_recs  &&  cleaned_cnt < max_clean_cnt)
        {
            delete data.events.front();
            data.events.pop_front();
            s_TotalRecords.Add(-1);
            --data.rec_number;
            ++cleaned_cnt;
        }
    }
    return cleaned_cnt;
}

Uint8
CNCSyncLog::GetLogSize(void)
{
    return s_TotalRecords.Get();
}

Uint8
CNCSyncLog::GetLogSize(Uint2 slot)
{
    SSlotData& data = s_GetSlotData(slot);
    return data.rec_number;
}

bool
CNCSyncLog::IsOverLimit(Uint2 slot)
{
    SSlotData& data = s_GetSlotData(slot);
    Uint4 max_recs = CNCDistributionConf::GetMaxSlotLogEvents();
    return data.rec_number > max_recs;
}

END_NCBI_SCOPE

