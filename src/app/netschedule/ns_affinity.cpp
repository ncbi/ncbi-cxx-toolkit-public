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
 * File Description: NeSchedule affinity registry
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbistd.hpp>

#include "ns_affinity.hpp"
#include "ns_queue.hpp"
#include "ns_db.hpp"
#include "job_status.hpp"


BEGIN_NCBI_SCOPE


SNSJobsAffinity::SNSJobsAffinity() :
    m_AffToken(NULL),
    m_Jobs(bm::BM_GAP),
    m_WNClients(bm::BM_GAP),
    m_ReaderClients(bm::BM_GAP),
    m_WaitGetClients(bm::BM_GAP),
    m_WaitReadClients(bm::BM_GAP),
    m_JobsOpCount(0),
    m_WNClientsOpCount(0),
    m_ReaderClientsOpCount(0),
    m_WaitGetClientsOpCount(0),
    m_WaitReadClientsCount(0)
{}


bool SNSJobsAffinity::CanBeDeleted(void) const
{
    return !m_Jobs.any() &&
           !m_WNClients.any() &&
           !m_ReaderClients.any() &&
           !m_WaitGetClients.any() &&
           !m_WaitReadClients.any();
}


void SNSJobsAffinity::x_JobsOp(void)
{
    if (++m_JobsOpCount >= k_OpLimitToOptimize) {
        m_JobsOpCount = 0;
        m_Jobs.optimize(0, TNSBitVector::opt_free_0);
    }
}


void SNSJobsAffinity::x_WNClientsOp(void)
{
    if (++m_WNClientsOpCount >= k_OpLimitToOptimize) {
        m_WNClientsOpCount = 0;
        m_WNClients.optimize(0, TNSBitVector::opt_free_0);
    }
}


void SNSJobsAffinity::x_ReaderClientsOp(void)
{
    if (++m_ReaderClientsOpCount >= k_OpLimitToOptimize) {
        m_ReaderClientsOpCount = 0;
        m_ReaderClients.optimize(0, TNSBitVector::opt_free_0);
    }
}


void SNSJobsAffinity::x_WaitGetOp(void)
{
    if (++m_WaitGetClientsOpCount >= k_OpLimitToOptimize) {
        m_WaitGetClientsOpCount = 0;
        m_WaitGetClients.optimize(0, TNSBitVector::opt_free_0);
    }
}


void SNSJobsAffinity::x_WaitReadOp(void)
{
    if (++m_WaitReadClientsCount >= k_OpLimitToOptimize) {
        m_WaitReadClientsCount = 0;
        m_WaitReadClients.optimize(0, TNSBitVector::opt_free_0);
    }
}

CNSAffinityRegistry::CNSAffinityRegistry() :
    m_AffDictDB(NULL),
    m_LastAffinityID(0)
{}


CNSAffinityRegistry::~CNSAffinityRegistry()
{
    Detach();
    x_Clear();
    return;
}


void CNSAffinityRegistry::Attach(SAffinityDictDB *  aff_dict_db)
{
    m_AffDictDB = aff_dict_db;
}


void CNSAffinityRegistry::Detach(void)
{
    m_AffDictDB = NULL;
}


void  CNSAffinityRegistry::x_InitLastAffinityID(unsigned int  value)
{
    CFastMutexGuard     guard(m_LastAffinityIDLock);
    m_LastAffinityID = value;
    return;
}


size_t  CNSAffinityRegistry::size(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_AffinityIDs.size();
}


unsigned int
CNSAffinityRegistry::GetIDByToken(const string &  aff_token) const
{
    if (aff_token.empty())
        return 0;

    CMutexGuard                             guard(m_Lock);
    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator  affinity =
                                                m_AffinityIDs.find(&aff_token);

    if (affinity == m_AffinityIDs.end())
        return 0;
    return affinity->second;
}


string  CNSAffinityRegistry::GetTokenByID(unsigned int  aff_id) const
{
    if (aff_id == 0)
        return "";

    CMutexGuard                             guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::const_iterator  found = m_JobsAffinity.find(aff_id);
    if (found == m_JobsAffinity.end())
        return "";
    return *(found->second.m_AffToken);
}


// Adds a new record in the database if required
// and adds the job to the affinity
unsigned int
CNSAffinityRegistry::ResolveAffinityToken(const string &     token,
                                          unsigned int       job_id,
                                          unsigned int       client_id,
                                          ECommandGroup      cmd_group)
{
    if (token.empty())
        return 0;

    CMutexGuard                             guard(m_Lock);

    // Search for this affinity token
    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator  found = m_AffinityIDs.find(&token);
    if (found != m_AffinityIDs.end()) {
        // This token is known. Update the jobs/clients vectors and finish
        unsigned int    aff_id = found->second;

        map< unsigned int,
             SNSJobsAffinity >::iterator    jobs_affinity =
                                                    m_JobsAffinity.find(aff_id);
        if (job_id != 0)
            jobs_affinity->second.AddJob(job_id);

        if (client_id != 0)
            x_AddClient(jobs_affinity->second, client_id, cmd_group);
        return aff_id;
    }

    // Here: this is a new token. The DB and memory structures should be
    //       created. Let's start with a new identifier.
    unsigned int    aff_id = x_GetNextAffinityID();
    for (;;) {
        if (m_JobsAffinity.find(aff_id) == m_JobsAffinity.end())
            break;
        aff_id = x_GetNextAffinityID();
    }

    // Create a record in the token->id map
    string *    new_token = new string(token);
    m_AffinityIDs[new_token] = aff_id;

    // Create a record in the id->attributes map
    SNSJobsAffinity     new_job_affinity;
    new_job_affinity.m_AffToken = new_token;
    if (job_id != 0)
        new_job_affinity.AddJob(job_id);

    if (client_id != 0)
        x_AddClient(new_job_affinity, client_id, cmd_group);

    m_JobsAffinity[aff_id] = new_job_affinity;

    // Memorize the new affinity id
    m_RegisteredAffinities.set_bit(aff_id);

    // Update the database. The transaction is created in the outer scope.
    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->token = token;
    m_AffDictDB->UpdateInsert();

    return aff_id;
}


// The function is used when a WGET is received and there were no jobs for this
// client. In this case non-existed affinities must be resolved and the client
// must be memorized as a referencer of the affinities.
// The DB transaction must be set in the outer scope.
vector<unsigned int>
CNSAffinityRegistry::ResolveAffinities(const list< string > &  tokens)
{
    vector<unsigned int>    result;

    for (list<string>::const_iterator  k(tokens.begin());
         k != tokens.end(); ++k)
        result.push_back(ResolveAffinity(*k));
    return result;
}


// Non-existent affinity will be created
unsigned int  CNSAffinityRegistry::ResolveAffinity(const string &  token)
{
    CMutexGuard             guard(m_Lock);

    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator  found = m_AffinityIDs.find(&token);
    if (found != m_AffinityIDs.end())
        return found->second;

    // Here: this is a new token. The DB and memory structures should be
    //       created. Let's start with a new identifier.
    unsigned int    aff_id = x_GetNextAffinityID();
    for (;;) {
        if (m_JobsAffinity.find(aff_id) == m_JobsAffinity.end())
            break;
        aff_id = x_GetNextAffinityID();
    }

    // Create a record in the token->id map
    string *    new_token = new string(token);
    m_AffinityIDs[new_token] = aff_id;

    // Create a record in the id->attributes map
    SNSJobsAffinity     new_job_affinity;
    new_job_affinity.m_AffToken = new_token;
    m_JobsAffinity[aff_id] = new_job_affinity;

    // Memorize the new affinity ID
    m_RegisteredAffinities.set_bit(aff_id);

    // Update the database. The transaction is created in the outer scope.
    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->token = token;
    m_AffDictDB->UpdateInsert();

    return aff_id;
}


list< SAffinityStatistics >
CNSAffinityRegistry::GetAffinityStatistics(
                            const CJobStatusTracker &  status_tracker) const
{
    list< SAffinityStatistics >     result;
    CMutexGuard                     guard(m_Lock);

    for (map< unsigned int,
              SNSJobsAffinity >::const_iterator  k = m_JobsAffinity.begin();
         k != m_JobsAffinity.end(); ++k) {
        SAffinityStatistics     stat;

        stat.m_Token = *k->second.m_AffToken;
        stat.m_NumberOfWNPreferred = k->second.m_WNClients.count();
        stat.m_NumberOfWNWaitGet = k->second.m_WaitGetClients.count();
        stat.m_NumberOfReaderPreferred = k->second.m_ReaderClients.count();
        stat.m_NumberOfReaderWaitRead = k->second.m_WaitReadClients.count();

        // Count the number of pending and running jobs
        stat.m_NumberOfPendingJobs = 0;
        stat.m_NumberOfRunningJobs = 0;

        TNSBitVector::enumerator    en(k->second.m_Jobs.first());
        for ( ; en.valid(); ++en) {
            TJobStatus      status = status_tracker.GetStatus(*en);
            if (status == CNetScheduleAPI::ePending)
                ++stat.m_NumberOfPendingJobs;
            else if (status == CNetScheduleAPI::eRunning)
                ++stat.m_NumberOfRunningJobs;
        }

        result.push_back(stat);
    }
    return result;
}


TNSBitVector
CNSAffinityRegistry::GetJobsWithAffinity(unsigned int  aff_id) const
{
    if (aff_id == 0)
        return TNSBitVector();

    CMutexGuard                             guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::const_iterator  found = m_JobsAffinity.find(aff_id);

    if (found != m_JobsAffinity.end())
        return found->second.m_Jobs;
    return TNSBitVector();
}


TNSBitVector
CNSAffinityRegistry::GetJobsWithAffinities(const TNSBitVector &  affs) const
{
    TNSBitVector                            jobs;
    TNSBitVector::enumerator                en(affs.first());
    map< unsigned int,
         SNSJobsAffinity >::const_iterator  found;
    CMutexGuard                             guard(m_Lock);

    for ( ; en.valid(); ++en) {
        found = m_JobsAffinity.find(*en);
        if (found != m_JobsAffinity.end())
            jobs |= found->second.m_Jobs;
    }
    return jobs;
}


TNSBitVector
CNSAffinityRegistry::GetRegisteredAffinities(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_RegisteredAffinities;
}


// Removes the job from affinity and memorizes the aff_id in a list of
// candidates for the removal from the DB.
void CNSAffinityRegistry::RemoveJobFromAffinity(unsigned int  job_id,
                                                unsigned int  aff_id)
{
    if (job_id == 0 || aff_id == 0)
        return;

    CMutexGuard                         guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);

    if (found == m_JobsAffinity.end())
        // The affinity is not known
        return;

    found->second.RemoveJob(job_id);
    if (found->second.CanBeDeleted())
        // Mark for deletion by the garbage collector
        m_RemoveCandidates.set_bit(aff_id);
    return;
}


// Removes the client from affinities and memorize those affinities which do
// not have any references to them in the list of candidates for deletion.
size_t
CNSAffinityRegistry::RemoveClientFromAffinities(unsigned int          client_id,
                                                const TNSBitVector &  aff_ids,
                                                ECommandGroup         cmd_group)
{
    return x_RemoveClientFromAffinities(client_id, aff_ids, false, cmd_group);
}


// Removes the waiting client from affinities and memorize those affinities
// which do not have any references to them in the list of candidates
// for deletion.
size_t
CNSAffinityRegistry::RemoveWaitClientFromAffinities(
                                            unsigned int          client_id,
                                            const TNSBitVector &  aff_ids,
                                            ECommandGroup         cmd_group)
{
    return x_RemoveClientFromAffinities(client_id, aff_ids, true, cmd_group);
}


// Registers the client as the one which has the affinity as preferred
void
CNSAffinityRegistry::AddClientToAffinity(unsigned int   client_id,
                                         unsigned int   aff_id,
                                         ECommandGroup  cmd_group)
{
    if (client_id == 0 || aff_id == 0)
        return;

    CMutexGuard                         guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);
    if (found == m_JobsAffinity.end())
        return;     // Affinity is not known.
                    // This should never happened basically.

    x_AddClient(found->second, client_id, cmd_group);
    return;
}


void
CNSAffinityRegistry::AddClientToAffinities(unsigned int          client_id,
                                           const TNSBitVector &  aff_ids,
                                           ECommandGroup         cmd_group)
{
    if (client_id == 0 || !aff_ids.any())
        return;

    CMutexGuard                 guard(m_Lock);
    TNSBitVector::enumerator    en(aff_ids.first());
    for ( ; en.valid(); ++en) {
        map< unsigned int,
             SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(*en);
        if (found != m_JobsAffinity.end())
            x_AddClient(found->second, client_id, cmd_group);
    }
}


size_t
CNSAffinityRegistry::x_RemoveClientFromAffinities(
                                        unsigned int          client_id,
                                        const TNSBitVector &  aff_ids,
                                        bool                  is_wait_client,
                                        ECommandGroup         cmd_group)
{
    if (client_id == 0 || !aff_ids.any())
        return 0;

    size_t                      del_count = 0;
    CMutexGuard                 guard(m_Lock);

    TNSBitVector::enumerator    en(aff_ids.first());
    for ( ; en.valid(); ++en) {
        map< unsigned int,
             SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(*en);
        if (found == m_JobsAffinity.end())
            continue;   // Affinity is not known.
                        // This should never happened basically.

        if (is_wait_client)
            x_RemoveWaitClient(found->second, client_id, cmd_group);
        else
            x_RemoveClient(found->second, client_id, cmd_group);

        if (found->second.CanBeDeleted()) {
            // Mark for the deletion by the garbage collector
            m_RemoveCandidates.set_bit(*en);
            ++del_count;
        }
    }

    return del_count;
}


void
CNSAffinityRegistry::SetWaitClientForAffinities(
                                        unsigned int          client_id,
                                        const TNSBitVector &  aff_ids,
                                        ECommandGroup         cmd_group)
{
    if (client_id == 0 || !aff_ids.any())
        return;

    CMutexGuard                 guard(m_Lock);

    TNSBitVector::enumerator    en(aff_ids.first());
    for ( ; en.valid(); ++en) {
        map< unsigned int,
             SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(*en);
        if (found == m_JobsAffinity.end())
            continue;   // Affinity is not known.
                        // This should never happened basically.

        x_AddWaitClient(found->second, client_id, cmd_group);
    }
}


string  CNSAffinityRegistry::Print(const CQueue *              queue,
                                   const CNSClientsRegistry &  clients_registry,
                                   size_t                      batch_size,
                                   bool                        verbose) const
{
    string              result;
    TNSBitVector        batch;

    TNSBitVector                registered_affs = GetRegisteredAffinities();
    TNSBitVector::enumerator    en(registered_affs.first());

    while (en.valid()) {
        batch.set_bit(*en);
        ++en;

        if (batch.count() >= batch_size) {
            result += x_PrintSelected(batch, queue, clients_registry,
                                      verbose) + "\n";
            batch.clear();
        }
    }

    if (batch.count() > 0)
        result += x_PrintSelected(batch, queue, clients_registry,
                                  verbose) + "\n";
    return result;
}


string
CNSAffinityRegistry::x_PrintSelected(const TNSBitVector &        batch,
                                     const CQueue *              queue,
                                     const CNSClientsRegistry &  clients_registry,
                                     bool                        verbose) const
{
    string          buffer;
    size_t          printed = 0;

    CMutexGuard     guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::const_iterator      k = m_JobsAffinity.begin();
    for ( ; k != m_JobsAffinity.end(); ++k ) {
        if (batch[k->first]) {
            buffer += x_PrintOne(k->first, k->second,
                                 queue, clients_registry, verbose);
            ++printed;
            if (printed >= batch.count())
                break;
        }
    }

    return buffer;
}

string
CNSAffinityRegistry::x_PrintOne(unsigned int                aff_id,
                                const SNSJobsAffinity &     jobs_affinity,
                                const CQueue *              queue,
                                const CNSClientsRegistry &  clients_registry,
                                bool                        verbose) const
{
    string      buffer;

    buffer += "OK:AFFINITY: '" +
              NStr::PrintableString(*(jobs_affinity.m_AffToken)) + "'\n"
              "OK:  ID: " + NStr::NumericToString(aff_id) + "\n";

    if (verbose) {
        if (jobs_affinity.m_Jobs.any()) {
            buffer += "OK:  JOBS:\n";

            TNSBitVector::enumerator    en(jobs_affinity.m_Jobs.first());
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
                  NStr::NumericToString(jobs_affinity.m_Jobs.count()) + "\n";

    if (verbose) {
        if (jobs_affinity.m_WNClients.any()) {
            buffer += "OK:  WN CLIENTS (PREFERRED):\n";

            TNSBitVector::enumerator    en(jobs_affinity.m_WNClients.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + clients_registry.GetNodeName(*en) +
                          "'\n";
        }
        else
            buffer += "OK:  WN CLIENTS (PREFERRED): NONE\n";
    }
    else
        buffer += "OK:  NUMBER OF WN CLIENTS (PREFERRED): " +
                  NStr::NumericToString(jobs_affinity.m_WNClients.count()) +
                  "\n";

    if (verbose) {
        if (jobs_affinity.m_ReaderClients.any()) {
            buffer += "OK:  READER CLIENTS (PREFERRED):\n";

            TNSBitVector::enumerator    en(jobs_affinity.
                                                    m_ReaderClients.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + clients_registry.GetNodeName(*en) +
                          "'\n";
        }
        else
            buffer += "OK:  READER CLIENTS (PREFERRED): NONE\n";
    }
    else
        buffer += "OK:  NUMBER OF READER CLIENTS (PREFERRED): " +
                  NStr::NumericToString(jobs_affinity.m_ReaderClients.count()) +
                  "\n";

    if (verbose) {
        if (jobs_affinity.m_WaitGetClients.any()) {
            buffer += "OK:  CLIENTS (EXPLICIT WGET):\n";

            TNSBitVector::enumerator    en(jobs_affinity.
                                                    m_WaitGetClients.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + clients_registry.GetNodeName(*en) +
                          "'\n";
        }
        else
            buffer += "OK:  CLIENTS (EXPLICIT WGET): NONE\n";
    }
    else
        buffer += "OK:  NUMBER OF CLIENTS (EXPLICIT WGET): " +
                  NStr::NumericToString(jobs_affinity.
                                        m_WaitGetClients.count()) + "\n";

    if (verbose) {
        if (jobs_affinity.m_WaitReadClients.any()) {
            buffer += "OK:  CLIENTS (EXPLICIT READ):\n";

            TNSBitVector::enumerator    en(jobs_affinity.
                                                    m_WaitReadClients.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + clients_registry.GetNodeName(*en) +
                          "'\n";
        }
        else
            buffer += "OK:  CLIENTS (EXPLICIT READ): NONE\n";
    }
    else
        buffer += "OK:  NUMBER OF CLIENTS (EXPLICIT READ): " +
                  NStr::NumericToString(jobs_affinity.
                                        m_WaitReadClients.count()) + "\n";

    return buffer;
}


// Reads the DB and fills in two maps. The jobs list for a certain affinity is
// empty at this stage.
// A set of AddJobToAffinity(...) calls is expected after loading the
// dictionary.
// The final step in the loading affinities is to get the
// FinalizeAffinityDictionaryLoading() call.
void  CNSAffinityRegistry::LoadAffinityDictionary(void)
{
    CMutexGuard         guard(m_Lock);
    CBDB_FileCursor     cur(*m_AffDictDB);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;
    for (; cur.Fetch() == eBDB_Ok;) {
        unsigned int        aff_id = m_AffDictDB->aff_id;
        string *            new_token = new string(m_AffDictDB->token);
        SNSJobsAffinity     new_record;

        new_record.m_AffToken = new_token;

        m_JobsAffinity[aff_id] = new_record;
        m_AffinityIDs[new_token] = aff_id;

        m_RegisteredAffinities.set_bit(aff_id);
    }
    return;
}


// Adds one job to the affinity.
// The affinity must exist in the dictionary, otherwise it is an internal logic
// error.
void  CNSAffinityRegistry::AddJobToAffinity(unsigned int  job_id,
                                            unsigned int  aff_id)
{
    CMutexGuard                         guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);

    if (found == m_JobsAffinity.end()) {
        // It is likely an internal error
        ERR_POST("Internal error while loading affinity registry. "
                 "The affinity with id " + NStr::NumericToString(aff_id) +
                 " is not found in the loaded dictionary.");
        return;
    }

    found->second.AddJob(job_id);

    // It is for sure that the affinity cannot be deleted
    m_RemoveCandidates.set_bit(aff_id, false);
    return;
}


// Deletes all the records for which there are no jobs. It might happened
// because the DB reloading might happened after a long delay and some jobs
// could be expired by that time.
// After all the initial value of the affinity ID is set as the max value of
// those which survived.
void  CNSAffinityRegistry::FinalizeAffinityDictionaryLoading(void)
{
    unsigned int                        max_aff_id = 0;
    CMutexGuard                         guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    candidate = m_JobsAffinity.begin();
    for ( ; candidate != m_JobsAffinity.end(); ++candidate ) {
        if (candidate->first > max_aff_id)
            // It is safer to have next id advanced regardless whether the
            // record is deleted or not
            max_aff_id = candidate->first;

        if (candidate->second.CanBeDeleted()) {
            // Garbage collector will pick it up
            m_RegisteredAffinities.set_bit(candidate->first, false);
            m_RemoveCandidates.set_bit(candidate->first);
        }
    }

    // Update the affinity id
    x_InitLastAffinityID(max_aff_id);
    return;
}


void CNSAffinityRegistry::ClearMemoryAndDatabase(void)
{
    // Clear the data structures in memory
    x_Clear();

    // Clear the Berkley DB table.
    // Safe truncate can delete everything without a transaction.
    m_AffDictDB->SafeTruncate();
    return;
}


// The DB transaction is set in the outer scope
unsigned int  CNSAffinityRegistry::CollectGarbage(unsigned int  max_to_del)
{
    unsigned int                del_count = 0;

    CMutexGuard                 guard(m_Lock);
    TNSBitVector::enumerator    en(m_RemoveCandidates.first());
    for ( ; en.valid(); ++en) {
        unsigned int        aff_id = *en;

        m_RemoveCandidates.set_bit(aff_id, false);

        map< unsigned int,
             SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);
        if (found == m_JobsAffinity.end())
            continue;

        if (found->second.CanBeDeleted()) {
            x_DeleteAffinity(aff_id, found);
            ++del_count;
            if (del_count >= max_to_del)
                break;
        }
    }

    return del_count;
}


unsigned int  CNSAffinityRegistry::CheckRemoveCandidates(void)
{
    unsigned int                still_candidate = 0;

    CMutexGuard                 guard(m_Lock);
    TNSBitVector::enumerator    en(m_RemoveCandidates.first());
    for ( ; en.valid(); ++en) {
        unsigned int                        aff_id = *en;
        map< unsigned int,
             SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);

        if (found == m_JobsAffinity.end()) {
            m_RemoveCandidates.set_bit(aff_id, false);
            continue;
        }

        if (found->second.CanBeDeleted())
            ++still_candidate;
        else
            m_RemoveCandidates.set_bit(aff_id, false);
    }
    return still_candidate;
}


void CNSAffinityRegistry::x_Clear(void)
{
    // Delete all the allocated strings
    CMutexGuard                         guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    jobs_affinity = m_JobsAffinity.begin();
    for (; jobs_affinity != m_JobsAffinity.end(); ++jobs_affinity)
        delete jobs_affinity->second.m_AffToken;

    // Clear containers
    m_AffinityIDs.clear();
    m_JobsAffinity.clear();
    m_RegisteredAffinities.clear();
    return;
}


void CNSAffinityRegistry::x_DeleteAffinity(
                            unsigned int                   aff_id,
                            map<unsigned int,
                                SNSJobsAffinity>::iterator found_aff)
{
    map< const string *,
         unsigned int,
         SNSTokenCompare >::iterator    aff_tok = m_AffinityIDs.find(
                                                found_aff->second.m_AffToken);
    if (aff_tok != m_AffinityIDs.end())
        m_AffinityIDs.erase(aff_tok);

    delete found_aff->second.m_AffToken;
    m_JobsAffinity.erase(found_aff);

    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->Delete(CBDB_File::eIgnoreError);

    m_RegisteredAffinities.set_bit(aff_id, false);
    return;
}


unsigned int
CNSAffinityRegistry::x_GetNextAffinityID(void)
{
    CFastMutexGuard     guard(m_LastAffinityIDLock);

    ++m_LastAffinityID;
    if (m_LastAffinityID == 0)
        ++m_LastAffinityID;
    return m_LastAffinityID;
}


void
CNSAffinityRegistry::x_AddClient(SNSJobsAffinity &  aff_data,
                                 unsigned int       client_id,
                                 ECommandGroup      cmd_group)
{
    switch (cmd_group) {
        case eGet:
            aff_data.AddWNClient(client_id);
            break;
        case eRead:
            aff_data.AddReaderClient(client_id);
            break;
        default:
            break;
    }
}


void
CNSAffinityRegistry::x_RemoveClient(SNSJobsAffinity &  aff_data,
                                    unsigned int       client_id,
                                    ECommandGroup      cmd_group)
{
    switch (cmd_group) {
        case eGet:
            aff_data.RemoveWNClient(client_id);
            break;
        case eRead:
            aff_data.RemoveReaderClient(client_id);
            break;
        default:
            break;
    }
}


void
CNSAffinityRegistry::x_AddWaitClient(SNSJobsAffinity &  aff_data,
                                     unsigned int       client_id,
                                     ECommandGroup      cmd_group)
{
    switch (cmd_group) {
        case eGet:
            aff_data.AddWNWaitClient(client_id);
            break;
        case eRead:
            aff_data.AddReadWaitClient(client_id);
            break;
        default:
            break;
    }
}


void
CNSAffinityRegistry::x_RemoveWaitClient(SNSJobsAffinity &  aff_data,
                                        unsigned int       client_id,
                                        ECommandGroup      cmd_group)
{
    switch (cmd_group) {
        case eGet:
            aff_data.RemoveWNWaitClient(client_id);
            break;
        case eRead:
            aff_data.RemoveReadWaitClient(client_id);
            break;
        default:
            break;
    }
}

END_NCBI_SCOPE

