#if !defined(OBJECTS_OBJMGR___SEQ_MAP__INL)
#define OBJECTS_OBJMGR___SEQ_MAP__INL

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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Inline methods of CSeqMap class.
 *
 */


/////////////////////////////////////////////////////////////////////
//  CSeqMap: inline methods

inline
size_t CSeqMap::x_GetSegmentsCount(void) const
{
    return m_Segments.size() - 1;
}


inline
size_t CSeqMap::x_GetLastEndSegmentIndex(void) const
{
    return m_Segments.size() - 1;
}


inline
size_t CSeqMap::x_GetFirstEndSegmentIndex(void) const
{
    return 0;
}


inline
const CSeqMap::CSegment& CSeqMap::x_GetSegment(size_t index) const
{
    if ( index > x_GetSegmentsCount() ) {
        x_GetSegmentException(index);
    }
    return m_Segments[index];
}


inline
TSeqPos CSeqMap::x_GetSegmentPosition(size_t index, CScope* scope) const
{
    if ( index <= m_Resolved )
        return m_Segments[index].m_Position;
    return x_ResolveSegmentPosition(index, scope);
}


inline
TSeqPos CSeqMap::x_GetSegmentLength(size_t index, CScope* scope) const
{
    TSeqPos length = x_GetSegment(index).m_Length;
    if ( length == kInvalidSeqPos ) {
        length = x_ResolveSegmentLength(index, scope);
    }
    return length;
}


inline
TSeqPos CSeqMap::x_GetSegmentEndPosition(size_t index, CScope* scope) const
{
    return x_GetSegmentPosition(index, scope)+x_GetSegmentLength(index, scope);
}


inline
TSeqPos CSeqMap::GetLength(CScope* scope) const
{
    if (m_SeqLength == kInvalidSeqPos) {
        m_SeqLength = x_GetSegmentPosition(x_GetSegmentsCount(), scope);
    }
    return m_SeqLength;
}


inline
CSeqMap::TMol CSeqMap::GetMol(void) const
{
    return m_Mol;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/09/30 18:36:11  vasilche
 * Added methods to work with end segments.
 *
 * Revision 1.4  2003/06/26 19:47:26  grichenk
 * Added sequence length cache
 *
 * Revision 1.3  2003/06/11 19:32:53  grichenk
 * Added molecule type caching to CSeqMap, simplified
 * coding and sequence type calculations in CSeqVector.
 *
 * Revision 1.2  2003/01/22 20:11:53  vasilche
 * Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
 * CSeqMap_CI now supports resolution and iteration over sequence range.
 * Added several caches to CScope.
 * Optimized CSeqVector().
 * Added serveral variants of CBioseqHandle::GetSeqVector().
 * Tried to optimize annotations iterator (not much success).
 * Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
 *
 * Revision 1.1  2002/12/26 16:39:22  vasilche
 * Object manager class CSeqMap rewritten.
 *
 *
 * ===========================================================================
 */

#endif
