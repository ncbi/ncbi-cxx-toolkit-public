#ifndef OLIGOFAR_CQUERYHASH__HPP
#define OLIGOFAR_CQUERYHASH__HPP

#include "cquery.hpp"
#include "hashtables.hpp"
#include "fourplanes.hpp"
#include "cpermutator4b.hpp"
#include "array_set.hpp"
#include "chashparam.hpp"
#include "chashatom.hpp"

#include <deque>

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// 0....|....1....|....2
// 111010111111110111111 18 bits
// aaaaaaaaaaa            9 bits
//           bbbbbbbbbbb 10 bits

class CHashPopulator;
class CQueryHash : public CHashParam
{
public:
    typedef array_set<Uint1> TSkipPositions;

    typedef AVectorTable<CHashAtom> CVectorTable;
    typedef AMultimapTable<CHashAtom> CMultimapTable;
    typedef AArraymapTable<CHashAtom> CArraymapTable;

    //typedef CVectorTable THashTable;
    //typedef CMultimapTable THashTable;
    //typedef QUERY_HASH_IMPL THashTable;

	~CQueryHash();
    CQueryHash( EHashType type, unsigned winlen, int maxmism, int maxalt, double maxsimpl );

    int AddQuery( CQuery * query );
    int AddQuery( CQuery * query, int component );

    int GetHashedQueriesCount() const { return m_hashedQueries; }

    void Clear();
	void Freeze();
    void Reserve( unsigned batch );

    template<class container>
    void SetSkipPositions( container c ) { m_skipPositions.clear(); AddSkipPositions( c ); }
    template<class container> 
    void AddSkipPositions( container c ) { AddSkipPositions( c.begin(), c.end() ); }
    template<class iterator>
    void AddSkipPositions( iterator a, iterator b ) { copy( a, b, inserter( m_skipPositions, m_skipPositions.end() ) ); }

    typedef vector<CHashAtom> TMatchSet;

    class C_ListInserter
    {
    public:
        C_ListInserter( TMatchSet& ms ) : m_matchSet( ms ) {}
        void operator () ( const CHashAtom& atom ) { m_matchSet.push_back( atom ); }
    protected:
        TMatchSet& m_matchSet;
    };

    template<class Callback>
    void ForEach( Uint8 hash, Callback& callback ) const;

    bool CanOptimizeOffset() const { return m_skipPositions.size() == 0; }

    int  GetNcbi4na( Uint8& window, CSeqCoding::ECoding, const unsigned char * data, unsigned length );
    int  GetNcbi4na( Uint8& window, Uint8& windowx, CSeqCoding::ECoding, const unsigned char * data, unsigned length );

    void SetNcbipnaToNcbi4naScore( Uint2 score ) { m_ncbipnaToNcbi4naScore = score; }
    void SetNcbiqnaToNcbi4naScore( Uint2 score ) { m_ncbiqnaToNcbi4naScore = score; }
    
    void SetStrands( int s ) { m_strands = s; }

	int GetAbsoluteMaxMism() const { return m_permutators.size() - 1; }
	int GetMinimalMaxMism() const { return m_minMism; }
	int GetMaxMism() const { return m_maxMism; }

	void SetMaxMism( int i );
	void SetMinimalMaxMism( int i );
    
    void SetHashType( EHashType, int win );
    const char * GetHashTypeName() const {
        switch( m_hashType ) {
        case eHash_arraymap: return "arraymap"; 
        case eHash_multimap: return "multimap"; 
        case eHash_vector: return "vector"; 
        default: return "UNKNOWN";
        }
    }

    template <class Callback>
    class C_ForEachFilter
    {
    public:
        C_ForEachFilter( Callback& cbk, Uint2 mask, Uint2 flags ) : 
            m_callback( cbk ), m_flags( flags & mask ), m_mask( mask ) {}
        void operator () ( const CHashAtom& a ) {
            if( (a.GetFlags() & m_mask) == m_flags ) m_callback( a );
        }
    protected:
        Callback& m_callback;
        Uint2 m_flags;
        Uint2 m_mask;
    };

protected:
	typedef Uint8 TCvt( const unsigned char *, int, unsigned short );
	typedef Uint2 TDecr( Uint2 );

    int x_GetNcbi4na_ncbi8na( Uint8& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbipna( Uint8& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbiqna( Uint8& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_colorsp( Uint8& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_quality( Uint8& window, const unsigned char * data, unsigned length, TCvt * fun, int incr, Uint2 score, TDecr * decr );

    int x_GetNcbi4na_ncbi8na( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbipna( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbiqna( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_colorsp( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_quality( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length, TCvt * fun, int incr, Uint2 score, TDecr * decr );

	static Uint2 x_UpdateNcbipnaScore( Uint2 score ) { return score /= 2; }
	static Uint2 x_UpdateNcbiqnaScore( Uint2 score ) { return score - 1; }
	static TCvt  x_Ncbipna2Ncbi4na;
	static TCvt  x_Ncbiqna2Ncbi4na;

    static Uint8 x_ComputeWordRetAmbcount( Uint8& window, const unsigned char * data, int len, TCvt * fun, unsigned short score ) {
        if( len ) { window = fun( data, len, score ); return Ncbi4naAlternativeCount( window, len ); } else { window = 0; return 1; }
    }

    static bool x_AltcountOk( Uint8 ac, Uint8 acx, Uint8 limit ) { return ac < (Uint8(1) << 32) && acx < (Uint8(1) << 32)  && ac * acx <= limit; }
    Uint8 x_MakeXword( Uint8 word, Uint8 ext ) const { return ( ext | (word << m_offset)) & ((1 << m_wordLen) - 1 ); }

    int PopulateHash( int ext, CHashPopulator& hashPopulator );
    
protected:
    mutable TMatchSet m_listA, m_listB; // to keep memory space reserved between ForEach calls
	
	CVectorTable   m_hashTableV;
	CMultimapTable m_hashTableM;
	CArraymapTable m_hashTableA;

    TSkipPositions m_skipPositions;

    double m_maxSimplicity;
    
    Uint2 m_ncbipnaToNcbi4naScore;
	Uint2 m_ncbiqnaToNcbi4naScore;
    Uint2 m_strands;
	Uint2 m_minMism;
	Uint2 m_maxMism;
	Uint2 m_maxAlt;

    unsigned m_hashedQueries;
	vector<CPermutator4b*> m_permutators;
};

////////////////////////////////////////////////////////////////////////
// Implementation

inline CQueryHash::~CQueryHash() 
{
	for( int i = 0; i < int( m_permutators.size() ); ++i )
		delete m_permutators[i];
}

inline CQueryHash::CQueryHash( EHashType type, unsigned winsize, int maxm, int maxa, double maxsimpl ) : 
    CHashParam( type, winsize ),
    m_hashTableV( type == eHash_vector   ? m_wordLen : 0, maxm, maxa ), 
    m_hashTableM( type == eHash_multimap ? m_wordLen : 0, maxm, maxa ), 
    m_hashTableA( type == eHash_arraymap ? m_wordLen : 0, maxm, maxa ), 
    m_maxSimplicity( maxsimpl ), 
	m_ncbipnaToNcbi4naScore( 0x7f ), m_ncbiqnaToNcbi4naScore( 3 ), m_strands( 3 ), 
	m_minMism( 0 ), m_maxMism( 0 ), m_maxAlt( maxa ), 
    m_hashedQueries( 0 ),
    m_permutators( maxm + 1 )
{
	for( int i = 0; i <= maxm; ++i ) 
		m_permutators[i] = new CPermutator4b( i, maxa );
}

inline void CQueryHash::Clear()
{
    m_hashTableV.Clear();
    m_hashTableM.Clear();
    m_hashTableA.Clear();
    m_hashedQueries = 0;
}
inline	void CQueryHash::Freeze() 
{ 
	m_hashTableV.Freeze(); 
	m_hashTableM.Freeze(); 
	m_hashTableA.Freeze(); 
}

inline void CQueryHash::Reserve( unsigned batch ) 
{ 
	m_hashTableV.Reserve( batch ); 
	m_hashTableM.Reserve( batch ); 
	m_hashTableA.Reserve( batch ); 
}

inline void CQueryHash::SetMaxMism( int i ) 
{ 
	ASSERT( i >= m_minMism );
	ASSERT( i <= GetAbsoluteMaxMism() ); 
	m_maxMism = i; 
} 

inline void CQueryHash::SetMinimalMaxMism( int i ) 
{ 
	ASSERT( i >= 0 );
    ASSERT( i <= GetAbsoluteMaxMism() ); 
	m_minMism = i; 
	if( i > m_maxMism ) m_maxMism = i; 
}

template<class Callback>
void CQueryHash::ForEach( Uint8 hash, Callback& callback ) const 
{ 
    if( SingleHash() ) {
        Uint4 h = Uint4( hash >> ( GetOffset()*2 ) );
        if( ((hash >> GetOffset()*2) & ~CBitHacks::WordFootprint<Uint8>( 2*m_wordLen )) != 0 ) {
            THROW( logic_error, NStr::UInt8ToString( hash, 0, 2 ) << " >> " << (GetOffset()*2) <<  " = " <<  NStr::UInt8ToString( hash >> (2*GetOffset()), 0, 2 ) 
                   << "; wordLen = " << unsigned(m_wordLen) << ", ~footprint = " << NStr::UInt8ToString(~CBitHacks::WordFootprint<Uint8>( 2*m_wordLen ), 0, 2 ) );
        }
        C_ForEachFilter<Callback> cbk( callback, 0, 0 );
        switch( GetHashType() ) {
        case eHash_vector:   m_hashTableV.ForEach( h, cbk ); break;
        case eHash_multimap: m_hashTableM.ForEach( h, cbk ); break;
        case eHash_arraymap: m_hashTableA.ForEach( h, cbk ); break;
        }
    } else {
        TMatchSet& listA( m_listA );
        TMatchSet& listB( m_listB );

        listA.resize(0);
        listB.resize(0);

        Uint4 hashA = Uint4( hash >> (GetOffset()*2) );
        Uint4 hashB = Uint4( hash & (( 1 << (2*GetWordLength())) - 1) );

        C_ListInserter iA( listA );
        C_ListInserter iB( listB );

        C_ForEachFilter<C_ListInserter> cbk0( iA, CHashAtom::fFlag_words, CHashAtom::fFlag_word0 );
        C_ForEachFilter<C_ListInserter> cbk1( iB, CHashAtom::fFlag_words, CHashAtom::fFlag_word1 );

        switch( GetHashType() ) {
        case eHash_vector:   m_hashTableV.ForEach( hashA, cbk0 ); m_hashTableV.ForEach( hashB, cbk1 ); break;
        case eHash_multimap: m_hashTableM.ForEach( hashA, cbk0 ); m_hashTableM.ForEach( hashB, cbk1 ); break;
        case eHash_arraymap: m_hashTableA.ForEach( hashA, cbk0 ); m_hashTableA.ForEach( hashB, cbk1 ); break;
        }

        sort( listA.begin(), listA.end() );
        sort( listB.begin(), listB.end() );

        TMatchSet::const_iterator a = listA.begin(), A = listA.end();
        TMatchSet::const_iterator b = listB.begin(), B = listB.end();

        while( a != A && b != B ) {

            if( *a < *b ) { ++a; continue; }
            if( *b < *a ) { ++b; continue; }

            callback( a->GetStrand() == '+' ? *b : *a ); // one time is enough

            TMatchSet::const_iterator x = a; 

            do if( ! ( ++a < A ) ) return; while( *a == *x );
            do if( ! ( ++b < B ) ) return; while( *b == *x );
        }
    }
}

END_OLIGOFAR_SCOPES

#endif
