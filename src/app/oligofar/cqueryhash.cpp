#include <ncbi_pch.hpp>
#include "cqueryhash.hpp"
#include "chashpopulator.hpp"
#include "uinth.hpp"
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

int CQueryHash::ComputeAmbiguitiesNcbi8na( UintH w, int l ) const
{
    int amb = 0;
    for( int i = 0; i < l; ++i, ( w >>= 4 ) ) {
        CNcbi8naBase b( char(( w.GetLo() ) & 0xf) );
        amb += int( b.IsAmbiguous() );
    }
    return amb;
}

double CQueryHash::ComputeComplexityNcbi2na( Uint8 w, int l ) const
{
    CComplexityMeasure m;
    for( int i = 3; i < l; ++i, ( w >>= 2 ) ) m.Add( w & 0x3f );
    return double( m );
}

void CQueryHash::PopulateInsertion2( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos )
{
    UintH ifbase =   CBitHacks::InsertBits<UintH, 4, 0x0>( fwindow, pos );
    ifbase = maskH & CBitHacks::InsertBits<UintH, 4, 0x0>( ifbase,  pos );
    UintH k1 = UintH( 1 ) << (pos*4);
    UintH k2 = k1 << 4;
    for( int i = 0; i < 4; ++i ) {
        UintH b1 = k1 << i;
        for( int j = 0; j < 4; ++j ) {
            UintH b2 = k2 << j;
            UintH ifwindow = b1|b2|ifbase;
            dest.PopulateHash( ifwindow );
        }
    }
}

void CQueryHash::PopulateInsertion1( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos )
{
    UintH ifbase = maskH & CBitHacks::InsertBits<UintH, 4, 0x0>( fwindow, pos );
    UintH k1 = UintH( 1 ) << (pos*4);
    for( int i = 0; i < 4; ++i ) {
        UintH b1 = k1 << i;
        UintH ifwindow = b1|ifbase;
        dest.PopulateHash( ifwindow );
    }
}

void CQueryHash::PopulateDeletion2( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos )
{
    UintH dfwindow =   CBitHacks::DeleteBits<UintH, 4>(  fwindow, pos );
    dfwindow = maskH & CBitHacks::DeleteBits<UintH, 4>( dfwindow, pos );
    dest.PopulateHash( dfwindow );
}

void CQueryHash::PopulateDeletion1( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow, int pos )
{
    UintH dfwindow = maskH & CBitHacks::DeleteBits<UintH, 4>( fwindow, pos );
    dest.PopulateHash( dfwindow );
}

static const int g_IndelMargin = 1;

void CQueryHash::PopulateInsertions( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow )
{
    int pos0 = 1 - g_IndelMargin;
    int pos1 = m_windowSize + m_strideSize - GetMaxInsertions() + g_IndelMargin;
    switch( int ins = GetMaxInsertions() ) {
    case 2: 
        dest.SetPermutator( m_permutators[min( GetMaxMismatches(), GetMaxDistance() - ins )] );
        dest.SetIndels( ins, CHashAtom::eInsertion );
        if( m_indelPos == eIndel_anywhere )
            for( int pos = pos0; pos < pos1; ++pos ) PopulateInsertion2( dest, maskH, fwindow, pos );
        else PopulateInsertion2( dest, maskH, fwindow, m_indelPos );
        --ins;
    case 1:
        ++pos1;
        dest.SetPermutator( m_permutators[min( GetMaxMismatches(), GetMaxDistance() - ins )] );
        dest.SetIndels( ins, CHashAtom::eInsertion );
        if( m_indelPos == eIndel_anywhere )
            for( int pos = pos0; pos < pos1; ++pos ) PopulateInsertion1( dest, maskH, fwindow, pos );
        else PopulateInsertion1( dest, maskH, fwindow, m_indelPos );
        --ins;
    case 0: break;
    default: THROW( logic_error, "Maximal number of insertions may be only 0, 1, or 2, got " << ins );
    }
}

void CQueryHash::PopulateDeletions( CHashPopulator& dest, const UintH& maskH, const UintH& fwindow )
{
    int pos0 = 1 - g_IndelMargin;
    int pos1 = m_windowSize + m_strideSize + g_IndelMargin;
    switch( int del = GetMaxDeletions() ) {
    case 2: 
        dest.SetPermutator( m_permutators[min( GetMaxMismatches(), GetMaxDistance() - del )] );
        dest.SetIndels( del, CHashAtom::eDeletion );
        if( m_indelPos == eIndel_anywhere )
            for( int pos = pos0; pos < pos1; ++pos ) PopulateDeletion2( dest, maskH, fwindow, pos );
        else PopulateDeletion2( dest, maskH, fwindow, m_indelPos );
        --del;
    case 1:
        dest.SetPermutator( m_permutators[min( GetMaxMismatches(), GetMaxDistance() - del )] );
        dest.SetIndels( del, CHashAtom::eDeletion );
        if( m_indelPos == eIndel_anywhere )
            for( int pos = pos0; pos < pos1; ++pos ) PopulateDeletion1( dest, maskH, fwindow, pos );
        else PopulateDeletion1( dest, maskH, fwindow, m_indelPos );
        --del;
    case 0: break;
    default: THROW( logic_error, "Maximal number of deletions may be only 0, 1, or 2, got " << del );
    }
}

int CQueryHash::PopulateWindow( CHashPopulator& hashPopulator, UintH fwindow, int offset, int ambiguities )
{
    ASSERT( offset <= CHashAtom::GetMaxOffset() );
    CompressFwd<UintH,4>( fwindow );

    size_t reserve = size_t( 
        min( Uint8(numeric_limits<size_t>::max() ), 
             m_hashPopulatorReverve * ( Uint8( 1 ) << ( ambiguities * 2 ) ) ) );

    hashPopulator.Reserve( hashPopulator.size() + reserve );
    
    Uint2 w = m_windowSize + m_strideSize - 1;
    UintH maskH = CBitHacks::WordFootprint<UintH>( 4 * w );

    if( m_indelPos < w ) {
        PopulateInsertions( hashPopulator, maskH, fwindow );
        PopulateDeletions(  hashPopulator, maskH, fwindow );
    }

    // no indels, just mismatches
    hashPopulator.SetIndels( 0, CHashAtom::eNone );
    hashPopulator.SetPermutator( m_permutators[GetMaxMismatches()] );
    hashPopulator.PopulateHash( fwindow );
//     hashPopulator.Unique();
    
//     ITERATE( CHashPopulator, h, hashPopulator ) m_hashTable.AddEntry( h->first, h->second );

    return hashPopulator.size();
}

int CQueryHash::x_AddQuery( CQuery * query, int component )
{
    ASSERT( component == 0 || component == 1 );

    int off = max(0, m_windowStart);
    int maxWinCnt = m_skipPositions.size() ? 1 : m_maxWindowCount;
    int wstep = GetHasherWindowStep(); 

    return x_AddQuery( query, component, off, maxWinCnt, wstep );
}

int CQueryHash::x_AddQuery( CQuery * query, int component, int off, int maxWinCnt, int wstep )
{
    int len = query->GetLength( component );
    if( len == 0 ) return 0;
    
    if( len < m_windowLength ) {
        query->SetRejectAsShort( component );
        return 0; // too short read
    }

    int ret = 0;
    for( int i = 0; i < maxWinCnt; ++i, (off += wstep) ) {
        if( len - off < m_windowLength ) continue;

        UintH fwindow = 0;
        int offset = GetNcbi4na( fwindow, query->GetCoding(), (unsigned char*)query->GetData( component, off ), len - off );

        if( offset < 0 ) {
            query->SetRejectAsLoQual( component );
            continue; 
        }
        if( offset > CHashAtom::GetMaxOffset() ) break;

        query->ClearRejectFlags( component );
        int ambiguities = 0; //ComputeAmbiguitiesNcbi8na( fwindow, m_windowLength );
        if( ComputeComplexityNcbi8na( fwindow, m_windowLength, ambiguities ) > m_maxSimplicity ) {
            query->SetRejectAsLoCompl( component );
            offset = -1;
        }
        if( ambiguities > m_maxAmb ) { query->SetRejectAsLoQual( component ); continue; }
        if( offset < 0 ) continue;

        offset += off;
        if( offset >= CHashAtom::GetMaxOffset() ) break;
        off = offset; // restart from this place - we don't know how far we should travel
        
        CHashPopulator populator( m_windowSize, m_wordSize, m_strideSize, query, 
                                  m_strands, offset, component, CHashAtom::eConvEq, 
                                  m_wbmAccess );

        if( m_nahso3mode ) {
            if( query->GetCoding() != CSeqCoding::eCoding_colorsp ) {
                UintH tcwin = x_Convert4tc( fwindow, m_windowLength );
                UintH agwin = x_Convert4ag( fwindow, m_windowLength );
                if( tcwin != agwin ) {
                    populator.SetConv( CHashAtom::eConvTC );
                    PopulateWindow( populator, tcwin, offset, ambiguities );
                    populator.SetConv( CHashAtom::eConvAG );
                    PopulateWindow( populator, agwin, offset, ambiguities );
                } else {
                    populator.SetConv( CHashAtom::eConvEq );
                    PopulateWindow( populator, fwindow, offset, ambiguities );
                }
            } else {
                populator.SetConv( CHashAtom::eConvEq );
                PopulateWindow( populator, fwindow, offset, ambiguities );
            }
        } else {
            populator.SetConv( CHashAtom::eNoConv );
            PopulateWindow( populator, fwindow, offset, ambiguities );
        }

        populator.Unique();
//         int loComplCount = 0;
//         ITERATE( CHashPopulator, item, populator ) {
//             double simpl = ComputeComplexityNcbi2na( item->first, m_windowLength );
//             if( simpl > m_maxSimplicity ) loComplCount++;
//         }
//         if( loComplCount == 0 ) {
        ITERATE( CHashPopulator, item, populator ) 
            m_hashTable.AddEntry( item->first, item->second );
        ret += populator.size();
//         } else {
//             query->SetRejectAsLoCompl( component );
//         }
    }

    if( ret ) query->ClearRejectFlags( component );
    return ret;
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

void CQueryHash::Freeze() 
{ 
    CheckConstraints(); 
    ComputeScannerWindowLength(); 
    m_hashTable.Sort(); 
    m_status = eStatus_ready; 
}

void CQueryHash::CheckConstraints()
{
    if( m_wbmAccess ) {
        if( m_wbmAccess->GetWordStep() > (Uint4)GetStrideSize() )
            THROW( runtime_error, "Oops... hash word bitmap should have same stride size as query hash" );
        if( GetWindowStep() % m_wbmAccess->GetWordStep() != 0 )
            THROW( runtime_error, "Oops... hash word bitmap should have same stride size as query window step" );
        if( m_windowStart && ( m_windowStart % m_wbmAccess->GetWordStep() != 0 )  )
            THROW( runtime_error, "Oops... hash word bitmap should have same stride size as query window step" );
    }
}

const unsigned char * CQueryHash::s_convert2eq = (const unsigned char *)
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
    "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
    "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
    "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
    "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
    "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
    "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
    "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
    "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
    "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
    "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
    "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
    "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
    "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
    "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

const unsigned char * CQueryHash::s_convert2tc = (const unsigned char *)
    "\x00\x01\x02\x01\x04\x05\x06\x05\x08\x09\x0a\x09\x04\x05\x06\x05"
    "\x10\x11\x12\x11\x14\x15\x16\x15\x18\x19\x1a\x19\x14\x15\x16\x15"
    "\x20\x21\x22\x21\x24\x25\x26\x25\x28\x29\x2a\x29\x24\x25\x26\x25"
    "\x10\x11\x12\x11\x14\x15\x16\x15\x18\x19\x1a\x19\x14\x15\x16\x15"
    "\x40\x41\x42\x41\x44\x45\x46\x45\x48\x49\x4a\x49\x44\x45\x46\x45"
    "\x50\x51\x52\x51\x54\x55\x56\x55\x58\x59\x5a\x59\x54\x55\x56\x55"
    "\x60\x61\x62\x61\x64\x65\x66\x65\x68\x69\x6a\x69\x64\x65\x66\x65"
    "\x50\x51\x52\x51\x54\x55\x56\x55\x58\x59\x5a\x59\x54\x55\x56\x55"
    "\x80\x81\x82\x81\x84\x85\x86\x85\x88\x89\x8a\x89\x84\x85\x86\x85"
    "\x90\x91\x92\x91\x94\x95\x96\x95\x98\x99\x9a\x99\x94\x95\x96\x95"
    "\xa0\xa1\xa2\xa1\xa4\xa5\xa6\xa5\xa8\xa9\xaa\xa9\xa4\xa5\xa6\xa5"
    "\x90\x91\x92\x91\x94\x95\x96\x95\x98\x99\x9a\x99\x94\x95\x96\x95"
    "\x40\x41\x42\x41\x44\x45\x46\x45\x48\x49\x4a\x49\x44\x45\x46\x45"
    "\x50\x51\x52\x51\x54\x55\x56\x55\x58\x59\x5a\x59\x54\x55\x56\x55"
    "\x60\x61\x62\x61\x64\x65\x66\x65\x68\x69\x6a\x69\x64\x65\x66\x65"
    "\x50\x51\x52\x51\x54\x55\x56\x55\x58\x59\x5a\x59\x54\x55\x56\x55";

const unsigned char * CQueryHash::s_convert2ag = (const unsigned char *)
    "\xaa\xa9\xaa\xab\xa6\xa5\xa6\xa7\xaa\xa9\xaa\xab\xae\xad\xae\xaf"
    "\x9a\x99\x9a\x9b\x96\x95\x96\x97\x9a\x99\x9a\x9b\x9e\x9d\x9e\x9f"
    "\xaa\xa9\xaa\xab\xa6\xa5\xa6\xa7\xaa\xa9\xaa\xab\xae\xad\xae\xaf"
    "\xba\xb9\xba\xbb\xb6\xb5\xb6\xb7\xba\xb9\xba\xbb\xbe\xbd\xbe\xbf"
    "\x6a\x69\x6a\x6b\x66\x65\x66\x67\x6a\x69\x6a\x6b\x6e\x6d\x6e\x6f"
    "\x5a\x59\x5a\x5b\x56\x55\x56\x57\x5a\x59\x5a\x5b\x5e\x5d\x5e\x5f"
    "\x6a\x69\x6a\x6b\x66\x65\x66\x67\x6a\x69\x6a\x6b\x6e\x6d\x6e\x6f"
    "\x7a\x79\x7a\x7b\x76\x75\x76\x77\x7a\x79\x7a\x7b\x7e\x7d\x7e\x7f"
    "\xaa\xa9\xaa\xab\xa6\xa5\xa6\xa7\xaa\xa9\xaa\xab\xae\xad\xae\xaf"
    "\x9a\x99\x9a\x9b\x96\x95\x96\x97\x9a\x99\x9a\x9b\x9e\x9d\x9e\x9f"
    "\xaa\xa9\xaa\xab\xa6\xa5\xa6\xa7\xaa\xa9\xaa\xab\xae\xad\xae\xaf"
    "\xba\xb9\xba\xbb\xb6\xb5\xb6\xb7\xba\xb9\xba\xbb\xbe\xbd\xbe\xbf"
    "\xea\xe9\xea\xeb\xe6\xe5\xe6\xe7\xea\xe9\xea\xeb\xee\xed\xee\xef"
    "\xda\xd9\xda\xdb\xd6\xd5\xd6\xd7\xda\xd9\xda\xdb\xde\xdd\xde\xdf"
    "\xea\xe9\xea\xeb\xe6\xe5\xe6\xe7\xea\xe9\xea\xeb\xee\xed\xee\xef"
    "\xfa\xf9\xfa\xfb\xf6\xf5\xf6\xf7\xfa\xf9\xfa\xfb\xfe\xfd\xfe\xff";
