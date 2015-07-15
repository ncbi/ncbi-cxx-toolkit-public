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
#include <corelib/rwstream.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/iterator.hpp>
#include <serial/impl/stdtypes.hpp>

#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Request_Get_Blob_Info.hpp>
#include <objects/id2/ID2_Request_Get_Seq_id.hpp>
#include <objects/id2/ID2_Get_Blob_Details.hpp>
#include <objects/id2/ID2_Seq_id.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
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

#include <dbapi/driver/driver_mgr.hpp>

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
    void x_InitConnection(bool show_init);
    void x_InitConnection(const string& server_name, bool show_init);
    void x_InitPubSeqConnection(const string& server_name, bool show_init);
    void x_SendRequestPacket(CID2_Request_Packet& packet);
    void x_ReadReply(CID2_Reply& reply);
    void x_ReadReply(CID2_Reply& reply, CObjectInfo& object);
    void x_ProcessRequest(CID2_Request& request, bool dump = true);
    void x_ProcessRequest(CID2_Request_Packet& packet, bool dump = true);
    void x_ProcessData(const CID2_Reply_Data& data);
    void x_SaveDataObject(const CObjectInfo& object, CNcbiOstream& out);

    AutoPtr<CConn_ServiceStream>  m_ID2Conn;
    AutoPtr<CDB_Connection>       m_PubSeqOS;
    AutoPtr<CNcbiIstream>         m_PubSeqOSReply;
    CNcbiOstream*                 m_OutFile;  // ID2 reply output
    CNcbiOstream*                 m_DataFile; // ID2 data output
    ESerialDataFormat             m_Format;
    bool                          m_SkipData;
    bool                          m_ParseData;
    bool                          m_CopyData;
    bool                          m_PipeData;
    int                           m_SerialNumber;
};


void CId2FetchApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    AutoPtr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI
    arg_desc->AddOptionalKey
        ("gi", "SeqEntryID",
         "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);
    // Seq-id
    arg_desc->AddOptionalKey
        ("id", "SeqEntryID",
         "Seq-id of the Seq-Entry to fetch",
         CArgDescriptions::eString);
    // Seq-id
    arg_desc->AddOptionalKey
        ("blob_id", "BlobID",
         "Blob id of the Seq-Entry to fetch (sat,sat-key)",
         CArgDescriptions::eString);
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
    // Copy blob data
    arg_desc->AddFlag("copy_data", "Copy blob data");
    // Parse blob data
    arg_desc->AddFlag("parse_data", "Parse blob data");
    // Pipe blob data
    arg_desc->AddFlag("pipe_data", "Pipe parsing blob data");

    // Print init reply
    arg_desc->AddFlag("show_init", "Show init reply");

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump server reply in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint("fmt", &(*new CArgAllow_Strings,
                                     "asn", "asnb", "xml", "none"));

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

    // Server to connect
    arg_desc->AddOptionalKey
        ("pubseqos", "PubSeqOS",
         "PubSeqOS server name",
         CArgDescriptions::eString);

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
    m_ID2Conn.reset(new CConn_ServiceStream
        (server_name, fSERV_Any, 0, 0, &tmout));
    
    CONN_Wait(m_ID2Conn->GetCONN(), eIO_Write, &tmout);
    const char* descr = CONN_Description(m_ID2Conn->GetCONN());
    if ( descr ) {
        LOG_POST("  connection description: " << descr);
    }
    
    x_InitConnection(show_init);
}


void CId2FetchApp::x_InitPubSeqConnection(const string& server_name,
                                          bool show_init)
{
    C_DriverMgr drvMgr;
    map<string,string> args;
    args["packet"]="3584"; // 7*512
    args["version"]="125"; // for correct connection to OpenServer
    string errmsg;
    I_DriverContext* context = drvMgr.GetDriverContext("ftds", &errmsg, &args);
    if ( !context ) {
        ERR_FATAL("Failed to create ftds context: "<<errmsg);
    }

    m_PubSeqOS.reset(context->Connect(server_name, "anyone", "allowed", 0));
    if ( !m_PubSeqOS.get() ) {
        ERR_FATAL("Failed to open PubSeqOS connection: "<<server_name);
    }

    {{
        AutoPtr<CDB_LangCmd> cmd(m_PubSeqOS->LangCmd("set blob_stream on"));
        if ( cmd ) {
            cmd->Send();
            cmd->DumpResults();
        }
    }}

    x_InitConnection(show_init);
}


void CId2FetchApp::x_InitConnection(bool show_init)
{
    m_SerialNumber = 0;
    CID2_Request req;
    req.SetRequest().SetInit();
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));

    x_ProcessRequest(packet, show_init);
}


namespace {
    bool sx_FetchNextItem(CDB_Result& result, const CTempString& name)
    {
        while ( result.Fetch() ) {
            for ( unsigned pos = 0; pos < result.NofItems(); ++pos ) {
                if ( result.ItemName(pos) == name ) {
                    return true;
                }
                result.SkipItem();
            }
        }
        return false;
    }
    
    class CDB_Result_Reader : public CObject, public IReader
    {
    public:
        CDB_Result_Reader(AutoPtr<CDB_RPCCmd> cmd,
                          AutoPtr<CDB_Result> db_result)
            : m_DB_RPCCmd(cmd), m_DB_Result(db_result)
            {
            }

        ERW_Result Read(void*   buf,
                        size_t  count,
                        size_t* bytes_read)
            {
                if ( !count ) {
                    if ( bytes_read ) {
                        *bytes_read = 0;
                    }
                    return eRW_Success;
                }
                size_t ret;
                while ( (ret = m_DB_Result->ReadItem(buf, count)) == 0 ) {
                    if ( !sx_FetchNextItem(*m_DB_Result, "asnout") ) {
                        break;
                    }
                }
                if ( bytes_read ) {
                    *bytes_read = ret;
                }
                return ret? eRW_Success: eRW_Eof;
            }
        ERW_Result PendingCount(size_t* /*count*/)
            {
                return eRW_NotImplemented;
            }

    private:
        AutoPtr<CDB_RPCCmd> m_DB_RPCCmd;
        AutoPtr<CDB_Result> m_DB_Result;
    };
}


void CId2FetchApp::x_SendRequestPacket(CID2_Request_Packet& packet)
{
    // Open connection to ID1 server
    if ( m_ID2Conn ) {
        CObjectOStreamAsnBinary id2_server_output(*m_ID2Conn);
        // Send request packet to the server
        id2_server_output << packet;
        id2_server_output.Flush();
    }
    else {
        m_PubSeqOSReply.reset();
        const size_t MAX_ASN_IN = 20*1024;
        char buffer[MAX_ASN_IN];
        size_t size;
        {{
            // NOT CNcbiOstrstream, because that can be ostringstream, which
            // doesn't support setting up a fixed-length buffer in this manner.
            ostrstream mem_str(buffer, sizeof(buffer));
            {{
                CObjectOStreamAsnBinary obj_str(mem_str);
                obj_str << packet;
            }}
            if ( !mem_str ) {
                ERR_FATAL("PubSeqOS: packet size overflow");
            }
            size = mem_str.pcount();
        }}
        CDB_VarChar service("ID2");
        CDB_VarChar short_asn;
        CDB_LongBinary long_asn(size);
        long_asn.SetValue(buffer, size);
        CDB_TinyInt text_in(0);
        CDB_TinyInt text_out(0);
    
        AutoPtr<CDB_RPCCmd> cmd(m_PubSeqOS->RPC("os_asn_request"));
        cmd->SetParam("@service", &service);
        cmd->SetParam("@asnin", &short_asn);
        cmd->SetParam("@text", &text_in);
        cmd->SetParam("@out_text", &text_out);
        cmd->SetParam("@asnin_long", &long_asn);
        cmd->Send();

        AutoPtr<CDB_Result> dbr;
        while( cmd->HasMoreResults() ) {
            if ( cmd->HasFailed() ) {
                ERR_FATAL("PubSeqOS: failed RPC");
            }
            dbr = cmd->Result();
            if ( !dbr.get() || dbr->ResultType() != eDB_RowResult ) {
                continue;
            }
            if ( sx_FetchNextItem(*dbr, "asnout") ) {
                AutoPtr<CDB_Result_Reader> reader
                    (new CDB_Result_Reader(cmd, dbr));
                m_PubSeqOSReply.reset(new CRStream(reader.release(),
                                                   0, 0,
                                                   CRWStreambuf::fOwnAll));
                return;
            }
        }
        ERR_FATAL("PubSeqOS: no more results");
    }
}


namespace {
    class COSSReader : public IReader
    {
    public:
        typedef vector<char> TOctetString;
        typedef list<TOctetString*> TOctetStringSequence;

        COSSReader(const TOctetStringSequence& in)
            : m_Input(in),
              m_CurVec(in.begin())
            {
                x_SetVec();
            }
        
        virtual ERW_Result Read(void* buffer,
                                size_t  count,
                                size_t* bytes_read = 0)
            {
                size_t pending = x_Pending();
                count = min(pending, count);
                if ( bytes_read ) {
                    *bytes_read = count;
                }
                if ( pending == 0 ) {
                    return eRW_Eof;
                }
                if ( count ) {
                    memcpy(buffer, &(**m_CurVec)[m_CurPos], count);
                    m_CurPos += count;
                }
                return eRW_Success;
            }

        virtual ERW_Result PendingCount(size_t* count)
            {
                size_t pending = x_Pending();
                *count = pending;
                return pending? eRW_Success: eRW_Eof;
            }

    protected:
        void x_SetVec(void)
            {
                m_CurPos = 0;
                m_CurSize = m_CurVec == m_Input.end()? 0: (**m_CurVec).size();
            }
        size_t x_Pending(void)
            {
                size_t size;
                while ( (size = m_CurSize - m_CurPos) == 0 &&
                        m_CurVec != m_Input.end() ) {
                    ++m_CurVec;
                    x_SetVec();
                }
                return size;
            }
    private:
        const TOctetStringSequence& m_Input;
        TOctetStringSequence::const_iterator m_CurVec;
        size_t m_CurPos;
        size_t m_CurSize;
    };

    class CReadDataObjectHookSkip : public CReadClassMemberHook
    {
    public:
        virtual void ReadClassMember(CObjectIStream& in,
                                     const CObjectInfoMI& member) {
            CID2_Reply_Data& data =
                *(CID2_Reply_Data*)member.GetClassObject().GetObjectPtr();
            for ( CIStreamContainerIterator it(in, member.GetMemberType());
                  it; it.NextElement(), ++it ) {
                CObjectIStream::ByteBlock block(in);
                    
                char buf[4096];
                size_t count;
                while ( (count = block.Read(buf, sizeof(buf))) != 0 ) {
                }
                    
                block.End();
            }
            data.SetData();
        }
    };

    class COSSPipeReader : public IReader
    {
    public:
        COSSPipeReader(CObjectIStream& in,
                       const CObjectInfoMI& member)
            : m_Input(in),
              m_Iter(in, member.GetMemberType()) {
        }

        ~COSSPipeReader() {
            do {
                x_CloseBlock();
            } while ( x_OpenBlock() );
        }
        
        virtual ERW_Result Read(void* buffer,
                                size_t  buffer_size,
                                size_t* bytes_read = 0) {
            for ( ;; ) {
                if ( m_Block.get() ) {
                    size_t count = m_Block->Read(buffer, buffer_size);
                    if ( count != 0 ) {
                        if ( bytes_read ) {
                            *bytes_read = count;
                        }
                        return eRW_Success;
                    }
                    else {
                        x_CloseBlock();
                    }
                }
                else {
                    if ( !x_OpenBlock() ) {
                        if ( bytes_read ) {
                            *bytes_read = 0;
                        }
                        return eRW_Eof;
                    }
                }
            }
        }
        virtual ERW_Result PendingCount(size_t* /*count*/) {
            return eRW_NotImplemented;
        }
    protected:
        bool x_OpenBlock(void) {
            if ( !m_Iter ) {
                return false;
            }
            m_Block.reset(new CObjectIStream::ByteBlock(m_Input));
            return true;
        }
        void x_CloseBlock(void) {
            if ( m_Block.get() ) {
                m_Block->End();
                m_Block.reset();
                m_Iter.NextElement();
                ++m_Iter;
            }
        }
    private:
        CObjectIStream& m_Input;
        CIStreamContainerIterator m_Iter;
        AutoPtr<CObjectIStream::ByteBlock> m_Block;
    };

    class CReadDataObjectHook : public CReadClassMemberHook
    {
    public:
        CObjectInfo m_Object;

        virtual void ReadClassMember(CObjectIStream& in,
                                     const CObjectInfoMI& member) {
            CID2_Reply_Data& data =
                *(CID2_Reply_Data*)member.GetClassObject().GetObjectPtr();

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
                ERR_FATAL("Unknown data type in ID2_Reply_Data");
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
                ERR_FATAL("Unknown data format in ID2_Reply_Data");
            }
                
            {{
                COSSPipeReader reader(in, member);
                CRStream stream(&reader);
                AutoPtr<CObjectIStream> obj_stream;
                    
                switch ( data.GetData_compression() ) {
                case CID2_Reply_Data::eData_compression_none:
                {
                    obj_stream.reset(CObjectIStream::Open(format, stream));
                    break;
                }
                case CID2_Reply_Data::eData_compression_gzip:
                {
                    obj_stream.reset
                        (CObjectIStream::Open
                         (format,
                          *new CCompressionIStream(stream,
                                                   new CZipStreamDecompressor,
                                                   CCompressionIStream::fOwnProcessor),
                          eTakeOwnership));
                    break;
                }
                default:
                    ERR_FATAL("Unknown data compression in ID2_Reply_Data");
                }
                _ASSERT( obj_stream.get() );
                obj_stream->UseMemoryPool();
                m_Object = CObjectInfo(obj_type);
                obj_stream->Read(m_Object.GetObjectPtr(), obj_type);
            }}

            data.SetData();
        }
    };
}


void CId2FetchApp::x_ReadReply(CID2_Reply& reply)
{
    // Read server response in ASN.1 binary format
    if ( m_ID2Conn ) {
        CObjectIStreamAsnBinary id2_server_input(*m_ID2Conn);
        id2_server_input >> reply;
    }
    else {
        CObjectIStreamAsnBinary id2_server_input(*m_PubSeqOSReply);
        id2_server_input >> reply;
    }
}


void CId2FetchApp::x_ReadReply(CID2_Reply& reply, CObjectInfo& object)
{
    // Read server response in ASN.1 binary format
    CRef<CReadDataObjectHook> hook(new CReadDataObjectHook);
    CObjectHookGuard<CID2_Reply_Data> guard("data", *hook);
    if ( m_ID2Conn ) {
        CObjectIStreamAsnBinary id2_server_input(*m_ID2Conn);
        id2_server_input >> reply;
    }
    else {
        CObjectIStreamAsnBinary id2_server_input(*m_PubSeqOSReply);
        id2_server_input >> reply;
    }
    object = hook->m_Object;
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

    if ( false && dump ) {
        CNcbiOstrstream ostr;
        ostr << MSerial_AsnText << packet;
        LOG_POST("\nProcessing request:\n" << ostr.rdbuf());
    }

    x_SendRequestPacket(packet);

    size_t remaining_count = packet.Set().size();

    CID2_Reply reply;

    double time_first = 0;
    while ( remaining_count > 0 ) {
        if ( m_PipeData ) {
            CObjectInfo object;
            x_ReadReply(reply, object);
            if ( !time_first ) time_first = sw.Elapsed();
            if ( object && m_DataFile ) {
                x_SaveDataObject(object, *m_DataFile);
            }
        }
        else {
            x_ReadReply(reply);
            if ( !time_first ) time_first = sw.Elapsed();
            if ( m_ParseData || m_SkipData  ||  m_DataFile ) {
                CTypeIterator<CID2_Reply_Data> iter = Begin(reply);
                if ( iter && iter->IsSetData() ) {
                    if ( m_ParseData || m_DataFile ) {
                        x_ProcessData(*iter);
                    }
                    if ( m_SkipData ) {
                        iter->ResetData();
                        iter->SetData();
                    }
                }
            }
        }
        if ( dump && m_OutFile ) {
            AutoPtr<CObjectOStream> id2_client_output
                (CObjectOStream::Open(m_Format, *m_OutFile));

            *id2_client_output << reply;
            if (m_Format == eSerial_AsnText  ||  m_Format == eSerial_Xml) {
                *m_OutFile << NcbiEndl;
            }
        }
        if ( reply.IsSetEnd_of_reply() ) {
            --remaining_count;
        }
    }
    LOG_POST("Packet processed in " << sw.Elapsed()<< " first at "<<time_first);
}


void CId2FetchApp::x_ProcessData(const CID2_Reply_Data& data)
{
    _ASSERT( data.IsSetData() );

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
        ERR_FATAL("Unknown data type in ID2_Reply_Data");
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
        ERR_FATAL("Unknown data format in ID2_Reply_Data");
    }

    COSSReader reader(data.GetData());
    CRStream stream(&reader);
    AutoPtr<CObjectIStream> obj_stream;
    
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
            CCompressionIStream::fOwnProcessor)), eTakeOwnership));
        break;
    }
    default:
        ERR_FATAL("Unknown data compression in ID2_Reply_Data");
    }
    _ASSERT( obj_stream.get() );
    obj_stream->UseMemoryPool();
    AutoPtr<CObjectOStream> out_stream;
    if ( m_DataFile ) {
        out_stream.reset(CObjectOStream::Open(m_Format, *m_DataFile));
    }
    if ( m_CopyData && out_stream.get() ) {
        CObjectStreamCopier copier(*obj_stream, *out_stream);
        copier.Copy(obj_type);
    }
    else {
        CObjectInfo obj(obj_type);
        obj_stream->Read(obj.GetObjectPtr(), obj.GetTypeInfo());
        if ( out_stream.get() ) {
            out_stream->Write(obj.GetObjectPtr(), obj.GetTypeInfo());
        }
    }
    if ( out_stream.get() && m_Format != eSerial_AsnBinary) {
        out_stream->FlushBuffer();
        *m_DataFile << '\n';
    }
}


void CId2FetchApp::x_SaveDataObject(const CObjectInfo& object,
                                    CNcbiOstream& out)
{
    AutoPtr<CObjectOStream> out_stream(
        CObjectOStream::Open(m_Format, out));
    out_stream->Write(object.GetObjectPtr(), object.GetTypeInfo());
    if ( m_Format != eSerial_AsnBinary) {
        out_stream->FlushBuffer();
        out << '\n';
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

    m_OutFile = 0;
    m_DataFile = 0;
    if ( args["fmt"].AsString() != "none" ) {
        if ( args["data"] ) {
            m_DataFile = &args["data"].AsOutputFile();
        }
        if ( !m_DataFile || args["out"].AsString() != "-" ) {
            m_OutFile = &args["out"].AsOutputFile();
        }
    }
    const string& fmt = args["fmt"].AsString();
    if        (fmt == "asn") {
        m_Format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        m_Format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        m_Format = eSerial_Xml;
    }
    m_SkipData = args["skip_data"];
    m_CopyData = args["copy_data"];
    m_ParseData = args["parse_data"];
    m_PipeData = args["pipe_data"];

    int count = args["count"].AsInteger();

    if ( args["pubseqos"] ) {
        x_InitPubSeqConnection(args["pubseqos"].AsString(), args["show_init"]);
    }
    else {
        x_InitConnection(args["server"].AsString(), args["show_init"]);
    }

    typedef vector<CRef<CID2_Request_Packet> > TReqs;
    TReqs reqs;

    if ( args["gi"] ) {
        TGi gi = GI_FROM(int, args["gi"].AsInteger());
        CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
        reqs.push_back(packet);
        CRef<CID2_Request> req(new CID2_Request);
        packet->Set().push_back(req);

        req->SetRequest().SetGet_blob_info().SetBlob_id().SetResolve().
            SetRequest().SetSeq_id().SetSeq_id().SetSeq_id().SetGi(gi);
        req->SetRequest().SetGet_blob_info().SetGet_data();
    }
    else if ( args["id"] ) {
        CRef<CSeq_id> id(new CSeq_id(args["id"].AsString()));
        CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
        reqs.push_back(packet);
        CRef<CID2_Request> req(new CID2_Request);
        packet->Set().push_back(req);

        req->SetRequest().SetGet_blob_info().SetBlob_id().SetResolve().
            SetRequest().SetSeq_id().SetSeq_id().SetSeq_id(*id);
        req->SetRequest().SetGet_blob_info().SetGet_data();
    }
    else if ( args["blob_id"] ) {
        vector<string> vv;
        NStr::Tokenize(args["blob_id"].AsString(), ",", vv);
        if ( vv.size() != 2 ) {
            ERR_FATAL("Bad blob_id format: "<<args["blob_id"]);
        }
        int sat = NStr::StringToInt(vv[0]);
        int sat_key = NStr::StringToInt(vv[1]);
        CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
        reqs.push_back(packet);
        CRef<CID2_Request> req(new CID2_Request);
        packet->Set().push_back(req);
        CID2_Blob_Id& blob_id = 
            req->SetRequest().SetGet_blob_info().SetBlob_id().SetBlob_id();
        blob_id.SetSat(sat);
        blob_id.SetSat_key(sat_key);
        req->SetRequest().SetGet_blob_info().SetGet_data();
    }
    else if ( args["req"] ) {
        CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
        reqs.push_back(packet);
        CRef<CID2_Request> req(new CID2_Request);
        packet->Set().push_back(req);

        string text = args["req"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request ::= " + text;
        }
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> *req;
    }
    else if ( args["packet"] ) {
        CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
        reqs.push_back(packet);

        string text = args["packet"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "ID2-Request-Packet ::= " + text;
        }
        CID2_Request_Packet id2_packet;
        CNcbiIstrstream in(text.data(), text.size());
        in >> MSerial_AsnText >> *packet;
    }
    else if ( args["in"] ) {
        AutoPtr<CObjectIStream> req_input
            (CObjectIStream::Open(eSerial_AsnText, args["in"].AsInputFile()));

        while ( !req_input->EndOfData() ) {
            string type = req_input->ReadFileHeader();
            if (type == "ID2-Request") {
                CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
                reqs.push_back(packet);
                CRef<CID2_Request> req(new CID2_Request);
                packet->Set().push_back(req);

                req_input->Read(req, req->GetThisTypeInfo(),
                                CObjectIStream::eNoFileHeader);
            }
            else if (type == "ID2-Request-Packet") {
                CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
                reqs.push_back(packet);
                
                req_input->Read(packet, packet->GetThisTypeInfo(),
                                CObjectIStream::eNoFileHeader);
            }
            else {
                ERR_FATAL("Object type must be ID2-Request or ID2-Request-Packet.");
            }
        }
    }
    else {
        ERR_FATAL("No ID2-Request specified.");
    }


    NON_CONST_ITERATE ( TReqs, it, reqs ) {
        for ( int i = 0; i < count; ++i ) {
            x_ProcessRequest(**it);
        }
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
    return CId2FetchApp().AppMain(argc, argv);
}
