// $Id$
#ifndef OLIGOFAR_UTIL__HPP
#define OLIGOFAR_UTIL__HPP

#include "defs.hpp"
#include <string>

BEGIN_OLIGOFAR_SCOPES
BEGIN_SCOPE( NUtil )

pair<int,int> ParseRange( const char * str, const char * delim );

inline pair<int,int> ParseRange( const string& str, const string& delim )
{
    return ParseRange( str.c_str(), delim.c_str() );
}

END_SCOPE( NUtil )
END_OLIGOFAR_SCOPES

#endif
