#include <ncbi_pch.hpp>
#include "cqueryhash.hpp"
#include "chashpopulator.hpp"

USING_OLIGOFAR_SCOPES;

int CQueryHash::PopulateHash( int x, CHashPopulator& hashPopulator ) 
{
    switch( m_hashType ) {
    case eHash_vector:   return hashPopulator.PopulateHash( x ? m_hashTableVx : m_hashTableV );
    case eHash_multimap: return hashPopulator.PopulateHash( x ? m_hashTableMx : m_hashTableM );
    case eHash_arraymap: return hashPopulator.PopulateHash( x ? m_hashTableAx : m_hashTableA );
    }
    THROW( logic_error, "Unknown hash type in CQueryHash::PopulateHash" );
}

int CQueryHash::AddQuery( CQuery * query, int component )
{
    ASSERT( component == 0 || component == 1 );
    int len = query->GetLength( component );
    if( len == 0 ) return 0;
    if( GetOffset() == 0 || m_oneHash ) {
        Uint8 fwindow = 0;
        int offset = GetNcbi4na( fwindow, query->GetCoding(), (unsigned char*)query->GetData( component ), len );
		if( offset < 0 ) return 0;
		CHashPopulator hashPopulator( *m_permutators[m_maxMism], m_wordLen[0], query, m_strands, offset, component, m_maxSimplicity, fwindow, query->GetCoding() ); 
        return PopulateHash( 0, hashPopulator );
    } else {
        Uint8 fwindow = 0;
        Uint8 fwindowx = 0;
        int offset = GetNcbi4na( fwindow, fwindowx, query->GetCoding(), (unsigned char *)query->GetData( component ), len );
        if( offset < 0 ) return 0;
        int count = 0;
        if( m_strands & 0x1 ) {
            CHashPopulator hashPopulator ( *m_permutators[m_maxMism], m_wordLen[1], query, 1, offset + GetOffset(), component, m_maxSimplicity, fwindowx, query->GetCoding() );
            CHashPopulator hashPopulatorX( *m_permutators[m_maxMism], m_wordLen[0], query, 1, offset, component, m_maxSimplicity, fwindow, query->GetCoding() );
            count += PopulateHash( 0, hashPopulator );
            count += PopulateHash( 1, hashPopulatorX );
        }
        if( m_strands & 0x2 ) {
            CHashPopulator hashPopulator ( *m_permutators[m_maxMism], m_wordLen[1], query, 2, offset + GetOffset(), component, m_maxSimplicity, fwindowx, query->GetCoding() );
            CHashPopulator hashPopulatorX( *m_permutators[m_maxMism], m_wordLen[0], query, 2, offset, component, m_maxSimplicity, fwindow, query->GetCoding() );
            count += PopulateHash( 1, hashPopulator );
            count += PopulateHash( 0, hashPopulatorX ); // since order of words changes to opposite
        }
//         cerr << "\t" << GetWindowLength() << " = " << count << "\n"
//              << setw(GetWordLength(0)*4) << setfill('0') << NStr::IntToString( fwindowx, 0, 2 ) << "\t" << GetWordLength(0) << "\n"
//              << string(4*GetOffset(),'.') << setw(GetWordLength(1)*4) << setfill('0') << NStr::IntToString( fwindow, 0, 2 ) << "\t" << GetWordLength(1) << " + " << GetOffset() << "\n";
        return count;
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
    for( int left = length - m_wordLen[0]; left > 0; --left, ++off, ++t ) {
        if( CNcbi8naBase( t[0] ).GetAltCount() <= CNcbi8naBase( t[m_wordLen[0]] ).GetAltCount() )
            break; // there is no benefit in shifting right
    }
    window = Ncbi8na2Ncbi4na( t, m_wordLen[0] ); 
    return off;
}

int CQueryHash::x_GetNcbi4na_colorsp( Uint8& window, const unsigned char * data, unsigned length )
{
	// this encoding does not support ambiguities, 
    window = 0;
    // to provide uniform hashing algo, conver colors to 4-channel 1-bit per channel sets
    for( unsigned x = 0; x < m_wordLen[0]; ++x ) 
        window |= Uint8( "\x1\x2\x4\x8"[CColorTwoBase( data[x+1] ).GetColorOrd()] ) << (x*4); 
    return 1; // always has offset of 1
}

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
    int left = length - m_wordLen[0];
	if( left < 0 ) return -1;

    window = fun( t, m_wordLen[0], score );
    Uint8 ac = Ncbi4naAlternativeCount( window, m_wordLen[0] );

    while( ac > m_maxAlt ) {
        Uint8 aco = ~Uint8(0);
        Uint8 acm = ~Uint8(0);
        Uint8 owin = 0;
        Uint8 mwin = 0;
        if( left > 0 ) {
            owin = fun( t + incr, m_wordLen[0], score );
            aco  = Ncbi4naAlternativeCount( owin, m_wordLen[0] );
        }
        if( score ) { 
            mwin = fun( t, m_wordLen[0], decr( score ) );
            acm  = Ncbi4naAlternativeCount( mwin, m_wordLen[0] );
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


int CQueryHash::GetNcbi4na( Uint8& window, Uint8& windowx, CSeqCoding::ECoding coding,
                            const unsigned char * data, unsigned length ) 
{
    switch( coding ) {
    case CSeqCoding::eCoding_ncbi8na: return x_GetNcbi4na_ncbi8na( window, windowx, data, length );
    case CSeqCoding::eCoding_ncbiqna: return x_GetNcbi4na_ncbiqna( window, windowx, data, length );
    case CSeqCoding::eCoding_ncbipna: return x_GetNcbi4na_ncbipna( window, windowx, data, length );
    case CSeqCoding::eCoding_colorsp: return x_GetNcbi4na_colorsp( window, windowx, data, length );
    default: THROW( logic_error, "Invalid query coding " << coding );
    }
}

// this procedure tries to trim left ambiguity characters unless this introduces more severe ambiguities
int CQueryHash::x_GetNcbi4na_ncbi8na( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length )
{
    int off = 0;
    const unsigned char * t = data;
    for( int left = length - m_winLen; left > 0; --left, ++off, ++t ) {
        if( CNcbi8naBase( t[0] ).GetAltCount() <= CNcbi8naBase( t[m_winLen] ).GetAltCount() )
            break; // there is no benefit in shifting right
    }
    window = Ncbi8na2Ncbi4na( t, m_wordLen[0] ); 
    if( GetOffset() ) 
        windowx = Ncbi8na2Ncbi4na( t + GetOffset(), m_wordLen[1] ); 
    return off;
}

int CQueryHash::x_GetNcbi4na_colorsp( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length )
{
	// this encoding does not support ambiguities, 
    window = windowx = 0;
    // to provide uniform hashing algo, conver colors to 4-channel 1-bit per channel sets
    for( unsigned x = 0; x < m_wordLen[0]; ++x ) 
        window |= Uint8( "\x1\x2\x4\x8"[CColorTwoBase( data[x+1] ).GetColorOrd()] ) << (x*4); 
    for( unsigned x = GetOffset(), y = 0, X = GetOffset() + m_wordLen[1]; x < X; ++x, ++y ) 
        windowx |= Uint8( "\x1\x2\x4\x8"[CColorTwoBase( data[x+1] ).GetColorOrd()] ) << (y*4); 
    return 1; // always has offset of 1
}

Uint8 CQueryHash::x_Ncbipna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) { return Ncbipna2Ncbi4na( data, len, score ); }
Uint8 CQueryHash::x_Ncbiqna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) { return Ncbiqna2Ncbi4na( data, len, score ); }

// this procedure tries to perform optimization on number of alternatives 
// if one is out of allowed range
int CQueryHash::x_GetNcbi4na_ncbipna( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, windowx, data, length, x_Ncbipna2Ncbi4na, 5, m_ncbipnaToNcbi4naScore, x_UpdateNcbipnaScore );
}

int CQueryHash::x_GetNcbi4na_ncbiqna( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, windowx, data, length, x_Ncbiqna2Ncbi4na, 1, m_ncbiqnaToNcbi4naScore, x_UpdateNcbiqnaScore );
}

int CQueryHash::x_GetNcbi4na_quality( Uint8& window, Uint8& windowx, const unsigned char * data, unsigned length, TCvt * fun, int incr, unsigned short score, TDecr * decr )
{
    int off = 0;
    const unsigned char * t = data;
    int left = length - m_winLen;
	if( left < 0 ) return -1;

    int wordDelta = m_winLen - m_wordLen[0]; // should be same as GetOffset if words are symmetrical

    window = fun( t, m_wordLen[0], score );
    Uint8 ac = Ncbi4naAlternativeCount( window, m_winLen );
    Uint8 acx = x_ComputeWordRetAmbcount( windowx, t + m_wordLen[0], wordDelta, fun, score );

    while( x_AltcountOk( ac, acx, m_maxAlt ) ) {
        Uint8 aco = ~Uint8(0), acxo = ~Uint8(0);
        Uint8 acm = ~Uint8(0), acxm = ~Uint8(0);
        Uint8 owin = 0, owinx = 0;
        Uint8 mwin = 0, mwinx = 0;
        if( left > 0 ) {
            owin = fun( t + incr, m_wordLen[0], score );
            aco  = Ncbi4naAlternativeCount( owin, m_wordLen[0] );
            acxo = x_ComputeWordRetAmbcount( owinx, t + incr + m_wordLen[0], wordDelta, fun, score );
        }
        if( score ) { 
            mwin = fun( t, m_wordLen[0], decr( score ) );
            acm  = Ncbi4naAlternativeCount( mwin, m_wordLen[0] );
            acxm = x_ComputeWordRetAmbcount( mwinx, t + m_wordLen[0], wordDelta, fun, score );
        }
        if( aco == ~Uint8(0) && acm == ~Uint8(0) ) return off;
        if( x_AltcountOk( aco, acxo, m_maxAlt ) ) {
            if( x_AltcountOk( acm, acxm, m_maxAlt ) ) {
                if( acm*acxm >= aco*acxo ) {
                    window = mwin;
                    windowx = x_MakeXword( mwin, mwinx );
                    return off;
                } else {
                    window = owin;
                    windowx = x_MakeXword( owin, owinx );
                    return off + 1;
                }
            } else {
                window = owin;
                windowx = x_MakeXword( owin, owinx );
                return off + 1;
            }
        } else if( acm < m_maxAlt ) {
            window = mwin;
            windowx = x_MakeXword( mwin, mwinx );
            return off;
        } else if( acm <= aco ) {
            score = decr( score );
            window = mwin;
            windowx = x_MakeXword( mwin, mwinx );
            ac = acm;
            acx = acxm;
        } else {
            ++off;
            if( --left < 0 ) return off;
            t += incr;
            window = owin;
            windowx = x_MakeXword( owin, owinx );
            ac = aco;
            acx = acxo;
        }
    }
    
    return off;
}


    
