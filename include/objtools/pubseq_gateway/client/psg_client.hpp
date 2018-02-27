#ifndef OBJTOOLS_PUBSEQ_GATEWAY_PSG_CLIENT_HPP
#define OBJTOOLS_PUBSEQ_GATEWAY_PSG_CLIENT_HPP

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
 * Author: Denis Vakatov
 *
 */

#include <string>
#include <utility>
#include <mutex>
#include <memory>
#include <vector>

#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Reply_Get_Blob_Id.hpp>
#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE


class CBioIdResolutionQueue;
class CBlobRetrievalQueue;


/// Bio-id + context
///
class CBioId
{
public:
    using TSet = vector<CBioId>;

    struct IContext
    {
        virtual ~IContext() {}
    };

    CBioId(string id, IContext* ctx = nullptr)
        : m_Id(move(id)), m_Context(ctx) {}

    const string& GetId() const { return m_Id; }
    const IContext* GetContext() const  { return m_Context; }

private:
    string    m_Id;
    IContext* m_Context;
};


/// The generic part of the results of id resolution.
///
class CBlobId
{
public:
    using TSet = vector<CBlobId>;

    /// Blob storage location and other attributes
    struct SBlobInfo
    {
        using CID2_Blob_Id = objects::CID2_Blob_Id;
        using CID2_Reply_Get_Blob_Id = objects::CID2_Reply_Get_Blob_Id;

        struct
        {
            CID2_Blob_Id::TSat     sat = 0;
            CID2_Blob_Id::TSat_key sat_key = 0;
        } id2;

        CID2_Reply_Get_Blob_Id::TBlob_state  state = 0;
        TGi                                  gi = 0;          // informational only
        time_t                               date_queued = 0; // informational only
        TTaxId                               tax_id = 0;
        Uint8                                seq_length = 0;
    };

    /// Get info about the blob (obtained as a result of its id resolving)
    /// @throw
    ///  If not in "eResolved" state
    const SBlobInfo& GetBlobInfo() const { return m_BlobInfo; }

    /// Resolution result
    /// @sa GetStatus
    enum EStatus {
        eResolved,   ///< Successfully resolved
        eFailed      ///< Resolution has failed
    };

    /// Get result of this id resolution
    EStatus GetStatus() const { return m_Status; }

    /// Resolution's fine-grain actionable details
    /// @sa GetStatusEx
    enum EStatusEx {
        eNone,       ///< No additional info available
        eForbidden,  ///< Access denied
        eNotFound,   ///< Not found
        eInvalid,    ///< Invalid id
        eCanceled,   ///< Request canceled
        eError       ///< Unknown error
    };

    /// Get result of this id resolution
    EStatusEx GetStatusEx() const { return m_StatusEx; }

    /// Unstructured text containing auxiliary info about the result
    const string& GetMessage() const { return m_Message; };

    /// Get the bio-blob id (such as an accession) which this result is for
    const CBioId& GetBioId() const { return m_BioId; }

private:
    CBlobId(CBioId bio_id) : m_BioId(move(bio_id)) {}

    CBioId     m_BioId;
    SBlobInfo  m_BlobInfo;
    EStatus    m_Status = eFailed;
    EStatusEx  m_StatusEx = eError;
    string     m_Message;

    friend class CBioIdResolutionQueue;
};


/// A queue to resolve "biological" blob ids (such as accessions) into the
/// storage specific blob ids.
///
/// Add more bio ids to resolve using Resolve(), then get the resolved
/// blob ids using GetBlobIds().
///
/// All methods are MT-safe.
///
/// The queue object can be used from more than one thread, either to add ids
/// or to retrieve the resolved ids, in any order thereof.
/// Resolution results for the ids which were added to a given instance of
/// the queue will be available for retrieval using this (and only this) queue
/// instance regardless of which threads were used to add the ids to the queue.
///
class CBioIdResolutionQueue
{
public:
    CBioIdResolutionQueue(const string& service);
    ~CBioIdResolutionQueue();

    /// Schedule more bio-ids for resolution
    /// @note
    ///  If more than one thread is blocked on adding new set of bio-ids to the
    ///  queue, then (when the queue gets free slots) one of the threads gets
    ///  to add all (or part) of its bio-ids to the queue. Other threads will
    ///  continue waiting (unless there is more free space left).
    /// @param bio_ids
    ///  List of bio-blob ids (such as accessions) to resolve.
    ///  Those bio-blob ids from the "bio_ids" container that make it
    ///  into the queue will be removed from the "bio_ids" container.
    /// @param deadline
    ///  For how long to try to push the bio-ids into the queue.
    void Resolve(CBioId::TSet* bio_ids, const CDeadline& deadline = 0);

    /// Perform signle bio-ids resolution
    /// @note
    /// This method is works synchronously static and does not require
    /// CBioIdResolutionQueue instance
    static CBlobId Resolve(const string& service, CBioId bio_id, const CDeadline& deadline = CTimeout::eInfinite);

    /// Retrieve results of the id resolution that are currently ready.
    /// @note
    ///  If more than one thread tries to retrieve id resolution results, then
    ///  one of the threads gets the results as they become ready; other
    ///  threads will continue waiting.
    /// @param deadline
    ///  Until when to wait for the results to get ready for retrieval.
    ///  If no resolution results are ready by the deadline expiration time,
    ///  then return empty container.
    /// @param max_results
    ///  Return no more than that number of results. Zero means no limit.
    /// @return
    ///  List of id resolution results
    /// @throw
    ///  If unrecoverable retrieval error has been detected.
    CBlobId::TSet GetBlobIds(const CDeadline& deadline = 0, size_t max_results = 0);

    /// Cancel all ongoing retrievals and return all not yet resolved bio-ids.
    /// You can continue working with the queue after that in a usual manner.
    /// If any of the bio-id resolution results are available they can
    /// be subsequently retrieved by calling GetBlobIds() method.
    /// @param bio_ids
    ///  The not yet resolved bio-ids will be added to "bio_ids". If "bio_ids"
    ///  is NULL, then they be discarded.
    void Clear(CBioId::TSet* bio_ids);

    /// Returns true if Queue has finished all resolutions
    /// and all items have been fetched or nothing was added for resolution at all
    bool IsEmpty() const;

private:
    mutable mutex m_ItemsMtx;

    struct CBioIdResolutionQueueItem;
    vector<unique_ptr<CBioIdResolutionQueueItem>> m_Items;
    shared_ptr<void> m_Future;
    string m_Service;
};


/// Blob that is ready to get its data retrieved
///
class CBlob
{
public:
    using TSet = vector<CBlob>;

    /// Get retrieved data
    unique_ptr<istream> GetStream() { return move(m_Stream); }

    /// Retrieval result
    /// @sa GetStatus
    enum EStatus {
        eRetrieved, ///< Successfully retrieved
        eFailed     ///< Retrieval has failed
    };

    /// Get result of this blob retrieval
    EStatus GetStatus() const { return m_Status; }

    /// Retrieval's fine-grain actionable details
    /// @sa GetStatusEx
    enum EStatusEx {
        eNone,       ///< No additional info available
        eForbidden,  ///< Access denied
        eNotFound,   ///< Not found
        eInvalid,    ///< Invalid id
        eCanceled,   ///< Request canceled
        eError       ///< Unknown error
    };

    /// Get result of this blob retrieval
    EStatusEx GetStatusEx() const { return m_StatusEx; }

    /// Unstructured text containing auxiliary info about the result
    const string& GetMessage() const { return m_Message; };

    /// Get the blob id which this blob is for
    const CBlobId& GetBlobId() const  { return m_BlobId; };

private:
    CBlob(CBlobId blob_id) : m_BlobId(move(blob_id)) {}

    CBlobId             m_BlobId;
    unique_ptr<istream> m_Stream;
    EStatus             m_Status = eFailed;
    EStatusEx           m_StatusEx = eError;
    string              m_Message;

    friend class CBlobRetrievalQueue;
};


/// A queue to retrieve blob data from the storage.
///
/// Call Retrieve() to schedule more blob-ids for retrieval, then
/// call GetBlobs() to get the blobs that are ready for immediate retrieval.
///
/// All methods are MT-safe.
///
/// The queue object can be used from more than one thread, either to add ids
/// or to get the list of ready-to-be-retrieved blobs.
/// Results for the ids which were added to a given instance of
/// the queue will be available for retrieval using this (and only this) queue
/// instance regardless of which threads were used to add the ids to the queue.
///
class CBlobRetrievalQueue
{
public:
    CBlobRetrievalQueue(const string& service);
    ~CBlobRetrievalQueue();

    /// Schedule more blob-ids for the blob retrieval
    /// @note
    ///  If more than one thread is blocked on adding new set of blob-ids
    ///  to the queue, then (when the queue gets free slots) one of
    ///  the threads gets to add all or part of its blob-ids to
    ///  the queue. Other threads will continue waiting (unless there is more
    ///  free space left).
    /// @param blob_ids
    ///  List of blob ids.
    ///  Those blob ids from the "blob_ids" container that make it
    ///  into the queue will be removed from the "blob_ids" container.
    /// @param deadline
    ///  For how long to try to push the blob-ids into the queue.
    void Retrieve(CBlobId::TSet* blob_ids, const CDeadline& deadline = 0);

    /// Perform single blob retrieval
    /// @note
    /// This method works synchronously static and does not require
    /// CBlobRetrievalQueue instance
    static CBlob Retrieve(const string& service, CBlobId blob_id, const CDeadline& deadline = CTimeout::eInfinite);

    /// Get blobs that are ready for the immediate retrieval (and/or those that
    /// cannot be retrieved for whatever reason).
    /// @note
    ///  If more than one thread tries to retrieve list of the ready blobs,
    ///  then one of the threads gets the results as they become ready; other
    ///  threads will continue waiting.
    /// @param deadline
    ///  Until when to wait for the blobs to get ready for retrieval.
    ///  If no blobs are ready for retrieval by the deadline expiration, then
    ///  return empty container.
    /// @param max_results
    ///  Return no more than that number of results. Zero means no limit.
    /// @return
    ///  List of blobs available for retrieval
    /// @throw
    ///  If unrecoverable retrieval error has been detected.
    CBlob::TSet GetBlobs(const CDeadline& deadline = 0, size_t max_results = 0);

    /// Cancel all ongoing retrievals and return all blob-ids
    /// for which there is no ready-to-retrieve blob obtained yet.
    /// You can continue working with the queue after that in a usual manner.
    /// If there are ready-to-be-retrieved blobs available in the queue
    /// they can be subsequently retrieved by calling GetBlobs() method.
    /// @param blob_ids
    ///  The blob-ids for which no retrievable blobs have yet been obtained will
    ///  be added to "blob_ids". If "blob_ids" is NULL, then they be discarded.
    void Clear(CBlobId::TSet* blob_ids);

    /// Returns true if Queue has finished all retrieval work
    /// and all items have been fetched or nothing was added for retrieval at all
    bool IsEmpty() const;

private:
    mutable mutex m_ItemsMtx;

    struct CBlobRetrievalQueueItem;
    vector<unique_ptr<CBlobRetrievalQueueItem>> m_Items;
    shared_ptr<void> m_Future;
    string m_Service;
};


END_NCBI_SCOPE


#endif
