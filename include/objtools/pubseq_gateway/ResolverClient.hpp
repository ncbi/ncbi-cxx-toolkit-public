#ifndef _RESOLVER_CLIENT_HPP_
#define _RESOLVER_CLIENT_HPP_

#include <string>
#include <utility>
#include <mutex>

#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Reply_Get_Blob_Id.hpp>
#include <corelib/ncbitime.hpp>

namespace HCT {
    class io_future;
    class http2_request;
};

BEGIN_NCBI_SCOPE

constexpr const char ACCVER_RESOLVER_SERVICE_ID[] = "ID.RESOLVER.ACCVER.1";
constexpr const char ACCVER_RESOLVER_COLUMNS[] = "ACCVER CVARCHAR NOTNULL KEY, GI INT8, LEN UINT4, SAT INT2, SAT_KEY UINT4, TAXID UINT4, DATE DATETIME, SUPPRESS BIT NOTNULL";
//                                                                             0        1          2          3              4            5              6

BEGIN_objects_SCOPE

/// Base class for the user-defined context
class IBioIdContext
{
public:
    virtual ~IBioIdContext() {}
};


/// Bio-id + context
class CBioId
{
public:
    CBioId(std::string id, IBioIdContext* ctx = nullptr)
        : m_Id(std::move(id)), m_Context(ctx) {}

    const std::string& GetId() const { return m_Id; }
    const IBioIdContext* GetContext() const  { return m_Context; }

private:
    std::string    m_Id;
    IBioIdContext* m_Context;
};

class CBioIdResolutionQueue;

/// The generic part of the results of id resolution.
///
class CBlobId
{
public:
    explicit CBlobId(const CBioId& bio_id) 
        : m_BioId(bio_id), m_Status(eResNone), m_StatusEx(eNone) {}
    /// Storage blob id (as accepted by the CBlobRetrieveQueue)
    struct SBlobId {
        CID2_Blob_Id::TSat     sat;
        CID2_Blob_Id::TSat_key sat_key;
        // CID2_Blob_Id::TSub_sat sub_sat;
        CID2_Blob_Id::TVersion version;
        SBlobId()
            : sat(0), sat_key(0), version(0) {}
    };

    /// Get storage blob id (as accepted by the CBlobRetrieveQueue)
    /// @throw
    ///  If not in "eResolved" state
    const SBlobId& GetBlobId() const { return m_BlobInfo.blob_id; }

    /// Blob storage location and other attributes
    struct SBlobInfo {
        SBlobId                              blob_id;
        CID2_Reply_Get_Blob_Id::TBlob_state  state;
        TGi                                  gi;          // informational only
        time_t                               date_queued; // informational only
        TTaxId                               tax_id;
        Uint8                                seq_length;
        SBlobInfo() 
            : state(0), gi(0), tax_id(0), seq_length(0) {}
    };

    /// Get info about the blob (obtained as a result of its id resolving)
    /// @throw
    ///  If not in "eResolved" state
    const SBlobInfo& GetBlobInfo() const { return m_BlobInfo; }

    /// Resolution result
    /// @sa GetStatus
    enum EStatus {
        eResNone,    ///< Resolution is not performed
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
    const std::string& GetMessage() const { return m_Message; };

    /// Get the bio-blob id (such as an accession) which this result is for
    const CBioId& GetBioId() const { return m_BioId; }


    // TODO:  ctor, dtor, mover, etc

private:
    CBioId     m_BioId;
    SBlobInfo  m_BlobInfo;
    EStatus    m_Status;
    EStatusEx  m_StatusEx;
    std::string  m_Message;
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

class CBioIdResolutionQueue
{
public:
    CBioIdResolutionQueue();
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
    void Resolve(std::vector<CBioId>*  bio_ids,
                 const CDeadline& deadline = CDeadline(0));

    /// Perform signle bio-ids resolution
    /// @note
    /// This method is works synchronously static and does not require 
    /// CBioIdResolutionQueue instance
    static CBlobId Resolve(CBioId bio_ids,
                 const CDeadline& deadline = CDeadline(0));

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
    std::vector<CBlobId> GetBlobIds(const CDeadline& deadline = CDeadline(0), 
                                    size_t max_results = 0);

    /// Cancel all ongoing retrievals and return all not yet resolved bio-ids.
    /// You can continue working with the queue after that in a usual manner.
    /// If any of the bio-id resolution results are available they can
    /// be subsequently retrieved by calling GetBlobIds() method.
    /// @param bio_ids
    ///  The not yet resolved bio-ids will be added to "bio_ids". If "bio_ids"
    ///  is NULL, then they be discarded.
    void Clear(std::vector<CBioId>* bio_ids);

    /// Returns true if Queue has finished all resolutions
    /// and all items have been fetched or nothing was added for resolution at all
    bool IsEmpty() const;
private:
    mutable std::mutex m_ItemsMtx;
    
    struct CBioIdResolutionQueueItem {
        CBioIdResolutionQueueItem(std::shared_ptr<HCT::io_future> afuture, CBioId bio_id);
        void SyncResolve(CBlobId& blob_id, const CDeadline& deadline);
        void WaitFor(long timeout_ms);
        void Wait();
        bool IsDone() const;
        void Cancel();
        void PopulateData(CBlobId& blob_id) const;
        std::shared_ptr<HCT::http2_request> m_Request;
        CBioId m_BioId;
    };

    std::vector<std::unique_ptr<CBioIdResolutionQueueItem>> m_Items;
    std::shared_ptr<HCT::io_future> m_Future;
};



/// Blob that is ready to get its data retrieved
class CBlob
{
public:
    /// Blob ID and other non-data ID resolution and retrieval attributes 
    const CBlobId& GetBlobId() const  { return m_BlobId; };

    /// Blob data retrieval API 
    // ........

private:
    CBlobId m_BlobId;
    // ........
};



/// A queue to retrieve bio-blob data from the storage.
///
/// Call Retrieve() to schedule more bio-ids (or blob-ids) for retrieval, then
/// call GetBlobs() to get the blobs that are ready for immediate retrieval.
///
/// All methods are MT-safe.
///
/// The queue object can be used from more than one thread, either to add ids
/// or to get the list of ready-to-be-retrieved blobs.
/// Results for the ids which were added to a given instance of
/// the queue will be available for retrieval using this (and only this) queue
/// instance regardless of which threads were used to add the ids to the queue.

class CBlobRetrievalQueue
{
public:
    /// Schedule more bio-ids or blob-ids for the blob retrieval
    /// @note
    ///  If more than one thread is blocked on adding new set of bio-ids (or
    ///  blob-ids) to the queue, then (when the queue gets free slots) one of
    ///  the threads gets to add all or part of its bio-ids  (or blob-ids) to
    ///  the queue. Other threads will continue waiting (unless there is more
    ///  free space left).
    /// @param bio_ids
    ///  List of bio-blob ids (such as accessions).
    ///  Those bio-blob ids from the "bio_ids" container that make it
    ///  into the queue will be removed from the "bio_ids" container.
    /// @param deadline
    ///  For how long to try to push the bio-ids into the queue.
    void Retrieve(std::vector<CBioId>*  bio_ids,
                  const CDeadline& deadline = CDeadline(0));
    /// @param blob_ids
    ///  List of blob ids.
    ///  Those blob ids from the "blob_ids" container that make it
    ///  into the queue will be removed from the "blob_ids" container.
    void Retrieve(std::vector<CBlobId>* blob_ids,
                  const CDeadline& deadline = CDeadline(0));

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
    ///  If unrecoverable resolution or retrieval error has been detected.
    std::vector<CBlob> GetBlobs(const CDeadline& deadline = CDeadline(0), 
                                size_t max_results = 0);

    /// Cancel all ongoing retrievals and return all bio- and blob-ids
    /// for which there is no ready-to-retrieve blob obtained yet.
    /// You can continue working with the queue after that in a usual manner.
    /// If there are ready-to-be-retrieved blobs available in the queue
    /// they can be subsequently retrieved by calling GetBlobs() method.
    /// @param bio_ids
    ///  The not yet resolved bio-ids will be added to "bio_ids". If "bio_ids"
    ///  is NULL, then they be discarded.
    /// @param blob_ids
    ///  The bio-ids for which no retrievable blobs have yet been obtained will
    ///  be added to "blob_ids". If "blob_ids" is NULL, then they be discarded.
    void Clear(std::vector<CBioId>* bio_ids, std::vector<CBlobId>* blob_ids);

    /// Returns true if Queue has finished all retrieval work
    /// and all items have been fetched or nothing was added for retrieval at all
    bool IsEmpty() const;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
