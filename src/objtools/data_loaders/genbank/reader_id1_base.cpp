/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/reader_id1_base.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CId1ReaderBase
/////////////////////////////////////////////////////////////////////////////


CId1ReaderBase::CId1ReaderBase(void)
{
}


CId1ReaderBase::~CId1ReaderBase()
{
}


bool CId1ReaderBase::LoadStringSeq_ids(CReaderRequestResult& /*result*/,
                                       const string& /*seq_id*/)
{
    return false;
}


bool CId1ReaderBase::LoadBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id)
{
    if ( result.IsBlobLoaded(blob_id) ) {
        return true;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        const int chunk_id = CProcessor::kMain_ChunkId;
        CLoadLockBlob blob(result, blob_id);
        if ( !CProcessor::IsLoaded(result, blob_id, chunk_id, blob) ) {
            dynamic_cast<const CProcessor_ExtAnnot&>
                (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
                .Process(result, blob_id, chunk_id);
        }
        _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id, blob));
        return true;
    }

    GetBlob(result, blob_id, CProcessor::kMain_ChunkId);
    _ASSERT(result.IsBlobLoaded(blob_id));
    return true;
}


bool CId1ReaderBase::LoadBlobState(CReaderRequestResult& result,
                                   const TBlobId& blob_id)
{
    CLoadLockBlobState lock(result, blob_id);
    if ( lock.IsLoadedBlobState() ) {
        return true;
    }

    GetBlobState(result, blob_id);
    return true;
}


bool CId1ReaderBase::LoadBlobVersion(CReaderRequestResult& result,
                                     const TBlobId& blob_id)
{
    CLoadLockBlobState blob(result, blob_id);
    if ( blob.IsLoadedBlobVersion() ) {
        return true;
    }

    GetBlobVersion(result, blob_id);
    return true;
}


bool CId1ReaderBase::LoadChunk(CReaderRequestResult& result,
                               const TBlobId& blob_id, TChunkId chunk_id)
{
    if ( !CProcessor_ExtAnnot::IsExtAnnot(blob_id, chunk_id) ) {
        return CReader::LoadChunk(result, blob_id, chunk_id);
    }
    
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob && blob.IsLoaded());
    CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(chunk_id);
    if ( chunk_info.IsLoaded() ) {
        return true;
    }

    CInitGuard init(chunk_info, result);
    if ( init ) {
        try {
            GetBlob(result, blob_id, chunk_id);
            _ASSERT(CProcessor::IsLoaded(result, blob_id, chunk_id,blob));
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.eNoConnection ) {
                return false;
            }
            else {
                throw;
            }
        }
    }
    return true;
}


bool CId1ReaderBase::IsAnnotSat(int sat)
{
    return sat == eSat_ANNOT || sat == eSat_ANNOT_CDD;
}


CId1ReaderBase::ESat CId1ReaderBase::GetAnnotSat(int subsat)
{
    return subsat == eSubSat_CDD? eSat_ANNOT_CDD: eSat_ANNOT;
}


END_SCOPE(objects)
END_NCBI_SCOPE
