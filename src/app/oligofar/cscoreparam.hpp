#ifndef OLIGOFAR_CSCOREPARAM__HPP
#define OLIGOFAR_CSCOREPARAM__HPP

#include "defs.hpp"
#include "cbithacks.hpp"
#include <cmath>

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// Shared precomputed tables for all IScore-derived instances
class CScoreParam
{
public:
    CScoreParam( 
        double id =  1.0,
        double mi = -1.0,
        double go = -3.0,
        double ge = -1.5 ) :
        m_identityScore( id ),
        m_mismatchPenalty( -mi ),
        m_gapBasePenalty( -go + ge ),
        m_gapExtPenalty( -ge ) {
        x_InitSubjIdentityTable();
        x_InitScoreForPhrap();
    }
    double GetIdentityScore() const { return m_identityScore; } 
    double GetMismatchPenalty() const { return m_mismatchPenalty; }
    double GetGapBasePenalty() const { return m_gapBasePenalty; }
    double GetGapExtentionPenalty() const { return m_gapExtPenalty; }
    double GetGapOpeningPenalty() const { return m_gapBasePenalty + m_gapExtPenalty; }
    double GetIdentityScore( unsigned ncbi8naSubj ) const { return m_identityTbl[ncbi8naSubj]; }
    double GetMismatchScoreForPhrap( unsigned phrap ) const { return m_scoreForPhrap[phrap]; }

protected:
    void x_InitSubjIdentityTable();
    void x_InitScoreForPhrap();

protected:
    double m_identityScore;
    double m_mismatchPenalty;
    double m_gapBasePenalty;
    double m_gapExtPenalty;
    double m_identityTbl[16];
    double m_scoreForPhrap[64];
};

///////////////////////////////////////////////////////////////////////
// implementations

inline void CScoreParam::x_InitSubjIdentityTable()
{
    m_identityTbl[0] = 0;
    for( unsigned i = 1; i < 16; ++i ) {
        double p = 1.0/CBitHacks::BitCount1( i );
        m_identityTbl[i] = p*GetIdentityScore() - (1.0 - p)*GetMismatchPenalty();
    }
}

inline void CScoreParam::x_InitScoreForPhrap()
{
    for( int i = 0; i < 64; ++i ) {
        m_scoreForPhrap[i] = pow( 10.0, -double(i)/10 ) * (GetIdentityScore() - GetMismatchPenalty()) + GetMismatchPenalty();
    }
}

END_OLIGOFAR_SCOPES

#endif
