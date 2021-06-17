#include <ncbi_pch.hpp>
#include "cscoretbl.hpp"

USING_OLIGOFAR_SCOPES;

double * CScoreTbl::x_InitProbTbl() 
{
    static double tbl[16];
    tbl[0] = 0;
    for( int i = 1; i < 16; ++i ) 
        tbl[i] = 1.0 / double( "\xff\x1\x1\x2""\x1\x2\x2\x3""\x1\x2\x2\x3""\x2\x3\x3\x4"[i]);
    for( int i = 0; i < 64; ++i ) 
        s_phrapProbTbl[i] = 1.0 - std::pow( 10.0, -double( i ) / 10 );
    return tbl;
}

double * CScoreTbl::s_probtbl = 0;//x_InitProbTbl();
double CScoreTbl::s_phrapProbTbl[64];

