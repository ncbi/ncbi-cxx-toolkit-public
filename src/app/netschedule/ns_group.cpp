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

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbistd.hpp>

#include "ns_group.hpp"
#include "ns_db.hpp"
#include "ns_queue.hpp"


BEGIN_NCBI_SCOPE





// Group associated data
SNSGroupJobs::SNSGroupJobs() :
    m_GroupToken(NULL),
    m_GroupId(0),
    m_Jobs(bm::BM_GAP)
{}


bool SNSGroupJobs::CanBeDeleted(void) const
{
    return !m_Jobs.any();
}





CNSGroupsRegistry::CNSGroupsRegistry() :
    m_GroupDictDB(NULL),
    m_LastGroupID(0)
{}


CNSGroupsRegistry::~CNSGroupsRegistry()
{
    Detach();
    x_Clear();
    return;
}


void  CNSGroupsRegistry::Attach(SGroupDictDB *  group_dict_db)
{
    m_GroupDictDB = group_dict_db;
    return;
}


void  CNSGroupsRegistry::Detach(void)
{
    m_GroupDictDB = NULL;
    return;
}


void  CNSGroupsRegistry::x_InitLastGroupID(unsigned int  value)
{
    CFastMutexGuard     guard(m_LastGroupIDLock);
    m_LastGroupID = value;
    return;
}


size_t        CNSGroupsRegistry::size(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_IDToAttr.size();
}


TNSBitVector  CNSGroupsRegistry::GetJobs(const string &  group) const
{
    CMutexGuard                             guard(m_Lock);
    TGroupTokenToAttrMap::const_iterator    found = m_TokenToAttr.find(&group);

    if (found == m_TokenToAttr.end())
        NCBI_THROW(CNetScheduleException, eGroupNotFound,
                   "Group " + group + " is unknown");

    return found->second->m_Jobs;
}


TNSBitVector  CNSGroupsRegistry::GetRegisteredGroups(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_RegisteredGroups;
}


// Provides the group integer identifier.
// If the group does not exist then a new one, with no jobs in it is created.
// Requires a DB transaction in the outer scope
unsigned int  CNSGroupsRegistry::ResolveGroup(const string &  group)
{
    if (group.empty())
        return 0;

    CMutexGuard                             guard(m_Lock);
    TGroupTokenToAttrMap::const_iterator    found = m_TokenToAttr.find(&group);

    if (found != m_TokenToAttr.end())
        return found->second->m_GroupId;

    // The group has not been found. Create a new one.
    return x_CreateGroup(group);
}


string  CNSGroupsRegistry::ResolveGroup(unsigned int  group_id) const
{
    CMutexGuard                         guard(m_Lock);
    TGroupIDToAttrMap::const_iterator   found = m_IDToAttr.find(group_id);

    if (found == m_IDToAttr.end())
        NCBI_THROW(CNetScheduleException, eGroupNotFound,
                   "Group with id " + NStr::NumericToString(group_id) +
                   " is unknown");

    return *found->second->m_GroupToken;
}


// DB transaction is not required
unsigned int  CNSGroupsRegistry::AddJobs(unsigned int    group_id,
                                         unsigned int    first_job_id,
                                         unsigned int    count)
{
    if (first_job_id == 0 || count == 0 || group_id == 0)
        return 0;

    CMutexGuard                     guard(m_Lock);
    TGroupIDToAttrMap::iterator     found = m_IDToAttr.find(group_id);

    if (found != m_IDToAttr.end()) {
        found->second->m_Jobs.set_range(first_job_id, first_job_id + count - 1);
        return found->second->m_GroupId;
    }

    // The group does not exist. It is impossible to create it because
    // the group name is unknown here.
    NCBI_THROW(CNetScheduleException, eGroupNotFound,
               "Group with id " + NStr::NumericToString(group_id) + " is unknown");
}


// Requires a DB transaction in the outer scope
unsigned int  CNSGroupsRegistry::AddJob(const string &  group,
                                        unsigned int    job_id)
{
    if (job_id == 0 || group.empty())
        return 0;

    CMutexGuard                             guard(m_Lock);
    TGroupTokenToAttrMap::const_iterator    found = m_TokenToAttr.find(&group);

    if (found != m_TokenToAttr.end()) {
        // Group exists
        found->second->m_Jobs.set_bit(job_id, true);
        return found->second->m_GroupId;
    }

    // New group, need to create a record
    unsigned int    group_id = x_CreateGroup(group);
    m_IDToAttr[group_id]->m_Jobs.set_bit(job_id, true);
    return group_id;
}


// No DB transaction is required
void  CNSGroupsRegistry::AddJob(unsigned int    group_id,
                                unsigned int    job_id)
{
    if (group_id == 0 || job_id == 0)
        return;

    CMutexGuard                     guard(m_Lock);
    TGroupIDToAttrMap::iterator     found = m_IDToAttr.find(group_id);

    if (found != m_IDToAttr.end()) {
        found->second->m_Jobs.set_bit(job_id, true);
        return;
    }

    // The group does not exist. It is impossible to create it because
    // the group name is unknown here.
    NCBI_THROW(CNetScheduleException, eGroupNotFound,
               "Group with id " + NStr::NumericToString(group_id) + " is unknown");
}


// No DB transaction is required
void  CNSGroupsRegistry::RemoveJob(unsigned int  group_id,
                                   unsigned int  job_id)
{
    if (group_id == 0 || job_id == 0)
        return;

    CMutexGuard                     guard(m_Lock);
    TGroupIDToAttrMap::iterator     found = m_IDToAttr.find(group_id);

    if (found != m_IDToAttr.end()) {
        found->second->m_Jobs.set_bit(job_id, false);

        if (found->second->CanBeDeleted())
            // No more jobs in the group. Delete it.
            x_DeleteSingleInMemory(found);
        return;
    }

    // The group does not exist. It is impossible to create it because
    // the group name is unknown here.
    NCBI_THROW(CNetScheduleException, eGroupNotFound,
               "Group with id " + NStr::NumericToString(group_id) + " is unknown");
}


// No DB transaction required
void  CNSGroupsRegistry::ClearMemoryAndDatabase(void)
{
    // Clear the data structures in memory
    x_Clear();

    // Clear the Berkley DB table.
    // Safe truncate can delete everything without a transaction.
    m_GroupDictDB->SafeTruncate();
    return;
}


string  CNSGroupsRegistry::Print(const CQueue *  queue,
                                 size_t          batch_size,
                                 bool            verbose) const
{
    string              result;
    TNSBitVector        batch;

    TNSBitVector                registered_groups = GetRegisteredGroups();
    TNSBitVector::enumerator    en(registered_groups.first());

    while (en.valid()) {
        batch.set_bit(*en);
        ++en;

        if (batch.count() >= batch_size) {
            result += x_PrintSelected(batch, queue, verbose);
            batch.clear();
        }
    }

    if (batch.count() > 0)
        result += x_PrintSelected(batch, queue, verbose);
    return result;
}


// Reads the DB and fills in two maps. The jobs list for a certain group is
// empty at this stage.
// A set of AddJob(...) calls is expected after loading the
// dictionary.
// The final step in the loading groups is to get the
// FinalizeGroupDictionaryLoading() call.
void  CNSGroupsRegistry::LoadGroupDictionary(void)
{
    CMutexGuard         guard(m_Lock);
    CBDB_FileCursor     cur(*m_GroupDictDB);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;
    for (; cur.Fetch() == eBDB_Ok;) {
        unsigned int        group_id = m_GroupDictDB->group_id;
        string *            new_token = new string(m_GroupDictDB->token);
        SNSGroupJobs *      new_record = new SNSGroupJobs;

        new_record->m_GroupToken = new_token;
        new_record->m_GroupId = group_id;

        m_IDToAttr[group_id] = new_record;
        m_TokenToAttr[new_token] = new_record;

        m_RegisteredGroups.set_bit(group_id);
    }
    return;
}


// Deletes all the records for which there are no jobs. It might happened
// because the DB reloading might happened after a long delay and some jobs
// could be expired by that time.
// After all the initial value of the group ID is set as the max value of
// those which survived.
void  CNSGroupsRegistry::FinalizeGroupDictionaryLoading(void)
{
    unsigned int                        max_group_id = 0;
    CMutexGuard                         guard(m_Lock);
    TGroupIDToAttrMap::iterator         candidate = m_IDToAttr.begin();
    for ( ; candidate != m_IDToAttr.end(); ++candidate ) {
        if (candidate->first > max_group_id)
            // It is safer to have next id advanced regardless whether the
            // record is deleted or not
            max_group_id = candidate->first;

        if (candidate->second->CanBeDeleted()) {
            // Garbage collector will pick it up
            m_RegisteredGroups.set(candidate->first, false);
            m_RemoveCandidates.set(candidate->first, true);
        }
    }

    // Update the group id
    x_InitLastGroupID(max_group_id);
    return;
}


// The DB transaction is set in the outer scope
unsigned int  CNSGroupsRegistry::CollectGarbage(unsigned int  max_to_del)
{
    unsigned int                del_count = 0;
    CMutexGuard                 guard(m_Lock);
    TNSBitVector::enumerator    en(m_RemoveCandidates.first());

    for ( ; en.valid(); ++en) {
        unsigned int        group_id = *en;

        m_RemoveCandidates.set(group_id, false);

        m_GroupDictDB->group_id = group_id;
        m_GroupDictDB->Delete(CBDB_File::eIgnoreError);

        TGroupIDToAttrMap::iterator     found = m_IDToAttr.find(group_id);
        if (found == m_IDToAttr.end())
            continue;

        if (found->second->CanBeDeleted())
            x_DeleteSingleInMemory(found);

        ++del_count;
        if (del_count >= max_to_del)
            break;
    }

    return del_count;
}


unsigned int  CNSGroupsRegistry::x_GetNextGroupID(void)
{
    CFastMutexGuard     guard(m_LastGroupIDLock);

    ++m_LastGroupID;
    if (m_LastGroupID == 0)
        ++m_LastGroupID;
    return m_LastGroupID;
}


// Creates a group with no jobs in it and saves the info in the DB.
// The caller provides the containers lock.
// It's the caller responsibility to provide the berkley DB transaction for
// this.
unsigned int  CNSGroupsRegistry::x_CreateGroup(const string &  group)
{
    unsigned int        group_id = x_GetNextGroupID();
    for (;;) {
        if (m_IDToAttr.find(group_id) == m_IDToAttr.end())
            break;
        group_id = x_GetNextGroupID();
    }

    auto_ptr<string>        new_token(new string(group));
    auto_ptr<SNSGroupJobs>  new_attr(new SNSGroupJobs);

    new_attr->m_GroupToken = new_token.get();
    new_attr->m_GroupId = group_id;

    // Add to containers
    m_TokenToAttr[new_token.get()] = new_attr.get();
    m_IDToAttr[group_id] = new_attr.get();

    new_token.release();
    new_attr.release();

    // Add to the registered groups list
    m_RegisteredGroups.set_bit(group_id);

    // Update the database.
    // The transaction is in the outer scope.
    m_GroupDictDB->group_id = group_id;
    m_GroupDictDB->token = group;
    m_GroupDictDB->UpdateInsert();

    return group_id;
}


string
CNSGroupsRegistry::x_PrintSelected(const TNSBitVector &    batch,
                                   const CQueue *          queue,
                                   bool                    verbose) const
{
    string      buffer;
    size_t      printed = 0;

    CMutexGuard                         guard(m_Lock);
    TGroupIDToAttrMap::const_iterator   k = m_IDToAttr.begin();
    for ( ; k != m_IDToAttr.end(); ++k ) {
        if (batch[k->first] && k->second->m_Jobs.any()) {
            buffer += x_PrintOne(*k->second, queue, verbose);
            ++printed;
            if (printed >= batch.count())
                break;
        }
    }

    return buffer;
}


string
CNSGroupsRegistry::x_PrintOne(const SNSGroupJobs &     group_attr,
                              const CQueue *           queue,
                              bool                     verbose) const
{
    string      buffer;

    buffer += "OK:GROUP: '" +
              NStr::PrintableString(*(group_attr.m_GroupToken)) + "'\n"
              "OK:  ID: " + NStr::NumericToString(group_attr.m_GroupId) + "\n";

    if (verbose) {
        if (group_attr.m_Jobs.any()) {
            buffer += "OK:  JOBS:\n";

            TNSBitVector::enumerator    en(group_attr.m_Jobs.first());
            for ( ; en.valid(); ++en) {
                unsigned int    job_id = *en;
                TJobStatus      status = queue->GetJobStatus(job_id);
                buffer += "OK:    " + queue->MakeJobKey(job_id) + " " +
                          CNetScheduleAPI::StatusToString(status) + "\n";
            }
        }
        else
            buffer += "OK:  JOBS: NONE\n";
    }
    else
        buffer += "OK:  NUMBER OF JOBS: " +
                  NStr::NumericToString(group_attr.m_Jobs.count()) + "\n";

    return buffer;
}


// Cleans up the allocated memory and does not touch the DB
void  CNSGroupsRegistry::x_Clear(void)
{
    CMutexGuard                         guard(m_Lock);
    TGroupIDToAttrMap::iterator         to_del = m_IDToAttr.begin();
    for ( ; to_del != m_IDToAttr.end(); ++to_del) {
        delete to_del->second->m_GroupToken;
        delete to_del->second;
    }

    // Containers
    m_IDToAttr.clear();
    m_TokenToAttr.clear();
    m_RegisteredGroups.clear();
    return;
}


void  CNSGroupsRegistry::x_DeleteSingleInMemory(TGroupIDToAttrMap::iterator  to_del)
{
    unsigned int    group_id = to_del->second->m_GroupId;
    const string *  group_token = to_del->second->m_GroupToken;

    // Containers
    TGroupTokenToAttrMap::iterator  to_del_too = m_TokenToAttr.find(group_token);
    delete group_token;
    delete to_del->second;

    m_IDToAttr.erase(to_del);
    if (to_del_too == m_TokenToAttr.end()) {
        // Likely an internal error
        ERR_POST("Internal inconsistency detected. The group " +
                 NStr::NumericToString(group_id) +
                 " exists in the id->attr container and does "
                 "not exist in the token->attr container.");
    } else {
        m_TokenToAttr.erase(to_del_too);
    }

    // Registered group
    m_RegisteredGroups.set(group_id, false);
    m_RemoveCandidates.set(group_id, true);
    return;
}


END_NCBI_SCOPE

