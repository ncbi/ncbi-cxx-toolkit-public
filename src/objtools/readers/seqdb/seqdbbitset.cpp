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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbbitset.cpp
/// Implementation for the CSeqDB_BitSet class, a bit vector.

#include <ncbi_pch.hpp>
#include "seqdbbitset.hpp"

BEGIN_NCBI_SCOPE

CSeqDB_BitSet::CSeqDB_BitSet(size_t        start,
                             size_t        end,
                             const TByte * p1,
                             const TByte * p2)
    : m_Start  (start),
      m_End    (end),
      m_Special(eNone)
{
    _ASSERT(eWordShift); // must be 32 or 64
    _ASSERT(TByte(0) < (TByte(-1))); // must be unsigned
    
    // Allocation is guaranteed to zero out the bit memory.
    x_Alloc(end-start);
    
    size_t bytes = m_Bits.size();
    
    while(size_t(p2-p1) < bytes) {
        bytes--;
    }
    
    _ASSERT((eWordBits*m_Bits.size()) >= (bytes*8));
    memcpy(& m_Bits[0], p1, bytes);
}

void CSeqDB_BitSet::SetBit(size_t index)
{
    _ASSERT(m_Special == eNone);
    _ASSERT(index >= m_Start);
    _ASSERT(index < m_End);
    
    index -= m_Start;
    
    size_t vx = index >> eWordShift;
    int wx = index & eWordMask;
    
    _ASSERT(m_Bits.size() > vx);
    m_Bits[vx] |= (TByte(0x80 >> wx));
}

void CSeqDB_BitSet::ClearBit(size_t index)
{
    _ASSERT(m_Special == eNone);
    _ASSERT(index >= m_Start);
    _ASSERT(index < m_End);
    
    index -= m_Start;
    
    size_t vx = index >> eWordShift;
    int wx = index & eWordMask;
    
    _ASSERT(m_Bits.size() > vx);
    m_Bits[vx] &= ~(TByte(0x80 >> wx));
}

bool CSeqDB_BitSet::CheckOrFindBit(size_t & index) const
{
    if (index < m_Start)
        index = m_Start;
    
    if (index >= m_End)
        return false;
    
    if (m_Special == eAllSet) {
        return true;
    }
    
    if (m_Special == eAllClear) {
        return false;
    }
    
    size_t nwords = m_Bits.size();
    size_t ix = index - m_Start;
    size_t vx = ix >> eWordShift;
    size_t vx0 = vx;
    
    while(vx < nwords && ! m_Bits[vx]) {
        vx ++;
    }
    
    if (vx != vx0) {
        ix = (vx << eWordShift);
    }
    
    _ASSERT((ix + m_Start) >= index);
    
    size_t bitcount = m_End - m_Start;
    
    while(ix < bitcount) {
        size_t vx = ix >> eWordShift;
        int wx = ix & eWordMask;
        
        _ASSERT(nwords > vx);
        if (m_Bits[vx] & (TByte(0x80) >> wx))
            break;
        
        ix ++;
    }
    
    if (ix < bitcount) {
        index = (ix + m_Start);
        return true;
    }
    
    return false;
}

void CSeqDB_BitSet::UnionWith(CSeqDB_BitSet & other, bool consume)
{
    if (other.m_Special == eAllClear) {
        // Nothing to do.
        return;
    }
    
    if (m_Special == eAllClear) {
        // Result is just 'other'.
        x_Copy(other, consume);
        return;
    }
    
    // Our all-1s mask covers the other.
    
    if (other.m_Start >= m_Start &&
        other.m_End   <= m_End   &&
        m_Special     == eAllSet) {
        
        return;
    }
    
    // The other all-1s mask covers ours.
    
    if (other.m_Start   <= m_Start &&
        other.m_End     >= m_End   &&
        other.m_Special == eAllSet) {
        
        // Copy is probably better than swap here.
        x_Copy(other, consume);
        return;
    }
    
    // Adjust the range if needed; convert special cases to eNone.
    
    x_Normalize(other.m_Start, other.m_End);
    
    switch(other.m_Special) {
    case eAllSet:
        AssignBitRange(other.m_Start, other.m_End, true);
        break;
        
    case eNone:
        x_CopyBits(other);
        break;
        
    case eAllClear:
        _ASSERT(false);
    }
}

void CSeqDB_BitSet::IntersectWith(CSeqDB_BitSet & other, bool consume)
{
    // All clear cases
    
    if (m_Special == eAllClear) {
        return;
    }
    
    if (other.m_Special == eAllClear) {
        x_Copy(other, consume);
        return;
    }
    
    // All set cases.
    
    if (m_Special == eAllSet && other.m_Special == eAllSet) {
        size_t start = std::max(m_Start, other.m_Start);
        size_t end   = std::min(m_End,   other.m_End);
            
        if (start >= end) {
            // The intersected ranges don't overlap.
            m_Special = eAllClear;
        } else {
            m_Start = start;
            m_End = end;
        }
        return;
    }
    
    if (other.m_Special == eAllSet || m_Special == eAllSet) {
        CSeqDB_BitSet result;
        CSeqDB_BitSet range;
        
        if (m_Special == eAllSet) {
            result.x_Copy(other, consume);
            range.x_Copy(*this, true);
        } else {
            Swap(result);
            range.x_Copy(other, consume);
        }
        
        if (result.m_Start < range.m_Start)
            result.AssignBitRange(result.m_Start, range.m_Start, false);
        
        if (result.m_End > range.m_End)
            result.AssignBitRange(range.m_End, result.m_End, false);
        
        Swap(result);
        
        return;
    }
    
    if ((m_Start         == other.m_Start) &&
        (m_Bits.size()   == other.m_Bits.size()) &&
        (m_Special       == eNone) &&
        (other.m_Special == eNone)) {
        
        size_t i = 0;
        size_t end1 = (m_Bits.size() / sizeof(int)) * sizeof(int);
        size_t end2 = m_Bits.size();
        
        // [ The first while() is only needed in the case of unaligned
        // large-character-array allocation, which probably never
        // happens in practice. ]
        
        while(i != end2 && (i & (sizeof(int)-1))) {
            unsigned char * dst = & m_Bits[i];
            unsigned char * src = & other.m_Bits[i];
            
            *dst &= *src;
            i ++;
        }
        
        while(i != end1) {
            int * dst = (int*)(& m_Bits[i]);
            int * src = (int*)(& other.m_Bits[i]);
            
            *dst &= *src;
            i += sizeof(int);
        }
        
        while(i != end2) {
            unsigned char * dst = & m_Bits[i];
            unsigned char * src = & other.m_Bits[i];
            
            *dst &= *src;
            i ++;
        }
        return;
    }
    
    // Intersection between unaligned or differently size bit sets.
    // Some of these cases could be split off but this is currently
    // not likely to happen in production code.
    
    for(size_t i=0; CheckOrFindBit(i); i++) {
        if (! other.CheckOrFindBit(i)) {
            ClearBit(i);
        }
    }
}

void CSeqDB_BitSet::x_CopyBits(const CSeqDB_BitSet & src, size_t start, size_t end)
{
    for(size_t i = start; src.CheckOrFindBit(i) && i < end; i++) {
        SetBit(i);
    }
}

void CSeqDB_BitSet::x_CopyBits(const CSeqDB_BitSet & src)
{
    for(size_t i=0; src.CheckOrFindBit(i); i++) {
        SetBit(i);
    }
}

void CSeqDB_BitSet::x_Normalize(size_t start, size_t end)
{
    // Note: the "range change" paths are unlikely to be active for
    // SeqDB, and could be improved (i.e. this is not the efficient
    // way to move a range of bits).
    
    if (m_Start > start || m_End < end || m_Special != eNone) {
        CSeqDB_BitSet dup(std::min(m_Start, start),
                          std::max(m_End,   end));
        
        Swap(dup);
        
        switch(m_Special) {
        case eAllClear:
            break;
            
        case eAllSet:
            AssignBitRange(m_Start, m_End, true);
            break;
            
        case eNone:
            x_CopyBits(dup);
            break;
        }
    }
}

void CSeqDB_BitSet::x_Copy(CSeqDB_BitSet & other, bool consume)
{
    if (consume && other.m_Special == eNone) {
        Swap(other);
    } else {
        m_Start   = other.m_Start;
        m_End     = other.m_End;
        m_Special = other.m_Special;
        m_Bits    = other.m_Bits;
    }
}

bool CSeqDB_BitSet::GetBit(size_t index) const
{
    if (m_Special != eNone) {
        return (m_Special == eAllSet) ? true : false;
    }
    
    _ASSERT(index >= m_Start);
    _ASSERT(index < m_End);
    
    index -= m_Start;
    
    size_t vx = index >> eWordShift;
    int wx = index & eWordMask;
    
    _ASSERT(m_Bits.size() > vx);
    return !! (m_Bits[vx] & (TByte(0x80) >> wx));
}

void CSeqDB_BitSet::Swap(CSeqDB_BitSet & other)
{
    std::swap(m_Start,   other.m_Start);
    std::swap(m_End,     other.m_End);
    std::swap(m_Special, other.m_Special);
    other.m_Bits.swap(m_Bits);
}

void CSeqDB_BitSet::AssignBitRange(size_t start, size_t end, bool value)
{
    _ASSERT(start >= m_Start && end <= m_End);
    
    if ((start + eWordBits*3) > end) {
        for(size_t i = start; i < end; i++) {
            AssignBit(i, value);
        }
        return;
    } else {
        size_t i = start - m_Start;
        size_t e = end - m_Start;
        
        while(i & eWordMask) {
            AssignBit(i + m_Start, value);
            i++;
        }
        
        size_t vx = i >> eWordShift,
            evx = e >> eWordShift;
        
        char mask = value ? 0xFF : 0;
        
        memset(& m_Bits[vx], mask, evx-vx);
        
        i = vx << eWordShift;
        
        while(i < e) {
            AssignBit(i + m_Start, value);
            i++;
        }
    }
}

void CSeqDB_BitSet::AssignBit(size_t i, bool value)
{
    if (value) {
        SetBit(i);
    } else {
        ClearBit(i);
    }
}

void CSeqDB_BitSet::Normalize()
{
    if (m_Special != eNone) {
        x_Normalize(m_Start, m_End);
    }
}

END_NCBI_SCOPE

