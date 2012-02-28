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
#include <objtools/alnmgr/aln_explorer.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_XALNMGR_EXPORT CAlnMap : public CObject, public IAlnExplorer
{
    typedef CObject TParent;

public:
    // data types
    typedef unsigned int TSegTypeFlags; // binary OR of ESegTypeFlags
    enum ESegTypeFlags {
        fSeq                      = 0x0001,
        fNotAlignedToSeqOnAnchor  = 0x0002,
        fInsert                   = fSeq | fNotAlignedToSeqOnAnchor,
        fUnalignedOnRight         = 0x0004, // unaligned region to the right of this segment
        fUnalignedOnLeft          = 0x0008,
        fNoSeqOnRight             = 0x0010, // maybe gaps on the right but no seq
        fNoSeqOnLeft              = 0x0020,
        fEndOnRight               = 0x0040, // this is the last segment
        fEndOnLeft                = 0x0080,
        fUnaligned                = 0x0100, // this is an unaligned region
        fUnalignedOnRightOnAnchor = 0x0200, // unaligned region to the right of the corresponding segment on the anchor
        fUnalignedOnLeftOnAnchor  = 0x0400,
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

    // constructors
    CAlnMap(const CDense_seg& ds);
    CAlnMap(const CDense_seg& ds, TNumrow anchor);

    // Flag indicating how to treat partially overlapping segment
    // when creating a sub-range of an alignment.
    enum ESegmentTrimFlag {
        eSegment_Include, // include whole segment
        eSegment_Trim,    // trim segment to the requested range
        eSegment_Remove   // do not include partial segments
    };

    // Create CAlnMap using a sub-range and rows sub-set of the source
    // alignment.
    CRef<CSeq_align> CreateAlignFromRange(const vector<TNumrow>& selected_rows,
        TSignedSeqPos          aln_from,
        TSignedSeqPos          aln_to,
        ESegmentTrimFlag       seg_flag = eSegment_Trim);

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

    // Alignment segments
    TSeqPos GetAlnStart(TNumseg seg) const;
    TSeqPos GetAlnStop (TNumseg seg) const;
    TSeqPos GetAlnStart(void)        const { return 0; }
    TSeqPos GetAlnStop (void)        const;

    bool    IsSetAnchor(void)           const;
    TNumrow GetAnchor  (void)           const;
    void    SetAnchor  (TNumrow anchor);
    void    UnsetAnchor(void);

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
    // if target seq pos is a gap, will use dir/try_reverse_dir
    TSignedSeqPos GetSeqPosFromSeqPos    (TNumrow for_row,
                                          TNumrow row, TSeqPos seq_pos,
                                          ESearchDirection dir = eNone,
                                          bool try_reverse_dir = true)  const;
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
                                   TNumseg left_seg,
                                   TNumseg right_seg,
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
        CAlnChunkVec(const CAlnMap& aln_map, TNumrow row) :
            m_AlnMap(aln_map), 
            m_Row(row),
            m_LeftDelta(0),
            m_RightDelta(0) {}

        CConstRef<CAlnChunk> operator[] (TNumchunk i) const;

        TNumchunk size(void) const { return TNumchunk(m_StartSegs.size()); };

    private:
#if defined(NCBI_COMPILER_MSVC) || defined(__clang__) // kludge
        friend class CAlnMap;
#elif defined(NCBI_COMPILER_WORKSHOP)  &&  NCBI_COMPILER_VERSION >= 550
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
                                                 TNumseg left_seg,
                                                 TNumseg right_seg,
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
    typedef vector<TSegTypeFlags> TRawSegTypes;
    void              x_Init             (void);
    void              x_CreateAlnStarts  (void);
    TRawSegTypes&     x_GetRawSegTypes   ()                         const;
    TSegTypeFlags     x_GetRawSegType    (TNumrow row, TNumseg seg, int hint_idx = -1) const;
    TSegTypeFlags     x_SetRawSegType    (TNumrow row, TNumseg seg) const;
    void              x_SetRawSegTypes   (TNumrow row)              const;
    CNumSegWithOffset x_GetSegFromRawSeg (TNumseg seg)              const;
    TNumseg           x_GetRawSegFromSeg (TNumseg seg)              const;
    TSignedSeqPos     x_GetRawStart      (TNumrow row, TNumseg seg) const;
    TSignedSeqPos     x_GetRawStop       (TNumrow row, TNumseg seg) const;
    TSeqPos           x_GetLen           (TNumrow row, TNumseg seg) const;
    const TNumseg&    x_GetSeqLeftSeg    (TNumrow row)              const;
    const TNumseg&    x_GetSeqRightSeg   (TNumrow row)              const;

    TSignedSeqPos     x_FindClosestSeqPos(TNumrow row,
                                          TNumseg seg,
                                          ESearchDirection dir,
                                          bool try_reverse_dir) const;

    bool x_SkipType               (TSegTypeFlags type,
                                   TGetChunkFlags flags) const;
    bool x_CompareAdjacentSegTypes(TSegTypeFlags left_type, 
                                   TSegTypeFlags right_type,
                                   TGetChunkFlags flags) const;
    // returns true if types are the same (as specified by flags)

    CConstRef<CDense_seg>       m_DS;
    TNumrow                     m_NumRows;
    TNumseg                     m_NumSegs;
    const CDense_seg::TIds&     m_Ids;
    const CDense_seg::TStarts&  m_Starts;
    const CDense_seg::TLens&    m_Lens;
    const CDense_seg::TStrands& m_Strands;
    const CDense_seg::TScores&  m_Scores;
    const CDense_seg::TWidths&  m_Widths;
    TNumrow                     m_Anchor;
    vector<TNumseg>             m_AlnSegIdx;
    mutable vector<TNumseg>     m_SeqLeftSegs;
    mutable vector<TNumseg>     m_SeqRightSegs;
    CDense_seg::TStarts         m_AlnStarts;
    vector<CNumSegWithOffset>   m_NumSegWithOffsets;
    mutable TRawSegTypes *      m_RawSegTypes;
};



class NCBI_XALNMGR_EXPORT CAlnMapPrinter : public CObject
{
public:
    /// Constructor
    CAlnMapPrinter(const CAlnMap& aln_map,
                   CNcbiOstream&  out);

    /// Printing methods
    void CsvTable(char delim = ',');
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
    return IsSetAnchor() ? TNumseg(m_AlnSegIdx.size()) : m_NumSegs;
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
    if ( !m_Widths.empty() ) {
        _ASSERT(m_Widths.size() == (size_t) m_NumRows);
        return m_Widths[row];
    } else {
        return 1;
    }
}

inline
TSeqPos CAlnMap::x_GetLen(TNumrow row, TNumseg seg) const
{
    /// Optimization for return m_Lens[seg] * GetWidth(row);
    if (GetWidth(row) != 1) {
        _ASSERT(GetWidth(row) == 3);
        TSeqPos len = m_Lens[seg];
        return len + len + len;
    } else {
        return m_Lens[seg];
    }
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
CAlnMap::TRawSegTypes& 
CAlnMap::x_GetRawSegTypes() const {
    if ( !m_RawSegTypes ) {
        // Using kZero for 0 works around a bug in Compaq's C++ compiler.
        static const TSegTypeFlags kZero = 0;
        m_RawSegTypes = new vector<TSegTypeFlags>
            (m_NumRows * m_NumSegs, kZero);
    }
    return *m_RawSegTypes;
}


inline 
CAlnMap::TSegTypeFlags 
CAlnMap::x_GetRawSegType(TNumrow row, TNumseg seg, int hint_idx) const
{
    TRawSegTypes& types = x_GetRawSegTypes();
    if ( !(types[row] & fTypeIsSet) ) {
        x_SetRawSegTypes(row);
    }
    _ASSERT(hint_idx < 0  ||  hint_idx == m_NumRows * seg + row);
    return types[hint_idx >= 0 ? hint_idx : m_NumRows * seg + row] & (~ fTypeIsSet);
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

#endif // OBJECTS_ALNMGR___ALNMAP__HPP
