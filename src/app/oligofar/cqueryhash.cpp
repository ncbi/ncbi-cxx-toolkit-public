#include <ncbi_pch.hpp>
#include "cqueryhash.hpp"
#include "chashpopulator.hpp"

USING_OLIGOFAR_SCOPES;

int CQueryHash::AddQuery( CQuery * query, int component )
{
    ASSERT( component == 0 || component == 1 );
    int len = query->GetLength( component );
    if( len == 0 ) return 0;
    Uint8 fwindow = 0;
    int offset = GetNcbi4na( fwindow, query->GetCoding(), (unsigned char*)query->GetData( component ), len );
	if( offset < 0 ) return 0;
	CHashPopulator hashPopulator( *m_permutators[m_maxMism], m_windowLength, query, m_strands, offset, component, m_maxSimplicity, fwindow, query->GetCoding() ); 
	switch( m_hashType ) {
	case eHash_vector:   return hashPopulator.PopulateHash( m_hashTableV );
	case eHash_multimap: return hashPopulator.PopulateHash( m_hashTableM );
	case eHash_arraymap: return hashPopulator.PopulateHash( m_hashTableA );
	}
	THROW( logic_error, "Unknown hash type " << m_hashType << " is set in CQueryHash::AddQuery" );
}

int CQueryHash::AddQuery( CQuery * query )
{
    int hashcnt = AddQuery( query, 0 ) + AddQuery( query, 1 );
    if( hashcnt ) m_hashedQueries++;
    return hashcnt;
}
    
// this procedure tries to trim left ambiguity characters unless this introduces more severe ambiguities
int CQueryHash::x_GetNcbi4na_ncbi8na( Uint8& window, const unsigned char * data, unsigned length )
{
    int off = 0;
    const unsigned char * t = data;
    for( int left = length - m_windowLength; left > 0; --left, ++off, ++t ) {
        if( CNcbi8naBase( t[0] ).GetAltCount() <= CNcbi8naBase( t[m_windowLength] ).GetAltCount() )
            break; // there is no benefit in shifting right
    }
    window = Ncbi8na2Ncbi4na( t, m_windowLength ); 
    return off;
}

int CQueryHash::x_GetNcbi4na_colorsp( Uint8& window, const unsigned char * data, unsigned length )
{
	// this encoding does not support ambiguities, 
    window = 0;
    // to provide uniform hashing algo, conver colors to 4-channel 1-bit per channel sets
    for( unsigned x = 0; x < m_windowLength; ++x ) 
        window |= Uint8( "\x1\x2\x4\x8"[CColorTwoBase( data[x+1] ).GetColorOrd()] ) << (x*4); 
    return 1; // always has offset of 1
}

Uint8 CQueryHash::x_Ncbipna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) { return Ncbipna2Ncbi4na( data, len, score ); }
Uint8 CQueryHash::x_Ncbiqna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) { return Ncbiqna2Ncbi4na( data, len, score ); }

// this procedure tries to perform optimization on number of alternatives 
// if one is out of allowed range
int CQueryHash::x_GetNcbi4na_ncbipna( Uint8& window, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, data, length, x_Ncbipna2Ncbi4na, 5, m_ncbipnaToNcbi4naScore, x_UpdateNcbipnaScore );
}

int CQueryHash::x_GetNcbi4na_ncbiqna( Uint8& window, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, data, length, x_Ncbiqna2Ncbi4na, 1, m_ncbiqnaToNcbi4naScore, x_UpdateNcbiqnaScore );
}

int CQueryHash::x_GetNcbi4na_quality( Uint8& window, const unsigned char * data, unsigned length, TCvt * fun, int incr, unsigned short score, TDecr * decr )
{
    int off = 0;
    const unsigned char * t = data;
    int left = length - m_windowLength;
	if( left < 0 ) return -1;

    window = fun( t, m_windowLength, score );
    Uint8 ac = Ncbi4naAlternativeCount( window, m_windowLength );

    while( ac > m_maxAlt ) {
        Uint8 aco = ~Uint8(0);
        Uint8 acm = ~Uint8(0);
        Uint8 owin = 0;
        Uint8 mwin = 0;
        if( left > 0 ) {
            owin = fun( t + incr, m_windowLength, score );
            aco  = Ncbi4naAlternativeCount( owin, m_windowLength );
        }
        if( score ) { 
            mwin = fun( t, m_windowLength, decr( score ) );
            acm  = Ncbi4naAlternativeCount( mwin, m_windowLength );
        }
        if( aco == ~Uint8(0) && acm == ~Uint8(0) ) return off;
        if( aco < m_maxAlt ) {
            if( acm < m_maxAlt ) {
                if( acm >= aco ) {
                    window = mwin;
                    return off;
                } else {
                    window = owin;
                    return off + 1;
                }
            } else {
                window = owin;
                return off + 1;
            }
        } else if( acm < m_maxAlt ) {
            window = mwin;
            return off;
        } else if( acm <= aco ) {
            score = decr( score );
            window = mwin;
            ac = acm;
        } else {
            ++off;
            if( --left < 0 ) return off;
            t += incr;
            window = owin;
            ac = aco;
        }
    }
    
    return off;
}

int CQueryHash::GetNcbi4na( Uint8& window, CSeqCoding::ECoding coding,
                            const unsigned char * data, unsigned length ) 
{
    switch( coding ) {
    case CSeqCoding::eCoding_ncbi8na: return x_GetNcbi4na_ncbi8na( window, data, length );
    case CSeqCoding::eCoding_ncbiqna: return x_GetNcbi4na_ncbiqna( window, data, length );
    case CSeqCoding::eCoding_ncbipna: return x_GetNcbi4na_ncbipna( window, data, length );
    case CSeqCoding::eCoding_colorsp: return x_GetNcbi4na_colorsp( window, data, length );
    default: THROW( logic_error, "Invalid query coding " << coding );
    }
}


    
