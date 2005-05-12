#ifndef READER__HPP_INCLUDED
#define READER__HPP_INCLUDED
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
*  Author:  Eugene Vasilchenko
*
*  File Description: Base data reader interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CReaderRequestResult;
class CReadDispatcher;
class CBlob_id;
class CSeq_id_Handle;
class CLoadLockSeq_ids;
class CLoadLockBlob_ids;
class CLoadLockBlob;

class NCBI_XREADER_EXPORT CReader : public CObject
{
public:
    CReader(void);
    virtual ~CReader(void);

    typedef unsigned TConn;
    typedef CBlob_id TBlobId;
    typedef int TBlobState;
    typedef int TBlobVersion;
    typedef int TBlobSplitVersion;
    typedef int TChunkId;
    typedef int TContentsMask;

    /// All LoadXxx() methods should return false if
    /// there is no requested data in the reader.
    /// This will notify dispatcher that there is no sense to retry.
    virtual bool LoadStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id) = 0;
    virtual bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id) = 0;
    virtual bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id) = 0;
    virtual bool LoadSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id);
    virtual bool LoadBlobVersion(CReaderRequestResult& result,
                                 const TBlobId& blob_id) = 0;
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           const string& seq_id,
                           TContentsMask mask);
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id,
                           TContentsMask mask);
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids blobs,
                           TContentsMask mask);
    virtual bool LoadBlob(CReaderRequestResult& result,
                          const CBlob_id& blob_id) = 0;
    virtual bool LoadChunk(CReaderRequestResult& result,
                           const TBlobId& blob_id, TChunkId chunk_id);

    void SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                 const string& seq_id) const;
    void SetAndSaveStringGi(CReaderRequestResult& result,
                            const string& seq_id,
                            int gi) const;
    void SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id) const;
    void SetAndSaveSeq_idGi(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            int gi) const;
    void SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id) const;
    void SetAndSaveBlobVersion(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TBlobVersion version) const;
    void SetAndSaveNoBlob(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id);

    void SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                 const string& seq_id,
                                 CLoadLockSeq_ids& seq_ids) const;
    void SetAndSaveStringGi(CReaderRequestResult& result,
                            const string& seq_id,
                            CLoadLockSeq_ids& seq_ids,
                            int gi) const;
    void SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& seq_ids) const;
    void SetAndSaveSeq_idGi(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            CLoadLockSeq_ids& seq_ids,
                            int gi) const;
    void SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id,
                                  CLoadLockBlob_ids& blob_ids) const;
    void SetAndSaveBlobVersion(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               CLoadLockBlob& blob,
                               TBlobVersion version) const;
    void SetAndSaveNoBlob(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CLoadLockBlob& blob);

    void SetInitialConnections(int max);
    int SetMaximumConnections(int max);
    int GetMaximumConnections(void) const;
    virtual int GetMaximumConnectionsLimit(void) const;

    class CConn
    {
    public:
        CConn(CReader* reader);
        ~CConn(void);

        void Release(void);

        operator TConn(void) const;

    private:
        CReader* m_Reader;
        TConn m_Conn;

    private:
        CConn(const CConn&);
        void operator=(CConn&);
    };

    // returns the time in seconds when already retrived data
    // could become obsolete by fresher version
    // -1 - never
    virtual int GetConst(const string& const_name) const;

    virtual int GetRetryCount(void) const;
    virtual bool MayBeSkippedOnErrors(void) const;

    // CReadDispatcher can set m_Dispatcher
    friend class CReadDispatcher;

    static int ReadInt(CNcbiIstream& stream);

    virtual bool HasCache(void) const { return false; }

protected:
    CReadDispatcher* m_Dispatcher;

    // allocate connection slot with key 'conn'
    virtual void x_AddConnectionSlot(TConn conn) = 0;
    // disconnect and remove connection slot with key 'conn'
    virtual void x_RemoveConnectionSlot(TConn conn) = 0;
    // disconnect at connection slot with key 'conn'
    virtual void x_DisconnectAtSlot(TConn conn);
    // force connection at connection slot with key 'conn'
    virtual void x_ConnectAtSlot(TConn conn) = 0;

private:
    friend class CConn;

    TConn x_AllocConnection(bool oldest = false);
    void x_ReleaseConnection(TConn conn, bool oldest = false);
    void x_AbortConnection(TConn conn);

    void x_AddConnection(void);
    void x_RemoveConnection(void);

    typedef list<TConn> TFreeConnections;
    CAtomicCounter   m_NumConnections;
    CAtomicCounter   m_NextConnection;
    TFreeConnections m_FreeConnections;
    CMutex           m_ConnectionsMutex;
    CSemaphore       m_NumFreeConnections;
};


////////////////////////////////////////////////////////////////////////////
// CConn
////////////////////////////////////////////////////////////////////////////


inline
CReader::CConn::CConn(CReader* reader)
    : m_Reader(reader)
{
    if ( reader ) {
        m_Conn = reader->x_AllocConnection();
    }
}


inline
CReader::CConn::~CConn(void)
{
    if ( m_Reader ) {
        m_Reader->x_AbortConnection(m_Conn);
    }
}


inline
void CReader::CConn::Release(void)
{
    if ( m_Reader ) {
        m_Reader->x_ReleaseConnection(m_Conn);
        m_Reader = 0;
    }
}


inline
CReader::CConn::operator CReader::TConn(void) const
{
    _ASSERT(m_Reader);
    return m_Conn;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER__HPP_INCLUDED
