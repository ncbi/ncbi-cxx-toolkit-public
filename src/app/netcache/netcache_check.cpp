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
 * File Description:  Check NetCache service (functionality).
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/netcache_client.hpp>
#include <connect/ncbi_socket.hpp>


USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////


/// Netcache stop application
///
/// @internal
///
class CNetCacheCheck : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CNetCacheCheck::Init(void)
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache check.");
    
    arg_desc->AddPositional("service_address", 
        "NetCache host or service name: { host:port | lb_service_name }.", 
        CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}



int CNetCacheCheck::Run(void)
{
    CArgs args = GetArgs();
    const string& service_addr  = args["service_address"].AsString();

    auto_ptr<CNetCacheClient> cl;

    string host, port_str;
    if (NStr::SplitInTwo(service_addr, ":", host, port_str)) {
        unsigned short port = (unsigned short)NStr::StringToInt(port_str);
        cl.reset(new CNetCacheClient(host, port, "netcache_check"));
    } else { // LB service name
        cl.reset(new CNetCacheClient_LB("netcache_check", service_addr));
    }

    // functionality test
/*
    if (!cl->IsAlive()) {
        NcbiCerr << "Netcache server at " 
                 << service_addr 
                 << " is failed IsAlive() test." 
                 << NcbiEndl;
        return 1;
    }
*/

    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    const char test_data2[] = "Test 2.";
    string key = cl->PutData(test_data, sizeof(test_data));

    if (key.empty()) {
        NcbiCerr << "Failed to put data. " << NcbiEndl;
        return 1;
    }
    NcbiCout << key << NcbiEndl;

    char data_buf[1024];
    size_t blob_size;
    
    auto_ptr<IReader> reader(cl->GetData(key, &blob_size));
    if (reader.get() == 0) {
        NcbiCerr << "Failed to read data." << NcbiEndl;
        return 1;
    }
    reader->Read(data_buf, 1024);
    int res = strcmp(data_buf, test_data);
    if (res != 0) {
        NcbiCerr << "Incorrect functionality while reading data." << NcbiEndl;
        NcbiCerr << "Server returned:" << NcbiEndl << data_buf << NcbiEndl;
        NcbiCerr << "Expected:" << NcbiEndl << test_data << NcbiEndl;
        return 1;
    }
    reader.reset(0);

    
    auto_ptr<IWriter> wrt(cl->PutData(&key));
    size_t bytes_written;
    //ERW_Result wres = 
        wrt->Write(test_data2, sizeof(test_data2), &bytes_written);
    wrt.reset(0);
    memset(data_buf, 0xff, sizeof(data_buf));
    reader.reset(cl->GetData(key, &blob_size));
    reader->Read(data_buf, 1024);
    res = strcmp(data_buf, test_data2);
    if (res != 0) {
        NcbiCerr << "Incorrect functionality while reading updated data." << NcbiEndl;
        NcbiCerr << "Server returned:" << NcbiEndl << data_buf << NcbiEndl;
        NcbiCerr << "Expected:" << NcbiEndl << test_data << NcbiEndl;
        return 1;
    }

    
    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetCacheCheck().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/02/15 19:07:48  kuznets
 * IsAlive call removed
 *
 * Revision 1.1  2005/02/07 18:33:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
