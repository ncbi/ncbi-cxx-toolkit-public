/* $Id$
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
 * Author:  Sergey Satskiy
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/simple/sdbapi.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/resource_info.hpp>

#include "nst_dbconnection_thread.hpp"
#include "nst_precise_time.hpp"
#include "nst_database.hpp"


BEGIN_NCBI_SCOPE


static const CNSTPreciseTime    gs_MinRetryToConnectTimeout(1.0);
static const CNSTPreciseTime    gs_MinRetryToCreateTimeout(100.0);


CNSTDBConnectionThread::CNSTDBConnectionThread(bool &  connected,
                                               CDatabase * &  db,
                                               CNSTDatabase *  db_wrapper,
                                               CFastMutex &  db_lock) :
    m_Connected(connected), m_Database(db),
    m_DBWrapper(db_wrapper),
    m_DbLock(db_lock),
    m_StopSignal(0, 10000000)
{}


CNSTDBConnectionThread::~CNSTDBConnectionThread()
{}


void CNSTDBConnectionThread::Stop(void)
{
    m_StopFlag.Add(1);
    Wakeup();
}


void CNSTDBConnectionThread::Wakeup(void)
{
    m_StopSignal.Post();
}


void *  CNSTDBConnectionThread::Main(void)
{
    SetCurrentThreadName("netstoraged_dbc");
    for (;;) {
        try {
            if (m_StopFlag.Get() != 0)
                break;
            m_StopSignal.Wait();

            // There are two cases:
            // - could not decrypt the DB password
            // - connection is lost
            if (m_Database == NULL)
                x_CreateDatabase();
            x_RestoreConnection();
        } catch (const exception &  ex) {
            ERR_POST("Unexpected exception in the DB restore "
                     "connection thread Main() function: " << ex <<
                     " Ignore and continue.");
        } catch (...) {
            ERR_POST("Unexpected unknown exception in the DB restore "
                     "connection thread Main() function. Ignore and continue.");
        }
    }
    return NULL;
}


void CNSTDBConnectionThread::x_RestoreConnection(void)
{
    m_DbLock.Lock();
    if (m_Database == NULL) {
        m_DbLock.Unlock();
        return;
    }
    m_DbLock.Unlock();

    while (m_StopFlag.Get() == 0 && m_Connected == false) {
        CNSTPreciseTime     start = CNSTPreciseTime::Current();

        try {
            {
                CFastMutexGuard     guard(m_DbLock);
                m_Database->Close();
                m_Database->Connect();
                m_Connected = true;
            }
            LOG_POST(Note << "Database connection has been restored");
            return;
        } catch (const exception &  ex) {
            ERR_POST("Exception while restoring DB connection: " << ex <<
                     " Ignore and continue.");
        } catch (...) {
            ERR_POST("Unknown exception while restoring DB connection. "
                     "Ignore and continue.");
        }

        CNSTPreciseTime     loop_time = CNSTPreciseTime::Current() - start;
        if (loop_time < gs_MinRetryToConnectTimeout)
            m_StopSignal.TryWait(1, 0);
     }
}


// The function serves the case when there was a problem of decrypting the
// database password parameter. The server should try to decrypt the password
// every 100 seconds.
void CNSTDBConnectionThread::x_CreateDatabase(void)
{
    if (m_Database != NULL)
        return;

    while (m_StopFlag.Get() == 0) {
        CNSTPreciseTime     start = CNSTPreciseTime::Current();

        try {
            // Reset the key file cache
            CNcbiEncrypt::Reload();

            // false -> this not initialization; this is a periodic try to
            // decrypt the database password
            m_DBWrapper->x_CreateDatabase(false);
            if (m_Database != NULL) {
                LOG_POST(Note << "Encrypted database password "
                                 "has been decrypted");
                return;
            }
        } catch (const exception &  ex) {
            ERR_POST("Exception while decrypting database password: " << ex <<
                     " Ignore and continue.");
        } catch (...) {
            ERR_POST("Unknown exception while decrypting database password. "
                     "Ignore and continue.");
        }

        CNSTPreciseTime     loop_time = CNSTPreciseTime::Current() - start;
        if (loop_time < gs_MinRetryToCreateTimeout)
            m_StopSignal.TryWait(100, 0);
     }
}

END_NCBI_SCOPE


