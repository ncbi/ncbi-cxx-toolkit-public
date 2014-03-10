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
 */

#ifndef OBJECTS_SEQTABLE_IMPL_DELTA_CACHE
#define OBJECTS_SEQTABLE_IMPL_DELTA_CACHE

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// delta accumulation support
class NCBI_SEQ_EXPORT CIntDeltaSumCache : public CObject
{
public:
    CIntDeltaSumCache(size_t size); // size of deltas array
    ~CIntDeltaSumCache(void);

    typedef CSeqTable_multi_data TDeltas;
    typedef int TValue;

    TValue GetDeltaSum(const TDeltas& deltas,
                       size_t index);

protected:
    TValue x_GetDeltaSum2(const TDeltas& deltas,
                          size_t block_index,
                          size_t block_offset);

    // size of blocks of deltas for accumulation
    static const size_t kBlockSize;
    
    // accumulated deltas for each block
    // size = (totaldeltas+kBlockSize-1)/kBlockSize
    // m_Blocks[0] = sum of deltas in the 1st block (0..kBlockSize-1)
    // m_Blocks[1] = sum of deltas in first 2 blocks (0..2*kBlockSize-1)
    AutoArray<TValue> m_Blocks;
    // number of calculated entries in m_Block
    size_t m_BlocksFilled;
    
    // cached accumulated sums per delta within a block
    // size = kBlockSize
    // m_CacheBlockInfo[0] = sum of the first delta of cached block
    // m_CacheBlockInfo[1] = sum of first 2 deltas of cached block
    // ...
    AutoArray<TValue> m_CacheBlockInfo;
    // index of the block with cached sums (or size_t(-1))
    size_t m_CacheBlockIndex;
};


// delta accumulation support
class NCBI_SEQ_EXPORT CIndexDeltaSumCache : public CObject
{
public:
    CIndexDeltaSumCache(size_t size); // size of deltas array
    ~CIndexDeltaSumCache(void);

    typedef CSeqTable_sparse_index_Base::TIndexes_delta TDeltas;
    typedef size_t TValue;

    TValue GetDeltaSum(const TDeltas& deltas,
                       size_t index);
    size_t FindDeltaSum(const TDeltas& deltas,
                        TValue find_sum);

protected:
    TValue x_GetDeltaSum2(const TDeltas& deltas,
                          size_t block_index,
                          size_t block_offset);
    size_t x_FindDeltaSum2(const TDeltas& deltas,
                           size_t block_index,
                           TValue find_sum);

    // size of blocks of deltas for accumulation
    static const size_t kBlockSize;
    
    // special values returned from FindDeltaSum()
    static const size_t kInvalidRow;
    static const size_t kBlockTooLow;
    
    // accumulated deltas for each block
    // size = (totaldeltas+kBlockSize-1)/kBlockSize
    // m_Blocks[0] = sum of deltas in the 1st block (0..kBlockSize-1)
    // m_Blocks[1] = sum of deltas in first 2 blocks (0..2*kBlockSize-1)
    AutoArray<TValue> m_Blocks;
    // number of calculated entries in m_Block
    size_t m_BlocksFilled;
    
    // cached accumulated sums per delta within a block
    // size = kBlockSize
    // m_CacheBlockInfo[0] = sum of the first delta of cached block
    // m_CacheBlockInfo[1] = sum of first 2 deltas of cached block
    // ...
    AutoArray<TValue> m_CacheBlockInfo;
    // index of the block with cached sums (or size_t(-1))
    size_t m_CacheBlockIndex;
};


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQTABLE_IMPL_DELTA_CACHE
