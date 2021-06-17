#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"
#include "calignparam.hpp"

USING_OLIGOFAR_SCOPES;

namespace {

inline CIntron::TSet::const_iterator GetRelevantSplice( const CIntron::TSet& intronSet, int qs, int qe ) 
{
    CIntron::TSet::const_iterator splice = intronSet.begin();
    if( splice != intronSet.end() ) {
        while( splice->GetPos() < qs ) if( ++splice == intronSet.end() ) break;
        if( splice->GetPos() >= qe ) splice = intronSet.end();
        else {
            CIntron::TSet::const_iterator next = splice;
            if( ++next != intronSet.end() )
                if( next->GetPos() < qe ) 
                    THROW( runtime_error, "Oops... two introns in the same window :-(. Hasher should not have allowed thisr:\n" 
                            DISPLAY( qs ) << DISPLAY( qe ) << DISPLAY( splice->GetPos() ) << DISPLAY( next->GetPos() ) );
        }
    }
    return splice;
}

////////////////////////////////////////////////////////////////////////
// After some analysis, it is clear, that if we have indels (which is if 
// (Q-q)*m_sbjInc != (S-s)*m_qryInc ) we can't use "scalar" procedure
// unless it is IUPAC input and no splices within word.
// Considering (for now) this "IUPAC no splices" case as exception and
// we don't implement it now. As result we have two cases: 
// - no indels (in this case if we have splices we may penalize if 
//   splice does not allow 0 size)
// - indels - in this case we use two-diagonal matrix approach with 
//   traceback
// Since result transcript of matrix will be produced in reverse order,
// to avoid extra logic and/or extra transcript reordering, in scalar 
// (no indels) case we walk back to produce transcript in reverse order 
// as well. Then CAligner::Align should be changed to account this.

bool CAligner::AlignWindow( const char * & q, const char * & Q, const char * & s, const char * & S, TTranscript& tw )
{
    int sl = ( S - s )/m_sbjInc + 1; // subject window length
    int ql = ( Q - q )/m_qryInc + 1; // query window length
    int qp = ( q - m_qry )/m_qryInc; // query position
    int ds = ( ql - sl );

    const CAlignParam::TSpliceSet& sSet = GetSpliceSet();

    CIntron::TSet::const_iterator splice = GetRelevantSplice( sSet, qp, qp + max( ql, sl ) );

    enum EFlags { fSplice = 0x04, fIndel = 0x02, fQual = 0x01 };
    int flags = 0;

    if( ds ) flags |= fIndel;
    if( splice != sSet.end() ) flags |= fSplice;
    if( m_qryCoding == CSeqCoding::eCoding_ncbiqna || m_qryCoding == CSeqCoding::eCoding_ncbipna ) flags |= fQual;

    double go = m_scoreParam->GetGapBasePenalty();
    double ge = m_scoreParam->GetGapExtentionPenalty();

    switch( flags ) {
    case 0x3: 
        return AlignWinTwoDiag( q, Q, s, S, ql + 1, 0, 0, tw );
    case 0x6: case 0x7: 
        return AlignWinTwoDiag( q, Q, s, S, splice->GetPos() - qp - 1, splice->GetMin(), splice->GetMax(), tw );
    case 0x2: // (?)
    case 0x0: case 0x1: break;
    case 0x4: case 0x5: 
        // this window contains splice, but no indels are present; 
        // adjust score if splice does not include 0
        if( splice->GetMax() < 0 ) m_penalty -= go - splice->GetMax() * ge; 
        else if( splice->GetMin() > 0 ) m_penalty -= go + splice->GetMin() * ge; 
        else break;
        if( m_penalty < m_penaltyLimit ) return false;
    }

    int du = 0;
    int qoff = 0;
    int soff = 0;

    if( ds > 0 )      { du = + ds; qoff = du * m_qryInc; soff = 0; }
    else if( ds < 0 ) { du = - ds; soff = du * m_sbjInc; qoff = 0; }

    double id = m_scoreParam->GetIdentityScore();
    double idi = ds > 0 ? id * du : 0;
    double gap = du ? go + du * ge : 0;
    double bs  = 0;
    double sc0 = 0;
    double sc1 = -gap;
    
    const char * q0 = q - m_qryInc + qoff;
    const char * s0 = s - m_sbjInc + soff;
    const char * q1 = Q;
    const char * s1 = S;

    ++m_algoX0win;

    TTrItem cache( eEvent_NULL, 0 );

    int distance = 0;

    // TODO: THIS CODE! TODO: THIS CODE! TODO: THIS CODE! TODO: THIS CODE!
    int left = 0;
    int right = ql + 1; // should not be numeric_limit since later it is used to 
                        // estimate error, which would work wrong if first comparison 
                        // is mismatch
    double xs = id + m_scoreParam->GetMismatchPenalty();

    ////////////////////////////////////////////////////////////////////////
    // Actually, this procedure should work for indel cases no splice as 
    // well, but should not be used if there ase quality scores
    for( int i = 0; s0 != s1 && q0 != q1 ; ( s1 -= m_sbjInc ), ( q1 -= m_qryInc ), ++i ) {
        ++m_queryBasesChecked;
        bs += id;
        double sc = Score( q1, s1 );
        EEvent ev = sc > 0 ? eEvent_Match : eEvent_Replaced;
        if( sc > 0 ) {
            right = min( right, i );
            left = ql - i;
        }
        if( du ) {
            sc0 += sc;
            sc1 += (sc = Score( q1 - qoff, s1 - soff ));
            if( sc0 - gap < sc1 ) {
                tw.AppendItem( qoff   ? eEvent_Insertion : eEvent_Deletion, du );
                tw.AppendItem( sc > 0 ? eEvent_Match : eEvent_Replaced,  1 );
                distance += du + (sc <= 0? 1 : 0);
                bs += idi;
                q1 -= qoff;
                s1 -= soff;
                m_penalty = - bs + sc1;
                du  = 0;
            } else {
                if( ev != eEvent_Match ) ++distance;
                tw.AppendItem( ev, 1 );
                sc1 = sc0 - gap;
                m_penalty = - bs + sc1;
            }
            if( distance > m_wordDistance ) return false;
            if( m_penalty + gap + (left + right)*xs < m_penaltyLimit ) return false;
        } else {
            if( ev != eEvent_Match ) ++distance;
            tw.AppendItem( ev, 1 );
            m_penalty -= id - sc;
            if( distance > m_wordDistance ) return false;
            if( m_penalty + gap + (left + right)*xs < m_penaltyLimit ) return false;
        }
    }

    if( du ) { m_penalty += gap; }

    q = q1 + m_qryInc;
    s = s1 + m_sbjInc;

    // Addustment of positions and score happens in the calling function (Align())
    return true;
    // cerr << DISPLAY( left ) << DISPLAY( right ) << DISPLAY( xs ) << DISPLAY( m_penalty ) << DISPLAY( m_penaltyLimit ) << endl;
    // AdjustWindowBoundary( q, Q, s, S, tw ); // to handle cases like 1S35M or 17S18M or 5M2R9M1R15M2S for -n1 -w16 -f16 -N2
    // return m_penalty >= m_penaltyLimit;
}

void CAligner::AdjustWindowBoundary( const char * & q, const char * & Q, const char * & s, const char * & S, TTranscript& tw )
{
    //bool reverse = (m_qryInc * m_sbjInc) < 1;
    //ASSERT( m_scoreParam->GetMismatchPenalty() > 0 );
    while( tw.front().GetEvent() == CTrBase::eEvent_Replaced ) {
        int k = tw.front().GetCount();
        //if( !reverse ) q += m_qryInc * k; else Q -= m_qryInc * k;
        Q -= m_qryInc * k;
        S -= m_sbjInc * k; 
        m_penalty += k * (m_scoreParam->GetIdentityScore() + m_scoreParam->GetMismatchPenalty()); 
        tw.erase( tw.begin(), tw.begin() + 1 );
    }

    while( tw.back().GetEvent() == CTrBase::eEvent_Replaced ) {
        int k = tw.back().GetCount();
        //if(  reverse ) q += m_qryInc * k; else Q -= m_qryInc * k;
        q += m_qryInc * k; 
        s += m_sbjInc * k; 
        m_penalty += k * (m_scoreParam->GetIdentityScore() + m_scoreParam->GetMismatchPenalty()); 
        tw.resize( tw.size() - 1 );
    }
}
}
/*
void CAligner::AdjustWindowBoundary( const char * & q, const char * & Q, const char * & s, const char * & S, TTranscript& tw ) const
{
    bool reverse = (m_sbjInc*m_qryInc) < 0;

    while( tw.front().first != CCell::eEqu ) {
        int k = tw.front().second;
        switch( tw.front().first ) {
            case CCell::eNULL: THROW( logic_error, "Can't have null here!" );
            case CCell::eMis: Q -= m_qryInc * k; S -= m_sbjInc * k; m_penalty -= k*m_mismatchPenalty; break;
            case CCell::eIns: case CCell::eOvl: Q -= m_qryInc * k; break;
            case CCell::eDel: case CCell::eInt: S -= m_sbjInc * k; break;
        }
        tw.erase( tw.begin() );
    }

    while( tw.back().first != CCell::eEqu ) {
        int k = tw.back().second;
        switch( tw.back().first ) {
            case CCell::eNULL: THROW( logic_error, "Can't have null here!" );
            case CCell::eMis: q += m_qryInc * k; s += m_sbjInc * k; break;
            case CCell::eIns: case CCell::eOvl: q += m_qryInc * k; break;
            case CCell::eDel: case CCell::eInt: s += m_sbjInc * k; break;
        }
        tw.resize( tw.size() - 1 );
    }

}
*/
