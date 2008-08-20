#include <ncbi_pch.hpp>
#include "chit.hpp"
#include "cquery.hpp"

USING_OLIGOFAR_SCOPES;

Uint8 CHit::s_count = 0;

CHit::CHit( CQuery* q, Uint4 seqOrd, double score1, int from1, int to1, double score2, int from2, int to2 ) 
    : m_query( q ), m_next( 0 ), m_seqOrd( seqOrd ), 
      m_components( 3 ) 
{
    ASSERT( m_seqOrd != ~0U );
    m_score[0] = score1;
    m_score[1] = score2;
    m_from[0] = from1;
    m_from[1] = from2;
    m_to[0] = to1;
    m_to[1] = to2;
    ++s_count;
}

void CHit::SetTarget( bool pairmate, const char * begin, const char * end ) 
{ 
    if( begin == 0 || !HasComponent( pairmate ) ) {
        m_target[pairmate] = "";
        return;
    }
    int b = min( GetFrom( pairmate ), GetTo( pairmate ) );
    int l = abs( GetFrom( pairmate ) - GetTo( pairmate ) ) + 1;
    int p = 0, s = 0;
    if( b < 0 ) { 
        p = -b;
        l -= p;
        b = 0;
    }
    if( b + l > end - begin ) {
        s = b + l - (end - begin);
        l -= s;
    }
	if( l < 0 || l > 2*(int)m_query->GetLength( pairmate ) ) {
		m_target[pairmate] = "\xf"; // indication of bad alignment
	} else {
    	m_target[pairmate] = string( p, '\xf' ) + string( b + begin, l ) + string( s, '\xf' );
	}
}
