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
#ifdef _DEBUG
    // SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
#endif

    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());
    
    // Compose request to ID1 server
    CID1server_request id1_request;
    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        //    id1_request.SetGetsefromgi().SetGi() = gi;
        id1_request.SetGetseqidsfromgi() = gi;
    }
    else if ( args["req"] ) {
        string text = args["req"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID1server-request ::= " + text;
        }
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> id1_request;
    }

    // Open connection to ID1 server
    STimeout tmout;  tmout.sec = 9;  tmout.usec = 0;  
    CConn_ServiceStream id1_server("ID1", fSERV_Any, 0, 0, &tmout);
    {{
        CObjectOStreamAsnBinary id1_server_output(id1_server);

        // Send request to the server
        id1_server_output << id1_request;
        id1_server_output.Flush();
    }}

    // Get response (Seq-Entry) from the server, dump it to the
    // output data file in the requested format
    CNcbiOstream& datafile = args["out"].AsOutputFile();
    const string& fmt = args["fmt"].AsString();

    // Dump the raw data coming from server "as is", if so specified
    if (fmt == "raw") {
        datafile << id1_server.rdbuf();
        return 0;  // Done
    }

    CID1server_back id1_response;
    {{
        // Read server response in ASN.1 binary format
        CObjectIStreamAsnBinary id1_server_input(id1_server, false);
        id1_server_input >> id1_response;
    }}

    // Dump server response in the specified format
    ESerialDataFormat format;
    if        (fmt == "asn") {
        format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        format = eSerial_Xml;
    }

    {{
        auto_ptr<CObjectOStream> id1_client_output
            (CObjectOStream::Open(format, datafile));

        *id1_client_output << id1_response;
        if (fmt == "asn"  ||  fmt == "xml") {
            datafile << NcbiEndl;
        }
    }}

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


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.13  2004/05/21 21:41:40  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/02/20 20:04:08  vasilche
 * Added possibility to send any request to ID1.
 *
 * Revision 1.11  2002/06/12 16:51:55  lavr
 * Take advantage of CONNECT_Init()
 *
 * Revision 1.10  2002/03/11 21:52:05  lavr
 * Print complete debug and trace information when compiled with _DEBUG
 *
 * Revision 1.9  2002/01/17 17:22:21  vakatov
 * It should be okay to use "CObjectIStreamAsnBinary" instead of
 * CObjectIStream::Open() for non-blocking streams now (so rollback R1.4)
 *
 * Revision 1.8  2001/09/25 14:04:06  ucko
 * Update call to CConn_ServiceStream constructor for new interface.
 *
 * Revision 1.7  2001/07/19 19:40:20  lavr
 * Typo fixed
 *
 * Revision 1.6  2001/06/01 18:43:44  vakatov
 * Comment out excessive debug/trace printout
 *
 * Revision 1.5  2001/05/16 17:55:37  grichenk
 * Redesigned support for non-blocking stream read operations
 *
 * Revision 1.4  2001/05/11 20:41:16  grichenk
 * Added support for non-blocking stream reading
 *
 * Revision 1.3  2001/05/11 14:06:45  grichenk
 * The first working revision
 *
 * Revision 1.2  2001/04/13 14:09:34  grichenk
 * Next debug version, still not working
 *
 * Revision 1.1  2001/04/10 22:39:04  vakatov
 * Initial revision.
 * Compiles and links, but apparently is not working yet.
 *
 * ===========================================================================
 */
