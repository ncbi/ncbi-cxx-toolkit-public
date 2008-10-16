#include <ncbi_pch.hpp>
#include "cqueryhash.hpp"
#include "chashpopulator.hpp"
#include "dust.hpp"

USING_OLIGOFAR_SCOPES;

double CQueryHash::ComputeComplexityNcbi8na( UintH w, int l, int& amb ) const
{
    CComplexityMeasure m;
    int tripletid = 0;
    amb = 0;
    for( int i = 0; i < l; ++i, ( w >>= 4 ), (tripletid <<= 2) ) {
        CNcbi8naBase b( char(( w.GetLo() ) & 0xf) );
        amb += int( b.IsAmbiguous() );
        tripletid |= b.GetSmallestNcbi2na();
        if( i > 2 ) m.Add( tripletid & 0x3f );
    }
    return double( m );
}

void CQueryHash::Compress( UintH& w ) const
{
    int x = 0;
    ITERATE( TSkipPositions, p, m_skipPositions ) {
        if( *p > x ) CBitHacks::DeleteBits<UintH,4>( w, *p - ++x );
        if( *p > m_windowLength ) break;
    }
}

int CQueryHash::AddQuery( CQuery * query, int component )
{
    ASSERT( component == 0 || component == 1 );

    int len = query->GetLength( component );
    if( len == 0 ) return 0;
    
    if( len < m_windowLength ) {
        query->SetRejectAsShort( component );
        return 0; // too short read
    }

    UintH fwindow = 0;
    int ambiguities = 0;
    int offset = GetNcbi4na( fwindow, query->GetCoding(), (unsigned char*)query->GetData( component ), len );

    if( offset < 0 ) {
        query->SetRejectAsLoQual( component );
        return 0;
    }
    query->ClearRejectFlags( component );
    if( ComputeComplexityNcbi8na( fwindow, m_windowLength, ambiguities ) > m_maxSimplicity ) 
        query->SetRejectAsLoCompl( component );
    if( ambiguities > m_maxAmb ) { query->SetRejectAsLoQual( component ); }

    if( query->GetRejectFlags( component ) ) return 0;
    
    Compress( fwindow );

    size_t reserve = size_t( min( Uint8(numeric_limits<size_t>::max()), 
                                  ComputeEntryCountPerRead() * ( Uint8( 1 ) << ( ambiguities * 2 ) ) ) );

    CHashPopulator hashPopulator( m_windowSize, m_wordSize, m_strideSize, query, m_strands, offset, component );
    hashPopulator.Reserve( reserve );
    
    if( GetAllowIndel() ) {
        UintH maskH = CBitHacks::WordFootprint<UintH>( 4 * ( m_windowSize + m_strideSize - 1 ) );
        int maxMism = max( 0, m_maxMism - 1 ); // don't allow as many mismatches if indels are allowed
        hashPopulator.SetPermutator( m_permutators[maxMism] );
        hashPopulator.SetIndel( CHashAtom::eInsertion );
        for( int pos = 1; pos < m_windowSize - 1; ++pos ) {
            UintH ifwindow = maskH & CBitHacks::InsertBits<UintH, 4, 0xf>( fwindow, pos ); // insert N at the position 
            hashPopulator.PopulateHash( ifwindow );
        }
        hashPopulator.SetIndel( CHashAtom::eDeletion );
        for( int pos = 1; pos < m_windowSize; ++pos ) {
            UintH dfwindow = maskH & CBitHacks::DeleteBits<UintH, 4>( fwindow, pos ); // deletion at the position
            hashPopulator.PopulateHash( dfwindow );
        }
        fwindow &= maskH; // remove extra base
    }

    // no indels, just mismatches
    hashPopulator.SetIndel( CHashAtom::eNoIndel );
    hashPopulator.SetPermutator( m_permutators[m_maxMism] );
    hashPopulator.PopulateHash( fwindow );
    hashPopulator.Unique();
    
    ITERATE( CHashPopulator, h, hashPopulator ) {
        m_hashTable.AddEntry( h->first, h->second );
    }

    return hashPopulator.size();
}

// this procedure tries to trim left ambiguity characters unless this introduces more severe ambiguities
int CQueryHash::x_GetNcbi4na_ncbi8na( UintH& window, const unsigned char * data, unsigned length )
{
    int off = 0;
    const unsigned char * t = data;
    unsigned wlen = m_windowLength;

    ASSERT( wlen <= 32 );

    for( int left = length - wlen; CanOptimizeOffset() && left > 0; --left, ++off, ++t ) {
        if( CNcbi8naBase( t[0] ).GetAltCount() <= CNcbi8naBase( t[wlen] ).GetAltCount() )
            break; // there is no benefit in shifting right
    }
    window = 0;

    for( unsigned i = 0; i < wlen; ++i ) {
        CNcbi8naBase b( t[i] );
        UintH mask( b );
        UintH smask( mask << int(i*4) );
        window |= UintH( t[i] & 0x0f ) << int(i*4);
    }
    return off;
}

int CQueryHash::x_GetNcbi4na_colorsp( UintH& window, const unsigned char * data, unsigned length )
{
	// this encoding does not support ambiguities, 
    int off = 0;
    const unsigned char * t = data;
    unsigned wlen = m_windowLength;

    ASSERT( wlen <= 32 );

    for( int left = length - wlen; CanOptimizeOffset() && left > 0; --left, ++off, ++t ) {
        if( CNcbi8naBase( t[0] ).GetAltCount() <= CNcbi8naBase( t[wlen] ).GetAltCount() )
            break; // there is no benefit in shifting right
    }
    window = 0;
    // to provide uniform hashing algo, conver colors to 4-channel 1-bit per channel sets
    for( unsigned x = 0; x < wlen; ++x ) 
        window |= UintH( "\x1\x2\x4\x8"[CColorTwoBase( data[x+1] ).GetColorOrd()] ) << int(x*4); 

    return 1; // always has offset of 1
}

// this procedure tries to perform optimization on number of alternatives 
// if one is out of allowed range
int CQueryHash::x_GetNcbi4na_ncbipna( UintH& window, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, data, length, x_Ncbipna2Ncbi4na, 5, m_ncbipnaToNcbi4naScore, x_UpdateNcbipnaScore );
}

int CQueryHash::x_GetNcbi4na_ncbiqna( UintH& window, const unsigned char * data, unsigned length )
{
	return x_GetNcbi4na_quality( window, data, length, x_Ncbiqna2Ncbi4na, 1, m_ncbiqnaToNcbi4naScore, x_UpdateNcbiqnaScore );
}

UintH CQueryHash::ComputeAlternatives( UintH w, int l ) const
{
    UintH alternatives = 1;
    for( int i = 0; i < l; ++i, ( w >>= 4 ) ) {
        CNcbi8naBase b( char(( w.GetLo() ) & 0xf) );
        alternatives *= Uint1( b.GetAltCount() );
    }
    return alternatives;
}

int CQueryHash::x_GetNcbi4na_quality( UintH& window, const unsigned char * data, unsigned length, TCvt * fun, int incr, unsigned short score, TDecr * decr )
{
    int off = 0;
    const unsigned char * t = data;
    int left = length - m_windowLength;
	if( left < 0 ) return -1;

    unsigned wlen = m_windowLength;
    ASSERT( wlen <= 32 );

    window = fun( t, wlen, score );
    UintH ac = ComputeAlternatives( window, wlen );
    UintH maxAlt = Uint8( 1 ) << ( m_maxAmb * 4 );

    while( ac > maxAlt ) {
        UintH aco = ~UintH(0);
        UintH acm = ~UintH(0);
        UintH owin = 0;
        UintH mwin = 0;
        if( CanOptimizeOffset() && left > 0 ) {
            owin = fun( t + incr, wlen, score );
            aco  = ComputeAlternatives( owin, wlen );
        }
        if( score ) { 
            mwin = fun( t, wlen, decr( score ) );
            acm  = ComputeAlternatives( mwin, wlen );
        }
        if( aco == ~UintH(0) && acm == ~UintH(0) ) return off;
        if( aco < maxAlt ) {
            if( acm < maxAlt ) {
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
        } else if( acm < maxAlt ) {
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

int CQueryHash::GetNcbi4na( UintH& window, CSeqCoding::ECoding coding,
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

UintH CQueryHash::x_Ncbipna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) 
{ 
    UintH ret = 0;
    
    for( int x = 0; x < len; ++x )
        ret |= UintH( CNcbi8naBase( CNcbipnaBase( data + 5*x ), score ) ) << (x*4);
    
    return ret;
}

UintH CQueryHash::x_Ncbiqna2Ncbi4na( const unsigned char * data, int len, unsigned short score ) 
{ 
    UintH ret = 0;
    
    for( int x = 0; x < len; ++x )
        ret |= UintH( CNcbi8naBase( CNcbiqnaBase( data[x] ), score ) ) << (x*4);
    
    return ret;
}

