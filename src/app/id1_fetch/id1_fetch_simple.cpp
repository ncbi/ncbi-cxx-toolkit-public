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
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/04/10 22:39:04  vakatov
 * Initial revision.
 * Compiles and links, but apparently is not working yet.
 *
 * ===========================================================================
 */

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
#include <objects/seqloc/Seq_id.hpp>
#include <objects/id1/ID1server_back.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <memory>


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
    arg_desc->AddKey
        ("gi", "SeqEntryID",
         "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("gi", new CArgAllow_Integers(0, 99999999));

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump the resulting data in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint("fmt", &(*new CArgAllow_Strings,
                                     "asn", "asnb", "xml", "raw"));

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
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);

    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    
    // Compose request to ID1 server
    CID1server_request id1_request;
    int gi = args["gi"].AsInteger();
    id1_request.SetGetgi().SetGi() = gi;

    // Open connection to ID1 server
    CConn_ServiceStream id1_server("ID1");
    CObjectOStreamAsnBinary id1_server_output(id1_server);

    // Send request to the server
    id1_server_output << id1_request;

    // Get response (Seq-Entry) from the server, dump it to the
    // output data file in the requested format
    CNcbiOstream& datafile = args["out"].AsOutputFile();
    const string& fmt = args["fmt"].AsString();

    // Dump the raw data coming from server "as is", if so specified
    if (fmt == "raw") {
        datafile << id1_server.rdbuf();
        return 0;  // Done
    }

    // Read server response in ASN.1 binary format
    CID1server_back id1_response;
    CObjectIStreamAsnBinary id1_server_input(id1_server);
    id1_server_input >> id1_response;
    CSeq_entry& seq_entry = id1_response.GetGotseqentry();


    // Dump server response in the specified format
    ESerialDataFormat format;
    if        (fmt == "asn") {
        format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        format = eSerial_Xml;
    }

    auto_ptr<CObjectOStream> id1_client_output
        (CObjectOStream::Open(format, datafile, eSerial_StdWhenAny));
    
    *id1_client_output << seq_entry;

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
    return CId1FetchApp().AppMain(argc, argv);
}
