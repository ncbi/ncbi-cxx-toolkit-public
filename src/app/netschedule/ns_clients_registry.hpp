#ifndef NETSCHEDULE_CLIENTS_REGISTRY__HPP
#define NETSCHEDULE_CLIENTS_REGISTRY__HPP

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
 *   NetSchedule clients registry
 *
 */

#include <corelib/ncbimtx.hpp>

#include "ns_clients.hpp"

#include <map>
#include <string>


BEGIN_NCBI_SCOPE


class CQueue;
class CNSAffinityRegistry;
class CJobStatusTracker;


// The CNSClientsRegistry serves all the queue clients.
class CNSClientsRegistry
{
    public:
        CNSClientsRegistry();

        size_t  size(void) const;

        // Called before any command is issued by the client.
        // The client record is created or updated.
        unsigned short  Touch(CNSClientId &          client,
                              CNSAffinityRegistry &  aff_registry,
                              TNSBitVector &         running_jobs,
                              TNSBitVector &         reading_jobs,
                              bool &                 client_was_found,
                              bool &                 session_was_reset,
                              string &               old_session,
                              bool &                 pref_affs_were_reset);

        // Methods to update the client records.
        void  AddToSubmitted(const CNSClientId &  client,
                             size_t               count);
        void  AddToReading(const CNSClientId &  client,
                           unsigned int         job_id);
        void  AddToRunning(const CNSClientId &  client,
                           unsigned int         job_id);
        void  AddToBlacklist(const CNSClientId &  client,
                             unsigned int         job_id);
        void  ClearReading(const CNSClientId &  client,
                           unsigned int         job_id);
        void  ClearReading(unsigned int  job_id);
        void  ClearReadingSetBlacklist(unsigned int  job_id);
        void  ClearExecuting(const CNSClientId &  client,
                             unsigned int         job_id);
        void  ClearExecuting(unsigned int  job_id);
        void  ClearExecutingSetBlacklist(unsigned int  job_id);
        unsigned short  ClearWorkerNode(const CNSClientId &    client,
                                        CNSAffinityRegistry &  aff_registry,
                                        TNSBitVector &         running_jobs,
                                        TNSBitVector &         reading_jobs,
                                        bool &                 client_was_found,
                                        string &               old_session,
                                        bool &                 pref_affs_were_reset);
        TNSBitVector  GetBlacklistedJobs(const CNSClientId &  client) const;
        TNSBitVector  GetBlacklistedJobs(const string &  client_node) const;

        string  PrintClientsList(const CQueue *               queue,
                                 const CNSAffinityRegistry &  aff_registry,
                                 size_t                       batch_size,
                                 bool                         verbose) const;

        void  SetWaiting(const CNSClientId &          client,
                         unsigned short               port,
                         const TNSBitVector &         aff_ids,
                         CNSAffinityRegistry &        aff_registry);
        unsigned short  ResetWaiting(const CNSClientId &    client,
                                     CNSAffinityRegistry &  aff_registry);
        unsigned short  ResetWaiting(const string &         name,
                                     CNSAffinityRegistry &  aff_registry);
        TNSBitVector  GetPreferredAffinities(const CNSClientId &  client) const;
        TNSBitVector  GetPreferredAffinities(const string &  node) const;
        TNSBitVector  GetAllPreferredAffinities(void) const;
        TNSBitVector  GetWaitAffinities(const CNSClientId &  client) const;
        TNSBitVector  GetWaitAffinities(const string &  node) const;
        TNSBitVector  GetRegisteredClients(void) const;
        void  UpdatePreferredAffinities(const CNSClientId &   client,
                                        const TNSBitVector &  aff_to_add,
                                        const TNSBitVector &  aff_to_del);
        bool  UpdatePreferredAffinities(const CNSClientId &   client,
                                        unsigned int          aff_to_add,
                                        unsigned int          aff_to_del);
        void  SetPreferredAffinities(const CNSClientId &   client,
                                     const TNSBitVector &  aff_to_set);
        bool  IsRequestedAffinity(const string &         name,
                                  const TNSBitVector &   aff,
                                  bool                   use_preferred) const;
        bool  IsPreferredByAny(unsigned int  aff_id) const;
        string  GetNodeName(unsigned int  id) const;
        bool  GetAffinityReset(const CNSClientId &   client) const;
        vector< pair< unsigned int, unsigned short > >
                Purge(const CNSPreciseTime &  current_time,
                      const CNSPreciseTime &  timeout,
                      CNSAffinityRegistry &   aff_registry,
                      bool                    is_log);
        void SetBlacklistTimeout(const CNSPreciseTime &  blacklist_timeout)
        { m_BlacklistTimeout = blacklist_timeout; }
        void RegisterSocketWriteError(const CNSClientId &  client);
        void AppendType(const CNSClientId &  client,
                        unsigned int         type_to_append);
        void CheckBlacklistedJobsExisted(const CJobStatusTracker &  tracker);
        int  SetClientData(const CNSClientId &  client,
                           const string &  data, int  data_version);

    private:
        map< string, CNSClient >    m_Clients;  // All the queue clients
                                                // netschedule knows about
                                                // ClientNode -> struct {}
                                                // e.g. service10:9300 - > {}
        mutable CMutex              m_Lock;     // Lock for the map

        // Client IDs support
        unsigned int                m_LastID;
        CFastMutex                  m_LastIDLock;
        TNSBitVector                m_RegisteredClients; // The identifiers
                                                         // of all the clients
                                                         // which are currently
                                                         // in the registry
        TNSBitVector                m_WNAffinities; // Union of all affinities,
                                                    // assigned to all WNodes.

        CNSPreciseTime              m_BlacklistTimeout;

        unsigned int  x_GetNextID(void);

        unsigned short  x_ResetWaiting(CNSClient &            client,
                                       CNSAffinityRegistry &  aff_registry);
        string x_PrintSelected(const TNSBitVector &         batch,
                               const CQueue *               queue,
                               const CNSAffinityRegistry &  aff_registry,
                               bool                         verbose) const;

        void  x_BuildWNAffinities(void);
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS_REGISTRY__HPP */

