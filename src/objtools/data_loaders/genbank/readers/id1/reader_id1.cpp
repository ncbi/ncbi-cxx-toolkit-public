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

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>

#include <corelib/ncbistre.hpp>

#define ID1_COLLECT_STATS
#ifdef ID1_COLLECT_STATS
# include <corelib/ncbitime.hpp>
#endif
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
static int resolve_id_count = 0;
static double resolve_id_time = 0;

static int resolve_gi_count = 0;
static double resolve_gi_time = 0;

static int resolve_ver_count = 0;
static double resolve_ver_time = 0;

static double last_object_bytes = 0;

static int main_blob_count = 0;
static double main_bytes = 0;
static double main_time = 0;

static int snp_blob_count = 0;
static double snp_compressed = 0;
static double snp_uncompressed = 0;
static double snp_time = 0;
static double snp_total_read_time = 0;
static double snp_decompression_time = 0;

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


CId1Reader::CId1Reader(TConn noConn)
    : m_NoMoreConnections(false)
{
    
    noConn=1; // limit number of simultaneous connections to one
#if !defined(NCBI_THREADS)
    noConn=1;
#endif
    try {
        SetParallelLevel(noConn);
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
                           size_t count,
                           const char* what,
                           double time)
{
#ifdef ID1_COLLECT_STATS
    if ( !count ) {
        return;
    }
    LOG_POST(type <<' '<<count<<' '<<what<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time)<<" s "<<
             (time*1000/count)<<" ms/one");
#endif
}


void CId1Reader::PrintBlobStat(const char* type,
                               size_t count,
                               double bytes,
                               double time)
{
#ifdef ID1_COLLECT_STATS
    if ( !count ) {
        return;
    }
    LOG_POST(type<<' '<<count<<" blobs "<<
             setiosflags(ios::fixed)<<
             setprecision(2)<<
             (bytes/1024)<<" kB in "<<
             setprecision(3)<<
             (time)<<" s "<<
             setprecision(2)<<
             (bytes/time/1024)<<" kB/s");
#endif
}


void CId1Reader::LogStat(const char* type, const string& name, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<name<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogStat(const char* type,
                         const string& name, const string& subkey, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<name<<" ("<<subkey<<") in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogStat(const char* type, const CSeq_id& id, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<id.AsFastaString()<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogStat(const char* type,
                         const CID1server_maxcomplex& maxplex, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<" TSE("<<maxplex.GetSat()<<','<<maxplex.GetEnt()<<") in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogStat(const char* type, int gi, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<gi<<" in "<<
             setiosflags(ios::fixed)<<
             setprecision(3)<<
             (time*1000)<<" ms");
#endif
}


void CId1Reader::LogBlobStat(const char* type,
                             const CSeqref& seqref, double bytes, double time)
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() <= 1 ) {
        return;
    }
    LOG_POST(type<<' '<<seqref.printTSE()<<' '<<
             setiosflags(ios::fixed)<<
             setprecision(2)<<
             (bytes/1024)<<" kB in "<<
             setprecision(3)<<
             (time*1000)<<" ms "<<
             setprecision(2)<<
             (bytes/1024/time)<<" kB/s");
#endif
}


void CId1Reader::PrintStatistics(void) const
{
#ifdef ID1_COLLECT_STATS
    PrintStat("ID1 resolution: resolved",
              resolve_id_count, "ids", resolve_id_time);
    PrintStat("ID1 resolution: resolved",
              resolve_gi_count, "gis", resolve_gi_time);
    PrintStat("ID1 resolution: resolved",
              resolve_ver_count, "vers", resolve_ver_time);
    PrintBlobStat("ID1 non-SNP: loaded",
                  main_blob_count, main_bytes, main_time);
    PrintBlobStat("ID1 SNP: loaded",
                  snp_blob_count, snp_compressed, snp_time);
    if ( snp_blob_count ) {
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
    for (size_t i = oldSize; i < min(1u, size); ++i) {
        m_Pool[i] = x_NewConnection();
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
    delete m_Pool[conn];
    m_Pool[conn] = 0;
}


CConn_ServiceStream* CId1Reader::x_NewConnection(void)
{
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

            STimeout tmout;
            tmout.sec = 20;
            tmout.usec = 0;
            auto_ptr<CConn_ServiceStream> stream
                (new CConn_ServiceStream(id1_svc, fSERV_Any, 0, 0, &tmout));
            if ( !stream->bad() ) {
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


int CId1Reader::ResolveSeq_id_to_gi(const CSeq_id& seqId, TConn conn)
{
    CID1server_request id1_request;
    id1_request.SetGetgi(const_cast<CSeq_id&>(seqId));

    CID1server_back id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);

    return id1_reply.IsGotgi()? id1_reply.GetGotgi(): 0;
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


void CId1Reader::ResolveSeq_id(TSeqrefs& srs, const CSeq_id& id, TConn conn)
{
    if ( id.IsGeneral()  &&  id.GetGeneral().GetTag().IsId() ) {
        const CDbtag& dbtag = id.GetGeneral();
        const string& db = dbtag.GetDb();
        int id = dbtag.GetTag().GetId();
        if ( id != 0 ) {
            TSatMap::const_iterator iter = sc_SatMap.find(db.c_str());
            if ( iter != sc_SatMap.end() ) {
                srs.push_back(Ref(new CSeqref(id,
                                              iter->second.first,
                                              id,
                                              iter->second.second,
                                              CSeqref::fHasAllLocal)));
                return;
            }
            else {
                try {
                    srs.push_back(Ref(new CSeqref(0,
                                                  NStr::StringToInt(db),
                                                  id)));
                    return;
                }
                catch (...) {
                }
            }
        }
    }
    CReader::ResolveSeq_id(srs, id, conn);
}


void CId1Reader::RetrieveSeqrefs(TSeqrefs& srs, int gi, TConn conn)
{
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
        LOG_POST(Warning<<"CId1Reader::RetrieveSeqrefs("<<gi<<"): "
                 "gi is private");
        return;
    }
    if ( info.GetSat() < 0 || info.GetSat_key() < 0 ) {
        LOG_POST(Warning<<"CId1Reader::RetrieveSeqrefs("<<gi<<"): "
                 "negative sat/satkey");
        return;
    }
   
    if ( TrySNPSplit() ) {
        {{
            // add main blob
            int sat = info.GetSat();
            int sat_key = info.GetSat_key();
            CRef<CSeqref> ref(new CSeqref(gi, sat, sat_key));
            ref->SetVersion(x_GetVersion(info));
            srs.push_back(ref);
        }}
        if ( info.IsSetExtfeatmask() ) {
            int ext_feat = info.GetExtfeatmask();
            while ( ext_feat ) {
                int bit = ext_feat & ~(ext_feat-1);
                ext_feat -= bit;
#ifdef GENBANK_USE_SNP_SATELLITE_15
                if ( bit == CSeqref::eSubSat_SNP ) {
                    AddSNPSeqref(srs, gi);
                    continue;
                }
#endif
                srs.push_back(Ref(new CSeqref(gi,
                                              CSeqref::eSat_ANNOT,
                                              gi,
                                              bit,
                                              CSeqref::fHasExternal)));
            }
        }
    }
    else {
        // whole blob
        int sat = info.GetSat();
        int sat_key = info.GetSat_key();
        int ext_feat = info.IsSetExtfeatmask()? info.GetExtfeatmask(): 0;
        CRef<CSeqref> ref(new CSeqref(gi, sat, sat_key, ext_feat,
                                      CSeqref::fHasAllLocal));
        ref->SetVersion(x_GetVersion(info));
        srs.push_back(ref);
    }
}


int CId1Reader::GetVersion(const CSeqref& seqref, TConn conn)
{
    if ( seqref.GetVersion() == 0 ) {
        const_cast<CSeqref&>(seqref).SetVersion(x_GetVersion(seqref, conn));
    }
    return seqref.GetVersion();
}


int CId1Reader::x_GetVersion(const CSeqref& seqref, TConn conn)
{
    CID1server_request id1_request;
    x_SetParams(seqref, id1_request.SetGetblobinfo());
    
    CID1server_back    id1_reply;
    x_ResolveId(id1_reply, id1_request, conn);
    
    if ( id1_reply.IsGotblobinfo() ) {
        return x_GetVersion(id1_reply.GetGotblobinfo());
    }
    else {
        return 1; // default non-zero version
    }
}


void CId1Reader::x_ResolveId(CID1server_back& id1_reply,
                             const CID1server_request& id1_request,
                             TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
#endif

    CConn_ServiceStream* stream = x_GetConnection(conn);
    {{
        CObjectOStreamAsnBinary out(*stream);
        out << id1_request;
        out.Flush();
    }}
    
    {{
        CObjectIStreamAsnBinary in(*stream);
        in >> id1_reply;
    }}
    /*
    if ( id1_reply.IsError() && id1_reply.GetError() == 0 ) {
        char next_byte;
        if ( CStreamUtils::Readsome(*stream, &next_byte, 1) ) {
            CStreamUtils::Pushback(*stream, &next_byte, 1);
            ERR_POST("Extra reply from ID1 server: ERROR 0");
            CObjectIStreamAsnBinary in(*stream);
            in >> id1_reply;
        }
    }
    */

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        if ( id1_request.Which() == CID1server_request::e_Getgi ) {
            LogStat("CId1Reader: resolved id", id1_request.GetGetgi(), time);
            resolve_id_count++;
            resolve_id_time += time;
        }
        else if ( id1_request.Which() == CID1server_request::e_Getblobinfo ) {
            const CID1server_maxcomplex& req = id1_request.GetGetblobinfo();
            if ( req.IsSetSat() ) {
                LogStat("CId1Reader: got blob version", req, time);
                resolve_ver_count++;
                resolve_ver_time += time;
            }
            else {
                LogStat("CId1Reader: resolved gi", req.GetGi(), time);
                resolve_gi_count++;
                resolve_gi_time += time;
            }
        }
    }
#endif
}


CRef<CTSE_Info> CId1Reader::GetTSEBlob(const CSeqref& seqref,
                                       TConn conn)
{
    if ( seqref.GetFlags() & CSeqref::fPrivate ) {
        NCBI_THROW(CLoaderException, ePrivateData, "gi is private");
    }
    CID1server_back id1_reply;
    CRef<CID2S_Split_Info> split_info;
    x_GetTSEBlob(id1_reply, split_info, seqref, conn);

    CRef<CSeq_entry> seq_entry;
    bool dead = false;
    switch ( id1_reply.Which() ) {
    case CID1server_back::e_Gotseqentry:
        seq_entry.Reset(&id1_reply.SetGotseqentry());
        break;
    case CID1server_back::e_Gotdeadseqentry:
        dead = true;
        seq_entry.Reset(&id1_reply.SetGotdeadseqentry());
        break;
    case CID1server_back::e_Error:
        switch ( id1_reply.GetError() ) {
        case 1:
            NCBI_THROW(CLoaderException, ePrivateData, "id is withdrawn");
        case 2:
            NCBI_THROW(CLoaderException, ePrivateData, "id is private");
        case 10:
            NCBI_THROW(CLoaderException, eNoData, "invalid args");
        }
        ERR_POST("CId1Reader::GetMainBlob: ID1server-back.error "<<
                 id1_reply.GetError());
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "ID1server-back.error");
        break;
    default:
        // no data
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "bad ID1server-back type");
    }
    CRef<CTSE_Info> ret(new CTSE_Info(*seq_entry, dead));
    if ( split_info ) {
        CSplitParser::Attach(*ret, *split_info);
    }
    return ret;
}


CRef<CSeq_annot_SNP_Info> CId1Reader::GetSNPAnnot(const CSeqref& seqref,
                                                  TConn conn)
{
    CRef<CSeq_annot_SNP_Info> ret(new CSeq_annot_SNP_Info);

    x_GetSNPAnnot(*ret, seqref, conn);

    return ret;
}


void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip)
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


void CId1Reader::x_GetTSEBlob(CID1server_back& id1_reply,
                              CRef<CID2S_Split_Info>& /*split_info*/,
                              const CSeqref& seqref,
                              TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    
    try {
#endif
        CConn_ServiceStream* stream = x_GetConnection(conn);
        x_SendRequest(seqref, stream);
        x_ReadTSEBlob(id1_reply, seqref, *stream);
        
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Reader: read blob",
                        seqref, last_object_bytes, time);
            main_blob_count++;
            main_bytes += last_object_bytes;
            main_time += time;
        }
    }
    catch ( ... ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Reader: read fail blob",
                        seqref, 0, time);
            main_blob_count++;
            main_time += time;
        }
        throw;
    }
#endif
}


void CId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                               const CSeqref& seqref,
                               TConn conn)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw;

    if ( CollectStatistics() ) {
        sw.Start();
    }

    try {
#endif
        CConn_ServiceStream* stream = x_GetConnection(conn);
        x_SendRequest(seqref, stream);

#ifdef ID1_COLLECT_STATS
        size_t compressed;
        double decompression_time;
        double total_read_time;
#endif
        {{
            const size_t kSkipHeader = 2, kSkipFooter = 2;
        
            CStreamByteSourceReader src(0, stream);
        
            Id1ReaderSkipBytes(src, kSkipHeader);
        
            CNlmZipBtRdr src2(&src);
        
            x_ReadSNPAnnot(snp_info, seqref, src2);

#ifdef ID1_COLLECT_STATS
            compressed = src2.GetCompressedSize();
            decompression_time = src2.GetDecompressionTime();
            total_read_time = src2.GetTotalReadTime();
#endif
        
            Id1ReaderSkipBytes(src, kSkipFooter);
        }}

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Reader: read SNP blob",
                        seqref, compressed, time);
            snp_blob_count++;
            snp_compressed += compressed;
            snp_uncompressed += last_object_bytes;
            snp_time += time;
            snp_decompression_time += decompression_time;
            snp_total_read_time += total_read_time;
        }
    }
    catch ( ... ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Reader: read fail SNP blob",
                        seqref, 0, time);
            snp_blob_count++;
            snp_time += time;
        }
        throw;
    }
#endif
}


void CId1Reader::x_SetParams(const CSeqref& seqref,
                             CID1server_maxcomplex& params)
{
    int bits = (~seqref.GetSubSat() & 0xffff) << 4;
    params.SetMaxplex(eEntry_complexities_entry | bits);
    params.SetGi(seqref.GetGi());
    params.SetEnt(seqref.GetSatKey());
    if ( seqref.GetSat() == CSeqref::eSat_ANNOT ) {
        params.SetMaxplex(eEntry_complexities_entry); // TODO: TEMP: remove
        params.SetSat("ANNOT:"+NStr::IntToString(seqref.GetSubSat()));
    }
    else {
        params.SetSat(NStr::IntToString(seqref.GetSat()));
    }
}


int CId1Reader::x_GetVersion(const CID1blob_info& info) const
{
    int version = info.GetBlob_state();
    if ( version < 0 ) {
        version = -version;
    }
    if ( version == 0 ) {
        // set to default: 1
        // so that we will not reask version in future
        version = 1;
    }
    return version;
}


void CId1Reader::x_SendRequest(const CSeqref& seqref,
                               CConn_ServiceStream* stream)
{
    CID1server_request id1_request;
    x_SetParams(seqref, id1_request.SetGetsefromgi());
    CObjectOStreamAsnBinary out(*stream);
    out << id1_request;
    out.Flush();
}


void CId1Reader::x_ReadTSEBlob(CID1server_back& id1_reply,
                               const CSeqref& /*seqref*/,
                               CNcbiIstream& stream)
{
    CObjectIStreamAsnBinary obj_stream(stream);
    
    x_ReadTSEBlob(id1_reply, obj_stream);
}


void CId1Reader::x_ReadTSEBlob(CID1server_back& id1_reply,
                               CObjectIStream& stream)
{
    CReader::SetSeqEntryReadHooks(stream);

    stream >> id1_reply;

#ifdef ID1_COLLECT_STATS
    last_object_bytes = stream.GetStreamOffset();
#endif
}


void CId1Reader::x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                const CSeqref& /*seqref*/,
                                CByteSourceReader& reader)
{
    CObjectIStreamAsnBinary in(reader);

    CSeq_annot_SNP_Info_Reader::Parse(in, snp_info);

#ifdef ID1_COLLECT_STATS
    last_object_bytes = in.GetStreamOffset();
#endif
}


END_SCOPE(objects)

const string kId1ReaderDriverName("id1_reader");


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



END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.82  2004/06/30 21:02:02  vasilche
 * Added loading of external annotations from 26 satellite.
 *
 * Revision 1.81  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.80  2004/03/16 17:49:18  vasilche
 * Use enum constant for TRACE_CHGR entries
 *
 * Revision 1.79  2004/03/16 15:47:29  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.78  2004/03/05 17:43:52  dicuccio
 * Added support for satellite 31: TRACE_CHGR
 *
 * Revision 1.77  2004/03/03 22:59:47  ucko
 * Gracefully handle CNcbiApplication::Instance returning NULL, as it
 * *is* possible to use other frameworks.
 *
 * Revision 1.76  2004/02/19 19:24:09  vasilche
 * Added conditional compilation of statistics collection code.
 *
 * Revision 1.75  2004/02/19 17:06:57  dicuccio
 * Use sat, sat-key in seq-ref, not sat-key, sat
 *
 * Revision 1.74  2004/02/18 14:01:25  dicuccio
 * Added new satellites for TRACE_ASSM, TR_ASSM_CH.  Added support for overloading
 * the ID1 named service
 *
 * Revision 1.73  2004/02/04 17:47:41  kuznets
 * Fixed naming of entry points
 *
 * Revision 1.72  2004/01/28 20:53:43  vasilche
 * Added CSplitParser::Attach().
 *
 * Revision 1.71  2004/01/22 20:53:30  vasilche
 * Fixed include path.
 *
 * Revision 1.70  2004/01/22 20:10:36  vasilche
 * 1. Splitted ID2 specs to two parts.
 * ID2 now specifies only protocol.
 * Specification of ID2 split data is moved to seqsplit ASN module.
 * For now they are still reside in one resulting library as before - libid2.
 * As the result split specific headers are now in objects/seqsplit.
 * 2. Moved ID2 and ID1 specific code out of object manager.
 * Protocol is processed by corresponding readers.
 * ID2 split parsing is processed by ncbi_xreader library - used by all readers.
 * 3. Updated OBJMGR_LIBS correspondingly.
 *
 * Revision 1.69  2004/01/13 21:54:49  vasilche
 * Requrrected new version
 *
 * Revision 1.5  2004/01/13 16:55:56  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.4  2003/12/30 22:14:42  vasilche
 * Updated genbank loader and readers plugins.
 *
 * Revision 1.67  2003/12/30 16:00:24  vasilche
 * Added support for new ICache (CBDB_Cache) interface.
 *
 * Revision 1.66  2003/12/19 19:47:44  vasilche
 * Added support for TRACE data, Seq-id ::= general { db "ti", tag id NNN }.
 *
 * Revision 1.65  2003/12/03 14:30:02  kuznets
 * Code clean up.
 * Made use of driver name constant instead of immediate in-place string.
 *
 * Revision 1.64  2003/12/02 16:18:16  kuznets
 * Added plugin manager support for CReader interface and implementaions
 * (id1 reader, pubseq reader)
 *
 * Revision 1.63  2003/11/26 17:55:59  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.62  2003/11/21 16:32:52  vasilche
 * Cleaned code avoiding ERROR 0 packets.
 *
 * Revision 1.61  2003/11/19 15:43:03  vasilche
 * Temporary fix for extra ERROR 0 packed from ID1 server.
 *  CVS: ----------------------------------------------------------------------
 *
 * Revision 1.60  2003/11/07 16:59:01  vasilche
 * Fixed stats message.
 *
 * Revision 1.59  2003/11/04 21:53:32  vasilche
 * Check only SNP bit in extfeat field in ID1 response.
 *
 * Revision 1.58  2003/10/27 18:50:49  vasilche
 * Detect 'private' blobs in ID1 reader.
 * Avoid reconnecting after ID1 server replied with error packet.
 *
 * Revision 1.57  2003/10/27 15:05:41  vasilche
 * Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
 * Added recognition of ID1 error codes: private, etc.
 * Some formatting of old code.
 *
 * Revision 1.56  2003/10/24 13:27:40  vasilche
 * Cached ID1 reader made more safe. Process errors and exceptions correctly.
 * Cleaned statistics printing methods.
 *
 * Revision 1.55  2003/10/22 16:36:25  vasilche
 * Fixed typo.
 *
 * Revision 1.54  2003/10/22 16:32:42  vasilche
 * MSVC forces to use stream::bad() method.
 *
 * Revision 1.53  2003/10/22 16:12:38  vasilche
 * Added CLoaderException::eNoConnection.
 * Added check for 'fail' state of ID1 connection stream.
 * CLoaderException::eNoConnection will be rethrown from CGBLoader.
 *
 * Revision 1.52  2003/10/21 16:32:50  vasilche
 * Cleaned ID1 statistics messages.
 * Now by setting GENBANK_ID1_STATS=1 CId1Reader collects and displays stats.
 * And by setting GENBANK_ID1_STATS=2 CId1Reader logs all activities.
 *
 * Revision 1.51  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.50  2003/10/14 21:06:25  vasilche
 * Fixed compression statistics.
 * Disabled caching of SNP blobs.
 *
 * Revision 1.49  2003/10/14 18:59:55  vasilche
 * Temporarily remove collection of compression statistics.
 *
 * Revision 1.48  2003/10/14 18:31:54  vasilche
 * Added caching support for SNP blobs.
 * Added statistics collection of ID1 connection.
 *
 * Revision 1.47  2003/10/08 14:16:13  vasilche
 * Added version of blobs loaded from ID1.
 *
 * Revision 1.46  2003/10/01 18:08:14  kuznets
 * s_SkipBytes renamed to Id1ReaderSkipBytes (made non static)
 *
 * Revision 1.45  2003/09/30 19:38:26  vasilche
 * Added support for cached id1 reader.
 *
 * Revision 1.44  2003/09/30 16:22:02  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.43  2003/08/27 14:25:22  vasilche
 * Simplified CCmpTSE class.
 *
 * Revision 1.42  2003/08/19 18:34:40  vasilche
 * Fixed warning about enum conversion.
 *
 * Revision 1.41  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 * Revision 1.40  2003/07/24 19:28:09  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * Revision 1.39  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.38  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.37  2003/05/13 20:14:40  vasilche
 * Catching exceptions and reconnection were moved from readers to genbank loader.
 *
 * Revision 1.36  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.35  2003/04/15 15:30:15  vasilche
 * Added include <memory> when needed.
 * Removed buggy buffer in printing methods.
 * Removed unnecessary include of stream_util.hpp.
 *
 * Revision 1.34  2003/04/15 14:24:08  vasilche
 * Changed CReader interface to not to use fake streams.
 *
 * Revision 1.33  2003/04/07 16:56:57  vasilche
 * Fixed unassigned member in ID1 request.
 *
 * Revision 1.32  2003/03/31 17:02:03  lavr
 * Some code reformatting to [more closely] meet coding requirements
 *
 * Revision 1.31  2003/03/30 07:00:29  lavr
 * MIPS-specific workaround for lame-designed stream read ops
 *
 * Revision 1.30  2003/03/28 03:28:14  lavr
 * CId1Reader::xsgetn() added; code heavily reformatted
 *
 * Revision 1.29  2003/03/27 19:38:13  vasilche
 * Use safe NStr::IntToString() instead of sprintf().
 *
 * Revision 1.28  2003/03/03 20:34:51  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.27  2003/03/01 22:26:56  kimelman
 * performance fixes
 *
 * Revision 1.26  2003/02/04 18:53:36  dicuccio
 * Include file clean-up
 *
 * Revision 1.25  2002/12/26 20:53:25  dicuccio
 * Minor tweaks to relieve compiler warnings in MSVC
 *
 * Revision 1.24  2002/12/26 16:39:24  vasilche
 * Object manager class CSeqMap rewritten.
 *
 * Revision 1.23  2002/11/18 19:48:43  grichenk
 * Removed "const" from datatool-generated setters
 *
 * Revision 1.22  2002/07/25 15:01:51  grichenk
 * Replaced non-const GetXXX() with SetXXX()
 *
 * Revision 1.21  2002/05/09 21:40:59  kimelman
 * MT tuning
 *
 * Revision 1.20  2002/05/06 03:28:47  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.19  2002/05/03 21:28:10  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.18  2002/04/17 19:52:02  kimelman
 * fix: no gi found
 *
 * Revision 1.17  2002/04/09 16:10:56  ucko
 * Split CStrStreamBuf out into a common location.
 *
 * Revision 1.16  2002/04/08 20:52:26  butanaev
 * Added PUBSEQ reader.
 *
 * Revision 1.15  2002/03/29 02:47:05  kimelman
 * gbloader: MT scalability fixes
 *
 * Revision 1.14  2002/03/27 20:23:50  butanaev
 * Added connection pool.
 *
 * Revision 1.13  2002/03/26 23:31:08  gouriano
 * memory leaks and garbage collector fix
 *
 * Revision 1.12  2002/03/26 18:48:58  butanaev
 * Fixed bug not deleting streambuf.
 *
 * Revision 1.11  2002/03/26 17:17:02  kimelman
 * reader stream fixes
 *
 * Revision 1.10  2002/03/26 15:39:25  kimelman
 * GC fixes
 *
 * Revision 1.9  2002/03/25 18:12:48  grichenk
 * Fixed bool convertion
 *
 * Revision 1.8  2002/03/25 17:49:13  kimelman
 * ID1 failure handling
 *
 * Revision 1.7  2002/03/25 15:44:47  kimelman
 * proper logging and exception handling
 *
 * Revision 1.6  2002/03/22 21:50:21  kimelman
 * bugfix: avoid history rtequest for nonexistent sequence
 *
 * Revision 1.5  2002/03/21 19:14:54  kimelman
 * GB related bugfixes
 *
 * Revision 1.4  2002/03/21 01:34:55  kimelman
 * gbloader related bugfixes
 *
 * Revision 1.3  2002/03/20 04:50:13  kimelman
 * GB loader added
 *
 * Revision 1.2  2002/01/16 18:56:28  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 1.1  2002/01/11 19:06:22  gouriano
 * restructured objmgr
 *
 * Revision 1.7  2001/12/17 21:38:24  butanaev
 * Code cleanup.
 *
 * Revision 1.6  2001/12/13 00:19:25  kimelman
 * bugfixes:
 *
 * Revision 1.5  2001/12/12 21:46:40  kimelman
 * Compare interface fix
 *
 * Revision 1.4  2001/12/10 20:08:02  butanaev
 * Code cleanup.
 *
 * Revision 1.3  2001/12/07 21:24:59  butanaev
 * Interface development, code beautyfication.
 *
 * Revision 1.2  2001/12/07 16:43:58  butanaev
 * Fixed includes.
 *
 * Revision 1.1  2001/12/07 16:10:23  butanaev
 * Switching to new reader interfaces.
 *
 * Revision 1.3  2001/12/06 20:37:05  butanaev
 * Fixed timeout problem.
 *
 * Revision 1.2  2001/12/06 18:06:22  butanaev
 * Ported to linux.
 *
 * Revision 1.1  2001/12/06 14:35:23  butanaev
 * New streamable interfaces designed, ID1 reimplemented.
 *
 */
