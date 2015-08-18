#ifndef NETSCHEDULE_DB__HPP
#define NETSCHEDULE_DB__HPP

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
 * File Description:
 *   NetSchedule database structure.
 *
 */


#include <corelib/ncbicntr.hpp>

#include <util/bitset/ncbi_bitset.hpp>

#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_bv_store.hpp>

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

const unsigned kNetScheduleSplitSize       = 96;  // 64 -> 96 See CXX-3449
const unsigned kMaxClientIpSize            = 48;  // 64 -> 48 See CXX-3449
const unsigned kMaxSessionIdSize           = 48;  // 64 -> 48 See CXX-3449
const unsigned kMaxHitIdSize               = 64;
const unsigned kMaxDescriptionSize         = 16 * 1024;
const unsigned kMaxQueueNameSize           = 64;
const unsigned kMaxQueueLimitsSize         = 16 * 1024;
const unsigned kMaxWorkerNodeIdSize        = 64;
const unsigned kNetScheduleMaxOverflowSize = 1024 * 1024;
const unsigned kLinkedSectionsList         = 8192;
const unsigned kLinkedSectionValueNameSize = 8192;
const unsigned kLinkedSectionValueSize     = 8192;


// BDB table to store frequently used job info
struct SJobDB : public CBDB_File
{
    CBDB_FieldUint4        id;                  // Job id

    CBDB_FieldUint4        passport;            // Passport - generated integer
    CBDB_FieldInt4         status;              // Current job status
    CBDB_FieldDouble       timeout;             // Individual timeout
    CBDB_FieldDouble       run_timeout;         // Job run timeout
    CBDB_FieldDouble       read_timeout;

    CBDB_FieldUint4        subm_notif_port;     // notification port
    CBDB_FieldDouble       subm_notif_timeout;  // notification timeout

    CBDB_FieldUint4        listener_notif_addr;
    CBDB_FieldUint4        listener_notif_port;
    CBDB_FieldDouble       listener_notif_abstime;

    // This field shows the number of attempts from submission or last
    // reschedule, so the number of actual attempts in SEventsDB can be more
    // than this number
    CBDB_FieldUint4        run_counter;     // Number of execution attempts
    CBDB_FieldUint4        read_counter;    // Number if reading attempts

    /// Affinity token id (refers to the affinity dictionary DB)
    CBDB_FieldUint4        aff_id;
    CBDB_FieldUint4        mask;

    // Jobs group support
    CBDB_FieldUint4        group_id;        // group ID. If 0 => not in a group.
    CBDB_FieldDouble       last_touch;      // last access time_t
    CBDB_FieldChar         input_overflow;  // Is input in JobInfo table
    CBDB_FieldChar         output_overflow; // Is output in JobInfo table
    CBDB_FieldLString      input;           // Input data
    CBDB_FieldLString      output;          // Result data

    CBDB_FieldLString      client_ip;       // IP address came from CGI client
    CBDB_FieldLString      client_sid;      // CGI session ID
    CBDB_FieldLString      ncbi_phid;       // CGI page hit ID
    CBDB_FieldLString      progress_msg;    // Progress report message

    SJobDB()
    {
        DisableNull();

        BindKey("id",                       &id);

        BindData("passport",                &passport);
        BindData("status",                  &status);
        BindData("timeout",                 &timeout);
        BindData("run_timeout",             &run_timeout);
        BindData("read_timeout",            &read_timeout);

        BindData("subm_notif_port",         &subm_notif_port);
        BindData("subm_notif_timeout",      &subm_notif_timeout);

        BindData("listener_notif_addr",     &listener_notif_addr);
        BindData("listener_notif_port",     &listener_notif_port);
        BindData("listener_notif_abstime",  &listener_notif_abstime);

        BindData("run_counter",     &run_counter);
        BindData("read_counter",    &read_counter);
        BindData("aff_id",          &aff_id);
        BindData("mask",            &mask);
        BindData("group_id",        &group_id);
        BindData("last_touch",      &last_touch);
        BindData("input_overflow",  &input_overflow);
        BindData("output_overflow", &output_overflow);
        BindData("input",           &input,          kNetScheduleSplitSize);
        BindData("output",          &output,         kNetScheduleSplitSize);
        BindData("client_ip",       &client_ip,      kMaxClientIpSize);
        BindData("client_sid",      &client_sid,     kMaxSessionIdSize);
        BindData("ncbi_phid",       &ncbi_phid,      kMaxHitIdSize);
        BindData("progress_msg",    &progress_msg,   kNetScheduleMaxDBDataSize);
    }
};


// BDB table to store infrequently needed job info
struct SJobInfoDB : public CBDB_File
{
    CBDB_FieldUint4        id;              // Job id
    CBDB_FieldLString      input;           // Job input overflow
    CBDB_FieldLString      output;          // Job output overflow

    SJobInfoDB()
    {
        BindKey("id", &id);
        BindData("input",  &input,  kNetScheduleMaxOverflowSize);
        BindData("output", &output, kNetScheduleMaxOverflowSize);
    }
};


// BDB table to store events information
// Every instantiation of a job is reflected in this table under
// corresponding (id, event) key. In particular, this table stores
// ALL run attempts, so if the job was rescheduled, the number of
// actual attempts can be more than run_count.
struct SEventsDB : public CBDB_File
{
    CBDB_FieldUint4   id;               // Job id
    CBDB_FieldUint4   event_id;         // Job event id
    CBDB_FieldUint4   event;            // Event which caused the record
    CBDB_FieldInt4    status;           // Status to which the job moved to
                                        // after processing the event
    CBDB_FieldDouble  timestamp;        // The event timestamp
    CBDB_FieldUint4   node_addr;        // IP of the worker node
                                        //   (net byte order)
    CBDB_FieldInt4    ret_code;         // Return code
    CBDB_FieldLString client_node;      // client node identifier
    CBDB_FieldLString client_session;   // client node session
    CBDB_FieldLString err_msg;          // Error message (exception::what())

    SEventsDB()
    {
        BindKey("id",              &id);
        BindKey("event_id",        &event_id);
        BindData("event",          &event);
        BindData("status",         &status);
        BindData("timestamp",      &timestamp);
        BindData("node_addr",      &node_addr);
        BindData("ret_code",       &ret_code);
        BindData("client_node",    &client_node, kMaxWorkerNodeIdSize);
        BindData("client_session", &client_session, kMaxWorkerNodeIdSize);
        BindData("err_msg",        &err_msg, kNetScheduleMaxDBErrSize);
    }
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_DB__HPP */

