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

#include <util/bitset/bmalgo.h>
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/ncbi_bitset_alloc.hpp>

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


typedef CBV_PoolBlockAlloc<bm::block_allocator, CFastMutex> TBlockAlloc;
typedef bm::mem_alloc<TBlockAlloc, bm::ptr_allocator>       TMemAlloc;
typedef bm::bvector<TMemAlloc>                              TNSBitVector;

typedef CNetScheduleAPI::EJobStatus                         TJobStatus;

// Holds all the queue parameters - used for queue classes
// and for reading from DB and ini files
typedef map<string, SQueueParameters>                       TQueueParams;



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
const size_t    kDumpReservedSpaceFileBuffer = 1024 * 1024;

// Various hex viewers show this magic in a different way.
// Some swap first two bytes with the last two bytes
// Some reverse the signature byte by byte. So the magic is selected to be
// visible the same way everywhere.
const Int4      kDumpMagic(0xD0D0D0D0);


END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_TYPES__HPP */

