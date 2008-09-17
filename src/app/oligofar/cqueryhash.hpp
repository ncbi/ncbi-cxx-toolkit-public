#ifndef OLIGOFAR_CQUERYHASH__HPP
#define OLIGOFAR_CQUERYHASH__HPP

#include "cquery.hpp"
#include "hashtables.hpp"
#include "fourplanes.hpp"
#include "cpermutator4b.hpp"
#include "array_set.hpp"
#include "chashparam.hpp"

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
    struct SHashAtom
    {
        CQuery * query;
        unsigned char mism;
        unsigned char offset;
        unsigned char strand;
        unsigned char pairmate; // 0/1
        SHashAtom( CQuery * _o = 0, unsigned char _s = '+', bool matepair = false, 
                   unsigned _m = 0, int _f = 0) 
            : query( _o ), mism( _m ), offset( _f ), strand( _s ), pairmate( matepair ) {
            ASSERT( _s == '+' || _s == '-' );
        }
        bool operator == ( const SHashAtom& o ) const { return query == o.query && strand == o.strand && pairmate == o.pairmate; }
        bool operator < ( const SHashAtom& o ) const { 
            if( query < o.query ) return true;
            if( query > o.query ) return false;
            if( pairmate < o.pairmate ) return true;
            if( pairmate > o.pairmate ) return false;
            return strand < o.strand;
        }
    };

	enum EHashType {
		eHash_vector,
		eHash_arraymap,
		eHash_multimap
	};

    typedef AVectorTable<SHashAtom> CVectorTable;
    typedef AMultimapTable<SHashAtom> CMultimapTable;
    typedef AArraymapTable<SHashAtom> CArraymapTable;

    //typedef CVectorTable THashTable;
    //typedef CMultimapTable THashTable;
    //typedef QUERY_HASH_IMPL THashTable;

	~CQueryHash();
    CQueryHash( EHashType type, unsigned winlen, unsigned wordlen, unsigned winmsk, int maxmism, int maxalt, double maxsimpl );

	EHashType GetHashType() const { return m_hashType; }

    int AddQuery( CQuery * query );
    int AddQuery( CQuery * query, int component );

    int GetHashedQueriesCount() const { return m_hashedQueries; }

    void Clear();
	void Freeze();
    void Reserve( unsigned batch );

    typedef vector<SHashAtom> TMatchSet;

    class C_ListInserter
    {
    public:
        C_ListInserter( TMatchSet& ms ) : m_matchSet( ms ) {}
        void operator () ( const SHashAtom& atom ) { m_matchSet.push_back( atom ); }
    protected:
        TMatchSet& m_matchSet;
    };

    template<class Callback>
    void ForEach( Uint8 hash, Callback& callback ) const;

    bool GetOneHash() const { return m_oneHash; }
    void SetOneHash( bool to ) { m_oneHash = to; }

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
    Uint8 x_MakeXword( Uint8 word, Uint8 ext ) const { return ( ext | (word << m_offset)) & ((1 << m_wordLen[1]) - 1 ); }

    int PopulateHash( int ext, CHashPopulator& hashPopulator );
    
protected:
	EHashType m_hashType;

    mutable TMatchSet m_listA, m_listB; // to keep memory space reserved between ForEach calls
	
	CVectorTable m_hashTableV;
	CMultimapTable m_hashTableM;
	CArraymapTable m_hashTableA;

	CVectorTable m_hashTableVx;
	CMultimapTable m_hashTableMx;
	CArraymapTable m_hashTableAx;

    double m_maxSimplicity;
    
    Uint2 m_ncbipnaToNcbi4naScore;
	Uint2 m_ncbiqnaToNcbi4naScore;
    Uint2 m_strands;
	Uint2 m_minMism;
	Uint2 m_maxMism;
	Uint2 m_maxAlt;
    unsigned m_hashedQueries;
	vector<CPermutator4b*> m_permutators;

    bool m_oneHash;
};

////////////////////////////////////////////////////////////////////////
// Implementation

inline CQueryHash::~CQueryHash() 
{
	for( int i = 0; i < int( m_permutators.size() ); ++i )
		delete m_permutators[i];
}

inline CQueryHash::CQueryHash( EHashType type, unsigned winsize, unsigned wordsize, unsigned winmask, int maxm, int maxa, double maxsimpl ) : 
    CHashParam( winsize, wordsize, winmask ),
	m_hashType( type ),
    m_hashTableV( type == eHash_vector ? GetPackedSize(0):0, maxm, maxa ), 
    m_hashTableM( type == eHash_multimap ? GetPackedSize(0):0, maxm, maxa ), 
    m_hashTableA( type == eHash_arraymap ? GetPackedSize(0):0, maxm, maxa ), 
    m_hashTableVx( type == eHash_vector ? GetPackedSize(1):0, maxm, maxa ), 
    m_hashTableMx( type == eHash_multimap ? GetPackedSize(1):0, maxm, maxa ), 
    m_hashTableAx( type == eHash_arraymap ? GetPackedSize(1):0, maxm, maxa ), 
    m_maxSimplicity( maxsimpl ), 
	m_ncbipnaToNcbi4naScore( 0x7f ), m_ncbiqnaToNcbi4naScore( 3 ), m_strands( 3 ), 
	m_minMism( 0 ), m_maxMism( 0 ), m_maxAlt( maxa ), m_permutators( maxm + 1 ),
    m_oneHash( false )
{
	for( int i = 0; i <= maxm; ++i ) 
		m_permutators[i] = new CPermutator4b( i, maxa );
}

inline void CQueryHash::Clear()
{
    m_hashTableV.Clear();
    m_hashTableM.Clear();
    m_hashTableA.Clear();
    m_hashTableVx.Clear();
    m_hashTableMx.Clear();
    m_hashTableAx.Clear();
    m_hashedQueries = 0;
}
inline	void CQueryHash::Freeze() 
{ 
	m_hashTableV.Freeze(); 
	m_hashTableM.Freeze(); 
	m_hashTableA.Freeze(); 
	m_hashTableVx.Freeze(); 
	m_hashTableMx.Freeze(); 
	m_hashTableAx.Freeze(); 
}

inline void CQueryHash::Reserve( unsigned batch ) 
{ 
	m_hashTableV.Reserve( batch ); 
	m_hashTableM.Reserve( batch ); 
	m_hashTableA.Reserve( batch ); 
	m_hashTableVx.Reserve( batch ); 
	m_hashTableMx.Reserve( batch ); 
	m_hashTableAx.Reserve( batch ); 
}

inline void CQueryHash::SetMaxMism( int i ) 
{ 
	ASSERT( unsigned(i) < m_permutators.size() ); 
	m_maxMism = i; 
} 

inline void CQueryHash::SetMinimalMaxMism( int i ) 
{ 
	ASSERT( i >= 0 && i <= GetAbsoluteMaxMism() ); 
	m_minMism = i; 
	if( i > m_maxMism ) m_maxMism = i; 
}

template<class Callback>
void CQueryHash::ForEach( Uint8 hash, Callback& callback ) const 
{ 
    if( GetOffset() == 0 || m_oneHash ) {
        switch( GetHashType() ) {
        case eHash_vector:   m_hashTableV.ForEach( Uint4( hash ), callback ); break;
        case eHash_multimap: m_hashTableM.ForEach( Uint4( hash ), callback ); break;
        case eHash_arraymap: m_hashTableA.ForEach( Uint4( hash ), callback ); break;
        }
    } else {
        TMatchSet& listA( m_listA );
        TMatchSet& listB( m_listB );

        listA.resize(0);
        listB.resize(0);

        Uint4 hashA = Uint4( hash >> GetOffset()*2 );
        Uint4 hashB = Uint4( hash & (( 1 << (2*GetWordLength(1))) - 1) );
//         cerr << setw(GetWindowLength()*2) << setfill('0') << NStr::Int8ToString( hash, 0, 2 ) << "\t" << GetWindowLength() << "\n"
//              << setw(GetWordLength(0)*2) << setfill('0') << NStr::IntToString( hashA, 0, 2 ) << "\t" << GetWordLength(0) << "\n"
//              << string(2*GetOffset(),'.') << setw(GetWordLength(1)*2) << setfill('0') << NStr::IntToString( hashB, 0, 2 ) << "\t" << GetWordLength(1) << " + " << GetOffset() << "\n";
            
//             ASSERT( hashA & ~CBitHacks::WordFootprint<Uint4>( GetWordLength(0) ) == 0 );
//             ASSERT( hashB & ~CBitHacks::WordFootprint<Uint4>( GetWordLength(1) ) == 0 );
        C_ListInserter iA( listA );
        C_ListInserter iB( listB );
        switch( GetHashType() ) {
        case eHash_vector:   m_hashTableV.ForEach( hashA, iA ); m_hashTableVx.ForEach( hashB, iB ); break;
        case eHash_multimap: m_hashTableM.ForEach( hashA, iA ); m_hashTableMx.ForEach( hashB, iB ); break;
        case eHash_arraymap: m_hashTableA.ForEach( hashA, iA ); m_hashTableAx.ForEach( hashB, iB ); break;
        }
        sort( listA.begin(), listA.end() );
        sort( listB.begin(), listB.end() );
        TMatchSet::const_iterator a = listA.begin(), A = listA.end();
        TMatchSet::const_iterator b = listB.begin(), B = listB.end();
        while( a != A && b != B ) {
            if( *a < *b ) { ++a; continue; }
            if( *b < *a ) { ++b; continue; }
            callback( a->strand == '+' ? *b : *a ); // one time is enough
            TMatchSet::const_iterator x = a; 
            do if( ! ( ++a < A ) ) return; while( *a == *x );
            do if( ! ( ++b < B ) ) return; while( *b == *x );
        }
    }
}



END_OLIGOFAR_SCOPES

#endif
