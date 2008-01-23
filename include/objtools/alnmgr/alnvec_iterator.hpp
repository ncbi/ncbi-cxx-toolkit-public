#ifndef OBJTOOLS_ALNMGR___ALNVEC_ITERATOR__HPP
#define OBJTOOLS_ALNMGR___ALNVEC_ITERATOR__HPP

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


#include <objtools/alnmgr/aln_explorer.hpp>
#include <objtools/alnmgr/alnmap.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////
/// CAlnChunkSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

class NCBI_XALNMGR_EXPORT CAlnChunkSegment
    :   public  IAlnSegment
{
public:
    typedef  objects::CAlnMap::CAlnChunk TChunk;

    CAlnChunkSegment();
    CAlnChunkSegment(CConstRef<TChunk> chunk, bool reversed);
    //CAlnChunkSegment& operator=(CConstRef<TChunk> chunk);
    void    Init(CConstRef<TChunk> chunk, bool reversed);
    void    Reset() {   m_Chunk.Reset();    }

    virtual operator bool() const;

    virtual TSegTypeFlags GetType() const;
    virtual const TSignedRange&    GetAlnRange() const;
    virtual const TSignedRange&    GetRange() const;


protected:
    CConstRef<TChunk> m_Chunk;
    bool    m_Reversed;
};


////////////////////////////////////////////////////////////////////////////////
/// CAlnVecIterator - IAlnSegmentIterator implementation for CAlnMap::CAlnChunkVec

class NCBI_XALNMGR_EXPORT CAlnVecIterator
    :   public IAlnSegmentIterator
{
public:
    typedef objects::CAlnMap::CAlnChunkVec TChunkVec;

    CAlnVecIterator();
    CAlnVecIterator(const TChunkVec& vec, bool reversed, size_t index = 0);

    virtual IAlnSegmentIterator*    Clone() const;

    // returns true if iterator points to a valid segment
    virtual operator bool() const;

    virtual IAlnSegmentIterator& operator++();

    virtual bool    operator==(const IAlnSegmentIterator& it) const;
    virtual bool    operator!=(const IAlnSegmentIterator& it) const;

    virtual const value_type&  operator*() const;
    virtual const value_type* operator->() const;

protected:
    inline bool x_Equals(const CAlnVecIterator& it) const
    {
        return m_ChunkVec == it.m_ChunkVec  &&  m_ChunkIndex == it.m_ChunkIndex;
    }
    inline bool x_IsValidChunk() const
    {
        return m_ChunkVec  &&  m_ChunkIndex >=0  &&  m_ChunkIndex < m_ChunkVec->size();
    }

protected:
    CConstRef<TChunkVec>  m_ChunkVec;
    bool    m_Reversed;

    int         m_ChunkIndex;
    CAlnChunkSegment    m_Segment;  
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALNVEC_ITERATOR__HPP
