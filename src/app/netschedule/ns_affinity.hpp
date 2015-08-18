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


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE

const size_t    k_OpLimitToOptimize = 1000000;

class CNSClientsRegistry;
class CQueue;
class CJobStatusTracker;


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
    TNSBitVector    m_WNClients;
    TNSBitVector    m_ReaderClients;
    TNSBitVector    m_WaitGetClients;
    TNSBitVector    m_WaitReadClients;


    SNSJobsAffinity();
    bool CanBeDeleted(void) const;

    void AddJob(unsigned int  job_id)
    { m_Jobs.set_bit(job_id);
      x_JobsOp();
    }

    void RemoveJob(unsigned int  job_id)
    { m_Jobs.set_bit(job_id, false);
      x_JobsOp();
    }

    void AddWNClient(unsigned int  client_id)
    { m_WNClients.set_bit(client_id);
      x_WNClientsOp();
    }

    void AddReaderClient(unsigned int  client_id)
    { m_ReaderClients.set_bit(client_id);
      x_ReaderClientsOp();
    }

    void RemoveWNClient(unsigned int  client_id)
    { m_WNClients.set_bit(client_id, false);
      x_WNClientsOp();
    }

    void RemoveReaderClient(unsigned int  client_id)
    { m_ReaderClients.set_bit(client_id, false);
      x_ReaderClientsOp();
    }

    void AddWNWaitClient(unsigned int  client_id)
    { m_WaitGetClients.set_bit(client_id);
      x_WaitGetOp();
    }

    void AddReadWaitClient(unsigned int  client_id)
    { m_WaitReadClients.set_bit(client_id);
      x_WaitReadOp();
    }

    void RemoveWNWaitClient(unsigned int  client_id)
    { m_WaitGetClients.set_bit(client_id, false);
      x_WaitGetOp();
    }

    void RemoveReadWaitClient(unsigned int  client_id)
    { m_WaitReadClients.set_bit(client_id, false);
      x_WaitReadOp();
    }

    private:
        void x_JobsOp(void);
        void x_WNClientsOp(void);
        void x_ReaderClientsOp(void);
        void x_WaitGetOp(void);
        void x_WaitReadOp(void);

    private:
        size_t      m_JobsOpCount;
        size_t      m_WNClientsOpCount;
        size_t      m_ReaderClientsOpCount;
        size_t      m_WaitGetClientsOpCount;
        size_t      m_WaitReadClientsCount;
};


struct SAffinityStatistics
{
    string      m_Token;
    size_t      m_NumberOfPendingJobs;
    size_t      m_NumberOfRunningJobs;
    size_t      m_NumberOfWNPreferred;
    size_t      m_NumberOfWNWaitGet;
    size_t      m_NumberOfReaderPreferred;
    size_t      m_NumberOfReaderWaitRead;
};



// Provides storage, search and other high level affinity operations
class CNSAffinityRegistry
{
    public:
        CNSAffinityRegistry();
        ~CNSAffinityRegistry();

        size_t        size(void) const;
        unsigned int  GetIDByToken(const string &  aff_token) const;
        string        GetTokenByID(unsigned int  aff_id) const;

        unsigned int  ResolveAffinityToken(const string &  token,
                                           unsigned int    job_id,
                                           unsigned int    client_id,
                                           ECommandGroup   command_group);
        vector<unsigned int>  ResolveAffinities(const list< string > &  tokens);
        unsigned int  ResolveAffinity(const string &  token);
        list< SAffinityStatistics >
        GetAffinityStatistics(const CJobStatusTracker &  status_tracker) const;
        TNSBitVector  GetJobsWithAffinity(unsigned int  aff_id) const;
        TNSBitVector  GetJobsWithAffinities(const TNSBitVector &  affs) const;
        TNSBitVector  GetRegisteredAffinities(void) const;
        void  RemoveJobFromAffinity(unsigned int  job_id, unsigned int  aff_id);
        size_t  RemoveClientFromAffinities(unsigned int          client_id,
                                           const TNSBitVector &  aff_ids,
                                           ECommandGroup         cmd_group);
        size_t  RemoveWaitClientFromAffinities(unsigned int          client_id,
                                               const TNSBitVector &  aff_ids,
                                               ECommandGroup         cmd_group);
        void  AddClientToAffinity(unsigned int   client_id,
                                  unsigned int   aff_id,
                                  ECommandGroup  cmd_group);
        void  AddClientToAffinities(unsigned int          client_id,
                                    const TNSBitVector &  aff_ids,
                                    ECommandGroup         cmd_group);
        void  SetWaitClientForAffinities(unsigned int          client_id,
                                         const TNSBitVector &  aff_ids,
                                         ECommandGroup         cmd_group);

        string Print(const CQueue *              queue,
                     const CNSClientsRegistry &  clients_registry,
                     size_t                      batch_size,
                     bool                        verbose) const;

        void Clear(void);

        unsigned int  CollectGarbage(unsigned int  max_to_del);
        unsigned int  CheckRemoveCandidates(void);
        void Dump(const string &  dump_dir_name,
                  const string &  queue_name) const;
        void RemoveDump(const string &  dump_dir_name,
                        const string &  queue_name) const;

        // Used to load the affinities and register loaded jobs.
        // The loading procedure has 3 steps:
        // 1. Load the dictionary from the flat file
        // 2. For each loaded job -> call AddJobToAffinity()
        // 3. Call FinalizeAffinityDictionaryLoading()
        // These functions should not be used for anything but loading
        // affinities from files
        void LoadFromDump(const string &  dump_dir_name,
                          const string &  queue_name);
        void  AddJobToAffinity(unsigned int  job_id,  unsigned int  aff_id);
        void  FinalizeAffinityDictionaryLoading(void);

    private:
        size_t
        x_RemoveClientFromAffinities(unsigned int          client_id,
                                     const TNSBitVector &  aff_ids,
                                     bool                  is_wait_client,
                                     ECommandGroup         cmd_group);
        void x_DeleteAffinity(unsigned int                   aff_id,
                              map<unsigned int,
                                  SNSJobsAffinity>::iterator found_aff);

    private:
        map< const string *,
             unsigned int,
             SNSTokenCompare >  m_AffinityIDs;  // Aff token -> aff id
        map< unsigned int,
             SNSJobsAffinity >  m_JobsAffinity; // Aff id -> aff token and jobs
        TNSBitVector            m_RemoveCandidates;
        mutable CMutex          m_Lock;         // Lock for the operations

    private:
        unsigned int            m_LastAffinityID;
        CFastMutex              m_LastAffinityIDLock;
        unsigned int            x_GetNextAffinityID(void);
        void                    x_InitLastAffinityID(unsigned int  value);

        TNSBitVector            m_RegisteredAffinities;
                                                // The identifiers of all the
                                                // affinities which are
                                                // currently in the registry
        string  x_PrintSelected(const TNSBitVector &        batch,
                                const CQueue *              queue,
                                const CNSClientsRegistry &  clients_registry,
                                bool                        verbose) const;
        string  x_PrintOne(unsigned int                aff_id,
                           const SNSJobsAffinity &     jobs_affinity,
                           const CQueue *              queue,
                           const CNSClientsRegistry &  clients_registry,
                           bool                        verbose) const;
        void x_AddClient(SNSJobsAffinity &  aff_data,
                         unsigned int       client_id,
                         ECommandGroup      command_group);
        void x_RemoveClient(SNSJobsAffinity &  aff_data,
                            unsigned int       client_id,
                            ECommandGroup      command_group);
        void x_AddWaitClient(SNSJobsAffinity &  aff_data,
                             unsigned int       client_id,
                             ECommandGroup      command_group);
        void x_RemoveWaitClient(SNSJobsAffinity &  aff_data,
                                unsigned int       client_id,
                                ECommandGroup      command_group);
        string x_GetDumpFileName(const string &  dump_dir_name,
                                 const string &  qname) const;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_BDB_AFF__HPP */

