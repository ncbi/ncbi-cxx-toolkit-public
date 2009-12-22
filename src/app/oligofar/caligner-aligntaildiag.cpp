#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"

USING_OLIGOFAR_SCOPES;

bool CAligner::AlignTailDiag( int size, int qdir, double pm, const char * qseq, const char * sseq, int& qe, int& se, TTranscript& t, int qpos )
{
    // reminder: m_extentionPenaltyDropoff is compared to the local (extension) penalty, while m_penaltyLimit is compared to the whole alignment penalty
    ++m_algoX0end;
    CDiagonalScores scores( m_scoreParam->GetIdentityScore() );
    int qi = qdir * m_qryInc;
    int si = qdir * m_sbjInc;
    double lastpenalty = 0;
    double lastmatchpenalty = 0;
    int lastmatchpos = 0;
    int goodpos = (int)(size - pm/m_scoreParam->GetIdentityScore()); // TODO: check rounding

    int I = size;
    do {
        const char * send = si > 0 ? (m_sbj + m_sbjLength) : (m_sbj - 1);
        int slen = (send - sseq)/si;
        I = min( I, slen );
    } while(0);
    if( I < goodpos ) return false; // anyway tail will be too long for good penalty
    
    for( int i = 0; i < I; ++i, (qseq += qi), (sseq += si) ) {
        ++m_queryBasesChecked;
        double score = Score( qseq, sseq );
        scores.Add( score );
        if( i < goodpos && scores.ExceedsPenalty( pm ) ) return false;
        if( scores.ExceedsPenalty( m_extentionPenaltyDropoff ) ) { I = i; break; }
        lastpenalty = scores.GetAccumulatedPenalty();
        if( score > 0 ) {
            lastmatchpos = i;
            lastmatchpenalty = lastpenalty;
        }
        t.AppendItem( scores.GetLastEvent(), 1 );
        ++qe;
        ++se;
    }

//    qe = se = size - 1; // qe and se point to last position
    m_penalty -= lastmatchpenalty; // scores.GetAccumulatedPenalty();
    if( t.size() && t.back().GetEvent() == eEvent_Replaced ) {
        t.back().SetEvent( eEvent_SoftMask );
        // qe and se should point to last match
        qe -= t.back().GetCount();
        se -= t.back().GetCount(); 
        // NO NEED: penalty already contains it :: m_penalty -= t.back().second * m_scoreParam->GetMismatchPenalty();
        m_penalty -= t.back().GetCount() * m_scoreParam->GetIdentityScore();
    }
    if( I < size ) {
        m_penalty -= m_scoreParam->GetIdentityScore() * (size - I);
        t.AppendItem( eEvent_SoftMask, size - I );
    }
    reverse( t.begin(), t.end() );
    return m_penalty >= m_penaltyLimit;
}


