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
}


void CCachedId1Reader::SetBlobCache(IBLOB_Cache* blob_cache)
{
    m_BlobCache = blob_cache;
}


void CCachedId1Reader::SetIdCache(IIntCache* id_cache)
{
    m_IdCache = id_cache;
}


IReader* CCachedId1Reader::OpenBlobReader(const CSeqref& seqref)
{
    if (!m_BlobCache) 
        return 0;

    string blob_key = x_GetBlobKey(seqref);

    IReader* reader = m_BlobCache->GetReadStream(blob_key,
                                                 seqref.GetVersion()); 

    if (reader) {
        _TRACE("Retrieving cached BLOB. key = " << blob_key);
    }

    return reader;
}


IWriter* CCachedId1Reader::OpenBlobWriter(const CSeqref& seqref)
{
    if (!m_BlobCache) 
        return 0;

    string blob_key = x_GetBlobKey(seqref);

    IWriter* writer = m_BlobCache->GetWriteStream(blob_key,
                                                  seqref.GetVersion());
    
    if (writer) {
        _TRACE("Writing cache BLOB. key = " << blob_key);
    }

    return writer;    
}


bool CCachedId1Reader::GetBlobInfo(int gi, TSeqrefs& srs)
{
    if (!m_IdCache) 
        return false;

    vector<int> data;
    if ( !m_IdCache->Read(gi, 0, data) ) {
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

    return true;
}


void CCachedId1Reader::StoreBlobInfo(int gi, const TSeqrefs& srs)
{
    if (!m_IdCache) 
        return;

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
}


int CCachedId1Reader::GetSNPBlobVersion(int gi)
{
    if (!m_IdCache) 
        return 0;

    vector<int> data;
    if ( !m_IdCache->Read(gi, 1, data) ) {
        return 0;
    }
    
    _ASSERT(data.size() == 1);

    return data[0];
}


void CCachedId1Reader::StoreSNPBlobVersion(int gi, int version)
{
    if (!m_IdCache) 
        return;

    vector<int> data;
    data.push_back(version);

    _ASSERT(data.size() == 1);

    m_IdCache->Store(gi, 1, data);
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
    auto_ptr<IWriter> writer;
    writer.reset(OpenBlobWriter(seqref));
    if ( writer.get() ) {
        try {
            CWriterByteSourceReader proxy(&stream, writer.get());

            CObjectIStreamAsnBinary obj_stream(proxy);
            
            CStreamDelayBufferGuard guard(obj_stream);
            
            CId1Reader::x_ReadBlob(id1_reply, obj_stream);
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            if (m_BlobCache) {
                string blob_key = x_GetBlobKey(seqref);
                m_BlobCache->Remove(blob_key);
            }
            
            throw;
        }
    }
    else {
        CId1Reader::x_ReadBlob(id1_reply, seqref, stream);
    }
}


void CCachedId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                     const CSeqref& seqref,
                                     TConn conn)
{
    // update seqref's version
    GetVersion(seqref, conn);
    if ( !LoadSNPTable(snp_info, seqref) ) {
        // load SNP table from GenBank
        CId1Reader::x_GetSNPAnnot(snp_info, seqref, conn);

        // and store SNP table in cache
        StoreSNPTable(snp_info, seqref);
    }
}

/*
void CCachedId1Reader::x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                      const CSeqref& seqref,
                                      CByteSourceReader& reader)
{
    auto_ptr<IWriter> writer;
    writer.reset(OpenBlobWriter(seqref));
    if ( writer.get() ) {
        try {
            CWriterCopyByteSourceReader proxy(&reader, writer.get());

            CObjectIStreamAsnBinary in(proxy);
                        
            CStreamDelayBufferGuard guard(in);
            
            snp_info.Read(in);
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            if (m_BlobCache) {
                string blob_key = x_GetBlobKey(seqref);
                m_BlobCache->Remove(blob_key);
            }
            
            throw;
        }
    }
    else {
        CId1Reader::x_ReadSNPAnnot(snp_info, seqref, reader);
    }
}
*/


bool CCachedId1Reader::LoadBlob(CID1server_back& id1_reply,
                                const CSeqref& seqref)
{
    try {
        auto_ptr<IReader> reader(OpenBlobReader(seqref));
        if ( !reader.get() ) {
            return false;
        }

        CIRByteSourceReader rd(reader.get());
        
        CObjectIStreamAsnBinary in(rd);
        
        CReader::SetSeqEntryReadHooks(in);
        in >> id1_reply;
        
        // everything is fine
        return true;
    }
    catch ( exception& exc ) {
        ERR_POST("CCachedId1Reader: Exception while loading cached blob: " <<
                 seqref.printTSE() << ": " << exc.what());
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
    try {
        CStopWatch sw;
        sw.Start();

        size_t size = m_BlobCache->GetSize(key, version);
        if ( size == 0 ) {
            return false;
        }
        
        AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
        if ( !m_BlobCache->Read(key, version, buf.get(), size) ) {
            return false;
        }

        double time = sw.Elapsed();
        LOG_POST("CCachedId1Reader: Loaded SNP table: "<<seqref.printTSE()<<
                 " size: " << size << " in " << (time*1000) << " ms");

        CNcbiIstrstream stream(buf.get(), size);

        // blob type
        char type[4];
        if ( !stream.read(type, 4) || memcmp(type, "STBL", 4) != 0 ) {
            return false;
        }

        // table
        snp_info.LoadFrom(stream);
        return true;
    }
    catch ( exception& exc ) {
        ERR_POST("CCachedId1Reader: "
                 "Exception while loading cached SNP table: "<<
                 seqref.printTSE() << ": " << exc.what());
        snp_info.Reset();
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
    try {
        CNcbiOstrstream stream;
        stream.write("STBL", 4);
        snp_info.StoreTo(stream);

        size_t size = stream.pcount();
        const char* buf = stream.str();
        stream.freeze(false);

        CStopWatch sw;
        sw.Start();
        m_BlobCache->Store(key, version, buf, size);

        double time = sw.Elapsed();
        LOG_POST("CCachedId1Reader: Storing SNP table: "<<seqref.printTSE()<<
                 " size: " << size << " in " << (time*1000) << " ms");
    }        
    catch ( exception& exc ) {
        ERR_POST("CCachedId1Reader: "
                 "Exception while storing SNP table: "<<
                 seqref.printTSE() << ": " << exc.what());
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
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
