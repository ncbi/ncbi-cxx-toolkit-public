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
 * File Description: NetSchedule save/restore state utilities
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "ns_restore_state.hpp"
#include "ns_types.hpp"
#include "ns_queue.hpp"
#include "ns_server.hpp"

#include <sstream>


BEGIN_NCBI_SCOPE

// The serialization of the queue state is done at the moment a certain
// property is updated. This approach works better in case of a server crash.
// On the other hand the property change may happen simultaneously from many
// threads and thus there is a competition with file updaes.
CFastMutex      g_PausedQueueLock;
CFastMutex      g_RefuseSubmitLock;


bool IsPauseStatusValid(int  value)
{
    return value == CQueue::eNoPause ||
           value == CQueue::ePauseWithPullback ||
           value == CQueue::ePauseWithoutPullback;
}


void SerializePauseState(CNetScheduleServer *  server)
{
    if (!server->GetDiskless()) {
        SerializePauseState(server->GetDataPath(),
                            server->GetPauseQueues());
    }
}


void SerializePauseState(const string &  data_path,
                         const map<string, int> &  paused_queues)
{
    string  path = CFile::MakePath(data_path, kPausedQueuesFilesName);
    CFastMutexGuard     guard(g_PausedQueueLock);

    if (paused_queues.empty()) {
        // Remove the file
        try {
            CFile   pause_state_file(path);

            if (pause_state_file.Exists())
                pause_state_file.Remove();
        } catch (...) {
            ERR_POST("Error removing the paused queue file. "
                     "It may lead to an incorrect pause state "
                     "of queues when the server restarts.");
        }
        return;
    }

    // Overwrite or create the file
    try {
        CFileIO     f;
        char        buffer[32];
        f.Open(path, CFileIO_Base::eCreate, CFileIO_Base::eReadWrite);
        for (map<string, int>::const_iterator k = paused_queues.begin();
             k != paused_queues.end(); ++k) {
            f.Write(k->first.c_str(), k->first.size());
            int cnt = sprintf(buffer, " %d\n", k->second);
            f.Write(buffer, cnt);
        }
        f.Close();
    } catch (const exception &  exc) {
        ERR_POST("Error saving the paused queue file: " << exc.what() <<
                 "\nIt may lead to an incorrect pause state "
                 "of queues when the server restarts.");
    } catch (...) {
        ERR_POST("Unknown error saving the paused queue file. "
                 "It may lead to an incorrect pause state "
                 "of queues when the server restarts.");
    }
}


void SerializeRefuseSubmitState(CNetScheduleServer *  server)
{
    if (!server->GetDiskless()) {
        SerializeRefuseSubmitState(server->GetDataPath(),
                                   server->GetRefuseSubmits(),
                                   server->GetRefuseSubmitQueues());
    }
}


void SerializeRefuseSubmitState(const string &  data_path,
                                bool server_refuse_state,
                                const vector<string> refuse_submit_queues)
{
    string  path = CFile::MakePath(data_path, kRefuseSubmitFileName);
    CFastMutexGuard     guard(g_RefuseSubmitLock);

    if (refuse_submit_queues.empty() && !server_refuse_state) {
        // Remove the file
        try {
            CFile   refuse_submit_queues_file(path);

            if (refuse_submit_queues_file.Exists())
                refuse_submit_queues_file.Remove();
        } catch (...) {
            ERR_POST("Error removing the refuse submit file. "
                     "It may lead to an incorrect refuse submit state "
                     "of queues when the server restarts.");
        }
        return;
    }

    // Overwrite or create the file
    try {
        CFileIO     f;
        f.Open(path, CFileIO_Base::eCreate, CFileIO_Base::eReadWrite);

        if (server_refuse_state) {
            const char *    data = "[server]\n";
            f.Write(data, strlen(data));
        }

        for (vector<string>::const_iterator k = refuse_submit_queues.begin();
             k != refuse_submit_queues.end(); ++k) {
            f.Write(k->c_str(), k->size());
            f.Write("\n", 1);
        }
        f.Close();
    } catch (const exception &  exc) {
        ERR_POST("Error saving the refuse submit file: " << exc.what() <<
                 "\nIt may lead to an incorrect refuse submit state "
                 "of queues when the server restarts.");
    } catch (...) {
        ERR_POST("Unknown error saving the refuse submit file. "
                 "It may lead to an incorrect refuse submit state "
                 "of queues when the server restarts.");
    }
}


map<string, int> DeserializePauseState(const string &  data_path,
                                       bool  diskless)
{
    map<string, int>    paused_queues;

    if (!diskless) {
        string              path = CFile::MakePath(data_path,
                                                   kPausedQueuesFilesName);
        CFile               pause_state_file(path);
        bool                reported = false;

        if (pause_state_file.Exists()) {
            ifstream    queue_file(path);
            string      line;

            while (std::getline(queue_file, line)) {
                if (line.empty())
                    continue;

                std::istringstream  iss(line);
                string              queue_name;
                int                 pause_mode;

                if (!(iss >> queue_name >> pause_mode)) {
                    if (!reported) {
                        ERR_POST("Error reading queue name and its pause status "
                                 "from " << path << ". Ignore and continue.");
                        reported = true;
                    }
                    continue;
                }

                if (!IsPauseStatusValid(pause_mode)) {
                    ERR_POST("Invalid pause mode " << pause_mode <<
                             " for queue " << queue_name <<
                             " while reading from " << path <<
                             ". Ignore and continue.");
                } else {
                    paused_queues[queue_name] = pause_mode;
                }
            }
        }
    }
    return paused_queues;
}


vector<string> DeserializeRefuseSubmitState(const string &  data_path,
                                            bool  diskless)
{
    vector<string>      refuse_submit_queues;

    if (!diskless) {
        string              path = CFile::MakePath(data_path,
                                                   kRefuseSubmitFileName);
        CFile               refuse_submit_file(path);

        if (refuse_submit_file.Exists()) {
            ifstream    refuse_file(path);
            string      line;

            while (std::getline(refuse_file, line)) {
                if (line.empty())
                    continue;

                refuse_submit_queues.push_back(line);
            }
        }
    }
    return refuse_submit_queues;
}

END_NCBI_SCOPE
