#ifndef NETSCHEDULE_NS_TYPES__HPP
#define NETSCHEDULE_NS_TYPES__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule common types and various string constants
 *
 */


#include <string>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/ncbi_bitset_alloc.hpp>
#include <util/bitset/bmalgo.h>

#include <connect/services/netschedule_api.hpp>

#include "ns_queue_parameters.hpp"


BEGIN_NCBI_SCOPE

// Distinguishes get and read for various situations:
// - notifications
// - affinities
// etc
enum ECommandGroup {
    eGet,               // Comes from a worker node
    eRead,              // Comes from a reader
    eUndefined          // Comes from neither a wn nor a reader
};


// Submitter/listener notification reasons
enum ENotificationReason {
    eStatusChanged,             // Job status has been changed
    eNotificationStolen,        // Listener notification has been stolen
    eProgressMessageChanged,    // The progress message has been changed
    eJobDeleted                 // The job has been deleted from the DB
};


typedef bm::bvector<>                           TNSBitVector;
typedef CNetScheduleAPI::EJobStatus             TJobStatus;

// Holds all the queue parameters - used for queue classes
// and for reading from DB and ini files
typedef map<string, SQueueParameters, PNocase>  TQueueParams;



const string    kDumpSubdirName("dump");
const string    kDumpReservedSpaceFileName("space_keeper.dat");
const string    kQClassDescriptionFileName("qclass_descr.dump");
const string    kLinkedSectionsFileName("linked_sections.dump");
const string    kJobsFileName("jobs.dump");
const string    kDBStorageVersionFileName("DB_STORAGE_VER");
const string    kStartJobIDsFileName("STARTJOBIDS");
const string    kNodeIDFileName("NODE_ID");
const string    kCrashFlagFileName("CRASH_FLAG");
const string    kDumpErrorFlagFileName("DUMP_ERROR_FLAG");
const string    kPausedQueuesFilesName("PAUSED_QUEUES");
const string    kRefuseSubmitFileName("REFUSE_SUBMIT");
const size_t    kDumpReservedSpaceFileBuffer = 1024 * 1024;

static string   kNewLine("\n");


// Various hex viewers show this magic in a different way.
// Some swap first two bytes with the last two bytes
// Some reverse the signature byte by byte. So the magic is selected to be
// visible the same way everywhere.
// See kOldDumpMagic as well in ns_db_dump.cpp
const Uint4     kDumpMagic(0xF0F0F0F0);


// An empty bit vector is returned in quite a few places
const TNSBitVector  kEmptyBitVector;

// A few limits
const unsigned kMaxQueueNameSize           = 64;
const unsigned kMaxHitIdSize               = 128; // 64 -> 128 See CXX-11914
const unsigned kMaxClientIpSize            = 48;  // 64 -> 48 See CXX-3449
const unsigned kMaxSessionIdSize           = 48;  // 64 -> 48 See CXX-3449
const unsigned kLinkedSectionValueNameSize = 8192;
const unsigned kLinkedSectionValueSize     = 8192;
const unsigned kLinkedSectionsList         = 8192;
const unsigned kMaxQueueLimitsSize         = 16 * 1024;
const unsigned kMaxDescriptionSize         = 16 * 1024;
const unsigned kMaxWorkerNodeIdSize        = 64;
const unsigned kNetScheduleMaxOverflowSize = 1024 * 1024;


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_TYPES__HPP */

