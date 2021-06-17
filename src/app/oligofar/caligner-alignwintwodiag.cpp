#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"

USING_OLIGOFAR_SCOPES;

bool CAligner::AlignWinTwoDiag( 
        const char * & q, const char * & Q, const char * & s, const char * & S, 
        int splice, int smin, int smax, TTranscript& tw )
{
    int sl = ( S - s )/m_sbjInc + 1; // subject window length
    int ql = ( Q - q )/m_qryInc + 1; // query window length

    ++m_algoSWwin; 

   // THROW( logic_error, __PRETTY_FUNCTION__ << " should be fixed first!" );

    const char * qq = q;
    const char * ss = s;

    double id = m_scoreParam->GetIdentityScore();
    double mm = m_scoreParam->GetMismatchPenalty();
    double go = m_scoreParam->GetGapBasePenalty();
    double ge = m_scoreParam->GetGapExtentionPenalty();
    double noSplicePenalty = 0;

    if( smin > 0 ) noSplicePenalty = go + smin * ge;
    if( smax < 0 ) noSplicePenalty = go - smax * ge;

    // Main diagonal
    double wqq = 0;
    SetMatrix( -1, -1, 0.0, eEvent_NULL, 1 );
    for( int qs = 0, QS = ql; qs < QS; ++qs, (qq += m_qryInc), (ss += m_sbjInc) ) {
        double sc = Score( qq, ss );
        double er = qs == (splice + 1) ? noSplicePenalty : 0; //(qs == ( s0 + splice ) ) ? noSplicePenalty : 0;
        EEvent ev = sc > 0 ? eEvent_Match : eEvent_Replaced;
        SetMatrix( qs, qs, wqq += sc - er, ev, 1 );
    }

    // consider that if there is a splice just before the window it is tail aligner's 
    // responsibility to handle it

    double splicePenalty = 0;
    if( sl - ql < smin ) splicePenalty = go + (smin - (sl - ql)) * ge;
    if( sl - ql > smax ) splicePenalty = go - (smax - (sl - ql)) * ge;

    int q0 = max( 0, ql - sl );
    int s0 = max( 0, sl - ql );
    int du = abs( ql - sl );
    int dd = (splice < ql ? q0 : 0);

    /*
    if( splice < ql && q0 ) {
        dd -= q0;
        s0 -= q0;
        q0 -= q0;
    }
    */
    double aq = q0 * id;
    double gap = go + du * ge;

    qq = q0 * m_qryInc + q;
    ss = s0 * m_sbjInc + s;
    
    //if( m_sbjInc < 0 ) { while( ss >= m_sbj + m_sbjLength ) { ss += m_sbjInc; qq += m_qryInc; ++q0; ++s0; ++dd; } }
    //if( m_sbjInc > 0 ) { while( ss < m_sbj ) { ss += m_sbjInc; qq += m_qryInc; ++q0; ++s0; ++dd; } }

    EEvent evGap = ( ql > sl ? eEvent_Insertion : eEvent_Deletion );
    EEvent evSplice = ( ql > sl ? eEvent_Overlap : eEvent_Splice );

    double wqs = -gap;
    SetMatrix( q0 - 1, s0 - 1, wqs, evGap, du );

    bool indelFound = false;

    for( int qi = q0, si = s0; qi < ql && si < sl; ++dd, ++qi, ++si, (qq += m_qryInc), (ss += m_sbjInc) ) {
        double sc = Score( qq, ss );
        double sa = wqs + sc;
        double sb = dd == splice ? 
            (GetMatrix( qi, qi ).GetScore() - splicePenalty) : 
            (GetMatrix( dd, dd ).GetScore() - gap);
        aq += id;
        EEvent ev = eEvent_NULL;
        if( sa > sb ) {
            if( m_penalty + sa - aq < m_penaltyLimit - (m_wordDistance)*mm ) return false;
            SetMatrix( qi, si, wqs = sa, ev = sc > 0 ? eEvent_Match : eEvent_Replaced, 1 );
        } else {
            if( m_penalty + sb - aq < m_penaltyLimit - (m_wordDistance)*mm ) return false;
            SetMatrix( qi, si, wqs = sb, ev = (dd == splice ? evSplice : evGap), du );
            indelFound = true;
        }
    }

    int qe = ql - 1;
    int se = sl - 1;
    int qb = 0;
    int sb = 0;

    double diagonal = GetMatrix( qe, qe ).GetScore();
    double shifted  = GetMatrix( qe, se ).GetScore();

    if( diagonal > shifted ) { 
        if( ql > sl ) {
            Q -= du * m_qryInc;
            qe -= du;
            aq -= du * id;
        } else {
            S -= du * m_sbjInc;
            se -= du;
        }
        diagonal = GetMatrix( qe, se ).GetScore();
        m_penalty = diagonal - aq;
    } else if( !indelFound ) {
        if( ql > sl ) {
            q += du * m_qryInc;
            qb += du;
            aq -= du * id;
        } else {
            S += du * m_sbjInc;
            sb += du;
        }
        
        shifted = GetMatrix( qe, se ).GetScore() - GetMatrix( qb - 1, sb - 1).GetScore();
        m_penalty = shifted - aq;
    } else {
        m_penalty = shifted - aq;
    }

    if( m_penalty < m_penaltyLimit - (m_wordDistance)*mm ) return false;

    int distance = 0;
    while( qe >= qb || se >= sb ) {
        const CCell& cell = GetMatrix( qe, se );
        if( cell.GetEvent() != eEvent_Match ) ++distance; // eEvent_Changed should not appear here yet
        if( distance > m_wordDistance ) return false;
        tw.AppendItem( TTrItem( cell ) );
        switch( cell.GetEvent() ) {
        case eEvent_NULL:      THROW( logic_error, "Null is unexpected in back trace" );
        case eEvent_Overlap:   se += cell.GetCount(); break; // overlap
        case eEvent_Insertion: qe -= cell.GetCount(); break;
        case eEvent_Splice:   // intron
        case eEvent_Deletion:  se -= cell.GetCount(); break;
        case eEvent_Replaced:
        case eEvent_Match:     qe -= cell.GetCount(); 
                               se -= cell.GetCount();
                               break;
        default: THROW( logic_error, "Event " << cell << " should not appear here" );
        }
    }

    // Adjustment happens in the calling function (Align())
    return true;
    // AdjustWindowBoundary( q, Q, s, S, tw ); // to handle cases like 1S35M or 17S18M or 5M2R9M1R15M2S for -n1 -w16 -f16 -N2 
    // return m_penalty >= m_penaltyLimit;                                                                                                                                   
}

