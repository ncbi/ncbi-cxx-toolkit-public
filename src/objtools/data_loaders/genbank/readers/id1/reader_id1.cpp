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
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_cache.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimtx.hpp>

#include <objtools/data_loaders/genbank/readers/id1/statistics.hpp>

#include <corelib/plugin_manager_impl.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id1/id1__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/stream_utils.hpp>
#include <util/static_map.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <util/cache/icache.hpp>

#include <memory>
#include <algorithm>
#include <iomanip>

//#define GENBANK_ID1_RANDOM_FAILS 1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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
    static int var = GetConfigInt("GENBANK", "ID1_STATS");
    return var;
#else
    return 0;
#endif
}


static int GetDebugLevel(void)
{
    static int var = GetConfigInt("GENBANK", "ID1_DEBUG");
    return var;
}


enum EDebugLevel
{
    eTraceConn = 4,
    eTraceASN = 5,
    eTraceASNData = 8
};


DEFINE_STATIC_FAST_MUTEX(sx_FirstConnectionMutex);


CId1Reader::CId1Reader(TConn noConn)
    : m_NoMoreConnections(false)
{
#if !defined(NCBI_THREADS)
    noConn=1;
#else
    //noConn=1; // limit number of simultaneous connections to one
#endif
    try {
        SetParallelLevel(noConn);
        m_FirstConnection.reset(x_NewConnection());
    }
    catch ( ... ) {
        // close all connections before exiting
        SetParallelLevel(0);
        throw;
    }
}


CId1Reader::~CId1Reader()
{
    SetParallelLevel(0);
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


CReader::TConn CId1Reader::GetParallelLevel(void) const
{
    return m_Pool.size();
}


void CId1Reader::SetParallelLevel(TConn size)
{
    size_t oldSize = m_Pool.size();
    for (size_t i = size; i < oldSize; ++i) {
        delete m_Pool[i];
        m_Pool[i] = 0;
    }

    m_Pool.resize(size);

    if ( m_FirstConnection.get() ) { // check if we should close 'first' conn
        for ( size_t i = 0; i < size; ++i ) {
            if ( m_Pool[i] == 0 ) { // there is empty slot, so let it live
                return;
            }
        }
        m_FirstConnection.reset();
    }
}


CConn_ServiceStream* CId1Reader::x_GetConnection(TConn conn)
{
    _ASSERT(conn < m_Pool.size());
    CConn_ServiceStream* ret = m_Pool[conn];
    if ( !ret ) {
        ret = x_NewConnection();

        if ( !ret ) {
            NCBI_THROW(CLoaderException, eNoConnection,
                       "too many connections failed: probably server is dead");
        }

        m_Pool[conn] = ret;
    }
    return ret;
}


void CId1Reader::Reconnect(TConn conn)
{
    _TRACE("Reconnect(" << conn << ")");
    _ASSERT(conn < m_Pool.size());
    if ( m_Pool[conn] ) {
        if ( GetDebugLevel() >= eTraceConn ) {
            NcbiCout << "CId1Reader(" << conn << "): "
                "Closing connection..." << NcbiEndl;
        }
        delete m_Pool[conn];
        m_Pool[conn] = 0;
        if ( GetDebugLevel() >= eTraceConn ) {
            NcbiCout << "CId1Reader(" << conn << "): "
                "Connection closed." << NcbiEndl;
        }
    }
}


CConn_ServiceStream* CId1Reader::x_NewConnection(void)
{
    if ( m_FirstConnection.get() ) {
        CFastMutexGuard guard(sx_FirstConnectionMutex);
        if ( m_FirstConnection.get() ) {
            return m_FirstConnection.release();
        }
    }
    for ( int i = 0; !m_NoMoreConnections && i < 3; ++i ) {
        try {
            _TRACE("CId1Reader(" << this << ")->x_NewConnection()");

            static string id1_svc = GetConfigString("NCBI",
                                                    "SERVICE_NAME_ID1",
                                                    "ID1");

            if ( GetDebugLevel() >= eTraceConn ) {
                NcbiCout << "CId1Reader: New connection to " <<
                    id1_svc << "..." << NcbiEndl;
            }
            STimeout tmout;
            tmout.sec = 20;
            tmout.usec = 0;
            auto_ptr<CConn_ServiceStream> stream
                (new CConn_ServiceStream(id1_svc, fSERV_Any, 0, 0, &tmout));
            if ( !stream->bad() ) {
                if ( GetDebugLevel() >= eTraceConn ) {
                    NcbiCout << "CId1Reader: New connection to " <<
                        id1_svc << " opened." << NcbiEndl;
                }
                return stream.release();
            }
            ERR_POST("CId1Reader::x_NewConnection: cannot connect.");
        }
        catch ( CException& exc ) {
            ERR_POST("CId1Reader::x_NewConnection: cannot connect: " <<
                     exc.what());
        }
    }
    m_NoMoreConnections = true;
    return 0;
}


typedef CId1ReaderBase TRDR;
typedef pair<TRDR::ESat, TRDR::ESubSat> TSK;
typedef pair<const char*, TSK> TSI;
static const TSI sc_SatIndex[] = {
    TSI("ANNOT:CDD",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_CDD)),
    TSI("ANNOT:MGC",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_MGC)),
    TSI("ANNOT:SNP",  TSK(TRDR::eSat_ANNOT,      TRDR::eSubSat_SNP)),
    TSI("ANNOT:SNP GRAPH",TSK(TRDR::eSat_ANNOT,  TRDR::eSubSat_SNP_graph)),
    TSI("SNP",        TSK(TRDR::eSat_SNP,        TRDR::eSubSat_main)),
    TSI("ti",         TSK(TRDR::eSat_TRACE,      TRDR::eSubSat_main)),
    TSI("TR_ASSM_CH", TSK(TRDR::eSat_TR_ASSM_CH, TRDR::eSubSat_main)),
    TSI("TRACE_ASSM", TSK(TRDR::eSat_TRACE_ASSM, TRDR::eSubSat_main)),
    TSI("TRACE_CHGR", TSK(TRDR::eSat_TRACE_CHGR, TRDR::eSubSat_main))
};
typedef CStaticArrayMap<const char*, TSK, PNocase> TSatMap;
static const TSatMap sc_SatMap(sc_SatIndex, sizeof(sc_SatIndex));


void CId1Reader::ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockBlob_ids& ids,
                               const CSeq_id& id,
                               TConn conn)
{
    if ( id.IsGi() ) {
        ResolveGi(ids, id.GetGi(), conn);
        return;
    }

    if ( id.IsGeneral()  &&  id.GetGeneral().GetTag().IsId() ) {
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
                return;
            }
            /*
              numeric value in place of DB will not work anyway
            else {
                try {
                    CBlob_id blob_id;
                    blob_id.SetSat(NStr::StringToInt(db));
                    blob_id.SetSatKey(num);
                    ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                    return;
                }
                catch (...) {
                }
            }
            */
        }
    }

    CLoadLockSeq_ids seq_ids(result, id);
    if ( !seq_ids->IsLoadedGi() ) {
        seq_ids->SetLoadedGi(ResolveSeq_id_to_gi(id, conn));
    }
    int gi = seq_ids->GetGi();
    if ( !gi ) {
        return;
    }

    CLoadLockBlob_ids gi_ids(result, CSeq_id_Handle::GetGiHandle(gi));
    if ( !gi_ids.IsLoaded() ) {
        ResolveGi(gi_ids, gi, conn);
        gi_ids.SetLoaded();
    }

    // copy info from gi to original seq-id
    ITERATE ( CLoadInfoBlob_ids, it, *gi_ids ) {
        ids.AddBlob_id(it->first, it->second);
        ids->SetState(gi_ids->GetState());
    }
}


void CId1Reader::ResolveSeq_id(CReaderRequestResult& result,
                               CLoadLockSeq_ids& ids,
                               const CSeq_id& id,
                               TConn conn)
{
    if ( id.IsGi() ) {
        ResolveGi(ids, id.GetGi(), conn);
        return;
    }

    if ( id.IsGeneral()  &&  id.GetGeneral().GetTag().IsId() ) {
        const CDbtag& dbtag = id.GetGeneral();
        const string& db = dbtag.GetDb();
        int num = dbtag.GetTag().GetId();
        if ( num != 0 ) {
            TSatMap::const_iterator iter = sc_SatMap.find(db.c_str());
            if ( iter != sc_SatMap.end() ) {
                ids.AddSeq_id(id);
                return;
            }
        }
    }

    CLoadLockSeq_ids seq_ids(result, id);
    if ( !ids->IsLoadedGi() ) {
        ids->SetLoadedGi(ResolveSeq_id_to_gi(id, conn));
    }
    int gi = seq_ids->GetGi();

    CLoadLockSeq_ids gi_ids(result, CSeq_id_Handle::GetGiHandle(gi));
    if ( !gi_ids.IsLoaded() ) {
        ResolveGi(gi_ids, gi, conn);
        gi_ids.SetLoaded();
    }

    // copy info from gi to original seq-id
    ITERATE ( CLoadInfoSeq_ids, it, *gi_ids ) {
        ids.AddSeq_id(*it);
        ids->SetState(gi_ids->GetState());
    }
}


int CId1Reader::ResolveSeq_id_to_gi(const CSeq_id& seqId, TConn conn)
{
    CID1server_request id1_request;
    id1_request.SetGetgi(const_cast<CSeq_id&>(seqId));

    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);

    return id1_reply.IsGotgi()? id1_reply.GetGotgi(): 0;
}


const int kSat_BlobError = -1;

void CId1Reader::ResolveGi(CLoadLockBlob_ids& ids, int gi, TConn conn)
{
    if ( gi == 0 ) {
        return;
    }
    CID1server_request id1_request;
    {{
        CID1server_maxcomplex& blob = id1_request.SetGetblobinfo();
        blob.SetMaxplex(eEntry_complexities_entry);
        blob.SetGi(gi);
    }}
    
    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);

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
        return;
    }
    if (info.GetConfidential() > 0) {
        CBlob_id blob_id;
        blob_id.SetSat(kSat_BlobError);
        blob_id.SetSatKey(gi);
        ids.AddBlob_id(blob_id, 0);
        ids->SetState(CBioseq_Handle::fState_confidential|
                      CBioseq_Handle::fState_no_data);
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
        return;
    }
    if ( TrySNPSplit() ) {
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
}


void CId1Reader::ResolveGi(CLoadLockSeq_ids& ids, int gi, TConn conn)
{
    if ( gi == 0 ) {
        return;
    }
    CID1server_request id1_request;
    {{
        id1_request.SetGetseqidsfromgi(gi);
    }}
    
    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);

    if ( !id1_reply.IsIds() ) {
        return;
    }

    const CID1server_back::TIds& seq_ids = id1_reply.GetIds();
    ITERATE(CID1server_back::TIds, it, seq_ids) {
        ids.AddSeq_id(**it);
    }
}


CReader::TBlobVersion CId1Reader::GetVersion(const CBlob_id& blob_id,
                                             TConn conn)
{
    CID1server_request id1_request;
    x_SetParams(id1_request.SetGetblobinfo(), blob_id);
    
    CID1server_back    id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);
    
    if ( id1_reply.IsGotblobinfo() ) {
        return abs(id1_reply.GetGotblobinfo().GetBlob_state());
    }
    return 0;
}


void CId1Reader::x_ResolveId(CID1server_back& reply,
                             const CID1server_request& request,
                             TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    CConn_ServiceStream* stream = x_GetConnection(conn);
    x_SendRequest(stream, request);
    x_ReceiveReply(stream, reply);

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


void CId1Reader::GetTSEBlob(CTSE_Info& tse_info,
                            const CBlob_id& blob_id,
                            TConn conn)
{
    CID1server_back id1_reply;
    TSNP_InfoMap snps;
    GetSeq_entry(id1_reply, snps, blob_id, conn);
    SetSeq_entry(tse_info, id1_reply, snps);
}


void CId1Reader::GetSeq_entry(CID1server_back& id1_reply,
                              TSNP_InfoMap& snps,
                              const CBlob_id& blob_id,
                              TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif
    CConn_ServiceStream* stream = x_GetConnection(conn);
    {{
        CID1server_request request;
        x_SetBlobRequest(request, blob_id);
        x_SendRequest(stream, request);
    }}
    size_t size;
    {{
        CObjectIStreamAsnBinary obj_stream(*stream);
#ifdef GENBANK_USE_SNP_SATELLITE_15
        x_ReadBlobReply(id1_reply, obj_stream, blob_id);
#else
        if ( blob_id.GetSubSat() == eSubSat_SNP ) {
            x_ReadBlobReply(id1_reply, snps, obj_stream, blob_id);
        }
        else {
            x_ReadBlobReply(id1_reply, obj_stream, blob_id);
            if ( GetDebugLevel() >= eTraceConn   ) {
                NcbiCout << "CId1Reader("<<conn<<"): Received";
                if ( GetDebugLevel() >= eTraceASNData ||
                     GetDebugLevel() >= eTraceASN &&
                     !(id1_reply.IsGotseqentry() ||
                       id1_reply.IsGotdeadseqentry() ||
                       id1_reply.IsGotsewithinfo()) ) {
                    NcbiCout << ": " << MSerial_AsnText << id1_reply;
                }
                else {
                    NcbiCout << " ID1server-back.";
                }
                NcbiCout << NcbiEndl;
            }
        }
#endif
        size = obj_stream.GetStreamOffset();
    }}
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogStat("CId1Reader: read blob", blob_id, main_read, sw, size);
    }
#endif
}


void CId1Reader::SetSeq_entry(CTSE_Info& tse_info,
                              CID1server_back& id1_reply,
                              const TSNP_InfoMap& snps)
{
    CRef<CSeq_entry> seq_entry;
    switch ( id1_reply.Which() ) {
    case CID1server_back::e_Gotseqentry:
        seq_entry.Reset(&id1_reply.SetGotseqentry());
        break;
    case CID1server_back::e_Gotdeadseqentry:
        tse_info.SetBlobState(CBioseq_Handle::fState_dead);
        seq_entry.Reset(&id1_reply.SetGotdeadseqentry());
        break;
    case CID1server_back::e_Gotsewithinfo:
    {{
        const CID1blob_info& info =
            id1_reply.GetGotsewithinfo().GetBlob_info();
        if ( info.GetBlob_state() < 0 ) {
            tse_info.SetBlobState(CBioseq_Handle::fState_dead);
        }
        if ( id1_reply.GetGotsewithinfo().IsSetBlob() ) {
            seq_entry.Reset(&id1_reply.SetGotsewithinfo().SetBlob());
        }
        else {
            // no Seq-entry in reply, probably private data
            tse_info.SetBlobState(CBioseq_Handle::fState_no_data);
        }
        if ( info.GetSuppress() ) {
            tse_info.SetBlobState(
                (info.GetSuppress() & 4)
                ? CBioseq_Handle::fState_suppress_temp
                : CBioseq_Handle::fState_suppress_perm);
        }
        if ( info.GetWithdrawn() ) {
            tse_info.SetBlobState(CBioseq_Handle::fState_withdrawn|
                                  CBioseq_Handle::fState_no_data);
        }
        if ( info.GetConfidential() ) {
            tse_info.SetBlobState(CBioseq_Handle::fState_confidential|
                                  CBioseq_Handle::fState_no_data);
        }
        break;
    }}
    case CID1server_back::e_Error:
    {{
        int error = id1_reply.GetError();
        switch ( error ) {
        case 1:
            tse_info.SetBlobState(CBioseq_Handle::fState_withdrawn|
                                  CBioseq_Handle::fState_no_data);
            break;
        case 2:
            tse_info.SetBlobState(CBioseq_Handle::fState_confidential|
                                  CBioseq_Handle::fState_no_data);
            break;
        case 10:
            tse_info.SetBlobState(CBioseq_Handle::fState_no_data);
            break;
        default:
            ERR_POST("CId1Reader::GetMainBlob: ID1server-back.error "<<error);
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "ID1server-back.error "+NStr::IntToString(error));
        }
        break;
    }}
    default:
        // no data
        NCBI_THROW(CLoaderException, eLoaderFailed, "bad ID1server-back type");
    }
    if ( seq_entry ) {
        tse_info.SetSeq_entry(*seq_entry, snps);
        TBlobVersion version = x_GetVersion(id1_reply);
        if ( version >= 0 ) {
            tse_info.SetBlobVersion(version);
        }
    }
}


CReader::TBlobVersion CId1Reader::x_GetVersion(const CID1server_back& reply)
{
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        return abs(reply.GetGotblobinfo().GetBlob_state());
    case CID1server_back::e_Gotsewithinfo:
        return abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
    default:
        return -1;
    }
}


void CId1Reader::x_SetBlobRequest(CID1server_request& request,
                                  const CBlob_id& blob_id)
{
    x_SetParams(request.SetGetsewithinfo(), blob_id);
    //x_SetParams(request.SetGetsefromgi(), blob_id);
}


void CId1Reader::x_ReadBlobReply(CID1server_back& reply,
                                 CObjectIStream& stream,
                                 const CBlob_id& /*blob_id*/)
{
    CReader::SetSeqEntryReadHooks(stream);
    stream >> reply;
}


void CId1Reader::x_ReadBlobReply(CID1server_back& reply,
                                 TSNP_InfoMap& snps,
                                 CObjectIStream& stream,
                                 const CBlob_id& /*blob_id*/)
{
    CSeq_annot_SNP_Info_Reader::Parse(stream, Begin(reply), snps);
}


static void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip)
{
    // skip 2 bytes of hacked header
    const size_t kBufferSize = 128;
    char buffer[kBufferSize];
    while ( to_skip ) {
        size_t cnt = reader.Read(buffer, min(to_skip, sizeof(buffer)));
        if ( cnt == 0 ) {
            NCBI_THROW(CEofException, eEof,
                       "unexpected EOF while skipping ID1 SNP wrapper bytes");
        }
        to_skip -= cnt;
    }
}


CRef<CSeq_annot_SNP_Info> CId1Reader::GetSNPAnnot(const CBlob_id& blob_id,
                                                  TConn conn)
{
    CRef<CSeq_annot_SNP_Info> snp_annot_info;
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif
    CConn_ServiceStream* stream = x_GetConnection(conn);
    x_SendRequest(blob_id, stream);

#ifdef ID1_COLLECT_STATS
    size_t size;
#endif
    {{
        const size_t kSkipHeader = 2, kSkipFooter = 2;
        
        CStreamByteSourceReader src(0, stream);
        
        Id1ReaderSkipBytes(src, kSkipHeader);
        
        CNlmZipBtRdr src2(&src);
        
        {{
            CObjectIStreamAsnBinary in(src2);
                
            snp_annot_info = CSeq_annot_SNP_Info_Reader::ParseAnnot(in);
        }}

#ifdef ID1_COLLECT_STATS
        size = src2.GetCompressedSize();
#endif
            
        Id1ReaderSkipBytes(src, kSkipFooter);
    }}

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogStat("CId1Reader: read SNP blob", blob_id, snp_read, sw, size);
    }
#endif
    return snp_annot_info;
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


void CId1Reader::x_SendRequest(const CBlob_id& blob_id,
                               CConn_ServiceStream* stream)
{
    CID1server_request id1_request;
    x_SetParams(id1_request.SetGetsefromgi(), blob_id);
    x_SendRequest(stream, id1_request);
}


void CId1Reader::x_SendRequest(CConn_ServiceStream* stream,
                               const CID1server_request& request)
{
#if GENBANK_ID1_RANDOM_FAILS
    if ( random() % 64 == 0 ) {
        stream->setstate(ios::badbit);
    }
#endif
    TConn conn = 0;
    if ( GetDebugLevel() >= eTraceConn ) {
        conn = find(m_Pool.begin(), m_Pool.end(), stream) - m_Pool.begin();
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


void CId1Reader::x_ReceiveReply(CConn_ServiceStream* stream,
                                CID1server_back& reply)
{
#if GENBANK_ID1_RANDOM_FAILS
    if ( random() % 64 == 0 ) {
        stream->setstate(ios::badbit);
    }
#endif
    int conn = -1;
    if ( GetDebugLevel() >= eTraceConn ) {
        ITERATE ( vector<CConn_ServiceStream*>, it, m_Pool ) {
            if ( *it == stream ) {
                conn = it - m_Pool.begin();
                break;
            }
        }
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


const string kId1ReaderDriverName("id1");
const string kId1Reader_NoConn("no_conn");
const string kId1Reader_BlobCacheSection("blob_cache");
const string kId1Reader_IdCacheSection("id_cache");
const string kId1Reader_DriverKey("driver");

/// Class factory for ID1 reader
///
/// @internal
///
class CId1ReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader, objects::CId1Reader>
{
public:
    typedef 
      CSimpleClassFactoryImpl<objects::CReader, objects::CId1Reader> TParent;
public:
    CId1ReaderCF() : TParent(kId1ReaderDriverName, 0)
        {
        }
    ~CId1ReaderCF()
        {
        }

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
                kId1Reader_NoConn,
                false,
                3);
            typedef CPluginManager<ICache> TCacheManager;
            typedef CPluginManagerStore::CPMMaker<ICache> TCacheManagerStore;
            CRef<TCacheManager> CacheManager(TCacheManagerStore::Get());
            _ASSERT(CacheManager);
            const TPluginManagerParamTree* blob_params = params ?
                params->FindNode(kId1Reader_BlobCacheSection) : 0;
            const TPluginManagerParamTree* id_params = params ?
                params->FindNode(kId1Reader_IdCacheSection) : 0;
            ICache* blob_cache = CacheManager->CreateInstanceFromKey(
                blob_params,
                kId1Reader_DriverKey);
            ICache* id_cache = CacheManager->CreateInstanceFromKey(
                id_params,
                kId1Reader_DriverKey);
            if ( blob_cache  ||  id_cache ) {
                drv = new objects::CCachedId1Reader(noConn,
                                                    blob_cache,
                                                    id_cache);
            }
            else {
                drv = new objects::CId1Reader(noConn);
            }
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
