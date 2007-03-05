#ifndef OBJECTS_ALNMGR___ALNPOS_CI__HPP
#define OBJECTS_ALNMGR___ALNPOS_CI__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment position iterator. Uses CAlnMap.
*
*/

#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_XALNMGR_EXPORT CAlnPos_CI
{
public:
    CAlnPos_CI              (const CAlnMap& alnmap, TSeqPos aln_pos = 0);

    CAlnPos_CI&   operator= (const CAlnPos_CI& iter);

    CAlnPos_CI&   operator++(void);
    CAlnPos_CI&   operator--(void);

    DECLARE_OPERATOR_BOOL(m_Valid);

    TSeqPos       GetAlnPos (void) const;
    TSignedSeqPos GetSeqPos (CAlnMap::TNumrow row) const;

private:
    typedef vector<TSignedSeqPos> TSeqStartsCache;

    const CAlnMap&          m_AlnMap;
    TSeqPos                 m_AlnPos;
    TSeqPos                 m_AlnStart;
    TSeqPos                 m_AlnStop;
    mutable TSeqStartsCache m_SeqStartsCache;
    CAlnMap::TNumseg        m_AlnSeg;
    TSeqPos                 m_LDelta;
    TSeqPos                 m_RDelta;
    bool                    m_Valid;
    CAlnMap::TNumrow        m_Anchor;
};



///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////


inline
CAlnPos_CI& CAlnPos_CI::operator++()
{
    _ASSERT(m_Valid);
    if (m_AlnPos < m_AlnStop) {
        m_AlnPos++;
        if (m_RDelta) {
            m_RDelta--;
            m_LDelta++;
        } else {
            // time to move to the next segment
            if (m_Anchor == m_AlnMap.GetAnchor()) {
                m_AlnSeg++;
                m_RDelta = m_AlnMap.GetLen(m_AlnSeg) - 1;
                m_LDelta = 0;
                NON_CONST_ITERATE(TSeqStartsCache, it, m_SeqStartsCache) {
                    *it = -2;
                }
            } else {
                // the anchor has changed =>
                // this iterator is invalid
                m_Valid = false;
            }
        }
    } else {
        m_Valid = false;
    }
    return *this;
}


inline
CAlnPos_CI& CAlnPos_CI::operator--()
{
    _ASSERT(m_Valid);
    if (m_AlnPos > m_AlnStart) {
        m_AlnPos--;
        if (m_LDelta) {
            m_LDelta--;
            m_RDelta++;
        } else {
            // time to move to the next segment
            if (m_Anchor == m_AlnMap.GetAnchor()) {
                m_AlnSeg--;
                m_LDelta = 0;
                m_RDelta = m_AlnMap.GetLen(m_AlnSeg) - 1;
                NON_CONST_ITERATE(TSeqStartsCache, it, m_SeqStartsCache) {
                    *it = -2;
                }
            } else {
                // the anchor has changed =>
                // this iterator is invalid
                m_Valid = false;
            }
        }
    } else {
        m_Valid = false;
    }
    return *this;
}


inline
TSeqPos CAlnPos_CI::GetAlnPos() const
{
    _ASSERT(m_Valid);
    return m_AlnPos;
}


inline
TSignedSeqPos CAlnPos_CI::GetSeqPos(CAlnMap::TNumrow row) const
{
    _ASSERT(m_Valid);
    TSignedSeqPos& cached_seq_start = m_SeqStartsCache[row];
    if (cached_seq_start == -2) {
        // on demand caching
        cached_seq_start = m_AlnMap.GetStart(row, m_AlnSeg);
    }
    if (cached_seq_start == -1) {
        // gap
        return -1;
    } else {
        return cached_seq_start +
            (m_AlnMap.IsPositiveStrand(row) ? m_LDelta : m_RDelta);
    }
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_ALNMGR___ALNPOS_CI__HPP
