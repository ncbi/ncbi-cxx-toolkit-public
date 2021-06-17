#include <ncbi_pch.hpp>
#include "cpermutator8b.hpp"
#include "cbithacks.hpp"
#include "cseqcoding.hpp"
#include "cprogressindicator.hpp"

USING_OLIGOFAR_SCOPES;

CPermutator8b::CPermutator8b( int maxMism, int maxAmb ) : 
    m_maxMism( maxMism ), m_maxAmb( maxAmb )
{
    auto_ptr<CProgressIndicator> pi( new CProgressIndicator() );
    pi->SetMessage( "Initing permutation matrixes" );
    pi->SetUnits( "op" );
    m_data[0].resize(0x10);
    m_data[1].resize(0x100);
    m_data[2].resize(0x1000);
    m_data[3].resize(0x10000);
    for( unsigned ncbi4na = 1; ncbi4na <= 0x000f; ++ncbi4na ) {
        int ambiguities = CNcbi8naBase( ncbi4na ).IsAmbiguous();
        TEntry& e = SetEntry( ncbi4na, 1 );
        e.SetAmbiguities( ambiguities );
        if( ambiguities > m_maxAmb ) continue; // should be AFTER e.SetAmbiguities()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x03; ++ncbi2na ) {
            int mism = 1 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 1; ncbi4na <= 0x00ff; ++ncbi4na ) {
        if( ( ncbi4na & 0x000f ) == 0 || ( ncbi4na & 0x00f0 ) == 0 ) continue;
        int ambiguities = 
            int( CNcbi8naBase( ( ncbi4na >> 0 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >> 4 ) & 0x000f ).IsAmbiguous() );
        TEntry& e = SetEntry( ncbi4na, 2 );
        e.SetAmbiguities( ambiguities );
        if( ambiguities > m_maxAmb ) continue; // should be AFTER e.SetAmbiguities()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x0f; ++ncbi2na ) {
            int mism = 2 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 1; ncbi4na <= 0x0fff; ++ncbi4na ) {
        if( ( ncbi4na & 0x000f ) == 0 || ( ncbi4na & 0x00f0 ) == 0 || ( ncbi4na & 0x0f00 ) == 0 ) continue;
        int ambiguities = 
            int( CNcbi8naBase( ( ncbi4na >> 0 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >> 4 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >> 8 ) & 0x000f ).IsAmbiguous() );
        TEntry& e = SetEntry( ncbi4na, 3 );
        e.SetAmbiguities( ambiguities );
        if( ambiguities > m_maxAmb ) continue; // should be AFTER e.SetAmbiguities()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x3f; ++ncbi2na ) {
            int mism = 3 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 1; ncbi4na <= 0xffff; ++ncbi4na ) {
        if( ( ncbi4na & 0x000f ) == 0 || ( ncbi4na & 0x00f0 ) == 0 || ( ncbi4na & 0x0f00 ) == 0 || ( ncbi4na & 0xf000 ) == 0 ) continue;
        int ambiguities = 
            int( CNcbi8naBase( ( ncbi4na >>  0 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >>  4 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >>  8 ) & 0x000f ).IsAmbiguous() ) +
            int( CNcbi8naBase( ( ncbi4na >> 12 ) & 0x000f ).IsAmbiguous() );
        TEntry& e = SetEntry( ncbi4na, 4 );
        e.SetAmbiguities( ambiguities );
        if( ambiguities > m_maxAmb ) continue; // should be AFTER e.SetAmbiguities()
        for( unsigned ncbi2na = 0; ncbi2na <= 0xff; ++ncbi2na ) {
            int mism = 4 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( int i = 0; i < 4; ++i ) {
        for( TTable::iterator e = m_data[i].begin(); e != m_data[i].end(); ++e ) {
            e->Sort();
        }
    }
    pi->Summary();
}

bool CPermutator8b::x_InitStaticTables()
{
    for( int i = 0, k = 0; i < 16; ++i ) {
        unsigned h1 = (s_ncbi2na_ncbi4na_2bases[i] << 8);
        for( int j = 0; j < 16; ++j, ++k ) {
            s_ncbi2na_ncbi4na_4bases[k] = h1 | s_ncbi2na_ncbi4na_2bases[j];
        }
    }
    return true;
}

unsigned CPermutator8b::s_ncbi2na_ncbi4na_2bases[16] = { 0x11,0x12,0x14,0x18,0x21,0x22,0x24,0x28,0x41,0x42,0x44,0x48,0x81,0x82,0x84,0x88 };
unsigned CPermutator8b::s_ncbi2na_ncbi4na_4bases[256];
bool CPermutator8b::s_static_tables_inited = CPermutator8b::x_InitStaticTables();

