#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "fourplanes.hpp"

USING_OLIGOFAR_SCOPES;
USING_SCOPE(fourplanes);

class GPermutatorCallback 
{
public:
    GPermutatorCallback( int n ) : m_count(0), m_expected( n + n*3 ) {}
    void operator () ( Uint8 hash, int mism, Uint8 alt ) { ++m_count; }
    operator bool () const { return m_count == m_expected; }
protected:
    int m_count;
    int m_expected;
   // cerr << setw(3) << setfill('0') << ++count << "\t" << Ncbi2naToIupac( hash, 13 ) << "\t" << mism << "\t" << alt << endl;
};

int COligoFarApp::TestSuite()
{
//    Uint8 ncbi4na = 0;

    TESTVAL(int, CBitHacks::BitCount1( Uint1( '\x67' ) ), 5 );
    TESTVAL(int, CBitHacks::BitCount2( 0x4567u ), 8 );
    TESTVAL(int, CBitHacks::BitCount4( 0x01234567u ),12);
    TESTVAL(int, CBitHacks::BitCount8( 0x0123456789abcdefULL ), 32 );

    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits1( Uint1('\x01') ) ), CBitHacks::AsBits( Uint1('\x80') ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits2( Uint2( 0x0123U ) ) ), CBitHacks::AsBits( Uint2( 0xc480U ) ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits4( Uint4( 0x01234567UL ) ) ), CBitHacks::AsBits( Uint4( 0xe6a2c480UL ) ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBits8( 0x0123456789abcdefULL ) ), CBitHacks::AsBits( 0xf7b3d591e6a2c480ULL ) );
    TESTVAL( string, CBitHacks::AsBits( CBitHacks::ReverseBitPairs8( 0x0123456789abcdefULL ) ), CBitHacks::AsBits( 0xfb73ea62d951c840ULL ) );
    
    UintH uinth( 0x0123456789abcdefULL, 0x0123456789abcdefULL );
    TESTVAL( string, 
             CBitHacks::AsBits( uinth ), CBitHacks::AsBits( uinth.GetHi() ) + CBitHacks::AsBits( uinth.GetLo() ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0x0123456789abcdefULL ) ), 
             CBitHacks::AsBits( 0x0123456789abcdefULL ) + CBitHacks::AsBits( 0x0123456789abcdefULL ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0x0123456789abcdefULL ) ), 
             "00000001001000110100010101100111100010011010101111001101111011110000000100100011010001010110011110001001101010111100110111101111" );
    TESTVAL( string, 
             CBitHacks::AsBits( CBitHacks::ReverseBitsH( UintH( 0x0123456789abcdefULL, 0xf7b3d591e6a2c480ULL ) ) ), 
             CBitHacks::AsBits(                          UintH( 0x0123456789abcdefULL, 0xf7b3d591e6a2c480ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( CBitHacks::ReverseBitPairsH( UintH( 0x0123456789abcdefULL, 0xfb73ea62d951c840ULL ) ) ), 
             CBitHacks::AsBits(                              UintH( 0x0123456789abcdefULL, 0xfb73ea62d951c840ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) << 4 ), 
             CBitHacks::AsBits( UintH( 0x123456789abcdeffULL, 0xedcba98765432100ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) >> 4 ), 
             CBitHacks::AsBits( UintH( 0x00123456789abcdeULL, 0xffedcba987654321ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) << 32 ), 
             CBitHacks::AsBits( UintH( 0x89abcdeffedcba98ULL, 0x7654321000000000ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) >> 32 ), 
             CBitHacks::AsBits( UintH( 0x0000000001234567ULL, 0x89abcdeffedcba98ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) << 64 ), 
             CBitHacks::AsBits( UintH( 0xfedcba9876543210ULL, 0 ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) >> 64 ), 
             CBitHacks::AsBits( UintH( 0x0000000000000000ULL, 0x0123456789abcdefULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) << 96 ), 
             CBitHacks::AsBits( UintH( 0x7654321000000000ULL, 0 ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( UintH( 0x0123456789abcdefULL, 0xfedcba9876543210ULL ) >> 96 ), 
             CBitHacks::AsBits( UintH( 0x0000000000000000ULL, 0x0000000001234567ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( CBitHacks::InsertBits<UintH,4,0xf>( UintH( 0x1248124812481248ULL, 0x1248124812481248ULL ), 4 ) ), 
             CBitHacks::AsBits(                                     UintH( 0x2481248124812481ULL, 0x24812481248f1248ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( CBitHacks::InsertBits<UintH,4,0xf>( UintH( 0x1248124812481248ULL, 0x1248124812481248ULL ), 7 ) ), 
             CBitHacks::AsBits(                                     UintH( 0x2481248124812481ULL, 0x24812481f2481248ULL ) ) );
    TESTVAL( string, 
             CBitHacks::AsBits( CBitHacks::DeleteBits<UintH,4>( UintH( 0x1248124812481248ULL, 0x1248124812481248ULL ), 4 ) ), 
             CBitHacks::AsBits(                                 UintH( 0x0124812481248124ULL, 0x8124812481241248ULL ) ) );
    
    TESTVAL( UintH, UintH( 0 ) + UintH( 1 ), UintH( 1 ) );
    TESTVAL( UintH, UintH( 1, 0 ) + UintH( 1 ), UintH( 1, 1 ) );
    TESTVAL( UintH, UintH( ~Uint8(0) ) + UintH( 1 ), UintH( 1, 0 ) );
    TESTVAL( UintH, ~UintH( ~Uint8(0) ), UintH( ~Uint8(0), 0 ) );
    TESTVAL( UintH, UintH( 0 ) * UintH( 1 ), UintH( 0 ) );
    TESTVAL( UintH, UintH( 1 ) * UintH( 1 ), UintH( 1 ) );
    TESTVAL( UintH, UintH( 1 ) * UintH( 2 ), UintH( 2 ) );
    TESTVAL( UintH, UintH( 2 ) * UintH( 2 ), UintH( 4 ) );
    TESTVAL( UintH, UintH( 1, 1 ) * UintH( 2 ), UintH( 2, 2 ) );
    TESTVAL( UintH, UintH( 2, 1 ) * UintH( 2 ), UintH( 4, 2 ) );
    TESTVAL( UintH, UintH( 2, 2 ) * UintH( 2, 2 ), UintH( 8, 4 ) );
//     TESTVAL( UintH, CHashPlanes::Div( UintH( 12, 12 ), 1 ), UintH( 12, 12 ) );
//     TESTVAL( UintH, CHashPlanes::Div( UintH( 12, 12 ), 2 ), UintH(  6,  6 ) );
//     TESTVAL( UintH, CHashPlanes::Div( UintH( 12, 12 ), 3 ), UintH(  4,  4 ) );
//     TESTVAL( UintH, CHashPlanes::Div( UintH( 12, 12 ), 4 ), UintH(  3,  3 ) );
    

//     ncbi4na = Iupacna2Ncbi4na( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

//     TESTVAL( string, CBitHacks::AsBits(ncbi4na), CBitHacks::AsBits(0x0123456789abcdefULL) );

//     ncbi4na = Iupacna2Ncbi4naRevCompl( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

//     TESTVAL( string, CBitHacks::AsBits(ncbi4na), CBitHacks::AsBits(0x0f7b3d591e6a2c48ULL) );

//     CHashGenerator hashGen( 5 );
//     hashGen.AddIupacNa( 'A' );
//     hashGen.AddIupacNa( 'A' );
//     hashGen.AddIupacNa( 'A' );
//     hashGen.AddIupacNa( 'A' );
//     hashGen.AddIupacNa( 'A' );

//     for( CHashIterator iter( hashGen ); iter; ++iter ) {
//         if( *iter != 0 ) {
//             cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
//             cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
//                  << "\t1\t" << hashGen.GetPlane(1) << "\n"
//                  << "\t2\t" << hashGen.GetPlane(2) << "\n"
//                  << "\t3\t" << hashGen.GetPlane(3) << "\n";
//             cerr << "\t=\t" << hashGen.GetHashValue() << endl;
//             throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
//         }
//     }

//     hashGen.AddIupacNa( 'C' );

//     for( CHashIterator iter( hashGen ); iter; ++iter ) {
//         if( *iter != 0x0100 ) {
//             cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
//             cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
//                  << "\t1\t" << hashGen.GetPlane(1) << "\n"
//                  << "\t2\t" << hashGen.GetPlane(2) << "\n"
//                  << "\t3\t" << hashGen.GetPlane(3) << "\n";
//             cerr << "\t=\t" << hashGen.GetHashValue() << endl;
//             throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
//         }
//     }

//     hashGen.AddIupacNa( 'G' );

//     for( CHashIterator iter( hashGen ); iter; ++iter ) {
//         if( *iter != 0x0240 ) {
//             cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t"<< *iter << endl;
//             cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
//                  << "\t1\t" << hashGen.GetPlane(1) << "\n"
//                  << "\t2\t" << hashGen.GetPlane(2) << "\n"
//                  << "\t3\t" << hashGen.GetPlane(3) << "\n";
//             cerr << "\t=\t" << hashGen.GetHashValue() << endl;
//             throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
//         }
//     }

//     hashGen.AddIupacNa( 'T' );

//     for( CHashIterator iter( hashGen ); iter; ++iter ) {
//         if( *iter != 0x0390 ) {
//             cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t" << *iter << endl;
//             cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
//                  << "\t1\t" << hashGen.GetPlane(1) << "\n"
//                  << "\t2\t" << hashGen.GetPlane(2) << "\n"
//                  << "\t3\t" << hashGen.GetPlane(3) << "\n";
//             cerr << "\t=\t" << hashGen.GetHashValue() << endl;
//             throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
//         }
//     }

//     hashGen.AddIupacNa( 'A' );

//     for( CHashIterator iter( hashGen ); iter; ++iter ) {
//         if( *iter != 0x00e4 ) {
//             cerr << "TEST FAILED: " << setw(16) << setfill('0') << hex << *iter << dec << setw(0) << "\t"<< *iter << endl;
//             cerr << "\t0\t" << hashGen.GetPlane(0) << "\n"
//                  << "\t1\t" << hashGen.GetPlane(1) << "\n"
//                  << "\t2\t" << hashGen.GetPlane(2) << "\n"
//                  << "\t3\t" << hashGen.GetPlane(3) << "\n";
//             cerr << "\t=\t" << hashGen.GetHashValue() << endl;
//             throw logic_error( __FILE__ ":" + NStr::IntToString( __LINE__ ) );
//         }
//     }

//     Uint8 fwindow8 = 0;
//     Uint8 rwindow8 = 0;
    
//     fwindow8 = Iupacna2Ncbi4naRevCompl( (const unsigned char*)"NBDKHYWTVSRGMCA-", 15 );

//     TESTVAL( string, Ncbi4naToIupac( fwindow8, 15 ), "TGKCYSBAWRDMHVN" );

//     rwindow8 = Ncbi4naRevCompl( fwindow8, 15 );

//     TESTVAL( string, Ncbi4naToIupac( rwindow8, 15 ), "NBDKHYWTVSRGMCA" );

//  Uint4 fwindow4 = 0;
//  Uint4 rwindow4 = 0;

//     fwindow4 = Iupacna2Ncbi2na( (const unsigned char*)"ACGTATCGGCTATGCA", 15 );

//     TESTVAL( Uint4, fwindow4, 0x1b369ce4 );

//     fwindow4 = Iupacna2Ncbi2naRevCompl( (const unsigned char*)"TGCATAGCCGATACGT", 15 );
//     TESTVAL( string, CBitHacks::AsBits( fwindow4 ), CBitHacks::AsBits(0x06cda739U) );

//     rwindow4 = Ncbi2naRevCompl( fwindow4, 15 );
//     TESTVAL( string, CBitHacks::AsBits( rwindow4 ), CBitHacks::AsBits(0x24c9631bU) );

//     TESTVAL(string, Ncbi2naToIupac( rwindow4, 15 ),"TGCATAGCCGATACG"); // last char is changed to A - is out of window

// //     const char line[] = 
// //         " 40   6 -40 -40 "" 40  13 -40 -40 "" 40  19 -40 -40 "" 40  25 -40 -40 "
// //         " 40  30 -40 -40 "" 40  34 -40 -40 "" 40  37 -40 -40 "" 40  39 -40 -40 "
// //         " 40  40 -40 -40 "" 40  40 -30 -40 "" 40  40 -20 -40 "" 40  40 -10 -40 "
// //         " 40  40   0 -40 "" 40  40  10 -40 "" 40  40  20 -40 "" 40  40  30 -40 "
// //         " 40  40   0 -40 "" 40  40  10 -40 "" 40  40  20 -40 "" 40  40  40 -40";
// //     typedef vector<unsigned char> TNcbipna;
// //     TNcbipna ncbipna;
// //     cout << "Testing solexa => iupacna sensitivity\n";
// //     Solexa2Ncbipna( back_inserter( ncbipna ), line );
// // //     for( TNcbipna::const_iterator i = ncbipna.begin(); i != ncbipna.end(); i += 5 ) {
// // //         cout << Ncbi4naToIupac( Ncbipna2Ncbi4na( &*i ) );
// // //     }
// //     for( unsigned i = 0; i < ncbipna.size()/20; i ++ ) {
// //         cout << Ncbi4naToIupac(
// //             ( Ncbipna2Ncbi4na( &ncbipna[i*20 +  0] ) << 0x0 ) |
// //             ( Ncbipna2Ncbi4na( &ncbipna[i*20 +  5] ) << 0x4 ) |
// //             ( Ncbipna2Ncbi4na( &ncbipna[i*20 + 10] ) << 0x8 ) |
// //             ( Ncbipna2Ncbi4na( &ncbipna[i*20 + 15] ) << 0xc ) );
// //     }
// //     cout << endl;

//     CPermutator4b perm( 1, 1 );
//     GPermutatorCallback cbk( 13 );
//     perm.ForEach( 13, 0x1111111111111, cbk );

    return 0;
}

