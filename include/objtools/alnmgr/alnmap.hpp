#ifndef OBJECTS_ALNMGR___ALNMAP__HPP
#define OBJECTS_ALNMGR___ALNMAP__HPP

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
*   Interface for examining alignments (of type Dense-seg)
*
*/

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/alnmgr/alnexception.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

// forward declarations
// _none_so_far

class CAlnMap : public CObject
{
    typedef CObject TParent;

public:
    // data types
    typedef unsigned int TSegTypeFlags; // binary OR of ESegTypeFlags
    enum ESegTypeFlags {
        fSeq                     = 0x01,
        fNotAlignedToSeqOnAnchor = 0x02,
        fInsert                  = fSeq | fNotAlignedToSeqOnAnchor,
        fUnalignedOnRight        = 0x04, // unaligned region on the right
        fUnalignedOnLeft         = 0x08,
        fNoSeqOnRight            = 0x10, // maybe gaps on the right but no seq
        fNoSeqOnLeft             = 0x20,
        fEndOnRight              = 0x40, // this is the last segment
        fEndOnLeft               = 0x80,
        // reserved for internal use
        fTypeIsSet            = (TSegTypeFlags) 0x80000000
    };
    
    typedef CDense_seg::TDim      TDim;
    typedef TDim                  TNumrow;
    typedef CRange<TSeqPos>       TRange;
    typedef CRange<TSignedSeqPos> TSignedRange;
    typedef CDense_seg::TNumseg   TNumseg;

    enum EGetChunkFlags {
        fIgnoreUnaligned = 0x01,
        fInsertSameAsSeq = 0x02,
        fDeleteSameAsGap = 0x04,
        fIgnoreAnchor    = fInsertSameAsSeq | fDeleteSameAsGap
    };
    typedef int TGetChunkFlags; // binary OR of EGetChunkFlags

    typedef TNumseg TNumchunk;

    // constructors
    CAlnMap(const CDense_seg& ds);
    CAlnMap(const CDense_seg& ds, TNumrow anchor);

    // destructor
    ~CAlnMap(void);

    // Underlying Dense_seg accessor
    const CDense_seg& GetDenseg(void) const;

    // Dimensions
    TNumseg GetNumSegs(void) const;
    TDim    GetNumRows(void) const;

    // Seq ids
    const CSeq_id& GetSeqId(TNumrow row) const;

    // Strands
    bool IsPositiveStrand(TNumrow row) const;
    bool IsNegativeStrand(TNumrow row) const;
    int  StrandSign      (TNumrow row) const; // returns +/- 1

    // Sequence visible range
    TSignedSeqPos GetSeqAlnStart(TNumrow row) const; //aln coords, strand ignored
    TSignedSeqPos GetSeqAlnStop (TNumrow row) const;
    TSignedRange  GetSeqAlnRange(TNumrow row) const;
    TSignedSeqPos GetSeqStart   (TNumrow row) const; //seq coords, with strand
    TSignedSeqPos GetSeqStop    (TNumrow row) const;  
    TSignedRange  GetSeqRange   (TNumrow row) const;

    // Segment info
    TSignedSeqPos GetStart  (TNumrow row, TNumseg seg, int offset = 0) const;
    TSignedSeqPos GetStop   (TNumrow row, TNumseg seg, int offset = 0) const;
    TSignedRange  GetRange  (TNumrow row, TNumseg seg, int offset = 0) const;
    TSeqPos       GetLen    (             TNumseg seg, int offset = 0) const;
    TSegTypeFlags GetSegType(TNumrow row, TNumseg seg, int offset = 0) const;
    
    TSegTypeFlags GetTypeAtAlnPos(TNumrow row, TSeqPos aln_pos) const;

    static bool IsTypeInsert(TSegTypeFlags type);

    TSeqPos GetInsertedSeqLengthOnRight(TNumrow row, TNumseg seg) const;

    // Alignment segments
    TSeqPos GetAlnStart(TNumseg seg) const;
    TSeqPos GetAlnStop (TNumseg seg) const;
    TSeqPos GetAlnStop (void)        const;

    bool    IsSetAnchor(void)           const;
    TNumrow GetAnchor  (void)           const;
    void    SetAnchor  (TNumrow anchor);
    void    UnsetAnchor(void);

    // get number of
    TNumseg GetNumberOfInsertedSegmentsOnRight(TNumrow row, TNumseg seg) const;
    TNumseg GetNumberOfInsertedSegmentsOnLeft (TNumrow row, TNumseg seg) const;

    //Position mapping funcitons
    TNumseg       GetSeg                 (TSeqPos aln_pos)              const;
    TNumseg       GetRawSeg              (TNumrow row, TSeqPos seq_pos) const;
    TSignedSeqPos GetAlnPosFromSeqPos    (TNumrow row, TSeqPos seq_pos) const;
    TSignedSeqPos GetSeqPosFromAlnPos    (TNumrow for_row,
                                          TSeqPos aln_pos)              const;
    TSignedSeqPos GetSeqPosFromSeqPos    (TNumrow for_row,
                                          TNumrow row, TSeqPos seq_pos) const;
    TSeqPos       GetBestSeqPosFromAlnPos(TNumrow for_row,
                                          TSeqPos aln_pos)              const;

 

    // AlnChunks -- declared here for access to typedefs
    class CAlnChunk;
    class CAlnChunkVec;
    
    // Get a vector of chunks defined by flags in a range of aln coords
    CRef<CAlnChunkVec> GetAlnChunks(TNumrow row, const TSignedRange& range,
                                    TGetChunkFlags flags = 0) const;

    class CAlnChunkVec : public CObject
    {
    public:
        CAlnChunkVec(const CAlnMap& aln_map, TNumrow row)
            : m_AlnMap(aln_map), m_Row(row), m_Size(0) { }

        CConstRef<CAlnChunk> operator[] (TNumchunk i) const;
        TNumchunk            size       (void)        const { return m_Size; };

    private:
#ifdef NCBI_COMPILER_MSVC // kludge
        friend class CAlnMap;
#else
        friend
        CRef<CAlnChunkVec> CAlnMap::GetAlnChunks(TNumrow row,
                                                 const TSignedRange& range,
                                                 TGetChunkFlags flags = 0)
            const;
#endif

        // can only be created by CAlnMap::GetAlnChunks
        CAlnChunkVec(void); 
    
        const CAlnMap&  m_AlnMap;
        TNumrow         m_Row;
        vector<TNumseg> m_Idx;
        TNumchunk       m_Size;
        TSeqPos         m_LeftDelta;
        TSeqPos         m_RightDelta;
    };

    class CAlnChunk : public CObject
    {
    public:    
        TSegTypeFlags GetType(void) const { return m_TypeFlags; }
        CAlnChunk&    SetType(TSegTypeFlags type_flags)
            { m_TypeFlags = type_flags; return *this; }
        
        const TSignedRange& GetRange(void) const { return m_SignedRange; }
        bool                IsGap   (void) const
            { return m_SignedRange.GetFrom() < 0; }
        
    private:
        // can only be created or modified by
        friend CConstRef<CAlnChunk> CAlnChunkVec::operator[](TNumchunk i)
            const;
        CAlnChunk(void) {}
        TSignedRange& SetRange(void) { return m_SignedRange; }

        TSegTypeFlags m_TypeFlags;
        TSignedRange  m_SignedRange;
    };


protected:
    class CNumSegWithOffset
    {
    public:
        CNumSegWithOffset(TNumseg seg, int offset = 0)
            : m_AlnSeg(seg), m_Offset(offset) { }

        TNumseg GetAlnSeg(void) const { return m_AlnSeg; };
        int     GetOffset(void) const { return m_Offset; };
        
    private:
        TNumseg m_AlnSeg;
        int     m_Offset;
    };

    // Prohibit copy constructor and assignment operator
    CAlnMap(const CAlnMap& value);
    CAlnMap& operator=(const CAlnMap& value);

    friend CConstRef<CAlnChunk> CAlnChunkVec::operator[](TNumchunk i) const;

    // internal functions for handling alignment segments
    void              x_CreateAlnStarts (void);
    TSegTypeFlags     x_GetRawSegType   (TNumrow row, TNumseg seg) const;
    TSegTypeFlags     x_SetRawSegType   (TNumrow row, TNumseg seg) const;
    CNumSegWithOffset x_GetSegFromRawSeg(TNumseg seg)              const;
    TNumseg           x_GetRawSegFromSeg(TNumseg seg)              const;

    bool x_RawSegTypeDiffNextSegType(TNumrow row, TNumseg seg,
                                     TGetChunkFlags flags) const;


    CConstRef<CDense_seg>           m_DS;
    TNumrow                         m_Anchor;
    vector<TNumseg>                 m_AlnSegIdx;
    CDense_seg::TStarts             m_AlnStarts;
    vector<CNumSegWithOffset>       m_NumSegWithOffsets;
    mutable vector<TSegTypeFlags> * m_RawSegTypes;
};



///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CAlnMap::CAlnMap(const CDense_seg& ds) 
    : m_DS(&ds), m_Anchor(-1), m_RawSegTypes(0)
{
    x_CreateAlnStarts();
}


inline
CAlnMap::CAlnMap(const CDense_seg& ds, TNumrow anchor)
    : m_DS(&ds), m_Anchor(-1), m_RawSegTypes(0)
{
    SetAnchor(anchor);
}


inline
CAlnMap::~CAlnMap(void)
{
    if (m_RawSegTypes) {
        delete m_RawSegTypes;
    }
}


inline
const CDense_seg& CAlnMap::GetDenseg() const
{
    return *m_DS;
}


inline TSeqPos CAlnMap::GetAlnStart(TNumseg seg) const
{
    return m_AlnStarts[seg];
}


inline
TSeqPos CAlnMap::GetAlnStop(TNumseg seg) const
{
    return m_AlnStarts[seg] + m_DS->GetLens()[x_GetRawSegFromSeg(seg)] - 1;
}


inline
TSeqPos CAlnMap::GetAlnStop(void) const
{
    return GetAlnStop(GetNumSegs() - 1);
}


inline 
CAlnMap::TSegTypeFlags 
CAlnMap::GetSegType(TNumrow row, TNumseg seg, int offset) const
{
    return x_GetRawSegType(row, x_GetRawSegFromSeg(seg) + offset);
}


inline
CAlnMap::TNumseg CAlnMap::GetNumSegs(void) const
{
    return IsSetAnchor() ? m_AlnSegIdx.size() : m_DS->GetNumseg();
}


inline
CAlnMap::TDim CAlnMap::GetNumRows(void) const
{
    return m_DS->GetDim();
}


inline
bool CAlnMap::IsSetAnchor(void) const
{
    return m_Anchor >= 0;
}

inline
CAlnMap::TNumrow CAlnMap::GetAnchor(void) const
{
    return m_Anchor;
}



inline
CAlnMap::CNumSegWithOffset
CAlnMap::x_GetSegFromRawSeg(TNumseg raw_seg) const
{
    return IsSetAnchor() ? m_NumSegWithOffsets[raw_seg] : raw_seg;
}


inline
CAlnMap::TNumseg
CAlnMap::x_GetRawSegFromSeg(TNumseg seg) const
{
    return IsSetAnchor() ? m_AlnSegIdx[seg] : seg;
}


inline
int CAlnMap::StrandSign(TNumrow row) const
{
    return IsPositiveStrand(row) ? 1 : -1;
}


inline
bool CAlnMap::IsPositiveStrand(TNumrow row) const
{
    const CDense_seg::TStrands& strands = m_DS->GetStrands();
    return (strands.empty()  ||  strands[row] != eNa_strand_minus);
}


inline
bool CAlnMap::IsNegativeStrand(TNumrow row) const
{
    return ! IsPositiveStrand(row);
}


inline
TSignedSeqPos CAlnMap::GetStart(TNumrow row, TNumseg seg, int offset) const
{
    return m_DS->GetStarts()
        [(x_GetRawSegFromSeg(seg) + offset) * m_DS->GetDim() + row];
}

inline
TSeqPos CAlnMap::GetLen(TNumseg seg, int offset) const
{
    return m_DS->GetLens()[x_GetRawSegFromSeg(seg) + offset];
}


inline
TSignedSeqPos CAlnMap::GetStop(TNumrow row, TNumseg seg, int offset) const
{
    TSignedSeqPos start = GetStart(row, seg, offset);
    return ((start > -1) ? (start + (TSignedSeqPos)GetLen(seg, offset) - 1)
            : -1);
}


inline
const CSeq_id& CAlnMap::GetSeqId(TNumrow row) const
{
    return *(m_DS->GetIds()[row]);
}


inline 
CAlnMap::TSignedRange
CAlnMap::GetRange(TNumrow row, TNumseg seg, int offset) const
{
    TSignedSeqPos start = GetStart(row, seg, offset);
    if (start > -1) {
        return TSignedRange(start, start + GetLen(seg, offset) - 1);
    } else {
        return TSignedRange(-1, -1);
    }
}


inline
CAlnMap::TSignedRange CAlnMap::GetSeqRange(TNumrow row) const
{
    return TSignedRange(GetSeqStart(row), GetSeqStop(row));
}


inline
CAlnMap::TSignedRange CAlnMap::GetSeqAlnRange(TNumrow row) const
{
    return TSignedRange(GetSeqAlnStart(row), GetSeqAlnStop(row));
}


inline
TSeqPos CAlnMap::GetInsertedSeqLengthOnRight(TNumrow row, TNumseg seg) const
{
    return (IsPositiveStrand(row) ?
            GetStop(row, seg+1) - GetStart(row, seg) :
            GetStart(row, seg+1) - GetStop(row, seg));
}


inline 
CAlnMap::TSegTypeFlags 
CAlnMap::x_GetRawSegType(TNumrow row, TNumseg seg) const
{
    TSegTypeFlags type;
    if (m_RawSegTypes &&
        (type = (*m_RawSegTypes)[row + m_DS->GetDim() * seg]) & fTypeIsSet) {
        return type & (~ fTypeIsSet);
    } else {
        return x_SetRawSegType(row, seg);
    }
}

inline
bool CAlnMap::IsTypeInsert(TSegTypeFlags type)
{
    return (type & fInsert) == fInsert;
}

inline 
CAlnMap::TSegTypeFlags 
CAlnMap::GetTypeAtAlnPos(TNumrow row, TSeqPos aln_pos) const
{
    return GetSegType(row, GetSeg(aln_pos));
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2002/10/04 17:05:31  todorov
* Added GetTypeAtAlnPos method
*
* Revision 1.8  2002/09/27 16:58:21  todorov
* changed order of params for GetSeqPosFrom{Seq,Aln}Pos
*
* Revision 1.7  2002/09/27 02:26:32  ucko
* Remove static from the definition of CAlnMap::IsTypeInsert, as
* (like virtual) it should appear only on the initial declaration.
*
* Revision 1.6  2002/09/26 18:43:11  todorov
* Added a static method for convenient check for an insert
*
* Revision 1.5  2002/09/26 17:40:42  todorov
* Changed flag fAlignedToSeqOnAnchor to fNotAlignedToSeqOnAnchor. This proved more convenient.
*
* Revision 1.4  2002/09/25 18:16:26  dicuccio
* Reworked computation of consensus sequence - this is now stored directly
* in the underlying CDense_seg
* Added exception class; currently used only on access of non-existent
* consensus.
*
* Revision 1.3  2002/09/05 19:31:18  dicuccio
* - added ability to reference a consensus sequence for a given alignment
* - added caching of CSeqVector (big performance win)
* - many small bugs fixed
*
* Revision 1.2  2002/08/23 16:05:09  ucko
* Kludge friendship for MSVC.  (Sigh.)
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNMAP__HPP
