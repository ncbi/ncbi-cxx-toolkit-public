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

#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>

#include "client_admin.hpp"

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
    int Run(CNetScheduleClient& nc_client);
};



void CNetScheduleCheck::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule Check.");
    
    arg_desc->AddPositional("host_service", 
        "NetSchedule host or service name. Format: service|host:port", 
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("q",
                             "queue",
                             "Queue name",
                             CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}


int CNetScheduleCheck::Run(void)
{
    CArgs args = GetArgs();

    const string& host_service  = args["host_service"].AsString();

    string queue = "noname";
    if (args["q"]) {  
        queue = args["q"].AsString(); 
    }


    unsigned short port = 0;
    string host, port_str;
    auto_ptr<CNetScheduleClient> nc;
    string client_name = "netschedule_amdin";
    bool b = NStr::SplitInTwo(host_service, ":", host, port_str);
    if (b) {
        port = atoi(port_str.c_str());
        
        nc.reset(new CNetScheduleClient(host, port, client_name, queue));
                 
    } else {  // LB name
        nc.reset(new CNetScheduleClient_LB(client_name, 
                                           host_service, queue));
    }

    return Run(*nc);
}

int CNetScheduleCheck::Run(CNetScheduleClient& nc)
{
    CArgs args = GetArgs();

    const string input = "Hello ";
    const string output = "DONE ";
    string job_key = nc.SubmitJob(input);

    while ( true ) {
        SleepSec(1);

        string job_key1;
        string input1;
        bool job_exists = nc.GetJob(&job_key1, &input1);
        if (job_exists) {
            if (job_key1 != job_key)
                nc.ReturnJob(job_key1);
            else {
                if (input1 != input) {
                    string err = "Job's (" + job_key1 + 
                        ") input does not match.(" + input + ") ["+ input1 +"]";
                    nc.PutFailure(job_key1,err);
                } else {
                    nc.PutResult(job_key1, 0, output);
                }
                break;
            }
        }
    }

    int ret = 0;
    string err;
    bool check_again = true;
    while(check_again) {
        check_again = false;
        SleepSec(1);

        string output1;
        string input1;
        CNetScheduleClient::EJobStatus status = nc.GetStatus(job_key, 
                                                             &ret,
                                                             &output1,
                                                             &err,
                                                             &input1);
        switch(status) {
            
        case CNetScheduleClient::eJobNotFound:
            ret = 210;
            err = "Job (" + job_key +") is lost.";
            break;
        case CNetScheduleClient::eReturned:
            ret = 211;
            nc.CancelJob(job_key);
            err = "Job (" + job_key +") is returned.";
            break;
        case CNetScheduleClient::eCanceled:
            ret = 212;
            err = "Job (" + job_key +") is canceled.";
            break;           
        case CNetScheduleClient::eFailed:
            ret = 213;
            break;
        case CNetScheduleClient::eDone:
            if (ret != 0) {
                err = "Job (" + job_key +") is done, but retcode is not zero.";
            } else if (output1 != output) {
                err = "Job (" + job_key + ") is done, output does not match.(" 
                    + output + ") ["+ output1 +"]";
                ret = 214;
            } else if (input1 != input) {
                err = "Job (" + job_key +") is done, input does not match.(" 
                    + input + ") ["+ input1 +"]";
                ret = 215;
            }
            break;
        case CNetScheduleClient::ePending:
        case CNetScheduleClient::eRunning:
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
