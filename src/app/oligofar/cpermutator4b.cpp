#include <ncbi_pch.hpp>
#include "cpermutator4b.hpp"
#include "cbithacks.hpp"
#include "cprogressindicator.hpp"

USING_OLIGOFAR_SCOPES;

// Note: code here can be changed so that number of meaningful bases may be arbitrary
// Then combining few permutators we may make arbitrary word size (4-14)
// OR... just pad with A's (to make higher bits 0 in ncbi2na)?
CPermutator4b::CPermutator4b( int maxMism, int maxAlt ) : m_maxMism( maxMism ), m_maxAlt( maxAlt )
{
    auto_ptr<CProgressIndicator> pi( new CProgressIndicator() );
    pi->SetMessage( "Initing permutation matrixes" );
    pi->SetUnits( "op" );
    m_data[0].resize(0x10);
    m_data[1].resize(0x100);
    m_data[2].resize(0x1000);
    m_data[3].resize(0x10000);
    for( unsigned ncbi4na = 0; ncbi4na <= 0x000f; ++ncbi4na ) {
        unsigned alternatives = 
            CBitHacks::BitCount2( ncbi4na & 0x000f ) ;
        if( alternatives == 0 ) continue;
        TEntry& e = SetEntry( ncbi4na, 1 );
		if( e.GetAlternatives() == 0 ) e.SetAlternatives( alternatives );
        if( alternatives > m_maxAlt ) continue; // should be AFTER e.SetAlternatives()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x03; ++ncbi2na ) {
            unsigned mism = 1 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 0; ncbi4na <= 0x00ff; ++ncbi4na ) {
        unsigned alternatives = 
            CBitHacks::BitCount2( ncbi4na & 0x00f0 ) *
            CBitHacks::BitCount2( ncbi4na & 0x000f ) ;
        if( alternatives == 0 ) continue;
        TEntry& e = SetEntry( ncbi4na, 2 );
		if( e.GetAlternatives() == 0 ) e.SetAlternatives( alternatives );
        if( alternatives > m_maxAlt ) continue; // should be AFTER e.SetAlternatives()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x0f; ++ncbi2na ) {
            unsigned mism = 2 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 0; ncbi4na <= 0x0fff; ++ncbi4na ) {
        unsigned alternatives = 
            CBitHacks::BitCount2( ncbi4na & 0x0f00 ) *
            CBitHacks::BitCount2( ncbi4na & 0x00f0 ) *
            CBitHacks::BitCount2( ncbi4na & 0x000f ) ;
        if( alternatives == 0 ) continue;
        TEntry& e = SetEntry( ncbi4na, 3 );
		if( e.GetAlternatives() == 0 ) e.SetAlternatives( alternatives );
        if( alternatives > m_maxAlt ) continue; // should be AFTER e.SetAlternatives()
        for( unsigned ncbi2na = 0; ncbi2na <= 0x3f; ++ncbi2na ) {
            unsigned mism = 3 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
            if( mism <= m_maxMism ) {
                e.push_back( C_Atom( ncbi2na, mism ) );
                pi->Increment();
            }
        }
    }
    for( unsigned ncbi4na = 0; ncbi4na <= 0xffff; ++ncbi4na ) {
        unsigned alternatives = 
            CBitHacks::BitCount2( ncbi4na & 0xf000 ) *
            CBitHacks::BitCount2( ncbi4na & 0x0f00 ) *
            CBitHacks::BitCount2( ncbi4na & 0x00f0 ) *
            CBitHacks::BitCount2( ncbi4na & 0x000f ) ;
        if( alternatives == 0 ) continue;
        TEntry& e = SetEntry( ncbi4na, 4 );
		if( e.GetAlternatives() == 0 ) e.SetAlternatives( alternatives );
        if( alternatives > m_maxAlt ) continue; // should be AFTER e.SetAlternatives()
        for( unsigned ncbi2na = 0; ncbi2na <= 0xff; ++ncbi2na ) {
            unsigned mism = 4 - CBitHacks::BitCount4( s_ncbi2na_ncbi4na_4bases[ncbi2na] & ncbi4na );
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

bool CPermutator4b::x_InitStaticTables()
{
	for( int i = 0, k = 0; i < 16; ++i ) {
		unsigned h1 = (s_ncbi2na_ncbi4na_2bases[i] << 8);
		for( int j = 0; j < 16; ++j, ++k ) {
			s_ncbi2na_ncbi4na_4bases[k] = h1 | s_ncbi2na_ncbi4na_2bases[j];
		}
	}
    return true;
}

unsigned CPermutator4b::s_ncbi2na_ncbi4na_2bases[16] = { 0x11,0x12,0x14,0x18,0x21,0x22,0x24,0x28,0x41,0x42,0x44,0x48,0x81,0x82,0x84,0x88 };
unsigned CPermutator4b::s_ncbi2na_ncbi4na_4bases[256];
bool CPermutator4b::s_static_tables_inited = CPermutator4b::x_InitStaticTables();

