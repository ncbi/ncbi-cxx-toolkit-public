#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "fourplanes.hpp"

USING_OLIGOFAR_SCOPES;
USING_SCOPE(fourplanes);


int COligoFarApp::TestSuite()
{
    Uint8 ncbi4na = 0;

    TESTVAL(int, CBitHacks::BitCount1( Uint1( '\x67' ) ), 5 );
    TESTVAL(int, CBitHacks::BitCount2( 0x4567u ), 8 );
    TESTVAL(int, CBitHacks::BitCount4( 0x01234567u ),12);
    TESTVAL(int, CBitHacks::BitCount8( 0x0123456789abcdefULL ), 32 );

    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits1( Uint1('\x01') ) ), CBitHacks::AsBits( Uint1('\x80') ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits2( Uint2( 0x0123U ) ) ), CBitHacks::AsBits( Uint2( 0xc480U ) ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits4( 0x01234567UL ) ), CBitHacks::AsBits( 0xe6a2c480UL ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits8( 0x0123456789abcdefULL ) ), CBitHacks::AsBits( 0xf7b3d591e6a2c480ULL ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBitPairs8( 0x0123456789abcdefULL ) ), CBitHacks::AsBits( 0xfb73ea62d951c840ULL ) );
    
    ncbi4na = Iupacna2Ncbi4na( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

    TESTVAL( string, CBitHacks::AsBits(ncbi4na), CBitHacks::AsBits(0x0123456789abcdefULL) );

    ncbi4na = Iupacna2Ncbi4naRevCompl( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

    TESTVAL( string, CBitHacks::AsBits(ncbi4na), CBitHacks::AsBits(0x0f7b3d591e6a2c48ULL) );

    CHashGenerator hashGen( 5 );
    hashGen.AddIupacNa( 'A' );
    hashGen.AddIupacNa( 'A' );
    hashGen.AddIupacNa( 'A' );
    hashGen.AddIupacNa( 'A' );
    hashGen.AddIupacNa( 'A' );

    for( CHashIterator iter( hashGen ); iter; ++iter ) {
        if( *iter != 0 ) {
            cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
            cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
                 << "\t1\t" << hashGen.GetPlane(1) << "\n"
                 << "\t2\t" << hashGen.GetPlane(2) << "\n"
                 << "\t3\t" << hashGen.GetPlane(3) << "\n";
            cerr << "\t=\t" << hashGen.GetHashValue() << endl;
            throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
        }
    }

    hashGen.AddIupacNa( 'C' );

    for( CHashIterator iter( hashGen ); iter; ++iter ) {
        if( *iter != 0x0100 ) {
            cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
            cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
                 << "\t1\t" << hashGen.GetPlane(1) << "\n"
                 << "\t2\t" << hashGen.GetPlane(2) << "\n"
                 << "\t3\t" << hashGen.GetPlane(3) << "\n";
            cerr << "\t=\t" << hashGen.GetHashValue() << endl;
            throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
        }
    }

    hashGen.AddIupacNa( 'G' );

    for( CHashIterator iter( hashGen ); iter; ++iter ) {
        if( *iter != 0x0240 ) {
            cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t"<< *iter << endl;
            cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
                 << "\t1\t" << hashGen.GetPlane(1) << "\n"
                 << "\t2\t" << hashGen.GetPlane(2) << "\n"
                 << "\t3\t" << hashGen.GetPlane(3) << "\n";
            cerr << "\t=\t" << hashGen.GetHashValue() << endl;
            throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
        }
    }

    hashGen.AddIupacNa( 'T' );

    for( CHashIterator iter( hashGen ); iter; ++iter ) {
        if( *iter != 0x0390 ) {
            cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
            cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
                 << "\t1\t" << hashGen.GetPlane(1) << "\n"
                 << "\t2\t" << hashGen.GetPlane(2) << "\n"
                 << "\t3\t" << hashGen.GetPlane(3) << "\n";
            cerr << "\t=\t" << hashGen.GetHashValue() << endl;
            throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
        }
    }

    hashGen.AddIupacNa( 'A' );

    for( CHashIterator iter( hashGen ); iter; ++iter ) {
        if( *iter != 0x00e4 ) {
            cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t"<< *iter << endl;
            cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
                 << "\t1\t" << hashGen.GetPlane(1) << "\n"
                 << "\t2\t" << hashGen.GetPlane(2) << "\n"
                 << "\t3\t" << hashGen.GetPlane(3) << "\n";
            cerr << "\t=\t" << hashGen.GetHashValue() << endl;
            throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
        }
    }

    Uint8 fwindow = 0;
    Uint8 rwindow = 0;
    
    fwindow = Iupacna2Ncbi4naRevCompl( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

    TESTVAL( string, Ncbi4naToIupac( fwindow, 15 ), "TGKCYSBAWRDMHVN" );

    rwindow = Ncbi4naRevCompl( fwindow, 15 );

    TESTVAL( string, Ncbi4naToIupac( rwindow, 15 ), "NBDKHYWTVSRGMCA" );

    fwindow = Iupacna2Ncbi2na( (const unsigned char*)"ACGTATCGGCTATGCA", 15 );

    TESTVAL( Uint4, fwindow, 0x1b369ce4 );

    fwindow = Iupacna2Ncbi2naRevCompl( (const unsigned char*)"TGCATAGCCGATACGT", 15 );
    TESTVAL( string, CBitHacks::AsBits(Uint4(fwindow)), CBitHacks::AsBits(0x06cda739U) );

    rwindow = Ncbi2naRevCompl( fwindow, 15 );
    TESTVAL( string, CBitHacks::AsBits( Uint4(rwindow) ), CBitHacks::AsBits(0x24c9631bU) );

    TESTVAL(string, Ncbi2naToIupac( rwindow, 15 ),"TGCATAGCCGATACG"); // last char is changed to A - is out of window

//     const char line[] = 
//         " 40   6 -40 -40 "" 40  13 -40 -40 "" 40  19 -40 -40 "" 40  25 -40 -40 "
//         " 40  30 -40 -40 "" 40  34 -40 -40 "" 40  37 -40 -40 "" 40  39 -40 -40 "
//         " 40  40 -40 -40 "" 40  40 -30 -40 "" 40  40 -20 -40 "" 40  40 -10 -40 "
//         " 40  40   0 -40 "" 40  40  10 -40 "" 40  40  20 -40 "" 40  40  30 -40 "
//         " 40  40   0 -40 "" 40  40  10 -40 "" 40  40  20 -40 "" 40  40  40 -40";
//     typedef vector<unsigned char> TNcbipna;
//     TNcbipna ncbipna;
//     cout << "Testing solexa => iupacna sensitivity\n";
//     Solexa2Ncbipna( back_inserter( ncbipna ), line );
// //     for( TNcbipna::const_iterator i = ncbipna.begin(); i != ncbipna.end(); i += 5 ) {
// //         cout << Ncbi4naToIupac( Ncbipna2Ncbi4na( &*i ) );
// //     }
//     for( unsigned i = 0; i < ncbipna.size()/20; i ++ ) {
//         cout << Ncbi4naToIupac(
//             ( Ncbipna2Ncbi4na( &ncbipna[i*20 +  0] ) << 0x0 ) |
//             ( Ncbipna2Ncbi4na( &ncbipna[i*20 +  5] ) << 0x4 ) |
//             ( Ncbipna2Ncbi4na( &ncbipna[i*20 + 10] ) << 0x8 ) |
//             ( Ncbipna2Ncbi4na( &ncbipna[i*20 + 15] ) << 0xc ) );
//     }
//     cout << endl;

    return 0;
}

