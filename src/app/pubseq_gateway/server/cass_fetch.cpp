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


void CCassNamedAnnotFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassNAnnotTaskFetch *      loader =
                    static_cast<CCassNAnnotTaskFetch *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
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


void CCassSi2csiFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassSI2CSITaskFetch *      loader =
                    static_cast<CCassSI2CSITaskFetch *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}


void CCassSplitHistoryFetch::ResetCallbacks(void)
{
    if (m_Loader) {
        CCassBlobTaskFetchSplitHistory *    loader =
            static_cast<CCassBlobTaskFetchSplitHistory *>(m_Loader.get());
        loader->SetConsumeCallback(nullptr);
        loader->SetErrorCB(nullptr);
    }
}

