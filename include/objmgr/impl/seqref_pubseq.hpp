#ifndef SEQREF_PUBSEQ__HPP_INCLUDED
#define SEQREF_PUBSEQ__HPP_INCLUDED

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
*  File Description: support classes for data reader from Pubseq_OS
*
*/

#include <corelib/ncbiobj.hpp>

#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>

#include <util/bytesrc.hpp>

#include <objmgr/reader.hpp>

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>

#include <memory>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPubseqReader;

class NCBI_XOBJMGR_EXPORT CPubseqSeqref : public CSeqref
{
public:
    CPubseqSeqref(CPubseqReader& reader, int gi, int sat, int satkey);
    ~CPubseqSeqref(void);

    virtual CBlobSource* GetBlobSource(TPos start, TPos stop,
                                       TBlobClass blobClass,
                                       TConn conn = 0) const;

    int Gi()     const { return m_Gi; }
    int Sat()    const { return m_Sat; }
    int SatKey() const { return m_SatKey; }

    virtual const string print(void) const;
    virtual const string printTSE(void) const;

    virtual int Compare(const CSeqref& seqRef, EMatchLevel ml = eSeq) const;

private:
    friend class CPubseqBlobSource;

    CDB_Connection* x_GetConn(TConn conn) const;
    void x_Reconnect(TConn conn) const;

    CPubseqReader& m_Reader;
    int m_Gi;
    int m_Sat;
    int m_SatKey;
};


class NCBI_XOBJMGR_EXPORT CPubseqBlobSource : public CBlobSource
{
public:
    CPubseqBlobSource(const CPubseqSeqref& seqId, TConn conn);
    ~CPubseqBlobSource(void);

    virtual bool HaveMoreBlobs(void);
    virtual CBlob* RetrieveBlob(void);

private:
    friend class CPubseqBlob;

    void x_GetNextBlob(void);
    void x_Reconnect(void) const;

    const CPubseqSeqref& m_Seqref;
    TConn                m_Conn;
    auto_ptr<CDB_RPCCmd> m_Cmd;
    auto_ptr<CDB_Result> m_Result;
    CRef<CBlob>          m_Blob;
};


class NCBI_XOBJMGR_EXPORT CPubseqBlob : public CBlob
{
public:
    CPubseqBlob(CPubseqBlobSource& source,
                int cls, const string& descr,
                bool snp);
    ~CPubseqBlob(void);

    CSeq_entry *Seq_entry();

private:
    CPubseqBlobSource& m_Source;
    CRef<CSeq_entry>   m_Seq_entry;
    bool               m_SNP;
};


class NCBI_XOBJMGR_EXPORT CResultBtSrc : public CByteSource
{
public:
    CResultBtSrc(CDB_Result* result);
    ~CResultBtSrc();

    virtual CRef<CByteSourceReader> Open(void);

private:
    CDB_Result* m_Result;
};


class NCBI_XOBJMGR_EXPORT CResultBtSrcRdr : public CByteSourceReader
{
public:
    CResultBtSrcRdr(CDB_Result* result);
    ~CResultBtSrcRdr();

    virtual size_t Read(char* buffer, size_t bufferLength);

private:
    CDB_Result* m_Result;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.6  2003/07/24 19:28:08  vasilche
* Implemented SNP split for ID1 loader.
*
* Revision 1.5  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
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
* Revision 1.3  2003/04/15 16:31:37  dicuccio
* Placed CResultBtSrcRdr in XOBJMGR Win32 export namespace (was
* erroneously in XUTIL)
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

#endif // SEQREF_PUBSEQ__HPP_INCLUDED
