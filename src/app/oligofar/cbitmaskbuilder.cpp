#include <ncbi_pch.hpp>
#include "cbitmaskbuilder.hpp"
#include "cprogressindicator.hpp"
#include "fourplanes.hpp"

USING_OLIGOFAR_SCOPES;

CBitmaskBuilder::CBitmaskBuilder( int maxamb, int wsize, int wstep ) : 
    CBitmaskBase( maxamb, wsize, wstep ), m_bitCount( 0 ) 
{
    m_size = ((UintH(1) << (2*wsize))/Uint8(32)).GetLo();
    m_data = new Uint4[m_size];
    fill( m_data, m_data + m_size, 0 );
}

bool CBitmaskBuilder::Write( const string& name ) const 
{ 
    ofstream o( ( name + "~" ).c_str() ); 
    if( o.fail() ) THROW( runtime_error, "Failed to open file "+name+"~ for write: "+strerror( errno ) );
    if( Write( o ) ) {
        o.flush();
        o.close();
        if( o.fail() ) THROW( runtime_error, "Failed to complete writing file "+name+"~: "+strerror( errno ) );
        if( rename( (name + "~").c_str(), name.c_str() ) ) 
            THROW( runtime_error, "Failed to rename " + name + "~ to " + name + ": "+strerror( errno ) );
    } else {
        unlink( (name + "~").c_str() );
        THROW( runtime_error, "Failed to write file "+name+"~: "+strerror( errno ) );
    }
    return true;
}

Uint8 CBitmaskBuilder::CountBits() 
{
    m_bitCount = 0;
    for( Uint8 i = 0; i < m_size; ++i ) {
        m_bitCount += CBitHacks::BitCount4( m_data[i] );
    }
    return m_bitCount;
}

void CBitmaskBuilder::SequenceBuffer( CSeqBuffer * ncbi8na )
{
    CProgressIndicator progress( "Listing "+NStr::IntToString( m_wSize )+"-mers for "+m_seqId );
    fourplanes::CHashGenerator hgen( m_wSize );
    for( const char * seq = ncbi8na->GetBeginPtr(); seq < ncbi8na->GetEndPtr(); ++seq ) {
        hgen.AddBaseMask( CNcbi8naBase( seq ) );
        if( hgen.GetAmbiguityCount() <= (int)m_maxAmb ) {
            for( fourplanes::CHashIterator h( hgen ); h; ++h ) SetBit( *h );                
        }
        progress.Increment();
    }
    progress.Summary();
}

