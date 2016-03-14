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
class CNSNotificationList;


// The CNSClientsRegistry serves all the queue clients.
class CNSClientsRegistry
{
    public:
        CNSClientsRegistry();
        void SetRegistries(CNSAffinityRegistry *  aff_registry,
                           CNSNotificationList *  notif_registry);

        size_t  size(void) const
        { return m_Clients.size(); }

        // Called before any command is issued by the client.
        // The client record is created or updated.
        void  Touch(CNSClientId &          client,
                    TNSBitVector &         running_jobs,
                    TNSBitVector &         reading_jobs,
                    bool &                 client_was_found,
                    bool &                 session_was_reset,
                    string &               old_session,
                    bool &                 had_wn_pref_affs,
                    bool &                 had_reader_pref_affs);

        // Methods to update the client records.
        void  MarkAsAdmin(const CNSClientId &  client);
        void  AddToSubmitted(const CNSClientId &  client,
                             size_t               count);
        void  RegisterJob(const CNSClientId &  client,
                          unsigned int         job_id,
                          ECommandGroup        cmd_group);
        void  RegisterBlacklistedJob(const CNSClientId &  client,
                                     unsigned int         job_id,
                                     ECommandGroup        cmd_group);
        void  UnregisterJob(const CNSClientId &  client,
                            unsigned int         job_id,
                            ECommandGroup        cmd_group);
        void  UnregisterJob(unsigned int   job_id,
                            ECommandGroup  cmd_group);
        void  MoveJobToBlacklist(const CNSClientId &  client,
                                 unsigned int         job_id,
                                 ECommandGroup        cmd_group);
        void  MoveJobToBlacklist(unsigned int   job_id,
                                 ECommandGroup  cmd_group);
        TNSBitVector  GetBlacklistedJobs(const CNSClientId &  client,
                                         ECommandGroup        cmd_group) const;
        TNSBitVector  GetBlacklistedJobs(const string &  client_node,
                                         ECommandGroup   cmd_group) const;
        string  PrintClientsList(const CQueue *               queue,
                                 size_t                       batch_size,
                                 bool                         verbose) const;
        void  SetNodeWaiting(const CNSClientId &   client,
                             unsigned short        port,
                             const TNSBitVector &  aff_ids,
                             ECommandGroup         cmd_group);
        TNSBitVector  GetPreferredAffinities(const CNSClientId &  client,
                                             ECommandGroup        cmd_group) const;
        TNSBitVector  GetPreferredAffinities(const string &  node,
                                             ECommandGroup   cmd_group) const;

        TNSBitVector  GetAllPreferredAffinities(ECommandGroup  cmd_group) const;
        TNSBitVector  GetWaitAffinities(const CNSClientId &  client,
                                        ECommandGroup        cmd_group) const;
        TNSBitVector  GetWaitAffinities(const string &  node,
                                        ECommandGroup   cmd_group) const;
        TNSBitVector  GetRegisteredClients(void) const;
        void  UpdatePreferredAffinities(const CNSClientId &   client,
                                        const TNSBitVector &  aff_to_add,
                                        const TNSBitVector &  aff_to_del,
                                        ECommandGroup         cmd_group);
        bool  UpdatePreferredAffinities(const CNSClientId &   client,
                                        unsigned int          aff_to_add,
                                        unsigned int          aff_to_del,
                                        ECommandGroup         cmd_group);
        void  SetPreferredAffinities(const CNSClientId &   client,
                                     const TNSBitVector &  aff_to_set,
                                     ECommandGroup         cmd_group);
        bool  IsRequestedAffinity(const string &         name,
                                  const TNSBitVector &   aff,
                                  bool                   use_preferred,
                                  ECommandGroup          cmd_group) const;
        bool  IsPreferredByAny(unsigned int           aff_id,
                               ECommandGroup          cmd_group) const;
        string  GetNodeName(unsigned int  id) const;
        bool  GetAffinityReset(const CNSClientId &   client,
                               ECommandGroup         cmd_group) const;
        void  StaleNodes(const CNSPreciseTime &  current_time,
                         const CNSPreciseTime &  wn_timeout,
                         const CNSPreciseTime &  reader_timeout,
                         bool                    is_log);
        void  Purge(const CNSPreciseTime &  current_time,
                    const CNSPreciseTime &  timeout_worker_node,
                    unsigned int            min_worker_nodes,
                    const CNSPreciseTime &  timeout_admin,
                    unsigned int            min_admins,
                    const CNSPreciseTime &  timeout_submitter,
                    unsigned int            min_submitters,
                    const CNSPreciseTime &  timeout_reader,
                    unsigned int            min_readers,
                    const CNSPreciseTime &  timeout_unknown,
                    unsigned int            min_unknowns,
                    bool                    is_log);
        bool WasGarbageCollected(const CNSClientId &  client,
                                 ECommandGroup        cmd_group) const;
        void SetBlacklistTimeouts(
                                const CNSPreciseTime &  blacklist_timeout,
                                const CNSPreciseTime &  read_blacklist_timeout)
        { m_BlacklistTimeout = blacklist_timeout;
          m_ReadBlacklistTimeout = read_blacklist_timeout; }
        void RegisterSocketWriteError(const CNSClientId &  client);
        void SetLastScope(const CNSClientId &  client);
        void AppendType(const CNSClientId &  client,
                        unsigned int         type_to_append);
        void GCBlacklistedJobs(const CJobStatusTracker &  tracker,
                               ECommandGroup              cmd_group);
        int  SetClientData(const CNSClientId &  client,
                           const string &  data, int  data_version);
        bool CancelWaiting(CNSClient &  client, ECommandGroup  cmd_group,
                           bool  touch_notif_registry = true);
        bool CancelWaiting(const CNSClientId &  client,
                           ECommandGroup        cmd_group);
        bool CancelWaiting(const string &  name,
                           ECommandGroup   cmd_group,
                           bool            touch_notif_registry = true);
        void ClearClient(const CNSClientId &    client,
                         TNSBitVector &         running_jobs,
                         TNSBitVector &         reading_jobs,
                         bool &                 client_was_found,
                         string &               old_session,
                         bool &                 had_wn_pref_affs,
                         bool &                 had_reader_pref_affs);
        void ClearClient(const string &         node_name,
                         TNSBitVector &         running_jobs,
                         TNSBitVector &         reading_jobs,
                         bool &                 client_was_found,
                         string &               old_session,
                         bool &                 had_wn_pref_affs,
                         bool &                 had_reader_pref_affs);
        void ClearOnTimeout(CNSClient &     client,
                            const string &  client_node,
                            bool            is_log,
                            ECommandGroup   cmd_group);

    private:
        map< string, CNSClient >    m_Clients;  // All the queue clients
                                                // netschedule knows about
                                                // ClientNode -> struct {}
                                                // e.g. service10:9300 - > {}
        // Garbage collected worker nodes which had affinities reset
        set< string >               m_GCWNodeClients;
        // Garbage collected readers which had preferred affinities
        set< string >               m_GCReaderClients;
        mutable CMutex              m_Lock;     // Lock for the map

        // Client IDs support
        unsigned int                m_LastID;
        CFastMutex                  m_LastIDLock;
        TNSBitVector                m_RegisteredClients; // The identifiers
                                                         // of all the clients
                                                         // which are currently
                                                         // in the registry
        // Union of all affinities assigned to all WNodes
        TNSBitVector                m_WNodeAffinities;
        // Union of all affinities assigned to all readers
        TNSBitVector                m_ReaderAffinities;

        CNSPreciseTime              m_BlacklistTimeout;
        CNSPreciseTime              m_ReadBlacklistTimeout;

        CNSAffinityRegistry *       m_AffRegistry;
        CNSNotificationList *       m_NotifRegistry;

        unsigned int  x_GetNextID(void);

        string x_PrintSelected(const TNSBitVector &         batch,
                               const CQueue *               queue,
                               bool                         verbose) const;

        void x_BuildAffinities(ECommandGroup  cmd_group);
        void x_ClearClient(const string &     node_name,
                           CNSClient &        client,
                           TNSBitVector &     jobs,
                           bool &             had_pref_affs,
                           ECommandGroup      cmd_group);
        bool x_CouldBeStale(const CNSPreciseTime &  current_time,
                            const CNSPreciseTime &  timeout,
                            const CNSClient &       client,
                            ECommandGroup           cmd_group);

        // GC methods
        void  x_PurgeWNodesAndReaders(
                        const CNSPreciseTime &  current_time,
                        const CNSPreciseTime &  timeout_worker_node,
                        unsigned int            min_worker_nodes,
                        const CNSPreciseTime &  timeout_reader,
                        unsigned int            min_readers,
                        bool                    is_log);
        void  x_PurgeAdmins(const CNSPreciseTime &  current_time,
                            const CNSPreciseTime &  timeout_admin,
                            unsigned int            min_admins,
                            bool                    is_log);
        void  x_PurgeSubmitters(const CNSPreciseTime &  current_time,
                                const CNSPreciseTime &  timeout_submitter,
                                unsigned int            min_submitters,
                                bool                    is_log);
        void  x_PurgeUnknowns(const CNSPreciseTime &  current_time,
                              const CNSPreciseTime &  timeout_unknown,
                              unsigned int            min_unknowns,
                              bool                    is_log);
        void  x_PurgeInactiveClients(const CNSPreciseTime &  current_time,
                                     const CNSPreciseTime &  timeout,
                                     unsigned int            min_clients,
                                     unsigned int            client_type,
                                     bool                    is_log);
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS_REGISTRY__HPP */

