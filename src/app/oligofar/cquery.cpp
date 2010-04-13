#include <ncbi_pch.hpp>
#include "cquery.hpp"
#include "iscore.hpp"

USING_OLIGOFAR_SCOPES;

Uint4 CQuery::s_count = 0;

double CQuery::ComputeBestScore( const CScoreParam * score, int component ) const
{
#if DEVELOPMENT_VER
    return float( score->GetIdentityScore() * ( GetLength( component ) ) );
#else
    return m_bestScore[component] = float( score->GetIdentityScore() * GetLength( component ) );
#endif
}
