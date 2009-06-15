#ifndef NETCACHE_NC_STORAGE_BLOB__HPP
#define NETCACHE_NC_STORAGE_BLOB__HPP
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
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/obj_pool.hpp>

#include "nc_db_info.hpp"


BEGIN_NCBI_SCOPE


class CNCBlobStorage;
class CNCDB_Stat;


/// Object for reading or writing to NetCache blob.
/// Object supports automatic division of blob into chunks as necessary.
/// Object does only incremental reading/writing from blob beginning till end.
/// After object creation only one type of operations is possible - either
/// reading or writing depending on constructor's parameter. After something
/// is written to the blob it should be finalized by call to Finalize()
/// otherwise object assumes that something have gone wrong, this blob is
/// not needed anymore and can be deleted completely.
class CNCBlob
{
public:
    /// Read data from blob into buffer
    ///
    /// @param buffer
    ///   Pointer to buffer to write data from blob to
    /// @param size
    ///   Size of buffer available for writing
    /// @return
    ///   Number of bytes read from blob
    size_t Read (void*       buffer, size_t size);
    /// Write data to blob
    ///
    /// @param data
    ///   Pointer to data to write
    /// @param size
    ///   Number of bytes to write
    void   Write(const void* data,   size_t size);
    /// Finalize all writings to blob
    void   Finalize(void);

    /// Get current total size of the blob
    size_t GetSize(void) const;
    /// Get current position of reading/writing in the blob
    size_t GetPosition(void) const;
    /// Check if this blob was finalized after writing
    bool   IsFinalized(void) const;

public:
    CNCBlob(CNCBlobStorage* storage);

    /// Constructor for use in CNCBlobRWLock only
    ///
    /// @param conn
    ///   Connection to do all database operations
    /// @param blob_id
    ///   Id of the blob to read/write
    /// @param blob_size
    ///   Size of the blob (read from meta-information about blob)
    /// @param can_write
    ///   TRUE - object can only write to the blob, FALSE - object can only
    ///   read from the blob
    /// @param stat
    ///   Object gathering NetCache storage statistics
    void Init(SNCBlobInfo*    blob_info,
              bool            can_write);
    void Reset(void);

private:
    CNCBlob(const CNCBlob&);
    CNCBlob& operator=(const CNCBlob&);

    /// Write current chunk data to database
    void x_FlushCurChunk(void);


    /// Connection which this object will do all database operations in
    CNCBlobStorage*       m_Storage;
    /// Id of blob this object reads/writes
    SNCBlobInfo*          m_BlobInfo;
    TNCBlobBufferGuard    m_Buffer;
    /// Current position of reading/writing inside blob
    size_t                m_Position;
    /// Current position of reading/writing inside blob chunk
    size_t                m_ChunkPos;
    /// List of ids of all chunks blob consists of
    TNCIdsList            m_AllRowIds;
    /// "Pointer" to chunk id that will be read/written next
    TNCIdsList::iterator  m_NextRowIdIt;
    /// Flag if object can just write or just read
    bool                  m_CanWrite;
    /// Flag if Finalize() method was called
    bool                  m_Finalized;
};

typedef CObjFactory_NewParam<CNCBlob, CNCBlobStorage*>  TNCBlobsFactory;
typedef CObjPool<CNCBlob, TNCBlobsFactory>              TNCBlobsPool;


/// Type of access to NetCache blob
enum ENCBlobAccess {
    eRead,    ///< Read-only access
    eWrite,   ///< Write-only access to existing blob
    eCreate   ///< Write-only access with creation of blob if it doesn't exist
};


/// Object holding lock of NetCache blob.
/// Object takes care of blob creation when necessary. When it is destroyed
/// lock for the blob is released. Lock can also be released by explicit call
/// of ReleaseLock() method.
class CNCBlobLockHolder : public CObjectEx, public IRWLockHolder_Listener
{
public:
    virtual ~CNCBlobLockHolder(void);

    /// Check if blob lock was already acquired
    bool IsLockAcquired(void) const;
    /// Check if blob exists.
    /// Method can be called only after lock is acquired.
    bool IsBlobExists(void) const;
    /// Get key of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobKey       (void) const;
    /// Get subkey of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobSubkey    (void) const;
    /// Get version of the blob.
    /// Method can be called only after lock is acquired.
    int           GetBlobVersion   (void) const;
    size_t        GetBlobSize      (void) const;
    /// Get owner of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobOwner     (void) const;
    /// Get blob's timeout after last access before it will be deleted.
    /// Method can be called only after lock is acquired.
    int           GetBlobTTL       (void) const;
    /// Get last acess time to the blob.
    /// Method can be called only after lock is acquired.
    int           GetBlobAccessTime(void) const;
    /// Check if blob is already expired but not yet deleted by GC.
    /// Method can be called only after lock is acquired.
    bool          IsBlobExpired    (void) const;

    /// Get object to read/write to the blob.
    /// Method can be called only after lock is acquired.
    CNCBlob*      GetBlob          (void) const;

    /// Set blob's timeout after last access before it will be deleted.
    /// Method can be called only after lock is acquired.
    void SetBlobTTL(int ttl);
    /// Set owner of the blob.
    /// Method can be called only after lock is acquired.
    void SetBlobOwner(const string& owner);

    /// Delete the blob.
    /// Method can be called only after lock is acquired.
    void DeleteBlob(void);

    /// Release blob lock
    void ReleaseLock(void);

public:
    CNCBlobLockHolder(CNCBlobStorage* storage);

    CNCBlobStorage* GetStorage(void);

    /// Acquire access lock for the blob identified by key-subkey-version
    /// Constructor is for use internally in CNCBlobStorage only
    void AcquireLock(const string&   key,
                     const string&   subkey,
                     int             version,
                     ENCBlobAccess   access);
    /// Acquire access lock for the blob identified by blob id
    /// Constructor is for use internally in CNCBlobStorage only
    void AcquireLock(TNCDBPartId     part_id,
                     TNCBlobId       blob_id,
                     ENCBlobAccess   access,
                     bool            change_access_time);

private:
    CNCBlobLockHolder(const CNCBlobLockHolder&);
    CNCBlobLockHolder& operator= (const CNCBlobLockHolder&);

    /// Implementation of IRWLockHolder_Listener - get notified when lock
    /// is acquired
    virtual void OnLockAcquired(CRWLockHolder* holder);
    /// Implementation of IRWLockHolder_Listener - get notified when lock
    /// is released
    virtual void OnLockReleased(CRWLockHolder* holder);
    ///
    virtual void DeleteThis(void) const;

    /// Initialize object's variables and acquire lock.
    /// Called only from constructors.
    void x_InitLock(ENCBlobAccess access);
    /// Utility procedure that should be executed after lock is considered
    /// valid (can happen in several places).
    void x_OnLockValidated(void);
    /// Validate acquired lock
    ///
    /// @return
    ///   TRUE if lock is valid, i.e. if access mode is eCreate then blob
    ///   record exists in database (created if needed), for modes eRead and
    ///   eWrite no validation is necessary - just check and remember if blob
    ///   exists. FALSE if lock is invalid, i.e. mentioned above requirement
    ///   is not met
    bool x_ValidateLock(void);
    /// Lock blob (with blob id m_BlobId) and validate it
    ///
    /// @return
    ///   TRUE if method is successful and there's no need to call it once
    ///   more, FALSE otherwise. Basically that means that if lock was
    ///   acquired during the call then return value has the same meaning as
    ///   in x_ValidateLock(), if lock was not acquired during the method
    ///   execution then all other necessary stuff will be executed in
    ///   OnLockAcquired().
    ///
    /// @sa x_ValidateLock, OnLockAcquired
    bool x_LockAndValidate(ENCBlobAccess access);
    /// Internal releasing of the lock
    /// Only pure releasing without obtaining mutex and statistics
    /// registration, thus it should execute from constructor or under
    /// m_LockAcqMutex.
    void x_ReleaseLock(void);
    /// Try to create blob record in database (because it was already checked
    /// that record doesn't exist) or acquire lock for new record if it was
    /// created by another thread. Method is called only if access mode is
    /// eCreate.
    void x_RetakeCreateLock(void);
    /// Check if one can write to the blob, i.e. access mode is
    /// eWrite or eCreate
    bool x_IsBlobWritable(void) const;
    void x_CheckBlobInfoRead(void) const;


    /// NetCache storage created the holder
    CNCBlobStorage*          m_Storage;
    /// Full information about locked blob if it exists
    mutable SNCBlobInfo      m_BlobInfo;
    bool                     m_SaveInfoOnRelease;
    mutable bool             m_DeleteOnRelease;
    /// Object used to read/write to the blob
    mutable CNCBlob*         m_Blob;
    /// Holder of blob id lock
    TRWLockHolderRef         m_RWHolder;
    /// Timer to calculate time while lock was waited for and while lock
    /// persisted before releasing
    CStopWatch               m_LockTimer;
    /// Flag if the fact that lock is acquired is already known by the holder
    bool                     m_LockKnown;
    /// Flag if lock acquired by id lock holder is valid and can be presented
    /// to outside user.
    bool                     m_LockValid;
    /// Remembered flag if blob exists in database
    bool                     m_BlobExists;
    /// Mutex used for critical operations during acquiring of the lock
    mutable CFastMutex       m_LockAcqMutex;
};

typedef CObjFactory_NewParam<CNCBlobLockHolder,
                             CNCBlobStorage*>            TNCBlobLockFactory;
typedef CObjPool<CNCBlobLockHolder, TNCBlobLockFactory>  TNCBlobLockObjPool;
/// Type that should be always used to store pointers to CNCBlobLockHolder
typedef CRef<CNCBlobLockHolder> TNCBlobLockHolderRef;



inline
CNCBlobLockHolder::CNCBlobLockHolder(CNCBlobStorage* storage)
    : m_Storage(storage),
      m_Blob(NULL),
      m_LockKnown(false),
      m_LockValid(false)
{}

inline CNCBlobStorage*
CNCBlobLockHolder::GetStorage(void)
{
    return m_Storage;
}

inline bool
CNCBlobLockHolder::IsLockAcquired(void) const
{
    return m_LockKnown  &&  m_LockValid;
}

inline bool
CNCBlobLockHolder::IsBlobExists(void) const
{
    _ASSERT(IsLockAcquired());

    return m_BlobExists;
}

inline bool
CNCBlobLockHolder::x_IsBlobWritable(void) const
{
    return m_RWHolder->GetLockType() == eWriteLock;
}

inline const string&
CNCBlobLockHolder::GetBlobKey(void) const
{
    return m_BlobInfo.key;
}

inline const string&
CNCBlobLockHolder::GetBlobSubkey(void) const
{
    return m_BlobInfo.subkey;
}

inline int
CNCBlobLockHolder::GetBlobVersion(void) const
{
    return m_BlobInfo.version;
}

inline size_t
CNCBlobLockHolder::GetBlobSize(void) const
{
    x_CheckBlobInfoRead();
    return m_BlobInfo.size;
}

inline const string&
CNCBlobLockHolder::GetBlobOwner(void) const
{
    x_CheckBlobInfoRead();
    return m_BlobInfo.owner;
}

inline int
CNCBlobLockHolder::GetBlobTTL(void) const
{
    x_CheckBlobInfoRead();
    return m_BlobInfo.ttl;
}

inline int
CNCBlobLockHolder::GetBlobAccessTime(void) const
{
    x_CheckBlobInfoRead();
    return m_BlobInfo.access_time;
}

inline bool
CNCBlobLockHolder::IsBlobExpired(void) const
{
    x_CheckBlobInfoRead();
    return m_BlobInfo.expired;
}

inline void
CNCBlobLockHolder::SetBlobOwner(const string& owner)
{
    _ASSERT(IsLockAcquired()  &&  x_IsBlobWritable());

    x_CheckBlobInfoRead();
    m_BlobInfo.owner = owner;
}


inline size_t
CNCBlob::GetSize(void) const
{
    return m_BlobInfo->size;
}

inline size_t
CNCBlob::GetPosition(void) const
{
    return m_Position;
}

inline bool
CNCBlob::IsFinalized(void) const
{
    return m_Finalized;
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_STORAGE_BLOB__HPP */
