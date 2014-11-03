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

#include "nst_dbconnection_thread.hpp"
#include "nst_precise_time.hpp"

#include <sys/prctl.h>


BEGIN_NCBI_SCOPE


static const CNSTPreciseTime    gs_MinRetryTimeout(1.0);


CNSTDBConnectionThread::CNSTDBConnectionThread(bool &  connected,
                                               CDatabase *  db) :
    m_Connected(connected), m_Database(db),
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
    prctl(PR_SET_NAME, "netstoraged_dbc", 0, 0, 0);
    for (;;) {
        try {
            if (m_StopFlag.Get() != 0)
                break;
            m_StopSignal.Wait();
            x_RestoreConnection();
        } catch (const CException &  ex) {
            ERR_POST("Unexpected toolkit exception in the DB restore "
                     "connection thread Main() function: " + string(ex.what()) +
                     " -- Ignore and continue.");
        } catch (const exception &  ex) {
            ERR_POST("Unexpected C++ std exception in the DB restore "
                     "connection thread Main() function: " + string(ex.what()) +
                     " -- Ignore and continue.");
        } catch (...) {
            ERR_POST("Unexpected unknown exception in the DB restore "
                     "connection thread Main() function. Ignore and continue.");
        }
    }
    return NULL;
}


void CNSTDBConnectionThread::x_RestoreConnection(void)
{
    // Paranoya check: this must never happened
    if (m_Database == NULL)
        return;

    while (m_StopFlag.Get() == 0 && m_Connected == false) {
        CNSTPreciseTime     start = CNSTPreciseTime::Current();

        try {
            m_Database->Close();
            m_Database->Connect();
            m_Connected = true;
            LOG_POST("Database connection has been restored");
            return;
        } catch (const CException &  ex) {
            ERR_POST("Toolkit exception while restoring DB connection: " +
                     string(ex.what()) + " -- Ignore and continue.");
        } catch (const exception &  ex) {
            ERR_POST("C++ std exception while restoring DB connection: " +
                     string(ex.what()) + " -- Ignore and continue.");
        } catch (...) {
            ERR_POST("Unknown exception while restoring DB connection. "
                     "Ignore and continue.");
        }

        CNSTPreciseTime     loop_time = CNSTPreciseTime::Current() - start;
        if (loop_time < gs_MinRetryTimeout)
            m_StopSignal.TryWait(1, 0);
     }
}

END_NCBI_SCOPE


