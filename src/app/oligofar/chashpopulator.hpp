#ifndef OLIGOFAR_CHASHPOPULATOR__HPP
#define OLIGOFAR_CHASHPOPULATOR__HPP

#include "cbitmaskaccess.hpp"
#include "cpermutator8b.hpp"
#include "chashatom.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQuery;
class CHashPopulator 
{
public:
    template<class T> friend class CHashInserterCbk;
    typedef vector<pair<Uint8,CHashAtom> > THashList;

    typedef THashList::value_type value_type;
    typedef THashList::const_iterator const_iterator;
    
    static bool Less( const value_type&, const value_type& );
    static bool Same( const value_type&, const value_type& );

    CHashPopulator( int windowSize, 
                    int wordSize,
                    int strideSize,
                    CQuery * query,
                    int strands,
                    int offset,
                    int component,
                    CHashAtom::EConv conv,
                    CBitmaskAccess * bmAccess ) :
        m_query( query ),
        m_windowSize( windowSize ),
        m_wordSize( wordSize ),
        m_strideSize( strideSize ),
        m_strands( strands ),
        m_offset( offset ),
        m_windowMask( CBitHacks::WordFootprint<UintH>( 4 * windowSize ) ),
        m_wordMask( CBitHacks::WordFootprint<Uint8>( 2 * wordSize ) ),
        m_permutator( 0 ),
        m_gaps( 0 ),
        m_gapType( CHashAtom::eNone ),
        m_flags( conv | ( component ? CHashAtom::fFlag_pairMate1 : CHashAtom::fFlag_pairMate0 ) ),
        m_wbmAccess( bmAccess )
        {}
    CHashPopulator& SetPermutator( const CPermutator8b * p ) { m_permutator = p; return *this; }
    CHashPopulator& SetIndels( int gaps, CHashAtom::EGapType t ) { m_gaps = gaps; m_gapType = t; return *this; }
    CHashPopulator& Reserve( size_t count ) { m_data.reserve( count ); return *this; }
    
    THashList& SetData() { return m_data; }
    const THashList& GetData() const { return m_data; }
    
    CHashPopulator& Unique();

    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    size_t size() const { return m_data.size(); }

    void PopulateHash( const UintH& window );

    void operator() ( Uint8 hash, int mism, int amb );
    void SetConv( CHashAtom::EConv conv ) {
        m_flags = ( m_flags & ~CHashAtom::fMask_convert ) | conv;
    }

    void Print( ostream& o ) const;
    static void Print( ostream& o, const value_type& );

protected:
    CQuery * m_query;
    int m_windowSize;
    int m_wordSize;
    int m_strideSize;
    int m_strands;
    int m_offset;
    int m_stride;
    UintH m_fwindow;
    UintH m_rwindow;
    UintH m_windowMask; // 4bpp
    Uint8 m_wordMask;   // 2bpp
    const CPermutator8b * m_permutator;
    Uint1 m_gaps;
    CHashAtom::EGapType m_gapType;
    Uint1 m_flags;
    CBitmaskAccess * m_wbmAccess;
    THashList m_data;
};

inline CHashPopulator& CHashPopulator::Unique()
{
    sort( m_data.begin(), m_data.end(), Less );
    m_data.erase( unique( m_data.begin(), m_data.end(), Same ), m_data.end() );
    return *this;
}

inline void CHashPopulator::Print( ostream& o, const value_type& v )
{
    o << setw(16) << hex << setfill('0') << v.first << dec << "\t";
    v.second.PrintDebug( o );
}

inline void CHashPopulator::Print( ostream& o ) const
{
    ITERATE( THashList, l, m_data ) { Print( o, *l ); o << "\n"; }
}

inline bool CHashPopulator::Less( const value_type& a, const value_type& b ) 
{
    ASSERT( a.second.GetQuery() == b.second.GetQuery() );
    ASSERT( a.second.GetPairmate() == b.second.GetPairmate() );
    
    if( a.first < b.first ) return true;
    if( a.first > b.first ) return false;

    if( a.second.GetOffset() < b.second.GetOffset() ) return true;
    if( a.second.GetOffset() > b.second.GetOffset() ) return false;
    if( a.second.GetStrandId() < b.second.GetStrandId() ) return true;
    if( a.second.GetStrandId() > b.second.GetStrandId() ) return false;
    if( a.second.GetWordId() < b.second.GetWordId() ) return true;
    if( a.second.GetWordId() > b.second.GetWordId() ) return false;
    if( a.second.GetConv() < b.second.GetConv() ) return true;
    if( a.second.GetConv() > b.second.GetConv() ) return false;
    // NB: we should distinguish different word qualities to help find better first
    return CHashAtom::LessDiffs( a.second, b.second );
}

inline bool CHashPopulator::Same( const value_type& a, const value_type& b ) 
{
    return 
        a.first == b.first && 
        a.second.GetOffset() == b.second.GetOffset() && 
        a.second.GetWordId() == b.second.GetWordId() && 
        a.second.GetConv() == b.second.GetConv() && 
        a.second.GetStrandId() == b.second.GetStrandId() &&
        // following line SHOULD be present since it affects alignment code
        CHashAtom::EqualDiffs( a.second, b.second );
}

inline void CHashPopulator::PopulateHash( const UintH& window )
{
    ASSERT( m_permutator != 0 );
    
    m_fwindow = window;
    m_rwindow = m_query->GetCoding() == CSeqCoding::eCoding_colorsp ?
        CBitHacks::ReverseBitQuadsH( window ) >> (128 - 4 * (m_windowSize + m_strideSize - 1)) :
        CBitHacks::ReverseBitsH( window ) >> (128 - 4 * (m_windowSize + m_strideSize - 1));
    
    for( m_stride = 0; m_stride < m_strideSize; ++m_stride ) {
        if( m_strands & 1 ) {
            (m_flags &= ~CHashAtom::fMask_strand) |= CHashAtom::fFlag_strandFwd;
            UintH strideWindow = (m_fwindow >> (4*m_stride) ) & m_windowMask;
            m_permutator->ForEach( m_windowSize, strideWindow, *this );
        }
        if( m_strands & 2 ) {
            (m_flags &= ~CHashAtom::fMask_strand) |= CHashAtom::fFlag_strandRev;
            UintH strideWindow = (m_rwindow >> (4*(m_strideSize - m_stride - 1))) & m_windowMask;
            m_permutator->ForEach( m_windowSize, strideWindow, *this );
        }
    }
}

inline void CHashPopulator::operator () ( Uint8 hash, int mism, int amb )
{
    if( m_wbmAccess != 0 && ! m_wbmAccess->HasWord( hash ) ) return;

    if( m_wordSize == m_windowSize ) {
        CHashAtom a(m_query, m_flags, m_stride + m_offset, mism, m_gaps, m_gapType );
        m_data.push_back( make_pair( hash, a  ) );
    } else {
        int o = m_windowSize - m_wordSize;
        int o0 = o, o1 = 0;
        CHashAtom a0( m_query, m_flags | CHashAtom::fFlag_wordId0, m_stride + m_offset, mism, m_gaps, m_gapType );
        CHashAtom a1( m_query, m_flags | CHashAtom::fFlag_wordId1, m_stride + m_offset, mism, m_gaps, m_gapType );
        m_data.push_back( make_pair( (hash >> unsigned(o0*2)) & m_wordMask, a0 ) );
        m_data.push_back( make_pair( (hash >> unsigned(o1*2)) & m_wordMask, a1 ) );
    }
}

/* ========================================================================
   This should help to understand:
   ( w - window hash, mask - wpord mask, ~w - reverse window hash (~ is not C inverse here)
   ------------------------------------------------------------------------
   ACGGATGATGGAT
   2302302210 = w
   ACGGATGATGGAT
   302210 = ( w >> 0 ) & mask
   ACGGATGATGGAT
   230230 = ( w >> x ) & mask
   ACGGATGATGGAT
   3211301301 = ~w
   ACGGATGATGGAT
   321130 = ( ~w >> x ) & mask
   ACGGATGATGGAT
   301301 = ( ~w >> 0 ) & mask
   ======================================================================== */
    
END_OLIGOFAR_SCOPES

#endif
