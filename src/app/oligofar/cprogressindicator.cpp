#include <ncbi_pch.hpp>
#include "cprogressindicator.hpp"
#include <sstream>
#include <cmath>

USING_OLIGOFAR_SCOPES;

namespace 
{

    unsigned char * s_heapTop = (unsigned char *)sbrk(0);
    
    inline ostream& FormatBigNumber( ostream&o, double number ) 
    {
        if( number >= 1e13 ) {
            o << round(number/1e12) << "T";
        } else if( number >= 1e10 ) {
            o << round(number/1e9) << "G";
        } else if( number >= 1e7 ) {
            o << round(number/1e6) << "M";
        } else if( number >= 1e4 ) {
            o << round(number/1e3) << "k";
        } else {
            o << round(number);
        }
        return o;
    }
    

    void CProgressIndicator::Format( EIndicate i, time_t t2 )
    {
        int seconds = t2 - m_t0;
        char buffer[4096];
        strftime( buffer, sizeof( buffer ), "\r\x1b[K[%H:%M:%S,%F] ", localtime( &t2 ) );
        double range = m_currentValue - m_initialValue;
        unsigned long long memUsed = ((unsigned char*)sbrk(0)) - s_heapTop;

        ostringstream o;
        o << buffer << " ";
        FormatBigNumber( o, memUsed );
        o << "b " << m_message << ": ";
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
        write( 2, o.str().c_str(), o.str().length() );
        m_t1 = time(0);
    }
}

