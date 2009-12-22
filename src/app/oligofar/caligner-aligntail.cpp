#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"
#include "string-util.hpp"

USING_OLIGOFAR_SCOPES;

void CAligner::MakeLocalSpliceSet( CIntron::TSet& iSet, int qdir, int qpos ) const
{
    if( NoSplicesDefined() ) return;
    const CAlignParam::TSpliceSet& ss = GetSpliceSet();
    iSet.reserve( ss.size() + 1 ); // since we will be pushing end of sequence marker to save one "if" in loop
    if( qdir < 0 ) {
        for( CIntron::TSet::const_iterator i = ss.begin() ; i != ss.end(); ++i ) {
            if( i->GetPos() <= qpos ) 
                iSet.insert( CIntron( qpos - i->GetPos(), *i ) );
            else break;
        }
    } else {
        for( CIntron::TSet::const_iterator i = ss.begin(); i != ss.end(); ++i ) {
            if( i->GetPos() <= qpos ) continue;
            for( ; i != ss.end() ; ++i ) {
                iSet.insert( CIntron( i->GetPos() - qpos - 1, *i ) );
            }
            break;
        }
    }
}

bool CAligner::AlignTail( int size, int qdir, const char * qseq, const char * sseq, int& qe, int& se, TTranscript& t, int qpos ) 
{
    int qi = m_qryInc * qdir; // we may walk reverse by query
    int si = m_sbjInc * qdir; // which affects also walk on subject

    ////////////////////////////////////////////////////////////////////////
    // m_extentionPenaltyDropoff and m_penaltyLimit are orthogonal things. First indicates 
    // when alignment should be stopped even if we did not reach end of query or penaltyLimit,
    // the second indicates that the penalty is too high for the alignment to be reported

    ////////////////////////////////////////////////////////////////////////
    // we start from first position which was not matched or is unmatched
    qseq += qi; 
    sseq += si;

    const char * qend = qseq + size*qi;
    const char * send = si > 0 ? (m_sbj + m_sbjLength) : (m_sbj - 1);

    qe = se = -1;

    int qlen = (qend - qseq)/qi;
    int slen = (send - sseq)/si;

    double  pm = -( m_penaltyLimit - m_penalty );
    double  go = m_scoreParam->GetGapBasePenalty();
    double  ge = m_scoreParam->GetGapExtentionPenalty();
    double  id = m_scoreParam->GetIdentityScore();
    double  mm = m_scoreParam->GetMismatchPenalty();

    ASSERT( go >  0 );
    ASSERT( ge >  0 );
    if( pm < 0 ) {
        cerr << "Warning: AlignTail got unexpected pm value < 0 : " << DISPLAY( pm ) << DISPLAY( m_penaltyLimit ) << DISPLAY( m_penalty ) << ": fail alignment\n";
        return false;
    }
    
    CIndelLimits glim( pm, m_scoreParam, m_alignParam );

    if( NoSplicesDefined() ) { // introns disable diagonal alignment for now 
        if( (!glim.AreIndelsAllowed()) || size * id < ge + go ) 
            return AlignTailDiag( size, qdir, pm, qseq, sseq, qe, se, t, qpos );
    }

    ++m_algoSWend;

    int dm0 = glim.GetDelBand();
    int im0 = glim.GetInsBand();

    int dm = glim.GetMaxDelLength();
    int im = glim.GetMaxInsLength();

    ////////////////////////////////////////////////////////////////////////
    // Init matrix
    do {
        CCell cell( - numeric_limits<double>::max() /*m_penaltyLimit - m_penalty - go - ge*/, eEvent_NULL, 1 ); 
        fill( m_matrix.begin(), m_matrix.end(), cell );
    } while(0);

    ////////////////////////////////////////////////////////////////////////
    // intronsets are supposed to be really small - 0,1,2,3 elements
    CIntron::TSet iSet;
    MakeLocalSpliceSet( iSet, qdir, qpos );
    iSet.insert( CIntron( size, numeric_limits<int>::max() ) );

    ////////////////////////////////////////////////////////////////////////
    // Init matrix edges
    SetMatrix( -1, -1, 0, eEvent_NULL, 1 );
    for( int q = 0, Q = min( m_matrixSize, im0 ); q < Q; ++q ) 
        SetMatrix(  q, -1, - ( go + ge * (q + 1) ), eEvent_Insertion, q + 1 );
    for( int s = 0, S = min( m_matrixSize, dm0 ); s < S; ++s ) 
        SetMatrix( -1,  s, - ( go + ge * (s + 1) ), eEvent_Deletion, s + 1 );

    ////////////////////////////////////////////////////////////////////////
    // Notes: 
    // 1. We may recompute maximal insertions and deletions allowed for each cell and for each q-plane
    // 2. We should stop when for all q-plane max insertions and deletions is 0 and no scores above 
    //    limit are found
    // 3. Try recursive call to Align with matrix storing best result of NULL (to recompute the result)
    
    double endScore = 0;
    double aq = 0;
    double lastaq = 0;

    // intronsets are supposed to be really small - 0,1,2,3 elements
    CIntron::TSet::const_iterator iPos = iSet.begin();

    int bestSq = -1;
    // NB: next two values have changed from -1 to 0 on 2008/04/15 to fix a problem 
    // when only perfect alignment with no splices is allowed here (result was that 
    // in main part of the loop iteration by s was from q-1 to q-1). 
    int baseSmin = 0;
    int baseSmax = 0;

    double bestSscore = 0; // last computed

    int goodpos = (int)(size - pm/m_scoreParam->GetIdentityScore()); // TODO: check rounding

    ////////////////////////////////////////////////////////////////////////
    // Compute matrix
    for( int q = 0; q < qlen; ++q, (qpos += qdir) ) {

#if 1
        if( iPos->GetPos() == q ) { // here we adjust matrix
            // For intron: (1) adjust baseS, (2) modify matrix
            baseSmin = max( m_matrixOffset, bestSq + iPos->GetMin() ); // we need - q since we will be adding q... 
            baseSmax = min( slen - 1,       bestSq + iPos->GetMax() ); // alternatively we would have to add var baseQ
            for( int s = baseSmin; s <= baseSmax; ++s ) {
                int d = bestSq - s;
                if( d != 0 ) SetMatrix( q - 1, s, bestSscore, d < 0 ? eEvent_Splice : eEvent_Overlap, abs(d) );
            }
            for( int s = baseSmin - 1, S = max( m_matrixOffset, baseSmin - im0 ); s >= S; --s ) {
                const CCell& cell( GetMatrix( q, s ) );
                double sc = bestSscore - go - (baseSmin - s)*ge;
                if( cell.GetScore() < sc ) {
                    int d = bestSq - s;
                    if( d ) SetMatrix( q - 1, s, sc, d < 0 ? eEvent_Splice : eEvent_Overlap, abs( d ) );
                }
            }
            for( int s = baseSmax + 1, S = min( slen - 1, baseSmax + dm0 ); s <= S; ++s ) {
                const CCell& cell( GetMatrix( q, s ) );
                double sc = bestSscore - go - (s - baseSmax)*ge;
                if( cell.GetScore() < sc ) {
                    int d = bestSq - s;
                    if( d ) SetMatrix( q - 1, s, sc, d < 0 ? eEvent_Splice : eEvent_Overlap, abs( d ) );
                }
            }
            baseSmin -= q - 1;
            baseSmax -= q - 1;
            ++iPos;
        }
#endif

        ////////////////////////////////////////////////////////////////////////
        // Now as usual
        int s = max( m_matrixOffset + 1, baseSmin + q - im0 ); // band is of the same width!
        int S = min( slen, baseSmax + q + dm0 + 1 );
        double pq = numeric_limits<double>::max();
        double bq = id; //BestScore( qseq + q*qi );

        bestSscore = - numeric_limits<double>::max();

        if( s >= S ) {
            if( s >= slen ) { break; } // no reason to align read against nothing... 
            THROW( logic_error, "Oops... unexpected range limitation in optimized S-W" );
        }

        aq += bq;
        ++m_queryBasesChecked;

        for( ; s < S; ++s ) {
            double wqs = Score( qseq + q*qi, sseq + s*si );

            EEvent event = wqs > 0 ? eEvent_Match : eEvent_Replaced;
            int count = 1;
            do {
                const CCell& cell = GetMatrix( q - 1, s - 1 );
                wqs += cell.GetScore();
                if( cell.GetEvent() == event ) count += cell.GetCount();
            } while(0);
            for( int qq = max( 0, q - im ); qq < q; ++qq ) {
                double wqsb = GetMatrix( qq, s ).GetScore() - go - ge * ( q - qq );
                if( wqsb > wqs ) {
                    wqs = wqsb;
                    event = eEvent_Insertion;
                    count = ( q - qq );
                }
            }
            for( int ss = max( 0, s - dm ); ss < s; ++ss ) {
                double wqsc = GetMatrix( q, ss ).GetScore() - go - ge * ( s - ss );
                if( wqsc > wqs ) {
                    wqs = wqsc;
                    event = eEvent_Deletion;
                    count = ( s - ss );
                }
            }
            // we need it always here because it may happen that we will
            // be computing other values of matrix based on this even though 
            // it is bad (we need to know it is bad!)
            SetMatrix( q, s, wqs, event, count ); 
            double pqs = aq - wqs;
            // TODO: optimize next three "if"s.
            if( pqs < pq ) { pq = pqs; }
            if( pqs < pm && wqs > endScore ) {
                endScore = wqs;
                lastaq = aq;
                qe = q;
                se = s;
            }
            if( wqs > bestSscore ) {
                bestSscore = wqs;
                bestSq = s;
            }
        }
        if( q < goodpos && pq > pm ) return false; // no need to compute anything else
        dm = min( dm, int( (pm - pq - go)/ge ) );
        if( pq > m_extentionPenaltyDropoff ) break; // stop alignment extension
    }
    m_penalty += endScore - lastaq;
    m_penalty -= (qlen - 1 - qe)*(mm); // this corrects dovetail penalty
    if( m_penalty < m_penaltyLimit ) return false;
    ////////////////////////////////////////////////////////////////////////
    // here we reconstruct transcript
    // first remove trailing mismatches, which may happen at least in case of completely unaligned short (1-2 bases) dovetail
    while( qe >= 0 || se >= 0 ) {
        const CCell& cell = GetMatrix( qe, se );
        if( cell.GetEvent() == eEvent_Replaced ) {
            qe -= cell.GetCount();
            se -= cell.GetCount();
            m_penalty -= cell.GetCount() * mm;
            if( m_penalty < m_penaltyLimit ) return false;
        } else break;
    }
    ////////////////////////////////////////////////////////////////////////
    // then trace back all the rest of path
    int q = qe;
    int s = se;
    if( q < qlen - 1 ) t.AppendItem( eEvent_SoftMask, qlen - q - 1 );
    while( q >= 0 || s >= 0 ) {
        const CCell& cell = GetMatrix( q, s );
        t.AppendItem( cell.GetEvent(), cell.GetCount() );
        switch( cell.GetEvent() ) {
        case eEvent_Overlap:   s += cell.GetCount(); break; // overlap
        case eEvent_Insertion: q -= cell.GetCount(); break;
        case eEvent_Splice:    // intron
        case eEvent_Deletion:  s -= cell.GetCount(); break;
        case eEvent_Replaced:
        case eEvent_Match:     q -= cell.GetCount(); 
                               s -= cell.GetCount();
                               break;
        default: THROW( logic_error, "Unexpected event " << cell << " in back trace\n" );
        }
    }
    if( s < -1 ) t.AppendItem( eEvent_Overlap, -1 - s );
    return true;
}


