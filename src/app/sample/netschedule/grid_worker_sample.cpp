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
#include <vector>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netcache_client.hpp>
#include <connect/services/grid_worker_app.hpp>



USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


class CSampleJob : public IWorkerNodeJob
{
public:
    CSampleJob() {}
    ~CSampleJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        LOG_POST( context.GetJobKey() + " " + context.GetJobInput());
        /// 1. Get an input data from the client
        CNcbiIstream& is = context.GetIStream();
        int count;
        is >> count;
        vector<double> dvec;
        dvec.reserve(count);
        LOG_POST( "Getting " << count << " doubles from stream...");
        for (int i = 0; i < count; ++i) {
            if (!is.good()) {
                LOG_POST( "Input stream error. Index : " << i );
                /// if something bad has happend throw an exception
                /// and its message will be delivered to the client.
                throw exception("Input stream error"); 
            }
            /// Don't forget to check if shutdown has been requested
            if (count % 1000 == 0) {
                if (context.GetShutdownLevel() 
                    != CNetScheduleClient::eNoShutdown)
                    return 1;
            }
            double d;
            is >> d;
            dvec.push_back(d);
        }
       
        /// 2. Doing some time consuming job here
        /// Don't forget to check if shutdown 
        /// has been requested.
        for (int i = 0; i < 5; ++i) {
            if (context.GetShutdownLevel() 
                != CNetScheduleClient::eNoShutdown) {
                return 1;
            }
            SleepMicroSec(3000);
        }
        sort(dvec.begin(), dvec.end());

        /// 3. Return the result to the client
        CNcbiOstream& os = context.GetOStream();
        os << dvec.size() << ' ';
        for (int i = 0; i < count; ++i) {
            if (!os.good()) {
                LOG_POST( "Output stream error. Index : " << i );
                /// if something bad has happend throw an exception
                /// and its message will be delivered to the client.
                throw exception("Output stream error"); 
            }
            /// Don't forget to check if shutdown was requested
            /// We will interupt the job here only if urgent shutdown
            /// has been requested.
            if (count % 1000 == 0) {
                if (context.GetShutdownLevel() 
                      == CNetScheduleClient::eShutdownImmidiate) 
                    return 1;
            }
            os << dvec[i] << ' ';
        }

        /// 4. Say the system that the job is done and the result 
        /// can be return to the client.
        context.CommitJob();
        LOG_POST( "Job " << context.GetJobKey() << " is done.");
        return 0;
    }
};

/// Sample Job Factory
///
class CSampleJobFactory : public IWorkerNodeJobFactory
{
public:
    virtual ~CSampleJobFactory() {}

    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new CSampleJob;
    }
    virtual string GetJobVersion() const 
    {
        return "Sample Job worker node ver.0.01";
    }
};


int main(int argc, const char* argv[])
{
    CGridWorkerApp app(new CSampleJobFactory);
    return app.AppMain(argc, argv, 0, eDS_Default, "grid_worker_sample.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/24 15:10:41  didenko
 * Added samples for Worker node framework
 *
 * ===========================================================================
 */

