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
	CHashPopulator hashPopulator( *m_permutators[m_maxMism], m_windowLength, query, m_strands, offset, component, m_maxSimplicity, fwindow ); 
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
        unsigned char x = *t;
        if( Ncbi4naAlternativeCount( x ) <= Ncbi4naAlternativeCount( t[m_windowLength] ) ) 
            break; // there is no benefit in shifting right
    }
    window = Ncbi8na2Ncbi4na( t, m_windowLength ); 
    return off;
}

// this procedure tries to perform optimization on number of alternatives 
// if one is out of allowed range
int CQueryHash::x_GetNcbi4na_ncbipna( Uint8& window, const unsigned char * data, unsigned length )
{
    int off = 0;
    const unsigned char * t = data;
    unsigned short mask = m_ncbipnaToNcbi4naMask;
    int left = length - m_windowLength;

    window = Ncbipna2Ncbi4na( t, m_windowLength, mask );
    Uint8 ac = Ncbi4naAlternativeCount( window, m_windowLength );

    while( ac > m_maxAlt ) {
        Uint8 aco = ~Uint8(0);
        Uint8 acm = ~Uint8(0);
        Uint8 owin = 0;
        Uint8 mwin = 0;
        if( left ) {
            owin = Ncbipna2Ncbi4na( t+5, m_windowLength, mask );
            aco  = Ncbi4naAlternativeCount( owin, m_windowLength );
        }
        if( mask ) {
            mwin = Ncbipna2Ncbi4na( t, m_windowLength, mask << 1 );
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
            mask <<= 1;
            window = mwin;
            ac = acm;
        } else {
            ++off;
            --left;
            t += 5;
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
    case CSeqCoding::eCoding_ncbipna: return x_GetNcbi4na_ncbipna( window, data, length );
    default: THROW( logic_error, "Invalid query coding " << coding );
    }
}


    
