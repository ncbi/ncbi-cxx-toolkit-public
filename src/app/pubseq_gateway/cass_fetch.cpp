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


#include <ncbi_pch.hpp>

#include "cass_fetch.hpp"
#include "protocol_utils.hpp"



CCassBlobWaiter * CCassFetch::GetLoader(void)
{
    return m_Loader.get();
}


void CCassFetch::SetReadFinished(void)
{
    m_FinishedRead = true;
}


EPendingRequestType CCassFetch::GetRequestType(void) const
{
    return m_RequestType;
}


bool CCassFetch::ReadFinished(void) const
{
    return m_FinishedRead;
}


void CCassNamedAnnotFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassNAnnotTaskFetch *      loader =
                    static_cast<CCassNAnnotTaskFetch *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassNamedAnnotFetch::SetLoader(CCassNAnnotTaskFetch *  fetch)
{
    m_Loader.reset(fetch);
}


CCassNAnnotTaskFetch * CCassNamedAnnotFetch::GetLoader(void)
{
    return static_cast<CCassNAnnotTaskFetch *>(m_Loader.get());
}


bool CCassBlobFetch::IsBlobPropStage(void) const
{
    // At the time of an error report it needs to be known to what the
    // error message is associated - to blob properties or to blob chunks
    return !m_BlobPropSent;
}


size_t CCassBlobFetch::GetBlobPropItemId(CProtocolUtils *  protocol_utils)
{
    if (m_BlobPropItemId == 0)
        m_BlobPropItemId = protocol_utils->GetItemId();
    return m_BlobPropItemId;
}


size_t CCassBlobFetch::GetBlobChunkItemId(CProtocolUtils *  protocol_utils)
{
    if (m_BlobChunkItemId == 0)
        m_BlobChunkItemId = protocol_utils->GetItemId();
    return m_BlobChunkItemId;
}


void CCassBlobFetch::SetLoader(CCassBlobTaskLoadBlob *  fetch)
{
    m_Loader.reset(fetch);
}


CCassBlobTaskLoadBlob *  CCassBlobFetch::GetLoader(void)
{
    return static_cast<CCassBlobTaskLoadBlob *>(m_Loader.get());
}


ETSEOption CCassBlobFetch::GetTSEOption(void) const
{
    return m_TSEOption;
}


EBlobIdentificationType CCassBlobFetch::GetBlobIdType(void) const
{
    return m_BlobIdType;
}


SBlobId CCassBlobFetch::GetBlobId(void) const
{
    return m_BlobId;
}


int32_t CCassBlobFetch::GetTotalSentBlobChunks(void) const
{
    return m_TotalSentBlobChunks;
}


void CCassBlobFetch::IncrementTotalSentBlobChunks(void)
{
    ++m_TotalSentBlobChunks;
}


void CCassBlobFetch::ResetTotalSentBlobChunks(void)
{
    m_TotalSentBlobChunks = 0;
}


void CCassBlobFetch::SetBlobPropSent(void)
{
    m_BlobPropSent = true;
}


void CCassBlobFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBlobTaskLoadBlob *     loader =
                        static_cast<CCassBlobTaskLoadBlob *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr, nullptr);
        loader->SetErrorCB(nullptr);
        loader->SetChunkCallback(nullptr);
    }
}


void CCassBioseqInfoFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBioseqInfoTaskFetch *      loader =
                    static_cast<CCassBioseqInfoTaskFetch *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassBioseqInfoFetch::SetLoader(CCassBioseqInfoTaskFetch *  fetch)
{
    m_Loader.reset(fetch);
}


CCassBioseqInfoTaskFetch * CCassBioseqInfoFetch::GetLoader(void)
{
    return static_cast<CCassBioseqInfoTaskFetch *>(m_Loader.get());
}


void CCassSi2csiFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassSI2CSITaskFetch *      loader =
                    static_cast<CCassSI2CSITaskFetch *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassSi2csiFetch::SetLoader(CCassSI2CSITaskFetch *  fetch)
{
    m_Loader.reset(fetch);
}


CCassSI2CSITaskFetch * CCassSi2csiFetch::GetLoader(void)
{
    return static_cast<CCassSI2CSITaskFetch *>(m_Loader.get());
}

