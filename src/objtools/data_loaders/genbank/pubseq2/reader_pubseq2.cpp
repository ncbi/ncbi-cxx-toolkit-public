/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: ID2 Data reader via PubSeqOS
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/pubseq2/reader_pubseq2.hpp>
#include <objtools/data_loaders/genbank/pubseq2/reader_pubseq2_entry.hpp>
#include <objtools/data_loaders/genbank/pubseq2/reader_pubseq2_params.h>
#include <objtools/data_loaders/genbank/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <corelib/ncbicntr.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/rwstream.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/serial.hpp>

#include <objects/id2/id2__.hpp>

#define BINARY_REQUESTS     1
#define LONG_REQUESTS       1

#if BINARY_REQUESTS
# include <serial/objostrasnb.hpp>
#define CRequestSerializer CObjectOStreamAsnBinary
#else
# include <serial/objostrasn.hpp>
#define CRequestSerializer CObjectOStreamAsn
#endif
#if LONG_REQUESTS
# define MAX_ASN_IN          (20*1024)
#else
# define MAX_ASN_IN          255
#endif

#define DEFAULT_DB_SERVER   "PUBSEQ_OS_PUBLIC"
#define DEFAULT_DB_USER     "anyone"
#define DEFAULT_DB_PASSWORD "allowed"
#define DEFAULT_DB_DRIVER   "ftds;ctlib"
#define DEFAULT_NUM_CONN    2
#define MAX_MT_CONN         5
#define DEFAULT_EXCL_WGS_MASTER false

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Pubseq2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif


CPubseq2Reader::CPubseq2Reader(int max_connections,
                               const string& server,
                               const string& user,
                               const string& pswd,
                               const string& dbapi_driver)
    : m_Server(server) , m_User(user), m_Password(pswd),
      m_DbapiDriver(dbapi_driver),
      m_Context(0),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER)
{
    if ( m_Server.empty() ) {
        m_Server = DEFAULT_DB_SERVER;
    }
    if ( m_User.empty() ) {
        m_User = DEFAULT_DB_USER;
    }
    if ( m_Password.empty() ) {
        m_Password = DEFAULT_DB_PASSWORD;
    }
    if ( m_DbapiDriver.empty() ) {
        m_DbapiDriver = DEFAULT_DB_DRIVER;
    }

#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif
    SetMaximumConnections(max_connections, DEFAULT_NUM_CONN);
}


CPubseq2Reader::CPubseq2Reader(const TPluginManagerParamTree* params,
                             const string& driver_name)
    : m_Context(0),
      m_ExclWGSMaster(DEFAULT_EXCL_WGS_MASTER)
{
    CConfig conf(params);
    m_Server = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ2_PARAM_SERVER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_SERVER);
    m_User = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ2_PARAM_USER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_USER);
    m_Password = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ2_PARAM_PASSWORD,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_PASSWORD);
    m_DbapiDriver = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ2_PARAM_DRIVER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_DRIVER);
#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif
    m_ExclWGSMaster = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ2_PARAM_EXCL_WGS_MASTER,
        CConfig::eErr_NoThrow,
        DEFAULT_EXCL_WGS_MASTER);

    CReader::InitParams(conf, driver_name, DEFAULT_NUM_CONN);
}


CPubseq2Reader::~CPubseq2Reader()
{
#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    s_pubseq_readers.Add(-1);
#endif
}


int CPubseq2Reader::GetMaximumConnectionsLimit(void) const
{
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    return MAX_MT_CONN;
#else
    return 1;
#endif
}


void CPubseq2Reader::x_AddConnectionSlot(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CPubseq2Reader::x_RemoveConnectionSlot(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CPubseq2Reader::x_DisconnectAtSlot(TConn conn, bool failed)
{
    _ASSERT(m_Connections.count(conn));
    SConnection& c = m_Connections[conn];
    if ( c.m_Connection ) {
        LOG_POST_X(1, Warning << "CPubseq2Reader("<<conn<<"): PubSeqOS2"
                   " GenBank connection "<<(failed? "failed": "too old")<<
                   ": reconnecting...");
        c.m_Result.reset();
        c.m_Connection.reset();
    }
}


string CPubseq2Reader::x_ConnDescription(TConn conn) const
{
    return "";
}


CDB_Connection& CPubseq2Reader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    SConnection& c = m_Connections[conn];
    if ( c.m_Connection.get() ) {
        c.m_Result.reset();
        return *c.m_Connection;
    }
    OpenConnection(conn);
    SConnection& c2 = m_Connections[conn];
    c2.m_Result.reset();
    return *c2.m_Connection;
}


CObjectIStream& CPubseq2Reader::x_GetCurrentResult(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    TConnections::iterator iter = m_Connections.find(conn);
    if ( iter == m_Connections.end() || !iter->second.m_Result ) {
        NCBI_THROW(CLoaderException, eOtherError,
                   "CPubseq2Reader: no active command");
    }
    return *iter->second.m_Result;
}


void CPubseq2Reader::x_SetCurrentResult(TConn conn,
                                        AutoPtr<CObjectIStream> result)
{
    _ASSERT(m_Connections.count(conn));
    TConnections::iterator iter = m_Connections.find(conn);
    if ( iter == m_Connections.end() ) {
        NCBI_THROW(CLoaderException, eOtherError,
                   "CPubseq2Reader: no active connection");
    }
    iter->second.m_Result = result;
}


namespace {
    class CPubseq2Validator : public IConnValidator
    {
    public:
        typedef CPubseq2Reader::TConn TConn;

        CPubseq2Validator(CPubseq2Reader* reader,
                          TConn conn,
                          bool excl_wgs_master)
            : m_Reader(reader),
              m_Conn(conn),
              m_ExclWGSMaster(excl_wgs_master)
            {
            }
        
        virtual EConnStatus Validate(CDB_Connection& conn) {
            if ( m_ExclWGSMaster ) {
                AutoPtr<CDB_LangCmd> cmd
                    (conn.LangCmd("set exclude_wgs_master on"));
                cmd->Send();
                cmd->DumpResults();
            }
            m_Reader->x_InitConnection(conn, m_Conn);
            return eValidConn;
        }

        virtual string GetName(void) const {
            return "CPubseq2Validator";
        }

    private:
        CPubseq2Reader* m_Reader;
        TConn m_Conn;
        bool m_ExclWGSMaster;
    };
    
    bool sx_FetchNextItem(CDB_Result& result, const CTempString& name)
    {
        while ( result.Fetch() ) {
            for ( size_t pos = 0; pos < result.NofItems(); ++pos ) {
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
                        m_DB_RPCCmd->DumpResults();
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


void CPubseq2Reader::x_ConnectAtSlot(TConn conn_)
{
    if ( !m_Context ) {
        DBLB_INSTALL_DEFAULT();
        C_DriverMgr drvMgr;
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        args["version"]="125"; // for correct connection to OpenServer
        vector<string> driver_list;
        NStr::Tokenize(m_DbapiDriver, ";", driver_list);
        size_t driver_count = driver_list.size();
        vector<string> errmsg(driver_count);
        for ( size_t i = 0; i < driver_count; ++i ) {
            try {
                m_Context = drvMgr.GetDriverContext(driver_list[i],
                                                    &errmsg[i], &args);
                if ( m_Context )
                    break;
            }
            catch ( CException& exc ) {
                errmsg[i] = exc.what();
            }
        }
        if ( !m_Context ) {
            for ( size_t i = 0; i < driver_count; ++i ) {
                LOG_POST_X(2, "Failed to create dbapi context with driver '"
                           <<driver_list[i]<<"': "<<errmsg[i]);
            }
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Cannot create dbapi context with driver '"+
                       m_DbapiDriver+"'");
        }

        m_Context->SetTimeout(20);
    }

    CPubseq2Validator validator(this, conn_, m_ExclWGSMaster);
    AutoPtr<CDB_Connection> conn
        (m_Context->ConnectValidated(m_Server, m_User, m_Password, validator));
    
    if ( !conn.get() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed, "connection failed");
    }

    if ( GetDebugLevel() >= 2 ) {
        NcbiCout << "CPubseq2Reader::Connected to " << conn->ServerName()
                 << NcbiEndl;
    }

    m_Connections[conn_].m_Connection.reset(conn.release());
}


void CPubseq2Reader::x_InitConnection(CDB_Connection& db_conn, TConn conn)
{
    // prepare init request
    CID2_Request req;
    req.SetRequest().SetInit();
    x_SetContextData(req);
    CID2_Request_Packet packet;
    packet.Set().push_back(Ref(&req));
    // that's it for now
    // TODO: add params

    AutoPtr<CObjectIStream> result;
    // send init request
    {{
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn, "CPubseq2Reader");
            s << "Sending";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << packet;
            }
            else {
                s << " ID2-Request-Packet";
            }
            s << "...";
        }
        try {
            result = x_SendPacket(db_conn, conn, packet);
        }
        catch ( CException& exc ) {
            NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                         "failed to send init request");
        }
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn, "CPubseq2Reader");
            s << "Sent ID2-Request-Packet.";
        }
    }}
    
    // receive init reply
    CID2_Reply reply;
    {{
        if ( GetDebugLevel() >= eTraceConn ) {
            CDebugPrinter s(conn, "CPubseq2Reader");
            s << "Receiving ID2-Reply...";
        }
        CId2ReaderBase::x_ReceiveReply(*result, conn, reply);
        if ( GetDebugLevel() >= eTraceConn   ) {
            CDebugPrinter s(conn, "CPubseq2Reader");
            s << "Received";
            if ( GetDebugLevel() >= eTraceASN ) {
                s << ": " << MSerial_AsnText << reply;
            }
            else {
                s << " ID2-Reply.";
            }
        }
    }}

    // check init reply
    if ( reply.IsSetDiscard() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'discard' is set");
    }
    if ( reply.IsSetError() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'error' is set");
    }
    if ( !reply.IsSetEnd_of_reply() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'end-of-reply' is not set");
    }
    if ( reply.GetReply().Which() != CID2_Reply::TReply::e_Init ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad init reply: 'reply' is not 'init'");
    }
    if ( result->HaveMoreData() ) {
        ERR_POST_X(1, "More data in reply");
    }
    // that's it for now
    // TODO: process params
}


void CPubseq2Reader::x_SendPacket(TConn conn,
                                  const CID2_Request_Packet& packet)
{
    x_SetCurrentResult(conn,
                       x_SendPacket(x_GetConnection(conn), conn, packet));
}


void CPubseq2Reader::x_ReceiveReply(TConn conn,
                                    CID2_Reply& reply)
{
    CId2ReaderBase::x_ReceiveReply(x_GetCurrentResult(conn), conn, reply);
}


void CPubseq2Reader::x_EndOfPacket(TConn conn)
{
    CObjectIStream& stream = x_GetCurrentResult(conn);
    if ( stream.HaveMoreData() ) {
        ERR_POST_X(4, "CPubseq2Reader: extra blob data");
    }
    x_SetCurrentResult(conn, 0);
}


AutoPtr<CObjectIStream>
CPubseq2Reader::x_SendPacket(CDB_Connection& db_conn,
                             TConn conn,
                             const CID2_Request_Packet& packet)
{
    char buffer[MAX_ASN_IN];
    size_t size;
    {{
        CNcbiOstrstream mem_str(buffer, sizeof(buffer));
        {{
            CRequestSerializer obj_str(mem_str);
            obj_str << packet;
        }}
        if ( !mem_str ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "CPubseq2Reader: packet size overflow");
        }
        size = mem_str.pcount();
    }}
    CDB_VarChar service("ID2");
    CDB_VarChar short_asn;
    CDB_LongBinary long_asn(size);
    if ( LONG_REQUESTS ) {
        long_asn.SetValue(buffer, size);
    }
    else {
        short_asn.SetValue(buffer, size);
    }
    CDB_TinyInt text_in(!BINARY_REQUESTS);
    CDB_TinyInt text_out(0);
    
    AutoPtr<CDB_RPCCmd> cmd(db_conn.RPC("os_asn_request"));
    cmd->SetParam("@service", &service);
    cmd->SetParam("@asnin", &short_asn);
    cmd->SetParam("@text", &text_in);
    cmd->SetParam("@out_text", &text_out);
    cmd->SetParam("@asnin_long", &long_asn);
    cmd->Send();

    AutoPtr<CDB_Result> dbr;
    while( cmd->HasMoreResults() ) {
        if ( cmd->HasFailed() ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "CPubseq2Reader: failed RPC");
        }
        dbr = cmd->Result();
        if ( !dbr.get() ) {
            continue;
        }
        
        if ( dbr->ResultType() != eDB_RowResult ) {
            while ( dbr->Fetch() )
                ;
            continue;
        }
        if ( sx_FetchNextItem(*dbr, "asnout") ) {
            AutoPtr<CDB_Result_Reader> reader
                (new CDB_Result_Reader(cmd, dbr));
            AutoPtr<CRStream> stream
                (new CRStream(reader.release(), 0, 0, CRWStreambuf::fOwnAll));
            AutoPtr<CObjectIStream> obj_str
                (new CObjectIStreamAsnBinary(*stream.release(), eTakeOwnership));
            return obj_str;
        }
    }
    NCBI_THROW(CLoaderException, eOtherError,
               "CPubseq2Reader: no more results");
}


END_SCOPE(objects)

void GenBankReaders_Register_Pubseq2(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_ReaderPubseqos2);
}


/// Class factory for Pubseq reader
///
/// @internal
///
class CPubseq2ReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader,
                                   objects::CPubseq2Reader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CPubseq2Reader> TParent;
public:
    CPubseq2ReaderCF()
        : TParent(NCBI_GBLOADER_READER_PUBSEQ2_DRIVER_NAME, 0) {}

    ~CPubseq2ReaderCF() {}

    objects::CReader* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        objects::CReader* drv = 0;
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CReader)) 
                            != CVersionInfo::eNonCompatible) {
            drv = new objects::CPubseq2Reader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_ReaderPubseqos2(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CPubseq2ReaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_pubseqos2(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos2(info_list, method);
}


END_NCBI_SCOPE
