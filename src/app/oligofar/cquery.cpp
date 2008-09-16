#include <ncbi_pch.hpp>
#include "cquery.hpp"
#include "tseqref.hpp"
#include "cscoretbl.hpp"

USING_OLIGOFAR_SCOPES;

Uint4 CQuery::s_count = 0;

double CQuery::ComputeBestScore( const CScoreTbl& scoreTbl, int component ) 
{
	switch( GetCoding() ) {
	case CSeqCoding::eCoding_colorsp:
	case CSeqCoding::eCoding_ncbi8na: return m_bestScore[component] = float( scoreTbl.GetIdentityScore() * GetLength( component ) );
	case CSeqCoding::eCoding_ncbiqna: return x_ComputeBestScore<CNcbiqnaBase,1,CSeqCoding::eCoding_ncbiqna>( scoreTbl, component );
	case CSeqCoding::eCoding_ncbipna: return x_ComputeBestScore<CNcbipnaBase,5,CSeqCoding::eCoding_ncbipna>( scoreTbl, component );
	default: THROW( logic_error, "Unknown encoding" );
	}
}

template <class CBase, int incr, CSeqCoding::ECoding coding>
double CQuery::x_ComputeBestScore( const CScoreTbl& scoreTbl, int component ) 
{
	double score = 0;
	TSeqRef<CBase,incr,coding> data( GetData( component ) );
	for( int i = 0, I = GetLength( component ); i < I; ++i, ++data ) {
        char ref = CNcbi8naBase( CNcbi8naBase( data.GetBase() ).GetSmallestNcbi2na() );
		score += scoreTbl.ScoreRef( data, TSeqRef<CNcbi8naBase,1,CSeqCoding::eCoding_ncbi8na>( &ref ), i, i ); // consider match to non-ambiguous base
    }
	m_bestScore[component] = float( score );
	return score;
}
