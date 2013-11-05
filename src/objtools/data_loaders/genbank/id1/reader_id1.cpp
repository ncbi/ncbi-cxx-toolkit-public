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
 *
 * ===========================================================================
 *
 *  Author:  Anton Butanaev, Eugene Vasilchenko
 *
 *  File Description: Data reader from ID1
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>

#include <objtools/data_loaders/genbank/id1/reader_id1.hpp>
#include <objtools/data_loaders/genbank/id1/reader_id1_entry.hpp>
#include <objtools/data_loaders/genbank/id1/reader_id1_params.h>
#include <objtools/data_loaders/genbank/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/annot_selector.hpp>

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/id1/id1__.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include <connect/ncbi_conn_stream.hpp>
#include <util/static_map.hpp>

#include <corelib/plugin_manager_store.hpp>

#include <iomanip>


#define NCBI_USE_ERRCODE_X   Objtools_Reader_Id1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define DEFAULT_SERVICE  "ID1"
#define DEFAULT_NUM_CONN 3
#define MAX_MT_CONN      5

//#define GENBANK_ID1_RANDOM_FAILS 1
#define GENBANK_ID1_RANDOM_FAILS_FREQUENCY 20
#define GENBANK_ID1_RANDOM_FAILS_RECOVER 0 // new + write + read

namespace {
    class CDebugPrinter : public CNcbiOstrstream
    {
    public:
        CDebugPrinter(CId1Reader::TConn conn)
            {
                flush() << "CId1Reader(" << conn << "): ";
#ifdef NCBI_THREADS
                flush() << "T" << CThread::GetSelf() << ' ';
#endif
            }
        ~CDebugPrinter()
            {
                LOG_POST_X(1, rdbuf());
                /*
                DEFINE_STATIC_FAST_MUTEX(sx_DebugPrinterMutex);
                CFastMutexGuard guard(sx_DebugPrinterMutex);
                (NcbiCout << rdbuf()).flush();
                */
            }
    };
}


#ifdef GENBANK_ID1_RANDOM_FAILS
static void SetRandomFail(CConn_IOStream& stream)
{
    static int fail_recover = 0;
    if ( fail_recover > 0 ) {
        --fail_recover;
        return;
    }
    if ( random() % GENBANK_ID1_RANDOM_FAILS_FREQUENCY == 0 ) {
        fail_recover = GENBANK_ID1_RANDOM_FAILS_RECOVER;
        stream.setstate(ios::badbit);
    }
}
#else
# define SetRandomFail(stream)
#endif


NCBI_PARAM_DECL(int, GENBANK, ID1_DEBUG);
NCBI_PARAM_DECL(string, GENBANK, ID1_SERVICE_NAME);
NCBI_PARAM_DECL(string, NCBI, SERVICE_NAME_ID1);

NCBI_PARAM_DEF_EX(int, GENBANK, ID1_DEBUG, 0,
                  eParam_NoThread, GENBANK_ID1_DEBUG);
NCBI_PARAM_DEF_EX(string, GENBANK, ID1_SERVICE_NAME, kEmptyStr,
                  eParam_NoThread, GENBANK_ID1_SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, NCBI, SERVICE_NAME_ID1, DEFAULT_SERVICE,
                  eParam_NoThread, GENBANK_SERVICE_NAME_ID1);


static int GetDebugLevel(void)
{
    static const int s_Value =
        NCBI_PARAM_TYPE(GENBANK, ID1_DEBUG)::GetDefault();
    return s_Value;
}


enum EDebugLevel
{
    eTraceError = 1,
    eTraceOpen = 2,
    eTraceConn = 4,
    eTraceASN = 5,
    eTraceASNData = 8
};


CId1Reader::CId1Reader(int max_connections)
    : m_Connector(DEFAULT_SERVICE)
{
    SetMaximumConnections(max_connections, DEFAULT_NUM_CONN);
}


CId1Reader::CId1Reader(const TPluginManagerParamTree* params,
                       const string& driver_name)
{
    CConfig conf(params);
    string service_name = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_ID1_PARAM_SERVICE_NAME,
        CConfig::eErr_NoThrow,
        kEmptyStr);
    if ( service_name.empty() ) {
        service_name = NCBI_PARAM_TYPE(GENBANK,ID1_SERVICE_NAME)::GetDefault();
    }
    if ( service_name.empty() ) {
        service_name = NCBI_PARAM_TYPE(NCBI, SERVICE_NAME_ID1)::GetDefault();
    }
    m_Connector.SetServiceName(service_name);
    m_Connector.InitTimeouts(conf, driver_name);
    CReader::InitParams(conf, driver_name, DEFAULT_NUM_CONN);
}


CId1Reader::~CId1Reader()
{
}


int CId1Reader::GetMaximumConnectionsLimit(void) const
{
#ifdef NCBI_THREADS
    return MAX_MT_CONN;
#else
    return 1;
#endif
}


void CId1Reader::x_AddConnectionSlot(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CId1Reader::x_RemoveConnectionSlot(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CId1Reader::x_DisconnectAtSlot(TConn conn, bool failed)
{
    _ASSERT(m_Connections.count(conn));
    CReaderServiceConnector::SConnInfo& conn_info = m_Connections[conn];
    m_Connector.RememberIfBad(conn_info);
    if ( conn_info.m_Stream ) {
        LOG_POST_X(2, Warning << "CId1Reader("<<conn<<"): ID1"
                   " GenBank connection "<<(failed? "failed": "too old")<<
                   ": reconnecting...");
        conn_info.m_Stream.reset();
    }
}


CConn_IOStream* CId1Reader::x_GetConnection(TConn conn)
{
    _VERIFY(m_Connections.count(conn));
    CReaderServiceConnector::SConnInfo& conn_info = m_Connections[conn];
    if ( conn_info.m_Stream.get() ) {
        return conn_info.m_Stream.get();
    }
    OpenConnection(conn);
    return m_Connections[conn].m_Stream.get();
}


string CId1Reader::x_ConnDescription(CConn_IOStream& stream) const
{
    return m_Connector.GetConnDescription(stream);
}


void CId1Reader::x_ConnectAtSlot(TConn conn)
{
    CReaderServiceConnector::SConnInfo conn_info = m_Connector.Connect();

    CConn_IOStream& stream = *conn_info.m_Stream;
#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(stream);
#endif

    if ( stream.bad() ) {
        NCBI_THROW(CLoaderException, eConnectionFailed,
                   "cannot open connection: "+x_ConnDescription(stream));
    }
    
    if ( GetDebugLevel() >= eTraceOpen ) {
        CDebugPrinter s(conn);
        s << "New connection: " << x_ConnDescription(stream); 
    }

    STimeout tmout;
    m_Connector.SetTimeoutTo(&tmout);
    CONN_SetTimeout(stream.GetCONN(), eIO_ReadWrite, &tmout);
    tmout.sec = 0; tmout.usec = 1; // no wait on close
    CONN_SetTimeout(stream.GetCONN(), eIO_Close, &tmout);

    m_Connections[conn] = conn_info;
}


typedef CId1ReaderBase TRDR;
typedef SStaticPair<TRDR::ESat, TRDR::ESubSat> TSK;
typedef SStaticPair<const char*, TSK> TSI;
static const TSI sc_SatIndex[] = {
    { "ANNOT:CDD",  { TRDR::eSat_ANNOT_CDD,  TRDR::eSubSat_CDD } },
    { "ANNOT:EXON", { TRDR::eSat_ANNOT,      TRDR::eSubSat_Exon } },
    { "ANNOT:HPRD", { TRDR::eSat_ANNOT,      TRDR::eSubSat_HPRD } },
    { "ANNOT:MGC",  { TRDR::eSat_ANNOT,      TRDR::eSubSat_MGC } },
    { "ANNOT:microRNA", { TRDR::eSat_ANNOT,  TRDR::eSubSat_microRNA } },
    { "ANNOT:SNP",  { TRDR::eSat_ANNOT,      TRDR::eSubSat_SNP } },
    { "ANNOT:SNP GRAPH",{ TRDR::eSat_ANNOT,  TRDR::eSubSat_SNP_graph } },
    { "ANNOT:STS",  { TRDR::eSat_ANNOT,      TRDR::eSubSat_STS } },
    { "ANNOT:TRNA", { TRDR::eSat_ANNOT,      TRDR::eSubSat_tRNA } },
    { "ti",         { TRDR::eSat_TRACE,      TRDR::eSubSat_main } },
    { "TR_ASSM_CH", { TRDR::eSat_TR_ASSM_CH, TRDR::eSubSat_main } },
    { "TRACE_ASSM", { TRDR::eSat_TRACE_ASSM, TRDR::eSubSat_main } },
    { "TRACE_CHGR", { TRDR::eSat_TRACE_CHGR, TRDR::eSubSat_main } }
};
typedef CStaticPairArrayMap<const char*, TSK, PNocase_CStr> TSatMap;
DEFINE_STATIC_ARRAY_MAP(TSatMap, sc_SatMap, sc_SatIndex);


bool CId1Reader::LoadSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedGi() ) {
        return true;
    }

    CID1server_request id1_request;
    id1_request.SetGetgi(const_cast<CSeq_id&>(*seq_id.GetSeqId()));

    CID1server_back id1_reply;
    x_ResolveId(result, id1_reply, id1_request);

    TGi gi;
    if ( id1_reply.IsGotgi() ) {
        gi = id1_reply.GetGotgi();
    }
    else {
        gi = ZERO_GI;
    }
    SetAndSaveSeq_idGi(result, seq_id, ids, gi);
    return true;
}


bool CId1Reader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }
    
    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        return LoadGiSeq_ids(result, seq_id);
    }

    if ( seq_id.Which() == CSeq_id::e_General ) {
        // accept some general Seq-ids
        CConstRef<CSeq_id> id_ref = seq_id.GetSeqId();
        const CDbtag& dbtag = id_ref->GetGeneral();
        Int8 id;
        if ( dbtag.GetTag().GetIdType(id) == CObject_id::e_Id && id > 0 &&
             sc_SatMap.find(dbtag.GetDb().c_str()) != sc_SatMap.end() ) {
            // only one source Seq-id and no synonyms
            TSeqIds seq_ids;
            seq_ids.push_back(seq_id);
            ids.SetLoadedSeq_ids(0, CFixedSeq_ids(eTakeOwnership, seq_ids));
            return true;
        }
    }
    
    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }
    TGi gi = ids->GetGi();
    if (gi == ZERO_GI) {
        // no gi -> no Seq-ids
        ids.SetNoSeq_ids(0);
        return true;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    CLoadLockSeq_ids gi_ids(result, gi_handle);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);

    // copy Seq-id list from gi to original seq-id
    ids.SetLoadedSeq_ids(gi_ids);
    return true;
}


bool CId1Reader::LoadGiSeq_ids(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoaded() ) {
        return true;
    }
    
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    TGi gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == ZERO_GI ) {
        ids.SetNoSeq_ids(0);
        return true;
    }

    CID1server_request id1_request;
    {{
        id1_request.SetGetseqidsfromgi(gi);
    }}
    
    CID1server_back id1_reply;
    x_ResolveId(result, id1_reply, id1_request);

    if ( !id1_reply.IsIds() ) {
        ids.SetNoSeq_ids(0);
        return true;
    }

    TSeqIds seq_ids;
    const CID1server_back::TIds& reply_ids = id1_reply.GetIds();
    ITERATE(CID1server_back::TIds, it, reply_ids) {
        seq_ids.push_back(CSeq_id_Handle::GetHandle(**it));
    }
    SetAndSaveSeq_idSeq_ids(result, seq_id, ids, 0,
                            CFixedSeq_ids(eTakeOwnership, seq_ids));
    return true;
}


bool CId1Reader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id,
                                   const SAnnotSelector* sel)
{
    CLoadLockBlob_ids ids(result, seq_id, sel);
    if ( ids.IsLoaded() ) {
        return true;
    }
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        return CReader::LoadSeq_idBlob_ids(result, seq_id, sel);
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        return LoadGiBlob_ids(result, seq_id);
    }

    if ( seq_id.Which() == CSeq_id::e_General ) {
        CConstRef<CSeq_id> id_ref = seq_id.GetSeqId();
        const CSeq_id& id = *id_ref;
        if ( id.GetGeneral().GetTag().IsId() ) {
            const CDbtag& dbtag = id.GetGeneral();
            const string& db = dbtag.GetDb();
            int num = dbtag.GetTag().GetId();
            if ( num != 0 ) {
                TSatMap::const_iterator iter = sc_SatMap.find(db.c_str());
                if ( iter != sc_SatMap.end() ) {
                    TBlobIds blob_ids;
                    CRef<CBlob_id> blob_id(new CBlob_id);
                    blob_id->SetSat(iter->second.first);
                    blob_id->SetSatKey(num);
                    blob_id->SetSubSat(iter->second.second);
                    blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
                    ids.SetLoadedBlob_ids(0, CFixedBlob_ids(eTakeOwnership, blob_ids));
                    return true;
                }
            }
        }
    }

    CLoadLockSeq_ids seq_ids(result, seq_id);
    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    TGi gi = seq_ids->GetGi();
    if (gi == ZERO_GI) {
        // no gi -> no blobs
        ids.SetNoBlob_ids(0);
        return true;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    CLoadLockBlob_ids gi_ids(result, gi_handle, 0);
    m_Dispatcher->LoadSeq_idBlob_ids(result, gi_handle, 0);

    // copy Seq-id list from gi to original seq-id
    ids.SetLoadedBlob_ids(gi_ids);
    return true;
}


const int kSat_BlobError = -1;

bool CId1Reader::LoadGiBlob_ids(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    CLoadLockBlob_ids ids(result, seq_id, 0);
    if ( ids.IsLoaded() ) {
        return true;
    }
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    TGi gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == ZERO_GI ) {
        ids.SetNoBlob_ids(0);
        return true;
    }

    CID1server_request id1_request;
    {{
        CID1server_maxcomplex& blob = id1_request.SetGetblobinfo();
        blob.SetMaxplex(eEntry_complexities_entry);
        blob.SetGi(gi);
    }}
    
    CID1server_back id1_reply;
    TBlobState state = x_ResolveId(result, id1_reply, id1_request);

    if ( !id1_reply.IsGotblobinfo() ) {
        if ( !state ) {
            state = CBioseq_Handle::fState_other_error;
        }
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, ids, state);
        return true;
    }

    const CID1blob_info& info = id1_reply.GetGotblobinfo();
    if (info.GetWithdrawn() > 0) {
        state = CBioseq_Handle::fState_withdrawn;
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, ids, state);
        return true;
    }
    if (info.GetConfidential() > 0) {
        state = CBioseq_Handle::fState_confidential;
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, ids, state);
        return true;
    }
    if ( info.GetSat() < 0 || info.GetSat_key() < 0 ) {
        LOG_POST_X(3, Warning<<"CId1Reader: gi "<<gi<<" negative sat/satkey");
        if ( !state ) {
            state = CBioseq_Handle::fState_other_error;
        }
        SetAndSaveNoSeq_idBlob_ids(result, seq_id, 0, ids, state);
        return true;
    }
    TBlobIds blob_ids;
    if ( CProcessor::TrySNPSplit() ) {
        {{
            // add main blob
            CRef<CBlob_id> blob_id(new CBlob_id);
            blob_id->SetSat(info.GetSat());
            blob_id->SetSatKey(info.GetSat_key());
            blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
        }}
        if ( info.IsSetExtfeatmask() ) {
            int ext_feat = info.GetExtfeatmask();
            while ( ext_feat ) {
                int bit = ext_feat & ~(ext_feat-1);
                ext_feat -= bit;
                CRef<CBlob_id> blob_id(new CBlob_id);
                blob_id->SetSat(GetAnnotSat(bit));
                blob_id->SetSatKey(GI_TO(int, gi));
                blob_id->SetSubSat(bit);
                blob_ids.push_back(CBlob_Info(blob_id, fBlobHasExtAnnot));
            }
        }
    }
    else {
        // whole blob
        CRef<CBlob_id> blob_id(new CBlob_id);
        blob_id->SetSat(info.GetSat());
        blob_id->SetSatKey(info.GetSat_key());
        if ( info.IsSetExtfeatmask() ) {
            blob_id->SetSubSat(info.GetExtfeatmask());
        }
        blob_ids.push_back(CBlob_Info(blob_id, fBlobHasAllLocal));
    }
    SetAndSaveSeq_idBlob_ids(result, seq_id, 0, ids, state,
                             CFixedBlob_ids(eTakeOwnership, blob_ids));
    return true;
}


void CId1Reader::GetBlobVersion(CReaderRequestResult& result,
                                const CBlob_id& blob_id)
{
    CID1server_request id1_request;
    x_SetParams(id1_request.SetGetblobinfo(), blob_id);
    
    CID1server_back    reply;
    TBlobState state = x_ResolveId(result, reply, id1_request);

    TBlobVersion version = -1;
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        version = abs(reply.GetGotblobinfo().GetBlob_state());
        break;
    case CID1server_back::e_Gotsewithinfo:
        version = abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
        break;
    case CID1server_back::e_Error:
        version = 0;
        break;
    default:
        ERR_POST_X(5, "CId1Reader::GetBlobVersion: "
                      "invalid ID1server-back.");
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId1Reader::GetBlobVersion: "
                   "invalid ID1server-back");
    }

    if ( version >= 0 ) {
        SetAndSaveBlobVersion(result, blob_id, version);
    }
    if ( state ) {
        SetAndSaveNoBlob(result, blob_id, CProcessor::kMain_ChunkId, state);
    }
}


CReader::TBlobVersion
CId1Reader::x_ResolveId(CReaderRequestResult& result,
                        CID1server_back& reply,
                        const CID1server_request& request)
{
    CConn conn(result, this);
    x_SendRequest(conn, request);
    x_ReceiveReply(conn, reply);
    if ( !reply.IsError() ) {
        conn.Release();
        return 0;
    }
    TBlobState state = 0;
    int error = reply.GetError();
    switch ( error ) {
    case 1:
        state =
            CBioseq_Handle::fState_withdrawn|
            CBioseq_Handle::fState_no_data;
        break;
    case 2:
        state =
            CBioseq_Handle::fState_confidential|
            CBioseq_Handle::fState_no_data;
        break;
    case 10:
        state = CBioseq_Handle::fState_no_data;
        break;
    case 100:
        NCBI_THROW_FMT(CLoaderException, eConnectionFailed,
                       "ID1server-back.error "<<error);
    default:
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "unknown ID1server-back.error "<<error);
    }
    conn.Release();
    return state;
}


void CId1Reader::GetBlob(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         TChunkId chunk_id)
{
    CConn conn(result, this);
    if ( chunk_id == CProcessor::kMain_ChunkId ) {
        CLoadLockBlob blob(result, blob_id);
        if ( blob.IsLoaded() ) {
            conn.Release();
            return;
        }
    }
    {{
        CID1server_request request;
        x_SetBlobRequest(request, blob_id);
        x_SendRequest(conn, request);
    }}
    CProcessor::EType processor_type;
    if ( blob_id.GetSubSat() == eSubSat_SNP ) {
        processor_type = CProcessor::eType_ID1_SNP;
    }
    else {
        processor_type = CProcessor::eType_ID1;
    }
    CConn_IOStream* stream = x_GetConnection(conn);
    try {
        m_Dispatcher->GetProcessor(processor_type)
            .ProcessStream(result, blob_id, chunk_id, *stream);
    }
    catch ( CException& exc ) {
        NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                     "failed to receive reply: "+
                     x_ConnDescription(*stream));
    }
    conn.Release();
}


void CId1Reader::x_SetBlobRequest(CID1server_request& request,
                                  const CBlob_id& blob_id)
{
    x_SetParams(request.SetGetsewithinfo(), blob_id);
}


void CId1Reader::x_SetParams(CID1server_maxcomplex& params,
                             const CBlob_id& blob_id)
{
    int bits = (~blob_id.GetSubSat() & 0xffff) << 4;
    params.SetMaxplex(eEntry_complexities_entry | bits);
    params.SetGi(ZERO_GI);
    params.SetEnt(blob_id.GetSatKey());
    int sat = blob_id.GetSat();
    if ( IsAnnotSat(sat) ) {
        params.SetMaxplex(eEntry_complexities_entry); // TODO: TEMP: remove
        params.SetSat("ANNOT:"+NStr::IntToString(blob_id.GetSubSat()));
    }
    else {
        params.SetSat(NStr::IntToString(sat));
    }
}


void CId1Reader::x_SendRequest(const CBlob_id& blob_id, TConn conn)
{
    CID1server_request id1_request;
    x_SetParams(id1_request.SetGetsefromgi(), blob_id);
    x_SendRequest(conn, id1_request);
}


void CId1Reader::x_SendRequest(TConn conn,
                               const CID1server_request& request)
{
    CConn_IOStream* stream = x_GetConnection(conn);

#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(*stream);
#endif
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s(conn);
        s << "Sending";
        if ( GetDebugLevel() >= eTraceASN ) {
            s << ": " << MSerial_AsnText << request;
        }
        else {
            s << " ID1server-request";
        }
        s << "...";
    }
    try {
        CObjectOStreamAsnBinary out(*stream);
        out << request;
        out.Flush();
    }
    catch ( CException& exc ) {
        NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                     "failed to send request: "+
                     x_ConnDescription(*stream));
    }
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s(conn);
        s << "Sent ID1server-request.";
    }
}


void CId1Reader::x_ReceiveReply(TConn conn,
                                CID1server_back& reply)
{
    CConn_IOStream* stream = x_GetConnection(conn);

#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(*stream);
#endif
    if ( GetDebugLevel() >= eTraceConn ) {
        CDebugPrinter s(conn);
        s << "Receiving ID1server-back...";
    }
    try {
        CObjectIStreamAsnBinary in(*stream);
        in >> reply;
    }
    catch ( CException& exc ) {
        NCBI_RETHROW(exc, CLoaderException, eConnectionFailed,
                     "failed to receive reply: "+
                     x_ConnDescription(*stream));
    }
    if ( GetDebugLevel() >= eTraceConn   ) {
        CDebugPrinter s(conn);
        s << "Received";
        if ( GetDebugLevel() >= eTraceASN ) {
            s << ": " << MSerial_AsnText << reply;
        }
        else {
            s << " ID1server-back.";
        }
    }
}

END_SCOPE(objects)

void GenBankReaders_Register_Id1(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_Id1Reader);
}


/// Class factory for ID1 reader
///
/// @internal
///
class CId1ReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CId1Reader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CId1Reader> TParent;
public:
    CId1ReaderCF()
        : TParent(NCBI_GBLOADER_READER_ID1_DRIVER_NAME, 0) {}
    ~CId1ReaderCF() {}

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
            drv = new objects::CId1Reader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_Id1Reader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CId1ReaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_id1(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_Id1Reader(info_list, method);
}


END_NCBI_SCOPE
