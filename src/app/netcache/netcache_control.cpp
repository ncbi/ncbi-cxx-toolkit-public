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
 * File Description:  NetCache administration utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/ncbi_socket.hpp>


USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////


/// Netcache stop application
///
/// @internal
///
class CNetCacheControl : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CNetCacheControl::Init(void)
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache control.");
    
    arg_desc->AddPositional("service", 
                            "NetCache service name.", CArgDescriptions::eString);


    arg_desc->AddFlag("shutdown", "Shutdown server");
    arg_desc->AddFlag("ver", "Server version");
    arg_desc->AddFlag("getconf", "Server config");
    arg_desc->AddFlag("stat", "Server statistics");
    arg_desc->AddFlag("dropstat", "Drop server statistics");
    arg_desc->AddFlag("monitor", "Monitor server");

    arg_desc->AddOptionalKey("fetch",
                     "key",
                     "Retrieve data by key",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("log",
                             "server_logging",
                             "Switch server side logging",
                             CArgDescriptions::eBoolean);


    arg_desc->AddOptionalKey("owner",
                             "owner",
                             "Get BLOB's owner",
                             CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}


class CSimpleSink : public CNetServiceAPI_Base::ISink
{
public:
    CSimpleSink(CNcbiOstream& os) : m_Os(os) {}
    ~CSimpleSink() {}
    
    virtual CNcbiOstream& GetOstream(CNetServerConnection conn)
    {
        m_Os << conn.GetHost() << ":" << conn.GetPort() << endl;
        return m_Os;
    }
    virtual void EndOfData(CNetServerConnection conn)
    {
        m_Os << endl;
    }
private:
    CNcbiOstream& m_Os;
};


int CNetCacheControl::Run(void)
{
    const CArgs& args = GetArgs();
    const string& service  = args["service"].AsString();

    CNetCacheAPI nc_client(service,"netcache_control");
    CNetCacheAdmin admin = nc_client.GetAdmin();

    if (args["fetch"]) {
        string key = args["fetch"].AsString();
        size_t blob_size = 0;
        auto_ptr<IReader> reader(nc_client.GetData(key, &blob_size));
        if (!reader.get()) {
            ERR_POST("cannot retrieve data");
            return 1;
        }
        CRStream strm(reader.get());
        NcbiCout << strm.rdbuf();
        return 0;
    }

    if (args["log"]) {  // logging control
        bool on_off = args["log"].AsBoolean();
        admin.Logging(on_off);
        NcbiCout << "Logging turned " 
                 << (on_off ? "ON" : "OFF") << " on the server" << NcbiEndl;
    }

    if (args["owner"]) {  // BLOB's owner
        string key = args["owner"].AsString();
        string owner = nc_client.GetOwner(key);
        NcbiCout << "BLOB belongs to: [" << owner << "]" << NcbiEndl;
    }


    if (args["getconf"]) {  // config
        CSimpleSink sink(NcbiCout);
        admin.PrintConfig(sink);
    }
    if (args["monitor"]) {  // monitor
        admin.Monitor(NcbiCout);
    }

    if (args["stat"]) {  // statistics
        CSimpleSink sink(NcbiCout);
        admin.PrintStat(sink);
    }
    if (args["dropstat"]) {  // drop stat
        admin.DropStat();
        NcbiCout << "Drop statistics request has been sent to server" << NcbiEndl;
    }

    if (args["ver"]) {
        CSimpleSink sink(NcbiCout);
        admin.GetServerVersion(sink);
    }

    if (args["shutdown"]) {  // shutdown
        admin.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
        return 0;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetCacheControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
