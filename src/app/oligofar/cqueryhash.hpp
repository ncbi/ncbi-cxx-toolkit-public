#ifndef OLIGOFAR_CQUERYHASH__HPP
#define OLIGOFAR_CQUERYHASH__HPP

#include "cquery.hpp"
#include "hashtables.hpp"
#include "fourplanes.hpp"
#include "cpermutator4b.hpp"
#include "array_set.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQueryHash 
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
        bool operator < ( const SHashAtom& o ) const { return query < o.query; }
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
    CQueryHash( EHashType type, int winlen, int maxmism, int maxalt, double maxsimpl );

	EHashType GetHashType() const { return m_hashType; }

    int AddQuery( CQuery * query );
    int AddQuery( CQuery * query, int component );

    int GetHashedQueriesCount() const { return m_hashedQueries; }

    void Clear();
	void Freeze();
    void Reserve( unsigned batch );

    template<class Callback, class HashImpl>
    void ForEach( unsigned hash, const HashImpl& hashTable, Callback& callback ) const { hashTable.ForEach( hash, callback ); }

    template<class Callback>
    void ForEach( unsigned hash, Callback& callback ) const { 
		switch( GetHashType() ) {
		case eHash_vector:   m_hashTableV.ForEach( hash, callback ); break;
		case eHash_multimap: m_hashTableM.ForEach( hash, callback ); break;
		case eHash_arraymap: m_hashTableA.ForEach( hash, callback ); break;
		}
	}

    int GetNcbi4na( Uint8& window, CSeqCoding::ECoding, const unsigned char * data, unsigned length );
    void SetNcbipnaToNcbi4naMask( Uint2 mask ) { m_ncbipnaToNcbi4naMask = mask; }
    
    void SetStrands( int s ) { m_strands = s; }

	int GetAbsoluteMaxMism() const { return m_permutators.size() - 1; }
	int GetMinimalMaxMism() const { return m_minMism; }
	int GetMaxMism() const { return m_maxMism; }
	void SetMaxMism( int i );
	void SetMinimalMaxMism( int i );
protected:
    int x_GetNcbi4na_ncbi8na( Uint8& window, const unsigned char * data, unsigned length );
    int x_GetNcbi4na_ncbipna( Uint8& window, const unsigned char * data, unsigned length );
    
protected:
//    THashTable m_hashTable;
//    CPermutator4b m_permutator;

	EHashType m_hashType;
	
	CVectorTable m_hashTableV;
	CMultimapTable m_hashTableM;
	CArraymapTable m_hashTableA;

    double m_maxSimplicity;
    unsigned m_windowLength;
    Uint2 m_ncbipnaToNcbi4naMask;
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

inline CQueryHash::CQueryHash( EHashType type, int winsize, int maxm, int maxa, double maxsimpl ) : 
	m_hashType( type ),
    m_hashTableV( type == eHash_vector ? winsize:0, maxm, maxa ), 
    m_hashTableM( type == eHash_multimap ? winsize:0, maxm, maxa ), 
    m_hashTableA( type == eHash_arraymap ? winsize:0, maxm, maxa ), 
    m_maxSimplicity( maxsimpl ), m_windowLength( winsize ), 
	m_ncbipnaToNcbi4naMask( 0xffc0 ), m_strands( 3 ), 
	m_minMism( 0 ), m_maxMism( 0 ), m_maxAlt( maxa ), m_permutators( maxm + 1 )
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
	ASSERT( unsigned(i) < m_permutators.size() ); 
	m_maxMism = i; 
} 

inline void CQueryHash::SetMinimalMaxMism( int i ) 
{ 
	ASSERT( i >= 0 && i <= GetAbsoluteMaxMism() ); 
	m_minMism = i; 
	if( i > m_maxMism ) m_maxMism = i; 
}

END_OLIGOFAR_SCOPES

#endif
