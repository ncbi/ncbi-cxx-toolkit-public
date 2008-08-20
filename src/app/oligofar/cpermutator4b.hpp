#ifndef OLIGOFAR_CPERMUTATOR4b__HPP
#define OLIGOFAR_CPERMUTATOR4b__HPP

//#include "cbasemask.hpp"
#include "iupac-util.hpp"

#include <vector>

BEGIN_OLIGOFAR_SCOPES

class CPermutator4b
{
protected:
    class C_Atom
    {
    public:
        unsigned char GetHash() const { return m_hash; }
        unsigned char GetMismatches() const { return m_mismatches; }
		bool operator < ( const C_Atom& a ) const { return GetMismatches() < a.GetMismatches(); }
        C_Atom( unsigned char hash, unsigned char mismatches ) 
			: m_hash( hash ), m_mismatches( mismatches ) {}
    protected:
        unsigned char m_hash;
        unsigned char m_mismatches;
    };
	
	class C_Entry
	{
	public:
		typedef C_Atom value_type;
		typedef vector<value_type>::const_iterator const_iterator;
		C_Entry() : m_alternatives(0) { m_atoms.reserve(16*12); }
        unsigned char GetAlternatives() const { return m_alternatives; }
		const_iterator begin() const { return m_atoms.begin(); }
		const_iterator end() const { return m_atoms.end(); }
		size_t size() const { return m_atoms.size(); }
		void Sort() { sort( m_atoms.begin(), m_atoms.end() ); }
		void SetAlternatives( unsigned a ) { m_alternatives = a; }
		void push_back( const C_Atom& a ) { m_atoms.push_back( a ); }
	protected:
		// alternatives is a number of variants of ncbi2na with no mismatches (considering N-A = match)
        unsigned m_alternatives;
		vector<value_type> m_atoms;
	};

    typedef C_Entry TEntry;
    typedef vector<TEntry> TTable;
        
protected:
	TEntry& SetEntry( unsigned short ncbi4na, int bases = 4 ) { return m_data[--bases&3][ncbi4na]; }
public:

    CPermutator4b( int maxMism = 1, int maxAlt = 256 );

    unsigned GetMaxMismatches() const { return m_maxMism; }
    unsigned GetMaxAlternatives() const { return m_maxAlt; }
    
    typedef TEntry::const_iterator const_iterator;
	
	const TEntry& GetEntry( unsigned short ncbi4na, int bases = 4 ) const { return m_data[--bases&3][ncbi4na]; }

    const_iterator begin( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).begin(); }
    const_iterator end( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).end(); }
    size_t size( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).size(); }

	unsigned GetAlternatives( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).GetAlternatives(); }

    template<class Callback>
    void ForEach1q( unsigned w, Uint8 ncbi4na, Callback& callback, 
                    unsigned m = 0, Uint8 a = 1, unsigned h = 0 ) const {
        w = (w - 1) % 4 + 1;
        unsigned x = ncbi4na & ((1<<(4*w))-1);
		const TEntry& e = GetEntry( x, w );
        Uint8 ai = a * e.GetAlternatives();
		if( ai == 0 || ai > m_maxAlt ) return;
        for( const_iterator i = e.begin(), I = e.end(); i!=I; ++i ) {
            unsigned si =  m + i->GetMismatches();
            if( si > m_maxMism ) break;
            callback( h | i->GetHash(), si, ai );
        }
    }
    
    template<class Callback>
    void ForEach2q( unsigned w, Uint8 ncbi4na, Callback& callback,
                    unsigned m = 0, Uint8 a = 1, unsigned h = 0 ) const {
        w = (w - 1) % 4 + 1;
        unsigned x = (ncbi4na >> 16) & ((1<<(4*w))-1);
		const TEntry& e = GetEntry( x, w );
        Uint8 ai = a * e.GetAlternatives();
		if( ai == 0 || ai > m_maxAlt ) return;
        for( const_iterator i = e.begin(), I = e.end(); i!=I; ++i ) {
            unsigned si = m + i->GetMismatches();
            if( si > m_maxMism ) break;
            unsigned hi = h | ( unsigned(i->GetHash()) << 8 );
            ForEach1q( 4, ncbi4na, callback, si, ai, hi );
        }
    }
    
    template<class Callback>
    void ForEach3q( unsigned w, Uint8 ncbi4na, Callback& callback,
                    unsigned m = 0, Uint8 a = 1, unsigned h = 0 ) const {
        w = (w - 1) % 4 + 1;
        unsigned x = (ncbi4na >> 32) & ((1<<(4*w))-1);
		const TEntry& e = GetEntry( x, w );
        Uint8 ai = a * e.GetAlternatives();
		if( ai == 0 || ai > m_maxAlt ) return;
        for( const_iterator i = e.begin(), I = e.end(); i!=I; ++i ) {
            unsigned si = m + i->GetMismatches();
            if( si > m_maxMism ) break;
            unsigned hi = h | ( unsigned(i->GetHash()) << 16 );
            ForEach2q( 8, ncbi4na, callback, si, ai, hi );
        }
    }

    template<class Callback>
    void ForEach4q( unsigned w, Uint8 ncbi4na, Callback& callback, unsigned m = 0 ) const {
        w = (w - 1) % 4 + 1;
        unsigned x = (ncbi4na >> 48) & ((1<<(4*w))-1);
		const TEntry& e = GetEntry( x, w );
        Uint8 ai = e.GetAlternatives();
		if( ai == 0 || ai > m_maxAlt ) return;
        for( const_iterator i = e.begin(), I = e.end(); i!=I; ++i ) {
            unsigned si = m + i->GetMismatches();
            if( si > m_maxMism ) break;
            unsigned hi = ( unsigned(i->GetHash()) << 24 );
            ForEach3q( 12, ncbi4na, callback, si, ai, hi );
        }
    }

    template<class Callback>
    void ForEach( unsigned windowSize, Uint8 ncbi4na, Callback& callback, unsigned m = 0 ) const {
        if( windowSize > 12 ) ForEach4q( windowSize, ncbi4na, callback, m );
        else if( windowSize > 8 ) ForEach3q( windowSize, ncbi4na, callback, m );
        else if( windowSize > 4 ) ForEach2q( windowSize, ncbi4na, callback, m );
        else ForEach1q( windowSize, ncbi4na, callback, m );
    }

protected:
    unsigned m_maxMism;
    unsigned m_maxAlt;

    TTable m_data[4];

    static unsigned s_ncbi2na_ncbi4na_2bases[];
    static unsigned s_ncbi2na_ncbi4na_4bases[];
    static bool     s_static_tables_inited;
    
protected:

    static bool x_InitStaticTables();
    
};

END_OLIGOFAR_SCOPES

#endif

