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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/alnmgr/alnvec_iterator.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

////////////////////////////////////////////////////////////////////////////////
/// CAlnChunkSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

CAlnChunkSegment::CAlnChunkSegment()
:   m_Reversed(false)
{
}


CAlnChunkSegment::CAlnChunkSegment(CConstRef<TChunk> chunk, bool reversed)
:   m_Chunk(chunk),
    m_Reversed(reversed)
{
}

/*
CAlnChunkSegment& CAlnChunkSegment::operator=(CConstRef<TChunk> chunk)
:   m_Chunk(chunk),
    m_Reversed(reversed)
{
    return *this;
}
*/

void CAlnChunkSegment::Init(CConstRef<TChunk> chunk, bool reversed)
{
    m_Chunk = chunk;
    m_Reversed = reversed;
}


CAlnChunkSegment::operator bool() const
{
    return m_Chunk != NULL;
}


CAlnChunkSegment::TSegTypeFlags CAlnChunkSegment::GetType() const
{
    _ASSERT(m_Chunk);

    TSegTypeFlags type = m_Chunk->IsGap() ? fGap : fAligned;
    if(m_Reversed)
        type |= fReversed;
    return type;
}


const CAlnChunkSegment::TSignedRange& CAlnChunkSegment::GetAlnRange() const
{
    _ASSERT(m_Chunk);

    return m_Chunk->GetAlnRange();
}


const CAlnChunkSegment::TSignedRange& CAlnChunkSegment::GetRange() const
{
    _ASSERT(m_Chunk);

    return m_Chunk->GetRange();
}


////////////////////////////////////////////////////////////////////////////////
/// CAlnVecIterator

CAlnVecIterator::CAlnVecIterator()
:   m_ChunkVec(NULL),
    m_Reversed(false),
    m_ChunkIndex(-1)
{
}


CAlnVecIterator::CAlnVecIterator(const TChunkVec& vec, bool reversed, size_t index)
:   m_ChunkVec(&vec),
    m_Reversed(reversed),
    m_ChunkIndex(index)
{
    if(x_IsValidChunk())    {
        m_Segment.Init((*m_ChunkVec)[m_ChunkIndex], m_Reversed);
    } else {
        m_Segment.Reset();
    }
}


IAlnSegmentIterator*    CAlnVecIterator::Clone() const
{
    return new CAlnVecIterator(*m_ChunkVec, m_Reversed, m_ChunkIndex);
}


CAlnVecIterator::operator bool() const
{
    return m_ChunkVec  &&  m_ChunkIndex >=0  &&  m_ChunkIndex < m_ChunkVec->size();
}


IAlnSegmentIterator& CAlnVecIterator::operator++()
{
    _ASSERT(m_ChunkVec);

    m_ChunkIndex++;
    if(x_IsValidChunk())    {
        m_Segment.Init((*m_ChunkVec)[m_ChunkIndex], m_Reversed);
    } else {
        m_Segment.Reset();
    }
    return *this;
}


bool CAlnVecIterator::operator==(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CAlnVecIterator* aln_vec_it =
            dynamic_cast<const CAlnVecIterator*>(&it);
        return x_Equals(*aln_vec_it);
    }
    return false;
}

bool CAlnVecIterator::operator!=(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CAlnVecIterator* aln_vec_it =
            dynamic_cast<const CAlnVecIterator*>(&it);
        return ! x_Equals(*aln_vec_it);
    }
    return true;
}


const CAlnVecIterator::value_type& CAlnVecIterator::operator*() const
{
    _ASSERT(x_IsValidChunk());

    return m_Segment;
}


const CAlnVecIterator::value_type* CAlnVecIterator::operator->() const
{
    _ASSERT(x_IsValidChunk());

    return &m_Segment;
}


END_NCBI_SCOPE
