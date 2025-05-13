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

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_url.hpp>
#include <corelib/request_ctx.hpp>
#include <connect/services/json_over_uttp.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>

#include <optional>
#include <unordered_map>


#if defined(NCBI_THREADS) && defined(HAVE_LIBNGHTTP2) && defined(HAVE_LIBUV)
#  define HAVE_PSG_CLIENT 1
#endif


#if defined(HAVE_PSG_CLIENT)
BEGIN_NCBI_SCOPE



class CPSG_Exception : public CException
{
public:
    enum EErrCode {
        eTimeout,
        eServerError,
        eInternalError,
        eParameterMissing
    };

    virtual const char* GetErrCodeString(void) const override;

    NCBI_EXCEPTION_DEFAULT(CPSG_Exception, CException);
};



/// Arbitrary request URL arguments
///
struct SPSG_UserArgs : unordered_map<string, set<string>>
{
    /// Allows regular construction.
    /// @code
    /// SPSG_UserArgs args{{ "param1", { "value1", "value2" }}, { "param2", { "value3" }}};
    /// @endcode
    using unordered_map<string, set<string>>::unordered_map;

    /// Allow construction from a CUrlArgs instance or using CUrlArgs parsing.
    /// @code
    /// SPSG_UserArgs args("param1=value1&param1=value2&param2=value3");
    /// @endcode
    SPSG_UserArgs(const CUrlArgs& url_args);
    SPSG_UserArgs(const string& query) : SPSG_UserArgs(CUrlArgs(query)) {}
};



/// Request to the PSG server (see "CPSG_Request_*" below)
///

class CPSG_Request
{
public:
    enum EType {
        eBiodata,
        eResolve,
        eBlob,
        eNamedAnnotInfo,
        eChunk,
        eIpgResolve,
    };

    enum EFlags {
        fExcludeHUP         = (0 << 0),
        fIncludeHUP         = (1 << 0),
        eDefaultFlags       = fExcludeHUP
    };
    DECLARE_SAFE_FLAGS_TYPE(EFlags, TFlags);
    using TOptionalFlags = CNullable<TFlags>;

    /// Get the user-provided context
    template<typename TUserContext>
    shared_ptr<TUserContext> GetUserContext() const
    { return static_pointer_cast<TUserContext>(m_UserContext); }

    /// Get request context
    CRef<CRequestContext> GetRequestContext() const { return m_RequestContext; }

    /// Set request context
    void SetRequestContext(CRef<CRequestContext> request_context);

    /// Get request type
    EType GetType() const { return x_GetType(); }

    // Get request ID
    string GetId() const { return x_GetId(); }

    /// Set flags
    void SetFlags(TOptionalFlags flags) { m_Flags = std::move(flags); }

    /// Set arbitrary URL arguments to add to this request.
    /// @code
    /// request->SetUserArgs("param1=value1&param1=value2&param2=value3");
    /// request->SetUserArgs({{ "param1", { "value1", "value2" }}, { "param2", { "value3" }}});
    /// @endcode
    void SetUserArgs(SPSG_UserArgs user_args) { m_UserArgs = std::move(user_args); }

protected:
    template <class T> T GetCtx(T c) { return c ? c : CDiagContext::GetRequestContext().Clone(); }

    CPSG_Request(shared_ptr<void> user_context = {},
                 CRef<CRequestContext> request_context = {})
        : m_UserContext(user_context),
          m_RequestContext(GetCtx(std::move(request_context)))
    {}

    virtual ~CPSG_Request() = default;

private:
    virtual EType x_GetType() const = 0;
    virtual string x_GetId() const = 0;
    virtual void x_GetAbsPathRef(ostream&) const = 0;

    shared_ptr<void> m_UserContext;
    CRef<CRequestContext> m_RequestContext;
    TOptionalFlags m_Flags;
    SPSG_UserArgs m_UserArgs;

    friend class CPSG_Queue;
};



/// Bio-id (such as accession)
///
class CPSG_BioId
{
public:
    using CSeq_id = objects::CSeq_id;
    using TType = CSeq_id::E_Choice;

    /// @param id
    ///  Bio ID (like accession)
    CPSG_BioId(string id, TType type = {}) : m_Id(std::move(id)), m_Type(type) {}

    /// @param seq_id
    ///  Seq ID
    CPSG_BioId(const CSeq_id& seq_id) : m_Type(seq_id.Which()) { seq_id.GetLabel(&m_Id, CSeq_id::eFastaContent); }

    /// @param seq_id_handle
    ///  Seq ID handle
    CPSG_BioId(const objects::CSeq_id_Handle& seq_id_handle) : CPSG_BioId(*seq_id_handle.GetSeqId()) {}

    /// Get tilde-separated string representation of this bio ID (e.g. for logging)
    string Repr() const;

    /// Get ID
    const string& GetId() const { return m_Id; }

    /// Get type
    TType GetType() const { return m_Type; }

private:
    string m_Id;
    TType  m_Type;
};

using CPSG_BioIds = vector<CPSG_BioId>;



/// Blob data unique ID
///
class CPSG_DataId
{
public:
    virtual ~CPSG_DataId() = default;

    /// Get tilde-separated string representation of this data ID (e.g. for logging)
    virtual string Repr() const = 0;
};



/// Blob unique ID
///
class CPSG_BlobId : public CPSG_DataId
{
public:
    using TLastModified = CNullable<Int8>;

    /// Mainstream blob ID ctor - from a string ID
    /// @param id
    ///  Blob ID
    CPSG_BlobId(string id, TLastModified last_modified = {})
        : m_Id(std::move(id)),
          m_LastModified(std::move(last_modified))
    {}

    /// Historical blob ID system -- based on the "satellite" and the "key"
    /// inside it. It'll be translated into "<sat>.<sat_key>" string.
    /// @sa  objects::CID2_Blob_Id::TSat, objects::CID2_Blob_Id::TSat_key
    CPSG_BlobId(int sat, int sat_key, TLastModified last_modified = {})
        : m_Id(to_string(sat) + "." + to_string(sat_key)),
          m_LastModified(std::move(last_modified))
    {}

    /// Get tilde-separated string representation of this blob ID (e.g. for logging)
    string Repr() const override;

    /// Get ID
    const string& GetId() const { return m_Id; }

    /// Get last modified
    const TLastModified& GetLastModified() const { return m_LastModified; }

private:
    string m_Id;
    TLastModified m_LastModified;
};



/// Chunk unique ID
///
class CPSG_ChunkId : public CPSG_DataId
{
public:
    CPSG_ChunkId(int id2_chunk, string id2_info)
        : m_Id2Chunk(id2_chunk),
          m_Id2Info(std::move(id2_info))
    {}

    /// Get tilde-separated string representation of this chunk ID (e.g. for logging)
    string Repr() const override;

    /// Get ID2 chunk number
    int GetId2Chunk() const { return m_Id2Chunk; }

    /// Get ID2 info
    const string& GetId2Info() const { return m_Id2Info; }

private:
    int m_Id2Chunk;
    string m_Id2Info;
};



/// Whether and how to substitute version-less primary seq-ids with
/// the "more unique" secondary seq-ids
enum class EPSG_AccSubstitution {
    Default,  ///< Substitute always (default)
    Limited,  ///< Substitute only if the resolved record's seq_id_type is GI(12)
    Never     ///< No substitution whatsoever - return exact raw accession info
};



/// Whether to try to resolve provided seq-ids before use
enum class EPSG_BioIdResolution {
    Resolve,    ///< Try to resolve provided seq-ids
    NoResolve,  ///< Use provided seq-ids as is
};



/// Request to the PSG server (by bio-id, for a biodata specific info and data)
///

class CPSG_Request_Biodata : public CPSG_Request
{
public:
    /// @param bio_id
    ///  ID of the bioseq
    /// @param bio_id_resolution
    ///  Whether to try to resolve using provided ID (or use it as-is)
    CPSG_Request_Biodata(CPSG_BioId             bio_id,
                         EPSG_BioIdResolution   bio_id_resolution,
                         shared_ptr<void>       user_context = {},
                         CRef<CRequestContext>  request_context = {})
        : CPSG_Request(std::move(user_context), std::move(request_context)),
          m_BioId(std::move(bio_id)),
          m_BioIdResolution(bio_id_resolution)
    {}

    CPSG_Request_Biodata(CPSG_BioId             bio_id,
                         shared_ptr<void>       user_context = {},
                         CRef<CRequestContext>  request_context = {})
        : CPSG_Request_Biodata(std::move(bio_id), EPSG_BioIdResolution::Resolve, std::move(user_context), std::move(request_context))
    {}

    const CPSG_BioId& GetBioId() const { return m_BioId; }
    EPSG_BioIdResolution GetBioIdResolution() const { return m_BioIdResolution; }

    /// Specify which info and data is needed
    enum EIncludeData {
        /// Server default
        eDefault,

        /// Only the info
        eNoTSE,

        /// If ID2 split is available, return split info blob only.
        /// Otherwise, return no data.
        eSlimTSE,

        /// If ID2 split is available, return split info blob only.
        /// Otherwise, return all Cassandra data chunks of the blob itself.
        eSmartTSE,

        /// If ID2 split is available, return all split blobs.
        /// Otherwise, return all Cassandra data chunks of the blob itself.
        eWholeTSE,

        /// Return all Cassandra data chunks of the blob itself.
        eOrigTSE
    };
    void IncludeData(EIncludeData include) { m_IncludeData = include; }

    EIncludeData GetIncludeData() const { return m_IncludeData; }

    using TExcludeTSEs = vector<CPSG_BlobId>;

    void ExcludeTSE(CPSG_BlobId blob_id) { m_ExcludeTSEs.emplace_back(std::move(blob_id)); }

    const TExcludeTSEs& GetExcludeTSEs() const { return m_ExcludeTSEs; }

    /// Set substitution policy for version-less primary seq-ids
    void SetAccSubstitution(EPSG_AccSubstitution acc_substitution) { m_AccSubstitution = acc_substitution; }

    /// Set resend timeout
    void SetResendTimeout(CTimeout resend_timeout) { m_ResendTimeout = std::move(resend_timeout); }

private:
    EType x_GetType() const override { return eBiodata; }
    string x_GetId() const override { return GetBioId().Repr(); }
    void x_GetAbsPathRef(ostream&) const override;

    CPSG_BioId    m_BioId;
    EPSG_BioIdResolution m_BioIdResolution;
    EIncludeData  m_IncludeData = EIncludeData::eDefault;
    TExcludeTSEs  m_ExcludeTSEs;
    EPSG_AccSubstitution m_AccSubstitution = EPSG_AccSubstitution::Default;
    CTimeout m_ResendTimeout = CTimeout::eDefault;
};



/// Request to the PSG server (by bio-id, for a biodata specific info and data)
///

class CPSG_Request_Resolve : public CPSG_Request
{
public:
    /// @param bio_id
    ///  ID of the bioseq
    /// @param bio_id_resolution
    ///  Whether to try to resolve using provided ID (or use it as-is)
    CPSG_Request_Resolve(CPSG_BioId             bio_id,
                         EPSG_BioIdResolution   bio_id_resolution,
                         shared_ptr<void>       user_context = {},
                         CRef<CRequestContext>  request_context = {})
        : CPSG_Request(std::move(user_context), std::move(request_context)),
          m_BioId(std::move(bio_id)),
          m_BioIdResolution(bio_id_resolution)
    {}

    CPSG_Request_Resolve(CPSG_BioId             bio_id,
                         shared_ptr<void>       user_context = {},
                         CRef<CRequestContext>  request_context = {})
        : CPSG_Request_Resolve(std::move(bio_id), EPSG_BioIdResolution::Resolve, std::move(user_context), std::move(request_context))
    {}

    const CPSG_BioId& GetBioId() const { return m_BioId; }
    EPSG_BioIdResolution GetBioIdResolution() const { return m_BioIdResolution; }

    /// Specify which info and data is needed
    /// Some processors may provide more than the flags prescribe
    /// Always check if corresponding data is actually available using CPSG_BioseqInfo::IncludedInfo()
    /// @sa CPSG_BioseqInfo::IncludedInfo()
    enum EIncludeInfo : unsigned {
        // These flags correspond exactly to the CPSG_BioseqInfo's getters
        fCanonicalId      = (1 << 1),
        fName             = (1 << 2), ///< Requests name to use for canonical bio-id
        fOtherIds         = (1 << 3),
        fMoleculeType     = (1 << 4),
        fLength           = (1 << 5),
        fChainState       = (1 << 6),
        fState            = (1 << 7),
        fBlobId           = (1 << 8),
        fTaxId            = (1 << 9),
        fHash             = (1 << 10),
        fDateChanged      = (1 << 11),
        fGi               = (1 << 12),
        fAllInfo          = numeric_limits<unsigned>::max()
    };
    DECLARE_SAFE_FLAGS_TYPE(EIncludeInfo, TIncludeInfo);
    void IncludeInfo(TIncludeInfo include) { m_IncludeInfo = include; }

    TIncludeInfo      GetIncludeInfo() const { return m_IncludeInfo; }

    /// Set substitution policy for version-less primary seq-ids
    void SetAccSubstitution(EPSG_AccSubstitution acc_substitution) { m_AccSubstitution = acc_substitution; }

private:
    EType x_GetType() const override { return eResolve; }
    string x_GetId() const override { return GetBioId().Repr(); }
    void x_GetAbsPathRef(ostream&) const override;

    CPSG_BioId    m_BioId;
    EPSG_BioIdResolution m_BioIdResolution;
    TIncludeInfo  m_IncludeInfo = TIncludeInfo(0);
    EPSG_AccSubstitution m_AccSubstitution = EPSG_AccSubstitution::Default;
};



/// Request to the PSG server (by blob-id, for a particular blob of data)
///

class CPSG_Request_Blob : public CPSG_Request
{
public:
    /// 
    CPSG_Request_Blob(CPSG_BlobId           blob_id,
                      shared_ptr<void>      user_context = {},
                      CRef<CRequestContext> request_context = {})
        : CPSG_Request(std::move(user_context), std::move(request_context)),
          m_BlobId(std::move(blob_id))
    {}

    const CPSG_BlobId& GetBlobId()       const { return m_BlobId; }

    /// Specify which data is needed (info is always returned)
    using EIncludeData = CPSG_Request_Biodata::EIncludeData;
    void IncludeData(EIncludeData include) { m_IncludeData = include; }

    EIncludeData GetIncludeData() const { return m_IncludeData; }

private:
    EType x_GetType() const override { return eBlob; }
    string x_GetId() const override { return GetBlobId().Repr(); }
    void x_GetAbsPathRef(ostream&) const override;

    CPSG_BlobId  m_BlobId;
    EIncludeData m_IncludeData = EIncludeData::eDefault;
};



/// Request meta-information for the named annotations which are defined on the
/// bioseq
///

class CPSG_Request_NamedAnnotInfo : public CPSG_Request
{
public:
    /// Names of the named annotations
    using TAnnotNames = vector<string>;

    /// @param bio_ids
    ///  IDs (aliases) of the bioseq
    /// @param annot_names
    ///  List of NAs for which to request the metainfo
    /// @param bio_id_resolution
    ///  Whether to try to resolve using provided IDs (or use them as-is)
    CPSG_Request_NamedAnnotInfo(CPSG_BioIds             bio_ids,
                                TAnnotNames             annot_names,
                                EPSG_BioIdResolution    bio_id_resolution,
                                shared_ptr<void>        user_context = {},
                                CRef<CRequestContext>   request_context = {})
        : CPSG_Request(std::move(user_context), std::move(request_context)),
          m_BioIds(std::move(bio_ids)),
          m_AnnotNames(std::move(annot_names)),
          m_BioIdResolution(bio_id_resolution)
    {
        if (m_BioIds.empty()) {
            NCBI_THROW(CPSG_Exception, eParameterMissing, "bio_ids cannot be empty");
        }
    }

    CPSG_Request_NamedAnnotInfo(CPSG_BioIds             bio_ids,
                                TAnnotNames             annot_names,
                                shared_ptr<void>        user_context = {},
                                CRef<CRequestContext>   request_context = {})
        : CPSG_Request_NamedAnnotInfo(std::move(bio_ids), std::move(annot_names), EPSG_BioIdResolution::Resolve, std::move(user_context), std::move(request_context))
    {}

    /// @param bio_id
    ///  ID of the bioseq
    template <class... TArgs>
    CPSG_Request_NamedAnnotInfo(CPSG_BioId bio_id, TArgs&&... args)
        : CPSG_Request_NamedAnnotInfo(CPSG_BioIds{bio_id}, std::forward<TArgs>(args)...)
    {}

    const CPSG_BioId&  GetBioId()      const { return m_BioIds.front(); }
    const CPSG_BioIds& GetBioIds()     const { return m_BioIds;     }
    const TAnnotNames& GetAnnotNames() const { return m_AnnotNames; }
    EPSG_BioIdResolution GetBioIdResolution() const { return m_BioIdResolution; }

    /// Set substitution policy for version-less primary seq-ids
    void SetAccSubstitution(EPSG_AccSubstitution acc_substitution) { m_AccSubstitution = acc_substitution; }

    /// Specify which data is needed (info is always returned)
    using EIncludeData = CPSG_Request_Biodata::EIncludeData;
    void IncludeData(EIncludeData include) { m_IncludeData = include; }

    EIncludeData GetIncludeData() const { return m_IncludeData; }

    /// Scale-limit parameter for SNP annotations
    using ESNPScaleLimit = objects::CSeq_id::ESNPScaleLimit;
    void SetSNPScaleLimit(ESNPScaleLimit snp_scale_limit) { m_SNPScaleLimit = snp_scale_limit; }
    ESNPScaleLimit GetSNPScaleLimit() const { return m_SNPScaleLimit; }

private:
    EType x_GetType() const override { return eNamedAnnotInfo; }
    string x_GetId() const override { return GetBioId().Repr(); }
    void x_GetAbsPathRef(ostream&) const override;

    CPSG_BioIds m_BioIds;
    TAnnotNames m_AnnotNames;
    EPSG_BioIdResolution m_BioIdResolution;
    EPSG_AccSubstitution m_AccSubstitution = EPSG_AccSubstitution::Default;
    EIncludeData m_IncludeData = EIncludeData::eDefault;
    ESNPScaleLimit m_SNPScaleLimit = ESNPScaleLimit::eSNPScaleLimit_Default;
};



/// Request blob data chunk
///

class CPSG_Request_Chunk : public CPSG_Request
{
public:
    CPSG_Request_Chunk(CPSG_ChunkId          chunk_id,
                       shared_ptr<void>      user_context = {},
                       CRef<CRequestContext> request_context = {})
        : CPSG_Request(std::move(user_context), std::move(request_context)),
          m_ChunkId(std::move(chunk_id))
    {}

    const CPSG_ChunkId& GetChunkId() const { return m_ChunkId; }

private:
    EType x_GetType() const override { return eChunk; }
    string x_GetId() const override { return GetChunkId().Repr(); }
    void x_GetAbsPathRef(ostream&) const override;

    CPSG_ChunkId m_ChunkId;
};



/// Request IPG resolve
///

class CPSG_Request_IpgResolve : public CPSG_Request
{
public:
    using TNucleotide = CNullable<string>;

    CPSG_Request_IpgResolve(string                protein,
                            Int8                  ipg = {},
                            TNucleotide           nucleotide = {},
                            shared_ptr<void>      user_context = {},
                            CRef<CRequestContext> request_context = {});

    const string& GetProtein() const { return m_Protein; }
    Int8 GetIpg() const { return m_Ipg; }
    const TNucleotide& GetNucleotide() const { return m_Nucleotide; }

private:
    EType x_GetType() const override { return eIpgResolve; }
    string x_GetId() const override;
    void x_GetAbsPathRef(ostream&) const override;

    string m_Protein;
    Int8 m_Ipg;
    TNucleotide m_Nucleotide;
};



/// Retrieval result
/// @sa GetStatus
enum class EPSG_Status {
    eSuccess,       ///< Successfully retrieved
    eInProgress,    ///< Retrieval is not finalized yet, more info may come
    eNotFound,      ///< Not found
    eCanceled,      ///< Request canceled
    eForbidden,     ///< User is not authorized for the retrieval

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



struct SPSG_Message : string
{
    EDiagSev severity = eDiag_Error;
    optional<int> code;
    explicit operator bool() const { return !empty(); }
};



class CPSG_Reply;



/// A self-containing part of the reply, e.g. a meta-data or a data blob.

class CPSG_ReplyItem
{
public:
    enum EType {
        eBlobData,
        eBlobInfo,
        eSkippedBlob,
        eBioseqInfo,
        eNamedAnnotInfo,
        ePublicComment,
        eProcessor,
        eIpgInfo,
        eNamedAnnotStatus,
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
    /// @param min_severity
    ///  Minimum severity level of messages to be retrieved.
    SPSG_Message GetNextMessage(EDiagSev min_severity = eDiag_Error) const;

    /// Get the reply that contains this item
    shared_ptr<CPSG_Reply> GetReply() const { return m_Reply; }

    /// Get processor ID
    const string& GetProcessorId() { return m_ProcessorId; }

    virtual ~CPSG_ReplyItem();

protected:
    CPSG_ReplyItem(EType type);

private:
    struct SImpl;
    unique_ptr<SImpl>      m_Impl;
    shared_ptr<CPSG_Reply> m_Reply;
    const EType            m_Type;
    string                 m_ProcessorId;

    friend class CPSG_Reply;
};



/// Blob data.

class CPSG_BlobData : public CPSG_ReplyItem
{
public:
    /// Get data ID
    template <class TDataId = CPSG_DataId>
    const TDataId* GetId() const { return dynamic_cast<const TDataId*>(m_Id.get()); }

    /// Get the stream from which to read the item's content.
    /// @note  If no content, then reading from the stream will result in EOF.
    istream& GetStream() const { return *m_Stream; }

private:
    CPSG_BlobData(unique_ptr<CPSG_DataId> id);

    unique_ptr<CPSG_DataId> m_Id;
    unique_ptr<istream> m_Stream;

    friend class CPSG_Reply;
};



/// Blob data meta information

class CPSG_BlobInfo : public CPSG_ReplyItem
{
public:
    /// Get data ID
    template <class TDataId = CPSG_DataId>
    const TDataId* GetId() const { return dynamic_cast<const TDataId*>(m_Id.get()); }

    /// Get data compression algorithm: gzip, bzip2, zip, compress, nlmzip, ...
    /// Return empty string if the blob data is not compressed
    string GetCompression() const;

    /// Get data serialization format:  asn.1, asn1-text, json, xml, ...
    string GetFormat() const;

    /// Get size of the blob data (as it is stored)
    Uint8 GetStorageSize() const;

    /// Get size of the real (before any compression or encryption) blob data
    Uint8 GetSize() const;

    /// Check if the blob data has a special state
    /// @{
    enum EState {
        eDead,
        eSuppressed,
        eSuppressedTemporarily,
        eWithdrawn,
        eWithdrawnBase,
        eWithdrawnPermanently,
        eEditBlocked,
    };
    bool IsState(EState state) const;
    /// @}

    /// Return TRUE if the blob data is "dead"
    bool IsDead() const { return IsState(eDead); }

    /// Return TRUE if the blob data is "suppressed"
    bool IsSuppressed() const { return IsState(eSuppressed); }

    /// Return TRUE if the blob data is "withdrawn"
    bool IsWithdrawn() const { return IsState(eWithdrawn); }

    /// Date when the blob data will be released for public use.
    /// If the blob data is already released, then return "empty" (IsEmpty()) time
    CTime GetHupReleaseDate() const;

    /// Blob data owner's ID
    Uint8 GetOwner() const;

    /// Date when the blob data was first loaded into the database
    CTime GetOriginalLoadDate() const;

    /// Class of this blob data
    objects::CBioseq_set::EClass GetClass() const;

    /// Internal division value (used by various dumpers)
    string GetDivision() const;

    /// Name of the user who loaded this blob data
    string GetUsername() const;

    /// Get ID2 info
    string GetId2Info() const;

    /// Get number of Cassandra data chunks that constitute this blob data.
    Uint8 GetNChunks() const;

private:
    CPSG_BlobInfo(unique_ptr<CPSG_DataId> id);

    unique_ptr<CPSG_DataId> m_Id;
    CJsonNode m_Data;
    mutable optional<Int8> m_Flags;

    friend class CPSG_Reply;
};



/// Skipped blob.

class CPSG_SkippedBlob : public CPSG_ReplyItem
{
public:
    using TSeconds = CNullable<double>;

    enum EReason {
        eExcluded,   // Explicitly excluded by the client
        eInProgress, // Is being sent to the client
        eSent,       // Already sent to the client
        eUnknown,    // Skipped for unknown reason
    };

    /// Get data ID
    template <class TDataId = CPSG_DataId>
    const TDataId* GetId() const { return dynamic_cast<const TDataId*>(m_Id.get()); }

    // Get reason for blob skipping
    EReason GetReason() const { return m_Reason; }

    /// Seconds passed since the blob was last sent to the client.
    const TSeconds& GetSentSecondsAgo() const { return m_SentSecondsAgo; }

    /// Seconds before the blob will be sent to the client.
    const TSeconds& GetTimeUntilResend() const { return m_TimeUntilResend; }

private:
    CPSG_SkippedBlob(unique_ptr<CPSG_DataId> id, EReason reason, TSeconds sent_seconds_ago, TSeconds time_until_resend);

    unique_ptr<CPSG_DataId> m_Id;
    EReason     m_Reason;
    TSeconds    m_SentSecondsAgo;
    TSeconds    m_TimeUntilResend;

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
    CPSG_BioIds GetOtherIds() const;

    /// The bioseq's molecule type (DNA, RNA, protein, etc)
    objects::CSeq_inst::TMol GetMoleculeType() const;

    /// Length of bio-sequence
    Uint8 GetLength() const;

    /// State of the bio-sequence's seq-id
    enum EState {
        eDead     =  0,
        eSought   =  1,
        eReserved =  5,
        eMerged   =  7,
        eLive     = 10
    };
    typedef int TState;  ///< @sa EState

    /// State of the bio-sequence's seq-id chain, i.e. the state of the very
    /// latest seq-id in this bio-sequence's seq-id chain
    TState GetChainState() const;

    /// State of this exact bio-sequence's seq-id.
    /// I.e., for the latest seq-id in a chain it is equal to GetState(), and
    /// for all other seq-ids in a chain it's zero (eDead).
    TState GetState() const;

    /// Get coordinates of the TSE blob that contains the bioseq itself
    CPSG_BlobId GetBlobId() const;

    /// Get the bioseq's taxonomy ID
    TTaxId GetTaxId() const;

    /// Get the bioseq's (pre-calculated) hash
    int GetHash() const;

    /// Date when the bioseq was changed last time
    CTime GetDateChanged() const;

    /// Get GI
    TGi GetGi() const;

    /// What data is immediately available now. Other data will require
    /// a separate hit to the server.
    /// @sa CPSG_Request_Resolve::IncludeInfo()
    CPSG_Request_Resolve::TIncludeInfo IncludedInfo() const;

private:
    CPSG_BioseqInfo();

    CJsonNode m_Data;

    friend class CPSG_Reply;
};



/// Named Annotations (NAs) metainfo -- reply to CPSG_Request_NamedAnnotInfo.
///
/// It can be used to identify where various types of requested NAs are located
/// on the bioseq. It also provides information how to retrieve the
/// corresponding NA data blobs (as needed).

class CPSG_NamedAnnotInfo : public CPSG_ReplyItem
{
public:
    /// Name of the annotation
    const string& GetName() const { return m_Name; }

    /// Coordinates of the blob that contains the NA data
    CPSG_BlobId GetBlobId() const;

    /// Base64 encoded asn.1 of ID2-Seq-annot-Info
    string GetId2AnnotInfo() const;

    /// Detailed ID2-Seq-annot-Info structures (from GetId2AnnotInfo, decoded)
    /// @sa GetId2AnnotInfo
    /// @{
    using TId2AnnotInfo = objects::CID2S_Seq_annot_Info;
    using TId2AnnotInfoList = list<CRef<TId2AnnotInfo>>;
    TId2AnnotInfoList GetId2AnnotInfoList() const;
    /// @}
    
private:
    CPSG_NamedAnnotInfo(string name);

    string     m_Name;
    CJsonNode  m_Data;

    friend class CPSG_Reply;
};



/// Named Annotations (NAs) status -- reply to CPSG_Request_NamedAnnotInfo.
///

class CPSG_NamedAnnotStatus : public CPSG_ReplyItem
{
public:
    /// Individual NA statuses
    using TId2AnnotStatus = pair<string, EPSG_Status>;
    using TId2AnnotStatusList = list<TId2AnnotStatus>;
    TId2AnnotStatusList GetId2AnnotStatusList() const;

private:
    CPSG_NamedAnnotStatus();

    CJsonNode  m_Data;

    friend class CPSG_Reply;
};



/// Public comment

class CPSG_PublicComment : public CPSG_ReplyItem
{
public:
    /// Get data ID for this public comment
    template <class TDataId = CPSG_DataId>
    const TDataId* GetId() const { return dynamic_cast<const TDataId*>(m_Id.get()); }

    /// Get text
    const string& GetText() const { return m_Text; }

private:
    CPSG_PublicComment(unique_ptr<CPSG_DataId> id, string text);

    unique_ptr<CPSG_DataId> m_Id;
    string m_Text;

    friend class CPSG_Reply;
};



/// Processor event

class CPSG_Processor : public CPSG_ReplyItem
{
public:
    enum EProgressStatus {
        eStart,
        eDone,
        eNotFound,
        eCanceled,
        eTimeout,
        eError,
        eUnauthorized,
        eInProgress,
        eUnknown,
    };

    /// Get progress status
    EProgressStatus GetProgressStatus() const { return m_ProgressStatus; }

private:
    CPSG_Processor(EProgressStatus progress_status);

    EProgressStatus m_ProgressStatus;

    friend class CPSG_Reply;
};



/// Ipg info -- result of the IPG resolution.

class CPSG_IpgInfo : public CPSG_ReplyItem
{
public:
    /// Get protein
    string GetProtein() const;

    /// Get Ipg
    Int8 GetIpg() const;

    /// Get nucleotide
    string GetNucleotide() const;

    /// Get taxonomy ID
    TTaxId GetTaxId() const;

    typedef int TState;

    /// Get state
    TState GetGbState() const;

private:
    CPSG_IpgInfo();

    CJsonNode m_Data;

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
    /// @param min_severity
    ///  Minimum severity level of messages to be retrieved.
    SPSG_Message GetNextMessage(EDiagSev min_severity = eDiag_Error) const;

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

    ~CPSG_Reply();

private:
    CPSG_Reply();

    struct SImpl;
    unique_ptr<SImpl>              m_Impl;
    shared_ptr<const CPSG_Request> m_Request;

    friend class CPSG_Queue;
    friend class CPSG_Misc;
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
    /// Creates an uninitialized instance.
    /// It allows to postpone queue initialization until later.
    /// The uninitialized instances can then be initialized using
    /// regular constructor and move assignment operator.
    CPSG_Queue();

    /// @param service
    ///  Either a name of service (which can be resolved into a set of PSG
    ///  servers) or a single fixed PSG server (in format "host:port")
    ///  or empty value to use one from configuration file/environment/default.
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


    /// Push request into the queue and get corresponding reply.
    /// @param request
    ///  The request to send.
    /// @param deadline
    ///  For how long to try to push the request into the queue.
    /// @return
    ///  - Reply object from which you can obtain particular items.
    ///  - On expired timeout, the returned pointer will be empty (nullptr).
    /// @throw  CPSG_Exception
    ///  If any (non-timeout) error condition occures.
    shared_ptr<CPSG_Reply> SendRequestAndGetReply(shared_ptr<CPSG_Request> request,
                                                  CDeadline                deadline);


    /// Wait for events on this queue, on replies returned by this queue and/or
    /// on reply items returned by the replies returned by this queue.
    /// @param deadline
    ///  For how long to wait for events in the queue.
    bool WaitForEvents(CDeadline deadline);


    /// Stop accepting new requests.
    /// All already accepted requests will be processed as usual.
    /// No requests are accepted after the stop.
    void Stop();


    /// Stop accepting new requests and
    /// cancel all requests whose replies have not been returned yet.
    /// No requests are accepted and no replies are returned after the reset.
    void Reset();


    /// Check whether the queue was stopped/reset and is now empty.
    bool IsEmpty() const;


    /// Check whether the queue has been initialized.
    bool IsInitialized() const { return static_cast<bool>(m_Impl); }


    /// Is the queue in a state (possibly temporary) when requests get immediately rejected.
    bool RejectsRequests() const;


    /// Set request flags
    void SetRequestFlags(CPSG_Request::TFlags request_flags);


    /// Set arbitrary URL arguments to add to every request.
    /// @code
    /// queue.SetUserArgs("param1=value1&param1=value2&param2=value3");
    /// queue.SetUserArgs({{ "param1", { "value1", "value2" }}, { "param2", { "value3" }}});
    /// @endcode
    void SetUserArgs(SPSG_UserArgs user_args);


    /// Get an API lock.
    /// Holding this API lock is essential if numerous short-lived queue instances are used.
    /// It prevents an internal I/O implementation (threads, TCP connections, HTTP sessions, etc)
    /// from being destroyed (on destroying last remaining queue instance)
    /// and then re-created (with new queue instance).
    using TApiLock = shared_ptr<void>;
    static TApiLock GetApiLock();


    CPSG_Queue(CPSG_Queue&&);
    CPSG_Queue& operator=(CPSG_Queue&&);

private:
    struct SImpl;
    unique_ptr<SImpl> m_Impl;
};



/// A class derived from the queue class that additionally allows to run event loop.
///
/// Call SendRequest() (possibly from other threads) to add requests to the queue.
/// Call RunOnce() to run the event loop once, use IsEmpty() to check
/// if all processing is complete. Or, call Run() to run the event loop until all
/// processing is complete.
///
/// NOTE: Requests can be added to the queue at any time, including in parallel with
/// calling Run() and RunOnce() methods of the event loop. However, do remember to
/// call Stop() once all required requests have been added to the queue as this will
/// help the event loop to stop promptly, as soon as all submitted requests have
/// been processed.
///
/// Derived (the event loop) part of the instance is not MT-safe,
/// one event loop cannot be run by multiple threads.
/// Base class methods are MT-safe.
///
/// The queue base can be used by more than one thread,
/// usually to add requests and stop the queue after.
/// If needed, it can also be used to wait for events and to manually retrieve replies.
/// Manually retrieved replies will not be processed by the event loop though.
///
/// Results for the requests which were pushed into a given instance
/// will be processed using this (and only this) instance
/// regardless of which threads were used to push the requests.
///
/// If more than one request was pushed into the instance, then the replies to all
/// of the requests may come, in any order.
///

class CPSG_EventLoop : public CPSG_Queue
{
public:
    using TItemComplete  = function<void(EPSG_Status, const shared_ptr<CPSG_ReplyItem>&)>;
    using TReplyComplete = function<void(EPSG_Status, const shared_ptr<CPSG_Reply>&)>;
    using TNewItem       = function<void(const shared_ptr<CPSG_ReplyItem>&)>;

    /// Creates an uninitialized instance.
    /// It allows to postpone queue initialization until later.
    /// The uninitialized instances can then be initialized using
    /// regular constructor and move assignment operator.
    CPSG_EventLoop();

    /// @param service
    ///  Either a name of service (which can be resolved into a set of PSG
    ///  servers) or a single fixed PSG server (in format "host:port")
    ///  or empty value to use one from configuration file/environment/default.
    /// @param item_complete
    ///  Mandatory user callback to call when an item is complete (i.e. not eInProgress)
    /// @param reply_complete
    ///  Mandatory user callback to call when a reply and all its items are complete
    /// @param new_item
    ///  Optional user callback to call when new item arrives
    CPSG_EventLoop(const string&  service,
                   TItemComplete  item_complete,
                   TReplyComplete reply_complete,
                   TNewItem       new_item = nullptr);

    /// Check whether the queue was stopped/reset, is now empty and all processing is complete
    bool IsEmpty() const { return CPSG_Queue::IsEmpty() && m_Replies.empty(); }

    /// Wait once for events in the queue and process any.
    /// @param deadline
    ///  For how long to wait for events in the queue.
    /// @return
    ///  - TRUE if there have been some events
    ///  - FALSE on timeout (i.e. if there have been no events before the specified deadline)
    bool RunOnce(CDeadline deadline);

    /// Process everything in the queue until it's empty or times out.
    /// @param deadline
    ///  For how long to process events in the queue.
    /// @return
    ///  - TRUE if everything has been processed
    ///  - FALSE on timeout (i.e. if it's still not empty before the specified deadline)
    bool Run(CDeadline deadline);

    CPSG_EventLoop(CPSG_EventLoop&&);
    CPSG_EventLoop& operator=(CPSG_EventLoop&&);

private:
    TItemComplete m_ItemComplete;
    TReplyComplete m_ReplyComplete;
    TNewItem m_NewItem;
    list<pair<shared_ptr<CPSG_Reply>, list<shared_ptr<CPSG_ReplyItem>>>> m_Replies;
};



inline void CPSG_Request::SetRequestContext(CRef<CRequestContext> request_context)
{
    m_RequestContext = GetCtx(std::move(request_context));
}

inline bool CPSG_EventLoop::Run(CDeadline deadline)
{
    while (!IsEmpty()) {
        if (!RunOnce(deadline)) {
            return false;
        }
    }

    return true;
}


DECLARE_SAFE_FLAGS(CPSG_Request::EFlags);
DECLARE_SAFE_FLAGS(CPSG_Request_Resolve::EIncludeInfo);

END_NCBI_SCOPE


#endif  /* HAVE_PSG_CLIENT */
#endif  /* OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_HPP */
