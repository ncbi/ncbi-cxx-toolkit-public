#ifndef OLIGOFAR_CHASHATOM__HPP
#define OLIGOFAR_CHASHATOM__HPP

#include "cquery.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashAtom
{
public:
    enum EIndel {
        eINVALID = 1,
        eInsertion = 2,
        eDeletion = 3,
        eNoIndel = 0
    };
    enum EFlags {
        eBit_pairMate = 0,
        eBit_wordId   = 1,
        eBit_strand   = 2,
        fMask_pairMate = 1 << eBit_pairMate,
        fMask_wordId   = 1 << eBit_wordId,
        fMask_strand   = 1 << eBit_strand,
        fFlag_pairMate0 = 0 << eBit_pairMate,
        fFlag_pairMate1 = 1 << eBit_pairMate,
        fFlag_wordId0   = 0 << eBit_wordId,
        fFlag_wordId1   = 1 << eBit_wordId,
        fFlag_strandFwd = 0 << eBit_strand,
        fFlag_strandRev = 1 << eBit_strand,
        fMask_COMPARE = fMask_strand | fMask_pairMate,
        fMask_ALL = fMask_wordId | fMask_pairMate | fMask_strand ,
        fFlags_NONE = 0
    };


    CHashAtom( CQuery * query = 0, 
               Uint1 flags = 0,
               int offset = 0,
               int mism = 0,
               EIndel indel = eNoIndel )
        : m_query( query ), m_subkey( 0 ), m_offset( offset ),
          m_flags( flags ), m_mism( mism ), m_gaps( indel ) {}

    Uint2 GetSubkey() const { return m_subkey; }
    CQuery * GetQuery() const { return m_query; }
    char GetStrandId() const  { return m_flags&fMask_strand; } // for sort
    char GetStrand() const  { return "+-"[ (m_flags&fMask_strand) >> eBit_strand ]; }
    int GetOffset() const   { return m_offset; }
    int GetPairmate() const { return (m_flags&fMask_pairMate) >> eBit_pairMate; }
    int GetWordId() const   { return (m_flags&fMask_wordId) >> eBit_wordId; };
    int GetMismatches() const { return m_mism; }
    EIndel GetIndel() const { return EIndel( m_gaps ); }
    Uint1 GetFlags( Uint1 mask = fMask_ALL ) const { return m_flags & mask; } 

    static bool LessSubkey( const CHashAtom& a, const CHashAtom& b ) {
        return a.GetSubkey() < b.GetSubkey();
    }
    static bool LessQueryReadWord( const CHashAtom& a, const CHashAtom& b ) {
        if( a.m_query < b.m_query ) return true;
        if( a.m_query > b.m_query ) return false;
        if( a.m_offset < b.m_offset ) return true;
        if( a.m_offset > b.m_offset ) return false;
        return ( a.m_flags & fMask_COMPARE ) < ( b.m_flags & fMask_COMPARE );
    }
    static bool EqualQueryReadWord( const CHashAtom& a, const CHashAtom& b ) {
        return a.m_query == b.m_query && 
            a.m_offset == b.m_offset &&
            ( a.m_flags & fMask_COMPARE ) == ( b.m_flags & fMask_COMPARE );
    }

    bool operator == ( const CHashAtom& other ) const { return EqualQueryReadWord( *this, other ); }
    bool operator <  ( const CHashAtom& other ) const { return LessQueryReadWord( *this, other ); }

protected:
    friend class CWordHash;
    CHashAtom( Uint2 subkey ) : 
        m_query(0), m_subkey( subkey ), m_offset(0), m_flags(0), m_mism(0), m_gaps( eNoIndel ) {}
    void SetSubkey( Uint2 subkey ) { m_subkey = subkey; }
protected:
    CQuery * m_query;
    Uint2 m_subkey;
    Uint1 m_offset;
    int m_flags:4;
    int m_mism:2;
    int m_gaps:2;
};

END_OLIGOFAR_SCOPES

#endif
