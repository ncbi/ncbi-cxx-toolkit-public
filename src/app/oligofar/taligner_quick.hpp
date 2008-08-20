#ifndef OLIGOFAR_TALIGNER_QUICK__HPP
#define OLIGOFAR_TALIGNER_QUICK__HPP

#include "talignerbase.hpp"
#include "fourplanes.hpp"

BEGIN_OLIGOFAR_SCOPES

class CAligner_quick_Base
{
public:
    typedef pair<int,int> TParam;
};

template<class CQuery,class CSubject>
class TAligner_quick : public TAlignerBase<CQuery,CSubject>, public CAligner_quick_Base
{
public:
    typedef TAlignerBase<CQuery,CSubject> TSuper;
    typedef TAligner_quick<CQuery,CSubject> TSelf;

    TAligner_quick( const TParam& param ) : 
        m_wordsize( param.first ), m_wordcount( param.second ) {}

	const char * GetName() const { return "Forward word lookup aligner"; }

    void Align( int flags );
protected:
	void DoIdentity( int cnt = 1 );
	void DoMismatch( int cnt = 1 );
	void DoInsertion( int cnt = 1 );
	void DoDeletion( int cnt = 1 );
protected:
	int m_flags;
    int m_wordsize;
    int m_wordcount;
    bool m_inGap;
};

template<class CQuery,class CSubject>
inline void TAligner_quick<CQuery,CSubject>::Align( int flags )
{
	m_flags = flags;
	m_inGap = false;

	const CQuery Q = TSuper::GetQueryEnd();
	const CSubject S = TSuper::GetSubjectEnd();

	// following vars are modified by Do* functions!!!
    CQuery& q = TSuper::m_query;
    CSubject& s = TSuper::m_subject;

    while( q < Q && s < S ) {
        
        while( q < Q && s < S && Match( q, s ) ) DoIdentity();

        if( q >= Q || s >= S ) break;

        int wordsize = ( (Q - q - 1) < m_wordsize ) ? Q - q - 1 : m_wordsize;
		if( wordsize == 0 ) { DoMismatch(1); }
		else {
        	ASSERT( wordsize > 0 );
            // we have mismatch here
            typedef pair<int,unsigned> TWordOffset;
            vector<TWordOffset> qwords;
			qwords.reserve( 4*m_wordcount );
            
            unsigned mask = CBitHacks::WordFootprint<unsigned>( 2 * wordsize );

            fourplanes::CHashGenerator hq( wordsize );
            CQuery qq( q ); 
            for( CQuery QQ(q + wordsize) ;qq < QQ; ++qq ) hq.AddBaseMask( CNcbi8naBase( *qq ) );
            for( int i = 0; i < m_wordcount && qq < Q; ++i, hq.AddBaseMask( CNcbi8naBase( *++qq ) ) ) {
                for( fourplanes::CHashIterator iq( hq, mask, mask ); iq; ++iq ) {
                    qwords.push_back( make_pair( i, *iq ) );
                }
			}
            fourplanes::CHashGenerator hs( wordsize );
            CSubject ss( s );
            for( CSubject SS(s + wordsize);ss < SS; ++ss ) hs.AddBaseMask( CNcbi8naBase( *ss ) );
            for( int j = 0; j < m_wordcount && ss < S; ++j, hs.AddBaseMask( CNcbi8naBase( *++ss ) ) ) {
                for( fourplanes::CHashIterator is( hs, mask, mask ); is; ++is ) {
                    ITERATE( vector<TWordOffset>, qw, qwords ) {
                        if( qw->second == *is ) {
                            if( qw->first < j ) {
                                DoDeletion( j - qw->first );
                                DoMismatch( qw->first );
                            } else if( qw->first > j ) {
                                DoInsertion( qw->first - j );
                                DoMismatch( j );
                            } else {
                                DoMismatch( j );
                            }
                            DoIdentity( wordsize );
                            goto match_found;
                        }
                    }
                }
            }
            do {
                int delta = Q - q;
                q = Q;
                s += delta;
            } while(0);
            TSuper::SetScore() += TSuper::GetScoreTbl().GetMismatchScore() * (Q - q);
        match_found:;
        } while(0);
    }
    TSuper::SetQueryAligned() = q - TSuper::GetQueryBegin();
    TSuper::SetSubjectAligned() = s - TSuper::GetSubjectBegin();
}

template<class CQuery,class CSubject>
inline void TAligner_quick<CQuery,CSubject>::DoIdentity( int i )
{
    while( i-- > 0 ) {
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
}

template<class CQuery,class CSubject>
inline void TAligner_quick<CQuery,CSubject>::DoMismatch( int cnt )
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
inline void TAligner_quick<CQuery,CSubject>::DoInsertion( int cnt )
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
				  ( m_inGap = true, TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetInsertions() ++;
		}
		++TSuper::m_query;
	}
}

template<class CQuery,class CSubject>
inline void TAligner_quick<CQuery,CSubject>::DoDeletion( int cnt )
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
				  ( m_inGap = true, TSuper::GetAlignerBase().GetScoreTbl().GetGapOpeningScore() );
			TSuper::SetDeletions() ++;
		}
		++TSuper::m_subject;
	}
	m_inGap = false;
}

END_OLIGOFAR_SCOPES

#endif
