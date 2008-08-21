#ifndef OLIGOFAR_CSCORETBL__HPP
#define OLIGOFAR_CSCORETBL__HPP

#include "cscoring.hpp"

BEGIN_OLIGOFAR_SCOPES

class CScoreTbl : public CScoring
{
public:
	CScoreTbl() { x_InitScoretbl(); }
	CScoreTbl( double id, double mm, double go, double ge ) : CScoring( id, mm, go, ge ) { x_InitScoretbl(); }

	////////////////////////////////////////////////////////////////////////	
	// TSeqRef<C*naBase,incr,CSeqCoding::eCoding_*na> API
	template<class QryRef, class SbjRef>
	static bool MatchRef( const QryRef& q, const SbjRef& s ) {
		return ( CNcbi8naBase( q.GetBase() ) & CNcbi8naBase( s.GetBase() ) ) != 0;
	}
	
	template<class QryRef, class SbjRef>
	double ScoreRef( const QryRef& q, const SbjRef& s ) const {
		CNcbi8naBase sbj( s.GetBase() );
		// q.GetCoding() is constant, so optimizer hopefuly should exclude one of branches
		if( q.GetCoding() == CSeqCoding::eCoding_ncbipna ) { 
			CNcbipnaBase qry( q.GetBase() );
       		unsigned sc = max( max( qry[0] * ((sbj&1)>>0), 
									qry[1] * ((sbj&2)>>1) ),
                	           max( qry[2] * ((sbj&4)>>2), 
								   	qry[3] * ((sbj&8)>>3) ) );
        	return ProbScore( double(sc)/qry[4]*s_probtbl[(int)sbj] );
		} else {
			CNcbi8naBase qry( q.GetBase() );
			return qry & sbj ? m_scoretbl[int( sbj )] : m_mismatch;
		}
	}

	////////////////////////////////////////////////////////////////////////	
	// CSeqRef API
#if 0
    template<class CQueryBase, class CSubjectBase> 
    static bool Match( const CSeqRef& query, const CSeqRef& subj ) {
        return CNcbi8naBase( query.GetBase<CQueryBase>() ) & CNcbi8naBase( subj.GetBase<CSubjectBase>() );
    }
    template<class CQueryBase, class CSubjectBase> 
    double Score( const CSeqRef& query, const CSeqRef& subj ) const {
        return ScoreBase( query.GetBase<CQueryBase>(), subj.GetBase<CSubjectBase>() );
    }

	template<class CQueryBase, class CSubjectBase>
	double ScoreBase( const CQueryBase& q, const CSubjectBase& s ) const {
		CNcbi8naBase qry( q );
		CNcbi8naBase sbj( s );
		return qry & sbj ? m_scoretbl[int( sbj )] : m_mismatch;
	}

	template<class CSubjectBase>
	double ScoreBase( const CNcbipnaBase& q, const CSubjectBase& s ) const {
		CNcbi8naBase subj( s );
        unsigned sc = max( max( q[0] * ((subj&1)>>0), 
								q[1] * ((subj&2)>>1) ),
                           max( q[2] * ((subj&4)>>2), 
							   	q[3] * ((subj&8)>>3) ) );
        return ProbScore( double(sc)/q[4]*s_probtbl[(int)subj] );
	}
#endif
    double ProbScore( double prob ) const {
        return m_identity * prob + m_mismatch * (1 - prob);
    }
protected:
    double m_scoretbl[16];
protected:
    static double * x_InitProbTbl();
    void x_InitScoretbl() {
        if( s_probtbl == 0 ) s_probtbl = x_InitProbTbl();
        m_scoretbl[0] = m_gapOpening;
        for( int i = 1; i < 16; ++i ) {
            m_scoretbl[i] = ProbScore( s_probtbl[i] );
        }
    }
    static double * s_probtbl;
};

END_OLIGOFAR_SCOPES

#endif
