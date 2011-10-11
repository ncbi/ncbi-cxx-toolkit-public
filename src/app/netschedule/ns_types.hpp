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
 *   NetSchedule common types
 *
 */

/// @file ns_types.hpp
/// NetSchedule common types.
///
/// This file defines datatypes, common for internal working of NetSchedule
///
/// @internal

#include <string>

#include <util/bitset/bmalgo.h>
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/ncbi_bitset_alloc.hpp>

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

typedef CBV_PoolBlockAlloc<bm::block_allocator, CFastMutex> TBlockAlloc;
typedef bm::mem_alloc<TBlockAlloc, bm::ptr_allocator>       TMemAlloc;
typedef bm::bvector<TMemAlloc>                              TNSBitVector;
//typedef bm::bvector<>                                       TNSBitVector;

typedef CNetScheduleAPI::EJobStatus                         TJobStatus;

END_NCBI_SCOPE

#endif /* NETSCHEDULE_NS_TYPES__HPP */

