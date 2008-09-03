#ifndef OLIGOFAR_IUPACUTIL__HPP
#define OLIGOFAR_IUPACUTIL__HPP

#include "cbithacks.hpp"
#include <cmath>

BEGIN_OLIGOFAR_SCOPES

inline string Ncbi2naToIupac( unsigned char w );
inline string Ncbi4naToIupac( unsigned short w );

inline string Ncbi2naToIupac( Uint4 window, unsigned windowSize );
inline string Ncbi4naToIupac( Uint8 window, unsigned windowSize );

inline unsigned char Iupacna2Ncbi4na( unsigned char c );
inline unsigned char Iupacna2Ncbi4naCompl( unsigned char c );
inline unsigned char Iupacna2Ncbi2na( unsigned char c );
inline unsigned char Iupacna2Ncbi2naCompl( unsigned char c );
inline unsigned char Ncbi4na2Ncbi2na( unsigned char c );

inline char IupacnaComplement( char iupacna );
inline string IupacnaRevCompl( const string& iupacna );

inline unsigned char Ncbi4naCompl( unsigned char x );
inline unsigned char Ncbi2naCompl( unsigned char x );

inline unsigned char Ncbipna2Ncbi4na( unsigned char a, unsigned char c,
                                      unsigned char g, unsigned char t, 
                                      unsigned char n = 255, 
                                      unsigned short score = 0x7f );

inline unsigned char Ncbipna2Ncbi4naCompl( unsigned char a, unsigned char c,
                                           unsigned char g, unsigned char t, 
                                           unsigned char n = 255,
                                           unsigned short score = 0x7f );

inline unsigned char Ncbipna2Ncbi4na( const unsigned char * p, unsigned short score = 0x7f );
inline unsigned char Ncbipna2Ncbi4naCompl( const unsigned char * p, unsigned short score = 0x7f );
inline unsigned char Ncbipna2Ncbi4naN( const unsigned char * p, unsigned short score = 0x7f );
inline unsigned char Ncbipna2Ncbi4naComplN( const unsigned char * p, unsigned short score = 0x7f );

inline unsigned char Ncbiqna2Ncbi4na( const unsigned char * p, unsigned short score = 3 );
inline unsigned char Ncbiqna2Ncbi4naCompl( const unsigned char * p, unsigned short score = 3 );

template<class iterator>
inline iterator Solexa2Ncbipna( iterator dest, const string& line, int base );
template<class iterator>
inline iterator Iupacnaq2Ncbapna( iterator dest, const string& iupac, const string& qual, int base );

inline double ComputeComplexity( unsigned hash, unsigned bases );

inline Uint8 Ncbipna2Ncbi4na( const unsigned char * data, unsigned windowLength, unsigned short score = 0x7f );
inline Uint8 Ncbipna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength, unsigned short score = 0x7f );
inline Uint8 Ncbiqna2Ncbi4na( const unsigned char * data, unsigned windowLength, unsigned short score = 3 );
inline Uint8 Ncbiqna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength, unsigned short score = 3 );
inline Uint8 Ncbi8na2Ncbi4na( const unsigned char * data, unsigned windowLength );
inline Uint8 Ncbi8na2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength );
inline Uint8 Iupacna2Ncbi4na( const unsigned char * data, unsigned windowLength );
inline Uint8 Iupacna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength );
inline Uint4 Iupacna2Ncbi2na( const unsigned char * window, unsigned windowSize );
inline Uint4 Iupacna2Ncbi2naRevCompl( const unsigned char * data, unsigned windowLength );
inline Uint8 Ncbi4naRevCompl( Uint8 window, unsigned windowSize );
inline Uint4 Ncbi2naRevCompl( Uint4 window, unsigned windowSize );

inline bool Ncbi4naIsAmbiguous( unsigned t );
inline unsigned Ncbi4naAlternativeCount( unsigned t );
inline Uint8 Ncbi4naAlternativeCount( Uint8 t, unsigned windowSize );

#include "iupac-util.impl.hpp"

END_OLIGOFAR_SCOPES

#endif
