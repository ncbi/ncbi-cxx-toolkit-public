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
#include <objtools/data_loaders/genbank/impl/reader_id1_base.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/processors.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Reader_Id1

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


bool CId1ReaderBase::LoadBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    if ( blob.IsLoadedBlob() ) {
        return true;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(blob_id) ) {
        dynamic_cast<const CProcessor_ExtAnnot&>
            (m_Dispatcher->GetProcessor(CProcessor::eType_ExtAnnot))
            .Process(result, blob_id, kMain_ChunkId);
        _ASSERT(blob.IsLoadedBlob());
        return true;
    }

    GetBlob(result, blob_id, kMain_ChunkId);
    _ASSERT(blob.IsLoadedBlob());
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
    CLoadLockBlobVersion blob(result, blob_id);
    if ( blob.IsLoadedBlobVersion() ) {
        return true;
    }

    GetBlobVersion(result, blob_id);
    return true;
}


bool CId1ReaderBase::LoadChunk(CReaderRequestResult& result,
                               const TBlobId& blob_id, TChunkId chunk_id)
{
    _ASSERT(CProcessor_ExtAnnot::IsExtAnnot(blob_id, chunk_id));
    _ASSERT(chunk_id == kDelayedMain_ChunkId);

    CLoadLockBlob blob(result, blob_id, chunk_id);
    _ASSERT(blob.IsLoadedBlob());
    if ( blob.IsLoadedChunk() ) {
        return true;
    }
    
    GetBlob(result, blob_id, chunk_id);
    if ( !blob.IsLoadedChunk() ) {
        CLoadLockSetter setter(blob);
        if ( !setter.IsLoaded() ) {
            ERR_POST_X(1, "ExtAnnot chunk is not loaded: "<<blob_id);
            setter.SetLoaded();
        }
     }
    _ASSERT(blob.IsLoadedChunk());
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


void CId1ReaderBase::CreateExtAnnotBlob_ids(TBlobIds& blob_ids, TIntId gi, int ext_feat)
{
    while ( int bit = GetNextExtAnnotBit(ext_feat) ) {
        CRef<CBlob_id> blob_id(new CBlob_id);
        SetExtAnnotBlob_id(*blob_id, gi, bit);
        blob_ids.push_back(CBlob_Info(blob_id, fBlobHasExtAnnot));
    }
}


int CId1ReaderBase::GetNextExtAnnotBit(int& ext_feat)
{
    int bits = ext_feat;
    int bit = bits & ~(bits-1);
    ext_feat = bits - bit;
    return bit;
}


void CId1ReaderBase::SetExtAnnotBlob_id(CBlob_id& blob_id, TIntId gi, int bit)
{
    blob_id.SetSat(GetAnnotSat(bit));
    blob_id.SetSatKey(Int4(gi));
    blob_id.SetSubSat(Int4((gi>>16)&0xffff0000)|(bit&0xffff));
}


TIntId CId1ReaderBase::GetExtAnnotGi(const CBlob_id& blob_id)
{
    return (TIntId(Int4(blob_id.GetSubSat()&0xffff0000))<<16)|Uint4(blob_id.GetSatKey());
}


int CId1ReaderBase::GetExtAnnotSubSat(const CBlob_id& blob_id)
{
    return blob_id.GetSubSat()&0xffff;
}


END_SCOPE(objects)
END_NCBI_SCOPE
