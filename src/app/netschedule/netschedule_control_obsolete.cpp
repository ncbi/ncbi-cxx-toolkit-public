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
 * File Description:  NetSchedule administration utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>


USING_NCBI_SCOPE;

/// Proxy class to open ShutdownServer
///
/// @internal
class CNetScheduleClient_Control : public CNetScheduleClient 
{
public:
    CNetScheduleClient_Control(const string&  host,
                            unsigned short port)
      : CNetScheduleClient(host, port, 
                           "netschedule_control", "noname")
    {}

    void ShutdownServer() { CNetScheduleClient::ShutdownServer(); }
//    void Logging(bool on_off) { CNetCacheClient::Logging(on_off); }
};
///////////////////////////////////////////////////////////////////////


/// NetSchedule control application
///
/// @internal
///
class CNetScheduleControl : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CNetScheduleControl::Init(void)
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule control.");
    
    arg_desc->AddPositional("hostname", 
                            "NetSchedule host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.", 
                            CArgDescriptions::eInteger);

    arg_desc->AddFlag("s", "Shutdown server");
    arg_desc->AddFlag("v", "Server version");

    arg_desc->AddOptionalKey("log",
                             "server_logging",
                             "Switch server side logging",
                             CArgDescriptions::eBoolean);

    SetupArgDescriptions(arg_desc.release());
}



int CNetScheduleControl::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();

    CNetScheduleClient_Control nc_client(host, port);

    if (args["log"]) {  // logging control
        bool on_off = args["log"].AsBoolean();
/*
        nc_client.Logging(on_off);
        NcbiCout << "Logging turned " 
                 << (on_off ? "ON" : "OFF") << " on the server" << NcbiEndl;
*/
    }

    if (args["s"]) {  // shutdown
        nc_client.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
        return 0;
    }


    string version = nc_client.ServerVersion();
    if (version.empty()) {
        NcbiCout << "NetCache server communication error." << NcbiEndl;
        return 1;
    }
    NcbiCout << version << NcbiEndl;
    
    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetScheduleControl().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/02/08 17:55:53  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
