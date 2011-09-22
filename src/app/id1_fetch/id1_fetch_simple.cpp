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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   New IDFETCH network client (get Seq-Entry by GI)
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>

#include <objects/id1/ID1server_request.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/id1/ID1server_back.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <memory>

#include<common/test_assert.h>   /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);



/////////////////////////////////
//  CId1FetchApp::
//

class CId1FetchApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void CId1FetchApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI
    arg_desc->AddOptionalKey
        ("gi", "SeqEntryID",
         "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);
    // Request
    arg_desc->AddOptionalKey
        ("req", "Request",
         "ID1 request in ASN.1 text format",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("in", "RequestFile",
         "File to read request(s) from",
         CArgDescriptions::eInputFile);

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump the resulting data in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint("fmt", &(*new CArgAllow_Strings,
                                     "asn", "asnb", "xml", "raw", "none"));

    // Output datafile
    arg_desc->AddDefaultKey
        ("out", "ResultFile",
         "File to dump the resulting data to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);

    // Log file
    arg_desc->AddOptionalKey
        ("log", "LogFile",
         "File to post errors and messages to",
         CArgDescriptions::eOutputFile,
         0);

    // Server to connect
    arg_desc->AddDefaultKey
        ("server", "Server",
         "ID1 server name",
         CArgDescriptions::eString, "ID1");

    // Program description
    string prog_description =
        "Fetch SeqEntry from ID server by its GI id";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //
    SetupArgDescriptions(arg_desc.release());
}


int CId1FetchApp::Run(void)
{
    // Process command line args
    const CArgs& args = GetArgs();

    // Setup and tune logging facilities
    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }
#ifdef _DEBUG
    // SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
#endif

    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());
    
    // Compose request to ID1 server
    typedef vector<CRef<CID1server_request> > TReqs;
    TReqs reqs;
    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        //    id1_request.SetGetsefromgi().SetGi() = gi;
        CRef<CID1server_request> req(new CID1server_request);
        req->SetGetseqidsfromgi(gi);
        reqs.push_back(req);
    }
    else if ( args["req"] ) {
        string text = args["req"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID1server-request ::= " + text;
        }
        CRef<CID1server_request> req(new CID1server_request);
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> *req;
        reqs.push_back(req);
    }
    else if ( args["in"] ) {
        auto_ptr<CObjectIStream> req_input
            (CObjectIStream::Open(eSerial_AsnText, args["in"].AsInputFile()));
        while ( !req_input->EndOfData() ) {
            CRef<CID1server_request> req(new CID1server_request);
            *req_input >> *req;
            reqs.push_back(req);
        }
    }

    // Open connection to ID1 server
    string server_name = args["server"].AsString();
    STimeout tmout;  tmout.sec = 9;  tmout.usec = 0;  
    CConn_ServiceStream id1_server(server_name, fSERV_Any, 0, 0, &tmout);
    CObjectOStreamAsnBinary id1_server_output(id1_server);
    CObjectIStreamAsnBinary id1_server_input(id1_server);

    CNcbiOstream* datafile =
        args["fmt"].AsString() == "none"? 0: &args["out"].AsOutputFile();
    ESerialDataFormat format = eSerial_None;
    auto_ptr<CObjectOStream> id1_client_output;
    if ( datafile ) {
        string fmt = args["fmt"].AsString();
        if        (fmt == "asn") {
            format = eSerial_AsnText;
        } else if (fmt == "asnb") {
            format = eSerial_AsnBinary;
        } else if (fmt == "xml") {
            format = eSerial_Xml;
        }
        if ( format != eSerial_None ) {
            id1_client_output.reset(CObjectOStream::Open(format, *datafile));
        }
    }

    ITERATE ( TReqs, it, reqs ) {
        // Send request to the server
        id1_server_output << **it;
        id1_server_output.Flush();
        
        // Get response (Seq-Entry) from the server, dump it to the
        // output data file in the requested format
        // Dump the raw data coming from server "as is", if so specified
        if ( !id1_client_output.get() && datafile ) {
            *datafile << id1_server.rdbuf();
            return 0;  // Done
        }
        CID1server_back id1_response;
        // Read server response in ASN.1 binary format
        id1_server_input.UseMemoryPool();
        id1_server_input >> id1_response;
        if ( id1_client_output.get() ) {
        // Dump server response in the specified format
            *id1_client_output << id1_response;
            if ( format != eSerial_AsnBinary ) {
                id1_client_output->FlushBuffer();
                *datafile << '\n';
            }
        }
    }

    return 0;  // Done
}



// Cleanup
void CId1FetchApp::Exit(void)
{
    SOCK_ShutdownAPI();
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[]) 
{
    return CId1FetchApp().AppMain(argc, argv /*, 0, eDS_Default, 0*/);
}
