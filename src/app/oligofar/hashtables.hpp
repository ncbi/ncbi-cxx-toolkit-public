#ifndef OLIGOFAR_HASHTABLES__HPP
#define OLIGOFAR_HASHTABLES__HPP

#include "cquery.hpp"

BEGIN_OLIGOFAR_SCOPES

template<class Atom>
class AVectorTable 
{
public:
    typedef Atom data_type;
    typedef Atom TAtom;
    typedef vector<TAtom> TEntry;
    typedef vector<TEntry> TTable;
    
    AVectorTable( int win, int mism, int alt ) : m_table( 1 << (2*win) ) {}
    static const char * ClassName() { return "AVectorTable"; }
    static int DefaultWindowSize() { return sizeof(void*) == 8 ? 13 : 11; }
    
    template<class Callback>
    void Count( Callback& callback ) const;
    template<class Callback>
    void ForEach( Callback& callback ) const;
    template<class Callback>
    void ForEach( unsigned hash, Callback& callback ) const;
    void Insert( unsigned hash, Atom oligo );
	void Freeze() {}
	/*
    void AddCount( unsigned hash );
    void ReserveCounts();
	*/
    void Reserve( unsigned ) {}
    void Clear();
    Uint8 Size( unsigned hash ) { return m_table[hash].size(); }

    class C_ConstIterator 
    {
    public:
        unsigned GetHash() const { return m_hash; }
        const Atom& GetValue() const { return *m_pointer; }
        C_ConstIterator& operator ++ () {
            if( ++m_pointer == m_end ) {
                while( m_hash < m_table.size() && m_table[m_hash].size() == 0 ) ++m_hash;
                if( m_hash == m_table.size() ) return *this;
                m_pointer = m_table[m_hash].begin();
                m_end = m_table[m_hash].end();
            }
            return *this;
        }
        
        bool operator == ( const C_ConstIterator& other ) const;
        bool operator != ( const C_ConstIterator& other ) const { return !operator == ( other ); }
        C_ConstIterator( const TTable& t, unsigned h );
        typedef C_ConstIterator TSelf;
    protected:
        const TTable& m_table;
        unsigned m_hash;
        typename TEntry::const_iterator m_pointer, m_end;
    };
    
    typedef C_ConstIterator const_iterator;
    
    const_iterator begin() const { return C_ConstIterator( m_table, 0 ); }
    const_iterator end() const { return C_ConstIterator( m_table, m_table.size() ); }

protected:
    TTable m_table;
};

template<class Atom>
class AMultimapTable
{
public:
    typedef Atom data_type;
    typedef Atom TAtom;
    typedef multimap<unsigned,Atom> TTable;
    
    class C_ConstIterator : public multimap<unsigned,Atom>::const_iterator
    {
    public:
        typedef typename multimap<unsigned,Atom>::const_iterator TSuper;
        unsigned GetHash() const { return (*this)->first; }
        const Atom& GetValue() const { return (*this)->second; }
        C_ConstIterator( const TSuper& other ) : TSuper( other ) {}
    };

    typedef C_ConstIterator const_iterator;

    AMultimapTable( int = 0, int = 0, int = 0 ) {}
    static const char * ClassName() { return "AMultimapTable"; }
    static int DefaultWindowSize() { return 15; }

    template<class Callback>
    void ForEach( Callback& callback ) const;
    template<class Callback>
    void ForEach( unsigned hash, Callback& callback ) const;
    void Insert( unsigned hash, Atom oligo );
    void Reserve( unsigned ) {}
	void Freeze() {}
    void Clear();

    const_iterator begin() const { return m_table.begin(); }
    const_iterator end() const { return m_table.end(); }
    
protected:
    TTable m_table;
};


template<class Atom>
class AArraymapTable
{
public:
    typedef Atom data_type;
    typedef Atom TAtom;
    typedef pair<unsigned,Atom> TEntry;
    typedef vector<pair<unsigned,Atom> > TTable;
    
    class C_ConstIterator : public TTable::const_iterator
    {
    public:
        typedef typename TTable::const_iterator TSuper;
        unsigned GetHash() const { return (*this)->first; }
        const Atom& GetValue() const { return (*this)->second; }
        C_ConstIterator( const TSuper& other ) : TSuper( other ) {}
    };

    typedef C_ConstIterator const_iterator;

    AArraymapTable( int win, int mism, int alt ) { m_order = ( Uint8( pow( (double)win, mism ) ) + 1 )*Uint8( sqrt( alt ) ); }

    static const char * ClassName() { return "AArraymapTable"; }
    static int DefaultWindowSize() { return 15; }

	class C_Less
	{
	public:
		bool operator () ( const TEntry& a, const TEntry& b ) const	{ return a.first < b.first; }
	};
    
    template<class Callback>
    void ForEach( Callback& callback ) const;
    template<class Callback>
    void ForEach( unsigned hash, Callback& callback ) const;
    void Insert( unsigned hash, Atom oligo );
    void Reserve( unsigned batch ) { m_table.reserve( batch * m_order ); }
	void Freeze();
    void Clear();

    const_iterator begin() const { return m_table.begin(); }
    const_iterator end() const { return m_table.end(); }
    
protected:
    TTable m_table;
    Uint8 m_order;
};


////////////////////////////////////////////////////////////////////////
// implementations

// template <class Atom>
// inline AVectorTable<Atom>::C_ConstIterator& AVectorTable<Atom>::C_ConstIterator::operator ++ () 
// {
// }

template <class Atom>
inline AVectorTable<Atom>::C_ConstIterator::C_ConstIterator( const TTable& t, unsigned h ) : m_table( t ), m_hash( h ) 
{ 
    if( h < m_table.size() ) { m_pointer = m_table[h].begin(); m_end = m_table[h].end(); }
    else m_pointer = m_end = m_table[m_table.size()>0?m_table.size()-1:0].end();
}

template <class Atom>
inline bool AVectorTable<Atom>::C_ConstIterator::operator == ( const AVectorTable<Atom>::C_ConstIterator& other ) const {
    return m_hash == other.m_hash && ( m_hash == m_table.size() || m_pointer == other.m_pointer );
}


template<class Atom>
template<class Callback>
inline void AVectorTable<Atom>::ForEach( unsigned hash, Callback& callback ) const
{
    typedef typename TEntry::const_iterator const_iterator;

    const TEntry& e = m_table[hash];
    for( const_iterator o = e.begin(); o != e.end(); ++o ) callback( *o );
}

template<class Atom>
template<class Callback>
inline void AVectorTable<Atom>::Count( Callback& callback ) const
{
    typedef typename TTable::const_iterator const_iterator;
    for( const_iterator e = m_table.begin(); e != m_table.end(); ++e ) {
        callback( e - m_table.begin(), e->size() );
    }
}

template<class Atom>
template<class Callback>
inline void AVectorTable<Atom>::ForEach( Callback& callback ) const
{
    typedef typename TTable::const_iterator const_iterator;
    for( const_iterator e = m_table.begin(); e != m_table.end(); ++e ) {
        typedef typename TEntry::const_iterator const_iterator;
        for( const_iterator o = e->begin(); o != e->end(); ++o ) 
            callback( e - m_table.begin(), *o );
    }
}

template<class Atom>
template<class Callback>
inline void AMultimapTable<Atom>::ForEach( unsigned hash, Callback& callback ) const 
{
    typedef typename TTable::const_iterator const_iterator;

    for( const_iterator x = m_table.find( hash ); x != m_table.end() && x->first == hash; ++x )
        callback( x->second );
}

template<class Atom>
template<class Callback>
inline void AMultimapTable<Atom>::ForEach( Callback& callback ) const 
{
    typedef typename TTable::const_iterator const_iterator;

    for( const_iterator x = m_table.begin(); x != m_table.end(); ++x )
        callback( x->first, x->second );
}

template<class Atom>
inline void AVectorTable<Atom>::Insert( unsigned hash, Atom query )
{
    ASSERT(hash < m_table.size());
    m_table[hash].push_back( query );
}

/*
template<class Atom>
inline void AVectorTable<Atom>::AddCount( unsigned hash )
{
    switch( m_table[hash].size() ) {
    case 0: m_table[hash].push_back(1); break;
    case 1: m_table[hash][0]++; break;
    default: throw logic_error( string(__PRETTY_FUNCTION__) + ": something is wrong" );
    }
}

template<class Atom>
inline void AVectorTable<Atom>::ReserveCounts()
{
    typedef typename TTable::iterator iterator;
    for( iterator t = m_table.begin(); t != m_table.end(); ++t ) {
        switch( t->size() ) {
        case 0: continue;
        case 1: break;
        default: throw logic_error( string( __PRETTY_FUNCTION__ ) + ": something is very wrong" );
        }
        int c = (*t)[0];
        t->clear();
        t->reserve( c );
    }
}
*/

template<class Atom>
inline void AMultimapTable<Atom>::Insert( unsigned hash, Atom query )
{
    m_table.insert( make_pair( hash, query ) );
}

template<class Atom>
inline void AVectorTable<Atom>::Clear()
{
    unsigned x = m_table.size();
    m_table.clear();
    m_table.resize( x );
    // don't clear table - keep it reserved
}

template<class Atom>
inline void AMultimapTable<Atom>::Clear()
{
    m_table.clear();
}

template<class Atom>
template<class Callback>
inline void AArraymapTable<Atom>::ForEach( Callback& callback ) const
{
	for( typename TTable::const_iterator x = m_table.begin(); x != m_table.end(); ++x ) callback( x->first, x->second );
}

template<class Atom>
template<class Callback>
inline void AArraymapTable<Atom>::ForEach( unsigned hash, Callback& callback ) const
{
    TEntry e( hash, 0 );
	typename TTable::const_iterator x = lower_bound( m_table.begin(), m_table.end(), e, C_Less() );
	for( ; x != m_table.end() && x->first == hash ; ++x ) callback( x->second );
}

template<class Atom>
inline void AArraymapTable<Atom>::Insert( unsigned hash, Atom oligo )
{
	m_table.push_back( make_pair( hash, oligo ) );
}

template<class Atom>
inline void AArraymapTable<Atom>::Freeze()
{
	sort( m_table.begin(), m_table.end() );
}

template<class Atom>
inline void AArraymapTable<Atom>::Clear()
{
	m_table.clear();
}

END_OLIGOFAR_SCOPES

#endif
