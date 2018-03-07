#ifndef OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_HPP

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
 * Authors: Denis Vakatov, Rafael Sadyrov
 *
 */

#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Reply_Get_Blob_Id.hpp>
#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE


/// Bio-id (such as accession), plus optional user-provided context data
///
class CPSG_BioId
{
public:
    /// Base class for user-provided context data (optional)
    struct IContext
    {
        virtual ~IContext()  {}
    };

    /// @param id
    ///  Bio ID (like accession)
    /// @param ctx
    ///  User-provided context (owned by the caller)
    CPSG_BioId(string id, IContext* ctx = nullptr)
        : m_Id(move(id)), m_Context(ctx)  {}

    const string&   GetId()      const  { return m_Id;      }
    const IContext* GetContext() const  { return m_Context; }

private:
    string    m_Id;
    IContext* m_Context;
};


typedef vector<CPSG_BioId> TPSG_BioIds;



/// The generic part of the results of id resolution
///
class CPSG_BlobId
{
public:
    /// Blob storage location and other attributes
    struct SBlobInfo
    {
        objects::CID2_Blob_Id::TSat                   sat           = {};
        objects::CID2_Blob_Id::TSat_key               sat_key       = {};
        objects::CID2_Reply_Get_Blob_Id::TBlob_state  state         = {};
        TGi                                           gi            = {};
        time_t                                        last_modified = {};
        TTaxId                                        tax_id        = {};
        Uint8                                         seq_length    = {};
    };

    /// Get info about the blob (obtained as a result of its id resolving)
    /// @throw
    ///  If not in "eResolved" state
    const SBlobInfo& GetBlobInfo() const  { return m_BlobInfo; }

    /// Resolution result
    /// @sa GetStatus
    enum EStatus {
        eSuccess,       ///< Successfully resolved
        eNotFound,      ///< Not found
        eCanceled,      ///< Request canceled
        eTimeout,       ///< Timeout
        eUnknownError   ///< Unknown error
    };

    /// Get result of this id resolution
    EStatus GetStatus() const  { return m_Status; }

    /// Unstructured text containing auxiliary info about the result
    const string& GetMessage() const  { return m_Message; };

    /// Get the bio-blob id (such as an accession) which this result is for
    const CPSG_BioId& GetBioId() const  { return m_BioId; }

private:
    CPSG_BlobId(CPSG_BioId bio_id) : m_BioId(move(bio_id))  {}

    CPSG_BioId  m_BioId;
    SBlobInfo   m_BlobInfo;
    EStatus     m_Status = eUnknownError;
    string      m_Message;

    friend class CPSG_BioIdResolutionQueue;
};


typedef vector<CPSG_BlobId> TPSG_BlobIds;



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
class CPSG_BioIdResolutionQueue
{
public:
    /// @param service
    ///  Either a name of service (which can be resolved into a set of PSG
    ///  servers) or a single fixed PSG server (in format "host:port")
    CPSG_BioIdResolutionQueue(const string& service);
    ~CPSG_BioIdResolutionQueue();

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
    void Resolve(TPSG_BioIds*     bio_ids,
                 const CDeadline& deadline = 0);

    /// Perform a single bio-id resolution, synchronously
    static CPSG_BlobId Resolve(const string&    service,
                               CPSG_BioId       bio_id,
                               const CDeadline& deadline = CDeadline::eInfinite);

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
    TPSG_BlobIds GetBlobIds(const CDeadline& deadline = 0,
                            size_t           max_results = 0);

    /// Cancel all ongoing retrievals and return all not yet resolved bio-ids.
    /// You can continue working with the queue after that in a usual manner.
    /// If any of the bio-id resolution results are available they can
    /// be subsequently retrieved by calling GetBlobIds() method.
    /// @param bio_ids
    ///  The not yet resolved bio-ids will be added to "bio_ids". If "bio_ids"
    ///  is NULL, then they be discarded.
    void Clear(TPSG_BioIds* bio_ids);

    /// TRUE if there is nothing left in the queue (whether resolved or not)
    bool IsEmpty() const;

private:
    struct SItem;
    mutable pair<vector<unique_ptr<SItem>>, mutex> m_Items;
    shared_ptr<void>          m_Future;
    string                    m_Service;
};



/// Blob that is ready to get its data retrieved
///
class CPSG_Blob
{
public:
    /// Get retrieved data
    istream& GetStream()  { return *m_Stream; }

    /// Retrieval result
    /// @sa GetStatus
    enum EStatus {
        eSuccess,       ///< Successfully retrieved
        eNotFound,      ///< Not found
        eCanceled,      ///< Request canceled
        eTimeout,       ///< Timeout
        eUnknownError   ///< Unknown error
    };

    /// Get result of this blob retrieval
    EStatus GetStatus() const  { return m_Status; }

    /// Unstructured text containing auxiliary info about the result
    const string& GetMessage() const  { return m_Message; };

    /// Get the blob id which this blob is for
    const CPSG_BlobId& GetBlobId() const  { return m_BlobId; };

private:
    CPSG_Blob(CPSG_BlobId blob_id) : m_BlobId(move(blob_id))  {}

    CPSG_BlobId         m_BlobId;
    unique_ptr<istream> m_Stream;
    EStatus             m_Status = eUnknownError;
    string              m_Message;

    friend class CPSG_BlobRetrievalQueue;
};


typedef vector<CPSG_Blob> TPSG_Blobs;



/// A queue to retrieve blob data from the storage.
///
/// Call Retrieve() to schedule more blobs to retrieve (by their bio-ids or
/// blob-ids). Then, call GetBlobs() to get the blobs that are ready for
/// immediate retrieval.
///
/// All methods are MT-safe.
///
/// The queue object can be used from more than one thread, either to add ids
/// or to get the list of ready-to-be-retrieved blobs.
/// Results for the ids which were added to a given instance of
/// the queue will be available for retrieval using this (and only this) queue
/// instance regardless of which threads were used to add the ids to the queue.
///
class CPSG_BlobRetrievalQueue
{
public:
    /// @param service
    ///  Either a name of service (which can be resolved into a set of PSG
    ///  servers) or a single fixed PSG server (in format "host:port")
    CPSG_BlobRetrievalQueue(const string& service);
    ~CPSG_BlobRetrievalQueue();

    /// Try to schedule more bio-ids for the blob retrieval.
    /// @note
    ///  If more than one thread is blocked on adding new set of bio-ids
    ///  (or blob-ids) to the queue, then (when the queue gets free slots) one
    ///  of the threads gets to add all or part of its bio-ids (or blob-ids) to
    ///  the queue. Other threads will continue waiting (unless there is even 
    ///  more free space left in the queue).
    /// @param bio_ids
    ///  List of bio ids.
    ///  Those bio-ids from the "bio_ids" container that make it
    ///  into the queue will be removed from the "bio_ids" container.
    /// @param deadline
    ///  For how long to try to push the bio-ids into the queue. The function
    ///  returns if either it succeeds to push ALL of the bio-ids into the
    ///  queue; or if it hits the deadline.
    /* TODO: void Retrieve(TPSG_BioIds*     bio_ids,
                           const CDeadline& deadline = 0);*/


    /// Perform single blob retrieval (by its bio-id), synchronously
    /* TODO: static CPSG_Blob Retrieve(const string&    service,
                                       CPSG_BioId       bio_id,
                                       const CDeadline& deadline = CDeadline::eInfinite);*/

    /// Try to schedule more blob-ids for the blob retrieval.
    /// @note
    ///  If more than one thread is blocked on adding new set of blob-ids
    ///  (or bio-ids) to the queue, then (when the queue gets free slots) one
    ///  of the threads gets to add all or part of its blob-ids (or bio-ids) to
    ///  the queue. Other threads will continue waiting (unless there is even 
    ///  more free space left in the queue).
    /// @param blob_ids
    ///  List of blob ids.
    ///  Those blob ids from the "blob_ids" container that make it
    ///  into the queue will be removed from the "blob_ids" container.
    /// @param deadline
    ///  For how long to try to push the blob-ids into the queue. The function
    ///  returns if either it succeeds to push ALL of the blob-ids into the
    ///  queue; or if it hits the deadline.
    void Retrieve(TPSG_BlobIds*    blob_ids,
                  const CDeadline& deadline = 0);

    /// Perform single blob retrieval (by its blob-id), synchronously
    static CPSG_Blob Retrieve(const string&    service,
                              CPSG_BlobId      blob_id,
                              const CDeadline& deadline = CDeadline::eInfinite);

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
    TPSG_Blobs GetBlobs(const CDeadline& deadline = 0,
                        size_t           max_results = 0);

    /// Cancel all ongoing retrievals and return all blob-ids and bio-ids
    /// for which there is no ready-to-retrieve blob obtained yet.
    /// You can continue working with the queue after that in a usual manner.
    /// If there are ready-to-be-retrieved blobs available in the queue
    /// they can be subsequently retrieved by calling GetBlobs() method.
    /// @param bio_ids
    ///  The bio-ids for which no retrievable blobs have yet been obtained 'll
    ///  be added to "bio_ids". If "bio_ids" is NULL, then they be discarded.
    /// @param blob_ids
    ///  The blob-ids for which no retrievable blobs have yet been obtained 'll
    ///  be added to "blob_ids". If "blob_ids" is NULL, then they be discarded.
    void Clear(/* TODO: TPSG_BioIds* bio_ids,*/ TPSG_BlobIds* blob_ids);

    /// TRUE if there is nothing left in the queue (whether retrieved or not)
    bool IsEmpty() const;

private:
    struct SItem;
    mutable pair<vector<unique_ptr<SItem>>, mutex> m_Items;
    shared_ptr<void>          m_Future;
    string                    m_Service;
};


END_NCBI_SCOPE


#endif  /* OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_HPP */
