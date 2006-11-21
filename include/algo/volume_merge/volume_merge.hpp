#ifndef ALGO_MERGE_HPP_
#define ALGO_MERGE_HPP_

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
 * Author:  Mike DiCuccio, Anatoliy Kuznetsov
 *   
 * File Description: Interfaces and algorithm to merge sorted volumes
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbimisc.hpp>

#include <util/resource_pool.hpp>

#include <vector>


BEGIN_NCBI_SCOPE


class  CMerge_Exception;
struct IMergeVolumeWalker;
struct IMergeStore;
class  IMergeBlob;

/// Main volume merge algorithm class
///
class CMergeVolumes
{
public:
    /// Raw BLOB buffer type used for raw information exchange between 
    /// merge algorithm interfaces
    /// Raw buffers are allocated/deallocated using one central 
    /// resource pool
    typedef vector<unsigned char> TRawBuffer;

    /// Memory manager for raw buffers to avoid unnecessary reallocations
    /// and memory fragmentation
    typedef CResourcePool<TRawBuffer, CFastMutex> TBufResourcePool;

    /// Buffer pool guard 
    typedef CResourcePoolGuard<TBufResourcePool>   TBufPoolGuard;

public:
    CMergeVolumes();
    ~CMergeVolumes();

    /// Set merge accumulator component
    void SetMergeAccumulator(IMergeBlob*  merger, 
                             EOwnership   own=eTakeOwnership);

    /// Set merge volumes 
    void SetVolumes(const vector<IMergeVolumeWalker*>& vol_vector,
                    EOwnership                         own=eTakeOwnership);

    /// Set merge store (destination)
    void SetMergeStore(IMergeStore*  store,
                       EOwnership   own=eTakeOwnership);

    /// Get BLOB buffer pool
    TBufResourcePool& GetBufResourcePool() { return m_BufResourcePool; }

    /// Execute merge process (main merge loop)
    void Run();
private:
    /// check if volume key is new minimum or equals to old minimum
    void x_EvaluateMinKey(unsigned new_key, size_t volume_idx);
    /// Reset min. evaluation
    void x_ResetMinKey();
    /// Merge all discovered min key candidates
    void x_MergeCandidates();
    void x_MergeVolume(IMergeVolumeWalker* volume);
    void x_StoreMerger();

private:
    TBufResourcePool            m_BufResourcePool;
    IMergeBlob*                 m_Merger;
    EOwnership                  m_OwnMerger;
    vector<IMergeVolumeWalker*> m_VolumeVect;
    EOwnership                  m_OwnVolumeVect;
    IMergeStore*                m_Store;
    EOwnership                  m_OwnStore;

    unsigned                    m_MergeKey;         ///< key in the merger
    unsigned                    m_MinKey;           ///< min key value
    vector<size_t>              m_MinKeyCandidates; ///< min-key volumes
};


/// Base Merge algorithms exception class
///
class CMerge_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eOperationNotReady,
        eUnsupportedKeyType,
        eInterfaceNotReady,
        eStoreFailure,
        eInputVolumeFailure
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eOperationNotReady:    return "eOperationNotReady";
        case eUnsupportedKeyType:   return "eUnsupportedKeyType";
        case eInterfaceNotReady:    return "eInterfaceNotReady";
        case eStoreFailure:         return "eStoreFailure";
        case eInputVolumeFailure:   return "eInputVolumeFailure";
        default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CMerge_Exception, CException);
};

/**
    Interface defines async. processing primitives
*/
struct IAsyncInterface
{
    virtual ~IAsyncInterface() {}

    /// Async. device status, describes if interface is in Ok 
    /// condition to continue processing 
    /// Some interface implementations can support async. execution, 
    /// and stay "not ready" until background logic finishes 
    /// (successfully or not)
    enum EStatus
    {
        eReady = 0,   ///< Volume is ready 
        eFailed,      ///< Last operation failed and interface cannot recover
        eNoMoreData,  ///< Interface reached the end (EOF)
        eNotReady     ///< Last request did not finish yet
    };

    /// Get current interface async. status
    virtual EStatus GetStatus() const = 0;

    /// Wait until interface is ready (or operation fails)
    /// (On failure volume is free to throw an exception)
    virtual EStatus WaitReady() const = 0;
};


/**
    Interface to traverse volume for merge.

    Volume provides access to key->BLOB pairs, and guarantees
    that keys are coming in an ascending sorted order.
    Volume can support async. execution using threads or remote system
    to facilitate parallel execution
*/
struct IMergeVolumeWalker
{
    virtual ~IMergeVolumeWalker() {}

    /// Get pointer to async. suport.
    /// Returns NULL if this interface is fully syncronous.
    virtual IAsyncInterface* QueryIAsync() = 0;

    /// Return TRUE when volume traverse reaches the end
    virtual bool IsEof() const = 0;

    /// Return TRUE if volume is in good condition (not failed)
    virtual bool IsGood() const = 0;

    /// Request to start fetching data
    /// This request can be asyncronous 
    ///   caller needs to check status using IAsyncInterface 
    ///   to make sure Fetch finished
    virtual void FetchFirst() = 0;

    /// Request to get next record
    /// This request can be asyncronous 
    ///   caller needs to check status using IAsyncInterface 
    ///   to make sure Fetch finished
    virtual void Fetch() = 0;

    /// Get low level access to the current key buffer
    virtual const unsigned char* GetKeyPtr() const = 0;

    /// Get access to the key as unsigned integer 
    /// (if this type is supported)
    virtual Uint4 GetUint4Key() const = 0;

    /// Get low level access to the merge BLOB buffer and buffer size
    /// (next Fetch call invalidates this pointer)
    virtual const unsigned char* GetBufferPtr(size_t* buf_size) const = 0;

    /// Close volume (when it ends)
    /// Method is responsible for finalization of merge procedure 
    /// (it could be deletion of records, compaction of data files, etc),
    /// stopping background threads, closing server connections
    virtual void Close() = 0;

    /// Signals that current record moved to merged storage
    /// (volume manager may decide to delete it later)
    /// Volume manager should NOT fetch next record on this call
    virtual void SetRecordMoved() = 0;
};


/**
    BLOB merge interface, merges one or more BLOBs together

    Implementation of this interface should be statefull so
    consequtive merges accumulate in the internal buffer,
    then flush called to get the final merged BLOB to storage

    Implementation notice:
    When receiving first Merge call implementation should not immediately
    unpack the buffer, because the next call may ask to discard the merge, or
    get the BLOB back, in this case decompression will be a waste CPU.
*/
class IMergeBlob
{
public:
    IMergeBlob() 
        : m_BufResourcePool(0)
    {}

    virtual ~IMergeBlob() {}

    /// Set resource pool for BLOB buffer memory management
    void SetResourcePool(CMergeVolumes::TBufResourcePool& res_pool)
    {
        m_BufResourcePool = &res_pool;
    }

    /// Merge request
    /// Implementation MUST buffer to the buffer pool (may be delayed)
    virtual void Merge(CMergeVolumes::TRawBuffer* buffer) = 0;

    /// Returns destination (merged) buffer
    /// Caller MUST return the buffer to the buffer pool
    ///
    /// @return 
    ///    NULL if nothing to merge
    virtual CMergeVolumes::TRawBuffer* GetMergeBuffer() = 0;

    /// Reset merging, forget all the accumulated buffers
    virtual void Reset() = 0;

protected:
    CMergeVolumes::TBufResourcePool* m_BufResourcePool;
};

/**
    Interface to store merged BLOBs 
*/
struct IMergeStore
{
    virtual ~IMergeStore() {}

    /// Get pointer to async. suport.
    /// Returns NULL if this interface is fully syncronous.
    virtual IAsyncInterface* QueryIAsync() = 0;

    /// Set resource pool for BLOB buffer memory management
    void SetResourcePool(CMergeVolumes::TBufResourcePool& res_pool)
    {
        m_BufResourcePool = &res_pool;
    }

    /// Return TRUE if storage device is in good shape
    virtual bool IsGood() const = 0;

    /// Store BLOB request 
    /// This request can be asyncronous 
    ///   caller needs to check status using IAsyncInterface 
    ///   to make sure Fetch finished
    /// Methos MUST return storage buffer to the resource pool
    virtual void Store(Uint4 blob_id, CMergeVolumes::TRawBuffer* buffer) = 0;
    
    /// Close storage (when it ends)
    /// Method is responsible for finalization of store procedure, 
    /// stopping background threads, closing server connections
    virtual void Close() = 0;

protected:
    CMergeVolumes::TBufResourcePool* m_BufResourcePool;
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/11/21 14:38:58  kuznets
 * WaitReady() declared const
 *
 * Revision 1.2  2006/11/21 06:52:05  kuznets
 * Use CFastMutex as pool locking parameter
 *
 * Revision 1.1  2006/11/17 07:29:32  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif /* ALGO_MERGE_HPP */

