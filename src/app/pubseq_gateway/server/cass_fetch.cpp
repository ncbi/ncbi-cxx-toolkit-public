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
#include "psgs_reply.hpp"
#include "pubseq_gateway.hpp"



void CCassFetch::RemoveFromExcludeBlobCache(void)
{
    // Remove the requested blob from cache if necessary

    if (!IsBlobFetch())
        return;

    if (m_ClientId.empty())
        return;

    if (m_ExcludeBlobCacheUpdated) {
        auto *  app = CPubseqGatewayApp::GetInstance();
        app->GetExcludeBlobCache()->Remove(m_ClientId,
                                           m_BlobId.m_Sat,
                                           m_BlobId.m_SatKey);
        // To prevent any updates
        m_ExcludeBlobCacheUpdated = false;
    }
}


EPSGS_CacheAddResult CCassFetch::AddToExcludeBlobCache(bool &  completed)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        cache_result = app->GetExcludeBlobCache()->AddBlobId(
                    m_ClientId, m_BlobId.m_Sat, m_BlobId.m_SatKey, completed);
    if (cache_result == ePSGS_Added)
        m_ExcludeBlobCacheUpdated = true;
    return cache_result;
}


void CCassFetch::SetExcludeBlobCacheCompleted(void)
{
    if (!IsBlobFetch())
        return;

    if (m_ClientId.empty())
        return;

    if (m_ExcludeBlobCacheUpdated) {
        auto *  app = CPubseqGatewayApp::GetInstance();
        app->GetExcludeBlobCache()->SetCompleted(m_ClientId,
                                                 m_BlobId.m_Sat,
                                                 m_BlobId.m_SatKey, true);
        // To prevent any updates
        m_ExcludeBlobCacheUpdated = false;
    }
}


void CCassNamedAnnotFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassNAnnotTaskFetch *      loader =
                    static_cast<CCassNAnnotTaskFetch *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


size_t CCassBlobFetch::GetBlobPropItemId(CPSGS_Reply *  reply)
{
    if (m_BlobPropItemId == 0)
        m_BlobPropItemId = reply->GetItemId();
    return m_BlobPropItemId;
}


size_t CCassBlobFetch::GetBlobChunkItemId(CPSGS_Reply *  reply)
{
    if (m_BlobChunkItemId == 0)
        m_BlobChunkItemId = reply->GetItemId();
    return m_BlobChunkItemId;
}


void CCassBlobFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBlobTaskLoadBlob *     loader =
                        static_cast<CCassBlobTaskLoadBlob *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetErrorCB(nullptr);
        loader->SetChunkCallback(nullptr);
    }
}


void CCassBioseqInfoFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBioseqInfoTaskFetch *      loader =
                    static_cast<CCassBioseqInfoTaskFetch *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassSi2csiFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassSI2CSITaskFetch *      loader =
                    static_cast<CCassSI2CSITaskFetch *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassSplitHistoryFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBlobTaskFetchSplitHistory *    loader =
            static_cast<CCassBlobTaskFetchSplitHistory *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassPublicCommentFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassStatusHistoryTaskGetPublicComment *    loader =
            static_cast<CCassStatusHistoryTaskGetPublicComment *>(m_Loader.get());
        loader->SetDataReadyCB(nullptr);
        loader->SetCommentCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}

