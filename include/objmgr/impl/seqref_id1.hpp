#ifndef SEQREF_ID1__HPP_INCLUDED
#define SEQREF_ID1__HPP_INCLUDED

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
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: support classes for data reader from ID1
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/reader.hpp>

#include <objects/id1/ID1server_back.hpp>
#include <objects/id1/ID1server_request.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <connect/ncbi_conn_stream.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CId1Reader;

class NCBI_XOBJMGR_EXPORT CId1Seqref : public CSeqref
{
public:
    CId1Seqref(CId1Reader& reader, int gi, int sat, int satkey);
    ~CId1Seqref(void);

    virtual CBlobSource* GetBlobSource(TPos start, TPos stop,
                                       TBlobClass blobClass,
                                       TConn conn = 0) const;

    virtual int Compare(const CSeqref& seqRef, EMatchLevel ml = eSeq) const;
    virtual const string print(void)    const;
    virtual const string printTSE(void) const;

    int& Gi()     { return m_Gi; };
    int& Sat()    { return m_Sat; };
    int& SatKey() { return m_SatKey; };
  
    int Gi()     const { return m_Gi; };
    int Sat()    const { return m_Sat; };
    int SatKey() const { return m_SatKey; };

private:
    friend class CId1BlobSource;

    CConn_ServiceStream* x_GetService(TConn conn) const;
    void x_Reconnect(TConn conn) const;

    CId1Reader&    m_Reader;
    int m_Gi;
    int m_Sat;
    int m_SatKey;
};


class NCBI_XOBJMGR_EXPORT CId1BlobSource : public CBlobSource
{
public:
    CId1BlobSource(const CId1Seqref& seqId, TConn conn);
    ~CId1BlobSource(void);

    virtual bool HaveMoreBlobs(void);
    virtual CBlob* RetrieveBlob(void);

private:
    friend class CId1Blob;

    CConn_ServiceStream* x_GetService(void) const;
    void x_Reconnect(void);

    const CId1Seqref&    m_Seqref;
    TConn                m_Conn;
    int                  m_Count;
};


class NCBI_XOBJMGR_EXPORT CId1Blob : public CBlob
{
public:
    CId1Blob(CId1BlobSource& source, bool is_snp);
    ~CId1Blob(void);

    void ReadSeq_entry(void);

private:
    CId1BlobSource&  m_Source;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.6  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.5  2003/07/24 19:28:08  vasilche
* Implemented SNP split for ID1 loader.
*
* Revision 1.4  2003/06/02 16:01:37  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.3  2003/05/29 17:57:32  lavr
* Remove unnecessary headers
*
* Revision 1.2  2003/04/15 15:30:14  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.1  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
*/

#endif // SEQREF_ID1__HPP_INCLUDED
