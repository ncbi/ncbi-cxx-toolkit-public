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
* Authors:
*           Aleksey Grichenko
*           Michael Kimelman
*           Andrei Gourianov
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2002/03/27 15:07:53  grichenk
* Fixed CSeqMap::CSegmentInfo::operator==()
*
* Revision 1.9  2002/03/15 18:10:05  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.8  2002/03/08 21:25:30  gouriano
* fixed errors with unresolvable references
*
* Revision 1.7  2002/02/25 21:05:26  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/30 22:08:47  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:37  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/seq_id_handle.hpp>
#include <objects/seq/Seq_data.hpp>
#include <corelib/ncbiobj.hpp>
#include <vector>

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
//  CSeqMap::
//     Formal sequence map -- to describe sequence parts in general --
//     location and type only, without providing real data


class CScope;

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

    class CSegmentInfo : public CObject
    {
    public:
        CSegmentInfo(ESegmentType seg_type,
            TSeqPosition position, TSeqLength length,
            bool minus_strand /*= false*/);
        CSegmentInfo(const CSegmentInfo& seg);
        ~CSegmentInfo(void);

        CSegmentInfo& operator= (const CSegmentInfo& seg);
        bool operator== (const CSegmentInfo& seg) const;
        bool operator<  (const CSegmentInfo& seg) const;

        ESegmentType GetType(void) const;
        TSeqPosition GetPosition(void) const;
        TSeqLength   GetLength(void) const;

    private:
        ESegmentType         m_SegType; // Type of map segment
        TSeqPosition         m_position;
        TSeqLength           m_length;
        // Referenced bioseq information
        CSeq_id_Handle       m_RefSeq;
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

    // 'ctors
    CSeqMap(void);
    CSeqMap(const CSeqMap& another);
    ~CSeqMap(void);

    const size_t size(void) const;
    CSeqMap& operator=(const CSeqMap& another);
    // Get all intervals
    const CSegmentInfo& operator[](int seg_idx) const;

private:
    // Add interval to the map.
    // Throw an exception if there is an "equal" interval.
    void Add(CSegmentInfo& interval);
    void Add(ESegmentType seg_type,
        TSeqPosition position, TSeqLength length,
        bool minus_strand = false);

private:
    // Get the segment containing point "pos"
    int x_FindSegment(int pos);
    // Try to resolve segment lengths up to the "pos". Return index of the
    // segment containing "pos".
    CSegmentInfo x_Resolve(int pos, CScope& scope);
    void x_CalculateSegmentLengths(void);

    vector< CRef<CSegmentInfo> > m_Data;
    // Segment lengths are resolved up to this position (not index)
    int m_FirstUnresolvedPos;

    friend class CDataSource;
};

/////////////////////////////////////////////////////////////////////
//  Inline methods


inline
bool operator< (const CSeqMap::CSegmentInfo& int1,
                const CSeqMap::CSegmentInfo& int2)
{
    // Ignore type, only position is important
    return int1.GetPosition() < int2.GetPosition();
}

/////////////////////////////////////////////////////////////////////
//  CSeqMap::CSegmentInfo: inline methods

inline
CSeqMap::CSegmentInfo::CSegmentInfo(ESegmentType seg_type,
    TSeqPosition position, TSeqLength length, bool minus_strand)
    : m_SegType(seg_type),
      m_position(position),
      m_length(length),
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
      m_position(seg.m_position),
      m_length(seg.m_length),
      m_RefSeq(seg.m_RefSeq),
      m_RefData(seg.m_RefData),
      m_RefPos(seg.m_RefPos),
      m_RefLen(seg.m_RefLen),
      m_MinusStrand(seg.m_MinusStrand),
      m_Resolved(seg.m_Resolved)
{
}

inline
CSeqMap::CSegmentInfo::~CSegmentInfo(void)
{
}

inline
CSeqMap::CSegmentInfo&
CSeqMap::CSegmentInfo::operator= (const CSegmentInfo& seg)
{
    m_SegType  = seg.m_SegType;
    m_position = seg.m_position;
    m_length   = seg.m_length;
    m_RefSeq   = seg.m_RefSeq;
    m_RefData  = seg.m_RefData;
    m_RefPos   = seg.m_RefPos;
    m_RefLen   = seg.m_RefLen;
    m_Resolved = seg.m_Resolved;
    m_MinusStrand = seg.m_MinusStrand;
    return *this;
}

inline
bool CSeqMap::CSegmentInfo::operator== (const CSegmentInfo& seg) const
{
    return
        m_position == seg.m_position &&
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
    //###if (m_RefData < seg.m_RefData)
    //###    return true;
    //###if (m_RefData > seg.m_RefData)
    //###    return false;
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
TSeqPosition CSeqMap::CSegmentInfo::GetPosition(void) const
{
    return m_position;
}

inline
TSeqLength   CSeqMap::CSegmentInfo::GetLength(void) const
{
    return m_length;
}

/////////////////////////////////////////////////////////////////////
//  CSeqMap: inline methods

inline
CSeqMap::CSeqMap(void)
    : m_FirstUnresolvedPos(0)
{
    return;
}

inline
CSeqMap::CSeqMap(const CSeqMap& another)
{
    *this = another;
}

inline
CSeqMap::~CSeqMap(void)
{
    return;
}

inline
const size_t CSeqMap::size(void) const
{
    return m_Data.size();
}

inline
CSeqMap& CSeqMap::operator=(const CSeqMap& another)
{
    if (this != &another)
    {
        m_Data.clear();
        vector< CRef< CSegmentInfo> >::const_iterator it;
        for (it = another.m_Data.begin(); it != another.m_Data.end(); ++it)
        {
            m_Data.push_back( *it);
        }
    }
    m_FirstUnresolvedPos = another.m_FirstUnresolvedPos;
    return *this;
}

inline
const CSeqMap::CSegmentInfo& CSeqMap::operator[](int seg_idx) const
{
    return *m_Data[seg_idx];
}


inline
void CSeqMap::Add(ESegmentType seg_type,
                  TSeqPosition position, TSeqLength length,
                  bool minus_strand)
{
    Add(*(new CSegmentInfo(seg_type, position, length, minus_strand)));
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_MAP__HPP
