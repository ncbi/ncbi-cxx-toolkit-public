#ifndef __OBJTOOLS_ALNMGR___SPARSE_CI__HPP
#define __OBJTOOLS_ALNMGR___SPARSE_CI__HPP

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

#include <corelib/ncbimisc.hpp>

#include <objtools/alnmgr/sparse_aln.hpp>

BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
/// CSparseSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

class NCBI_XALNMGR_EXPORT CSparseSegment : public  IAlnSegment
{
public:
    CSparseSegment(void);
    virtual operator bool(void) const;
    virtual TSegTypeFlags GetType(void) const;
    virtual const TSignedRange& GetAlnRange(void) const;
    virtual const TSignedRange& GetRange(void) const;

    void Init(TSignedSeqPos aln_from,
              TSignedSeqPos aln_to,
              TSignedSeqPos from,
              TSignedSeqPos to,
              TSegTypeFlags type)
    {
        m_AlnRange.Set(aln_from, aln_to);
        m_RowRange.Set(from, to);
        m_Type = type;
    }

private:
    friend class CSparse_CI;

    TSegTypeFlags m_Type;
    TSignedRange  m_AlnRange;
    TSignedRange  m_RowRange;
};


////////////////////////////////////////////////////////////////////////////////
/// CSparseIterator - IAlnSegmentIterator implementation for CAlnMap::CAlnChunkVec

class NCBI_XALNMGR_EXPORT CSparse_CI : public IAlnSegmentIterator
{
public:
    typedef CPairwise_CI::TSignedRange TSignedRange;
    typedef CSparseAln::TDim           TDim;

    CSparse_CI(void);
    CSparse_CI(const CSparseAln&   anchored,
               TDim                row,
               EFlags              flags);
    CSparse_CI(const CSparseAln&   anchored,
               TDim                row,
               EFlags              flags,
               const TSignedRange& range);
    CSparse_CI(const CSparse_CI& orig);

    virtual ~CSparse_CI();

    virtual IAlnSegmentIterator* Clone(void) const;

    // returns true if iterator points to a valid segment
    virtual operator bool(void) const;

    /// postfix operators are not defined to avoid performance overhead
    virtual IAlnSegmentIterator& operator++(void);

    virtual bool operator==(const IAlnSegmentIterator& it) const;
    virtual bool operator!=(const IAlnSegmentIterator& it) const;

    virtual const value_type& operator*(void) const;
    virtual const value_type* operator->(void) const;

    bool IsAnchorDirect(void) const { return m_AnchorDirect; }

private:
    void x_InitIterator(void);
    void x_InitSegment(void);
    void x_CheckSegment(void);
    void x_NextSegment(void);
    bool x_Equals(const CSparse_CI& other) const;

    EFlags         m_Flags;        // iterating mode
    CSparseSegment m_Segment;
    CConstRef<CAnchoredAln> m_Aln;
    TDim           m_Row;          // Selected row
    TSignedRange   m_TotalRange;   // Total requested range
    CPairwise_CI   m_AnchorIt;     // Anchor iterator
    CPairwise_CI   m_RowIt;        // Selected row iterator
    TSignedRange   m_NextAnchorRg; // Next alignment range on the anchor
    TSignedRange   m_NextRowRg;    // Next alignment range on the selected row
    bool           m_AnchorDirect; // Anchor row direction.
    bool           m_RowDirect;    // Row direction relative to the anchor
};


END_NCBI_SCOPE

#endif  // __OBJTOOLS_ALNMGR___SPARSE_ITERATOR__HPP
