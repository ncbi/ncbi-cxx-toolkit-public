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
	void DoIdentity();
	void DoMismatch( int cnt = 1 );
	void DoInsertion( int cnt = 1 );
	void DoDeletion( int cnt = 1 );
	void DoFlip() { DoMismatch( 1 ); } // to handle cases like CS/GC

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

	// following vars are modified by Do* functions!!!
    CQuery& q = TSuper::m_query;
    CSubject& s = TSuper::m_subject;

	while( q < Q && s < S ) {
		if( Match( q, s ) ) { DoIdentity(); continue; }
		if( q + 1 < Q && s + 1 < S && Match( q + 1, s + 1 ) ) {
			if( Match( q + 1, s ) ) {
				int qc = Count1( q + 1, Q );
				int sc = Count1( s, S );
                int cc = min( qc, sc );
				if( qc < sc || ! Match( q + 1 + cc, s + cc ) ) {
					DoMismatch();
					DoIdentity();
				} else {
					DoInsertion();
					DoIdentity();
				}
			} else if( Match( q, s + 1 ) ) {
				int qc = Count1( q, Q );
				int sc = Count1( s + 1, S );
                int cc = min( qc, sc );
				if( qc > sc || ! Match( q + cc, s + 1 + cc ) ) {
					DoMismatch();
					DoIdentity();
				} else {
					DoDeletion();
					DoIdentity();
				}
			} else {
				DoMismatch();
				DoIdentity();
			}
		} else if( s + 1 < S && Match( q, s + 1 ) ) {
			if( q + 1 < Q ) {
				if( Match( q + 1, s ) ) {
					int qc = Count2( q, Q );
					int sc = Count2( s, S );
					if( qc > 0 && sc > 0 ) {
						if( qc < sc ) DoDeletion();
						else DoInsertion();
					} else {
						DoFlip();
					}
				} else {
					DoDeletion();
				}
			} else {
				DoMismatch();
			}
		} else if( q + 1 < Q && Match( q + 1, s ) ) {
			DoInsertion();
		} else if( s + 2 < S && Match( q, s + 2 ) ) {
			DoDeletion( 2 );
		} else if( q + 2 < Q && Match( q + 2, s ) ) {
			DoInsertion( 2 );
		} else {
			DoMismatch();
		}
	}

//     TSuper::m_query = q;
//     TSuper::m_subject = s;

    TSuper::SetQueryAligned() = TSuper::m_query - TSuper::GetQueryBegin();
    TSuper::SetSubjectAligned() = TSuper::m_subject - TSuper::GetSubjectBegin();
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoIdentity()
{
	if( m_flags & CAlignerBase::fComputePicture ) {
		TSuper::SetQueryString() += TSuper::GetIupacnaQuery( m_flags );
		TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( m_flags );
		TSuper::SetAlignmentString() += '|';
	}
	if( m_flags & CAlignerBase::fComputeScore ) {
		TSuper::SetScore() += Score( TSuper::m_query, TSuper::m_subject );
		TSuper::SetIdentities() ++;
	}
	++TSuper::m_query;
	++TSuper::m_subject;
	m_inGap = false;
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoMismatch( int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += TSuper::GetIupacnaQuery( m_flags );
			TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( m_flags );
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += Score( TSuper::m_query, TSuper::m_subject );
			TSuper::SetMismatches() ++;
		}
		++TSuper::m_query;
		++TSuper::m_subject;
	}
	m_inGap = false;
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoInsertion( int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += TSuper::GetIupacnaQuery( m_flags );
			TSuper::SetSubjectString() += '-';
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += m_inGap ?
				  TSuper::GetAlignerBase().GetScoreTbl().GetGapExtentionScore() :
				  ( (m_inGap = true), TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetInsertions() ++;
		}
		++TSuper::m_query;
	}
}

template<class CQuery,class CSubject>
inline void TAligner_fast<CQuery,CSubject>::DoDeletion( int cnt )
{
	while( --cnt >= 0 ) {
		if( m_flags & CAlignerBase::fComputePicture ) {
			TSuper::SetQueryString() += '-';
			TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( m_flags );
			TSuper::SetAlignmentString() += ' ';
		}
		if( m_flags & CAlignerBase::fComputeScore ) {
			TSuper::SetScore() += m_inGap ?
				  TSuper::GetAlignerBase().GetScoreTbl().GetGapExtentionScore() :
				  ( (m_inGap = true), TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetDeletions() ++;
		}
		++TSuper::m_subject;
	}
	m_inGap = false;
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
