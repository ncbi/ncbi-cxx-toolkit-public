#ifndef CASS_FETCH__HPP
#define CASS_FETCH__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include "pubseq_gateway_types.hpp"
#include "psgs_request.hpp"
#include "cass_blob_id.hpp"
#include "exclude_blob_cache.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/fetch_split_history.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/tasks.hpp>

USING_IDBLOB_SCOPE;


class CPendingOperation;
class CPSGS_Reply;

// Keeps track of the retrieving something from cassandra
class CCassFetch
{
public:
    CCassFetch() :
        m_FinishedRead(false),
        m_FetchType(ePSGS_UnknownFetch),
        m_Canceled(false),
        m_AutoBlobSkipping(false),
        m_ExcludeBlobCacheUpdated(false)
    {}

    CCassFetch(const string &  client_id,
               bool  auto_blob_skipping,
               const SCass_BlobId &  blob_id) :
        m_FinishedRead(false),
        m_FetchType(ePSGS_UnknownFetch),
        m_Canceled(false),
        m_ClientId(client_id),
        m_AutoBlobSkipping(auto_blob_skipping),
        m_BlobId(blob_id),
        m_ExcludeBlobCacheUpdated(false)
    {}

    virtual ~CCassFetch()
    { RemoveFromExcludeBlobCache(); }

    virtual void ResetCallbacks(void) = 0;
    virtual string Serialize(void) const = 0;

public:
    CCassBlobWaiter * GetLoader(void)
    { return m_Loader.get(); }

    void SetReadFinished(void)
    { m_FinishedRead = true; }

    EPSGS_DbFetchType GetFetchType(void) const
    { return m_FetchType; }

    bool ReadFinished(void) const
    { return m_FinishedRead; }

    bool Canceled(void) const
    { return m_Canceled; }

    void Cancel(void)
    {
        if (m_Loader) {
            if (!m_Canceled) {
                m_Canceled = true;
                m_Loader->Cancel();
            }
        }
    }

    bool IsBlobFetch(void) const
    {
        return m_FetchType == ePSGS_BlobBySeqIdFetch ||
               m_FetchType == ePSGS_BlobBySatSatKeyFetch ||
               m_FetchType == ePSGS_AnnotationBlobFetch;
    }

    SCass_BlobId GetBlobId(void) const
    { return m_BlobId; }

    bool GetAutoBlobSkipping(void) const
    { return m_AutoBlobSkipping; }

    void SetAutoBlobSkipping(bool  value)
    { m_AutoBlobSkipping = value; }

    string GetClientId(void) const
    { return m_ClientId; }

    void RemoveFromExcludeBlobCache(void);
    EPSGS_CacheAddResult AddToExcludeBlobCache(bool &  completed);

    void SetExcludeBlobCacheUpdated(bool  value)
    { m_ExcludeBlobCacheUpdated = value; }

    void SetExcludeBlobCacheCompleted(void);

protected:
    // There are multiple types of the loaders stored here:
    // - CCassBlobTaskLoadBlob to load a blob
    // - CCassNAnnotTaskFetch to load an annotation
    // The loader must be here because it exposes a few generic interfaces
    // including errors
    unique_ptr<CCassBlobWaiter>     m_Loader;

    // Indication that there will be no more data
    bool                            m_FinishedRead;

    // At the time of a x_Peek() there is a last resort error handling.
    // That code receives a pointer to the base class but it needs to know
    // what kind of request it was because an appropriate error message needs
    // to be send. Thus this member is required.
    EPSGS_DbFetchType               m_FetchType;

    bool                            m_Canceled;

    // These fields are only for the blob fetches IsBlobFetches() == true
    string                          m_ClientId;
    bool                            m_AutoBlobSkipping;
    SCass_BlobId                    m_BlobId;
    bool                            m_ExcludeBlobCacheUpdated;
};



class CCassNamedAnnotFetch : public CCassFetch
{
public:
    CCassNamedAnnotFetch(const SPSGS_AnnotRequest &  annot_request)
    {
        m_FetchType = ePSGS_AnnotationFetch;
    }

    CCassNamedAnnotFetch()
    {}

    virtual ~CCassNamedAnnotFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassNamedAnnotFetch";
    }

public:
    void SetLoader(CCassNAnnotTaskFetch *  fetch)
    { m_Loader.reset(fetch); }

    CCassNAnnotTaskFetch * GetLoader(void)
    { return static_cast<CCassNAnnotTaskFetch *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);
};


class CCassAccVerHistoryFetch : public CCassFetch
{
public:
    CCassAccVerHistoryFetch(const SPSGS_AccessionVersionHistoryRequest &  acc_version_history_request)
    {
        m_FetchType = ePSGS_AccVerHistoryFetch;
    }

    CCassAccVerHistoryFetch()
    {}

    virtual ~CCassAccVerHistoryFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassAccVerHistoryFetch";
    }

public:
    void SetLoader(CCassAccVerHistoryTaskFetch *  fetch)
    { m_Loader.reset(fetch); }

    CCassAccVerHistoryTaskFetch * GetLoader(void)
    { return static_cast<CCassAccVerHistoryTaskFetch *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);
};


class CCassBlobFetch : public CCassFetch
{
public:
    CCassBlobFetch(const SPSGS_BlobBySeqIdRequest &  blob_request,
                   const SCass_BlobId &  blob_id) :
        CCassFetch(blob_request.m_ClientId,
                   blob_request.m_AutoBlobSkipping, blob_id),
        m_TSEOption(blob_request.m_TSEOption),
        m_BlobPropSent(false),
        m_UserProvidedBlobId(false),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {
        m_FetchType = ePSGS_BlobBySeqIdFetch;
    }

    CCassBlobFetch(const SPSGS_BlobBySatSatKeyRequest &  blob_request,
                   const SCass_BlobId &  blob_id) :
        CCassFetch(blob_request.m_ClientId, false, blob_id),
        m_TSEOption(blob_request.m_TSEOption),
        m_BlobPropSent(false),
        m_UserProvidedBlobId(true),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {
        m_FetchType = ePSGS_BlobBySatSatKeyFetch;
    }

    CCassBlobFetch(const SPSGS_AnnotRequest &  blob_request,
                   const SCass_BlobId &  blob_id) :
        CCassFetch(blob_request.m_ClientId,
                   blob_request.m_AutoBlobSkipping, blob_id),
        m_TSEOption(blob_request.m_TSEOption),
        m_BlobPropSent(false),
        m_UserProvidedBlobId(true),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {
        // Note: this constructor is for the case when a blob is retrieved for
        // an annotation after an annotation record is received.
        m_FetchType = ePSGS_AnnotationBlobFetch;
    }

    virtual string Serialize(void) const
    {
        return "CCassBlobFetch; BlobId: " + m_BlobId.ToString() +
               " BlobPropSent: " + to_string(m_BlobPropSent) +
               " TotalSentBlobChunks: " + to_string(m_TotalSentBlobChunks);
    }

    // The TSE chunk request is pretty much the same as a blob request
    // - eUnknownTSE is used to skip the blob prop analysis logic
    // - client id is empty so the cache of blobs sent to the user is not used
    // - UserProvidedBlobId == false is used to report a server error 500 in
    //   case the blob props are not found.
    CCassBlobFetch(const SPSGS_TSEChunkRequest &  chunk_request,
                   const SCass_BlobId &  tse_id) :
        CCassFetch("", false, tse_id),
        m_TSEOption(SPSGS_BlobRequestBase::ePSGS_UnknownTSE),
        m_BlobPropSent(false),
        m_UserProvidedBlobId(false),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {
        m_FetchType = ePSGS_TSEChunkFetch;
    }

    CCassBlobFetch() :
        m_TSEOption(SPSGS_BlobRequestBase::ePSGS_UnknownTSE),
        m_BlobPropSent(false),
        m_UserProvidedBlobId(true),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {}

    virtual ~CCassBlobFetch()
    {}

public:
    // string GetClientId(void) const
    // { return m_ClientId; }

    SPSGS_BlobRequestBase::EPSGS_TSEOption GetTSEOption(void) const
    { return m_TSEOption; }

    bool GetUserProvidedBlobId(void) const
    { return m_UserProvidedBlobId; }

    int32_t GetTotalSentBlobChunks(void) const
    { return m_TotalSentBlobChunks; }

    void IncrementTotalSentBlobChunks(void)
    { ++m_TotalSentBlobChunks; }

    void ResetTotalSentBlobChunks(void)
    { m_TotalSentBlobChunks = 0; }

    void SetBlobPropSent(void)
    { m_BlobPropSent = true; }

    // At the time of an error report it needs to be known to what the
    // error message is associated - to blob properties or to blob chunks
    bool IsBlobPropStage(void) const
    { return !m_BlobPropSent; }

    void SetLoader(CCassBlobTaskLoadBlob *  fetch)
    { m_Loader.reset(fetch); }

    CCassBlobTaskLoadBlob *  GetLoader(void)
    { return static_cast<CCassBlobTaskLoadBlob *>(m_Loader.get()); }

public:
    size_t GetBlobPropItemId(CPSGS_Reply *  reply);
    size_t GetBlobChunkItemId(CPSGS_Reply *  reply);

    virtual void ResetCallbacks(void);

private:
    SPSGS_BlobRequestBase::EPSGS_TSEOption  m_TSEOption;
    bool                                    m_BlobPropSent;

    // There are three cases when a blob is requested and depending on which
    // one is used a different error message should be reported (blob
    // properties are not found):
    // 1. The user provided seq_id. In this case the error is a server data
    //    error (500) because the seq_id has been resolved
    // 2. The user provided blob sat & sat_key. In this case it is a client
    //    error (404) because the user provided something unknown
    // 3. The TSE chunk request also should lead to a server error because
    //    the chunks in this request are coming for already resolved split blob
    bool                                    m_UserProvidedBlobId;

    int32_t                                 m_TotalSentBlobChunks;

    size_t                                  m_BlobPropItemId;
    size_t                                  m_BlobChunkItemId;
};


class CCassBioseqInfoFetch : public CCassFetch
{
public:
    CCassBioseqInfoFetch()
    {
        m_FetchType = ePSGS_BioseqInfoFetch;
    }

    virtual ~CCassBioseqInfoFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassBioseqInfoFetch";
    }

public:
    void SetLoader(CCassBioseqInfoTaskFetch *  fetch)
    { m_Loader.reset(fetch); }

    CCassBioseqInfoTaskFetch * GetLoader(void)
    { return static_cast<CCassBioseqInfoTaskFetch *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);
};


class CCassSi2csiFetch : public CCassFetch
{
public:
    CCassSi2csiFetch()
    {
        m_FetchType = ePSGS_Si2csiFetch;
    }

    virtual ~CCassSi2csiFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassSi2csiFetch";
    }

public:
    void SetLoader(CCassSI2CSITaskFetch *  fetch)
    { m_Loader.reset(fetch); }

    CCassSI2CSITaskFetch * GetLoader(void)
    { return static_cast<CCassSI2CSITaskFetch *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);
};


class CCassSplitHistoryFetch : public CCassFetch
{
public:
    CCassSplitHistoryFetch(const SPSGS_TSEChunkRequest &  chunk_request,
                           const SCass_BlobId &  tse_id,
                           int64_t  split_version) :
        m_TSEId(tse_id),
        m_Chunk(chunk_request.m_Id2Chunk),
        m_SplitVersion(split_version),
        m_UseCache(chunk_request.m_UseCache)
    {
        m_FetchType = ePSGS_SplitHistoryFetch;
    }

    virtual ~CCassSplitHistoryFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassSplitHistoryFetch";
    }

public:
    SCass_BlobId  GetTSEId(void) const
    { return m_TSEId; }

    int64_t  GetChunk(void) const
    { return m_Chunk; }

    int64_t  GetSplitVersion(void) const
    { return m_SplitVersion; }

    SPSGS_RequestBase::EPSGS_CacheAndDbUse  GetUseCache(void) const
    { return m_UseCache; }

    void SetLoader(CCassBlobTaskFetchSplitHistory *  fetch)
    { m_Loader.reset(fetch); }

    CCassBlobTaskFetchSplitHistory * GetLoader(void)
    { return static_cast<CCassBlobTaskFetchSplitHistory *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);

private:
    SCass_BlobId                            m_TSEId;
    int64_t                                 m_Chunk;
    int64_t                                 m_SplitVersion;
    SPSGS_RequestBase::EPSGS_CacheAndDbUse  m_UseCache;
};


class CCassPublicCommentFetch : public CCassFetch
{
public:
    enum EPSGS_Identification {
        ePSGS_ByCassBlob,
        ePSGS_ById2
    };

public:
    CCassPublicCommentFetch() :
        m_Identification(ePSGS_ByCassBlob),
        m_LastModified(-1),
        m_Id2Chunk(-1)
    {
        m_FetchType = ePSGS_PublicCommentFetch;
    }

    virtual ~CCassPublicCommentFetch()
    {}

    virtual string Serialize(void) const
    {
        return "CCassPublicCommentFetch";
    }

public:
    EPSGS_Identification  GetIdentification(void) const
    {
        return m_Identification;
    }

    void SetCassBlobIdentification(const SCass_BlobId &  blob_id,
                                   int64_t  last_modified)
    {
        m_Identification = ePSGS_ByCassBlob;
        m_BlobId = blob_id;
        m_LastModified = last_modified;
    }

    void SetId2Identification(int64_t  id2_chunk,
                              const string &  id2_info)
    {
        m_Identification = ePSGS_ById2;
        m_Id2Chunk = id2_chunk;
        m_Id2Info = id2_info;
    }

    SCass_BlobId  GetBlobId(void) const
    { return m_BlobId; }

    int64_t  GetLastModified(void) const
    { return m_LastModified; }

    int64_t  GetId2Chunk(void) const
    { return m_Id2Chunk; }

    string  GetId2Info(void) const
    { return m_Id2Info; }

    void SetLoader(CCassStatusHistoryTaskGetPublicComment *  fetch)
    { m_Loader.reset(fetch); }

    CCassStatusHistoryTaskGetPublicComment * GetLoader(void)
    { return static_cast<CCassStatusHistoryTaskGetPublicComment *>(m_Loader.get()); }

public:
    virtual void ResetCallbacks(void);

private:
    // Selector between two options
    EPSGS_Identification    m_Identification;

    // ePSGS_ByCassBlob
    SCass_BlobId            m_BlobId;
    int64_t                 m_LastModified;

    // ePSGS_ById2
    int64_t                 m_Id2Chunk;
    string                  m_Id2Info;
};




#endif

