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
#include <objtools/alnmgr/alnexception.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_XALNMGR_EXPORT CAlnMap : public CObject
{
    typedef CObject TParent;

public:
    // data types
    typedef unsigned int TSegTypeFlags; // binary OR of ESegTypeFlags
    enum ESegTypeFlags {
        fSeq                     = 0x0001,
        fNotAlignedToSeqOnAnchor = 0x0002,
        fInsert                  = fSeq | fNotAlignedToSeqOnAnchor,
        fUnalignedOnRight        = 0x0004, // unaligned region on the right
        fUnalignedOnLeft         = 0x0008,
        fNoSeqOnRight            = 0x0010, // maybe gaps on the right but no seq
        fNoSeqOnLeft             = 0x0020,
        fEndOnRight              = 0x0040, // this is the last segment
        fEndOnLeft               = 0x0080,
        fUnaligned               = 0x0100, // this is an unaligned region
        // reserved for internal use
        fTypeIsSet            = (TSegTypeFlags) 0x80000000
    };
    
    typedef CDense_seg::TDim      TDim;
    typedef TDim                  TNumrow;
    typedef CRange<TSeqPos>       TRange;
    typedef CRange<TSignedSeqPos> TSignedRange;
    typedef CDense_seg::TNumseg   TNumseg;
    typedef list<TSeqPos>         TSeqPosList;


    enum EGetChunkFlags {
        fAllChunks           = 0x0000,
        fIgnoreUnaligned     = 0x0001,
        // fInsertSameAsSeq, fDeletionSameAsGap and fIgnoreAnchor
        // are used to consolidate adjacent segments which whose type
        // only differs in how they relate to the anchor.
        // Still, when obtaining the type of the chunks, the info about
        // the relationship to anchor (fNotAlignedToSeqOnAnchor) will be
        // present.
        fInsertSameAsSeq     = 0x0002,
        fDeletionSameAsGap   = 0x0004,
        fIgnoreAnchor        = fInsertSameAsSeq | fDeletionSameAsGap,
        fIgnoreGaps          = 0x0008,
        fChunkSameAsSeg      = 0x0010,
        
        fSkipUnalignedGaps   = 0x0020,
        fSkipDeletions       = 0x0040,
        fSkipAllGaps         = fSkipUnalignedGaps | fSkipDeletions,
        fSkipInserts         = 0x0080,
        fSkipAlnSeq          = 0x0100,
        fSeqOnly             = fSkipAllGaps | fSkipInserts,
        fInsertsOnly         = fSkipAllGaps | fSkipAlnSeq,
        fAlnSegsOnly         = fSkipInserts | fSkipUnalignedGaps,

        // preserve the wholeness of the segments when intersecting
        // with the given range instead of truncating them
        fDoNotTruncateSegs   = 0x0200,

        // In adition to other chunks, intoduce chunks representing
        // regions of sequence which are implicit inserts but are
        // not tecnically present in the underlying alignment,
        // AKA "unaligned regions"
        fAddUnalignedChunks  = 0x0400
    };
    typedef int TGetChunkFlags; // binary OR of EGetChunkFlags

    typedef TNumseg TNumchunk;

    enum ESearchDirection {
        eNone,
        eBackwards,
        eForward,
        eLeft,
        eRight
    };

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

    // Widths
    int  GetWidth        (TNumrow row) const;

    // Sequence visible range
    TSignedSeqPos GetSeqAlnStart(TNumrow row) const; //aln coords, strand ignored
    TSignedSeqPos GetSeqAlnStop (TNumrow row) const;
    TSignedRange  GetSeqAlnRange(TNumrow row) const;
    TSeqPos       GetSeqStart   (TNumrow row) const; //seq coords, with strand
    TSeqPos       GetSeqStop    (TNumrow row) const;  
    TRange        GetSeqRange   (TNumrow row) const;

    // Segment info
    TSignedSeqPos GetStart  (TNumrow row, TNumseg seg, int offset = 0) const;
    TSignedSeqPos GetStop   (TNumrow row, TNumseg seg, int offset = 0) const;
    TSignedRange  GetRange  (TNumrow row, TNumseg seg, int offset = 0) const;
    TSeqPos       GetLen    (             TNumseg seg, int offset = 0) const;
    TSeqPos       GetSeqLen (TNumrow row, TNumseg seg, int offset = 0) const;
    TSegTypeFlags GetSegType(TNumrow row, TNumseg seg, int offset = 0) const;
    
    TSegTypeFlags GetTypeAtAlnPos(TNumrow row, TSeqPos aln_pos) const;

    static bool IsTypeInsert(TSegTypeFlags type);

    TSeqPos GetInsertedSeqLengthOnRight(TNumrow row, TNumseg seg) const;

    // Alignment segments
    TSeqPos GetAlnStart(TNumseg seg) const;
    TSeqPos GetAlnStop (TNumseg seg) const;
    TSeqPos GetAlnStart(void)        const { return 0; }
    TSeqPos GetAlnStop (void)        const;

    bool    IsSetAnchor(void)           const;
    TNumrow GetAnchor  (void)           const;
    void    SetAnchor  (TNumrow anchor);
    void    UnsetAnchor(void);

    // get number of
    TNumseg GetNumberOfInsertedSegmentsOnRight(TNumrow row, TNumseg seg) const;
    TNumseg GetNumberOfInsertedSegmentsOnLeft (TNumrow row, TNumseg seg) const;

    //
    // Position mapping funcitons
    // 
    // Note: Some of the mapping functions have optional parameters
    //       ESearchDirection dir and bool try_reverse_dir 
    //       which are used in case an exact match is not found.
    //       If nothing is found in the ESearchDirection dir and 
    //       try_reverse_dir == true will search in the opposite dir.

    TNumseg       GetSeg                 (TSeqPos aln_pos)              const;
    // if seq_pos falls outside the seq range or into an unaligned region
    // and dir is provided, will return the first seg in according to dir
    TNumseg       GetRawSeg              (TNumrow row, TSeqPos seq_pos,
                                          ESearchDirection dir = eNone,
                                          bool try_reverse_dir = true)  const;
    // if seq_pos is outside the seq range or within an unaligned region or
    // within an insert dir/try_reverse_dir will be used
    TSignedSeqPos GetAlnPosFromSeqPos    (TNumrow row, TSeqPos seq_pos,
                                          ESearchDirection dir = eNone,
                                          bool try_reverse_dir = true)  const;
    TSignedSeqPos GetSeqPosFromSeqPos    (TNumrow for_row,
                                          TNumrow row, TSeqPos seq_pos) const;
    // if seq pos is a gap, will use dir/try_reverse_dir
    TSignedSeqPos GetSeqPosFromAlnPos    (TNumrow for_row,
                                          TSeqPos aln_pos,
                                          ESearchDirection dir = eNone,
                                          bool try_reverse_dir = true)  const;
    
    // Create a vector of relative mapping positions from row0 to row1.
    // Input:  row0, row1, aln_rng (vertical slice)
    // Output: result (the resulting vector of positions),
    //         rng0, rng1 (affected ranges in native sequence coords)
    void          GetResidueIndexMap     (TNumrow row0,
                                          TNumrow row1,
                                          TRange aln_rng,
                                          vector<TSignedSeqPos>& result,
                                          TRange& rng0,
                                          TRange& rng1)                 const;

    // AlnChunks -- declared here for access to typedefs
    class CAlnChunk;
    class CAlnChunkVec;
    
protected:
    void x_GetChunks              (CAlnChunkVec * vec,
                                   TNumrow row,
                                   TNumseg first_seg,
                                   TNumseg last_seg,
                                   TGetChunkFlags flags) const;

public:
    // Get a vector of chunks defined by flags
    // in alignment coords range
    CRef<CAlnChunkVec> GetAlnChunks(TNumrow row, const TSignedRange& range,
                                    TGetChunkFlags flags = fAlnSegsOnly) const;
    // or in native sequence coords range
    CRef<CAlnChunkVec> GetSeqChunks(TNumrow row, const TSignedRange& range,
                                    TGetChunkFlags flags = fAlnSegsOnly) const;

    class NCBI_XALNMGR_EXPORT CAlnChunkVec : public CObject
    {
    public:
        CAlnChunkVec(const CAlnMap& aln_map, TNumrow row)
            : m_AlnMap(aln_map), m_Row(row) { }

        CConstRef<CAlnChunk> operator[] (TNumchunk i) const;

        TNumchunk size(void) const { return m_StartSegs.size(); };

    private:
#if defined(NCBI_COMPILER_MSVC) ||  defined(NCBI_COMPILER_METROWERKS) // kludge
        friend class CAlnMap;
#else
        friend
        CRef<CAlnChunkVec> CAlnMap::GetAlnChunks(TNumrow row,
                                                 const TSignedRange& range,
                                                 TGetChunkFlags flags) const;
        friend
        CRef<CAlnChunkVec> CAlnMap::GetSeqChunks(TNumrow row,
                                                 const TSignedRange& range,
                                                 TGetChunkFlags flags) const;
        friend
        void               CAlnMap::x_GetChunks (CAlnChunkVec * vec,
                                                 TNumrow row,
                                                 TNumseg first_seg,
                                                 TNumseg last_seg,
                                                 TGetChunkFlags flags) const;
#endif

        // can only be created by CAlnMap::GetAlnChunks
        CAlnChunkVec(void); 
    
        const CAlnMap&  m_AlnMap;
        TNumrow         m_Row;
        vector<TNumseg> m_StartSegs;
        vector<TNumseg> m_StopSegs;
        TSeqPos         m_LeftDelta;
        TSeqPos         m_RightDelta;
    };

    class NCBI_XALNMGR_EXPORT CAlnChunk : public CObject
    {
    public:    
        TSegTypeFlags GetType(void) const { return m_TypeFlags; }
        CAlnChunk&    SetType(TSegTypeFlags type_flags)
            { m_TypeFlags = type_flags; return *this; }

        const TSignedRange& GetRange(void) const { return m_SeqRange; }

        const TSignedRange& GetAlnRange(void) const { return m_AlnRange; }

        bool IsGap(void) const { return m_SeqRange.GetFrom() < 0; }
        
    private:
        // can only be created or modified by
        friend CConstRef<CAlnChunk> CAlnChunkVec::operator[](TNumchunk i)
            const;
        CAlnChunk(void) {}
        TSignedRange& SetRange(void)    { return m_SeqRange; }
        TSignedRange& SetAlnRange(void) { return m_AlnRange; }

        TSegTypeFlags m_TypeFlags;
        TSignedRange  m_SeqRange;
        TSignedRange  m_AlnRange;
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
    void              x_Init            (void);
    void              x_CreateAlnStarts (void);
    TSegTypeFlags     x_GetRawSegType   (TNumrow row, TNumseg seg) const;
    TSegTypeFlags     x_SetRawSegType   (TNumrow row, TNumseg seg) const;
    CNumSegWithOffset x_GetSegFromRawSeg(TNumseg seg)              const;
    TNumseg           x_GetRawSegFromSeg(TNumseg seg)              const;
    TSignedSeqPos     x_GetRawStart     (TNumrow row, TNumseg seg) const;
    TSignedSeqPos     x_GetRawStop      (TNumrow row, TNumseg seg) const;
    TSeqPos           x_GetLen          (TNumrow row, TNumseg seg) const;
    const TNumseg&    x_GetSeqLeftSeg   (TNumrow row)              const;
    const TNumseg&    x_GetSeqRightSeg  (TNumrow row)              const;

    bool x_SkipType               (TSegTypeFlags type,
                                   TGetChunkFlags flags) const;
    bool x_CompareAdjacentSegTypes(TSegTypeFlags left_type, 
                                   TSegTypeFlags right_type,
                                   TGetChunkFlags flags) const;
    // returns true if types are the same (as specified by flags)

    CConstRef<CDense_seg>           m_DS;
    TNumrow                         m_NumRows;
    TNumseg                         m_NumSegs;
    const CDense_seg::TIds&         m_Ids;
    const CDense_seg::TStarts&      m_Starts;
    const CDense_seg::TLens&        m_Lens;
    const CDense_seg::TStrands&     m_Strands;
    const CDense_seg::TScores&      m_Scores;
    const CDense_seg::TWidths&      m_Widths;
    TNumrow                         m_Anchor;
    vector<TNumseg>                 m_AlnSegIdx;
    mutable vector<TNumseg>         m_SeqLeftSegs;
    mutable vector<TNumseg>         m_SeqRightSegs;
    CDense_seg::TStarts             m_AlnStarts;
    vector<CNumSegWithOffset>       m_NumSegWithOffsets;
    mutable vector<TSegTypeFlags> * m_RawSegTypes;
};



class NCBI_XALNMGR_EXPORT CAlnMapPrinter : public CObject
{
public:
    /// Constructor
    CAlnMapPrinter(const CAlnMap& aln_map,
                   CNcbiOstream&  out);

    /// Printing methods
    void CsvTable();
    void Segments();
    void Chunks  (CAlnMap::TGetChunkFlags flags = CAlnMap::fAlnSegsOnly);

    /// Fasta style Ids
    const string& GetId      (CAlnMap::TNumrow row) const;

    /// Field printers
    void          PrintId    (CAlnMap::TNumrow row) const;
    void          PrintNumRow(CAlnMap::TNumrow row) const;
    void          PrintSeqPos(TSeqPos pos) const;

private:
    const CAlnMap&         m_AlnMap;
    mutable vector<string> m_Ids;

protected:
    size_t                 m_IdFieldLen;
    size_t                 m_RowFieldLen;
    size_t                 m_SeqPosFieldLen;
    const CAlnMap::TNumrow m_NumRows;
    CNcbiOstream*          m_Out;
};



///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CAlnMap::CAlnMap(const CDense_seg& ds) 
    : m_DS(&ds),
      m_NumRows(ds.GetDim()),
      m_NumSegs(ds.GetNumseg()),
      m_Ids(ds.GetIds()),
      m_Starts(ds.GetStarts()),
      m_Lens(ds.GetLens()),
      m_Strands(ds.GetStrands()),
      m_Scores(ds.GetScores()),
      m_Widths(ds.GetWidths()),
      m_Anchor(-1),
      m_RawSegTypes(0)
{
    x_Init();
    x_CreateAlnStarts();
}


inline
CAlnMap::CAlnMap(const CDense_seg& ds, TNumrow anchor)
    : m_DS(&ds),
      m_NumRows(ds.GetDim()),
      m_NumSegs(ds.GetNumseg()),
      m_Ids(ds.GetIds()),
      m_Starts(ds.GetStarts()),
      m_Lens(ds.GetLens()),
      m_Strands(ds.GetStrands()),
      m_Scores(ds.GetScores()),
      m_Widths(ds.GetWidths()),
      m_Anchor(-1),
      m_RawSegTypes(0)
{
    x_Init();
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
    return m_AlnStarts[seg] + m_Lens[x_GetRawSegFromSeg(seg)] - 1;
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
    return IsSetAnchor() ? m_AlnSegIdx.size() : m_NumSegs;
}


inline
CAlnMap::TDim CAlnMap::GetNumRows(void) const
{
    return m_NumRows;
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
TSignedSeqPos CAlnMap::x_GetRawStart(TNumrow row, TNumseg seg) const
{
    return m_Starts[seg * m_NumRows + row];
}

inline
int CAlnMap::GetWidth(TNumrow row) const
{
    return
        (m_Widths.size() == (size_t) m_NumRows) ? m_Widths[row] : 1;
}

inline
TSeqPos CAlnMap::x_GetLen(TNumrow row, TNumseg seg) const
{
    return m_Lens[seg] * GetWidth(row);
}

inline
TSignedSeqPos CAlnMap::x_GetRawStop(TNumrow row, TNumseg seg) const
{
    TSignedSeqPos start = x_GetRawStart(row, seg);
    return ((start > -1) ? (start + (TSignedSeqPos)x_GetLen(row, seg) - 1)
            : -1);
}


inline
int CAlnMap::StrandSign(TNumrow row) const
{
    return IsPositiveStrand(row) ? 1 : -1;
}


inline
bool CAlnMap::IsPositiveStrand(TNumrow row) const
{
    return (m_Strands.empty()  ||  m_Strands[row] != eNa_strand_minus);
}


inline
bool CAlnMap::IsNegativeStrand(TNumrow row) const
{
    return ! IsPositiveStrand(row);
}


inline
TSignedSeqPos CAlnMap::GetStart(TNumrow row, TNumseg seg, int offset) const
{
    return m_Starts
        [(x_GetRawSegFromSeg(seg) + offset) * m_NumRows + row];
}

inline
TSeqPos CAlnMap::GetLen(TNumseg seg, int offset) const
{
    return m_Lens[x_GetRawSegFromSeg(seg) + offset];
}


inline
TSeqPos CAlnMap::GetSeqLen(TNumrow row, TNumseg seg, int offset) const
{
    return x_GetLen(row, x_GetRawSegFromSeg(seg) + offset);
}


inline
TSignedSeqPos CAlnMap::GetStop(TNumrow row, TNumseg seg, int offset) const
{
    TSignedSeqPos start = GetStart(row, seg, offset);
    return ((start > -1) ? 
            (start + (TSignedSeqPos)GetSeqLen(row, seg, offset) - 1) :
            -1);
}


inline
const CSeq_id& CAlnMap::GetSeqId(TNumrow row) const
{
    return *(m_Ids[row]);
}


inline 
CAlnMap::TSignedRange
CAlnMap::GetRange(TNumrow row, TNumseg seg, int offset) const
{
    TSignedSeqPos start = GetStart(row, seg, offset);
    if (start > -1) {
        return TSignedRange(start, start + GetSeqLen(row, seg, offset) - 1);
    } else {
        return TSignedRange(-1, -1);
    }
}


inline
TSeqPos CAlnMap::GetSeqStart(TNumrow row) const
{
    return 
        m_Starts[(IsPositiveStrand(row) ?
                  x_GetSeqLeftSeg(row) :
                  x_GetSeqRightSeg(row)) * m_NumRows + row];
}


inline
TSeqPos CAlnMap::GetSeqStop(TNumrow row) const
{
    const TNumseg& seg = IsPositiveStrand(row) ?
        x_GetSeqRightSeg(row) : x_GetSeqLeftSeg(row);
    return m_Starts[seg * m_NumRows + row] + x_GetLen(row, seg) - 1;
}


inline
CAlnMap::TRange CAlnMap::GetSeqRange(TNumrow row) const
{
    return TRange(GetSeqStart(row), GetSeqStop(row));
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
        (type = (*m_RawSegTypes)[row + m_NumRows * seg]) & fTypeIsSet) {
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

inline
const string&
CAlnMapPrinter::GetId(CAlnMap::TNumrow row) const
{
    return m_Ids[row];
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
* Revision 1.43  2005/03/15 22:18:01  todorov
* + PrintSeqPos
*
* Revision 1.42  2005/03/15 19:18:11  todorov
* + CAlnMapPrinter::PrintRow
*
* Revision 1.41  2005/03/15 17:44:24  todorov
* Added a printer class
*
* Revision 1.40  2004/10/12 19:42:44  rsmith
* work around compiler bug in Codewarrior
*
* Revision 1.39  2004/09/15 20:07:29  todorov
* Added fUnaligned and fAddUnalignedChunks
*
* Revision 1.38  2004/06/30 16:52:09  ivanov
* Rollback to R1.35 -- MSVC6 compile errors
*
* Revision 1.35  2004/03/03 20:33:27  todorov
* +comments
*
* Revision 1.34  2004/03/03 19:39:43  todorov
* +GetResidueIndexMap
*
* Revision 1.33  2004/01/21 20:59:42  todorov
* fDoNotTruncate -> fDoNotTruncateSegs; +comments
*
* Revision 1.32  2004/01/21 20:53:44  todorov
* EGetChunkFlags += fDoNotTruncate
*
* Revision 1.31  2003/12/31 17:26:18  todorov
* +fIgnoreAnchore usage comment
*
* Revision 1.30  2003/09/18 23:05:11  todorov
* Optimized GetSeqAln{Start,Stop}
*
* Revision 1.29  2003/09/09 19:42:37  dicuccio
* Fixed thinko in GetWidth() - properly order ternary operator
*
* Revision 1.28  2003/09/08 19:49:19  todorov
* signed vs unsigned warnings fixed
*
* Revision 1.27  2003/08/29 18:17:17  dicuccio
* Minor change in specification of default parameters - rely only on prameters,
* not on member variables
*
* Revision 1.26  2003/08/25 16:35:06  todorov
* exposed GetWidth
*
* Revision 1.25  2003/08/20 14:35:14  todorov
* Support for NA2AA Densegs
*
* Revision 1.24  2003/07/17 22:46:56  todorov
* name change +TSeqPosList
*
* Revision 1.23  2003/07/08 20:26:28  todorov
* Created seq end segments cache
*
* Revision 1.22  2003/06/05 19:03:29  todorov
* Added const refs to Dense-seg members as a speed optimization
*
* Revision 1.21  2003/06/02 16:01:38  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.20  2003/05/23 18:10:38  todorov
* +fChunkSameAsSeg
*
* Revision 1.19  2003/03/20 16:37:14  todorov
* +fIgnoreGaps for GetXXXChunksalnmap.cpp
*
* Revision 1.18  2003/03/07 17:30:14  todorov
* + ESearchDirection dir, bool try_reverse_dir for GetAlnPosFromSeqPos
*
* Revision 1.17  2003/03/04 16:18:58  todorov
* Added advance search options for GetRawSeg
*
* Revision 1.16  2003/01/15 18:48:36  todorov
* Added GetSeqChunks to be used with native seq range
*
* Revision 1.15  2003/01/09 22:08:11  todorov
* Changed the default TGetChunkFlags for consistency
*
* Revision 1.14  2003/01/07 23:02:48  todorov
* Fixed EGetChunkFlags
*
* Revision 1.13  2002/12/26 12:38:08  dicuccio
* Added Win32 export specifiers
*
* Revision 1.12  2002/10/21 19:14:36  todorov
* reworked aln chunks: now supporting more types; added chunk aln coords
*
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
