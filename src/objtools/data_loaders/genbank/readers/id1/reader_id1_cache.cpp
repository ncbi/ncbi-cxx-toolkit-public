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
#include <util/cache/blob_cache.hpp>
#include <util/bytesrc.hpp>
#include <serial/objistr.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/reader_zlib.hpp>

#include <objects/id1/id1__.hpp>

#include <serial/serial.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Utility function to skip part of the input byte source
void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip);



CCachedId1Reader::CCachedId1Reader(TConn noConn, 
                                   IBLOB_Cache* cache,
                                   EOwnership take_ownership)
    : CId1Reader(noConn),
      m_Cache(cache),
      m_OwnCache(take_ownership)
{
}


CCachedId1Reader::~CCachedId1Reader()
{
    if (m_OwnCache == eTakeOwnership) {
        delete m_Cache;
    }
}

void CCachedId1Reader::SetCache(IBLOB_Cache* cache, EOwnership take_ownership)
{
    
    if (m_OwnCache == eTakeOwnership) {
        delete m_Cache;
    }
    m_Cache = cache;
    m_OwnCache = take_ownership;
}

IReader* CCachedId1Reader::OpenBlob(const CSeqref& seqref, 
                                    int version)
{
    if (!m_Cache) 
        return 0;

    string blob_key = x_GetBlobKey(seqref);

    IReader* reader = 
       m_Cache->GetReadStream(blob_key, version); 

    if (reader) {
        _TRACE("Retriving cached BLOB. key = " << blob_key);
    }

    return reader;
}

IWriter* CCachedId1Reader::StoreBlob(const CSeqref& seqref,
                                     int   version)
{
    if (!m_Cache) 
        return 0;

    string blob_key = x_GetBlobKey(seqref);

    IWriter* writer = 
        m_Cache->GetWriteStream(blob_key, version);
    
    if (writer) {
        _TRACE("Writing cache BLOB. key = " << blob_key);
    }

    return writer;    
}

string CCachedId1Reader::x_GetBlobKey(const CSeqref& seqref)
{
    int sat = seqref.GetSat();
    int sat_key = seqref.GetSatKey();

    char szBlobKeyBuf[256];
    sprintf(szBlobKeyBuf, "%i-%i", sat, sat_key);
    return szBlobKeyBuf;
}


void CCachedId1Reader::x_GetBlob(CID1server_back& id1_reply,
                                 const CSeqref& seqref,
                                 TConn conn)
{
    auto_ptr<IReader> reader(OpenBlob(seqref, GetVersion(seqref, conn)));
    if ( reader.get() ) {

        CIRByteSourceReader rd(reader.get());

        auto_ptr<CObjectIStream> in(CObjectIStream::Create(eSerial_AsnBinary, rd));

        CReader::SetSeqEntryReadHooks(*in);
        *in >> id1_reply;

    }
    else {
        CId1Reader::x_GetBlob(id1_reply, seqref, conn);
    }
}



void CCachedId1Reader::x_ReadBlob(CID1server_back& id1_reply,
                                  const CSeqref& seqref,
                                  CNcbiIstream& stream)
{
    IWriter* wr = StoreBlob(seqref, seqref.GetVersion());
    auto_ptr<IWriter> writer(wr);
    if ( writer.get() ) {
        try {
            CWriterByteSourceReader proxy(&stream, writer.get());

            auto_ptr<CObjectIStream> in(CObjectIStream::Create(eSerial_AsnBinary,proxy));
            
            CReader::SetSeqEntryReadHooks(*in);
            
            CStreamDelayBufferGuard guard(*in);
            
            *in >> id1_reply;

        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            if (m_Cache) {
                string blob_key = x_GetBlobKey(seqref);
                m_Cache->Remove(blob_key);
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
    auto_ptr<IReader> reader(OpenBlob(seqref, GetVersion(seqref, conn)));
    if ( reader.get() ) {

        CIRByteSourceReader rd(reader.get());

        auto_ptr<CObjectIStream> in(CObjectIStream::Create(eSerial_AsnBinary, rd));

        snp_info.Read(*in);
    }
    else {
        CId1Reader::x_GetSNPAnnot(snp_info, seqref, conn);
    }
}


void CCachedId1Reader::x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                      const CSeqref& seqref,
                                      CByteSourceReader& reader)
{
    IWriter* wr = StoreBlob(seqref, seqref.GetVersion());
    auto_ptr<IWriter> writer(wr);
    if ( writer.get() ) {
        try {
            CWriterCopyByteSourceReader proxy(&reader, writer.get());

            auto_ptr<CObjectIStream> in(CObjectIStream::Create(eSerial_AsnBinary,proxy));
                        
            CStreamDelayBufferGuard guard(*in);
            
            snp_info.Read(*in);
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            if (m_Cache) {
                string blob_key = x_GetBlobKey(seqref);
                m_Cache->Remove(blob_key);
            }
            
            throw;
        }
    }
    else {
        CId1Reader::x_ReadSNPAnnot(snp_info, seqref, reader);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
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
