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
#include <serial/objcopy.hpp>
#include <serial/iterator.hpp>

#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Info.hpp>
#include <objects/id2/ID2_Request_Get_Seq_id.hpp>
#include <objects/id2/ID2_Get_Blob_Details.hpp>
#include <objects/id2/ID2_Seq_id.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/compress/zlib.hpp>
#include <corelib/rwstream.hpp>

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
    void x_ProcessRequest(CID2_Request& request, bool dump = true);
    void x_ProcessRequest(CID2_Request_Packet& packet, bool dump = true);
    void x_ProcessData(const CID2_Reply_Data& data, CNcbiOstrstream& out);

    auto_ptr<CConn_ServiceStream> m_Server;
    CNcbiOstream*                 m_OutFile;  // ID2 reply output
    CNcbiOstream*                 m_DataFile; // ID2 data output
    ESerialDataFormat             m_Format;
    bool                          m_SkipData;
    int                           m_SerialNumber;
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
         "Format to dump server reply in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint("fmt", &(*new CArgAllow_Strings,
                                     "asn", "asnb", "xml"));

    // Output file
    arg_desc->AddDefaultKey
        ("out", "ResultFile",
         "File to dump the resulting data to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);

    // ID2 data file
    arg_desc->AddOptionalKey
        ("data", "DataFile",
         "File to save blob data to",
         CArgDescriptions::eOutputFile, CArgDescriptions::fBinary);

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

    // Number of requests
    arg_desc->AddDefaultKey
        ("count", "Count",
         "Repeat request number of times",
         CArgDescriptions::eInteger, "1");

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
    
    CONN_Wait(m_Server->GetCONN(), eIO_Write, &tmout);
    const char* descr = CONN_Description(m_Server->GetCONN());
    if ( descr ) {
        LOG_POST("  connection description: " << descr);
    }
    
    m_SerialNumber = 0;

    CID2_Request req;
    req.SetRequest().SetInit();
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));

    x_ProcessRequest(packet, show_init);
}


void CId2FetchApp::x_SendRequestPacket(CID2_Request_Packet& packet)
{
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


void CId2FetchApp::x_ProcessRequest(CID2_Request& request, bool dump)
{
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&request));
    x_ProcessRequest(packet, dump);
}


void CId2FetchApp::x_ProcessRequest(CID2_Request_Packet& packet, bool dump)
{
    CStopWatch sw(CStopWatch::eStart);
    NON_CONST_ITERATE(CID2_Request_Packet::Tdata, it, packet.Set()) {
        CID2_Request& req = **it;
        if ( !req.IsSetSerial_number() ) {
            req.SetSerial_number(m_SerialNumber++);
        }
    }

    if ( dump ) {
        CNcbiOstrstream ostr;
        ostr << MSerial_AsnText << packet;
        LOG_POST("\nProcessing request:\n" << ostr.rdbuf());
    }

    x_SendRequestPacket(packet);

    size_t remaining_count = packet.Set().size();

    _ASSERT(m_OutFile);
    CID2_Reply reply;
    auto_ptr<CNcbiOstrstream> data_out;
    while ( remaining_count > 0 ) {
        x_ReadReply(reply);
        if ( m_SkipData  ||  m_DataFile ) {
            CTypeIterator<CID2_Reply_Data> iter = Begin(reply);
            if ( iter && iter->IsSetData() ) {
                if ( m_DataFile ) {
                    if ( !data_out.get() ) {
                        data_out.reset(new CNcbiOstrstream);
                    }
                    x_ProcessData(*iter, *data_out);
                }
                if ( m_SkipData ) {
                    iter->SetData().clear();
                }
            }
        }
        if ( dump ) {
            auto_ptr<CObjectOStream> id2_client_output
                (CObjectOStream::Open(m_Format, *m_OutFile));

            *id2_client_output << reply;
            if (m_Format == eSerial_AsnText  ||  m_Format == eSerial_Xml) {
                *m_OutFile << NcbiEndl;
            }
            if ( data_out.get() ) {
                *m_DataFile << data_out->rdbuf();
            }
        }
        if ( reply.IsSetEnd_of_reply() ) {
            --remaining_count;
        }
    }
    LOG_POST("Packet processed in " << sw.Elapsed());
}


void CId2FetchApp::x_ProcessData(const CID2_Reply_Data& data,
                                 CNcbiOstrstream& out)
{
    _ASSERT( data.IsSetData() );
    _ASSERT( m_DataFile );

    TTypeInfo obj_type = 0;
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
        obj_type = CSeq_entry::GetTypeInfo();
        break;
    case CID2_Reply_Data::eData_type_seq_annot:
        obj_type = CSeq_annot::GetTypeInfo();
        break;
    case CID2_Reply_Data::eData_type_id2s_split_info:
        obj_type = CID2S_Split_Info::GetTypeInfo();
        break;
    case CID2_Reply_Data::eData_type_id2s_chunk:
        obj_type = CID2S_Chunk::GetTypeInfo();
        break;
    default:
        ERR_POST(Fatal << "Unknown data type in ID2_Reply_Data");
    }

    ESerialDataFormat format;
    switch ( data.GetData_format() ) {
    case CID2_Reply_Data::eData_format_asn_binary:
        format = eSerial_AsnBinary;
        break;
    case CID2_Reply_Data::eData_format_asn_text:
        format = eSerial_AsnText;
        break;
    case CID2_Reply_Data::eData_format_xml:
        format = eSerial_Xml;
        break;
    default:
        ERR_POST(Fatal << "Unknown data format in ID2_Reply_Data");
        return;
    }

    vector<char> buf;
    ITERATE(CID2_Reply_Data::TData, it, data.GetData()) {
        buf.insert(buf.end(), (*it)->begin(), (*it)->end());
    }

    CNcbiIstrstream stream(&buf[0], buf.size());
    auto_ptr<CObjectIStream> obj_stream;
    
    switch ( data.GetData_compression() ) {
    case CID2_Reply_Data::eData_compression_none:
    {
        obj_stream.reset(CObjectIStream::Open(format, stream));
        break;
    }
    case CID2_Reply_Data::eData_compression_gzip:
    {
        obj_stream.reset(CObjectIStream::Open(format,
            *(new CCompressionIStream(stream,
            new CZipStreamDecompressor,
            CCompressionIStream::fOwnProcessor)),
            true));
        break;
    }
    default:
        ERR_POST(Fatal << "Unknown data compression in ID2_Reply_Data");
        return;
    }
    _ASSERT( obj_stream.get() );
    auto_ptr<CObjectOStream> out_stream(
        CObjectOStream::Open(m_Format, out));
    CObjectStreamCopier copier(*obj_stream, *out_stream);
    copier.Copy(obj_type);
    if ( m_Format != eSerial_AsnBinary) {
        out << NcbiEndl;
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
    
    m_OutFile = &args["out"].AsOutputFile();
    m_DataFile = args["data"] ? &args["data"].AsOutputFile() : 0;
    const string& fmt = args["fmt"].AsString();
    if        (fmt == "asn") {
        m_Format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        m_Format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        m_Format = eSerial_Xml;
    }
    m_SkipData = args["skip_data"];

    int count = args["count"].AsInteger();

    x_InitConnection(args["server"].AsString(), args["show_init"]);

    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        CID2_Request id2_request;
        id2_request.SetRequest().SetGet_blob_info().SetBlob_id().SetResolve().
            SetRequest().SetSeq_id().SetSeq_id().SetSeq_id().SetGi(gi);
        id2_request.SetRequest().SetGet_blob_info().SetGet_data();
        for ( int i = 0; i < count; ++i ) {
            x_ProcessRequest(id2_request);
        }
    }
    else if ( args["req"] ) {
        string text = args["req"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request ::= " + text;
        }
        CNcbiIstrstream in(text.data(), text.size());
        CID2_Request id2_request;
        in >> MSerial_AsnText >> id2_request;
        for ( int i = 0; i < count; ++i ) {
            x_ProcessRequest(id2_request);
        }
    }
    else if ( args["packet"] ) {
        string text = args["packet"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request_Packet ::= " + text;
        }
        CID2_Request_Packet id2_packet;
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> id2_packet;
        for ( int i = 0; i < count; ++i ) {
            x_ProcessRequest(id2_packet);
        }
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
                for ( int i = 0; i < count; ++i ) {
                    x_ProcessRequest(id2_request);
                }
            }
            else if (type == "ID2-Request-Packet") {
                CID2_Request_Packet id2_packet;
                req_input->Read(&id2_packet,
                                id2_packet.GetThisTypeInfo(),
                                CObjectIStream::eNoFileHeader);
                for ( int i = 0; i < count; ++i ) {
                    x_ProcessRequest(id2_packet);
                }
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
