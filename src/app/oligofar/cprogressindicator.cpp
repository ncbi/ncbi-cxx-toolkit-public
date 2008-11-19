#include <ncbi_pch.hpp>
#include "cprogressindicator.hpp"
#include <sstream>
#include <cmath>

USING_OLIGOFAR_SCOPES;

#ifndef _WIN32
static unsigned char * s_heapTop = (unsigned char *)sbrk(0);
#else
template <class real> real round( real x ) { return floor( x + 0.5 ); }
#endif    

inline ostream& CProgressIndicator::FormatBigNumber( ostream&o, double number ) 
{
    o.precision(-1);
    if( number >= 1e12 ) {
        o << round(number/1e11 )/10 << "T";
    } else if( number >= 1e9) {
        o << round(number/1e8 )/10 << "G";
    } else if( number >= 1e6 ) {
        o << round(number/1e5 )/10 << "M";
    } else if( number >= 1e3 ) {
        o << round(number/1e2 )/10 << "k";
    } else {
        o << round( number*10 )/10;
    }
    return o;
}


void CProgressIndicator::Format( EIndicate i, time_t t2 )
{
    int seconds = t2 - m_t0;
    char buffer[4096];
    strftime( buffer, sizeof( buffer ), "\r\x1b[K[%H:%M:%S,%F] ", localtime( &t2 ) );

    double range = m_currentValue - m_initialValue;
    ostringstream o;
    o << buffer << " ";

#ifndef _WIN32
    unsigned long long memUsed = ((unsigned char*)sbrk(0)) - s_heapTop;
    FormatBigNumber( o, memUsed );
    o << "b ";
#endif
    o << m_message << ": ";
    FormatBigNumber( o, range );
    if( m_units.length() ) o << " " << m_units;
    if( m_initialValue/m_increment < m_finalValue/m_increment ) {
        o << " of ";
        FormatBigNumber( o, m_finalValue - m_initialValue );
        if( m_units.length() ) o << " " << m_units;
    }
    if( seconds ) {
        o << " in ";
        int minutes = seconds/60;
        int hours = minutes/60;
        int days = hours/24;
        if( days ) o << days << "d ";
        if( int h = hours%24 ) o << h << "h ";
        if( int m = minutes%60 ) o << m << "m ";
        if( int s = seconds%60 ) o << s << "s ";
        o << "(";
        if( range > seconds ) {
            FormatBigNumber( o, range/seconds ) << " " << m_units << "/sec)";
        } else if ( range > minutes ) {
            o << range/minutes << " " << m_units << "/min)";
        } else if ( range > hours ) {
            o << range/hours << " " << m_units << "/hour)";
        } else {
            o << range/days << " " << m_units << "/day)";
        }
    }
    if( i == e_final ) o << "\n";
#ifndef _WIN32
    write( 2, o.str().c_str(), o.str().length() );
#else
    fwrite( o.str().c_str(), 1, o.str().length(), stderr );
#endif
    m_t1 = time(0);
}

