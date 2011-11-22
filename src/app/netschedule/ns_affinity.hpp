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

#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE


// Forward declaration of a Berkley DB table structure
struct SAffinityDictDB;


// The affinity token is used twice so to avoid storing it twice a heap
// allocated string is used. So here is a compare functor.
struct SNSTokenCompare
{
    bool operator () (const string * const &  lhs,
                      const string * const &  rhs) const
    {
        return (*lhs) < (*rhs);
    }
};


// Affinity identifier associated data
struct SNSJobsAffinity
{
    const string *  m_AffToken;
    TNSBitVector    m_Jobs;
    TNSBitVector    m_Clients;
};




// Provides storage, search and other high level affinity operations
class CNSAffinityRegistry
{
    public:
        CNSAffinityRegistry();
        ~CNSAffinityRegistry();

        void  Attach(SAffinityDictDB *  aff_dict_db);
        void  Detach(void);
        void  InitLastAffinityID(unsigned int  value);

        size_t        size(void) const;
        unsigned int  GetIDByToken(const string &  aff_token) const;
        string        GetTokenByID(unsigned int  aff_id) const;

        unsigned int  ResolveAffinityToken(const string &  token,
                                           unsigned int    job_id,
                                           unsigned int    client_id);
        TNSBitVector  GetAffinityIDs(const list< string > &  tokens) const;
        map< string, unsigned int >  GetJobsPerToken(void) const;
        TNSBitVector  GetJobsWithAffinity(const TNSBitVector &  aff_ids) const;
        TNSBitVector  GetJobsWithAffinity(unsigned int  aff_id) const;
        void  RemoveJobFromAffinity(unsigned int  job_id, unsigned int  aff_id);
        size_t  RemoveClientFromAffinities(unsigned int          client_id,
                                           const TNSBitVector &  aff_ids);

        // Used to load the affinity DB table and register loaded jobs.
        // The loading procedure has 3 steps:
        // 1. Load the dictionary from the DB
        // 2. For each loaded job -> call AddJobToAffinity()
        // 3. Call FinalizeAffinityDictionaryLoading()
        // These functions should not be used for anything but loading DB.
        void  LoadAffinityDictionary(void);
        void  AddJobToAffinity(unsigned int  job_id,  unsigned int  aff_id);
        void  FinalizeAffinityDictionaryLoading(void);

        // Needs when all the jobs are deleted (DROPQ)
        void  ClearMemoryAndDatabase(void);

    private:
        void x_Clear(void);
        void x_DeleteAffinity(unsigned int                   aff_id,
                              map<unsigned int,
                                  SNSJobsAffinity>::iterator found_aff);

    private:
        SAffinityDictDB *       m_AffDictDB;    // DB to store aff id -> aff token

        map< const string *,
             unsigned int,
             SNSTokenCompare >  m_AffinityIDs;  // Aff token -> aff id
        map< unsigned int,
             SNSJobsAffinity >  m_JobsAffinity; // Aff id -> aff token and jobs
        mutable CRWLock         m_Lock;         // Lock for the operations

    private:
        unsigned int            m_LastAffinityID;
        CFastMutex              m_LastAffinityIDLock;
        unsigned int            x_GetNextAffinityID(void);
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_BDB_AFF__HPP */

