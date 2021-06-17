#ifndef NETCACHE__SYNC_LOG__HPP
#define NETCACHE__SYNC_LOG__HPP

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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs replication.
 *
 */




BEGIN_NCBI_SCOPE


/// Event types to log
enum ENCSyncEvent
{
    eSyncWrite,     //< Blob write event
    eSyncProlong,   //< Blob life time prolongation event
    eSyncUpdate,    //< blob changed notification
    eSyncRemove,    //< blob removal request
};

/// Single event record
struct SNCSyncEvent
{
    Uint8  rec_no;          //< Local event sequential number.
    Uint8  blob_size;       //< blob size
    CNCBlobKeyLight key;    //< Blob key.
    ENCSyncEvent event_type;//< Event type (write, remove, prolong).
    Uint8  orig_time;       //< Timestamp of the event when
                            //< it originated by client.
    Uint8  orig_server;     //< The server where event has
                            //< been originated.
    Uint8  orig_rec_no;     //< Record number on the host where the
                            //< event was originated.
    Uint8  local_time;      //< Timestamp when the record was
                            //< recorded locally.

    SNCSyncEvent(void)
      : rec_no(0), blob_size(0), event_type(eSyncWrite), orig_time(0), orig_server(0), orig_rec_no(0), local_time(0)
    {
    }
    bool isOlder(const SNCSyncEvent& other) const
    {
        if (orig_time != other.orig_time)
            return orig_time < other.orig_time;

        // Timestamp matched, is that were on the same host?
        if (orig_server == other.orig_server)
            return orig_rec_no < other.orig_rec_no;

        if (event_type == eSyncWrite  &&  other.event_type != eSyncWrite)
            return false;
        if (other.event_type == eSyncWrite  &&  event_type != eSyncWrite)
            return true;

        // No way to detect, return true
        return orig_server < other.orig_server;
    }
};


// Events container - local time ordered
typedef list<SNCSyncEvent*>    TSyncEvents;

// Reduced events container for fast search
struct SBlobEvent
{
    // One of the fields is always filled
    SNCSyncEvent* wr_or_rm_event;
    SNCSyncEvent* prolong_event;

    SBlobEvent() :
        wr_or_rm_event(NULL), prolong_event(NULL)
    {}

    Uint8 getMaxRecNoWithinTimeLimit(Uint8 limit) const
    {
        if (prolong_event != NULL  &&  prolong_event->local_time < limit)
            return prolong_event->rec_no;
        if (wr_or_rm_event != NULL  &&  wr_or_rm_event->local_time < limit)
            return wr_or_rm_event->rec_no;
        return 0;
    }
};
typedef map<string, SBlobEvent>   TReducedSyncEvents;


class CNCSyncLog
{
public:
    static void Initialize(bool need_read_saved, Uint8 start_log_rec_no);

    // Save the log records to the file_name
    // Return: true if written successfully
    // It does not free the memory occupied by records
    // because it is called when the process exits.
    static bool Finalize(void);

    // The memory ownership is transferred to CNCSyncLog.
    // CNCSynclog fills id and the local_time fields.
    // The id field value is returned.
    // Must be thread safe.
    static Uint8 AddEvent(Uint2 slot, SNCSyncEvent* event);

    // Provides the last successful synchronized record numbers
    // for the given server.
    // If the server is unknown 0s are provided.
    // local_synced_rec_id is filled with the last record number till
    // which the remote server is synchronized
    // remote_synced_rec_id is filled with the last record number till
    // which the local server is synchronized
    static void GetLastSyncedRecNo(Uint8  server,
                                   Uint2  slot,
                                   Uint8* local_synced_rec_no,
                                   Uint8* remote_synced_rec_no);

    // Saves the last synchronized record ids for the given server.
    static void SetLastSyncRecNo(Uint8 server,
                                 Uint2 slot,
                                 Uint8 local_synced_rec_no,
                                 Uint8 remote_synced_rec_no);

    // Provides the local last created sync log
    static Uint8 GetCurrentRecNo(Uint2 slot);
    static Uint8 GetLastRecNo(void);

    // Provides the list of events which occurred for
    // the given slot starting from local_start_rec_id - kSyncInterval.
    // If the values of local_start_rec_id and remote_start_rec_id
    // arguments do not match what is saved for the given server,
    // then they are updated as the max value of the given and the
    // stored before building the list of events. (The saved values are
    // updated correspondingly).
    // Fills the events container with matched events and
    // returns false if the start_record_id is not available any more.
    // The consequent writes and removes must be compressed.
    // Must be thread safe.
    static bool GetEventsList(Uint8  server,
                              Uint2  slot,
                              Uint8* local_start_rec_no,
                              Uint8* remote_start_rec_no,
                              TReducedSyncEvents* events );

    // Calculates the lists of operations for synchronization.
    // The given start record ids are stored and basing on them
    // the intervals for analysis are identified in the local log and
    // in the given remote events log.
    // The local interval is compressed because a compressed one is
    // received from the remote host.
    // Then the local events range to analyze is defined as:
    // [local_start_rec_id, now - kSyncInterval]. If the interval is
    // negative then there are no sync operations.
    // The remote interval for analysis is defined as:
    // [remote_start_rec_id, now - kSyncInterval].
    // The local_synced_rec_id and remote_synced_rec_id are filled with
    // the end of the corresponding analysis intervals.
    // The return value is false if the given local_start_rec_id is not
    // available any more. (there are no operations in this case)
    static bool GetSyncOperations(Uint8 server,
                                  Uint2 slot,
                                  Uint8 local_start_rec_no,
                                  Uint8 remote_start_rec_no,
                                  const TReducedSyncEvents& remote_events,
                                  TSyncEvents* events_to_get,
                                  TSyncEvents* events_to_send,
                                  Uint8* local_synced_rec_no,
                                  Uint8* remote_synced_rec_no );

    static Uint8 Clean(Uint2 slot);

    // Provides the total number of records in the log
    static Uint8 GetLogSize(void);
    static Uint8 GetLogSize(Uint2 slot);

    static bool IsOverLimit(Uint2 slot);
};

END_NCBI_SCOPE


#endif /* NETCACHE__SYNC_LOG__HPP */

