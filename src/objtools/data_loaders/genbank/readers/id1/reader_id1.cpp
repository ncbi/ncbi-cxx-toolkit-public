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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>

#include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/seqref.hpp>

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

#include <memory>
#include <iomanip>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#ifdef ID1_COLLECT_STATS
static STimeStatistics resolve_id;
static STimeStatistics resolve_gi;
static STimeStatistics resolve_ver;

static STimeSizeStatistics main_read;
static STimeSizeStatistics snp_read;
static STimeSizeStatistics snp_parse;

static int s_GetCollectStatistics(void)
{
    const char* env = getenv("GENBANK_ID1_STATS");
    if ( !env || !*env ) {
        return 0;
    }
    try {
        return NStr::StringToInt(env);
    }
    catch ( ... ) {
        return 0;
    }
}
#endif

int CId1Reader::CollectStatistics(void)
{
#ifdef ID1_COLLECT_STATS
    static int ret = s_GetCollectStatistics();
    return ret;
#else
    return 0;
#endif
}


static int s_GetDebugLevel(void)
{
    const char* env = getenv("GENBANK_ID1_DEBUG");
    if ( !env || !*env ) {
        return 0;
    }
    try {
        return NStr::StringToInt(env);
    }
    catch ( ... ) {
        return 0;
    }
}


static int GetDebugLevel(void)
{
    static int ret = s_GetDebugLevel();
    return ret;
}


enum EDebugLevel
{
    eTraceConn = 4,
    eTraceASN = 5
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
    conn = conn % m_Pool.size();
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
    conn = conn % m_Pool.size();
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

            string id1_svc;
            {{
                CNcbiApplication* app = CNcbiApplication::Instance();
                static const char* env_var = "NCBI_SERVICE_NAME_ID1";
                if (app) { 
                    id1_svc = app->GetEnvironment().Get(env_var);
                } else {
                    char* s = ::getenv(env_var);
                    if (s) {
                        id1_svc = s;
                    }
                }
            }}
            if ( id1_svc.empty() ) {
                id1_svc = "ID1";
            }

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


typedef pair<CSeqref::ESat, CSeqref::ESubSat> TSK;
typedef pair<const char*, TSK> TSI;
static const TSI sc_SatIndex[] = {
    TSI("ANNOT:CDD",  TSK(CSeqref::eSat_ANNOT,      CSeqref::eSubSat_CDD)),
    TSI("ANNOT:MGS",  TSK(CSeqref::eSat_ANNOT,      CSeqref::eSubSat_MGS)),
    TSI("ANNOT:SNP",  TSK(CSeqref::eSat_ANNOT,      CSeqref::eSubSat_SNP)),
    TSI("SNP",        TSK(CSeqref::eSat_SNP,        CSeqref::eSubSat_main)),
    TSI("ti",         TSK(CSeqref::eSat_TRACE,      CSeqref::eSubSat_main)),
    TSI("TR_ASSM_CH", TSK(CSeqref::eSat_TR_ASSM_CH, CSeqref::eSubSat_main)),
    TSI("TRACE_ASSM", TSK(CSeqref::eSat_TRACE_ASSM, CSeqref::eSubSat_main)),
    TSI("TRACE_CHGR", TSK(CSeqref::eSat_TRACE_CHGR, CSeqref::eSubSat_main))
};
typedef CStaticArrayMap<const char*, TSK, PNocase> TSatMap;
static const TSatMap sc_SatMap(sc_SatIndex, sizeof(sc_SatIndex));


void CId1Reader::ResolveSeq_id(CLoadLockBlob_ids& ids,
                               const CSeq_id& id,
                               TConn conn)
{
    if ( id.IsGeneral()  &&  id.GetGeneral().GetTag().IsId() ) {
        const CDbtag& dbtag = id.GetGeneral();
        const string& db = dbtag.GetDb();
        int id = dbtag.GetTag().GetId();
        if ( id != 0 ) {
            TSatMap::const_iterator iter = sc_SatMap.find(db.c_str());
            if ( iter != sc_SatMap.end() ) {
                CBlob_id blob_id;
                blob_id.SetSat(iter->second.first);
                blob_id.SetSatKey(id);
                blob_id.SetSubSat(iter->second.second);
                ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                return;
            }
            else {
                try {
                    CBlob_id blob_id;
                    blob_id.SetSat(NStr::StringToInt(db));
                    blob_id.SetSatKey(id);
                    ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                    return;
                }
                catch (...) {
                }
            }
        }
    }
    int gi;
    if ( id.IsGi() ) {
        gi = id.GetGi();
    }
    else {
        gi = ResolveSeq_id_to_gi(id, conn);
    }
    ResolveGi(ids, gi, conn);
}


int CId1Reader::ResolveSeq_id_to_gi(const CSeq_id& seqId, TConn conn)
{
    CID1server_request id1_request;
    id1_request.SetGetgi(const_cast<CSeq_id&>(seqId));

    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);

    return id1_reply.IsGotgi()? id1_reply.GetGotgi(): 0;
}


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
        return;
    }

    const CID1blob_info& info = id1_reply.GetGotblobinfo();
    if ( info.GetWithdrawn() > 0 || info.GetConfidential() > 0 ) {
        //LOG_POST(Warning<<"CId1Reader: gi "<<gi<<" is private");
        return;
    }
    if ( info.GetSat() < 0 || info.GetSat_key() < 0 ) {
        LOG_POST(Warning<<"CId1Reader: gi "<<gi<<" negative sat/satkey");
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
                if ( bit == CSeqref::eSubSat_SNP ) {
                    AddSNPBlob_id(ids, gi);
                    continue;
                }
#endif
                CBlob_id blob_id;
                blob_id.SetSat(CSeqref::eSat_ANNOT);
                blob_id.SetSatKey(gi);
                blob_id.SetSubSat(bit);
                ids.AddBlob_id(blob_id, fBlobHasExternal);
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
    ids.SetLoaded();
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
    CStopWatch sw(CollectStatistics());
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
    GetSeq_entry(id1_reply, blob_id, conn);
    SetSeq_entry(tse_info, id1_reply);
}


CRef<CSeq_annot_SNP_Info> CId1Reader::GetSNPAnnot(const CBlob_id& blob_id,
                                                  TConn conn)
{
    CRef<CSeq_annot_SNP_Info> ret(new CSeq_annot_SNP_Info);

    x_GetSNPAnnot(*ret, blob_id, conn);

    return ret;
}


void CId1Reader::GetSeq_entry(CID1server_back& id1_reply,
                              const CBlob_id& blob_id,
                              TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics());
    try {
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
            CReader::SetSeqEntryReadHooks(obj_stream);
            x_ReadBlobReply(id1_reply, obj_stream, blob_id);
            size = obj_stream.GetStreamOffset();
        }}
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Reader: read blob", blob_id, main_read, sw, size);
        }
    }
    catch ( ... ) {
        if ( CollectStatistics() ) {
            LogStat("CId1Reader: fail blob", blob_id, main_read, sw, 0);
        }
        throw;
    }
#endif
}


void CId1Reader::SetSeq_entry(CTSE_Info& tse_info,
                              CID1server_back& id1_reply)
{
    CRef<CSeq_entry> seq_entry;
    CTSE_Info::ESuppression_Level suppression = CTSE_Info::eSuppression_none;
    switch ( id1_reply.Which() ) {
    case CID1server_back::e_Gotseqentry:
        seq_entry.Reset(&id1_reply.SetGotseqentry());
        break;
    case CID1server_back::e_Gotdeadseqentry:
        suppression = CTSE_Info::eSuppression_dead;
        seq_entry.Reset(&id1_reply.SetGotdeadseqentry());
        break;
    case CID1server_back::e_Gotsewithinfo:
    {{
        const CID1blob_info& info =
            id1_reply.GetGotsewithinfo().GetBlob_info();
        if ( info.GetBlob_state() < 0 ) {
            suppression = CTSE_Info::eSuppression_dead;
        }
        if ( id1_reply.GetGotsewithinfo().IsSetBlob() ) {
            seq_entry.Reset(&id1_reply.SetGotsewithinfo().SetBlob());
        }
        else {
            // no Seq-entry in reply, probably private data
            if ( info.GetWithdrawn() ) {
                NCBI_THROW(CLoaderException, ePrivateData, "id is withdrawn");
            }
            if ( info.GetConfidential() ) {
                NCBI_THROW(CLoaderException, ePrivateData, "id is withdrawn");
            }
            NCBI_THROW(CLoaderException, eNoData, "no Seq-entry");
        }
        break;
    }}
    case CID1server_back::e_Error:
    {{
        int error = id1_reply.GetError();
        switch ( error ) {
        case 1:
            NCBI_THROW(CLoaderException, ePrivateData, "id is withdrawn");
        case 2:
            NCBI_THROW(CLoaderException, ePrivateData, "id is private");
        case 10:
            NCBI_THROW(CLoaderException, eNoData, "invalid args");
        }
        ERR_POST("CId1Reader::GetMainBlob: ID1server-back.error "<<error);
        NCBI_THROW(CLoaderException, eLoaderFailed, "ID1server-back.error");
        break;
    }}
    default:
        // no data
        NCBI_THROW(CLoaderException, eLoaderFailed, "bad ID1server-back type");
    }
    tse_info.SetSeq_entry(*seq_entry);
    TBlobVersion version = x_GetVersion(id1_reply);
    if ( version >= 0 ) {
        tse_info.SetBlobVersion(version);
    }
    tse_info.SetSuppressionLevel(suppression);
}


CReader::TBlobVersion CId1Reader::x_GetVersion(const CID1server_back& reply)
{
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        return abs(reply.GetGotblobinfo().GetBlob_state());
        break;
    case CID1server_back::e_Gotsewithinfo:
        return abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
        break;
    default:
        return -1;
    }
}


void CId1Reader::x_SetBlobRequest(CID1server_request& request,
                                  const CBlob_id& blob_id)
{
    x_SetParams(request.SetGetsefromgi(), blob_id);
}


void CId1Reader::x_ReadBlobReply(CID1server_back& reply,
                                 CObjectIStream& stream,
                                 const CBlob_id& /*blob_id*/)
{
    stream >> reply;
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


void CId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                               const CBlob_id& blob_id,
                               TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics());
    try {
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
                
                CSeq_annot_SNP_Info_Reader::Parse(in, snp_info);
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
    }
    catch ( ... ) {
        if ( CollectStatistics() ) {
            LogStat("CId1Reader: fail SNP blob", blob_id, snp_read, sw, 0);
        }
        throw;
    }
#endif
}


void CId1Reader::x_SetParams(CID1server_maxcomplex& params,
                             const CBlob_id& blob_id)
{
    int bits = (~blob_id.GetSubSat() & 0xffff) << 4;
    params.SetMaxplex(eEntry_complexities_entry | bits);
    params.SetGi(0);
    params.SetEnt(blob_id.GetSatKey());
    if ( blob_id.GetSat() == CSeqref::eSat_ANNOT ) {
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
    int conn = -1;
    if ( GetDebugLevel() >= eTraceConn ) {
        ITERATE ( vector<CConn_ServiceStream*>, it, m_Pool ) {
            if ( *it == stream ) {
                conn = it - m_Pool.begin();
                break;
            }
        }
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

const string kId1ReaderDriverName("id1");


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
