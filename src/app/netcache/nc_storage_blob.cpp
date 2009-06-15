/*  $Id$
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 *
 */

#include <ncbi_pch.hpp>

#include "nc_storage_blob.hpp"
#include "nc_storage.hpp"


BEGIN_NCBI_SCOPE


#define NCBI_USE_ERRCODE_X  NetCache_Storage


static const size_t kNCMaxBlobChunkSize = 2000000;


CNCBlob::CNCBlob(CNCBlobStorage* storage)
    : m_Storage(storage),
      m_Buffer(storage->GetBlobBufferPool())
{}

inline void
CNCBlob::x_FlushCurChunk(void)
{
    if (m_NextRowIdIt == m_AllRowIds.end()) {
        TNCChunkId chunk_id = m_Storage->CreateNewChunk(*m_BlobInfo,
                                                        *m_Buffer);
        m_AllRowIds.push_back(chunk_id);
        m_NextRowIdIt = m_AllRowIds.end();
    }
    else {
        m_Storage->WriteChunkValue(m_BlobInfo->part_id,
                                   *m_NextRowIdIt,
                                   *m_Buffer);
        ++m_NextRowIdIt;
    }
    m_Storage->GetStat()->AddChunkWritten(m_Buffer->size());
    m_Buffer->resize(0);
}

void
CNCBlob::Init(SNCBlobInfo* blob_info, bool can_write)
{
    _ASSERT(blob_info);
    _ASSERT(blob_info->blob_id);

    m_BlobInfo = blob_info;
    m_Position = 0;
    m_ChunkPos = 0;
    m_CanWrite = can_write;
    m_Finalized = false;

    m_Storage->GetChunkIds(*m_BlobInfo, &m_AllRowIds);
    m_NextRowIdIt = m_AllRowIds.begin();

    m_Buffer->resize(0);
    m_Buffer->reserve(kNCMaxBlobChunkSize);
}

void
CNCBlob::Reset(void)
{
    if (m_CanWrite) {
        if (m_Finalized) {
            m_Storage->GetStat()->AddBlobWritten(m_BlobInfo->size);
        }
        else {
            m_Storage->GetStat()->AddStoppedWrite();
        }
    }
    else {
        if (m_Position == m_BlobInfo->size) {
            m_Storage->GetStat()->AddBlobRead(m_BlobInfo->size);
        }
        else {
            m_Storage->GetStat()->AddStoppedRead();
        }
    }
}

size_t
CNCBlob::Read(void* buffer, size_t size)
{
    _ASSERT(!m_CanWrite);

    if (m_ChunkPos == m_Buffer->size()) {
        m_Buffer->resize(0);
        m_ChunkPos = 0;

        if (m_NextRowIdIt == m_AllRowIds.end()) {
            return 0;
        }

        m_Storage->ReadChunkValue(m_BlobInfo->part_id,
                                  *m_NextRowIdIt,
                                  m_Buffer);
        ++m_NextRowIdIt;
    }

    size = min(size, m_Buffer->size() - m_ChunkPos);
    memcpy(buffer, m_Buffer->data() + m_ChunkPos, size);
    m_Position += size;
    m_ChunkPos += size;
    return size;
}

void
CNCBlob::Write(const void* data, size_t size)
{
    _ASSERT(m_CanWrite  &&  !m_Finalized);

    while (size != 0) {
        size_t write_size = min(size, kNCMaxBlobChunkSize - m_Buffer->size());
        memcpy(m_Buffer->data() + m_Buffer->size(), data, write_size);
        m_Buffer->resize(m_Buffer->size() + write_size);

        if (m_Buffer->size() == kNCMaxBlobChunkSize) {
            x_FlushCurChunk();
        }

        data = static_cast<const char*>(data) + write_size;
        size -= write_size;
        m_Position += write_size;
        m_BlobInfo->size = max(m_BlobInfo->size, m_Position);
    }
}

void
CNCBlob::Finalize(void)
{
    _ASSERT(m_CanWrite  &&  !m_Finalized);

    x_FlushCurChunk();

    if (m_NextRowIdIt != m_AllRowIds.end()) {
        m_Storage->DeleteLastChunks(*m_BlobInfo, *m_NextRowIdIt);
        m_AllRowIds.erase(m_NextRowIdIt, m_AllRowIds.end());
        m_NextRowIdIt = m_AllRowIds.end();
    }

    m_BlobInfo->size = m_Position;
    m_Finalized = true;
}



void
CNCBlobLockHolder::x_CheckBlobInfoRead(void) const
{
    _ASSERT(IsLockAcquired());

    // ttl will be equal to 0 if blob exists only when info was not read yet
    if (m_BlobInfo.ttl == 0  &&  m_BlobExists) {
        if (!m_Storage->ReadBlobInfo(&m_BlobInfo)) {
            m_DeleteOnRelease = true;
            NCBI_THROW(CNCBlobStorageException, eMalformedDB,
                "Blob data in database is malformed. "
                "Blob will be deleted.");
        }
    }
}

inline void
CNCBlobLockHolder::x_OnLockValidated(void)
{
    m_LockValid = true;
    if (!m_BlobExists) {
        m_SaveInfoOnRelease = m_DeleteOnRelease = false;
    }

    m_Storage->GetStat()->AddLockAcquired(m_LockTimer.Elapsed());
    m_LockTimer.Start();
}

inline bool
CNCBlobLockHolder::x_ValidateLock(void)
{
    bool key_empty = m_BlobInfo.key.empty();
    m_BlobExists = m_Storage->ReadBlobKey(&m_BlobInfo);
    // If there's no need to create (i.e. key was empty)
    // then we're valid anyway
    if (key_empty  ||  m_BlobExists) {
        x_OnLockValidated();
    }

    return m_LockValid;
}

inline void
CNCBlobLockHolder::x_ReleaseLock(void)
{
    if (m_Blob) {
        m_Blob->Reset();
        m_Storage->GetBlobsPool().Return(m_Blob);
        m_Blob = NULL;
    }
    m_Storage->UnlockBlobId(m_BlobInfo.blob_id, m_RWHolder);
    m_LockValid = false;
}

void
CNCBlobLockHolder::AcquireLock(TNCDBPartId   part_id,
                               TNCBlobId     blob_id,
                               ENCBlobAccess access,
                               bool          change_access_time)
{
    _ASSERT(blob_id > 0);
    _ASSERT(part_id >= 0);
    _ASSERT(access != eCreate);

    m_BlobInfo.Clear();
    m_BlobInfo.part_id = part_id;
    m_BlobInfo.blob_id = blob_id;
    m_SaveInfoOnRelease = change_access_time;
    m_DeleteOnRelease = access != eRead;

    x_InitLock(access);
}

void
CNCBlobLockHolder::AcquireLock(const string& key,
                               const string& subkey,
                               int           version,
                               ENCBlobAccess access)
{
    _ASSERT(!key.empty());

    m_BlobInfo.Clear();
    m_BlobInfo.key = key;
    m_BlobInfo.subkey = subkey;
    m_BlobInfo.version = version;
    m_DeleteOnRelease = access != eRead;
    m_SaveInfoOnRelease = m_DeleteOnRelease
                          ||  m_Storage->IsChangeTimeOnRead();

    x_InitLock(access);
}

void
CNCBlobLockHolder::x_InitLock(ENCBlobAccess access)
{
    m_Blob = NULL;
    m_LockKnown = m_LockValid = m_BlobExists = false;

    m_Storage->GetStat()->AddLockRequest();
    m_LockTimer.Start();

    try {
        if (access == eCreate) {
            _ASSERT(m_BlobInfo.blob_id == 0);

            // Assume that in majority of cases "Create" means new blob, thus
            // optimize performance for this case.
            m_Storage->GetNextBlobCoords(&m_BlobInfo);
            m_RWHolder = m_Storage->LockBlobId(m_BlobInfo.blob_id, eCreate);
            _ASSERT(m_RWHolder->IsLockAcquired());

            x_RetakeCreateLock();
        }
        else if (m_BlobInfo.blob_id == 0
                 &&  ! m_Storage->ReadBlobId(&m_BlobInfo))
        {
            // There's no such blob
            m_LockKnown = true;
            x_OnLockValidated();
        }
        else {
            // Standard way of read/write locking
            x_LockAndValidate(access);
        }
    }
    catch (...) {
        x_ReleaseLock();
        throw;
    }
}

void
CNCBlobLockHolder::OnLockAcquired(CRWLockHolder* holder)
{
    {{
        CFastMutexGuard guard(m_LockAcqMutex);

        if (m_RWHolder != holder  ||  !m_RWHolder->IsLockAcquired()
            ||  m_LockKnown)
        {
            // Some other thread already messed up with this object
            return;
        }
        m_LockKnown = true;
    }}

    if (!x_ValidateLock()) {
        x_RetakeCreateLock();
    }
}

void
CNCBlobLockHolder::OnLockReleased(CRWLockHolder*)
{}

void
CNCBlobLockHolder::x_RetakeCreateLock(void)
{
    for (;;) {
        if (m_Storage->CreateBlob(&m_BlobInfo)) {
            m_LockKnown = m_BlobExists = true;
            m_BlobInfo.ttl = m_Storage->GetDefBlobTTL();
            x_OnLockValidated();
            return;
        }
        if (x_LockAndValidate(eCreate)) {
            return;
        }
    }
}

bool
CNCBlobLockHolder::x_LockAndValidate(ENCBlobAccess access)
{
    TRWLockHolderRef rw_holder(
                          m_Storage->LockBlobId(m_BlobInfo.blob_id, access));

    bool lock_known;
    {{
        CFastMutexGuard guard(m_LockAcqMutex);

        x_ReleaseLock();
        rw_holder->AddListener(this);
        m_RWHolder = rw_holder;
        lock_known = m_LockKnown = rw_holder->IsLockAcquired();
    }}

    if (lock_known) {
        return x_ValidateLock();
    }
    else {
        return true;
    }
}

void
CNCBlobLockHolder::ReleaseLock(void)
{
    if (IsLockAcquired()) {
        if (m_DeleteOnRelease  &&  (!m_Blob  ||  !m_Blob->IsFinalized())) {
            m_Storage->DeleteBlob(m_BlobInfo);
        }
        else if (m_SaveInfoOnRelease) {
            // Info should be read before this
            _ASSERT(m_BlobInfo.ttl != 0);
            m_Storage->WriteBlobInfo(m_BlobInfo);
        }

        m_Storage->GetStat()->AddLockHeldTime(m_LockTimer.Elapsed());
    }

    CFastMutexGuard guard(m_LockAcqMutex);

    x_ReleaseLock();
}

CNCBlob*
CNCBlobLockHolder::GetBlob(void) const
{
    _ASSERT(IsLockAcquired()  &&  IsBlobExists());

    CFastMutexGuard guard(m_LockAcqMutex);

    if (!m_Blob) {
        x_CheckBlobInfoRead();
        m_Blob = m_Storage->GetBlobsPool().Get();
        m_Blob->Init(&m_BlobInfo, x_IsBlobWritable());
    }
    return m_Blob;
}

void
CNCBlobLockHolder::SetBlobTTL(int ttl)
{
    _ASSERT(IsLockAcquired()  &&  x_IsBlobWritable());

    x_CheckBlobInfoRead();
    if (ttl == 0) {
        ttl = m_Storage->GetDefBlobTTL();
    }
    else {
        ttl = min(ttl, m_Storage->GetMaxBlobTTL());
    }
    m_BlobInfo.ttl = ttl;
}

void
CNCBlobLockHolder::DeleteBlob(void)
{
    _ASSERT(IsLockAcquired());

    if (IsBlobExists()) {
        _ASSERT(m_RWHolder.NotNull()  &&  x_IsBlobWritable());

        m_Storage->DeleteBlob(m_BlobInfo);
        m_Storage->GetStat()->AddBlobDelete();

        m_BlobExists = false;
        m_SaveInfoOnRelease = m_DeleteOnRelease = false;
    }
}

void
CNCBlobLockHolder::DeleteThis(void) const
{
    CNCBlobLockHolder* nc_this = const_cast<CNCBlobLockHolder*>(this);
    nc_this->ReleaseLock();
    CleanWeakRefs();
    m_Storage->ReturnLockHolder(nc_this);
}

CNCBlobLockHolder::~CNCBlobLockHolder(void)
{
    try {
        ReleaseLock();
    }
    catch (exception& ex) {
        ERR_POST_X(13, "Error destroying lock holder for blob "
                       << m_BlobInfo.blob_id << ": " << ex.what());
    }
}

END_NCBI_SCOPE
