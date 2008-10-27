#ifndef NETSCHEDULE_BDB_AFF__HPP
#define NETSCHEDULE_BDB_AFF__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Net schedule job status database.
 *
 */

/// @file bdb_affinity.hpp
/// NetSchedule job affinity.
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE

struct SAffinityDictDB;
struct SAffinityDictTokenIdx;

/// Affinity dictionary database class
///
/// @internal
class CAffinityDict
{
public:
    CAffinityDict();
    ~CAffinityDict();

    /// Open the association files
//    void Open(CBDB_Env& env, const string& queue_name);

//    void Close();

    void Attach(SAffinityDictDB* aff_dict_db,
                SAffinityDictTokenIdx* aff_dict_token_idx);
    void Detach();

    /// Add a new token or return token's id if this affinity
    /// index is already in the database
    unsigned CheckToken(const char*       aff_token,
                        CBDB_Transaction& trans);

    /// Get affinity token id
    /// Returns 0 if token does not exist
    unsigned GetTokenId(const string& aff_token);

    /// Get affinity string by id
    string GetAffToken(unsigned aff_id);

    /// Remove affinity token
    void RemoveToken(unsigned          aff_id,
                     CBDB_Transaction& trans);

private:
    CAffinityDict(const CAffinityDict& );
    CAffinityDict& operator=(const CAffinityDict& );
private:
    CAtomicCounter           m_IdCounter;
    CFastMutex               m_DbLock;
    SAffinityDictDB*         m_AffDictDB;
    CBDB_FileCursor*         m_CurAffDB;
    SAffinityDictTokenIdx*   m_AffDict_TokenIdx;
    CBDB_FileCursor*         m_CurTokenIdx;
};


/// Holder for worker nodes affinity associations
/// Worker node -> affinity id
///
/// @internal
class CWorkerNodeAffinity
{
public:
    /// Affinity association information
    struct SAffinityInfo
    {
        TNSBitVector  aff_ids;          ///< List of affinity tokens
        TNSBitVector  candidate_jobs;   ///< List of job candidates for this node
        TNSBitVector  blacklisted_jobs; ///< List of jobs, blacklisted for node
        // Blacklist expiration handling
        /// We should not bother revise the list before this time
        time_t        min_expire_time;
        typedef vector<pair<time_t, unsigned> > TExpirationVector;
        TExpirationVector blacklisted_expirations;

        SAffinityInfo()
            : aff_ids(bm::BM_GAP), candidate_jobs(bm::BM_GAP),
              blacklisted_jobs(bm::BM_GAP)
        {}
        const TNSBitVector& GetBlacklistedJobs(time_t t);
    };
    typedef unsigned TNetAddress;
public:
    CWorkerNodeAffinity();
    ~CWorkerNodeAffinity();

    /// Clear all the affinity associations
    void ClearAffinity();

    /// Forget affinity association
    void ClearAffinity(const string& node_id);

    /// Create/update affinity association
    void AddAffinity(const string& node_id,
                     unsigned      aff_id,
                     time_t        exp_time);

    void BlacklistJob(const string& node_id,
                      unsigned      job_id,
                      time_t        exp_time);

    /// Remove affinity token association
    void RemoveAffinity(unsigned aff_id);

    /// Remove affinity token association, input is specified by a
    /// vector of ids
    void RemoveAffinity(const TNSBitVector& bv);

    /// Retrieve all affinity ids assigned to all(any) worker nodes
    /// Logical OR (Union) of all SAffinityInfo::aff_ids
    void GetAllAssignedAffinity(TNSBitVector& aff_ids);


    /// Free unused memory
    void OptimizeMemory();

    SAffinityInfo* GetAffinity(const string& node_id);
private:
    CWorkerNodeAffinity(const CWorkerNodeAffinity&);
    CWorkerNodeAffinity& operator=(const CWorkerNodeAffinity&);
private:
    /// Worker node to affinity id map
    typedef map<string, SAffinityInfo*> TAffMap;
    TAffMap m_AffinityMap;
};




END_NCBI_SCOPE

#endif /* NETSCHEDULE_BDB_AFF__HPP */

