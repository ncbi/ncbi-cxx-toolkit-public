#ifndef OLIGOFAR_CPERMUTATOR8b__HPP
#define OLIGOFAR_CPERMUTATOR8b__HPP

#include "cbithacks.hpp"

#include <vector>

BEGIN_OLIGOFAR_SCOPES

class CPermutator8b
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
        C_Entry() : m_ambiguities(0) { m_atoms.reserve(16*12); }
        int GetAmbiguities() const { return m_ambiguities; }
        const_iterator begin() const { return m_atoms.begin(); }
        const_iterator end() const { return m_atoms.end(); }
        size_t size() const { return m_atoms.size(); }
        void Sort() { sort( m_atoms.begin(), m_atoms.end() ); }
        void SetAmbiguities( unsigned a ) { m_ambiguities = a; }
        void push_back( const C_Atom& a ) { m_atoms.push_back( a ); }
    protected:
        int m_ambiguities;
        vector<value_type> m_atoms;
    };

    typedef C_Entry TEntry;
    typedef vector<TEntry> TTable;
        
protected:
    TEntry& SetEntry( unsigned short ncbi4na, int bases = 4 ) { return m_data[--bases&3][ncbi4na]; }
public:

    CPermutator8b( int maxMism = 1, int maxAmb = 4 );

    unsigned GetMaxMismatches() const { return m_maxMism; }
    unsigned GetMaxAmbiguities() const { return m_maxAmb; }
    
    typedef TEntry::const_iterator const_iterator;
    
    const TEntry& GetEntry( unsigned short ncbi4na, int bases = 4 ) const { return m_data[--bases&3][ncbi4na]; }

    const_iterator begin( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).begin(); }
    const_iterator end( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).end(); }
    size_t size( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).size(); }

    unsigned GetAmbiquities( unsigned short ncbi4na, int bases = 4 ) const { return GetEntry( ncbi4na, bases ).GetAmbiguities(); }

    template<class Callback>
    void ForEachQuad( int quad, unsigned w, const UintH& ncbi4na, Callback& callback,
                      int m = 0, int a = 0, Uint8 h = 0 ) const {
        w = ( w - 1 ) % 4 + 1;
        Uint4 x = Uint4(ncbi4na >> (quad*16)) & (( 1 << (4*w)) - 1);
        const TEntry& e = GetEntry( x, w );
        int ai = a + e.GetAmbiguities();
        if( ai > m_maxAmb ) return;
        for( const_iterator i = e.begin(), I = e.end(); i != I; ++i ) {
            int mi = m + i->GetMismatches();
            if( mi > m_maxMism ) break;
            if( quad == 0 ) callback( h | i->GetHash(), mi, ai );
            else {
                Uint8 hi = h | (Uint8( i->GetHash()) << (8 * quad));
                ForEachQuad( quad - 1, quad*4, ncbi4na, callback, mi, ai, hi );
            }
        }
    }

    template<class Callback>
    void ForEach( unsigned windowSize, const UintH& ncbi4na, Callback& callback, unsigned m = 0 ) const {
        ForEachQuad( (windowSize - 1)/4, windowSize, ncbi4na, callback, m );
    }

protected:
    int m_maxMism;
    int m_maxAmb;

    TTable m_data[4];

    static unsigned s_ncbi2na_ncbi4na_2bases[];
    static unsigned s_ncbi2na_ncbi4na_4bases[];
    static bool     s_static_tables_inited;
    
protected:

    static bool x_InitStaticTables();
    
};

END_OLIGOFAR_SCOPES

#endif

