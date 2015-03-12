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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Test application for NCBI Berkeley DB library (BDB)
 *
 */

/// @file test_bdb_thr.cpp
/// Illustrates use of concurrent BDB transactions in threads

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>
#include <stdio.h>

#include <db/bdb/bdb_expt.hpp>
#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_blob.hpp>
#include <db/bdb/bdb_map.hpp>
#include <db/bdb/bdb_blobcache.hpp>
#include <db/bdb/bdb_filedump.hpp>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_query.hpp>
#include <db/bdb/bdb_util.hpp>


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/// @internal
struct SThrTestDB : public CBDB_File
{
    CBDB_FieldUint4        thr_id;
    CBDB_FieldUint4        rec_id;

    CBDB_FieldString       txt;

    SThrTestDB()
    {
        DisableNull();
        BindKey("thr_id",   &thr_id);
        BindKey("rec_id",   &rec_id);

        BindData("txt", &txt, 1024);
    }
};


/// @internal
class  CBDB_TestThread : public CThread
{
public:
    CBDB_TestThread(CBDB_Env& env, unsigned thread_id, unsigned recs);
protected:
    /// Overload from CThread (main thread function)
    virtual void* Main(void);
private:
    ~CBDB_TestThread();
    CBDB_TestThread(const CBDB_TestThread&);
    CBDB_TestThread& operator=(const CBDB_TestThread&);
private:
    CBDB_Env&   m_Env;
    unsigned    m_ThreadId;
    unsigned    m_Recs;
};

CBDB_TestThread::CBDB_TestThread(CBDB_Env& env,
                                 unsigned  thread_id,
                                 unsigned  recs)
 : m_Env(env),
   m_ThreadId(thread_id),
   m_Recs(recs)
{
}

CBDB_TestThread::~CBDB_TestThread()
{
}

void* CBDB_TestThread::Main(void)
{
    SThrTestDB db;
    db.SetEnv(m_Env);
    db.Open("data.db", CBDB_RawFile::eReadWrite);

    for (unsigned i = 0; i < m_Recs; ++i) {
        try {
            CBDB_Transaction trans(*db.GetEnv(),
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
            db.SetTransaction(&trans);

            {{
            db.thr_id = m_ThreadId;
            db.rec_id = i;
            db.txt = "test";
            db.Insert();
            }}

            trans.Commit();
        }
        catch (CBDB_ErrnoException& ex)
        {
            if (ex.IsDeadLock()) {

                // dead lock transaction is a Berkeley DB reality which can
                // happen when two or more threads are writing in the same
                // file concurrently.
                //
                // typically we want to abort current transaction
                // and repeat it with the same data.
                //
                // In this case we simply ignore the error.

                NcbiCerr <<
                 "Dead lock situation detected. Transaction aborted!"
                 << NcbiEndl;
            } else {
                throw;
            }
        }
    } // for

    return (void*)0;
}

//////////////////////////////////////////////////////////////////
//
// Structure implements simple database table with integer id primary key
//
//


////////////////////////////////
/// Test application
///
/// @internal
///
class CBDB_TestThreads : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_TestThreads::Init(void)
{
    SetDiagTrace(eDT_Enable);

    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    SetDiagPostFlag(eDPF_Trace);


    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("test_bdb_threads",
                              "test BDB library with threads");

    arg_desc->AddOptionalKey("recs",
                             "recs",
                             "Number of records to load per thread",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("threads",
                             "threads",
                             "Number of concurrent threads",
                             CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}


int CBDB_TestThreads::Run(void)
{
    const CArgs& args = GetArgs();

    NcbiCout << "Initialize transactional environment..." << flush;

    string path = "./data_test_thr";
    path = CDirEntry::AddTrailingPathSeparator(path);

    {{
        CDir dir(path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}


    CBDB_Env env;

    // Error file for Berkeley DB
    string err_file = path + "err_thr_test.log";
    env.OpenErrFile(err_file.c_str());

    env.SetLogFileMax(50 * 1024 * 1024);

    // use in-memory logging to improve performance (warning: not durable)
    env.SetLogInMemory(true);
    env.SetLogBSize(100 * 1024 * 1024);

    env.SetLogAutoRemove(true);

    env.SetCacheSize(5 * 1024 * 1024);

    env.OpenWithTrans(path.c_str(), CBDB_Env::eThreaded /*| CBDB_Env::eRunRecovery*/);
    env.SetDirectDB(true);
    env.SetDirectLog(true);
    env.SetLockTimeout(10 * 1000000); // 10 sec
    env.SetTasSpins(5);

    SThrTestDB db;
    db.SetEnv(env);
    db.Open("data.db", CBDB_RawFile::eCreate);


    NcbiCout << "Ok." << NcbiEndl;

    try
    {
        unsigned kProcessingRecs = 100000;
        if (args["recs"]) {
            kProcessingRecs = args["recs"].AsInteger();
        }

        unsigned kThreadCount = 2;
        if (args["threads"]) {
            kThreadCount = args["threads"].AsInteger();
        }


        vector<CRef<CThread> > thread_list;
        thread_list.reserve(kThreadCount);

        CStopWatch sw(CStopWatch::eStart);

        NcbiCout << "Starting " << kThreadCount << " threads..." << flush;
        for (unsigned i = 0; i < kThreadCount; ++i) {
            CRef<CThread> bdb_thread(
                new CBDB_TestThread(env, i, kProcessingRecs));
            thread_list.push_back(bdb_thread);
            bdb_thread->Run();
        } // for
        NcbiCout << "Ok." << NcbiEndl;

        NcbiCout << "Waiting for threads to finish..." << flush;

        NON_CONST_ITERATE(vector<CRef<CThread> >, it, thread_list) {
            CRef<CThread> bdb_thread(*it);
            bdb_thread->Join();
        }

        NcbiCout << "Ok." << NcbiEndl;

        double elapsed = sw.Elapsed();
        unsigned total_recs = kProcessingRecs * kThreadCount;
        double rate = total_recs / elapsed;
        double rate_per_thread = kProcessingRecs / elapsed;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Elapsed: " << elapsed << " secs." << NcbiEndl;
        NcbiCout << "Transaction rate: " << rate << " recs/secs." << NcbiEndl;
        NcbiCout << "Rate per thread: " << rate_per_thread << " recs/secs."
                 << NcbiEndl;
    }
    catch (CBDB_ErrnoException& ex)
    {
        cout << "Error! DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        cout << "Error! DBD library exception:" << ex.what();
        return 1;
    }

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBDB_TestThreads().AppMain(argc, argv);
}
