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
#include "pending_requests.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi_task/fetch.hpp>
USING_IDBLOB_SCOPE;


class CPendingOperation;
class CProtocolUtils;

// Keeps track of the retrieving something from cassandra
class CCassFetch
{
public:
    CCassFetch() :
        m_FinishedRead(false),
        m_RequestType(eUnknownRequest)
    {}

    virtual ~CCassFetch()
    {}

    virtual void ResetCallbacks(void) = 0;

public:
    CCassBlobWaiter * GetLoader(void);
    void SetReadFinished(void);
    EPendingRequestType GetRequestType(void) const;
    bool ReadFinished(void) const;

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
    EPendingRequestType             m_RequestType;
};



class CCassNamedAnnotFetch : public CCassFetch
{
public:
    CCassNamedAnnotFetch(const SAnnotRequest &  annot_request)
    {
        m_RequestType = eAnnotationRequest;
    }

    CCassNamedAnnotFetch()
    {}

    virtual ~CCassNamedAnnotFetch()
    {}

public:
    virtual void ResetCallbacks(void);
    void SetLoader(CCassNAnnotTaskFetch *  fetch);
    CCassNAnnotTaskFetch *  GetLoader(void);
};


class CCassBlobFetch : public CCassFetch
{
public:
    CCassBlobFetch(const SBlobRequest &  blob_request) :
        m_BlobId(blob_request.m_BlobId),
        m_TSEOption(blob_request.m_TSEOption),
        m_ClientId(blob_request.m_ClientId),
        m_BlobPropSent(false),
        m_BlobIdType(blob_request.GetBlobIdentificationType()),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {
        m_RequestType = eBlobRequest;
    }

    CCassBlobFetch() :
        m_TSEOption(eUnknownTSE),
        m_BlobPropSent(false),
        m_BlobIdType(eBySatAndSatKey),
        m_TotalSentBlobChunks(0),
        m_BlobPropItemId(0),
        m_BlobChunkItemId(0)
    {}

    string GetClientId(void) const
    {
        return m_ClientId;
    }

    virtual ~CCassBlobFetch()
    {}

public:
    bool IsBlobPropStage(void) const;
    size_t GetBlobPropItemId(CProtocolUtils *  protocol_utils);
    size_t GetBlobChunkItemId(CProtocolUtils *  protocol_utils);
    void SetLoader(CCassBlobTaskLoadBlob *  fetch);
    CCassBlobTaskLoadBlob *  GetLoader(void);
    ETSEOption GetTSEOption(void) const;
    EBlobIdentificationType GetBlobIdType(void) const;
    SBlobId GetBlobId(void) const;
    int32_t GetTotalSentBlobChunks(void) const;
    void IncrementTotalSentBlobChunks(void);
    void ResetTotalSentBlobChunks(void);
    void SetBlobPropSent(void);

    virtual void ResetCallbacks(void);

private:
    SBlobId                             m_BlobId;
    ETSEOption                          m_TSEOption;
    string                              m_ClientId;

    bool                                m_BlobPropSent;

    EBlobIdentificationType             m_BlobIdType;
    int32_t                             m_TotalSentBlobChunks;

    size_t                              m_BlobPropItemId;
    size_t                              m_BlobChunkItemId;
};


class CCassBioseqInfoFetch : public CCassFetch
{
public:
    CCassBioseqInfoFetch()
    {
        m_RequestType = eBioseqInfoRequest;
    }

    virtual ~CCassBioseqInfoFetch()
    {}

public:
    virtual void ResetCallbacks(void);
    void SetLoader(CCassBioseqInfoTaskFetch *  fetch);
    CCassBioseqInfoTaskFetch *  GetLoader(void);
};


class CCassSi2csiFetch : public CCassFetch
{
public:
    CCassSi2csiFetch()
    {
        m_RequestType = eSi2csiRequest;
    }

    virtual ~CCassSi2csiFetch()
    {}

public:
    virtual void ResetCallbacks(void);
    void SetLoader(CCassSI2CSITaskFetch *  fetch);
    CCassSI2CSITaskFetch *  GetLoader(void);
};

#endif

