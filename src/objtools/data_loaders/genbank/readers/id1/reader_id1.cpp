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
#include <corelib/ncbi_config_value.hpp>

#include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_entry.hpp>
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <corelib/ncbimtx.hpp>

#include <objtools/data_loaders/genbank/readers/id1/statistics.hpp>

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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//#define GENBANK_ID1_RANDOM_FAILS 1
#define GENBANK_ID1_RANDOM_FAILS_FREQUENCY 20
#define GENBANK_ID1_RANDOM_FAILS_RECOVER 3 // new + write + read

#ifdef GENBANK_ID1_RANDOM_FAILS
static void SetRandomFail(CConn_ServiceStream& stream)
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


#ifdef ID1_COLLECT_STATS
static STimeStatistics resolve_id;
static STimeStatistics resolve_gi;
static STimeStatistics resolve_ver;

static STimeSizeStatistics main_read;
static STimeSizeStatistics snp_read;
static STimeSizeStatistics snp_parse;
#endif

int CId1Reader::CollectStatistics(void)
{
#ifdef ID1_COLLECT_STATS
    static int s_Value = -1;
    int value = s_Value;
    if ( value < 0 ) {
        value = GetConfigInt("GENBANK", "ID1_STATS");
        if ( value < 0 ) {
            value = 0;
        }
        s_Value = value;
    }
    return value;
#else
    return 0;
#endif
}


static int GetDebugLevel(void)
{
    static int s_Value = -1;
    int value = s_Value;
    if ( value < 0 ) {
        value = GetConfigInt("GENBANK", "ID1_DEBUG");
        if ( value < 0 ) {
            value = 0;
        }
        s_Value = value;
    }
    return value;
}


enum EDebugLevel
{
    eTraceConn = 4,
    eTraceASN = 5,
    eTraceASNData = 8
};


CId1Reader::CId1Reader(int max_connections)
{
    SetInitialConnections(max_connections);
}


CId1Reader::~CId1Reader()
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        PrintStatistics();
    }
#endif
}


void CId1Reader::PrintStat(const char* type,
                           const char* what,
                           const STimeStatistics& stat)
{
#ifdef ID1_COLLECT_STATS
    if ( !stat.count ) {
        return;
    }
    LOG_POST(type <<' '<<stat.count<<' '<<what<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (stat.time)<<" s "<<
             (stat.time*1000/stat.count)<<" ms/one");
#endif
}


void CId1Reader::PrintStat(const char* type,
                           const STimeSizeStatistics& stat)
{
#ifdef ID1_COLLECT_STATS
    if ( !stat.count ) {
        return;
    }
    LOG_POST(type<<' '<<stat.count<<" blobs "<<
             setiosflags(ios::fixed)<<
             setprecision(2)<<
             (stat.size/1024)<<" kB in "<<
             setprecision(3)<<
             (stat.time)<<" s "<<
             setprecision(2)<<
             (stat.size/stat.time/1024)<<" kB/s");
#endif
}


static string ToString(const CID1server_maxcomplex& maxplex)
{
    CNcbiOstrstream str;
    str << "TSE("<<maxplex.GetSat()<<','<<maxplex.GetEnt()<<')';
    return CNcbiOstrstreamToString(str);
}


void CId1Reader::LogIdStat(const char* type,
                           const char* kind,
                           const string& name,
                           STimeStatistics& stat,
                           CStopWatch& sw)
{
#ifdef ID1_COLLECT_STATS
    double time = sw.Restart();
    stat.add(time);
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<kind<<' '<<name<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogStat(const char* type,
                         const string& blob_id,
                         STimeSizeStatistics& stat,
                         CStopWatch& sw,
                         size_t size)
{
#ifdef ID1_COLLECT_STATS
    double time = sw.Restart();
    stat.add(time, size);
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<blob_id<<' '<<
             setiosflags(ios::fixed)<<
             setprecision(2)<<
             (size/1024)<<" kB in "<<
             setprecision(3)<<
             (time*1000)<<" ms "<<
             setprecision(2)<<
             (size/1024/time)<<" kB/s");
#endif
}


void CId1Reader::LogStat(const char* type,
                         const CBlob_id& blob_id,
                         STimeSizeStatistics& stat,
                         CStopWatch& sw,
                         size_t size)
{
#ifdef ID1_COLLECT_STATS
    LogStat(type, blob_id.ToString(), stat, sw, size);
#endif
}


void CId1Reader::PrintStatistics(void) const
{
#ifdef ID1_COLLECT_STATS
    PrintStat("ID1 resolution: resolved", "ids", resolve_id);
    PrintStat("ID1 resolution: resolved", "gis", resolve_gi);
    PrintStat("ID1 resolution: resolved", "vers", resolve_ver);
    PrintStat("ID1 non-SNP: loaded", main_read);
    PrintStat("ID1 SNP: loaded", snp_read);
    PrintStat("ID1 SNP: parsed", snp_parse);
/*
    if ( snp_parse.count ) {
        LOG_POST("ID1 SNP decompression: "<<
                 setiosflags(ios::fixed)<<
                 setprecision(2)<<
                 (snp_compressed/1024)<<" kB -> "<<
                 (snp_uncompressed/1024)<<" kB, compession ratio: "<<
                 setprecision(1)<<
                 (snp_uncompressed/snp_compressed));
        double snp_parse_time = snp_time - snp_total_read_time;
        LOG_POST("ID1 SNP times: decompression : "<<
                 setiosflags(ios::fixed)<<
                 setprecision(3)<<
                 (snp_decompression_time)<<" s, total read time: "<<
                 (snp_total_read_time)<<" s, parse time: "<<
                 (snp_parse_time)<<" s");
    }
    PrintBlobStat("ID1 total: loaded",
                  main_blob_count + snp_blob_count,
                  main_bytes + snp_compressed,
                  main_time + snp_time);
*/
#endif
}


int CId1Reader::GetMaximumConnectionsLimit(void) const
{
#ifdef NCBI_THREADS
    return 3;
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


void CId1Reader::x_DisconnectAtSlot(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CConn_ServiceStream>& stream = m_Connections[conn];
    if ( stream.get() ) {
        ERR_POST("CId1Reader: ID1 GenBank connection failed: reconnecting...");
        stream.reset();
    }
}


void CId1Reader::x_ConnectAtSlot(TConn conn)
{
    x_GetConnection(conn);
}


CConn_ServiceStream* CId1Reader::x_GetConnection(TConn conn)
{
    _VERIFY(m_Connections.count(conn));
    AutoPtr<CConn_ServiceStream>& stream = m_Connections[conn];
    if ( !stream.get() ) {
        stream.reset(x_NewConnection(conn));
    }
    return stream.get();
}


CConn_ServiceStream* CId1Reader::x_NewConnection(TConn conn)
{
    string id1_svc = GetConfigString("NCBI", "SERVICE_NAME_ID1", "ID1");

    STimeout tmout;
    tmout.sec = 20;
    tmout.usec = 0;
    
    AutoPtr<CConn_ServiceStream> stream
        (new CConn_ServiceStream(id1_svc, fSERV_Any, 0, 0, &tmout));

#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(*stream);
#endif

    if ( stream->bad() ) {
        NCBI_THROW(CLoaderException, eNoConnection, "connection failed");
    }
    
    if ( GetDebugLevel() >= eTraceConn ) {
        NcbiCout << "CId1Reader: New connection " << conn 
                 << " to " << id1_svc << " opened." << NcbiEndl;
    }

    return stream.release();
}


typedef CId1ReaderBase TRDR;
typedef pair<TRDR::ESat, TRDR::ESubSat> TSK;
typedef pair<const char*, TSK> TSI;
static const TSI sc_SatIndex[] = {
    TSI("ANNOT:CDD",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_CDD)),
    TSI("ANNOT:MGC",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_MGC)),
    TSI("ANNOT:SNP",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_SNP)),
    TSI("ANNOT:SNP GRAPH",TSK(TRDR::eSat_ANNOT,  TRDR::eSubSat_SNP_graph)),
    //TSI("SNP",        TSK(TRDR::eSat_SNP,        TRDR::eSubSat_main)),
    TSI("ti",         TSK(TRDR::eSat_TRACE,      TRDR::eSubSat_main)),
    TSI("TR_ASSM_CH", TSK(TRDR::eSat_TR_ASSM_CH, TRDR::eSubSat_main)),
    TSI("TRACE_ASSM", TSK(TRDR::eSat_TRACE_ASSM, TRDR::eSubSat_main)),
    TSI("TRACE_CHGR", TSK(TRDR::eSat_TRACE_CHGR, TRDR::eSubSat_main))
};
typedef CStaticArrayMap<const char*, TSK, PNocase> TSatMap;
static const TSatMap sc_SatMap(sc_SatIndex, sizeof(sc_SatIndex));


bool CId1Reader::LoadSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() || ids.IsLoaded() ) {
        return true;
    }

    CID1server_request id1_request;
    id1_request.SetGetgi(const_cast<CSeq_id&>(*seq_id.GetSeqId()));

    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request);

    int gi;
    if ( id1_reply.IsGotgi() ) {
        gi = id1_reply.GetGotgi();
    }
    else {
        gi = 0;
    }
    SetAndSaveSeq_idGi(result, seq_id, ids, gi);
    return true;
}


void CId1Reader::GetSeq_idSeq_ids(CReaderRequestResult& result,
                                  CLoadLockSeq_ids& ids,
                                  const CSeq_id_Handle& seq_id)
{
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        GetGiSeq_ids(result, seq_id, ids);
        return;
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
                    // only one source Seq-id and no synonyms
                    ids.AddSeq_id(id);
                    return;
                }
            }
        }
    }
    
    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    int gi = ids->GetGi();
    if ( !gi ) {
        // no gi -> no Seq-ids
        return;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);

    // copy Seq-id list from gi to original seq-id
    CLoadLockSeq_ids gi_ids(result, gi_handle);
    ids->m_Seq_ids = gi_ids->m_Seq_ids;
    ids->SetState(gi_ids->GetState());
}


void CId1Reader::GetSeq_idBlob_ids(CReaderRequestResult& result,
                                   CLoadLockBlob_ids& ids,
                                   const CSeq_id_Handle& seq_id)
{
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        GetGiBlob_ids(result, seq_id, ids);
        return;
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
                    CBlob_id blob_id;
                    blob_id.SetSat(iter->second.first);
                    blob_id.SetSatKey(num);
                    blob_id.SetSubSat(iter->second.second);
                    ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                    SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
                    return;
                }
            }
        }
    }

    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    CLoadLockSeq_ids seq_ids(result, seq_id);
    int gi = seq_ids->GetGi();
    if ( !gi ) {
        // no gi -> no blobs
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    m_Dispatcher->LoadSeq_idBlob_ids(result, gi_handle);

    // copy Seq-id list from gi to original seq-id
    CLoadLockBlob_ids gi_ids(result, gi_handle);
    ids->m_Blob_ids = gi_ids->m_Blob_ids;
    ids->SetState(gi_ids->GetState());
    SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
}


void CId1Reader::GetGiSeq_ids(CReaderRequestResult& /*result*/,
                              const CSeq_id_Handle& seq_id,
                              CLoadLockSeq_ids& ids)
{
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    int gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == 0 ) {
        return;
    }

    CID1server_request id1_request;
    {{
        id1_request.SetGetseqidsfromgi(gi);
    }}
    
    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request);

    if ( !id1_reply.IsIds() ) {
        return;
    }

    const CID1server_back::TIds& seq_ids = id1_reply.GetIds();
    ITERATE(CID1server_back::TIds, it, seq_ids) {
        ids.AddSeq_id(**it);
    }
}


const int kSat_BlobError = -1;

void CId1Reader::GetGiBlob_ids(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               CLoadLockBlob_ids& ids)
{
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    int gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == 0 ) {
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }

    CID1server_request id1_request;
    {{
        CID1server_maxcomplex& blob = id1_request.SetGetblobinfo();
        blob.SetMaxplex(eEntry_complexities_entry);
        blob.SetGi(gi);
    }}
    
    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request);

    if ( !id1_reply.IsGotblobinfo() ) {
        CBlob_id blob_id;
        blob_id.SetSat(kSat_BlobError);
        blob_id.SetSatKey(gi);
        ids.AddBlob_id(blob_id, 0);
        CTSE_Info::TBlobState state = CBioseq_Handle::fState_other_error|
                                      CBioseq_Handle::fState_no_data;
        if ( id1_reply.IsError() ) {
            switch ( id1_reply.GetError() ) {
            case 1:
                state = CBioseq_Handle::fState_withdrawn|
                        CBioseq_Handle::fState_no_data;
                break;
            case 2:
                state = CBioseq_Handle::fState_confidential|
                        CBioseq_Handle::fState_no_data;
                break;
            case 10:
                state = CBioseq_Handle::fState_no_data;
                break;
            }
        }
        ids->SetState(state);
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }

    const CID1blob_info& info = id1_reply.GetGotblobinfo();
    if (info.GetWithdrawn() > 0) {
        CBlob_id blob_id;
        blob_id.SetSat(kSat_BlobError);
        blob_id.SetSatKey(gi);
        ids.AddBlob_id(blob_id, 0);
        ids->SetState(CBioseq_Handle::fState_withdrawn|
                      CBioseq_Handle::fState_no_data);
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }
    if (info.GetConfidential() > 0) {
        CBlob_id blob_id;
        blob_id.SetSat(kSat_BlobError);
        blob_id.SetSatKey(gi);
        ids.AddBlob_id(blob_id, 0);
        ids->SetState(CBioseq_Handle::fState_confidential|
                      CBioseq_Handle::fState_no_data);
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }
    if ( info.GetSat() < 0 || info.GetSat_key() < 0 ) {
        LOG_POST(Warning<<"CId1Reader: gi "<<gi<<" negative sat/satkey");
        CBlob_id blob_id;
        blob_id.SetSat(kSat_BlobError);
        blob_id.SetSatKey(gi);
        ids.AddBlob_id(blob_id, 0);
        ids->SetState(CBioseq_Handle::fState_other_error|
                      CBioseq_Handle::fState_no_data);
        SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
        return;
    }
    if ( CProcessor::TrySNPSplit() ) {
        {{
            // add main blob
            CBlob_id blob_id;
            blob_id.SetSat(info.GetSat());
            blob_id.SetSatKey(info.GetSat_key());
            ids.AddBlob_id(blob_id, fBlobHasAllLocal);
        }}
        if ( info.IsSetExtfeatmask() ) {
            int ext_feat = info.GetExtfeatmask();
            while ( ext_feat ) {
                int bit = ext_feat & ~(ext_feat-1);
                ext_feat -= bit;
#ifdef GENBANK_USE_SNP_SATELLITE_15
                if ( bit == eSubSat_SNP ) {
                    AddSNPBlob_id(ids, gi);
                    continue;
                }
#endif
                CBlob_id blob_id;
                blob_id.SetSat(eSat_ANNOT);
                blob_id.SetSatKey(gi);
                blob_id.SetSubSat(bit);
                ids.AddBlob_id(blob_id, fBlobHasExtAnnot);
            }
        }
    }
    else {
        // whole blob
        CBlob_id blob_id;
        blob_id.SetSat(info.GetSat());
        blob_id.SetSatKey(info.GetSat_key());
        if ( info.IsSetExtfeatmask() ) {
            blob_id.SetSubSat(info.GetExtfeatmask());
        }
        ids.AddBlob_id(blob_id, fBlobHasAllLocal);
    }
    SetAndSaveSeq_idBlob_ids(result, seq_id, ids);
}


void CId1Reader::GetBlobVersion(CReaderRequestResult& result,
                                const CBlob_id& blob_id)
{
    CID1server_request id1_request;
    x_SetParams(id1_request.SetGetblobinfo(), blob_id);
    
    CID1server_back    reply;
    x_ResolveId(reply, id1_request);

    TBlobVersion version = -1;
    TBlobState blob_state = 0;
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        version = abs(reply.GetGotblobinfo().GetBlob_state());
        break;
    case CID1server_back::e_Gotsewithinfo:
        version = abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
        break;
    case CID1server_back::e_Error:
    {{
        version = 0;
        int error = reply.GetError();
        switch ( error ) {
        case 1:
            blob_state |=
                CBioseq_Handle::fState_withdrawn|
                CBioseq_Handle::fState_no_data;
            break;
        case 2:
            blob_state |=
                CBioseq_Handle::fState_confidential|
                CBioseq_Handle::fState_no_data;
            break;
        case 10:
            blob_state |= CBioseq_Handle::fState_no_data;
            break;
        default:
            ERR_POST("CId1Reader::GetBlobVersion: "
                     "ID1server-back.error "<<error);
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "ID1server-back.error "+NStr::IntToString(error));
        }
        break;
    }}
    default:
        ERR_POST("CId1Reader::GetBlobVersion: "
                 "invalid ID1server-back.");
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "invalid ID1server-back");
    }

    CLoadLockBlob blob(result, blob_id);
    if ( version >= 0 ) {
        SetAndSaveBlobVersion(result, blob_id, blob, version);
    }
    if ( blob_state ) {
        blob.SetBlobState(blob_state);
        SetAndSaveNoBlob(result, blob_id, CProcessor::kMain_ChunkId, blob);
    }
}


void CId1Reader::x_ResolveId(CID1server_back& reply,
                             const CID1server_request& request)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    {{
        CConn conn(this);
        x_SendRequest(conn, request);
        x_ReceiveReply(conn, reply);
        conn.Release();
    }}

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        if ( request.Which() == CID1server_request::e_Getgi ) {
            LogIdStat("CId1Reader: resolved",
                      "id", request.GetGetgi().AsFastaString(),
                      resolve_id, sw);
        }
        else if ( request.Which() == CID1server_request::e_Getblobinfo ) {
            const CID1server_maxcomplex& req = request.GetGetblobinfo();
            if ( req.IsSetSat() ) {
                LogIdStat("CId1Reader: resolved",
                          "blob version", ToString(req),
                          resolve_ver, sw);
            }
            else {
                LogIdStat("CId1Reader: resolved",
                          "gi", NStr::IntToString(req.GetGi()),
                          resolve_gi, sw);
            }
        }
        else if ( request.Which() == CID1server_request::e_Getseqidsfromgi ) {
            CID1server_request::TGetseqidsfromgi req = request.GetGetseqidsfromgi();
            LogIdStat("CId1Reader: get ids for",
                      "gi", NStr::IntToString(req),
                      resolve_gi, sw);
        }
        else {
            LogIdStat("CId1Reader: resolved", "id", "?", resolve_id, sw);
        }
    }
#endif
}


void CId1Reader::GetBlob(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         TChunkId chunk_id)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    CConn conn(this);
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
    m_Dispatcher->GetProcessor(processor_type)
        .ProcessStream(result, blob_id, chunk_id, *x_GetConnection(conn));
    conn.Release();

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogStat("CId1Reader: read blob", blob_id, main_read, sw, 0);
    }
#endif
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
    params.SetGi(0);
    params.SetEnt(blob_id.GetSatKey());
    if ( blob_id.GetSat() == eSat_ANNOT ) {
        params.SetMaxplex(eEntry_complexities_entry); // TODO: TEMP: remove
        params.SetSat("ANNOT:"+NStr::IntToString(blob_id.GetSubSat()));
    }
    else {
        params.SetSat(NStr::IntToString(blob_id.GetSat()));
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
    CConn_ServiceStream* stream = x_GetConnection(conn);

#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(*stream);
#endif
    if ( GetDebugLevel() >= eTraceConn ) {
        NcbiCout << "CId1Reader("<<conn<<"): Sending";
        if ( GetDebugLevel() >= eTraceASN ) {
            NcbiCout << ": " << MSerial_AsnText << request;
        }
        else {
            NcbiCout << " ID1server-request";
        }
        NcbiCout << "..." << NcbiEndl;
    }
    CObjectOStreamAsnBinary out(*stream);
    out << request;
    out.Flush();
    if ( GetDebugLevel() >= eTraceConn ) {
        NcbiCout << "CId1Reader("<<conn<<"): "
            "Sent ID1server-request." << NcbiEndl;
    }
}


void CId1Reader::x_ReceiveReply(TConn conn,
                                CID1server_back& reply)
{
    CConn_ServiceStream* stream = x_GetConnection(conn);

#ifdef GENBANK_ID1_RANDOM_FAILS
    SetRandomFail(*stream);
#endif
    if ( GetDebugLevel() >= eTraceConn ) {
        NcbiCout << "CId1Reader("<<conn<<"): "
            "Receiving ID1server-back..." << NcbiEndl;
    }
    {{
        CObjectIStreamAsnBinary in(*stream);
        in >> reply;
    }}
    if ( GetDebugLevel() >= eTraceConn   ) {
        NcbiCout << "CId1Reader("<<conn<<"): Received";
        if ( GetDebugLevel() >= eTraceASN ) {
            NcbiCout << ": " << MSerial_AsnText << reply;
        }
        else {
            NcbiCout << " ID1server-back.";
        }
        NcbiCout << NcbiEndl;
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
            objects::CReader::TConn noConn = GetParamDataSize(
                params,
                NCBI_GBLOADER_READER_ID1_PARAM_NUM_CONN,
                false,
                3);
            drv = new objects::CId1Reader(noConn);
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
