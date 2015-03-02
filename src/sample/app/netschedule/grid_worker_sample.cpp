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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/grid_worker_app.hpp>

#include <vector>
#include <algorithm>

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////

class CSampleJobCleanupListener : public IWorkerNodeCleanupEventListener
{
public:
    CSampleJobCleanupListener(const char* cleanup_type) :
        m_CleanupType(cleanup_type)
    {
    }
    virtual void HandleEvent(EWorkerNodeCleanupEvent cleanup_event);

private:
    const char* m_CleanupType;
};

void CSampleJobCleanupListener::HandleEvent(
    EWorkerNodeCleanupEvent cleanup_event)
{
    ERR_POST(Info << m_CleanupType << (cleanup_event ==
        IWorkerNodeCleanupEventListener::eRegularCleanup ?
            " regular clean-up" : " clean-up during force exit"));
}

/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///
class CSampleJob : public IWorkerNodeJob
{
public:
    CSampleJob(const IWorkerNodeInitContext& context)
    {
        const IRegistry& reg = context.GetConfig();
        m_Param = reg.GetString("sample", "parameter", "no parameter" );
        m_Iters = reg.GetInt("sample", "iterations", 1000,0,IRegistry::eReturn );
        m_SleepSec = reg.GetInt("sample", "sleep_sec", 2,0,IRegistry::eReturn );;

        context.GetCleanupEventSource()->AddListener(
            new CSampleJobCleanupListener("Worker node"));
    }

    virtual ~CSampleJob() {} 

    int Do(CWorkerNodeJobContext& context)
    {
        context.GetCleanupEventSource()->AddListener(
            new CSampleJobCleanupListener("Job-do"));

        ERR_POST( Info << context.GetJobKey() + " " + context.GetJobInput());
        ERR_POST( Info << Note << "This parameter is read from a config file: "
                               << m_Param);

        // 1. Get an input data from the client
        //    (You can use ASN.1 de-serialization here)
        //
        CNcbiIstream& is = context.GetIStream();
        CNcbiOstream& os = context.GetOStream();
        
        string output_type;
        is >> output_type; // could be "doubles" or "html"
        ERR_POST( Info << "Output type: " << output_type);
        int count;
        is >> count;
        vector<double> dvec;
        dvec.reserve(count);
        ERR_POST( Info << "Getting " << count << " doubles from stream...");
        for (int i = 0; i < count; ++i) {
            if (!is.good()) {
                ERR_POST( "Input stream error. Index : " << i );

                // If anything bad happened, throw an exception
                // and its message will be delivered to the client.
                throw runtime_error("Worker node input stream error"); 
            }
            // Don't forget to check if shutdown has been requested
            if (count % 1000 == 0)
                switch (context.GetShutdownLevel()) {
                case CNetScheduleAdmin::eShutdownImmediate:
                case CNetScheduleAdmin::eDie:
                    // Either this job is not needed anymore (canceled,
                    // expired or already executed elsewhere), or the
                    // server is going down and the job's execution
                    // should be gracefully (yet urgently) aborted and
                    // the job returned back to the network queue
                    // for execution by other worker node instances.
                    context.ReturnJob();
                    return 1;
                default:
                    break;
                }
            double d;
            is >> d;
            dvec.push_back(d);
        }
       
        // 2. Doing some time consuming job here
        // Well behaved algorithm checks from time to time if 
        // immediate shutdown has been requested and gracefully return 
        // without calling context.CommitJob()
        //
        for (int i = 0; i < m_Iters; ++i) {
            switch (context.GetShutdownLevel()) {
            case CNetScheduleAdmin::eShutdownImmediate:
            case CNetScheduleAdmin::eDie:
                // Either this job is not needed anymore (canceled,
                // expired or already executed elsewhere), or the
                // server is going down and the job's execution
                // should be gracefully (yet urgently) aborted and
                // the job returned back to the network queue
                // for execution by other worker node instances.
                context.ReturnJob();
                return 1;
            default:
                break;
            }
            context.PutProgressMessage("Iteration " + NStr::IntToString(i+1) +
                                       " from " + NStr::IntToString(m_Iters));
            SleepSec(m_SleepSec);            
        }
        sort(dvec.begin(), dvec.end());


        // 3. Return the result to the client 
        //    (You can use ASN.1 serialization here)
        //
        //        CNcbiOstream& os = context.GetOStream();
        if (output_type == "html") 
            os << "<html><head><title>"
                  "Sample Grid Worker Result Page"
                  "</title></head><body>"
                  "<p>Sample Grid Worker Result</p>";
            
        else
            os << dvec.size() << ' ';

        for (int i = 0; i < count; ++i) {
            if (!os.good()) {
                ERR_POST( "Output stream error. Index : " << i );
                throw runtime_error("Worker node output stream error"); 
            }
            os << dvec[i] << ' ';
        }
        if (output_type == "html") 
            os << "</body></html>";

        // 4. Indicate that the job is done and the result 
        // can be delivered to the client.
        //
        context.CommitJob();

        ERR_POST( Info << "Job " << context.GetJobKey() << " is done.");
        return 0;
    }
private:
    string m_Param;
    int m_Iters;
    int m_SleepSec;
};

// if an Idle task is needed then an implementation of IWorkerNodeIdelTask
// should be created.
class CSampleIdleTask : public IWorkerNodeIdleTask
{
public:
    CSampleIdleTask(const IWorkerNodeInitContext& context) 
        : m_Count(0)
    {
    }
    virtual ~CSampleIdleTask() {}

    virtual void Run(CWorkerNodeIdleTaskContext& context)
    {
        ERR_POST( Info << Note << "Staring idle task...");
        for (int i = 0; i < 10; ++i) {
            ERR_POST( Info << Note << "Idle task: iteration: " << i);
            if (context.IsShutdownRequested() )
                break;
            SleepSec(2);
        }
        if (++m_Count % 3 == 0)
            context.SetRunAgain();
        ERR_POST( Info << Note << "Stopping idle task...");
    }
private:
    int m_Count;
};


/////////////////////////////////////////////////////////////////////////////
//  Routine magic spells

// Use this macros to implement the main function for the CSampleJob version 1.0.1
// with idle task CSampleIdleTask
//NCBI_WORKERNODE_MAIN_EX(CSampleJob, CSampleIdleTask, 1.0.1);

// if you don't need an Idle task just use this macros.
NCBI_WORKERNODE_MAIN(CSampleJob, 1.0.1);
