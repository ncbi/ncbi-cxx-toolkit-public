#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"

USING_OLIGOFAR_SCOPES;

/*
 * Globally above statements should work. Question is how should they be updated on each step?
 */

inline void CAligner::AdjustMatrixSize()
{
    int diags = max( 
            max( m_alignParam->GetMaxInsertionLength(), m_alignParam->GetMaxDeletionLength() ),
            max( m_alignParam->GetMaxInsertionsCount(), m_alignParam->GetMaxDeletionsCount() ) );

    m_matrixOffset = - (m_alignParam->GetMaxSpliceOverlap() + 1);
    int needSize = m_qryLength + diags + m_alignParam->GetMaxSplicesSize() - m_matrixOffset + diags * GetSpliceCount();
    if( needSize >= m_matrixSize ) {
        m_matrixSize = needSize + 1;
        m_matrix.resize( m_matrixSize*m_matrixSize );
    }
}

bool CAligner::Align()
{
    ////////////////////////////////////////////////////////////////////////
    // It is essential that all gaps within anchor zone are penalized, 
    // since this penalty is used also in filter and at least now I
    // don't want to put this logic in multiple places
    s_maxPenalty[m_penaltyLimit]++;
    m_transcript.clear();
    m_posQ1 = m_posQ2 = m_posS1 = m_posS2 = -1;
    m_penalty = 0;
    m_qryInc = m_qryCoding == CSeqCoding::eCoding_ncbipna ? 5 : 1;

    m_scoringFactory->SetQueryCoding( m_qryCoding );
    m_scoringFactory->SetSubjectStrand( m_sbjStrand ); 
    // the third parameter (convertion table) will be set from outside
    m_score = m_scoringFactory->GetScoreMethod();
    ASSERT( m_score != 0 );

    m_scoreParam = m_score->GetScoreParam();
    ASSERT( m_scoreParam );

    ASSERT( m_sbjCoding == CSeqCoding::eCoding_ncbi8na );
    ASSERT( m_qryStrand == CSeqCoding::eStrand_pos );

    bool reverseStrand = m_qryStrand != m_sbjStrand;

    if( m_anchorQ1 > m_anchorQ2 ) swap( m_anchorQ1, m_anchorQ2 );
    if( (m_anchorS1 > m_anchorS2) ^ reverseStrand ) swap( m_anchorS1, m_anchorS2 );

    m_sbjInc = reverseStrand ? -1 : 1;

    ////////////////////////////////////////////////////////////////////////
    // NB: m_qryInc should be always positive, abs(m_sbjInc) should be always 1 
    ASSERT( m_qryInc > 0 && abs( m_sbjInc ) == 1 );
    const char * q = m_qry + m_anchorQ1*m_qryInc;
    const char * Q = m_qry + m_anchorQ2*m_qryInc;
    const char * s = m_sbj + m_anchorS1;
    const char * S = m_sbj + m_anchorS2;

    ++m_totalCalls;
    m_queryBases += m_qryLength;

    ////////////////////////////////////////////////////////////////////////
    // Don't forget this:
    m_alignParam = m_scoringFactory->GetAlignParam();
    ASSERT( m_alignParam );

    ////////////////////////////////////////////////////////////////////////
    // If indels are not allowed, we use much simpler and faster approach
    if( NoSplicesDefined() && // AlignDiag does not work with splices
            m_penaltyLimit >= m_extentionPenaltyDropoff && // AlignDiag does not work with limited extension
            !CMaxGapLengths( -m_penaltyLimit, m_scoreParam, m_alignParam ).AreIndelsAllowed() ) // and obviously can't do indels
        return ( (q - Q)*m_sbjInc == (s - S)*m_qryInc ) && AlignDiag( q, Q, s, S, reverseStrand );

    // allocate memory for matrix
    AdjustMatrixSize();

    ////////////////////////////////////////////////////////////////////////
    // Note, that without adjusting window as above code there still may be 
    // aberrations on the edge of the window, so it is not recommended to 
    // use -j2 with -n0 (even with -E2 it is OK, since -n1 hash seed will 
    // initiate better alignment). Attempt to use window adjustment code 
    // will make this more expensive and less reliable.
    
    const char * firstMatchQ = q;
    const char * firstMatchS = s;
    const char * lastMatchQ = Q;
    const char * lastMatchS = S;

    ////////////////////////////////////////////////////////////////////////
    // Align window part - may be pretty tricky if contains splices
    TTranscript tm;
    double id = m_scoreParam->GetIdentityScore();
    double go = m_scoreParam->GetGapBasePenalty();
    double ge = m_scoreParam->GetGapExtentionPenalty();
    if( !m_guideTranscript.size() ) {
        if( ! AlignWindow( firstMatchQ, lastMatchQ, firstMatchS, lastMatchS, tm ) ) return false;
    } else {
        /// Guided alignment
        const char * qq = reverseStrand ? Q : q, * ss = reverseStrand ? S : s;
        int si = abs( m_sbjInc );// transcript is in reverse coord
        int qi = m_qryInc * m_sbjInc / si;
        ASSERT( si > 0 );
        ASSERT( reverseStrand ? qi < 0 : ( qi > 0 ) );
        tm.reserve( m_guideTranscript.size() );
        ITERATE( TTranscript, t, m_guideTranscript ) {
            //cerr << DISPLAY( Event2Char( t->GetEvent() ) ) << DISPLAY( t->GetCount() ) << "\n";
            switch( t->GetEvent() ) {
                case eEvent_Changed: 
                case eEvent_Match: 
                case eEvent_Replaced:
                    for( int i = 0; i < t->GetCount(); ++i ) {
                        /*
                        cerr 
                            << DISPLAY( (qq - m_qry) ) << DISPLAY( (ss - m_sbj) )
                            << DISPLAY( (q - m_qry) ) << DISPLAY( (s - m_sbj) )
                            << DISPLAY( (Q - m_qry) ) << DISPLAY( (S - m_sbj) );
                            */
                        double sc = Score( qq, ss );
                        m_penalty += sc - id; 
                        /*
                        cerr 
                            << DISPLAY( CIupacnaBase( CNcbi8naBase( qq ) ) )
                            << DISPLAY( CIupacnaBase( CNcbi8naBase( ss ) ) )
                            << "\n";
                            */
                        qq += qi; ss += si;
                        tm.AppendItem( sc > 0 ? eEvent_Match : eEvent_Replaced );
                    }
                    break;
                case eEvent_Insertion:
                    qq += (int)t->GetCount() * qi;
                    m_penalty -= go + t->GetCount() * (id + ge);
                    tm.AppendItem( *t );
                    break;
                case eEvent_Deletion:
                    ss += (int)t->GetCount() * si;
                    m_penalty -= go + t->GetCount() * ge;
                    tm.AppendItem( *t );
                    break;
                case eEvent_Splice:
                    ss += (int)t->GetCount() * si;
                    tm.AppendItem( *t );
                    break;
                case eEvent_Overlap:
                    ss -= (int)t->GetCount() * si;
                    tm.AppendItem( *t );
                    break;
                default:;
            }
            if( m_penalty < m_penaltyLimit ) return false;
        }
    }

    // string tms( tm.ToString() );
    ASSERT( tm.size() );
    if( tm.size() ) {
        TTranscript::iterator t = tm.begin();
        bool run = true;
        while( run ) {
            switch( t->GetEvent() ) {
            case eEvent_Replaced: 
                m_penalty += id - Score( lastMatchQ, lastMatchS );
                lastMatchQ -= m_qryInc;
                lastMatchS -= m_sbjInc;
                t->DecrCount();
                break;
            default: run = false; break;
            }
            if( t->GetCount() == 0 ) { run &= ( ++t != tm.end() ); }
        }
        tm.erase( tm.begin(), t );
    } 

    if( tm.size() ) {
        TTranscript::reverse_iterator t = tm.rbegin();
        bool run = true;
        while( run ) {
            switch( t->GetEvent() ) {
            case eEvent_Replaced: 
                m_penalty += id - Score( firstMatchQ, firstMatchS );
                firstMatchQ += m_qryInc;
                firstMatchS += m_sbjInc;
                t->DecrCount();
                break;
            default: run = false; break;
            }
            if( t->GetCount() == 0 ) { run &= ( ++t != tm.rend() ); }
        }
        tm.resize( tm.size() - (t - tm.rbegin()) );
        //tm.erase( tm.rbegin(), t );
    }

    ////////////////////////////////////////////////////////////////////////
    // Following three lines are necessary to make corrections to score if deletions are adjacent
    // in different parts of alignments (there are no problems found with insertions)
    double deltaPf = tm.size() > 0 && tm.front().GetEvent() == eEvent_Deletion ? go : 0;
    double deltaPb = tm.size() > 1 && tm.back ().GetEvent() == eEvent_Deletion ? go : 0;
    m_penalty += deltaPb + deltaPf; // to avoid extra deletion of hits

    ////////////////////////////////////////////////////////////////////////
    // After all score adjustments check if we have to proceed
    if( m_penalty < m_penaltyLimit ) return false;

    ////////////////////////////////////////////////////////////////////////
    // Align floppy parts
    TTranscript th;
    int head = (firstMatchQ - m_qry)/m_qryInc; 
    if( head > 0 && ! AlignTail( head, -1, firstMatchQ, firstMatchS, m_posQ1, m_posS1, th, head ) ) return false;
    if( deltaPb && th.size() && th.back().GetEvent() == eEvent_Deletion ) {} else m_penalty -= deltaPb;

    TTranscript tt;
    int pos2 = (lastMatchQ - m_qry)/m_qryInc;
    int tail = m_qryLength - (lastMatchQ - m_qry)/m_qryInc - 1;
    if( tail > 0 && ! AlignTail( tail, +1, lastMatchQ,  lastMatchS,  m_posQ2, m_posS2, tt, pos2 ) ) return false;
    if( deltaPf && tt.size() && tt.back().GetEvent() == eEvent_Deletion ) {} else m_penalty -= deltaPf;

    ////////////////////////////////////////////////////////////////////////
    // We are cool, just need to compute end points and combine transcripts
    int qlen = m_qryLength;

    m_posQ1 = head - m_posQ1 - 1;
    m_posQ2 = qlen - tail + m_posQ2 + 1;
    if( reverseStrand ) {
        int s1 = m_posS1;
        m_posS1 = lastMatchS - m_sbj - m_posS2 - 1;
        m_posS2 = firstMatchS - m_sbj + s1 + 1;
        // s2 > s1
    } else {
        m_posS1 = firstMatchS - m_sbj - m_posS1 - 1;
        m_posS2 = lastMatchS - m_sbj + m_posS2 + 1;
        // s2 > s1
    }

    /*
    cerr << "Q: "; PrintQ( cerr, q, Q + m_qryInc ); cerr << "\n";
    cerr << "S: "; PrintS( cerr, s, S + m_sbjInc ); cerr << "\n";
    cerr << DISPLAY( th.ToString() ) << DISPLAY( tm.ToString() ) << DISPLAY( tt.ToString() ) << "\n";
    cerr << DISPLAY( m_anchorQ1 ) << DISPLAY( m_posQ1 ) << DISPLAY( m_anchorQ2 ) << DISPLAY( m_posQ2 ) << "\n" 
        << DISPLAY( m_anchorS1 ) << DISPLAY( m_posS1 ) << DISPLAY( m_anchorS2 ) << DISPLAY( m_posS2 ) << "\n";
    */
    m_transcript.reserve( tm.size() + th.size() + tt.size() );
    if( reverseStrand ) {
        AppendTranscript( tt.begin(),  tt.end()  );
        AppendTranscript( tm.begin(),  tm.end()  );
        AppendTranscript( th.rbegin(), th.rend() );
    } else {
        AppendTranscript( th.begin(),  th.end()  );
        AppendTranscript( tm.rbegin(), tm.rend() );
        AppendTranscript( tt.rbegin(), tt.rend() );
    }
    ASSERT( m_transcript.size() );

    if( m_qryCoding == CSeqCoding::eCoding_colorsp ) {
        if( m_scoringFactory->GetConvertionTable() != CScoringFactory::eNoConv ) {
            x_ColorspaceUpdatePenalty();
        }
    }
    
    ++m_successAligns;
    return m_penalty >= m_penaltyLimit;
}

void CAligner::x_ColorspaceUpdatePenalty() 
{
    int qstep = CColorTwoBase::BytesPerBase();
    int sstep = 1;
    const char * s = m_sbj + m_posS1;
    const char * q = m_qry;

    TTranscript tt( m_transcript );
    if( m_sbjStrand == CSeqCoding::eStrand_neg ) {
        tt.Reverse();
        sstep = -1;
        s += tt.ComputeSubjectLength() - 1;
    }

    CScoringFactory::EConvertionTable ct = m_scoringFactory->GetConvertionTable();

    double chg = 0;
    double delta = (m_scoreParam->GetIdentityScore() + m_scoreParam->GetMismatchPenalty());
   
    // we have to translate first, since even if we have match we may want to indicate difference
    // and we may want to merge adjacent differences to a single output
    CNcbi8naBase lastBase( '\x0' );
    bool transcriptChanged = false;
    ITERATE( TTranscript, it, tt ) {
        int count = it->GetCount(); 
        ASSERT( count > 0 );
        CTrBase::EEvent ev = it->GetEvent();
        bool dontAdd = false;
        switch( ev ) {
        case CTrBase::eEvent_Overlap: 
        case CTrBase::eEvent_SoftMask: 
        case CTrBase::eEvent_HardMask: q += qstep * count; break;
        case CTrBase::eEvent_Splice:   s += sstep * count; break; // just cut out intron
        case CTrBase::eEvent_Padding:  break; // ignore padding
        case CTrBase::eEvent_Insertion:
                  for( int i = 0; i < count; ++i, (q += qstep) ) {
                      CColorTwoBase c2b( *q );
                      CNcbi8naBase bc( c2b.GetBaseCalls() );
                      lastBase = bc ? bc : CNcbi8naBase( lastBase, c2b );
                  }
                  break;
        case CTrBase::eEvent_Deletion: s += sstep * count; break;
        case CTrBase::eEvent_Match: 
        case CTrBase::eEvent_Changed: 
        case CTrBase::eEvent_Replaced: 
                  for( int i = 0, used = 0; i < count; ++i, (s += sstep), (q += qstep) ) {
                      CNcbi8naBase sb( s );
                      if( it->GetEvent() == CTrBase::eEvent_Match ) {
                          CColorTwoBase c2b( q );
                          lastBase = sb;
                      } else {
                          if( CNcbi8naBase bc = CNcbi8naBase( CColorTwoBase( q ).GetBaseCalls() ) ) {
                              lastBase = bc.Get( m_sbjStrand );
                          } else {
                              CColorTwoBase c2b( q );
                              CNcbi8naBase bc( lastBase, c2b );
                              lastBase = bc;
                          }
                          if( count >= 1 && sb != lastBase ) {
                              bool changed = false;
                              switch( ct ) {
                              case CScoringFactory::eConvTC: 
                                  changed = ( sb == CNcbi8naBase::fBase_C && lastBase == CNcbi8naBase::fBase_T ); break;
                              case CScoringFactory::eConvAG:
                                  changed = ( sb == CNcbi8naBase::fBase_G && lastBase == CNcbi8naBase::fBase_A ); break;
                              default:;
                              }
                              if( changed ) {
                                  chg += delta;
                                  if( !used++ ) chg += delta;
                                  if( !transcriptChanged ) {
                                      m_transcript.clear();
                                      m_transcript.Append( TTranscript::const_iterator( tt.begin() ), it );
                                      transcriptChanged = true;
                                  }
                                  ev = CTrBase::eEvent_Changed;
                              }
                          }
                      }
                      if( transcriptChanged ) {
                          m_transcript.AppendItem( ev, 1 );
                          dontAdd = true;
                      }
                  }
            break;
        default: THROW( logic_error, "Invalid event [" << it->GetCigar() << "] in transcript " << tt.ToString() );
        }
        if( transcriptChanged && ! dontAdd ) m_transcript.AppendItem( ev, count );
    }
    m_penalty += chg;
    if( m_sbjStrand == CSeqCoding::eStrand_neg && transcriptChanged ) m_transcript.Reverse();
}
