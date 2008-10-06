#ifndef TYPE_ARRAY_SET__HPP
#define TYPE_ARRAY_SET__HPP

#include <corelib/ncbistd.hpp>
#include <algorithm>

////////////////////////////////////////////////////////////////////////
// $Id$
////////////////////////////////////////////////////////////////////////

BEGIN_NCBI_SCOPE

template<typename T, typename Cmp = less<T> >
class array_set
{
public:
	typedef unsigned size_type;
	typedef        T value_type;
	typedef       T& reference;
	typedef const T& const_reference;

	array_set(): m_data(0), m_size(0), m_capacity(0) {}
	array_set(const array_set& a): m_data(0), m_size(0), m_capacity(0)
	{ *this = a; }
	array_set& operator = (const array_set& a) {
        if(&a == this) return *this;
		delete [] m_data;
		m_capacity = a.m_capacity;
		m_size = a.m_size;
		if( a.m_capacity == 0 ) {
			m_data = 0;
		} else {
			m_data = new T[m_capacity];
			copy(a.begin(), a.end(), m_data);
		}
		return *this;
	}

	size_type size() const { return m_size; }
	size_type capacity() const { return m_capacity; }
    void set_capacity(unsigned c) {
        if( c <= m_capacity ) return;
        T* x = new T[m_capacity = c];
        copy(m_data, m_data+m_size, x);
        delete[] m_data;
        m_data = x;
    }

	typedef T * iterator;
	typedef const T * const_iterator;

	iterator find(T a) {
		size_t pos = find_pos(a);
		return m_data[pos] == a ? m_data+pos : end() ;
	}
	const_iterator find(T a) const {
		size_t pos = find_pos(a);
		return m_data[pos] == a ? m_data+pos : end() ;
	}

	bool has(T a) const { return m_data[find_pos(a)] == a; }

	const_iterator begin() const { return m_data; }
	const_iterator end()   const { return m_data+m_size; }
	iterator begin()  { return m_data; }
	iterator end()    { return m_data+m_size; }

	array_set<T,Cmp>& operator += (const array_set<T,Cmp>& a)
	{ merge(a); return *this; }
	array_set<T,Cmp>& operator += (T a)
	{ insert(a); return *this; }
	array_set<T,Cmp>& operator -= (T a)
	{ erase(a); return *this; }
	array_set<T,Cmp>& operator -= (const array_set<T,Cmp>& a)
	{ erase(a); return *this; }

    void clear() { m_size = m_capacity = 0; delete m_data; m_data = 0; }
    void purge() { m_size = 0; }

	iterator insert(iterator i, T t) { return insert(t); }
	iterator insert(T t) {
		size_type x = find_pos(t);
		return at_insert(x,t);
	}
	void erase(iterator i) {
		if( i < begin() || i >= end() ) return;
		at_erase(i-begin());
	}
	void erase(T t) {
		size_type x = find_pos(t);
		if( x < m_size && m_data[x] == t ) at_erase(x);
	}
	void erase(const array_set<T,Cmp>& t) {
		iterator i = begin(), I = end();
		const_iterator j = t.begin(), J = t.end();
		T* x = new T[capacity()], *X = x;
		while( i != I && j != J ) {
			if( m_cmp(*i,*j) ) *X++ = *i++;
			else if( m_cmp(*j,*i) ) ++j;
			else /* *i==*j */ ++i, ++j;
		}
		if( x != X ) {
			while( i != I ) *X++ = *i++;
			delete[] m_data;
			m_data = x;
			m_size = X-x;
		}
		else delete[] x;
	}
	void merge(const array_set<T,Cmp>& a) {
		if( a.size()== 0 ) return;
		if( size() == 0) { *this = a; return; }
		if( a.size() == 1 ) { insert(*a.begin()); return; }
		size_type ns = (size()+a.size()+3)&~3;
		T * nd = new T[ns];
		T* d = nd;
		T* p = m_data,   *P = m_data+m_size;
		T* q = a.m_data, *Q = a.m_data+a.size();
		T last=0;
		while(p<P && q<Q) {
			if( *p==*q || m_cmp(*p,*q) ) { *d++ = last=*p++; }
			else if( m_cmp(*q,*p) ) { *d++ = last=*q++; }
			while( p < P && last == *p ) ++p;
			while( q < Q && last == *q ) ++q;
		}
		for( ; p<P ; ++p ) {
			if( last != *p ) *d++ = last = *p;
		}
		for( ; q<Q ; ++q ) {
			if( last != *q ) *d++ = last = *q;
		}
		delete[] m_data;
		m_data = nd;
		m_capacity = ns;
		m_size = d-nd;
	}
    void swap(array_set<T,Cmp>& a) {
        swap(m_capacity, a.m_capacity);
        swap(m_size, a.m_size);
        swap(m_data, a.m_data);
    }
protected:
	void at_erase(size_type pos) {
		if( pos > m_size ) return;
		for( T *p = m_data+pos, *q = m_data+pos+1, *Q=m_data+m_size; q < Q; ) {
			*p++ = *q++;
		}
		m_size--;
	}
	iterator at_insert(size_type pos, T val) {
		if( pos > m_size ) pos = m_size;
		if( pos < m_size && m_data[pos] == val ) return end();
		if( m_size >= m_capacity ) {
			T* x = new T[m_capacity+4];
			std::copy(m_data, m_data+pos, x);
			if( m_size > pos ) std::copy(m_data+pos, m_data+m_size, x+pos+1);
			delete [] m_data;
			m_data = x;
			m_capacity += 4;
		} else {
			if( pos<m_size ) {
				for(T* p=m_data+m_size, *q=p-1, *P=m_data+pos; p>P; )
					*p-- = *q--;
			}
		}
		m_data[pos] = val;
		m_size++;
		return m_data+pos;
	}

	size_type find_pos(T t) const {
		if( size() == 0 ) return 0;
		typedef size_type pos;
		pos a = 0;
		pos b = m_size-1;
		while(1) {
			if( !m_cmp(m_data[a],t) ) return a;
			if( !m_cmp(t,m_data[b]) ) 
                return ( m_cmp(m_data[b],t) ? m_size : b );
			pos x=(a+b+1)/2;
			if( m_cmp(t,m_data[x]) )      { if( b==x ) return b; b = x; }
			else if( m_cmp(m_data[x],t) ) { if( a==x ) return a; a = x; }
			else return x;
		}
	}
protected:
	T    * m_data;
	size_type m_size;
	size_type m_capacity;
    Cmp    m_cmp;
};

END_NCBI_SCOPE

#endif

/*
 * $Log: array_set.hpp,v $
 * Revision 1.1  2007/03/27 16:15:00  rotmistr
 * Imported to database - supports schemes, versions
 *
 * Revision 1.1  2005/09/19 16:03:16  rotmistr
 * imported from development
 *
 */

// 		template<class T, class rc_iter, class a_iter, class b_iter>
// 		void merge_uniq(rc_iter rc, a_iter a, a_iter A, b_iter b, b_iter B)
// 		{
// 			T tmp(0);
// 			while(a!=A && b!=B) {
// 				T x((*a < *b)?*a++:*b++);
// 				if(x && x!=tmp) {
// 					*rc++=tmp=x;
// 				}
// 			}
// 			for(;a!=A;++a) {
// 				if(*a && *a!=tmp) *rc++=tmp=*b;
// 			}
// 			for(;b!=B;++b) {
// 				if(*b && *b!=tmp) *rc++=tmp=*b;
// 			}
// 		}
