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

#include <objmgr/reader_id1.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/seqref_id1.hpp>
#include <objmgr/impl/reader_zlib.hpp>
#include <objmgr/impl/reader_snp.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbitime.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id1/id1__.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static int resolve_id_count = 0;
static double resolve_id_time = 0;
static int resolve_gi_count = 0;
static double resolve_gi_time = 0;
static int blob_count = 0;
static double bytes_count = 0;
static double last_bytes_count = 0;
static double bytes_time = 0;
static double compr_size = 0;
static double decompr_size = 0;

static bool GetConnectionStatistics(void)
{
    const char* env = getenv("GENBANK_ID1_STATS");
    return env && *env && *env != '0';
}


static bool ConnectionStatistics(void)
{
    static bool ret = GetConnectionStatistics();
    return ret;
}

CId1Reader::CId1Reader(TConn noConn)
{
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
    if ( ConnectionStatistics() ) {
        if ( resolve_id_count ) {
            LOG_POST("Total: resolved ids "<<resolve_id_count<<" in "<<
                     (resolve_id_time*1000)<<" ms "<<
                     (resolve_id_time*1000/resolve_id_count)<<" ms/one");
        }
        if ( resolve_gi_count ) {
            LOG_POST("Total: resolved "<<resolve_gi_count<<" in "<<
                     (resolve_gi_time*1000)<<" ms "<<
                     (resolve_gi_time*1000/resolve_gi_count)<<" ms/one");
        }
        LOG_POST("Total: loaded "<<blob_count<<" blobs "<<
                 (bytes_count/1024)<<" kB in "<<
                 (bytes_time*1000)<<" ms "<<
                 (bytes_count/bytes_time/1024)<<" kB/s");
        if ( compr_size ) {
            LOG_POST("Total SNP: "<<(compr_size/1024)<<" kB -> decompressed "<<
                     (decompr_size/1024)<<" kB, compession ratio: "<<
                     (decompr_size/compr_size));
        }
    }
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
        ret = m_Pool[conn] = x_NewConnection();
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
    _TRACE("CId1Reader(" << this << ")->x_NewConnection()");
    STimeout tmout;
    tmout.sec = 20;
    tmout.usec = 0;
    return new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout);
}

void CId1Reader::RetrieveSeqrefs(TSeqrefs& srs,
                                 const CSeq_id& seqId,
                                 TConn conn)
{
    x_RetrieveSeqrefs(srs, seqId, x_GetConnection(conn));
}


void CId1Reader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                   const CSeq_id& seqId,
                                   CConn_ServiceStream* stream)
{
    int gi;
    if ( !seqId.IsGi() ) {
        gi = x_ResolveSeq_id_to_gi(seqId, stream);
    }
    else {
        gi = seqId.GetGi();
    }

    if ( gi ) {
        x_RetrieveSeqrefs(srs, gi, stream);
    }
}


void CId1Reader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                   int gi,
                                   CConn_ServiceStream* stream)
{
    CID1server_request id1_request;
    {{
        CID1server_maxcomplex& blob = id1_request.SetGetblobinfo();
        blob.SetMaxplex(eEntry_complexities_entry);
        blob.SetGi(gi);
    }}
    
    CID1server_back id1_reply;
    x_GetBlobInfo(id1_reply, id1_request, stream);

    if ( !id1_reply.IsGotblobinfo() ) {
        return;
    }

    const CID1blob_info& info = id1_reply.GetGotblobinfo();
    CRef<CSeqref> ref(new CSeqref(gi, info.GetSat(), info.GetSat_key()));
    x_UpdateVersion(*ref, info);
    srs.push_back(ref);
   
    if ( TrySNPSplit() ) {
        if ( !info.IsSetExtfeatmask() ) {
            AddSNPSeqref(srs, gi, CSeqref::fPossible);
        }
        else if ( info.GetExtfeatmask() != 0 ) {
            AddSNPSeqref(srs, gi);
        }
    }
}


int CId1Reader::GetVersion(const CSeqref& seqref,
                           TConn conn)
{
    if ( seqref.GetVersion() == 0 ) {
        CID1server_request id1_request;
        x_SetParams(seqref, id1_request.SetGetblobinfo(), IsSNPSeqref(seqref));

        CID1server_back    id1_reply;
        x_GetBlobInfo(id1_reply, id1_request, x_GetConnection(conn));
        
        if ( id1_reply.IsGotblobinfo() ) {
            x_UpdateVersion(const_cast<CSeqref&>(seqref),
                            id1_reply.GetGotblobinfo());
        }
    }
    return seqref.GetVersion();
}


void CId1Reader::x_GetBlobInfo(CID1server_back& id1_reply,
                               const CID1server_request& id1_request,
                               CConn_ServiceStream* stream)
{
    CStopWatch sw;
    if ( ConnectionStatistics() ) {
        sw.Start();
    }

    {{
        CObjectOStreamAsnBinary out(*stream);
        out << id1_request;
    }}
    
    {{
        CObjectIStreamAsnBinary in(*stream);
        in >> id1_reply;
    }}

    if ( ConnectionStatistics() ) {
        double time = sw.Elapsed();
        LOG_POST("CId1Reader: resolved gi in "<<(time*1000)<<" ms");
        resolve_gi_count++;
        resolve_gi_time += time;
    }
}


int CId1Reader::x_ResolveSeq_id_to_gi(const CSeq_id& seqId,
                                      CConn_ServiceStream* stream)
{
    CStopWatch sw;
    if ( ConnectionStatistics() ) {
        sw.Start();
    }

    {{
        CID1server_request id1_request;
        id1_request.SetGetgi(const_cast<CSeq_id&>(seqId));
        
        CObjectOStreamAsnBinary out(*stream);
        out << id1_request;
    }}
    
    CID1server_back id1_reply;
    {{
        CObjectIStreamAsnBinary in(*stream);
        in >> id1_reply;
    }}

    int gi;
    if ( !id1_reply.IsGotgi() ) {
        gi = 0;
    }
    else {
        gi = id1_reply.GetGotgi();
    }
    
    if ( ConnectionStatistics() ) {
        double time = sw.Elapsed();
        LOG_POST("CId1Reader: resolved "<<seqId.AsFastaString()<<" in "<<
                 (time*1000)<<" ms");
        resolve_id_count++;
        resolve_id_time += time;
    }

    return gi;
}


CRef<CTSE_Info> CId1Reader::GetMainBlob(const CSeqref& seqref,
                                        TConn conn)
{
    CID1server_back id1_reply;
    x_GetBlob(id1_reply, seqref, conn);

    CRef<CSeq_entry> seq_entry;
    if ( id1_reply.IsGotseqentry() ) {
        seq_entry.Reset(&id1_reply.SetGotseqentry());
    }
    else if (id1_reply.IsGotdeadseqentry()) {
        seq_entry.Reset(&id1_reply.SetGotdeadseqentry());
    }
    else {
        // no data
        NCBI_THROW(CLoaderException, eNoData, "no data");
    }

    return Ref(new CTSE_Info(*seq_entry, id1_reply.IsGotdeadseqentry()));
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


void CId1Reader::x_GetBlob(CID1server_back& id1_reply,
                           const CSeqref& seqref,
                           TConn conn)
{
    CStopWatch sw;
    if ( ConnectionStatistics() ) {
        sw.Start();
    }
    
    CConn_ServiceStream* stream = x_GetConnection(conn);
    x_SendRequest(seqref, stream, false);
    x_ReadBlob(id1_reply, seqref, *stream);

    if ( ConnectionStatistics() ) {
        double time = sw.Elapsed();
        LOG_POST("CId1Reader: read blob "<<seqref.printTSE()<<" "<<
                 (last_bytes_count/1024)<<" kB in "<<(time*1000)<<" ms");
        blob_count++;
        bytes_count += last_bytes_count;
        bytes_time += time;
    }
}


void CId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                               const CSeqref& seqref,
                               TConn conn)
{
    CStopWatch sw;
    //extern int s_ResultZBtSrcX_compr_size;

    if ( ConnectionStatistics() ) {
        //s_ResultZBtSrcX_compr_size = 0;
        
        sw.Start();
    }

    CConn_ServiceStream* stream = x_GetConnection(conn);
    x_SendRequest(seqref, stream, true);

    {{
        const size_t kSkipHeader = 2, kSkipFooter = 2;
        
        CStreamByteSourceReader src(0, stream);
        
        Id1ReaderSkipBytes(src, kSkipHeader);
        
        CResultZBtSrcRdr src2(&src);
        
        x_ReadSNPAnnot(snp_info, seqref, src2);
        
        Id1ReaderSkipBytes(src, kSkipFooter);
    }}

    if ( ConnectionStatistics() ) {
        double time = sw.Elapsed();
        int s_ResultZBtSrcX_compr_size = 0;
        LOG_POST("CId1Reader: read SNP blob "<<seqref.printTSE()<<" "<<
                 (s_ResultZBtSrcX_compr_size/1024)<<" kB in "<<
                 (time*1000)<<" ms");
        blob_count++;
        bytes_count += s_ResultZBtSrcX_compr_size;
        compr_size += s_ResultZBtSrcX_compr_size;
        decompr_size += last_bytes_count;
        bytes_time += time;
    }
}


void CId1Reader::x_SetParams(const CSeqref& seqref,
                             CID1server_maxcomplex& params,
                             bool is_snp)
{
    bool is_external = is_snp;
    bool skip_extfeat = !is_external && TrySNPSplit();
    enum {
        kNoExtFeat = 1<<4
    };
    EEntry_complexities maxplex = eEntry_complexities_entry;
    if ( skip_extfeat ) {
        maxplex = EEntry_complexities(int(maxplex) | kNoExtFeat);
    }
    params.SetMaxplex(maxplex);
    params.SetGi(seqref.GetGi());
    params.SetEnt(seqref.GetSatKey());
    params.SetSat(NStr::IntToString(seqref.GetSat()));
}


void CId1Reader::x_UpdateVersion(CSeqref& seqref,
                                 const CID1blob_info& info)
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
    seqref.SetVersion(version);
}


void CId1Reader::x_SendRequest(const CSeqref& seqref,
                               CConn_ServiceStream* stream,
                               bool is_snp)
{
    CID1server_request id1_request;
    x_SetParams(seqref, id1_request.SetGetsefromgi(), is_snp);
    CObjectOStreamAsnBinary out(*stream);
    out << id1_request;
    out.Flush();
}


void CId1Reader::x_ReadBlob(CID1server_back& id1_reply,
                            const CSeqref& /*seqref*/,
                            CNcbiIstream& stream)
{
    CObjectIStreamAsnBinary in(stream);
    CReader::SetSeqEntryReadHooks(in);

    in >> id1_reply;

    last_bytes_count = in.GetStreamOffset();
}


void CId1Reader::x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                const CSeqref& /*seqref*/,
                                CByteSourceReader& reader)
{
    auto_ptr<CObjectIStream> in;
    in.reset(CObjectIStream::Create(eSerial_AsnBinary, reader));

    snp_info.Read(*in);

    last_bytes_count = in->GetStreamOffset();
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
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
