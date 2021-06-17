#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"

USING_OLIGOFAR_SCOPES;

bool CAligner::AlignDiag( const char * q, const char * Q, const char * s, const char * S, bool reverseStrand )
{
    // Since this is NOT extension of an alignment, no m_extentionPenaltyDropoff should be used here
    m_algoX0all++;

    ////////////////////////////////////////////////////////////////////////
    // 1. Adjust positions to the beginning and to the end of the read

    int qhead = m_anchorQ1;
    int qtail = m_qryLength - m_anchorQ2 - 1;

    q -= qhead * m_qryInc;
    Q += qtail * m_qryInc;
    m_posQ1 = 0;
    m_posQ2 = m_qryLength - 1;

    if( !reverseStrand ) {
        s -= qhead;
        S += qtail;
        m_posS1 = m_anchorS1 - qhead;
        m_posS2 = m_anchorS2 + qtail;
    } else {
        s += qhead;
        S -= qtail;
        m_posS2 = m_anchorS1 + qhead;
        m_posS1 = m_anchorS2 - qtail;
    }

    ////////////////////////////////////////////////////////////////////////
    // 1.5. Check subject boundaries and adjust them if necessary (TODO: check this mess!!)
    ASSERT( m_posS1 < m_posS2 );
    if( !reverseStrand ) {
        if( m_posS1 < 0 ) { 
            int d = -m_posS1; 
            m_posS1 += d;
            m_posQ1 += d;
            s += d;
            q += d * m_qryInc;
        }
        if( m_posS2 >= m_sbjLength ) {
            int d = m_posS2 - m_sbjLength + 1;
            m_posS2 -= d;
            m_posQ2 -= d;
            S -= d;
            Q -= d * m_qryInc;
        }
    } else {
        if( m_posS1 < 0 ) {
            int d = -m_posS1;
            m_posS1 += d;
            m_posQ2 -= d;
            S += d;
            Q -= d * m_qryInc;
        }
        if( m_posS2 >= m_sbjLength ) {
            int d = m_posS2 - m_sbjLength + 1;
            m_posS2 -= d;
            m_posQ1 += d;
            s -= d;
            q += d * m_qryInc;
        }
    }

    double id = m_scoreParam->GetIdentityScore();
    double mi = m_scoreParam->GetMismatchPenalty();

    m_transcript.reserve( size_t( -m_penaltyLimit / (id/* + mi*/) + 1 ) );
    CDiagonalScores scores( id );

    ////////////////////////////////////////////////////////////////////////
    // 2. Walk along the main diagonal only
    int prefix = (Q - q)/m_qryInc;
    int suffix = prefix;
    if( !reverseStrand ) {
        for( int left = 0, right = suffix; q <= Q ; (q += m_qryInc), (s += m_sbjInc), ++left, --right ) {
            ++m_queryBasesChecked;
            double score = Score( q, s );
            if( score > 0 ) { prefix = min( prefix, left ); suffix = right; }
            if( scores.Add( score ).ExceedsPenalty( -m_penaltyLimit + (prefix + suffix)*mi ) ) return false;
            AppendTranscriptItem( scores.GetLastEvent() );
        }
    } else { 
        for( int left = prefix, right = 0; q <= Q ; (Q -= m_qryInc), (S -= m_sbjInc), ++right, --left ) {
            ++m_queryBasesChecked;
            double score = Score( Q, S );
            if( score > 0 ) { suffix = min( right, suffix ); prefix = left; }
            if( scores.Add( score ).ExceedsPenalty( -m_penaltyLimit + (prefix + suffix)*mi ) ) return false;
            AppendTranscriptItem( scores.GetLastEvent() );
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // 3. Adjust transcript and alignment positions
    if( m_transcript.front().GetEvent() == eEvent_Replaced ) {
        if( !reverseStrand ) m_posQ1 += m_qryInc * m_transcript.front().GetCount();
        else                 m_posQ2 -= m_qryInc * m_transcript.front().GetCount();
        m_posS1 += m_transcript.front().GetCount();
        m_transcript.front().SetEvent( eEvent_SoftMask );
    }
    if( m_transcript.back().GetEvent() == eEvent_Replaced ) {
        if( !reverseStrand ) m_posQ2 -= m_qryInc * m_transcript.back().GetCount();
        else                 m_posQ1 += m_qryInc * m_transcript.back().GetCount();
        m_posS2 -= m_transcript.back().GetCount();
        m_transcript.back().SetEvent( eEvent_SoftMask );
    }

    ////////////////////////////////////////////////////////////////////////
    // 4. Get correct score
    m_penalty = -scores.GetAccumulatedPenalty() + (prefix + suffix)*mi;
    ++m_successAligns;

    return m_penalty >= m_penaltyLimit;
}


