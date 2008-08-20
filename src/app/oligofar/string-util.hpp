// $Id$
#ifndef OLIGOFAR_STRINGUTIL__HPP
#define OLIGOFAR_STRINGUTIL__HPP

#include "defs.hpp"
#include <cstring>
#include <string>

BEGIN_OLIGOFAR_SCOPES

template<class iterator> 
inline iterator Split(const string& str, const string& delim,
                      iterator i, bool tokenize = true) 
{
	const char * c = str.c_str();
	int cnt = 0;
	while( const char * cc = strpbrk( c, delim.c_str() ) ) {
		if( cc > c || !tokenize ) *i++ = string( c, cc );
		c = cc + 1;
		if( tokenize ) while( *c && strchr( delim.c_str(), *c ) ) ++c;
		++cnt;
	}
	*i++ = c;
	return i;
}

template<class container>
inline string Join(const string& delim, const container& c) 
{
	string ret;
	typedef typename container::const_iterator iterator;
	for( iterator q = c.begin(); q != c.end(); ++q ) {
		if( q != c.begin() ) ret.append( delim );
		ret.append( *q );
	}
	return ret;
}

template<class iterator>
inline string Join(const string& delim, iterator b, iterator e) 
{
	string ret;
	for( iterator i = b; i != e; ++i ) {
		if( i != b ) ret.append( delim );
		ret.append( *i );
	}
	return ret;
}

END_OLIGOFAR_SCOPES

#endif
