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

#include <algo/volume_merge/volume_merge.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */

/**
    Generic iterator to traverse any CBDB_BLobFile for volume merge
    BF - any CBDB_BLobFile derived class
*/
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
        return m_Data;
    }
    virtual void Close() { m_Cursor.reset(0); }
    virtual void SetRecordMoved() {}

protected:
    TBlobFile*                  m_BlobFile;
    EOwnership                  m_OwnBlobFile;
    size_t                      m_FetchBufferSize;
    auto_ptr<CBDB_FileCursor>   m_Cursor;
    bool                        m_Eof;
    const void*                 m_Data;
    size_t                      m_DataLen;
    UInt4                       m_Key;
    const unsigned char*        m_KeyPtr;
};


/**
    Merge store saves result to BLOB store
*/
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
private:
    TBlobStore*  m_BlobStore;
    EOwnership   m_OwnBlobStore;
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
   m_OwnBlobFile(own),
   m_FetchBufferSize(fetch_buffer_size),
   m_Eof(false)
{
}

template<class BF>
CBDB_MergeBlobWalker<BF>::~CBDB_MergeBlobWalker()
{
    if (m_OwnBlobFile == eTakeOwnership) {
        delete m_BlobFile;
    }
}

template<class BF>
void CBDB_MergeBlobWalker<BF>::FetchFirst()
{
    m_Cursor.reset(new CBDB_FileCursor(*m_BlobFile));
    m_Cursor->SetCondition(CBDB_FileCursor::eGE);
    if (fetch_buffer_size) {
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
        m_Data     = cursor->GetLastMultiFetchData();
        m_DataLen = cursor->GetLastMultiFetchDataLen();
        {
        const CBDB_BufferManager* key_bm = m_BlobFile.GetKeyBuffer();
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
CBDB_MergeStore::CBDB_MergeStore(TBlobStore*  blob_store, 
                                 EOwnership   own)
  : m_BlobStore(blob_store),
    m_OwnBlobStore(own)
{
}

template<class BStore>
CBDB_MergeStore::~CBDB_MergeStore()
{
    if (m_OwnBlobStore == eTakeOwnership) {
        delete m_BlobStore;
    }
}

template<class BStore>
void CBDB_MergeStore<BStore>::Store(Uint4                      blob_id, 
                                    CMergeVolumes::TRawBuffer* buffer)
{
    TBufPoolGuard guard(*(this->m_BufResourcePool), buffer);
    EBDB_ErrCode err = m_BlobStore->Insert(
                                     blob_id, 
                                     &((*buffer)[0]), buffer->size());
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/11/20 08:24:03  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif /* BDB_BLOB_MERGE_HPP_ */
