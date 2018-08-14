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
 * Authors: Denis Vakatov (design), Rafael Sadyrov (implementation)
 *
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_url.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#ifdef NCBI_NAMED_ANNOTS_IMPLEMENTED
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#endif  /* NCBI_NAMED_ANNOTS_IMPLEMENTED */


BEGIN_NCBI_SCOPE


/// Request to the PSG server (one of: CPSG_Request_Biodata, CPSG_Request_Blob)
///

class CPSG_Request
{
public:
    /// Get the user-provided context
    template<typename TUserContext>
    shared_ptr<TUserContext> GetUserContext() const
    { return static_pointer_cast<TUserContext>(m_UserContext.lock()); }

protected:
    CPSG_Request(weak_ptr<void> user_context = {})
        : m_UserContext(user_context)
    {}

    virtual ~CPSG_Request() = default;

private:
    weak_ptr<void>  m_UserContext;
};



/// Bio-id (such as accession)
///
class CPSG_BioId
{
public:
    using TType = objects::CSeq_id::E_Choice;

    /// @param id
    ///  Bio ID (like accession)
    CPSG_BioId(string id, TType type = {}) : m_Id(move(id)), m_Type(type) {}

    const string& Get()     const { return m_Id; }
    TType         GetType() const { return m_Type; }

private:
    string m_Id;
    TType  m_Type;
};



/// Request to the PSG server (by bio-id, for a biodata specific info and data)
///

class CPSG_Request_Biodata : public CPSG_Request
{
public:
    /// 
    CPSG_Request_Biodata(CPSG_BioId      bio_id,
                         weak_ptr<void>  user_context = {})
        : CPSG_Request(user_context),
          m_BioId(bio_id)
    {}

    const CPSG_BioId& GetBioId() const { return m_BioId; }

    /// Specify which info and data is needed
    enum EIncludeData {
        /// Only the info
        /// @NOTE  Incompatible with the "fWholeTSE" and "fOrigTSE")
        fNoTSE     = (1 << 0),

        /// Only the "fast" info
        /// @note  Ignored if other flags require the "slow" info to be used
        fFastInfo  = (1 << 1),

        /// By default (unless "fNoTSE" flag is set):
        /// - if the TSE blob is split, then only the blob containing its split
        ///   info will be retrieved;
        /// - if the blob is not split, then the whole original TSE will be
        ///   retrieved.
        /// This flag forces the retrieval of the whole TSE. If it is set then:
        /// - if the TSE blob is split, then all its split info and split data
        ///   info will be retrieved;
        /// - if the blob is not split, then the whole original TSE will be
        ///   retrieved.
        fWholeTSE  = (1 << 2),

        /// Retrieve the whole original(!) TSE blob
        fOrigTSE   = (1 << 3),

        // These flags correspond exactly to the CPSG_BioseqInfo's getters
        fCanonicalId      = (1 <<  4),
        fOtherIds         = (1 <<  5),
        fMoleculeType     = (1 <<  6),
        fLength           = (1 <<  7),
        fState            = (1 <<  8),
        fBlobId           = (1 <<  9),
        fTaxId            = (1 << 10),
        fHash             = (1 << 11),
        fDateChanged      = (1 << 12)
    };
    typedef int TIncludeData;   // Bit-set of EIncludeData flags
    void IncludeData(TIncludeData include);

    TIncludeData      GetIncludeData() const { return m_IncludeData; }

private:
    CPSG_BioId    m_BioId;
    TIncludeData  m_IncludeData;

#ifdef NCBI_NAMED_ANNOTS_IMPLEMENTED
public:
    enum EAnnotTypes {
        fGene       = (1 << 0),  ///<
        fFeatTable  = (1 << 1),  ///< 
        fAlign      = (1 << 2),  ///<
        fGraph      = (1 << 3),  ///<
        fSeqTable   = (1 << 4)   ///<
    };
    typedef int TAnnotTypes;  // Bitset of CSeq_annot::C_Data::E_Choice
    void IncludeNamedAnnot(CTempString               annot_name,
                           TAnnotTypes               types     = 0 /*all*/,
                           vector<objects::CSeq_loc> locations = {});
private:
    struct SIncludeNamedAnnot {
        string                     annot_name;
        TAnnotTypes                types;
        vector<objects::CSeq_loc>  locations;
    };
    typedef list<SIncludeNamedAnnot> TIncludeNamedAnnots;
    TIncludeNamedAnnots m_IncludeNamedAnnots;
#endif  /* NCBI_NAMED_ANNOTS_IMPLEMENTED */
};



/// Data blob unique ID
///
class CPSG_BlobId
{
public:
    /// Mainstream blob ID ctor - from a string ID
    /// @param id
    ///  Blob ID
    CPSG_BlobId(string id) : m_Id(move(id)) {}

    /// Historical blob ID system -- based on the "satellite" and the "key"
    /// inside it. It'll be translated into "<sat>.<sat_key>" string.
    /// @sa  objects::CID2_Blob_Id::TSat, objects::CID2_Blob_Id::TSat_key
    CPSG_BlobId(int sat, int sat_key);

    /// Get the blob ID
    const string& Get() const { return m_Id; }

private:
    string m_Id;
};



/// Request to the PSG server (by blob-id, for a particular blob of data)
///

class CPSG_Request_Blob : public CPSG_Request
{
public:
    /// 
    CPSG_Request_Blob(CPSG_BlobId     blob_id,
                      string          last_modified = {},
                      weak_ptr<void>  user_context = {})
        : CPSG_Request(user_context),
          m_BlobId(blob_id),
          m_LastModified(last_modified)
    {}

    const CPSG_BlobId& GetBlobId()       const { return m_BlobId; }
    const string&      GetLastModified() const { return m_LastModified; }

private:
    CPSG_BlobId m_BlobId;
    string      m_LastModified;
};



/// Retrieval result
/// @sa GetStatus
enum class EPSG_Status {
    eSuccess,       ///< Successfully retrieved
    eInProgress,    ///< Retrieval is not finalized yet, more info may come
    eNotFound,      ///< Not found
    eCanceled,      ///< Request canceled

    /// An error was encountered while trying to send request or to read
    /// and to process the reply.
    /// If PSG server sends a message with severity:
    /// - Error, Critical or Fatal -- this status will be set, and any data
    ///   data in the reply item must be considered invalid; such messages
    ///   will also be logged by the client API with severity Error.
    /// - Trace, Info or Warning -- are considered to be informational, so
    ///   these do NOT affect the status; such messages however will still
    ///   be logged by the client API with the same (T, I or W) severity. 
    eError
};



class CPSG_Reply;



/// A self-containing part of the reply, e.g. a meta-data or a data blob.

class CPSG_ReplyItem
{
public:
    enum EType {
        eBlob,
        eBlobInfo,
        eBioseqInfo,
        eEndOfReply,    ///< No more items expected in the (overall!) reply
    };

    EType GetType() const { return m_Type; }

    /// Get the final result of this blob's retrieval.
    /// If the blob retrieval is not finalized by the deadline, then
    /// "eInProgress" is returned.
    EPSG_Status GetStatus(CDeadline deadline) const;

    /// Unstructured text containing auxiliary info about the result --
    /// such as messages and errors that came from the PSG server or occured
    /// while trying to send request or to read and to process the reply.
    string GetNextMessage() const;

    /// Get the reply that contains this item
    shared_ptr<CPSG_Reply> GetReply() const { return m_Reply; }

    /// Get actual reply item
    template <class TReplyItem>
    TReplyItem* CastTo() { return dynamic_cast<TReplyItem*>(this); }
    template <class TReplyItem>
    const TReplyItem* CastTo() const { return dynamic_cast<const TReplyItem*>(this); }

    virtual ~CPSG_ReplyItem();

protected:
    CPSG_ReplyItem(EType type);

private:
    struct SImpl;
    unique_ptr<SImpl>      m_Impl;
    shared_ptr<CPSG_Reply> m_Reply;
    const EType            m_Type;

    friend class CPSG_Reply;
};



/// Data blob.

class CPSG_Blob : public CPSG_ReplyItem
{
public:
    /// Get blob ID
    const CPSG_BlobId& GetId() const { return m_Id; }

    /// Get the stream from which to read the item's content.
    /// @note  If no content, then reading from the stream will result in EOF.
    istream& GetStream() const { return *m_Stream; }

private:
    CPSG_Blob(CPSG_BlobId id);

    CPSG_BlobId         m_Id;
    unique_ptr<istream> m_Stream;

    friend class CPSG_Reply;
};



/// Data blob meta information
///
/// @note  Most of the data comes from table "BIOSEQ_BLOB" or "ANNOT_BLOB".

class CPSG_BlobInfo : public CPSG_ReplyItem
{
public:
    /// Get blob ID
    CPSG_BlobId GetId() const;

    /// Get data compression algorithm: gzip, bzip2, zip, compress, nlmzip, ...
    /// Return empty string if the blob is not compressed
    string GetCompression() const;

    /// Get data serialization format:  asn.1, asn1-text, json, xml, ...
    string GetFormat() const;

    /// Get content type:  seq-entry, seq-annot, id2s-split-info, id2s-chunk
    string GetContentType() const;

    /// Get blob version (the larger the version the fresher the blob data)
    Uint8 GetVersion() const;

    /// Get size of the blob (as it is stored)
    Uint8 GetStorageSize() const;

    /// Get size of the real (before any compression or encryption) blob data
    Uint8 GetSize() const;

    /// Return TRUE if the blob is "dead"
    bool IsDead() const;

    /// Return TRUE if the blob is "suppressed"
    bool IsSuppressed() const;

    /// Return TRUE if the blob is only temporarily "suppressed"
    bool IsSuppressedTemporarily() const;

    /// Return TRUE if the blob is "withdrawn"
    bool IsWithdrawn() const;

    /// Date when the blob will be released for public use.
    /// If the blob is already released, then return "empty" (IsEmpty()) time
    CTime GetHupReleaseDate() const;

    /// Blob owner's ID
    Uint8 GetOwner() const;

    /// Date when the blob was first loaded into the database
    CTime GetOriginalLoadDate() const;

    /// Class of this blob
    objects::CBioseq_set::EClass GetClass() const;

    /// Internal division value (used by various dumpers)
    string GetDivision() const;

    /// Name of the user who loaded this blob
    string GetUsername() const;

    /// Types of the ID2 split info
    enum ESplitInfo {
        eSplitShell,  ///< Blob that contains shell (skeleton) of the bioseq
        eSplitInfo    ///< Blob that contains split info for the bioseq
    };

    /// Get coordinates of the blob that contains the specified ID2 split info.
    /// If the blob is not split, then return an empty blob id. Also return
    /// empty blob id for the (newer) split blob's "info part" when the latter
    /// is put into the "shell" blob.
    CPSG_BlobId GetSplitInfoBlobId(ESplitInfo split_info_type) const;


#ifdef NCBI_NAMED_ANNOTS_IMPLEMENTED
    /// For the blobs containing (named) annotations, this gives a bird's eye
    /// description about which types of annotations the blob contains,
    /// and where they are located on the bio-sequence
    vector<objects::CID2S_Seq_annot_Info> GetAnnotInfo() const;
#endif  /* NCBI_NAMED_ANNOTS_IMPLEMENTED */

private:
    CPSG_BlobInfo();

    friend class CPSG_Reply;
};



/// Bio-sequence metainfo -- result of the bio-id resolution.
///
/// It can be used to identify which data blobs (related to the requested
/// bio-id retrieval) server is sending right away. It also contains
/// resolution information as well as the information about which
/// other biodata-related blobs are also available on the server and how
/// they can be explicitly requested for later retrieval, if needed.
///
/// @note
///  Most of the data comes from table "BIOSEQ_INFO" and from the named
///  annotation tables.

class CPSG_BioseqInfo : public CPSG_ReplyItem
{
public:
    /// Get canonical bio-id for the bioseq (usually "accession.version")
    CPSG_BioId GetCanonicalId() const;

    /// Get non-canonical bio-ids (aliases) for the bioseq
    vector<CPSG_BioId> GetOtherIds() const;

    /// The bioseq's molecule type (DNA, RNA, protein, etc)
    objects::CSeq_inst::TMol GetMoleculeType() const;

    /// Length of bio-sequence
    Uint8 GetLength() const;

    /// State of bio-sequence
    typedef int TBioseqState;
    TBioseqState GetState() const;

    /// Get coordinates of the TSE blob that contains the bioseq itself
    CPSG_BlobId GetBlobId() const;

    /// Get coordinates of a chunk blob -- from the chunk's serial number.
    /// @throw  If the blob has not been splitted.
    CPSG_BlobId GetChunkBlobId(unsigned split_chunk_no) const;

#ifdef NCBI_NAMED_ANNOTS_IMPLEMENTED
    /// Get list of blobs containing (named) annotations (only those matching
    /// the request's parameters!) for the bioseq.
    vector<CPSG_BlobInfo> GetAnnotBlobs() const;
#endif  /* NCBI_NAMED_ANNOTS_IMPLEMENTED */

    /// Get the bioseq's taxonomy ID
    TTaxId GetTaxId() const;

    /// Get the bioseq's (pre-calculated) hash
    int GetHash() const;

    /// What data is immediately available now. Other data will require
    /// a separate hit to the server.
    /// @sa CPSG_Request_Biodata::IncludeData()
    CPSG_Request_Biodata::TIncludeData IncludedData() const;

private:
    CPSG_BioseqInfo();

    friend class CPSG_Reply;
};



/// PSG reply -- corresponds to a PSG request. It is used to retrieve data 
/// (accession resolution; bio-sequence; annotation blobs) from the storage.
/// 
/// Reply may contain:  
///  - Reply items (CPSG_ReplyItem), each of which in turn may contain
///    item-specific info and/or data blob
///  - Server messages related to the whole reply  
/// 

class CPSG_Reply
{
public:
    /// Get the final result of this whole reply's retrieval.
    /// If the reply retrieval is not finalized by the deadline, then
    /// "eInProgress" is returned.
    EPSG_Status GetStatus(CDeadline deadline) const;

    /// Unstructured text containing auxiliary info about the result --
    /// such as messages and errors that came from the PSG server or occured
    /// while trying to send request or to read and to process the reply.
    string GetNextMessage() const;

    /// Get the request that resulted in this reply
    shared_ptr<const CPSG_Request> GetRequest() const { return m_Request; }

    /// Get the next item which has started arriving from the server.
    /// @note
    ///  Some of the item's data may still be in transit or not even sent
    ///  in by the server yet.
    /// @param deadline
    ///  Until what time to wait for the next item to start coming in.
    /// @return
    ///  - The item objects from which you can start reading data
    ///  - If no more items expected in the reply, the returned item will have
    ///    type eEndOfReply
    ///  - On expired timeout, the returned pointer will be empty (nullptr)
    /// @throw
    ///  If an error has been detected.
    shared_ptr<CPSG_ReplyItem> GetNextItem(CDeadline deadline);

    virtual ~CPSG_Reply();

private:
    CPSG_Reply();

    struct SImpl;
    unique_ptr<SImpl>              m_Impl;
    shared_ptr<const CPSG_Request> m_Request;

    friend class CPSG_Queue;
};



/// A queue to retrieve data (accession resolution info; bio-sequence;
/// annotation blobs) from the storage.
///
/// Call SendRequest() to schedule retrievals (by their bio-ids or
/// blob-ids). Then, call GetNextReply() to get the next reply whose data
/// has started coming in.
///
/// All methods are MT-safe.  Data from different replies can be read in
/// parallel.
///
/// The queue object can be used from more than one thread, either to push
/// requests or to get the incoming ready-to-be-retrieved replies.
///
/// Results for the requests which were pushed into a given instance of
/// the queue will be available for retrieval using this (and only this) queue
/// instance regardless of which threads were used to push the request to the
/// queue.
///
/// If more than one request was pushed into the queue, then the replies to all
/// of the requests may come, in any order.
///

class CPSG_Queue
{
public:
    /// @param service
    ///  Either a name of service (which can be resolved into a set of PSG
    ///  servers) or a single fixed PSG server (in format "host:port")
    CPSG_Queue(const string& service);
    ~CPSG_Queue();

    /// Push request into the queue.
    /// @param request
    ///  The request (containing either bio- or blob-id to retrieve) to send.
    /// @param deadline
    ///  For how long to try to push the request into the queue.
    /// @return
    ///  - TRUE if it succeeds in pushing the request into the queue
    ///  - FALSE on timeout (ie. if cannot do it before the specified deadline)
    /// @throw  CPSG_Exception
    ///  If any (non-timeout) error condition occures.
    /// @sa Get()
    bool SendRequest(shared_ptr<CPSG_Request> request,
                     CDeadline                deadline);


    /// Get the next reply which has started arriving from the server.
    /// @param deadline
    ///  Until what time to wait for the next reply to start coming in.
    /// @return
    ///  - Reply object from which you can obtain particular items.
    ///  - On expired timeout, the returned pointer will be empty (nullptr).
    /// @throw
    ///  If an error has been detected.
    shared_ptr<CPSG_Reply> GetNextReply(CDeadline deadline);


    /// Cancel all requests (not yet sent to the server; waiting for a reply
    /// from the server; or in the middle of the reply data transmission).
    /// Also invalidate all CPSG_ReplyItem objects that have not yet received
    /// all of their data from the server.
    void Reset();


    /// Check whether the queue is empty -- i.e. that there are no:
    ///  - pending requests
    ///  - requests still waiting for a reply from the server
    ///  - replies that still have not been completely retrieved
    bool IsEmpty() const;

private:
    struct SImpl;
    unique_ptr<SImpl> m_Impl;
};


END_NCBI_SCOPE


#endif  /* OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_HPP */
