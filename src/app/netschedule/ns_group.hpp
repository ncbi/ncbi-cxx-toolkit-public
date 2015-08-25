#ifndef NETSCHEDULE_GROUPS__HPP
#define NETSCHEDULE_GROUPS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   Net schedule job groups
 *
 */

/// @file ns_group.hpp
/// NetSchedule job groups.
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE

class CQueue;


// Forward declaration of a Berkley DB table structure
struct SGroupDictDB;


// The group token is used twice so to avoid storing it twice a heap
// allocated string is used. So here is a compare functor.
struct SNSGroupTokenCompare
{
    bool operator () (const string * const &  lhs,
                      const string * const &  rhs) const
    {
        return (*lhs) < (*rhs);
    }
};


// Group associated data
struct SNSGroupJobs
{
    const string *  m_GroupToken;   // Group string identifier
    unsigned int    m_GroupId;      // Group integer identifier
    TNSBitVector    m_Jobs;         // Jobs in the group

    SNSGroupJobs();
    bool CanBeDeleted(void) const;
};


// Provides storage, search and other high level groups operations
class CNSGroupsRegistry
{
    public:
        CNSGroupsRegistry();
        ~CNSGroupsRegistry();

        size_t        size(void) const;

        TNSBitVector  GetJobs(const string &  group,
                              bool  allow_exception = true) const;
        TNSBitVector  GetJobs(const TNSBitVector &  group_ids) const;
        TNSBitVector  GetRegisteredGroups(void) const;
        unsigned int  ResolveGroup(const string &  group);
        string        ResolveGroup(unsigned int  group) const;
        vector<unsigned int>  ResolveGroups(const list< string > &  tokens);
        unsigned int  AddJobs(unsigned int    group_id,
                              unsigned int    first_job_id,
                              unsigned int    count);
        unsigned int  AddJob(const string &  group,
                             unsigned int    job_id);
        void  AddJob(unsigned int    group_id,
                     unsigned int    job_id);
        void  RemoveJob(unsigned int  group_id,
                        unsigned int  job_id);
        string  Print(const CQueue *  queue,
                      size_t          batch_size,
                      bool            verbose) const;

        void  Clear(void);

        unsigned int  CollectGarbage(unsigned int  max_to_del);
        void Dump(const string &  dump_dir_name,
                  const string &  queue_name) const;
        void RemoveDump(const string &  dump_dir_name,
                        const string &  queue_name) const;

        // Used to load the groups and register loaded jobs.
        // The loading procedure has 3 steps:
        // 1. Load the dictionary from the flat file
        // 2. For each loaded job -> call AddJobToGroup()
        // 3. Call FinalizeGroupDictionaryLoading()
        // These functions should not be used for anything but loading DB.
        void  LoadFromDump(const string &  dump_dir_name,
                           const string &  queue_name);
        void  AddJobToGroup(unsigned int  group_id, unsigned int  job_id);
        void  FinalizeGroupDictionaryLoading(void);

    private:

        typedef map< unsigned int,
                     SNSGroupJobs * >           TGroupIDToAttrMap;
        typedef map< const string *,
                     SNSGroupJobs *,
                     SNSGroupTokenCompare >     TGroupTokenToAttrMap;

    private:
        TGroupIDToAttrMap       m_IDToAttr;
        TGroupTokenToAttrMap    m_TokenToAttr;

        mutable CMutex          m_Lock;         // Lock for the operations

    private:
        unsigned int            m_LastGroupID;
        CFastMutex              m_LastGroupIDLock;
        unsigned int            x_GetNextGroupID(void);
        void                    x_InitLastGroupID(unsigned int  value);

        TNSBitVector            m_RegisteredGroups;
                                                // The identifiers of all the
                                                // groups which are
                                                // currently in the registry
        TNSBitVector            m_RemoveCandidates;

        unsigned int  x_CreateGroup(const string &  group);
        string  x_PrintSelected(const TNSBitVector &    batch,
                                const CQueue *          queue,
                                bool                    verbose) const;
        string  x_PrintOne(const SNSGroupJobs &     group_attr,
                           const CQueue *           queue,
                           bool                     verbose) const;
        void  x_DeleteSingleInMemory(TGroupIDToAttrMap::iterator  to_del);
        string x_GetDumpFileName(const string &  dump_dir_name,
                                 const string &  qname) const;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_GROUPS__HPP */

