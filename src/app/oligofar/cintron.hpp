#ifndef OLIGOFAR_CINTRON__HPP
#define OLIGOFAR_CINTRON__HPP

#include "debug.hpp"
#include "array_set.hpp"

BEGIN_OLIGOFAR_SCOPES

class CIntron
{
public:
    CIntron() : m_pos( 0 ), m_min( 0 ), m_max( 0 ) {}
    CIntron( int pos, int sz = 0 ) : m_pos( pos ), m_min( sz ), m_max( sz ) {}
    CIntron( int pos, const CIntron& other ) : m_pos( pos ), m_min( other.GetMin() ), m_max( other.GetMax() ) {}
    CIntron( const char * _arg );
    int GetPos() const { return m_pos; }
    int GetMin() const { return m_min; }
    int GetMax() const { return m_max; }
    bool operator < ( const CIntron& other ) const { return GetPos() < other.GetPos(); }
    bool operator == ( const CIntron& other ) const { return GetPos() == other.GetPos(); }
    typedef array_set<CIntron> TSet;
protected:
    int m_pos;
    int m_min;
    int m_max;
};

inline ostream& operator << ( ostream& out, const CIntron& i ) 
{
    if( i.GetMin() == i.GetMax() ) 
        return out << i.GetPos() << "(" << i.GetMin() << ")";
    else 
        return out << i.GetPos() << "(" << i.GetMin() << ":" << i.GetMax() << ")";
}

inline CIntron::CIntron( const char * _arg ) 
{
    const char * arg = _arg;
    if( !isdigit( *arg ) ) 
        THROW( runtime_error, "Can't parse splice definition [" << _arg << "]: no position is set" );
    m_pos = strtol( arg, const_cast<char**>(&arg), 10 );
    if( arg == 0 || *arg != '(' )
        THROW( runtime_error, "Can't parse splice definition [" << _arg << "]: open paranthesis is expected" );
    ++arg;
    if( !(isdigit( *arg ) || *arg == '-' ))
        THROW( runtime_error, "Can't parse splice definition [" << _arg << "]: number is expected in paranthesis" );
    m_min = m_max = strtol( arg, const_cast<char**>(&arg), 10 );
    switch( *arg ) {
        case ')': return;
        case ':': ++arg; break;
        default: 
            THROW( runtime_error, "Can't parse splice definition [" << _arg << "]: closing paranthesis or colon is expected" );
    }
    if( !(isdigit( *arg ) || *arg == '-' ))
        THROW( runtime_error, "Can't parse splice definition [" << _arg << "]: number is expected in paranthesis" );
    m_max = strtol( arg, const_cast<char**>(&arg), 10 );
    switch( *arg ) {
        case ')': return;
        default: 
            THROW( runtime_error, "Can't parst splice definition [" << _arg << "]: closing paranthesis is expected" );
    }
    if( m_max < m_min ) swap( m_max, m_min );
}

END_OLIGOFAR_SCOPES

#endif
