#ifndef OLIGOFAR_CWORDHASH__HPP
#define OLIGOFAR_CWORDHASH__HPP

#include "cbithacks.hpp"
#include "chashatom.hpp"

BEGIN_OLIGOFAR_SCOPES

class CQuery;
class CWordHash
{
public:
    typedef CHashAtom value_type;
    typedef value_type& reference_type;
    typedef const value_type& const_reference_type;

    typedef vector<value_type> TArray;
    typedef vector<TArray> TTable;

    int GetIndexBits() const { return m_indexBits; }
    Uint4 GetIndexMask() const { return m_indexMask; }

    CWordHash( int bits = 20 ) : m_indexBits(0), m_indexMask(0) { SetIndexBits( bits ); }

    void SetIndexBits( int bits ) {
        ASSERT( bits >= 0 && bits < 32 );
        m_indexBits = bits;
        m_indexMask = CBitHacks::WordFootprint<Uint4>( bits );
        Clear();
//        cerr << "Setting index bits: " << int( m_indexBits ) << " mask " << hex << m_indexMask << dec << " table size " << m_table.size() << endl;
    }

    void Clear() {
        m_table.clear();
        m_table.resize( 1 << m_indexBits );
        m_sorted = true;
    }

    void Sort() { for( Uint4 x = 0; x < m_table.size(); ++x ) { sort( m_table[x].begin(), m_table[x].end(), value_type::LessSubkey ); } m_sorted = true; }

    void AddEntry( Uint4 key, const value_type& item ) { 
        TArray& a( m_table[key & m_indexMask] );
        a.push_back( item ); 
        a.back().SetSubkey( key >> m_indexBits ); 
        m_sorted = false;
    }

    template<class Uint>
    static string Bits(Uint i) { return CBitHacks::AsBits( i ); }

    template<typename Callback>
    void ForEach( Uint4 key, Callback callback ) const {
        ASSERT( m_sorted );
        const TArray& a = m_table[key & m_indexMask];
        Uint2 subkey = key >> m_indexBits;
        TArray::const_iterator x = lower_bound( a.begin(), a.end(), value_type( subkey ), value_type::LessSubkey );
        for( ; x != a.end() && ! value_type::LessSubkey( subkey, x->GetSubkey() ) ; ++x ) callback( *x );
    }

    template<typename Callback>
    void ForEach( Uint4 key, Callback callback, Uint1 mask, Uint1 flags ) const {
        ASSERT( m_sorted );
        flags &= mask;
        const TArray& a = m_table[key & m_indexMask];
        Uint2 subkey = key >> m_indexBits;
        TArray::const_iterator x = lower_bound( a.begin(), a.end(), value_type( subkey ), value_type::LessSubkey ); 
        for( ; x != a.end() && ! value_type::LessSubkey( subkey, x->GetSubkey() ) ; ++x ) {
            if( x->GetFlags( mask ) == flags ) 
                callback( *x );
        }
    }

protected:
    Uint1 m_indexBits;
    Uint4 m_indexMask;
    TTable m_table;
    bool m_sorted;
};

END_OLIGOFAR_SCOPES

#endif
