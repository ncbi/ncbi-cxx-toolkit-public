#include <ncbi_pch.hpp>
#include "cquery.hpp"
#include "cscoretbl.hpp"

USING_OLIGOFAR_SCOPES;

Uint4 CQuery::s_count = 0;

double CQuery::ComputeBestScore( const CScoreTbl& scoreTbl, int component ) const
{
	// TODO: it may be computed much more carefuly... this is fast and dirty solution here
	return scoreTbl.GetIdentityScore() * GetLength( component );
}
