#ifndef OLIGOFAR_CHASHATOM__HPP
#define OLIGOFAR_CHASHATOM__HPP

#include "cquery.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashAtom
{
public:
    enum EFlags {
        fFlag_matePairs = 0x001,
        fFlag_matePair0 = 0x000,
        fFlag_matePair1 = 0x001,
        fFlag_words     = 0x010,
        fFlag_word0     = 0x000,
        fFlag_word1     = 0x010,
        fFlag_strands   = 0x100,
        fFlag_strandPos = 0x000,
        fFlag_strandNeg = 0x100,
        fFlags_COMPARE  = fFlag_strands | fFlag_matePairs,
        fFlags_NONE = 0x00
    };
    CHashAtom( CQuery * _o = 0, unsigned flags = fFlags_NONE, unsigned _m = 0, int _f = 0) 
        : m_query( _o ), m_flags( flags ), m_mism( _m ), m_offset( _f ) {}
    bool operator == ( const CHashAtom& o ) const { 
        return m_query == o.m_query && 
            ( m_flags & fFlags_COMPARE ) == ( o.m_flags & fFlags_COMPARE );
    }
    bool operator < ( const CHashAtom& o ) const { 
        if( m_query < o.m_query ) return true;
        if( m_query > o.m_query ) return false;
        return ( m_flags & fFlags_COMPARE ) < ( o.m_flags & fFlags_COMPARE );
    }
    CQuery * GetQuery() const { return m_query; }
    char GetStrand() const  { return "+-"[((m_flags & fFlag_strands) >> 8)]; }
    int GetOffset() const   { return m_offset; }
    int GetPairmate() const { return (m_flags & fFlag_matePairs) >> 0; }
    int GetMatepair() const { return (m_flags & fFlag_matePairs) >> 0; }
    int GetWordId() const   { return (m_flags & fFlag_words) >> 4; }
    Uint2 GetFlags() const  { return m_flags; }
protected:
    CQuery * m_query;
    Uint2 m_flags;
    Uint1 m_mism;
    Uint1 m_offset;
};

END_OLIGOFAR_SCOPES

#endif
