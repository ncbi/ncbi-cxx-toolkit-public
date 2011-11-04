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
#include "ns_db.hpp"


BEGIN_NCBI_SCOPE


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


void  CNSAffinityRegistry::InitLastAffinityID(unsigned int  value)
{
    CFastMutexGuard     guard(m_LastAffinityIDLock);
    m_LastAffinityID = value;
    return;
}


unsigned int
CNSAffinityRegistry::GetIDByToken(const string &  aff_token) const
{
    if (aff_token.empty())
        return 0;

    CReadLockGuard                              guard(m_Lock);
    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator      affinity = m_AffinityIDs.find(&aff_token);

    if (affinity == m_AffinityIDs.end())
        return 0;
    return affinity->second;
}


string  CNSAffinityRegistry::GetTokenByID(unsigned int  aff_id) const
{
    if (aff_id == 0)
        return "";

    CReadLockGuard                              guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::const_iterator      found = m_JobsAffinity.find(aff_id);
    if (found == m_JobsAffinity.end())
        return "";
    return *(found->second.m_AffToken);
}


// Adds a new record in the database if required
// and adds the job to the affinity
unsigned int
CNSAffinityRegistry::ResolveAffinityToken(const string &     token,
                                          unsigned int       job_id)
{
    if (token.empty())
        return 0;

    CWriteLockGuard         guard(m_Lock);

    // Search for this affinity token
    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator      found = m_AffinityIDs.find(&token);
    if (found != m_AffinityIDs.end()) {
        // This token is known. Update the jobs vector and finish
        unsigned int    aff_id = found->second;

        map< unsigned int,
             SNSJobsAffinity >::iterator        jobs_affinity = m_JobsAffinity.find(aff_id);
        jobs_affinity->second.m_Jobs.set(job_id, true);
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
    new_job_affinity.m_Jobs.set(job_id, true);
    m_JobsAffinity[aff_id] = new_job_affinity;

    // Update the database. The transaction is created in the outer scope.
    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->token = token;
    m_AffDictDB->UpdateInsert();

    return aff_id;
}


TNSBitVector
CNSAffinityRegistry::GetAffinityIDs(const list< string > &  tokens) const
{
    TNSBitVector                                result;
    CReadLockGuard                              guard(m_Lock);
    map< const string *,
         unsigned int,
         SNSTokenCompare >::const_iterator      found;

    for (list<string>::const_iterator  k = tokens.begin();
         k != tokens.end(); ++k) {
        const string &      token = *k;

        if (!token.empty()) {
            found = m_AffinityIDs.find(&token);
            if (found != m_AffinityIDs.end())
                result.set(found->second, true);
        }
    }
    return result;
}


map< string, unsigned int >
CNSAffinityRegistry::GetJobsPerToken(void) const
{
    map< string, unsigned int >     result;
    CReadLockGuard                  guard(m_Lock);

    for (map< unsigned int,
              SNSJobsAffinity >::const_iterator  k = m_JobsAffinity.begin();
         k != m_JobsAffinity.end(); ++k) {
        unsigned int    count = k->second.m_Jobs.count();
        if (count > 0)
            result[ *(k->second.m_AffToken) ] = count;
    }
    return result;
}


TNSBitVector
CNSAffinityRegistry::GetJobsWithAffinity(const TNSBitVector &  aff_ids) const
{
    TNSBitVector                            result;
    TNSBitVector::enumerator                aff_id_en = aff_ids.first();
    unsigned int                            aff_id;
    map< unsigned int,
         SNSJobsAffinity >::const_iterator  found;
    CReadLockGuard                          guard(m_Lock);

    for (; aff_id_en.valid(); ++aff_id_en) {
        aff_id = *aff_id_en;
        if (aff_id == 0)
            continue;

        found = m_JobsAffinity.find(aff_id);
        if (found != m_JobsAffinity.end())
            result |= (found->second.m_Jobs);
    }
    return result;
}


TNSBitVector
CNSAffinityRegistry::GetJobsWithAffinity(unsigned int  aff_id) const
{
    if (aff_id == 0)
        return TNSBitVector();

    CReadLockGuard                          guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::const_iterator  found = m_JobsAffinity.find(aff_id);

    if (found != m_JobsAffinity.end())
        return found->second.m_Jobs;
    return TNSBitVector();
}


// Removes the job from affinity and might remove the records if there are no
// more jobs with this affinity. Deletes a record from the DB if required.
// The transaction is set in the outer scope.
void CNSAffinityRegistry::RemoveJobFromAffinity(unsigned int  job_id,
                                                unsigned int  aff_id)
{
    if (job_id == 0 || aff_id == 0)
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);

    if (found == m_JobsAffinity.end())
        // The affinity is not known
        return;

    TNSBitVector &      aff_jobs = found->second.m_Jobs;

    aff_jobs.set(job_id, false);
    if (aff_jobs.any())
        // There are still some jobs with this affinity
        return;

    // No more jobs with this affinity - clean up the data structures
    map< const string *,
         unsigned int,
         SNSTokenCompare >::iterator    aff_tok = m_AffinityIDs.find(found->second.m_AffToken);
    if (aff_tok != m_AffinityIDs.end())
        m_AffinityIDs.erase(aff_tok);

    delete found->second.m_AffToken;
    m_JobsAffinity.erase(found);

    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->Delete(CBDB_File::eIgnoreError);
    return;
}


// Reads the DB and fills in two maps. The jobs list for a certain affinity is
// empty at this stage.
// A set of AddJobToAffinity(...) calls is expected after loading the
// dictionary.
// The final step in the loading affinities is to get the
// FinalizeAffinityDictionaryLoading() call.
void  CNSAffinityRegistry::LoadAffinityDictionary(void)
{
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
    }
    return;
}


// Adds one job to the affinity.
// The affinity must exist in the dictionary, otherwise it is an internal logic
// error.
void  CNSAffinityRegistry::AddJobToAffinity(unsigned int  job_id,
                                            unsigned int  aff_id)
{
    CWriteLockGuard                     guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    found = m_JobsAffinity.find(aff_id);

    if (found == m_JobsAffinity.end())
        // It is likely an internal error
        return;

    found->second.m_Jobs.set(job_id, true);
    return;
}


// Deletes all the records for which there are no jobs. It might happened
// because the DB reloading might happened after a long delay and some jobs
// could be expired by that time.
// After all the initial value of the affinity ID is set as the max value of
// those which survived.
void  CNSAffinityRegistry::FinalizeAffinityDictionaryLoading(void)
{
    vector< unsigned int >              to_delete;
    unsigned int                        max_aff_id = 0;
    CWriteLockGuard                     guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    candidate = m_JobsAffinity.begin();
    for ( ; candidate != m_JobsAffinity.end(); ++candidate ) {
        if (!candidate->second.m_Jobs.any())
            to_delete.push_back(candidate->first);
        else {
            if (candidate->first > max_aff_id)
                max_aff_id = candidate->first;
        }
    }

    // Delete those records for which there are no jobs
    map< const string *,
         unsigned int,
         SNSTokenCompare >::iterator        aff_tok;
    for (vector< unsigned int >::const_iterator  aff_id = to_delete.begin();
         aff_id != to_delete.end(); ++aff_id) {
        candidate = m_JobsAffinity.find(*aff_id);
        if (candidate == m_JobsAffinity.end())
            // Though it is likely an internal error
            continue;
        aff_tok = m_AffinityIDs.find(candidate->second.m_AffToken);
        if (aff_tok != m_AffinityIDs.end())
            m_AffinityIDs.erase(aff_tok);
        delete candidate->second.m_AffToken;
        m_JobsAffinity.erase(candidate);
    }

    // Update the affinity id
    InitLastAffinityID(max_aff_id);
    return;
}


void CNSAffinityRegistry::ClearMemoryAndDatabase(void)
{
    // Clear the data structures in memory
    x_Clear();

    // Clear the Berkley DB table.
    // Safe truncate can delete avarything without a transaction.
    m_AffDictDB->SafeTruncate();
    return;
}


void CNSAffinityRegistry::x_Clear(void)
{

    // Delete all the allocated strings
    CWriteLockGuard                     guard(m_Lock);
    map< unsigned int,
         SNSJobsAffinity >::iterator    jobs_affinity = m_JobsAffinity.begin();
    for (; jobs_affinity != m_JobsAffinity.end(); ++jobs_affinity)
        delete jobs_affinity->second.m_AffToken;

    // Clear containers
    m_AffinityIDs.clear();
    m_JobsAffinity.clear();
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

END_NCBI_SCOPE

