#ifndef OLIGOFAR_CSCORETBL__HPP
#define OLIGOFAR_CSCORETBL__HPP

#include "cscoring.hpp"
#include "tseqref.hpp"

BEGIN_OLIGOFAR_SCOPES

class CScoreTbl : public CScoring
{
public:
    CScoreTbl() { x_InitScoretbl(); }
    CScoreTbl( double id, double mm, double go, double ge ) : CScoring( id, mm, go, ge ), m_ncbi4naSelector(0) { x_InitScoretbl(); }

    ////////////////////////////////////////////////////////////////////////    
    // TSeqRef<C*naBase,incr,CSeqCoding::eCoding_*na> API
//  template<class QryRef, class SbjRef>
//  static bool MatchRef( const QryRef& q, const SbjRef& s ) {
//          if( q.GetCoding() == CSeqCoding::eCoding_colorsp ) 
//              return CColorTwoBase( char(q.GetBase()) ).GetColor() == CColorTwoBase( char(s.GetBase()) ).GetColor();
//          else
//              return ( CNcbi8naBase( q.GetBase() ) & CNcbi8naBase( s.GetBase() ) ) != 0;
//  }
    
    // Note: in following functions most of ifs are for constant arguments, so optimizer should ideally elminate them...

    /*
    template<class SbjRef>
    static CNcbi8naBase GetUncoloredBase( const SbjRef& s ) {
        return s.IsReverse() ? CNcbi8naBase((s - 1).GetBase() & 0x0f) : CNcbi8naBase(char(*s) & 0x0f);
    }
    */
    enum ETableSelector {
        eSel_NoConv = 0,
        eSel_ConvTC = 1,
        eSel_ConvAG = 2
    };
    void SelectBasicScoreTables( unsigned tbl ) { ASSERT( tbl < 3 ); m_ncbi4naSelector = tbl; }
    int  GetBasicScoreTables() const { return m_ncbi4naSelector; }

    template<class QryRef, class SbjRef>
    bool MatchRef( const QryRef& q, const SbjRef& s, int qp, int sp, ETableSelector sel ) const {
        if( q.GetCoding() == CSeqCoding::eCoding_colorsp ) 
            if( s.GetCoding() == CSeqCoding::eCoding_colorsp ) 
                return CColorTwoBase( char(*q) ).GetColor() == CColorTwoBase( char(*s) ).GetColor();
            else 
                if( CNcbi8naBase q4 = CColorTwoBase( q.GetBase() ).GetBaseCalls() ) { // first base of the query was IUPACna, so we know it, rest are only colors
                    if( q.IsReverse() ) q4 = q4.Complement();
                    /*char qb = CIupacnaBase( q4 );
                    char sb = CIupacnaBase( s.GetBase() );
                    cerr << DISPLAY( q.IsReverse() ) << DISPLAY( qp ) << DISPLAY( sp ) << DISPLAY( qb ) << DISPLAY( sb ) << endl;*/
                    return (q4 & CNcbi8naBase( s.GetBase() )) != 0;
                } else {
                    if( !q.IsReverse() ) 
                        return CColorTwoBase( char(*q) ).GetColor() == CColorTwoBase( s[-1], s[0] ).GetColor(); 
                    else
                        return CColorTwoBase( char(*q) ).GetColor() == CColorTwoBase( s[0], s[1] ).GetColor(); 
                }
        else {
            return m_ncbi4naMatchTbl[sel][CNcbi8naBase( q.GetBase() )][CNcbi8naBase( s.GetBase() )];
//            return ( CNcbi8naBase( q.GetBase() ) & CNcbi8naBase( s.GetBase() ) ) != 0;
        }
    }
    
    template<class QryRef, class SbjRef>
    bool MarkSameRef( const QryRef& q, const SbjRef& s, int qp, int qs ) const {
        return MatchRef( q, s, qp, qs, eSel_NoConv );
    }
    
    template<class QryRef, class SbjRef>
    bool MatchRef( const QryRef& q, const SbjRef& s, int qp, int sp ) const {
        return MatchRef( q, s, qp, sp, ETableSelector(m_ncbi4naSelector) );
    }
    
    template<class QryRef, class SbjRef>
    double ScoreRef( const QryRef& q, const SbjRef& s, int qp, int sp ) const {
        CNcbi8naBase sbj( s.GetBase() );
        switch( q.GetCoding() ) {
        case CSeqCoding::eCoding_ncbipna: 
            do {
                CNcbipnaBase qry( q.GetBase() );
                unsigned sc = max( max( qry[0] * ((sbj&1)>>0), 
                                        qry[1] * ((sbj&2)>>1) ),
                                   max( qry[2] * ((sbj&4)>>2), 
                                        qry[3] * ((sbj&8)>>3) ) );
                return ProbScore( double(sc)/qry[4] * s_probtbl[(int)sbj] );
            } while(0); break;
        case CSeqCoding::eCoding_ncbiqna: 
            do {
                CNcbiqnaBase qry( q.GetBase() );
                return m_ncbi4naMatchTbl[m_ncbi4naSelector][CNcbi8naBase( qry )][sbj] ?
                    ProbScore( s_phrapProbTbl[qry.GetPhrapScore()] * s_probtbl[sbj] ) : m_mismatch;
//                return CNcbi8naBase( qry ) & sbj ? ProbScore( s_phrapProbTbl[qry.GetPhrapScore()] * s_probtbl[sbj] ) : m_mismatch;
//              return CNcbi8naBase( qry ) & sbj ? m_phraptbl[sbj][qry.GetPhrapScore()] : m_mismatch;
            } while(0); break;
        case CSeqCoding::eCoding_colorsp: 
            do {
                return MatchRef( q, s, qp, sp ) ? m_identity : m_mismatch;
            } while(0); break;
        default:
            do {
                CNcbi8naBase qry( q.GetBase() );
                return m_ncbi4naScoreTbl[m_ncbi4naSelector][qry][sbj];
//                return qry & sbj ? m_scoretbl[int( sbj )] : m_mismatch;
            } while(0); break;
        }
    }

    double ProbScore( double prob ) const {
        return m_identity * prob + m_mismatch * (1 - prob);
    }
protected:
    double m_scoretbl[16];
    double m_phraptbl[16][64];
    double m_ncbi4naScoreTbl[3][16][16];
    bool   m_ncbi4naMatchTbl[3][16][16];
    int    m_ncbi4naSelector;
protected:
    static double * x_InitProbTbl();
    void x_InitScoretbl() {
        if( s_probtbl == 0 ) s_probtbl = x_InitProbTbl();
        m_scoretbl[0] = m_gapOpening;
        for( int i = 1; i < 16; ++i ) {
            m_scoretbl[i] = ProbScore( s_probtbl[i] );
        }
        for( int i = 0; i < 16; ++i ) {
            for( int j = 0; j < 64; ++j ) 
                m_phraptbl[i][j] = ProbScore( s_phrapProbTbl[j] * s_probtbl[i] );
        }
        x_InitNcbi4naTables( eSel_NoConv, 0,  0 );
        x_InitNcbi4naTables( eSel_ConvTC, 8, +2 );
        x_InitNcbi4naTables( eSel_ConvAG, 1, -2 );
    }
    static Uint1 x_Convert( Uint1 x, Uint1 mask, int shr ) {
        return 
            shr > 0 ? ( x /*& ~mask*/ ) | ((x & mask) >> (+shr)) :
            shr < 0 ? ( x /*& ~mask*/ ) | ((x & mask) << (-shr)) :
            x;
    }
    void x_InitNcbi4naTables( int which, Uint1 mask, int shr ) {
        for( unsigned a = 0; a < 16; ++a ) {
            unsigned A = x_Convert( a, mask, shr );
            for( unsigned b = 0; b < 16; ++b ) {
//                unsigned B = x_Convert( b, mask, shr );
                unsigned B = b; // genome is unchanged!
                m_ncbi4naMatchTbl[which][a][b] = (A&B) ? true : false;
                m_ncbi4naScoreTbl[which][a][b] = (A&B) ? m_scoretbl[int( B )] : m_mismatch;
            }
        }
    }
    static double * s_probtbl;
    static double s_phrapProbTbl[];
};

END_OLIGOFAR_SCOPES

#endif
