#ifndef OLIGOFAR_TALIGNER_FAST__HPP
#define OLIGOFAR_TALIGNER_FAST__HPP

#include "talignerbase.hpp"

BEGIN_OLIGOFAR_SCOPES

template<class CQuery,class CSubject>
class TAligner_fast : public TAlignerBase<CQuery,CSubject>
{
public:
    typedef TAlignerBase<CQuery,CSubject> TSuper;
    typedef TAligner_fast<CQuery,CSubject> TSelf;

    TAligner_fast(int = 0) {}

	const char * GetName() const { return "Simple dynamic aligner"; }
    void Align( int flags );
protected:
	void DoIdentity( CQuery& q, CSubject& s );
	void DoMismatch( CQuery& q, CSubject& s, int cnt = 1 );
	void DoInsertion( CQuery& q, CSubject& s, int cnt = 1 );
	void DoDeletion( CQuery& q, CSubject& s, int cnt = 1 );
	void DoFlip( CQuery& q, CSubject& s ) { DoMismatch( q, s, 1 ); } // to handle cases like CS/GC

	// count number of periods in simple repeats of length 1 and 2
	template<class CSeq> 
	int Count1( const CSeq& b, const CSeq& e ) const;
	template<class CSeq>
	int Count2( const CSeq& b, const CSeq& e ) const;
protected:
	int m_flags;
	bool m_inGap;
};

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::Align( int flags )
{
	m_flags = flags;
	m_inGap = false;

	const CQuery Q = TSuper::GetQueryEnd();
	const CSubject S = TSuper::GetSubjectEnd();

	// NOT_TRUE: following vars are modified by Do* functions!!!
    CQuery q = TSuper::m_query;
    CSubject s = TSuper::m_subject;

	while( q < Q && s < S ) {
		if( Match( q, s ) ) { DoIdentity( q, s ); continue; }
		if( q + 1 < Q && s + 1 < S && Match( q + 1, s + 1 ) ) {
			if( Match( q + 1, s ) ) {
				int qc = Count1( q + 1, Q );
				int sc = Count1( s, S );
//                int cc = min( qc, sc );
				if( qc < sc ) { // || ! Match( q + cc + 1, s + cc ) ) {
					DoMismatch( q, s );
					DoIdentity( q, s );
				} else {
					DoInsertion( q, s );
					DoIdentity( q, s );
				}
			} else if( Match( q, s + 1 ) ) {
				int qc = Count1( q, Q );
				int sc = Count1( s + 1, S );
//                int cc = min( qc, sc );
				if( qc > sc ) { // || ! Match( q + cc, s + cc + 1 ) ) {
					DoMismatch( q, s );
					DoIdentity( q, s );
				} else {
					DoDeletion( q, s );
					DoIdentity( q, s );
				}
			} else {
				DoMismatch( q, s );
				DoIdentity( q, s );
			}
		} else if( s + 1 < S && Match( q, s + 1 ) ) {
			if( q + 1 < Q ) {
				if( Match( q + 1, s ) ) {
					int qc = Count2( q, Q );
					int sc = Count2( s, S );
					if( qc > 0 && sc > 0 ) {
						if( qc < sc ) DoDeletion( q, s );
						else DoInsertion( q, s );
					} else {
						DoFlip( q, s );
					}
				} else {
					DoDeletion( q, s );
				}
			} else {
				DoMismatch( q, s );
			}
		} else if( q + 1 < Q && Match( q + 1, s ) ) {
			DoInsertion( q, s );
		} else if( s + 2 < S && Match( q, s + 2 ) ) {
			DoDeletion( q, s, 2 );
		} else if( q + 2 < Q && Match( q + 2, s ) ) {
			DoInsertion( q, s, 2 );
		} else {
			DoMismatch( q, s );
		}
	}

    TSuper::m_query = q;
    TSuper::m_subject = s;

    TSuper::SetQueryAligned() = TSuper::m_query - TSuper::GetQueryBegin();
    TSuper::SetSubjectAligned() = TSuper::m_subject - TSuper::GetSubjectBegin();
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoIdentity( CQuery& q, CSubject& s )
{
	if( m_flags & CAlignerBase::fComputePicture ) {
        char qb = TSuper::GetIupacnaQuery( q, m_flags );
        char sb = TSuper::GetIupacnaSubject( s, m_flags );
		TSuper::SetQueryString() += qb;
        TSuper::SetSubjectString() += sb;
		TSuper::SetAlignmentString() += qb == sb ? '|' : ':';
	}
	if( m_flags & CAlignerBase::fComputeScore ) {
		TSuper::SetScore() += Score( q, s );
		TSuper::SetIdentities() ++;
	}
	++q;
	++s;
	m_inGap = false;
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoMismatch( CQuery& q, CSubject& s, int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += TSuper::GetIupacnaQuery( q, m_flags );
			TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( s, m_flags );
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += Score( q, s );
			TSuper::SetMismatches() ++;
		}
		++q;
		++s;
	}
	m_inGap = false;
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoInsertion( CQuery& q, CSubject& s, int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += TSuper::GetIupacnaQuery( q, m_flags );
			TSuper::SetSubjectString() += '-';
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += m_inGap ?
				  TSuper::GetAlignerBase().GetScoreTbl().GetGapExtentionScore() :
				  ( (m_inGap = true), TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetInsertions() ++;
		}
		++q;
	}
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoDeletion( CQuery& q, CSubject& s, int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += '-';
			TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( s, m_flags );
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += m_inGap ?
				  TSuper::GetAlignerBase().GetScoreTbl().GetGapExtentionScore() :
				  ( (m_inGap = true), TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetDeletions() ++;
		}
		++s;
	}
}

template<class CQuery,class CSubject>
template<class CSeq>
inline int TAligner_fast<CQuery,CSubject>::Count1( const CSeq& a, const CSeq& b ) const
{
    int k = 1;
    for( CSeq x = a + 1; x < b && Match( x, x - 1 ) && Match( x, a ); ++x, ++k );
    return k;
}

template<class CQuery,class CSubject>
template<class CSeq>
inline int TAligner_fast<CQuery,CSubject>::Count2( const CSeq& a, const CSeq& b ) const
{
	int ret = 0;
	for( CSeq x = a + 2; x < b; ++x, ++ret ) if( !Match( x, x - 2) ) break;
	return ret;
}

END_OLIGOFAR_SCOPES

#endif
