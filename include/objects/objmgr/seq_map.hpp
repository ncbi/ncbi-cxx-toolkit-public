#ifndef SEQ_MAP__HPP
#define SEQ_MAP__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/01/16 16:26:37  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/seq/Seq_data.hpp>
#include <vector>

#include <objects/objmgr1/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Position in the sequence
typedef int TSeqPosition;

// Length of the sequence
typedef int TSeqLength;

// Interval in the sequence
struct SSeqInterval {
    TSeqPosition start;
    TSeqLength   length;
};

// Sequence data
struct SSeqData {
    TSeqLength           length;      /// Length of the sequence data piece
    TSeqPosition         dest_start;  /// Starting pos in the dest. Bioseq
    TSeqPosition         src_start;   /// Starting pos in the source Bioseq
    CConstRef<CSeq_data> src_data;    /// Source sequence data
};


////////////////////////////////////////////////////////////////////
//
//  CSeqMap::
//     Formal sequence map -- to describe sequence parts in general --
//     location and type only, without providing real data
//

class CSeqMap : public CObject
{
public:
    // typedefs
    enum ESegmentType {
        eSeqData,  // real sequence data
        eSeqRef,   // reference to another sequence (if length = 0,
                   // the whole sequence is referenced)
        eSeqGap,   // gap (no data at all)
        eSeqEnd    // last element in every map (pos = sequence length)
    };

    class CSegmentInfo
    {
    public:
        CSegmentInfo(void);
        CSegmentInfo(ESegmentType seg_type, bool minus_strand /*= false*/);
        CSegmentInfo(const CSegmentInfo& seg);

        CSegmentInfo& operator= (const CSegmentInfo& seg);

        bool operator== (const CSegmentInfo& seg) const;
        bool operator<  (const CSegmentInfo& seg) const;

        ESegmentType GetType(void) const;

    private:

        ESegmentType m_SegType;      // Type of map segment
        // Referenced bioseq information
        CBioseqHandle::THandle      m_RefSeq;
        // Seq-data (m_RefPos and m_RefLen must be also set)
        CConstRef<CSeq_data> m_RefData;
        // Referenced location
        TSeqPosition m_RefPos;
        TSeqLength   m_RefLen;
        bool         m_MinusStrand;
        bool         m_Resolved;

        friend class CDataSource;
        friend class CSeqVector;
        friend class CSeqMap;
    };
    typedef pair<TSeqPosition, CSegmentInfo> TSeqSegment;

    // 'ctors
    CSeqMap(void);
    ~CSeqMap(void);

    // Get all intervals
    const TSeqSegment& operator[](int seg_idx) const
        { return m_SeqMap[seg_idx]; }
    const size_t size(void) const { return m_SeqMap.size(); }

    // Add interval to the map.
    // Throw an exception if there is an "equal" interval.
    void Add(const TSeqSegment& interval);
    void Add(TSeqPosition pos,
        ESegmentType seg_type,
        bool minus_strand = false);

private:
    // This should be vector to allow random access
    typedef vector<TSeqSegment> TSeqMap;

    // Get the segment containing point "pos"
    TSeqSegment& x_FindSegment(int pos);
    // Try to resolve segment lengths up to the "pos". Return index of the
    // segment containing "pos".
    TSeqSegment& x_Resolve(int pos, CScope& scope);

    TSeqMap m_SeqMap;
    // Segment lengths are resolved up to this position (not index)
    int m_FirstUnresolvedPos;

    friend class CDataSource;
};


inline
bool operator< (const CSeqMap::TSeqSegment& int1,
                const CSeqMap::TSeqSegment& int2)
{
    // Ignore type, only position is important
    return int1.first < int2.first;
}


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqMap::CSegmentInfo::CSegmentInfo(void)
    : m_SegType(eSeqGap),
      m_RefSeq(0),
      m_RefData(0),
      m_RefPos(0),
      m_RefLen(0),
      m_MinusStrand(false),
      m_Resolved(false)
{
}

inline
CSeqMap::CSegmentInfo::CSegmentInfo(ESegmentType seg_type, bool minus_strand)
    : m_SegType(seg_type),
      m_RefSeq(0),
      m_RefData(0),
      m_RefPos(0),
      m_RefLen(0),
      m_MinusStrand(minus_strand),
      m_Resolved(false)
{
}

inline
CSeqMap::CSegmentInfo::CSegmentInfo(const CSegmentInfo& seg)
    : m_SegType(seg.m_SegType),
      m_RefSeq(seg.m_RefSeq),
      m_RefData(seg.m_RefData),
      m_RefPos(seg.m_RefPos),
      m_RefLen(seg.m_RefLen),
      m_MinusStrand(seg.m_MinusStrand),
      m_Resolved(seg.m_Resolved)
{
}

inline
CSeqMap::CSegmentInfo&
CSeqMap::CSegmentInfo::operator= (const CSegmentInfo& seg)
{
    m_SegType = seg.m_SegType;
    m_RefSeq = seg.m_RefSeq;
    m_RefData = seg.m_RefData;
    m_RefPos = seg.m_RefPos;
    m_RefLen = seg.m_RefLen;
    m_MinusStrand = seg.m_MinusStrand;
    m_Resolved = seg.m_Resolved;
    return *this;
}

inline
bool CSeqMap::CSegmentInfo::operator== (const CSegmentInfo& seg) const
{
    return
        m_SegType == seg.m_SegType  &&
        m_RefSeq == seg.m_RefSeq  &&
        m_RefData == seg.m_RefData  &&
        m_RefPos == seg.m_RefPos  &&
        m_RefLen == seg.m_RefLen  &&
        m_MinusStrand == seg.m_MinusStrand;
    // Do not check m_Resolved
}

inline
bool CSeqMap::CSegmentInfo::operator< (const CSegmentInfo& seg) const
{
    if (m_SegType < seg.m_SegType)
        return true;
    if (m_SegType > seg.m_SegType)
        return false;
    if (m_RefSeq < seg.m_RefSeq)
        return true;
    if (m_RefSeq > seg.m_RefSeq)
        return false;
    if (m_RefData < seg.m_RefData)
        return true;
    if (m_RefData > seg.m_RefData)
        return false;
    if (m_RefPos < seg.m_RefPos)
        return true;
    if (m_RefPos > seg.m_RefPos)
        return false;
    if (m_RefLen < seg.m_RefLen)
        return true;
    if (m_RefLen > seg.m_RefLen)
        return false;
    if (m_MinusStrand < seg.m_MinusStrand)
        return true;
    return false;
}

inline
CSeqMap::ESegmentType CSeqMap::CSegmentInfo::GetType(void) const
{
    return m_SegType;
}

inline
CSeqMap::CSeqMap(void)
    : m_FirstUnresolvedPos(0)
{
    return;
}

inline
CSeqMap::~CSeqMap(void)
{
    return;
}

inline
void CSeqMap::Add(TSeqPosition pos,
                  ESegmentType seg_type,
                  bool minus_strand)
{
    Add(TSeqSegment(pos, CSegmentInfo(seg_type, minus_strand)));
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_MAP__HPP
