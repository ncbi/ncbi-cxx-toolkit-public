#include <ncbi_pch.hpp>
#include "util.hpp"
#include <cstdlib>
#include <cstring>

BEGIN_OLIGOFAR_SCOPES
BEGIN_SCOPE( NUtil )

pair<int,int> ParseRange( const char * str, const char * delim ) 
{
    const char * x = 0;
    pair<int,int> ret(0,0);
    ret.first = strtol( str, const_cast<char**>( &x ), 10 );
    if( x == 0 || x == str )
        THROW( runtime_error, "Integer or integer pair expected, got [" << str << "]" );
    if( *x == 0 ) { ret.second = ret.first; return ret; }
    if( strchr( delim, *x ) ) {
        const char * s = x+1;
        ret.second = strtol( s, const_cast<char**>( &x ), 10 );
        if( x != 0 && x != s && *x == 0 ) 
            return ret;
    }
    THROW( runtime_error, "Integer or integer pair expected, got trailing characters (" << x << ") in [" << str << "]" );
}

END_SCOPE( NUtil )
END_OLIGOFAR_SCOPES
