#ifndef ALGO_BV_MERGE_HPP_
#define ALGO_BV_MERGE_HPP_

/* $Id$
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
 * Author:  Mike DiCuccio, Anatoliy Kuznetsov
 *   
 * File Description: IMergeBlob implementation for bit-vector
 *
 */

#include <algo/volume_merge/volume_merge.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>

#include <vector>

BEGIN_NCBI_SCOPE

/**
    Implementation of merger interface for bitsets.
*/
template<class BV = bm::bvector<> >
class CMergeBitsetBlob : public IMergeBlob
{
public:
    typedef  BV                                 TBitVector;
    typedef  vector<CMergeVolumes::TRawBuffer*> TRawBufferStack;
public:
    CMergeBitsetBlob();
    virtual ~CMergeBitsetBlob();
    virtual void Merge(CMergeVolumes::TRawBuffer* buffer);
    virtual CMergeVolumes::TRawBuffer* GetMergeBuffer();
    virtual void Reset();

protected:
    bm::word_t*      m_TmpBvBlock;  ///< deserialization temp block
    TBitVector       m_TmpBv;       ///< temp bit-vector
    TRawBufferStack  m_BlobStack;   ///< blocks to merge
    
};

template<class BV>
inline
CMergeBitsetBlob<BV>::CMergeBitsetBlob()
{
    m_TmpBvBlock = m_TmpBv.allocate_tempblock();
}

template<class BV>
inline
CMergeBitsetBlob<BV>::~CMergeBitsetBlob()
{
    m_TmpBv.free_tempblock(m_TmpBvBlock);
}

template<class BV>
inline
void CMergeBitsetBlob<BV>::Merge(CMergeVolumes::TRawBuffer* buffer)
{
    m_BlobStack.push_back(buffer);
}

template<class BV>
inline
CMergeVolumes::TRawBuffer* 
CMergeBitsetBlob<BV>::GetMergeBuffer()
{
    _ASSERT(m_BlobStack.size());
    // if it's just one buffer - we have nothing to merge
    // (no reserialization required)
    if (m_BlobStack.size() == 1) {
        CMergeVolumes::TRawBuffer* buf = m_BlobStack[0];
        m_BlobStack.resize(0);
        return buf;
    }
    // OR all buffers into temp vector, return Blobs to the pool
    m_TmpBv.clear(true);
    NON_CONST_ITERATE(TRawBufferStack, it, m_BlobStack) {
        CMergeVolumes::TRawBuffer* buf = (*it);
        const unsigned char* buffer = &((*buf)[0]);
        bm::deserialize(m_TmpBv, buffer, m_TmpBvBlock);
        m_BufResourcePool->Put(buf);
    }
    m_BlobStack.resize(0);

    // serialize
    m_TmpBv.optimize();
    typename TBitVector::statistics st1;
    m_TmpBv.calc_stat(&st1);

    CMergeVolumes::TRawBuffer* sbuf = m_BufResourcePool->Get();
    CMergeVolumes::TBufPoolGuard guard(*m_BufResourcePool, sbuf);

    if (st1.max_serialize_mem > sbuf->size()) {
        sbuf->resize(st1.max_serialize_mem);
    }
    size_t size = bm::serialize(m_TmpBv, &(*sbuf)[0], 
                                m_TmpBvBlock, 
                                bm::BM_NO_BYTE_ORDER);
    sbuf->resize(size);
    return guard.Release();

}

template<class BV>
inline
void CMergeBitsetBlob<BV>::Reset()
{
    NON_CONST_ITERATE(TRawBufferStack, it, m_BlobStack) {
        CMergeVolumes::TRawBuffer* buf = (*it);
        m_BufResourcePool->Put(buf);
    }
    m_BlobStack.resize(0);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/11/27 14:25:56  dicuccio
 * Stripped trailing line feeds
 *
 * Revision 1.1  2006/11/17 07:29:32  kuznets
 * initial revision
 *
 *
 * ===========================================================================
 */

#endif /* ALGO_MERGE_HPP */

