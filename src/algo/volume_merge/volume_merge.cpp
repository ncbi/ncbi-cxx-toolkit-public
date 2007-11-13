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
 * Author: Anatoliy Kuznetsov, Mike DiCuccio
 *
 * File Description:  Volume Merge
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>
#include <algo/volume_merge/volume_merge.hpp>

BEGIN_NCBI_SCOPE



CMergeVolumes::CMergeVolumes()
: m_Merger(0),
  m_OwnVolumeVect(eTakeOwnership),
  m_Store(0),
  m_MergeKey(0),
  m_MinKey(0)
{
}

CMergeVolumes::~CMergeVolumes()
{
    if (m_OwnVolumeVect == eTakeOwnership) {
        for (size_t i = 0; i < m_VolumeVect.size(); ++i) {
            delete m_VolumeVect[i];
        }
    }
}

void CMergeVolumes::SetMergeAccumulator(IMergeBlob*  merger, 
                                        EOwnership   own)
{
    m_Merger.reset(merger, own);
    if (merger) {
        merger->SetResourcePool(m_BufResourcePool);
    }
}

void CMergeVolumes::SetVolumes(const vector<IMergeVolumeWalker*>& vol_vector,
                               EOwnership                         own)
{
    if (m_OwnVolumeVect == eTakeOwnership) {
        for (size_t i = 0; i < m_VolumeVect.size(); ++i) {
            delete m_VolumeVect[i];
        }
    }
    m_VolumeVect = vol_vector;
    m_OwnVolumeVect = own;
}


void CMergeVolumes::SetMergeStore(IMergeStore*  store,
                                  EOwnership    own)
{
    m_Store.reset(store, own);
    if (store) {
        store->SetResourcePool(m_BufResourcePool);
    }
}

/// @internal
static
IAsyncInterface::EStatus s_GetAsyncStatus(IAsyncInterface* iasync)
{
    return 
        (iasync == 0) ? IAsyncInterface::eReady : iasync->GetStatus();
}


void CMergeVolumes::Run()
{
    _ASSERT(m_Store.get());
    _ASSERT(m_Merger.get());
    _ASSERT(m_VolumeVect.size());

    // initiate volume traverse
    NON_CONST_ITERATE(vector<IMergeVolumeWalker*>, it, m_VolumeVect) {
        (*it)->FetchFirst();
    }

    bool merger_empty = true;
    // volume scan exclude flag vector
    vector<unsigned>  vol_exclude(m_VolumeVect.size());
    // volume end flag vector
    vector<unsigned>  vol_eof(m_VolumeVect.size());
    for (size_t i = 0; i < vol_exclude.size(); ++i) {
        vol_eof[i] = 0;
    }


    // main merge loop
    size_t pending_volumes = m_VolumeVect.size();
    size_t processed_volumes = 0;
    while (pending_volumes != processed_volumes) 
    {
        x_ResetMinKey();
        // initiate exclusion flags to avoid volume re-evaluation
        // while spinning (exclude all eof volumes)
        vol_exclude = vol_eof; 
        bool complete_scan;

        // spin around volumes while not ready
        do {
            complete_scan = true;
            for (size_t i = 0; i < m_VolumeVect.size(); ++i) {
                // check if the volume has already been evaluated
                if (vol_exclude[i]) continue;

                IMergeVolumeWalker* volume = m_VolumeVect[i];
                _ASSERT(volume);
                if (!volume->IsGood()) {
                    NCBI_THROW(CMerge_Exception, eInputVolumeFailure,
                       "Input volume failed. Volume merge cannot recover.");                                        
                    continue;
                }
                if (volume->IsEof()) {
                    ++processed_volumes;
                    vol_exclude[i] = vol_eof[i] = 1;
                    continue;
                }

                IAsyncInterface::EStatus astatus =
                    s_GetAsyncStatus(volume->QueryIAsync());
                switch (astatus)
                {
                case IAsyncInterface::eReady:
                    {
                    Uint4 new_key = volume->GetUint4Key();
                    if (!merger_empty && (new_key == m_MergeKey)) {
                        x_MergeVolume(volume);
                        // merge volume called Fetch, so 
                        // new record is pending for evaluation
                        complete_scan = false;
                        vol_exclude[i] = 0;
                    } else {
                        x_EvaluateMinKey(new_key, i);
                        vol_exclude[i] = 1;
                    } 
                    }
                    break;
                case IAsyncInterface::eFailed:
                    NCBI_THROW(CMerge_Exception, eInputVolumeFailure,
                       "Input volume failed. Volume merge cannot recover.");
                    //++processed_volumes;
                    //vol_exclude[i] = 1;
                    break;
                case IAsyncInterface::eNoMoreData:
                    ++processed_volumes;
                    vol_exclude[i] = vol_eof[i] = 1;
                    continue;
                case IAsyncInterface::eNotReady:
                    complete_scan = false; // something is pending
                    break;
                default:
                    _ASSERT(0);
                } // switch
            } // for each volume
        } while (!complete_scan);

        if (!merger_empty) {
            // since we are out of the volume scan it means the merger
            // has accumulated all current min keys and no more is coming
            // we can store the BLOB now
            x_StoreMerger();
            merger_empty = true;
        }

        // merge next min key value BLOBs
        if (!m_MinKeyCandidates.empty()) {
            x_MergeCandidates();
            m_MinKeyCandidates.resize(0);
            merger_empty = false;
        }

    } // while pending volumes

    if (!merger_empty) {
        x_StoreMerger();
    }

    // closing input-output interfaces
    m_Store->Close();
    for (size_t i = 0; i < m_VolumeVect.size(); ++i) {
        m_VolumeVect[i]->Close();
    }
}

void CMergeVolumes::x_StoreMerger()
{
    IAsyncInterface* iasync = m_Store->QueryIAsync();
    while (iasync) {
        IAsyncInterface::EStatus astatus = iasync->WaitReady();
        switch (astatus) 
        {
        case IAsyncInterface::eReady:
            iasync = 0;
            break;
        case IAsyncInterface::eFailed:
        case IAsyncInterface::eNoMoreData:
            NCBI_THROW(CMerge_Exception, eStoreFailure,
                "Store device failed. Volume merge cannot recover.");
            break;
        case IAsyncInterface::eNotReady:
            break;
        default:
            _ASSERT(0);
        } // switch
    } // while 


    // check if store already has the same BLOB (if yes add to the merger)
    //
    TRawBuffer* store_blob_buffer = m_Store->ReadBlob(m_MergeKey);
    if (store_blob_buffer) {
        m_Merger->Merge(store_blob_buffer);
    }

    // merge
    //
    TRawBuffer* blob_buffer = m_Merger->GetMergeBuffer();
    if (blob_buffer) {
        TBufPoolGuard guard(m_BufResourcePool, blob_buffer);

        m_Merger->Reset();

        if (m_Store->IsGood()) {
            // Place async. store request
            m_Store->Store(m_MergeKey, guard.Release());
        } else {
            NCBI_THROW(CMerge_Exception, eStoreFailure,
                       "Store device failed. Volume merge cannot recover.");
        }
    }
    m_MergeKey = 0;
}

void CMergeVolumes::x_MergeVolume(IMergeVolumeWalker* volume)
{
    size_t buf_size;
    const unsigned char* buf = volume->GetBufferPtr(&buf_size);
    _ASSERT(buf);
    _ASSERT(buf_size);

    // construct detachable buffer
    TRawBuffer* blob_buffer = m_BufResourcePool.Get();
    TBufPoolGuard guard(m_BufResourcePool, blob_buffer);
    blob_buffer->resize(buf_size);
    ::memcpy(&((*blob_buffer)[0]), buf, buf_size);

    // inform the volume that record has been moved to a new location
    volume->SetRecordMoved();

    // place async fetch request
    volume->Fetch();

    // pass the raw buffer to the merger
    m_Merger->Merge(guard.Release());
}

void CMergeVolumes::x_MergeCandidates()
{
    ITERATE(vector<size_t>, it, m_MinKeyCandidates) {
        size_t vol_idx = *it;
        IMergeVolumeWalker* volume = m_VolumeVect[vol_idx];
        x_MergeVolume(volume);
        m_MergeKey = m_MinKey;
    }
}

void CMergeVolumes::x_EvaluateMinKey(unsigned new_key, 
                                     size_t   volume_idx)
{
    if (new_key < m_MinKey) {
        m_MinKey = new_key;
        m_MinKeyCandidates.resize(1);
        m_MinKeyCandidates[0] = volume_idx;
        return;
    }
    if (new_key == m_MinKey) {  // new merge candidate
        m_MinKeyCandidates.push_back(volume_idx);
    }
}
void CMergeVolumes::x_ResetMinKey()
{
    m_MinKey = kMax_UInt;
    m_MinKeyCandidates.resize(0);
}

END_NCBI_SCOPE
