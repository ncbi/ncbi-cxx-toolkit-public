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


class CNetScheduleHandler;
class CQueue;


// The CNSClientsRegistry serves all the queue clients.
class CNSClientsRegistry
{
    public:
        // Called before any command is issued by the client.
        // The client record is created or updated.
        void  Touch(const CNSClientId &  client,
                    CQueue *             queue);

        // Methods to update the client records.
        void  AddToSubmitted(const CNSClientId &  client,
                             unsigned int         job_id);
        void  AddToSubmitted(const CNSClientId &  client,
                             unsigned int         start_job_id,
                             unsigned int         number_of_jobs);
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
        void  ClearWorkerNode(const CNSClientId &  client,
                              CQueue *             queue);
        TNSBitVector  GetBlacklistedJobs(const CNSClientId &  client);

        void  PrintClientsList(const CQueue *         queue,
                               CNetScheduleHandler &  handler) const;

        void  SetWaitPort(const CNSClientId &  client,
                          unsigned short       port);
        unsigned short  GetAndResetWaitPort(const CNSClientId &  client);

    private:
        map< string, CNSClient >    m_Clients;  // All the queue clients
                                                // netschedule knows about
                                                // ClientNode -> struct {}
                                                // e.g. service10:9300 - > {}
        mutable CRWLock             m_Lock;     // Lock for the map
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS_REGISTRY__HPP */

