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
#include <serial/iterator.hpp>

#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Request_Get_Seq_id.hpp>
#include <objects/id2/ID2_Seq_id.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <memory>


USING_NCBI_SCOPE;
USING_SCOPE(objects);



/////////////////////////////////
//  CId1FetchApp::
//

class CId2FetchApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:
    void x_InitConnection(const string& server_name, bool show_init);
    void x_SendRequestPacket(CID2_Request_Packet& packet);
    void x_ReadReply(CID2_Reply& reply);
    void x_ProcessRequest(CID2_Request& request);
    void x_ProcessRequest(CID2_Request_Packet& packet);

    auto_ptr<CConn_ServiceStream> m_Server;
    CNcbiOstream*                 m_DataFile;
    ESerialDataFormat             m_Format;
    bool                          m_SkipData;
};


void CId2FetchApp::Init(void)
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
         "ID2 request in ASN.1 text format",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("packet", "Packet",
         "ID2 request packet in ASN.1 text format",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("in", "RequestFile",
         "File to read request(s) from",
         CArgDescriptions::eInputFile);

    // Skip blob data
    arg_desc->AddFlag("skip_data", "Skip blob data");

    // Print init reply
    arg_desc->AddFlag("show_init", "Show init reply");

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump the resulting data in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint("fmt", &(*new CArgAllow_Strings,
                                     "asn", "asnb", "xml"));

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
         "ID2 server name",
         CArgDescriptions::eString, "ID2");

    // Program description
    string prog_description =
        "Fetch SeqEntry from ID server by its GI id";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //
    SetupArgDescriptions(arg_desc.release());
}


void CId2FetchApp::x_InitConnection(const string& server_name,
                                    bool show_init)
{
    STimeout tmout;  tmout.sec = 9;  tmout.usec = 0;
    m_Server.reset(new CConn_ServiceStream
        (server_name, fSERV_Any, 0, 0, &tmout));

    CID2_Request req;
    req.SetRequest().SetInit();
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));

    if ( show_init ) {
        x_ProcessRequest(packet);
        return;
    }

    x_SendRequestPacket(packet);

    CID2_Reply reply;
    x_ReadReply(reply);
    // check init reply
    if ( reply.IsSetDiscard() ) {
        ERR_POST(Fatal << "Bad init reply: 'discard' is set");
    }
    if ( reply.IsSetError() ) {
        ERR_POST(Fatal << "Bad init reply: 'error' is set");
    }
    if ( !reply.IsSetEnd_of_reply() ) {
        ERR_POST(Fatal << "Bad init reply: 'end-of-reply' is not set");
    }
    if ( reply.GetReply().Which() != CID2_Reply::TReply::e_Init ) {
        ERR_POST(Fatal << "Bad init reply: 'reply' is not 'init'");
    }
}


void CId2FetchApp::x_SendRequestPacket(CID2_Request_Packet& packet)
{
    int ser_num = 0;
    NON_CONST_ITERATE(CID2_Request_Packet::Tdata, it, packet.Set()) {
        CID2_Request& req = **it;
        if ( !req.IsSetSerial_number() ) {
            req.SetSerial_number(ser_num++);
        }
    }
    // Open connection to ID1 server
    CObjectOStreamAsnBinary id2_server_output(*m_Server, false);
    // Send request packet to the server
    id2_server_output << packet;
    id2_server_output.Flush();
}


void CId2FetchApp::x_ReadReply(CID2_Reply& reply)
{
    // Read server response in ASN.1 binary format
    CObjectIStreamAsnBinary id2_server_input(*m_Server, false);
    id2_server_input >> reply;
}


void CId2FetchApp::x_ProcessRequest(CID2_Request& request)
{
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&request));
    x_ProcessRequest(packet);
}


void CId2FetchApp::x_ProcessRequest(CID2_Request_Packet& packet)
{
    {{
        CNcbiOstrstream ostr;
        ostr << MSerial_AsnText << packet;
        LOG_POST("\nProcessing request:\n" << ostr.rdbuf());
    }}

    x_SendRequestPacket(packet);

    size_t request_count = packet.Set().size();
    size_t remaining_count = request_count;

    _ASSERT(m_DataFile);
    CID2_Reply reply;
    while ( remaining_count > 0 ) {
        x_ReadReply(reply);
        if ( m_SkipData ) {
            CTypeIterator<CID2_Reply_Data> iter = Begin(reply);
            if ( iter && iter->IsSetData() ) {
                //CID2_Reply_Data::TData save;
                //save.swap(iter->SetData());
                iter->SetData().clear();
            }
        }
        auto_ptr<CObjectOStream> id2_client_output
            (CObjectOStream::Open(m_Format, *m_DataFile));

        *id2_client_output << reply;
        if (m_Format == eSerial_AsnText  ||  m_Format == eSerial_Xml) {
            *m_DataFile << NcbiEndl;
        }
        if ( reply.IsSetEnd_of_reply() ) {
            --remaining_count;
        }
    }
}


int CId2FetchApp::Run(void)
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
    
    m_DataFile = &args["out"].AsOutputFile();
    const string& fmt = args["fmt"].AsString();
    if        (fmt == "asn") {
        m_Format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        m_Format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        m_Format = eSerial_Xml;
    }
    m_SkipData = args["skip_data"];

    x_InitConnection(args["server"].AsString(), args["show_init"]);

    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        CID2_Request id2_request;
        id2_request.SetRequest().SetGet_blob_id().
            SetSeq_id().SetSeq_id().SetSeq_id().SetGi(gi);
        x_ProcessRequest(id2_request);
    }
    else if ( args["req"] ) {
        string text = args["req"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request ::= " + text;
        }
        CNcbiIstrstream in(text.data(), text.size());
        CID2_Request id2_request;
        in >> MSerial_AsnText >> id2_request;
        x_ProcessRequest(id2_request);
    }
    else if ( args["packet"] ) {
        string text = args["packet"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request_Packet ::= " + text;
        }
        CID2_Request_Packet id2_packet;
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> id2_packet;
        x_ProcessRequest(id2_packet);
    }
    else if ( args["in"] ) {
        auto_ptr<CObjectIStream> req_input
            (CObjectIStream::Open(eSerial_AsnText, args["in"].AsInputFile()));

        string type;
        while ( true ) {
            try {
                type = req_input->ReadFileHeader();
            }
            catch (...) {
                break;
            }
            if (type == "ID2-Request") {
                CID2_Request id2_request;
                req_input->Read(&id2_request,
                                id2_request.GetThisTypeInfo(),
                                CObjectIStream::eNoFileHeader);
                x_ProcessRequest(id2_request);
            }
            else if (type == "ID2-Request-Packet") {
                CID2_Request_Packet id2_packet;
                req_input->Read(&id2_packet,
                                id2_packet.GetThisTypeInfo(),
                                CObjectIStream::eNoFileHeader);
                x_ProcessRequest(id2_packet);
            }
            else {
                ERR_POST(Fatal <<
                    "Object type must be ID2-Request or ID2-Request-Packet.");
            }
        }
    }
    else {
        ERR_POST(Fatal << "No ID2-Request specified.");
    }

    return 0;  // Done
}



// Cleanup
void CId2FetchApp::Exit(void)
{
    SOCK_ShutdownAPI();
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[]) 
{
    return CId2FetchApp().AppMain(argc, argv /*, 0, eDS_Default, 0*/);
}


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2005/08/10 18:42:51  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
