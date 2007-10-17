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
 * File Description:  NetSchedule check utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistre.hpp>

#include <connect/services/netschedule_api.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/// NetSchedule check application
///
/// @internal
///
class CNetScheduleCheck : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
    int Run(CNetScheduleAPI& nc_client);
};



void CNetScheduleCheck::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule Check.");
    
    arg_desc->AddPositional("service", 
        "NetSchedule host or service name. Format: service|host:port", 
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("qclass",
                             "queue",
                             "Queue class name",
                             CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}

class CQueueFinder : public INetServiceAPI::ISink
{
public:
    typedef map<string, int> TQueueCounter;

    CQueueFinder(const string& requested_queue) 
        : m_ServerCounter(0), m_RequestedQueue(requested_queue) {}
    virtual ~CQueueFinder() {}

    virtual CNcbiOstream& GetOstream(CNetServerConnector& conn)
    {
        ++m_ServerCounter;
        m_Strm.reset(new CNcbiOstrstream);
        return *m_Strm;
    }
    virtual void EndOfData(CNetServerConnector& conn) 
    {
        string squeues = string(CNcbiOstrstreamToString(*m_Strm));
        m_Strm.release();
        list<string> queues;
        NStr::Split(squeues, ";",queues);
        ITERATE(list<string>, it, queues) {
            if(m_QueueCounter.find(*it) == m_QueueCounter.end())
                m_QueueCounter[*it] = 1;
            else
                m_QueueCounter[*it]++;
        }
    }

    string GetQueueClass() const
    {
        TQueueCounter::const_iterator i = m_QueueCounter.find(m_RequestedQueue);  
        if ( i != m_QueueCounter.end() ) {
            if ( i->second == m_ServerCounter )
                return m_RequestedQueue;
        }
        ITERATE(TQueueCounter, it, m_QueueCounter) {
            if (it->second == m_ServerCounter)
                return it->first;
        }
        return kEmptyStr; 
    }

private: 
    TQueueCounter m_QueueCounter;
    auto_ptr<CNcbiOstrstream> m_Strm;
    int m_ServerCounter;
    string m_RequestedQueue;
};

int CNetScheduleCheck::Run(void)
{
    const CArgs& args = GetArgs();

    const string& service  = args["service"].AsString();

    string queue = "test";
    if (args["qclass"]) {  
        queue = args["qclass"].AsString(); 
    }
    CQueueFinder finder(queue);


    CNetScheduleAPI cln1(service, "netschedule_admin", queue);
    CNetScheduleAdmin admin = cln1.GetAdmin();
    admin.GetQueueList(finder);
    string qclass = finder.GetQueueClass();
    if( qclass.empty()) {
        ERR_POST("Could not find siutable qclass for the given NetSchedule service: " << service);
        return 1;
    }

    queue = "netschedule_check_queue";
    admin.CreateQueue(queue, qclass);
    
    CNetScheduleAPI cln(service, "netschedule_admin", queue);

    int ret = Run(cln);
    admin.DeleteQueue(queue);

    return ret;
}

int CNetScheduleCheck::Run(CNetScheduleAPI& nc)
{
    CNetScheduleSubmitter submitter = nc.GetSubmitter();
    CNetScheduleExecuter executer = nc.GetExecuter();

    const string input = "Hello ";
    const string output = "DONE ";
    CNetScheduleJob job(input);
    submitter.SubmitJob(job);

    while ( true ) {
        //SleepSec(1);
        
        CNetScheduleJob job1;
        bool job_exists = executer.WaitJob(job1,5,9898);
        if (job_exists) {
            if (job1.job_id != job.job_id)
                executer.ReturnJob(job1.job_id);
            else {
                if (job1.input != job.input) {
                    job1.error_msg = "Job's (" + job1.job_id + 
                        ") input does not match.(" + job.input + ") ["+ job1.input +"]";
                    executer.PutFailure(job1);
                } else {
                    job1.output = output;
                    job1.ret_code = 0;
                    executer.PutResult(job1);
                }
                break;
            }
        }
    }

    bool check_again = true;
    int ret = 0;
    string err;
    while(check_again) {
        check_again = false;
        //SleepSec(1);

        CNetScheduleAPI::EJobStatus status = submitter.GetJobDetails(job);
        switch(status) {
        
        case CNetScheduleAPI::eJobNotFound:
            ret = 210;
            err = "Job (" + job.job_id +") is lost.";
            break;
        case CNetScheduleAPI::eReturned:
            ret = 211;
            submitter.CancelJob(job.job_id);
            err = "Job (" + job.job_id +") is returned.";
            break;
        case CNetScheduleAPI::eCanceled:
            ret = 212;
            err = "Job (" + job.job_id +") is canceled.";
            break;           
        case CNetScheduleAPI::eFailed:
            ret = 213;
            break;
        case CNetScheduleAPI::eDone:
            if (job.ret_code != 0) {
                ret = job.ret_code;
                err = "Job (" + job.job_id +") is done, but retcode is not zero.";
            } else if (job.output != output) {
                err = "Job (" + job.job_id + ") is done, output does not match.(" 
                    + output + ") ["+ job.output +"]";
                ret = 214;
            } else if (job.input != input) {
                err = "Job (" + job.job_id +") is done, input does not match.(" 
                    + input + ") ["+ job.input +"]";
                ret = 215;
            }
            break;
        case CNetScheduleAPI::ePending:
        case CNetScheduleAPI::eRunning:
        default:
            check_again = true;
        }
    }
    if (ret != 0)
        ERR_POST(err);
    return ret;
}


int main(int argc, const char* argv[])
{
    return CNetScheduleCheck().AppMain(argc, argv, 0, eDS_Default, 0);
}
