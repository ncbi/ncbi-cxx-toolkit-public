#ifndef OLIGOFAR_CHASHATOM__HPP
#define OLIGOFAR_CHASHATOM__HPP

#include "cquery.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashAtom
{
public:
    enum EFlags {
        eBit_pairMate = 0,
        eBit_wordId   = 1,
        eBit_strand   = 2,
        eBit_convert  = 3,
        fMask_pairMate = 1 << eBit_pairMate,
        fMask_wordId   = 1 << eBit_wordId,
        fMask_strand   = 1 << eBit_strand,
        fMask_convert  = 3 << eBit_convert,
        fFlag_pairMate0 = 0 << eBit_pairMate,
        fFlag_pairMate1 = 1 << eBit_pairMate,
        fFlag_wordId0   = 0 << eBit_wordId,
        fFlag_wordId1   = 1 << eBit_wordId,
        fFlag_strandFwd = 0 << eBit_strand,
        fFlag_strandRev = 1 << eBit_strand,
        fFlags_noConv = 0 << eBit_convert,
        fFlags_convTC = 1 << eBit_convert,
        fFlags_convAG = 2 << eBit_convert,
        fFlags_convEq = 3 << eBit_convert,
        fMask_COMPARE = fMask_strand | fMask_pairMate | fMask_convert,
        fMask_ALL = fMask_wordId | fMask_pairMate | fMask_strand ,
        fFlags_NONE = 0
    };

    enum EConv {
        eNoConv = fFlags_noConv,
        eConvTC = fFlags_convTC,
        eConvAG = fFlags_convAG,
        eConvEq = fFlags_convEq
    };

    enum EGapType {
        eNone = 0,
        eInsertion = 1,
        eDeletion = 2
    };

    CHashAtom( CQuery * query = 0, 
               Uint1 flags = 0,
               int offset = 0,
               int mism = 0,
               int gaps = 0,
               EGapType t = eNone )
           : m_query( query ), m_subkey( 0 ), m_offset( offset ),
             m_gapIsInsertion( t & 1 & (gaps > 0) ), m_mismCnt( mism ), m_gapsCnt( gaps ),
             m_flags( flags ) {}

    CHashAtom( const CHashAtom& a, const CHashAtom& b ); // is used in cqueryhash for correct configuration of data to call an aligner

    static int GetMaxOffset() { return ((1 << 6) - 1); }
    
    Uint2 GetSubkey() const { return m_subkey; }
    CQuery * GetQuery() const { return m_query; }
    bool IsReverseStrand() const { return (GetStrandId() >> eBit_strand) != 0; }
    bool IsForwardStrand() const { return !IsReverseStrand(); }
    char GetStrandId() const  { return m_flags&fMask_strand; } // for sort
    char GetStrand() const  { return "+-"[(int)IsReverseStrand()]; }
    int GetOffset() const   { return m_offset; }
    int GetPairmate() const { return (m_flags&fMask_pairMate) >> eBit_pairMate; }
    int GetPairmask() const { return 1 << GetPairmate(); }
    int GetWordId() const   { return (m_flags&fMask_wordId) >> eBit_wordId; };
    EConv GetConv() const   { return EConv(m_flags&fMask_convert); }

    bool HasDiffs() const { return m_mismCnt || m_gapsCnt; }
    int GetMismatches() const { return m_mismCnt; }
    int GetInsertions() const { return m_gapIsInsertion ? m_gapsCnt : 0; }
    int GetDeletions() const { return m_gapIsInsertion ? 0 : m_gapsCnt; }
    int GetIndels() const { return m_gapsCnt; }
    int GetDiffs() const { return m_mismCnt + m_gapsCnt; }
    EGapType GetIndelType() const { return m_gapsCnt ? m_gapIsInsertion ? eInsertion : eDeletion : eNone; }

    Uint1 GetFlags( Uint1 mask = fMask_ALL ) const { return m_flags & mask; } 

    static bool LessSubkey( const CHashAtom& a, const CHashAtom& b ) {
        return a.GetSubkey() < b.GetSubkey();
    }
    static bool LessQueryCoiterate( const CHashAtom& a, const CHashAtom& b ) {
        if( a.m_query < b.m_query ) return true;
        if( a.m_query > b.m_query ) return false;
        if( a.m_offset < b.m_offset ) return true;
        if( a.m_offset > b.m_offset ) return false;
        if( ( a.m_flags & fMask_COMPARE ) < ( b.m_flags & fMask_COMPARE ) ) return true;
        if( ( a.m_flags & fMask_COMPARE ) > ( b.m_flags & fMask_COMPARE ) ) return false;
        // need this to pass gaps property
        if( a.GetIndels() < b.GetIndels() ) return true;
        if( a.GetIndels() > b.GetIndels() ) return false;
        return a.GetIndelType() < b.GetIndelType();
    }
    static bool LessQueryReadWord( const CHashAtom& a, const CHashAtom& b ) {
        // should not be used to coiterate, but good for initial sorting
        // but is used to insert into array_set to match better words first
        // if not used there then there will be lost hits
        if( a.m_query < b.m_query ) return true;
        if( a.m_query > b.m_query ) return false;
        if( a.m_offset < b.m_offset ) return true;
        if( a.m_offset > b.m_offset ) return false;
        if( ( a.m_flags & fMask_COMPARE ) < ( b.m_flags & fMask_COMPARE ) ) return true;
        if( ( a.m_flags & fMask_COMPARE ) > ( b.m_flags & fMask_COMPARE ) ) return false;
        return LessDiffs( a, b );
    }
    static bool LessDiffs( const CHashAtom& a, const CHashAtom& b ) {
        if( a.GetIndels() < b.GetIndels() ) return true;
        if( a.GetIndels() > b.GetIndels() ) return false;
        if( a.m_gapIsInsertion < b.m_gapIsInsertion ) return true;
        if( a.m_gapIsInsertion > b.m_gapIsInsertion ) return false;
        return a.GetMismatches() < b.GetMismatches();
    }
    static bool EqualQueryReadWord( const CHashAtom& a, const CHashAtom& b ) {
        return a.m_query == b.m_query && 
            a.m_offset == b.m_offset &&
            ( a.m_flags & fMask_COMPARE ) == ( b.m_flags & fMask_COMPARE ) &&
            EqualDiffs( a, b );
    }
    static bool EqualDiffs( const CHashAtom& a, const CHashAtom& b ) {
        return
            ( a.m_gapIsInsertion == b.m_gapIsInsertion ) &&
            ( a.m_gapsCnt == b.m_gapsCnt ) &&
            ( a.m_mismCnt == b.m_mismCnt );
    }

    bool operator == ( const CHashAtom& other ) const { return EqualQueryReadWord( *this, other ); }
    bool operator <  ( const CHashAtom& other ) const { return LessQueryReadWord( *this, other ); }

    void PrintDebug( ostream& o ) const;
protected:
    friend class CWordHash;
    CHashAtom( Uint2 subkey ) : 
        m_query(0), m_subkey( subkey ), m_offset(0), m_gapIsInsertion( 0 ), m_mismCnt( 0 ), m_gapsCnt( 0 ), m_flags(0)
    {}
    void SetSubkey( Uint2 subkey ) { m_subkey = subkey; }
protected:
    CQuery * m_query;
    Uint2 m_subkey;
    Uint1 m_offset:6;
    Uint1 m_gapIsInsertion:1;
    Uint1 m_mismCnt:2;
    Uint1 m_gapsCnt:2;
    Uint1 m_flags:5;
};

inline CHashAtom::CHashAtom( const CHashAtom& a, const CHashAtom& b ) 
{
    if( a.GetStrandId() == CHashAtom::fFlag_strandFwd ) *this = b; else *this = a; 
    m_mismCnt = max( b.m_mismCnt, a.m_mismCnt );
    m_gapsCnt = max( b.m_gapsCnt, a.m_gapsCnt );
}

inline void CHashAtom::PrintDebug( ostream& o ) const
{
    o << hex << setw(4) << setfill('0') << GetSubkey() << dec << "\t" 
      << GetQuery()->GetId() << ":" << GetPairmate() << "\t"
      << GetStrand() << "\t"
      << GetOffset() << "(o)\t"
      << GetWordId() << "(w)" << "\t" //; //\t"
      << GetMismatches() << "(m)\t"
      << GetInsertions() << "(i)\t"
      << GetDeletions() << "(d)";
}

inline ostream& operator << ( ostream& o, const CHashAtom& a ) 
{
    a.PrintDebug( o );
    return o;
}

END_OLIGOFAR_SCOPES

#endif
