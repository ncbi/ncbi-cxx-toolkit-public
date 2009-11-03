#ifndef TYPE_ARRAY_MAP__HPP
#define TYPE_ARRAY_MAP__HPP

#include <corelib/ncbistd.hpp>
#include <algorithm>

////////////////////////////////////////////////////////////////////////
// $Id$
////////////////////////////////////////////////////////////////////////

BEGIN_NCBI_SCOPE

template<typename K, typename V, typename Cmp = less<K>, int kCapacityIncrement = 10000 >
class array_map
{
public:
    typedef unsigned size_type;
    typedef pair<K,V> data_type;
    typedef        K key_type;
    typedef        V value_type;
    typedef       V& reference;
    typedef const V& const_reference;

    ~array_map() { delete[] m_data; }
    array_map(): m_data(0), m_size(0) /*, m_capacity(0)*/ {}
    array_map(const array_map& a): m_data(0), m_size(0), m_capacity(0)
    { assign( a ); }
    array_map& operator = (const array_map& a) { return assign( a ); }
    array_map& assign( const array_map& a ) {
        if(&a == this) return *this;
        delete [] m_data;
        m_capacity = a.m_capacity;
        m_size = a.m_size;
        if( a.m_capacity == 0 ) {
            m_data = 0;
        } else {
            m_data = new data_type[m_capacity];
            copy(a.begin(), a.end(), m_data);
        }
        return *this;
    }

    bool empty() const { return m_size == 0; }
    size_type size() const { return m_size; }
    size_type capacity() const { return m_capacity; }
    void set_capacity(unsigned c) {
        if( c <= m_capacity ) return;
        data_type * x = new data_type[m_capacity = c];
        copy( m_data, m_data + m_size, x );
        delete[] m_data;
        m_data = x;
    }
    void reserve( size_t capacity ) { set_capacity( capacity ); }

    typedef data_type * iterator;
    typedef const data_type * const_iterator;

    iterator find(K a) {
        size_t pos = find_pos(a);
        return (pos < m_size && m_data[pos].first == a) ? m_data + pos : end() ;
    }
    const_iterator find(K a) const {
        size_t pos = find_pos(a);
        return (pos < m_size && m_data[pos].first == a) ? m_data + pos : end() ;
    }

    bool has(K a) const { return m_data[find_pos(a)].first == a; }

    const_iterator begin() const { return m_data; }
    const_iterator end()   const { return m_data + m_size; }
    iterator begin()  { return m_data; }
    iterator end()    { return m_data+m_size; }

    array_map<K,V,Cmp>& operator += (const array_map<K,V,Cmp>& a)
    { merge(a); return *this; }
    array_map<K,V,Cmp>& operator += (const data_type& a)
    { insert(a); return *this; }
    array_map<K,V,Cmp>& operator -= (const data_type& a)
    { erase(a); return *this; }
    array_map<K,V,Cmp>& operator -= (const array_map<K,V,Cmp>& a)
    { erase(a); return *this; }

    void clear() { m_size = m_capacity = 0; delete[] m_data; m_data = 0; }
    void purge() { m_size = 0; }

    const_reference operator [] ( K k ) const { return find(k)->second; }
    reference operator [] ( K k ) { 
        iterator x = find( k );
        if( x == end() ) x = insert( x, make_pair( k, V(0) ) );
        return x->second;
    }

    iterator insert( iterator i, const data_type& t ) { 
        if( i != end() && i->first == t.first ) {
            i->second = t.second;
            return i;
        } else return insert(t); 
    }
    iterator insert( const data_type& t ) {
        size_type x = find_pos( t.first );
        return at_insert( x, t );
    }
    void erase( iterator i ) {
        if( i < begin() || i >= end() ) return;
        at_erase( i - begin() );
    }
    void erase( K t ) {
        size_type x = find_pos( t );
        if( x < m_size && m_data[x].first == t ) at_erase( x );
    }
    void erase( const array_map<K,V,Cmp>& t ) {
        iterator i = begin(), I = end();
        const_iterator j = t.begin(), J = t.end();
        data_type* x = new data_type[ capacity() ], *X = x;
        while( i != I && j != J ) {
            if( m_cmp( i->first, j->first ) ) *X++ = *i++;
            else if( m_cmp( j->first, i->first ) ) ++j;
            else /* *i==*j */ ++i, ++j;
        }
        if( x != X ) {
            while( i != I ) *X++ = *i++;
            delete[] m_data;
            m_data = x;
            m_size = X - x;
        }
        else delete[] x;
    }
    void merge( const array_map<K,V,Cmp>& a ) {
        if( a.size()== 0 ) return;
        if( size() == 0) { *this = a; return; }
        if( a.size() == 1 ) { insert( *a.begin() ); return; }
        size_type ns = ( size() + a.size() + 3 ) & ~3;
        data_type * nd = new data_type[ns];
        data_type * d = nd;
        data_type * p = m_data,   *P = m_data + m_size;
        data_type * q = a.m_data, *Q = a.m_data + a.size();
        K last = 0;
        while( p < P && q < Q )  {
            if( p->first == q->first || m_cmp( p->first, q->first ) ) { last = (*d++ = *p++)->first; }
            else if( m_cmp( q->first, p->first ) ) { last = (*d++ = *q++)->first; }
            while( p < P && last == p->first ) ++p;
            while( q < Q && last == q->first ) ++q;
        }
        for( ; p < P ; ++p ) {
            if( last != p->first ) last = (*d++ = *p)->first;
        }
        for( ; q < Q ; ++q ) {
            if( last != q->first ) last = (*d++ = *q)->first;
        }
        delete[] m_data;
        m_data = nd;
        m_capacity = ns;
        m_size = d - nd;
    }
    void swap( array_map<K,V,Cmp>& a ) {
        swap( m_capacity, a.m_capacity );
        swap( m_size, a.m_size );
        swap( m_data, a.m_data );
    }
protected:
    void at_erase( size_type pos ) {
        if( pos > m_size ) return;
        for( data_type * p = m_data + pos, *q = m_data + pos + 1, * Q = m_data + m_size; q < Q; ) {
            *p++ = *q++;
        }
        m_size--;
    }
    iterator at_insert( size_type pos, const data_type& val ) {
        if( pos > m_size ) pos = m_size;
        if( pos < m_size && m_data[pos] == val ) return end();
        if( m_size >= m_capacity ) {
            if( kCapacityIncrement ) m_capacity += kCapacityIncrement;
            else if( m_capacity ) m_capacity <<= 1; 
            else m_capacity = 1;
            data_type * x = new data_type[m_capacity];
            std::copy( m_data, m_data + pos, x );
            if( m_size > pos ) std::copy( m_data + pos, m_data + m_size, x + pos + 1 );
            delete [] m_data;
            m_data = x;
            //m_capacity *= 2;
        } else {
            if( pos < m_size ) {
                for( data_type * p = m_data + m_size, *q = p - 1, *P = m_data + pos; p > P; )
                    *p-- = *q--;
            }
        }
        m_data[pos] = val;
        m_size++;
        return m_data + pos;
    }

    size_type find_pos( K t ) const {
        if( size() == 0 ) return 0;
        typedef size_type pos;
        pos a = 0;
        pos b = m_size - 1;
        while(1) {
            if( !m_cmp( m_data[a].first, t ) ) return a;
            if( !m_cmp( t, m_data[b].first ) ) 
                return ( m_cmp( m_data[b].first, t ) ? m_size : b );
            pos x = (a + b + 1)/2;
            if( m_cmp( t, m_data[x].first ) )      { if( b == x ) return b; b = x; }
            else if( m_cmp( m_data[x].first, t ) ) { if( a == x ) return a; a = x; }
            else return x;
        }
    }
protected:
    data_type * m_data;
    size_type m_size;
    size_type m_capacity;
    Cmp    m_cmp;
};

END_NCBI_SCOPE

#endif

