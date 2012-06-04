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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:  Check NetCache service (functionality).
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <connect/ncbi_socket.hpp>

#include <corelib/mswin_no_popup.h>

#define GRID_APP_NAME "netcache_check"

USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////


/// NetCache check application
///
/// @internal
///
class CNetCacheCheckApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CNetCacheCheckApp::Init(void)
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache check.");

    arg_desc->AddDefaultPositional("service_address",
        "NetCache host or service name: { host:port | lb_service_name }.",
        CArgDescriptions::eString,
        "");
    arg_desc->AddFlag("check-health", "Return server health percentage");
    arg_desc->AddDefaultKey("delay", "phase_delay",
        "Delay between test phases. Default: 3000 ms",
        CArgDescriptions::eInteger, "3000");

    SetupArgDescriptions(arg_desc.release());
}

#define CHECK_FAILED_RETVAL 100

int CNetCacheCheckApp::Run(void)
{
    CArgs args = GetArgs();

    string service_name = args["service_address"].AsString();

    if (service_name.empty()) {
        NCBI_THROW(CArgHelpException, eHelp, kEmptyStr);
    }

    bool return_health = args["check-health"];

    CNetCacheAPI cl(service_name, "netcache_check");

    // functionality test

    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    const char test_data2[] = "Test 2.";
    string key = cl.PutData(test_data, sizeof(test_data));

    if (key.empty()) {
        NcbiCerr << "Failed to put data. " << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }
    NcbiCout << key << NcbiEndl;

    char data_buf[1024];

    size_t blob_size = cl.GetBlobSize(key);

    if (blob_size != sizeof(test_data)) {
        NcbiCerr << "Failed to retrieve data size." << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }

    auto_ptr<IReader> reader(cl.GetData(key, &blob_size,
        CNetCacheAPI::eCaching_Disable));

    if (reader.get() == 0) {
        NcbiCerr << "Failed to read data." << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }

    reader->Read(data_buf, 1024);
    int res = strcmp(data_buf, test_data);
    if (res != 0) {
        NcbiCerr << "Could not read data." << NcbiEndl <<
            "Server returned:" << NcbiEndl << data_buf << NcbiEndl <<
            "Expected:" << NcbiEndl << test_data << NcbiEndl;

        return CHECK_FAILED_RETVAL;
    }
    reader.reset(0);

    {{
        auto_ptr<IEmbeddedStreamWriter> wrt(cl.PutData(&key));
        size_t bytes_written;
        wrt->Write(test_data2, sizeof(test_data2), &bytes_written);
        wrt->Close();
    }}

    SleepMilliSec(args["delay"].AsInteger());

    memset(data_buf, 0xff, sizeof(data_buf));
    reader.reset(cl.GetReader(key, &blob_size, CNetCacheAPI::eCaching_Disable));
    reader->Read(data_buf, 1024);
    res = strcmp(data_buf, test_data2);
    if (res != 0) {
        NcbiCerr << "Could not read updated data." << NcbiEndl <<
            "Server returned:" << NcbiEndl << data_buf << NcbiEndl <<
            "Expected:" << NcbiEndl << test_data2 << NcbiEndl;

        return CHECK_FAILED_RETVAL;
    }

    if (return_health) {
        if (cl.GetService().IsLoadBalanced()) {
            NcbiCerr << "Cannot use -check-health "
                "for a load-balanced server." << NcbiEndl;
            return CHECK_FAILED_RETVAL + 1;
        }

        CNcbiOstrstream health_cmd_output_stream;
        try {
            cl.GetAdmin().PrintHealth(health_cmd_output_stream);
        }
        catch (CNetCacheException& e) {
            if (e.GetErrCode() != CNetCacheException::eUnknownCommand)
                throw;
            return 0;
        }
        string health_cmd_output =
            CNcbiOstrstreamToString(health_cmd_output_stream);
        string::size_type new_line_pos = health_cmd_output.find('\n');
        if (new_line_pos != string::npos)
            health_cmd_output.erase(0, new_line_pos);
        unsigned health_percentage = NStr::StringToUInt(health_cmd_output,
            NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSymbols);
        if (health_percentage > 100)
            health_percentage = 100;
        return 100 - health_percentage;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    return CNetCacheCheckApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
