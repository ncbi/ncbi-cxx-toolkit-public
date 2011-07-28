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

/// @file ns_db.hpp
/// NetSchedule database structure.
///
/// This file collects all BDB data files and index files used in netschedule
///
/// @internal

#include <corelib/ncbicntr.hpp>

#include <util/bitset/ncbi_bitset.hpp>

#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_bv_store.hpp>

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

const unsigned kNetScheduleSplitSize = 64;
const unsigned kMaxClientIpSize      = 64;
const unsigned kMaxSessionIdSize     = 64;

/// BDB table to store queue
///
/// @internal
///
struct SQueueDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id

    CBDB_FieldInt4         status;          ///< Current job status
    CBDB_FieldUint4        time_submit;     ///< Job submit time
    CBDB_FieldUint4        timeout;         ///<     individual timeout
    CBDB_FieldUint4        run_timeout;     ///<     job run timeout

    CBDB_FieldUint4        subm_addr;       ///< netw BO (for notification)
    CBDB_FieldUint4        subm_port;       ///< notification port
    CBDB_FieldUint4        subm_timeout;    ///< notification timeout

    // This field shows the number of attempts from submission or last
    // reschedule, so the number of actual attempts in SEventsDB can be more
    // than this number
    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldUint4        read_counter;    ///< Number if reading attempts

    // When job is in Reading state, its read group id is here.
    CBDB_FieldUint4        read_group;

    /// Affinity token id (refers to the affinity dictionary DB)
    CBDB_FieldUint4        aff_id;
    CBDB_FieldUint4        mask;

    CBDB_FieldChar         input_overflow;  ///< Is input in JobInfo table
    CBDB_FieldChar         output_overflow; ///< Is output in JobInfo table
    CBDB_FieldLString      input;           ///< Input data
    CBDB_FieldLString      output;          ///< Result data

    CBDB_FieldLString      client_ip;       ///< IP address came from CGI client
    CBDB_FieldLString      client_sid;      ///< CGI session ID
    CBDB_FieldLString      progress_msg;    ///< Progress report message

    SQueueDB()
    {
        DisableNull();

        BindKey("id", &id);

        BindData("status",       &status);
        BindData("time_submit",  &time_submit);
        BindData("timeout",      &timeout);
        BindData("run_timeout",  &run_timeout);

        BindData("subm_addr",    &subm_addr);
        BindData("subm_port",    &subm_port);
        BindData("subm_timeout", &subm_timeout);

        BindData("run_counter",  &run_counter);
        BindData("read_counter", &read_counter);
        BindData("read_group",   &read_group);

        BindData("aff_id",       &aff_id);
        BindData("mask",         &mask);


        BindData("input_overflow",  &input_overflow);
        BindData("output_overflow", &output_overflow);
        BindData("input",           &input,          kNetScheduleSplitSize);
        BindData("output",          &output,         kNetScheduleSplitSize);
        BindData("client_ip",       &client_ip,      kMaxClientIpSize);
        BindData("client_sid",      &client_sid,     kMaxSessionIdSize);
        BindData("progress_msg",    &progress_msg,   kNetScheduleMaxDBDataSize);
    }
};


const unsigned kNetScheduleMaxOverflowSize = 1024*1024;
/// BDB table to store infrequently needed job info
///
/// @internal
///
struct SJobInfoDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id
    CBDB_FieldLString      tags;            ///< Tags for the job
    CBDB_FieldLString      input;           ///< Job input overflow
    CBDB_FieldLString      output;          ///< Job output overflow

    SJobInfoDB()
    {
        BindKey("id", &id);
        BindData("tags",   &tags,   kNetScheduleMaxOverflowSize);
        BindData("input",  &input,  kNetScheduleMaxOverflowSize);
        BindData("output", &output, kNetScheduleMaxOverflowSize);
    }
};


const unsigned kMaxWorkerNodeIdSize = 64;
/// BDB table to store events information
/// Every instantiation of a job is reflected in this table under
/// corresponding (id, event) key. In particular, this table stores
/// ALL run attempts, so if the job was rescheduled, the number of
/// actual attempts can be more than run_count.
struct SEventsDB : public CBDB_File
{
    CBDB_FieldUint4   id;           ///< Job id
    CBDB_FieldUint4   event;        ///< Job event
    CBDB_FieldInt4    status;       ///< Job status for this event
    CBDB_FieldUint4   timestamp;    ///< The event timestamp
    CBDB_FieldUint4   node_addr;    ///< IP of the worker node (net byte order)
    CBDB_FieldUint2   node_port;    ///< Node's port
    CBDB_FieldInt4    ret_code;     ///< Return code
    CBDB_FieldLString node_id;      ///< worker node id
    CBDB_FieldLString err_msg;      ///< Error message (exception::what())

    SEventsDB()
    {
        BindKey("id",           &id);
        BindKey("event",        &event);
        BindData("status",      &status);
        BindData("timestamp",   &timestamp);
        BindData("node_addr",   &node_addr);
        BindData("node_port",   &node_port);
        BindData("ret_code",    &ret_code);
        BindData("node_id",     &node_id, kMaxWorkerNodeIdSize);
        BindData("err_msg",     &err_msg, kNetScheduleMaxDBErrSize);
    }
};


/// Database of vectors of jobs to be deleted. Because deletion
/// occurs in background, with different pace for different tables,
/// and even in different manner (blocks of jobs from main and aux job
/// tables, and whole vectors from several records from affinity and tag
/// tables), we need to maintain several bit vectors with deleted jobs.
///
/// @internal
///
struct SDeletedJobsDB : public CBDB_BvStore<TNSBitVector>
{
    ///< Vector ID, 0 - job table, 1 - tag table, 2 - affinities
    CBDB_FieldUint4 id;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    SDeletedJobsDB()
    {
        DisableNull();
        BindKey("id", &id);
    }
};

/// Index of queue database (affinity to jobs)
///
/// @internal
///
struct SAffinityIdx : public CBDB_BvStore<TNSBitVector>
{
    CBDB_FieldUint4 aff_id;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    SAffinityIdx()
    {
        DisableNull();
        BindKey("aff_id", &aff_id);
    }
};


/// BDB table to store affinity
///
/// @internal
///
struct SAffinityDictDB : public CBDB_File
{
    CBDB_FieldUint4   aff_id;        ///< Affinity token id
    CBDB_FieldLString token;         ///< Affinity token

    SAffinityDictDB()
    {
        DisableNull();
        BindKey("aff_id", &aff_id);
        BindData("token", &token, kNetScheduleMaxDBDataSize);
    }
};

/// BDB affinity token index
///
/// @internal
///
struct SAffinityDictTokenIdx : public CBDB_File
{
    CBDB_FieldLString      token;
    CBDB_FieldUint4        aff_id;

    SAffinityDictTokenIdx()
    {
        DisableNull();
        BindKey("token",   &token, kNetScheduleMaxDBDataSize);
        BindData("aff_id", &aff_id);
    }
};


/// BDB tag storage
///
/// @internal
///
struct STagDB : public CBDB_BvStore<TNSBitVector>
{
    CBDB_FieldLString key;
    CBDB_FieldLString val;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    STagDB()
    {
        DisableNull();
        BindKey("key", &key);
        BindKey("val", &val);
    }
};


/// BDB table for storing queue descriptions
///
/// @internal
///
struct SQueueDescriptionDB : public CBDB_File
{
    CBDB_FieldLString queue;
    CBDB_FieldUint4   kind; // static - 0 or dynamic - 1
    CBDB_FieldUint4   pos;
    CBDB_FieldLString qclass;
    CBDB_FieldLString comment;
    SQueueDescriptionDB()
    {
        DisableNull();
        BindKey("queue",      &queue);
        BindData("kind",      &kind);
        BindData("pos",       &pos);
        BindData("qclass",    &qclass);
        BindData("comment",   &comment);
    }
};


struct SStartCounterDB : public CBDB_File
{
    CBDB_FieldUint4     pseudo_key;
    CBDB_FieldUint4     start_from;

    SStartCounterDB()
    {
        DisableNull();
        BindKey("pseudo_key", &pseudo_key);
        BindData("start_from", &start_from);
    }
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_DB__HPP */
