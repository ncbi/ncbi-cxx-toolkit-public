#ifndef BDB_BLOB_MERGE_HPP_
#define BDB_BLOB_MERGE_HPP_

/* $Id$
 * ===========================================================================
 *
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
 * Author:  Anatoliy Kuznetsov, Mike DiCuccio
 *   
 * File Description: BDB compatible volume merge components
 *
 */

#include <corelib/ncbimtx.hpp>
#include <algo/volume_merge/volume_merge.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>
#include <util/thread_nonstop.hpp>



BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */


/// Generic iterator to traverse any CBDB_BLobFile for volume merge
/// BF - any CBDB_BLobFile derived class
///
template<class BF>
class CBDB_MergeBlobWalker : public IMergeVolumeWalker
{
public:
    typedef  BF  TBlobFile;
    
public:
    CBDB_MergeBlobWalker(TBlobFile*   blob_file,
                         EOwnership   own=eTakeOwnership,
                         size_t       fetch_buffer_size = 10 * 1024 * 1024);
    virtual ~CBDB_MergeBlobWalker();

    virtual IAsyncInterface* QueryIAsync() { return 0; }
    virtual bool IsEof() const { return m_Eof; }
    virtual bool IsGood() const { return true; }
    virtual void FetchFirst();
    virtual void Fetch();
    virtual const unsigned char* GetKeyPtr() const { return m_KeyPtr; }
    virtual Uint4 GetUint4Key() const { return m_Key; }
    virtual const unsigned char* GetBufferPtr(size_t* buf_size) const
    {
        if (buf_size) {
            *buf_size = m_DataLen;
        }
        return (unsigned char*)m_Data;
    }
    virtual void Close() { m_Cursor.reset(0); }
    virtual void SetRecordMoved() {}

protected:
    TBlobFile*                  m_BlobFile;
    auto_ptr<TBlobFile>         m_BlobFile_Owned;
    auto_ptr<CBDB_FileCursor>   m_Cursor;
    size_t                      m_FetchBufferSize;
    bool                        m_Eof;
    const void*                 m_Data;
    size_t                      m_DataLen;
    Uint4                       m_Key;
    const unsigned char*        m_KeyPtr;
};


/// Generic iterator to traverse any CBDB_BLobFile for volume merge
/// BF - any CBDB_BLobFile derived class
/// This implementation supports asyncronous processing.
template<class BF>
class CBDB_MergeBlobWalkerAsync : public IMergeVolumeWalker
{
public:
    typedef  BF  TBlobFile;
    
public:
    CBDB_MergeBlobWalkerAsync(
                    TBlobFile*   blob_file,
                    EOwnership   own=eTakeOwnership,
                    size_t       fetch_buffer_size = 10 * 1024 * 1024);
    virtual ~CBDB_MergeBlobWalkerAsync();

    virtual IAsyncInterface* QueryIAsync()  { return &m_AsyncImpl; }
    virtual bool IsEof() const;
    virtual bool IsGood() const;
    virtual void FetchFirst();
    virtual void Fetch();
    virtual const unsigned char* GetKeyPtr() const;
    virtual Uint4 GetUint4Key() const;
    virtual const unsigned char* GetBufferPtr(size_t* buf_size) const;
    virtual void Close();
    virtual void SetRecordMoved() {}

protected:
    typedef CBDB_MergeBlobWalkerAsync<BF> TMainClass;

    /// IAsync implementation
    /// @internal
    class CAsync : public IAsyncInterface
    {
    public:
        CAsync(TMainClass& impl) : m_Impl (impl) {}
        virtual EStatus GetStatus() const
        {
            CFastMutex::TReadLockGuard guard(m_Impl.m_Lock);
            if (!m_Impl.m_Good)
                return eFailed;
            if (m_Impl.m_Data)
                return eReady;
            return eNotReady;
        }
        virtual EStatus WaitReady() const
        {
            while (1) {
                EStatus st = GetStatus();
                switch (st)
                {
                case eReady:    return st;
                case eNotReady: break;
                default: return st;
                }
            }
        }
    private:
        TMainClass& m_Impl;
    };
    /// Background thread class (executes async requests)
    /// @internal
    class CJobThread : public CThreadNonStop
    {
    public:
        CJobThread(TMainClass& impl) 
            : CThreadNonStop(1000), m_Impl(impl) 
        {}
    protected:
        /// Do job delegated processing to the main class
        virtual void DoJob(void) { m_Impl.DoFetch(); }
    protected:
        TMainClass&  m_Impl; 
    };
    friend class TMainClass::CAsync;
    friend class TMainClass::CJobThread;
protected:
    void DoFetch();
protected:
    mutable CFastMutex          m_Lock;
    TBlobFile*                  m_BlobFile;
    auto_ptr<TBlobFile>         m_BlobFile_Owned;
    auto_ptr<CBDB_FileCursor>   m_Cursor;
    size_t                      m_FetchBufferSize;
    bool                        m_Eof;
    bool                        m_Good;
    const void*                 m_Data;
    size_t                      m_DataLen;
    Uint4                       m_Key;
    const unsigned char*        m_KeyPtr;
    CRef<CJobThread>            m_JobThread;
    CAsync                      m_AsyncImpl;
};



///    Merge store saves result to BLOB store
///
template<class BStore>
class CBDB_MergeStore : public IMergeStore
{
public:
    typedef BStore  TBlobStore;
public:
    CBDB_MergeStore(TBlobStore*   blob_store, 
                    EOwnership    own=eTakeOwnership);
    virtual ~CBDB_MergeStore();

    virtual IAsyncInterface* QueryIAsync() { return 0; }
    virtual bool IsGood() const { return true; }
    virtual void Store(Uint4 blob_id, CMergeVolumes::TRawBuffer* buffer);
    virtual void Close() {}
    virtual CMergeVolumes::TRawBuffer* ReadBlob(Uint4 blob_id);
private:
    TBlobStore*  m_BlobStore;
    EOwnership   m_OwnBlobStore;
};


///  Merge store saves result to BLOB store.
///  This is asyncronous implementation, it starts a background thread
///  to process requests.
///
template<class BStore>
class CBDB_MergeStoreAsync : public IMergeStore
{
public:
    typedef BStore  TBlobStore;
public:
    CBDB_MergeStoreAsync(TBlobStore*   blob_store, 
                         EOwnership    own=eTakeOwnership);
    virtual ~CBDB_MergeStoreAsync();

    virtual IAsyncInterface* QueryIAsync() { return &m_AsyncImpl; }
    virtual bool IsGood() const;
    virtual void Store(Uint4 blob_id, CMergeVolumes::TRawBuffer* buffer);
    virtual void Close();
    virtual CMergeVolumes::TRawBuffer* ReadBlob(Uint4 blob_id);
protected:
    void DoStore();
protected:
    typedef CBDB_MergeStoreAsync<BStore> TMainClass;

    /// IAsync implementation
    /// @internal
    class CAsync : public IAsyncInterface
    {
    public:
        CAsync(TMainClass& impl) : m_Impl (impl) {}
        virtual EStatus GetStatus() const
        {
            CFastMutex::TReadLockGuard guard(m_Impl.m_Lock);
            if (!m_Impl.m_Good)
                return eFailed;
            if (m_Impl.m_Request_Buffer == 0)
                return eReady;
            return eNotReady;
        }
        virtual EStatus WaitReady() const
        {
            while (1) {
                EStatus st = GetStatus();
                switch (st)
                {
                case eReady:    return st;
                case eNotReady: break;
                default: return st;
                }
            }
        }
    private:
        TMainClass& m_Impl;
    };
    /// Background thread class (executes async requests)
    /// @internal
    class CJobThread : public CThreadNonStop
    {
    public:
        CJobThread(TMainClass& impl) 
            : CThreadNonStop(1000), m_Impl(impl) 
        {}
    protected:
        /// Do job delegated processing to the main class
        virtual void DoJob(void) { m_Impl.DoStore(); }
    protected:
        TMainClass&  m_Impl; 
    };
    friend class TMainClass::CAsync;
    friend class TMainClass::CJobThread;
protected:
    mutable CFastMutex         m_Lock;
    bool                       m_Good;
    TBlobStore*                m_BlobStore;
    EOwnership                 m_OwnBlobStore;
    CRef<CJobThread>           m_JobThread;
    Uint4                      m_Request_BlobId;
    CMergeVolumes::TRawBuffer* m_Request_Buffer;
    CAsync                     m_AsyncImpl;
};



 /* @} */

 /////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

template<class BF>
CBDB_MergeBlobWalker<BF>::CBDB_MergeBlobWalker(TBlobFile*   blob_file,
                                               EOwnership   own,
                                               size_t       fetch_buffer_size)
 : m_BlobFile(blob_file),
   m_FetchBufferSize(fetch_buffer_size),
   m_Eof(false)
{
    if (own == eTakeOwnership) {
        m_BlobFile_Owned.reset(m_BlobFile);
    }
}

template<class BF>
CBDB_MergeBlobWalker<BF>::~CBDB_MergeBlobWalker()
{
}

template<class BF>
void CBDB_MergeBlobWalker<BF>::FetchFirst()
{
    m_Cursor.reset(new CBDB_FileCursor(*m_BlobFile));
    m_Cursor->SetCondition(CBDB_FileCursor::eGE);
    if (m_FetchBufferSize) {
        m_Cursor->InitMultiFetch(m_FetchBufferSize);
    }
    this->Fetch();
}

template<class BF>
void CBDB_MergeBlobWalker<BF>::Fetch()
{
    EBDB_ErrCode err = m_Cursor->Fetch();
    switch (err) 
    {
    case eBDB_Ok:
        m_Data     = m_Cursor->GetLastMultiFetchData();
        m_DataLen  = m_Cursor->GetLastMultiFetchDataLen();
        _ASSERT(m_Data  &&  m_DataLen);
        {
        const CBDB_BufferManager* key_bm = m_BlobFile->GetKeyBuffer();
        const CBDB_Field& fld = key_bm->GetField(0);
        const void* ptr = fld.GetBuffer();
        m_KeyPtr =(const unsigned char*) ptr;
        m_Key = fld.GetUint();
        }
        break;
    case eBDB_NotFound:
        m_Data = 0;
        m_DataLen = 0;
        m_Eof = true;
        break;
    default:
        _ASSERT(0);
    };
}

/////////////////////////////////////////////////////////////////////////////

template<class BStore>
CBDB_MergeStore<BStore>::CBDB_MergeStore(TBlobStore*  blob_store, 
                                 EOwnership   own)
  : m_BlobStore(blob_store),
    m_OwnBlobStore(own)
{
}

template<class BStore>
CBDB_MergeStore<BStore>::~CBDB_MergeStore()
{
    if (m_OwnBlobStore == eTakeOwnership) {
        delete m_BlobStore;
    }
}

template<class BStore>
void CBDB_MergeStore<BStore>::Store(Uint4                      blob_id, 
                                    CMergeVolumes::TRawBuffer* buffer)
{
    CMergeVolumes::TBufPoolGuard guard(*(this->m_BufResourcePool), buffer);
    EBDB_ErrCode err = m_BlobStore->Insert(
                                     blob_id, 
                                     &((*buffer)[0]), buffer->size());
}

template<class BStore>
CMergeVolumes::TRawBuffer* CBDB_MergeStore<BStore>::ReadBlob(Uint4 blob_id)
{
    CMergeVolumes::TRawBuffer* buf = m_BufResourcePool->Get();
    CMergeVolumes::TBufPoolGuard buf_guard(*m_BufResourcePool, buf);
    EBDB_ErrCode ret = m_BlobStore->ReadRealloc(blob_id, buf);
    if (ret == eBDB_Ok) {
        return buf_guard.Release();
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////


template<class BStore>
CBDB_MergeStoreAsync<BStore>::CBDB_MergeStoreAsync(TBlobStore*  blob_store, 
                                                   EOwnership   own)
  : m_Good(true),
    m_BlobStore(blob_store),
    m_OwnBlobStore(own),
    m_Request_BlobId(0),
    m_Request_Buffer(0),
    m_AsyncImpl(*this)
{
    m_JobThread.Reset(new CJobThread(*this));
    m_JobThread->Run();
}

template<class BStore>
CBDB_MergeStoreAsync<BStore>::~CBDB_MergeStoreAsync()
{
    if (m_OwnBlobStore == eTakeOwnership) {
        delete m_BlobStore;
    }
}

template<class BStore>
void CBDB_MergeStoreAsync<BStore>::Close()
{
    while (1) {
        {{
            CFastMutex::TWriteLockGuard guard(m_Lock);
            if (m_Request_Buffer == 0) { // make sure the last request is done
                m_JobThread->RequestStop();
                m_JobThread->Join();
                break;
            }
        }}
    }
}

template<class BStore>
bool CBDB_MergeStoreAsync<BStore>::IsGood() const
{
    CFastMutex::TReadLockGuard guard(m_Lock);
    return m_Good;
}

template<class BStore>
void CBDB_MergeStoreAsync<BStore>::Store(Uint4                      blob_id, 
                                         CMergeVolumes::TRawBuffer* buffer)
{
    CFastMutex::TWriteLockGuard guard(m_Lock);
    if (m_Request_Buffer) {
        NCBI_THROW(CMerge_Exception, eOperationNotReady, "Store not ready.");
    }
    m_Request_BlobId = blob_id;
    m_Request_Buffer = buffer;
    m_JobThread->RequestDoJob();
}

template<class BStore>
void CBDB_MergeStoreAsync<BStore>::DoStore()
{
    CMergeVolumes::TBufPoolGuard 
        guard(*(this->m_BufResourcePool), m_Request_Buffer);

    CFastMutex::TWriteLockGuard lock(m_Lock);
    if (!m_Request_Buffer) {
        return;
    }
    try {
        EBDB_ErrCode err = m_BlobStore->Insert(
                                            m_Request_BlobId, 
                                            &((*m_Request_Buffer)[0]), 
                                            m_Request_Buffer->size());
        if (err != eBDB_Ok) {
            m_Good = false;
        }
        m_Request_Buffer = 0;
    } catch (...) {
        m_Good = false;
        throw;
    }
}


template<class BStore>
CMergeVolumes::TRawBuffer* CBDB_MergeStoreAsync<BStore>::ReadBlob(Uint4 blob_id)
{
    CMergeVolumes::TRawBuffer* buf = m_BufResourcePool->Get();
    CMergeVolumes::TBufPoolGuard buf_guard(*m_BufResourcePool, buf);
    {{
    CFastMutex::TWriteLockGuard guard(m_Lock);
    EBDB_ErrCode ret = m_BlobStore->ReadRealloc(blob_id, *buf);
    if (ret == eBDB_Ok) {
        return buf_guard.Release();
    }
    }}
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

template<class BF>
CBDB_MergeBlobWalkerAsync<BF>::CBDB_MergeBlobWalkerAsync(
                                               TBlobFile*   blob_file,
                                               EOwnership   own,
                                               size_t       fetch_buffer_size)
 : m_BlobFile(blob_file),
   m_FetchBufferSize(fetch_buffer_size),
   m_Eof(false),
   m_Good(true),
   m_AsyncImpl(*this)
{
    if (own == eTakeOwnership) {
        m_BlobFile_Owned.reset(m_BlobFile);
    }
    m_JobThread.Reset(new CJobThread(*this));
    m_JobThread->Run();
}

template<class BF>
CBDB_MergeBlobWalkerAsync<BF>::~CBDB_MergeBlobWalkerAsync()
{
}

template<class BF>
bool CBDB_MergeBlobWalkerAsync<BF>::IsEof() const
{
    CFastMutex::TReadLockGuard lock(m_Lock);
    return m_Eof;
}

template<class BF>
bool CBDB_MergeBlobWalkerAsync<BF>::IsGood() const
{
    CFastMutex::TReadLockGuard lock(m_Lock);
    return m_Good;
}

template<class BF>
void CBDB_MergeBlobWalkerAsync<BF>::FetchFirst()
{
    CFastMutex::TWriteLockGuard lock(m_Lock);

    m_Cursor.reset(new CBDB_FileCursor(*m_BlobFile));
    m_Cursor->SetCondition(CBDB_FileCursor::eGE);
    if (m_FetchBufferSize) {
        m_Cursor->InitMultiFetch(m_FetchBufferSize, 
                                 CBDB_FileCursor::eFetchGetBufferEnds);
    }
    m_Key = 0;
    m_KeyPtr = 0;
    m_DataLen = 0;
    m_Data = 0;

    m_JobThread->RequestDoJob();
}

template<class BF>
void CBDB_MergeBlobWalkerAsync<BF>::Fetch()
{
    CFastMutex::TWriteLockGuard lock(m_Lock);

    // here we try to move to the next multifetch buffer record
    // without delegating to background thread
    // if Fetch returns error code, indicating we are at the end of
    // the multifetch buffer we delegate next fetch call to the 
    // background reader (next call will be slow read from the disk)
    //
    // so here we have hybrid mode, when fast fetch is done in the same call
    // and potentially slow one is in the background
    //
    EBDB_ErrCode err = m_Cursor->Fetch();
    switch (err) 
    {
    case eBDB_Ok:
        m_Data     = m_Cursor->GetLastMultiFetchData();
        m_DataLen  = m_Cursor->GetLastMultiFetchDataLen();
        {
        const CBDB_BufferManager* key_bm = m_BlobFile->GetKeyBuffer();
        const CBDB_Field& fld = key_bm->GetField(0);
        const void* ptr = fld.GetBuffer();
        m_KeyPtr =(const unsigned char*) ptr;
        m_Key = fld.GetUint();
        }
        break;
    case eBDB_NotFound:
        m_Data = 0;
        m_DataLen = 0;
        m_Eof = true;
        break;
    case eBDB_MultiRowEnd:
        m_Key = 0;
        m_KeyPtr = 0;
        m_DataLen = 0;
        m_Data = 0;

        m_JobThread->RequestDoJob();        
        break;
    default:
        _ASSERT(0);
    };

}

template<class BF>
void CBDB_MergeBlobWalkerAsync<BF>::DoFetch()
{
    CFastMutex::TWriteLockGuard lock(m_Lock);
    if (m_Data || !m_Cursor.get()) { // fetch was not requested...
        return;
    }
    try {
        bool re_try;
        do {
            re_try = false;
        
            EBDB_ErrCode err = m_Cursor->Fetch();
            switch (err) 
            {
            case eBDB_Ok:
                m_Data     = m_Cursor->GetLastMultiFetchData();
                m_DataLen  = m_Cursor->GetLastMultiFetchDataLen();
                {
                const CBDB_BufferManager* key_bm = m_BlobFile->GetKeyBuffer();
                const CBDB_Field& fld = key_bm->GetField(0);
                const void* ptr = fld.GetBuffer();
                m_KeyPtr =(const unsigned char*) ptr;
                m_Key = fld.GetUint();
                }
                break;
            case eBDB_NotFound:
                m_Data = 0;
                m_DataLen = 0;
                m_Eof = true;
                break;
            case eBDB_MultiRowEnd:
                re_try = true;
                break;
            default:
                _ASSERT(0);
            };
        } while (re_try);
        
    } catch (...) {
        m_Good = false;
        throw;
    }
}


template<class BF>
const unsigned char* CBDB_MergeBlobWalkerAsync<BF>::GetKeyPtr() const
{
    CFastMutex::TReadLockGuard lock(m_Lock);
    return m_KeyPtr;
}

template<class BF>
Uint4 CBDB_MergeBlobWalkerAsync<BF>::GetUint4Key() const
{
    CFastMutex::TReadLockGuard lock(m_Lock);
    return m_Key;
}

template<class BF>
const unsigned char* 
CBDB_MergeBlobWalkerAsync<BF>::GetBufferPtr(size_t* buf_size) const
{
    CFastMutex::TReadLockGuard lock(m_Lock);
    if (buf_size) {
        *buf_size = m_DataLen;
    }
    return (unsigned char*)m_Data;
}

template<class BF>
void CBDB_MergeBlobWalkerAsync<BF>::Close()
{
    m_JobThread->RequestStop();
    m_JobThread->Join();
    {{
    CFastMutex::TWriteLockGuard lock(m_Lock);
    m_Cursor.reset(0);
    }}
}

END_NCBI_SCOPE

#endif /* BDB_BLOB_MERGE_HPP_ */
