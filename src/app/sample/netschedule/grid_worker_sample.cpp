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


#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///
class CSampleJob : public IWorkerNodeJob
{
public:
    CSampleJob() {}
    virtual ~CSampleJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        LOG_POST( context.GetJobKey() + " " + context.GetJobInput());

        // 1. Get an input data from the client
        //    (You can use ASN.1 de-serialization here)
        //
        CNcbiIstream& is = context.GetIStream();
        int count;
        is >> count;
        vector<double> dvec;
        dvec.reserve(count);
        LOG_POST( "Getting " << count << " doubles from stream...");
        for (int i = 0; i < count; ++i) {
            if (!is.good()) {
                ERR_POST( "Input stream error. Index : " << i );

                // if something bad has happend throw an exception
                // and its message will be delivered to the client.
                //
                throw runtime_error("Worker node input stream error"); 
            }
            // Don't forget to check if shutdown has been requested
            if (count % 1000 == 0) {
                if (context.GetShutdownLevel() 
                    != CNetScheduleClient::eNoShutdown)
                    return 1;
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
        for (int i = 0; i < 5; ++i) {
            if (context.GetShutdownLevel() 
                == CNetScheduleClient::eShutdownImmidiate) {
                return 1;
            }
            SleepMicroSec(3000);
        }
        sort(dvec.begin(), dvec.end());


        // 3. Return the result to the client 
        //    (You can use ASN.1 serialization here)
        //
        CNcbiOstream& os = context.GetOStream();
        os << dvec.size() << ' ';
        for (int i = 0; i < count; ++i) {
            if (!os.good()) {
                ERR_POST( "Output stream error. Index : " << i );
                throw runtime_error("Worker node output stream error"); 
            }
            os << dvec[i] << ' ';
        }

        // 4. Indicate that the job is done and the result 
        // can be delivered to the client.
        //
        context.CommitJob();

        LOG_POST( "Job " << context.GetJobKey() << " is done.");
        return 0;
    }
};

/// Sample Job Factory
/// Creates new job class every time worker node receives a request 
///  from the queue
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
        // Next formats are valid and supported:
        //   ProgramName 1.2.3
        //   ProgramName version 1.2.3
        //   ProgramName v. 1.2.3
        //   ProgramName ver. 1.2.3

        return "SampleNode version 1.0.1";
    }
};

/////////////////////////////////////////////////////////////////////////////
//

#if defined(NCBI_OS_UNIX)
/// @internal
extern "C" 
void GridWorker_SignalHandler( int )
{
    CGridWorkerApp* app = 
        dynamic_cast<CGridWorkerApp*>(CNcbiApplication::Instance());
    if (app) {
        app->RequestShutdown();
    }
}
#endif


int main(int argc, const char* argv[])
{
#if defined(NCBI_OS_UNIX)
    // attempt to get server gracefully shutdown on signal
       signal( SIGINT, GridWorker_SignalHandler);
       signal( SIGTERM, GridWorker_SignalHandler);    
#endif

    CGridWorkerApp app(new CSampleJobFactory);
    return app.AppMain(argc, argv, 0, eDS_Default, "grid_worker_sample.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/04/04 16:15:02  kuznets
 * Version string modified to match Program 1.2.3 mask
 *
 * Revision 1.6  2005/03/28 19:31:03  didenko
 * Added signals handling
 *
 * Revision 1.5  2005/03/25 16:28:48  didenko
 * Cosmetics
 *
 * Revision 1.4  2005/03/24 17:48:51  kuznets
 * Minor cleanup...
 *
 * Revision 1.3  2005/03/24 17:45:44  kuznets
 * Comments, formatting...
 *
 * Revision 1.2  2005/03/24 15:35:35  didenko
 * Made it compile on Unixes
 *
 * Revision 1.1  2005/03/24 15:10:41  didenko
 * Added samples for Worker node framework
 *
 * ===========================================================================
 */

