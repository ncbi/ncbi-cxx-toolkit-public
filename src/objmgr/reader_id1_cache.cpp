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
 *  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
 *
 *  File Description: Cached extension of data reader from ID1
 *
 */
#include <connect/ncbi_conn_stream.hpp>

#include <objmgr/reader_id1_cache.hpp>
#include <corelib/ncbitime.hpp>
#include <util/cache/blob_cache.hpp>
#include <util/cache/int_cache.hpp>
#include <util/rwstream.hpp>
#include <util/bytesrc.hpp>
#include <serial/objistr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/reader_zlib.hpp>

#include <objects/id1/id1__.hpp>

#include <serial/serial.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Utility function to skip part of the input byte source
void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip);


static size_t resolve_id_count = 0;
static double resolve_id_time = 0;

static size_t resolve_gi_count = 0;
static double resolve_gi_time = 0;

static size_t resolve_ver_count = 0;
static double resolve_ver_time = 0;

static size_t main_blob_count = 0;
static double main_bytes = 0;
static double main_time = 0;

static size_t snp_load_count = 0;
static double snp_load_bytes = 0;
static double snp_load_time = 0;

static size_t snp_store_count = 0;
static double snp_store_bytes = 0;
static double snp_store_time = 0;

CCachedId1Reader::CCachedId1Reader(TConn noConn, 
                                   IBLOB_Cache* blob_cache,
                                   IIntCache* id_cache)
    : CId1Reader(noConn),
      m_BlobCache(blob_cache),
      m_IdCache(id_cache)
{
}


CCachedId1Reader::~CCachedId1Reader()
{
    if ( CollectStatistics() ) {
        PrintStatistics();
    }
}


void CCachedId1Reader::PrintStatistics(void) const
{
    PrintStat("Cache resolution: resolved",
                resolve_id_count, "ids", resolve_id_time);
    PrintStat("Cache resolution: resolved",
                resolve_gi_count, "gis", resolve_gi_time);
    PrintStat("Cache resolution: resolved",
              resolve_ver_count, "blob ver", resolve_ver_time);
    PrintBlobStat("Cache non-SNP: loaded",
                  main_blob_count, main_bytes, main_time);
    PrintBlobStat("Cache SNP: loaded",
                  snp_load_count, snp_load_bytes, snp_load_time);
    PrintBlobStat("Cache SNP: stored",
                  snp_store_count, snp_store_bytes, snp_store_time);
}


void CCachedId1Reader::SetBlobCache(IBLOB_Cache* blob_cache)
{
    m_BlobCache = blob_cache;
}


void CCachedId1Reader::SetIdCache(IIntCache* id_cache)
{
    m_IdCache = id_cache;
}


void CCachedId1Reader::PurgeSeqrefs(const TSeqrefs& srs,
                                    const CSeq_id& /*id*/)
{
    if ( !m_IdCache ) {
        return;
    }

    int gi = -1;
    ITERATE ( TSeqrefs, it, srs ) {
        const CSeqref& sr = **it;
        if ( gi != sr.GetGi() ) {
            gi = sr.GetGi();
            m_IdCache->Remove(gi, 0);
            m_IdCache->Remove(gi, 1);
        }
    }
}


bool CCachedId1Reader::GetBlobInfo(int gi, TSeqrefs& srs)
{
    if (!m_IdCache) {
        return false;
    }

    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    vector<int> data;
    if ( !m_IdCache->Read(gi, 0, data) ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: failed to resolve gi", gi, time);
            resolve_gi_count++;
            resolve_gi_time += time;
        }
        return false;
    }
    
    _ASSERT(data.size() == 4 || data.size() == 8);

    for ( size_t pos = 0; pos + 4 <= data.size(); pos += 4 ) {
        int sat = data[pos];
        int satkey = data[pos+1];
        int version = data[pos+2];
        int flags = data[pos+3];
        CRef<CSeqref> sr(new CSeqref(gi, sat, satkey));
        sr->SetVersion(version);
        sr->SetFlags(flags);
        srs.push_back(sr);
    }

    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: resolved gi", gi, time);
        resolve_gi_count++;
        resolve_gi_time += time;
    }

    return true;
}


void CCachedId1Reader::StoreBlobInfo(int gi, const TSeqrefs& srs)
{
    if (!m_IdCache) 
        return;

    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    vector<int> data;

    ITERATE ( TSeqrefs, it, srs ) {
        const CSeqref& sr = **it;
        data.push_back(sr.GetSat());
        data.push_back(sr.GetSatKey());
        data.push_back(sr.GetVersion());
        data.push_back(sr.GetFlags());
    }

    _ASSERT(data.size() == 4 || data.size() == 8);

    m_IdCache->Store(gi, 0, data);

    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: saved gi", gi, time);
        resolve_gi_count++;
        resolve_gi_time += time;
    }
}


int CCachedId1Reader::GetSNPBlobVersion(int gi)
{
    if (!m_IdCache) 
        return 0;

    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    vector<int> data;
    if ( !m_IdCache->Read(gi, 1, data) ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: failed to get SNP ver",
                    gi, time);
            resolve_ver_count++;
            resolve_ver_time += time;
        }
        return 0;
    }
    
    _ASSERT(data.size() == 1);

    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: got SNP ver", gi, time);
        resolve_ver_count++;
        resolve_ver_time += time;
    }

    return data[0];
}


void CCachedId1Reader::StoreSNPBlobVersion(int gi, int version)
{
    if (!m_IdCache) 
        return;

    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    vector<int> data;
    data.push_back(version);

    _ASSERT(data.size() == 1);

    m_IdCache->Store(gi, 1, data);

    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: saved SNP ver", gi, time);
        resolve_ver_count++;
        resolve_ver_time += time;
    }
}


string CCachedId1Reader::x_GetBlobKey(const CSeqref& seqref)
{
    int sat = seqref.GetSat();
    int sat_key = seqref.GetSatKey();

    char szBlobKeyBuf[256];
    sprintf(szBlobKeyBuf, "%i-%i", sat, sat_key);
    return szBlobKeyBuf;
}


void CCachedId1Reader::x_RetrieveSeqrefs(TSeqrefs& sr,
                                         int gi,
                                         TConn conn)
{
    if ( !GetBlobInfo(gi, sr) ) {
        CId1Reader::x_RetrieveSeqrefs(sr, gi, conn);
        StoreBlobInfo(gi, sr);
    }
}


int CCachedId1Reader::x_GetVersion(const CSeqref& seqref, TConn conn)
{
    _ASSERT(IsSNPSeqref(seqref));
    _ASSERT(seqref.GetSatKey() == seqref.GetGi());
    int version = GetSNPBlobVersion(seqref.GetGi());
    if ( version == 0 ) {
        version = CId1Reader::x_GetVersion(seqref, conn);
        _ASSERT(version != 0);
        StoreSNPBlobVersion(seqref.GetGi(), version);
    }
    return version;
}


void CCachedId1Reader::x_GetBlob(CID1server_back& id1_reply,
                                 const CSeqref& seqref,
                                 TConn conn)
{
    // update seqref's version
    GetVersion(seqref, conn);
    if ( !LoadBlob(id1_reply, seqref) ) {
        // we'll intercept loading deeper and write loaded data on the fly
        CId1Reader::x_GetBlob(id1_reply, seqref, conn);
    }
}


void CCachedId1Reader::x_ReadBlob(CID1server_back& id1_reply,
                                  const CSeqref& seqref,
                                  CNcbiIstream& stream)
{
    if ( m_BlobCache ) {

        string key = x_GetBlobKey(seqref);
        int version = seqref.GetVersion();

        try {
            auto_ptr<IWriter> writer(m_BlobCache->GetWriteStream(key,
                                                                 version));
            
            if ( writer.get() ) {
                {{
                    CWriterByteSourceReader proxy(&stream, writer.get());
                    
                    CObjectIStreamAsnBinary obj_stream(proxy);
                    
                    CStreamDelayBufferGuard guard(obj_stream);
                    
                    CId1Reader::x_ReadBlob(id1_reply, obj_stream);
                }}

                writer.reset();
                // everything is fine
                return;
            }
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            try {
                m_BlobCache->Remove(key);
            }
            catch ( exception& /*exc*/ ) {
                // ignored
            }

            // continue with exception
            throw;
        }
    }
    // by deault read from ID1
    CId1Reader::x_ReadBlob(id1_reply, seqref, stream);
}


void CCachedId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                     const CSeqref& seqref,
                                     TConn conn)
{
    // update seqref's version
    GetVersion(seqref, conn);
    if ( !LoadSNPTable(snp_info, seqref) ) {
        snp_info.Reset();
        // load SNP table from GenBank
        CId1Reader::x_GetSNPAnnot(snp_info, seqref, conn);

        // and store SNP table in cache
        StoreSNPTable(snp_info, seqref);
    }
}


bool CCachedId1Reader::LoadBlob(CID1server_back& id1_reply,
                                const CSeqref& seqref)
{
    if ( !m_BlobCache ) {
        return false;
    }

    string key = x_GetBlobKey(seqref);
    int version = seqref.GetVersion();

    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    try {
        auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version));
        if ( !reader.get() ) {
            return false;
        }

        CIRByteSourceReader rd(reader.get());
        
        CObjectIStreamAsnBinary in(rd);
        
        CReader::SetSeqEntryReadHooks(in);
        in >> id1_reply;
        
        // everything is fine
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            size_t size = in.GetStreamOffset();
            LogBlobStat("CId1Cache: read blob", seqref, size, time);
            main_blob_count++;
            main_bytes += size;
            main_time += time;
        }

        return true;
    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache: Exception while loading cached blob: " <<
                 seqref.printTSE() << ": " << exc.what());

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Cache: read fail blob",
                        seqref, 0, time);
            main_blob_count++;
            main_time += time;
        }

        return false;
    }
}



bool CCachedId1Reader::LoadSNPTable(CSeq_annot_SNP_Info& snp_info,
                                    const CSeqref& seqref)
{
    if (!m_BlobCache) {
        return false;
    }
    
    string key = x_GetBlobKey(seqref);
    int version = seqref.GetVersion();
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    try {
        auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version));
        if ( !reader.get() ) {
            return false;
        }
        CRStream stream(reader.get());

        // blob type
        char type[4];
        if ( !stream.read(type, 4) || memcmp(type, "STBL", 4) != 0 ) {
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail SNP blob",
                            seqref, 0, time);
                snp_load_count++;
                snp_load_time += time;
            }

            return false;
        }

        // table
        snp_info.LoadFrom(stream);

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            size_t size = m_BlobCache->GetSize(key, version);
            LogBlobStat("CId1Cache: read SNP blob",
                        seqref, size, time);
            snp_load_count++;
            snp_load_bytes += size;
            snp_load_time += time;
        }

        return true;
    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache: "
                 "Exception while loading cached SNP table: "<<
                 seqref.printTSE() << ": " << exc.what());
        snp_info.Reset();

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Cache: read fail SNP blob",
                        seqref, 0, time);
            snp_load_count++;
            snp_load_time += time;
        }

        return false;
    }
}


void CCachedId1Reader::StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                                     const CSeqref& seqref)
{
    if ( !m_BlobCache ) {
        return;
    }

    string key = x_GetBlobKey(seqref);
    int version = seqref.GetVersion();
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    try {
        {{
            auto_ptr<IWriter> writer;
            writer.reset(m_BlobCache->GetWriteStream(key, version));
            if ( !writer.get() ) {
                return;
            }

            {{
                CWStream stream(writer.get());
                stream.write("STBL", 4);
                snp_info.StoreTo(stream);
            }}
            writer.reset();
        }}

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            size_t size = m_BlobCache->GetSize(key, version);
            LogBlobStat("CId1Cache: saved SNP blob",
                        seqref, size, time);
            snp_store_count++;
            snp_store_bytes += size;
            snp_store_time += time;
        }
    }        
    catch ( exception& exc ) {
        ERR_POST("CId1Cache: "
                 "Exception while storing SNP table: "<<
                 seqref.printTSE() << ": " << exc.what());
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Cache: save fail SNP blob",
                        seqref, 0, time);
            snp_store_count++;
            snp_store_time += time;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.14  2003/10/27 15:05:41  vasilche
 * Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
 * Added recognition of ID1 error codes: private, etc.
 * Some formatting of old code.
 *
 * Revision 1.13  2003/10/24 15:36:46  vasilche
 * Fixed incorrect order of objects' destruction - IWriter before object stream.
 *
 * Revision 1.12  2003/10/24 13:27:40  vasilche
 * Cached ID1 reader made more safe. Process errors and exceptions correctly.
 * Cleaned statistics printing methods.
 *
 * Revision 1.11  2003/10/23 13:48:38  vasilche
 * Use CRStream and CWStream instead of strstreams.
 *
 * Revision 1.10  2003/10/21 16:32:50  vasilche
 * Cleaned ID1 statistics messages.
 * Now by setting GENBANK_ID1_STATS=1 CId1Reader collects and displays stats.
 * And by setting GENBANK_ID1_STATS=2 CId1Reader logs all activities.
 *
 * Revision 1.9  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.8  2003/10/14 21:06:25  vasilche
 * Fixed compression statistics.
 * Disabled caching of SNP blobs.
 *
 * Revision 1.7  2003/10/14 19:31:18  kuznets
 * Removed unnecessary hook in SNP deserialization.
 *
 * Revision 1.6  2003/10/14 18:31:55  vasilche
 * Added caching support for SNP blobs.
 * Added statistics collection of ID1 connection.
 *
 * Revision 1.5  2003/10/08 18:58:23  kuznets
 * Implemented correct ID1 BLOB versions
 *
 * Revision 1.4  2003/10/03 17:41:44  kuznets
 * Added an option, that cache is owned by the ID1 reader.
 *
 * Revision 1.3  2003/10/02 19:29:14  kuznets
 * First working revision
 *
 * Revision 1.2  2003/10/01 19:32:22  kuznets
 * Work in progress
 *
 * Revision 1.1  2003/09/30 19:38:26  vasilche
 * Added support for cached id1 reader.
 *
 */
