#ifndef READER_ID1_CACHE__HPP_INCLUDED
#define READER_ID1_CACHE__HPP_INCLUDED

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
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objmgr/reader_id1.hpp>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;
class IIntCache;
class IReader;
class IWriter;

BEGIN_SCOPE(objects)

/// ID1 reader with caching capabilities
///
class NCBI_XOBJMGR_EXPORT CCachedId1Reader : public CId1Reader
{
public:
    CCachedId1Reader(TConn noConn = 5, 
                     IBLOB_Cache* blob_cache = 0,
                     IIntCache* id_cache = 0);
    ~CCachedId1Reader();

    void SetBlobCache(IBLOB_Cache* blob_cache);
    void SetIdCache(IIntCache* id_cache);

protected:
    
    void x_RetrieveSeqrefs(TSeqrefs& sr, int gi, TConn conn);
    int x_GetVersion(const CSeqref& seqref, TConn conn);

    void x_GetBlob(CID1server_back& id1_reply,
                   const CSeqref& seqref,
                   TConn conn);
    void x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                       const CSeqref& seqref,
                       TConn conn);

    void x_ReadBlob(CID1server_back& id1_reply,
                    const CSeqref&   seqref,
                    CNcbiIstream&    stream);
    /*
    void x_ReadSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                        const CSeqref& seqref,
                        CByteSourceReader&  reader);
    */

    /// Return BLOB key string based on CSeqref Sat() and SatKey()
    /// @sa CSeqref::Sat(), CSeqref::SatKey()
    string x_GetBlobKey(const CSeqref& seqref);

    /// Return IReader interface if BLOB described by the seqref 
    /// exists in cache. If IBLOB_Cache is not set or 
    /// BLOB does not exist function returns NULL
    IReader* OpenBlobReader(const CSeqref& seqref);

    /// Return IWriter interface to store BLOB described by the seqref
    /// Function returns NULL if IBLOB_Cache interface is not set.
    IWriter* OpenBlobWriter(const CSeqref& seqref);


    bool GetBlobInfo(int gi, TSeqrefs& sr);
    void StoreBlobInfo(int gi, const TSeqrefs& sr);

    int GetSNPBlobVersion(int gi);
    void StoreSNPBlobVersion(int gi, int version);

    bool LoadBlob(CID1server_back& id1_reply, const CSeqref& seqref);

    bool LoadSNPTable(CSeq_annot_SNP_Info& snp_info,
                      const CSeqref& seqref);
    void StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                       const CSeqref& seqref);

private:

    CCachedId1Reader(const CCachedId1Reader& );
    CCachedId1Reader& operator=(const CCachedId1Reader&);

private:
    IBLOB_Cache*   m_BlobCache;
    IIntCache*     m_IdCache;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.7  2003/10/21 14:27:34  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.6  2003/10/14 18:31:53  vasilche
* Added caching support for SNP blobs.
* Added statistics collection of ID1 connection.
*
* Revision 1.5  2003/10/08 18:57:49  kuznets
* Implemeneted correct ID1 BLOB versions.
*
* Revision 1.4  2003/10/03 17:41:33  kuznets
* Added an option, that cache is owned by the ID1 reader.
* Cache destroyed with the reader.
*
* Revision 1.3  2003/10/02 19:28:34  kuznets
* First working revision
*
* Revision 1.2  2003/10/01 19:32:01  kuznets
* Work in progress
*
* Revision 1.1  2003/09/30 19:38:26  vasilche
* Added support for cached id1 reader.
*
*/

#endif // READER_ID1_CACHE__HPP_INCLUDED
