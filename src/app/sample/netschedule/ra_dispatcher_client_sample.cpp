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
 *   Government have not placed any restriction on its use or reproduction.
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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>

#include <connect/services/ra_dispatcher_client.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <util/fileblobstorage/blobstorage_file.hpp>


USING_NCBI_SCOPE;

class CRADispatcherClientSampleApp : public CNcbiApplication
{
public:

    virtual void Init(void);
    virtual int Run(void);

protected:

};

void CRADispatcherClientSampleApp::Init(void)
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_File(); 
}

int CRADispatcherClientSampleApp::Run(void)
{

    IRegistry& reg = GetConfig();

    CConn_ServiceStream conn("remote_app_dispatcher");

    CBlobStorageFactory factory(reg);

    CRemoteAppRequestSB request(factory);

    request.SetAppName("app1");
    CNcbiOstream& os = request.GetStdIn();
        
    os << "Request data" << endl;

    request.SetCmdLine("-a sss -f1=/tmp/dddd.f1 -f=\"/tmp/ddd d.f\"");
    request.AddFileForTransfer("/tmp/dddd.f1");
    request.AddFileForTransfer("/tmp/ddd d.f");
    request.AddFileForTransfer("/tmp/ddd");

    CRADispatcherClient rad_client(factory, conn);
    rad_client.StartJob(request);

    CRADispatcherClient::EJobState st;
    do {
        st = rad_client.GetJobState();
        SleepSec(2);

    } while( st != CRADispatcherClient::eReady );

    CRemoteAppResultSB& result = rad_client.GetResult();

    NcbiCout << "Return code: " << result.GetRetCode() << NcbiEndl;
    NcbiCout << "StdOut : " <<  NcbiEndl;
    NcbiCout << result.GetStdOut().rdbuf();
    NcbiCout.clear();
    NcbiCout << NcbiEndl << "StdErr : " <<  NcbiEndl;
    NcbiCout << result.GetStdErr().rdbuf();
    NcbiCout.clear();
    NcbiCout << NcbiEndl << "----------------------" <<  NcbiEndl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CRADispatcherClientSampleApp().AppMain(argc, argv);
} 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/07/13 14:44:05  didenko
 * Added the example for CRADispatcherClient
 *
 * ===========================================================================
 */
 
