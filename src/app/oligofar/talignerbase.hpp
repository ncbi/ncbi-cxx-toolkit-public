#ifndef OLIGOFAR_TALIGNERBASE__HPP
#define OLIGOFAR_TALIGNERBASE__HPP

#include "calignerbase.hpp"
#include "tseqref.hpp"

BEGIN_OLIGOFAR_SCOPES

template <class CQuery, class CSubject>
class TAlignerBase //: public CAlignerBase
{
public:
	// transcript contains pairs of position (in alignment) and appropriate subject base in Ncbi8na; subject may be | 0x80 if it is deletion on query side
    typedef TAlignerBase<CQuery,CSubject> TSelf;
	TAlignerBase( CAlignerBase * abase = 0 ) : m_alignerBase( abase ) {} //const CScoreTbl& scoretbl = GetDefaultScoreTbl() );
	
    const CScoreTbl& GetScoreTbl() const { return *(m_alignerBase->m_scoreTbl); }
    const CAlignerBase& GetAlignerBase() const { return *m_alignerBase; }

    TSelf& SetAlignerBase( CAlignerBase * abase ) { m_alignerBase = abase; return *this; }
    TSelf& SetQuery( const char * query, int length );
    TSelf& SetSubject( const char * subject, int length );
    TSelf& ResetScores() { m_alignerBase->ResetScores(); return *this; }

    const CQuery& GetQueryCursor() const { return m_query; }
    const CSubject& GetSubjectCursor() const { return m_subject; }

    CQuery GetQueryBegin() const { return CQuery( m_alignerBase->m_qbegin ); }
    CSubject GetSubjectBegin() const { return CSubject( m_alignerBase->m_sbegin ); }
    CQuery GetQueryEnd() const { return CQuery( m_alignerBase->m_qend ); }
    CSubject GetSubjectEnd() const { return CSubject( m_alignerBase->m_send ); }

	//double ComputeBestQueryScore();
	double SetBestQueryScore( double score ) { return m_alignerBase->m_bestScore = score; }

	double GetGapOpeningScore() const { return GetAlignerBase().GetScoreTbl().GetGapOpeningScore(); }
	double GetGapExtentionScore() const { return GetAlignerBase().GetScoreTbl().GetGapExtentionScore(); }

	template<class TQuery,class TSubject>
	bool   Match( const TQuery& q, const TSubject& s ) const { return GetScoreTbl().MatchRef( q, s, q - m_query, s - m_subject ); }
	double Score( const CQuery& q, const CSubject& s ) const { return GetScoreTbl().ScoreRef( q, s, q - m_query, s - m_subject ); }
	template<class TQuery>
	bool   Match( const TQuery& q, const TQuery& s ) const { return GetScoreTbl().MatchRef( q, s, 2, 2 ); }

//    TODO: correct this code
	CSeqCoding::EStrand GetReferenceStrandForQuery( int flags ) const;
	CSeqCoding::EStrand GetReferenceStrandForSubject( int flags ) const;

 	CIupacnaBase GetIupacnaQuery( int flags ) const { return GetIupacnaQuery( m_query, flags ); }
 	CIupacnaBase GetIupacnaSubject( int flags ) const { return GetIupacnaSubject( m_subject, flags ); }

 	CIupacnaBase GetIupacnaQuery( const CQuery& q, int flags ) const;
 	CIupacnaBase GetIupacnaSubject( const CSubject& s, int flags ) const;

    void ReverseStringsIfNeeded( int flags );

protected:

    CAlignerBase& SetAlignerBase() { return *m_alignerBase; }

    int& SetIdentities() { return SetAlignerBase().m_identities; }
    int& SetMismatches() { return SetAlignerBase().m_mismatches; }
    int& SetInsertions() { return SetAlignerBase().m_insertions; }
    int& SetDeletions() { return SetAlignerBase().m_deletions; }

    int& SetQueryAligned() { return SetAlignerBase().m_qaligned; }
    int& SetSubjectAligned() { return SetAlignerBase().m_saligned; }
    
    double& SetScore() { return SetAlignerBase().m_score; }
    
    string& SetQueryString() { return SetAlignerBase().m_queryString; }
    string& SetSubjectString() { return SetAlignerBase().m_subjectString; }
    string& SetAlignmentString() { return SetAlignerBase().m_alignmentString; }
        
protected:
    CAlignerBase * m_alignerBase;
    CQuery m_query;
    CSubject m_subject;
};

////////////////////////////////////////////////////////////////////////
// Implementations

/* Note: keep this code *******
template<class CQuery,class CSubject>
inline double TAlignerBase<CQuery,CSubject>::ComputeBestQueryScore() 
{
    return m_alignerBase->m_bestScore = ( GetAlignerBase().GetQueryAlignedLength() * ( m_query.GetCoding() == CSeqCoding::eCoding_ncbipna ? 255 : 1 ) );
}
*/
template<class CQuery,class CSubject>
inline CIupacnaBase TAlignerBase<CQuery,CSubject>::GetIupacnaQuery( const CQuery& q, int flags ) const 
{
    ASSERT( CAlignerBase::fPictureQueryStrand );
    ASSERT( !CAlignerBase::fPictureSubjectStrand );

    if( flags & CAlignerBase::fPictureQueryStrand ) {
        if( q.GetCoding() == CSeqCoding::eCoding_colorsp && q == m_query ) 
            return CNcbi8naBase( char(*q) ); //CNcbi8naBase( char(q.GetBase())&0x0f ) ;
        else return q.GetBase();
    }
    else {
        if( q.GetCoding() == CSeqCoding::eCoding_colorsp && q == m_query ) {
            CNcbi8naBase x = char(*q); //CNcbi8naBase( char( q.GetBase( m_subject.GetStrand() ) ) & 0x0f );
            return m_subject.IsReverse() ? CNcbi8naBase( char(*q) ).Complement() : CNcbi8naBase( char(*q) );
        }
        else
            return q.GetBase( m_subject.GetStrand() );
    }
}

template<class CQuery,class CSubject>
inline CIupacnaBase TAlignerBase<CQuery,CSubject>::GetIupacnaSubject( const CSubject& s, int flags ) const 
{
    ASSERT( CAlignerBase::fPictureQueryStrand );
    ASSERT( !CAlignerBase::fPictureSubjectStrand );
    
    if( flags & CAlignerBase::fPictureQueryStrand ) return s.GetBase();
    else return s.GetBase( m_query.GetStrand() );
}

template<class CQuery,class CSubject>
inline void TAlignerBase<CQuery,CSubject>::ReverseStringsIfNeeded( int flags ) 
{
    if( flags & CAlignerBase::fComputePicture ) 
        if( flags & CAlignerBase::fPictureQueryStrand && m_query.IsReverse() ||
            (! (flags & CAlignerBase::fPictureSubjectStrand )) && m_subject.IsReverse() ) {
            reverse( m_alignerBase->m_queryString.begin(), m_alignerBase->m_queryString.end() );
            reverse( m_alignerBase->m_subjectString.begin(), m_alignerBase->m_subjectString.end() );
            reverse( m_alignerBase->m_alignmentString.begin(), m_alignerBase->m_alignmentString.end() );
        }
}

template<class CQuery,class CSubject>
TAlignerBase<CQuery,CSubject>& TAlignerBase<CQuery,CSubject>::SetQuery( const char * query, int length ) 
{ 
    m_query = m_alignerBase->m_qbegin = query; 
    m_alignerBase->m_qend = query + length; 
    /*
    cerr << "QUERY: ";
    for( CQuery q( m_query ); q != m_alignerBase->m_qend; ++q )
        cerr << CIupacnaBase( *q );
    cerr << "\n";
    */
    return *this; 
}

template<class CQuery,class CSubject>
TAlignerBase<CQuery,CSubject>& TAlignerBase<CQuery,CSubject>::SetSubject( const char * subject, int length ) 
{ 
    m_subject = m_alignerBase->m_sbegin = subject; 
    m_alignerBase->m_send = subject + length; 
    /*
    cerr << "SBJCT: ";
    for( CSubject s( m_subject ); s != m_alignerBase->m_send; ++s )
        cerr << CIupacnaBase( *s );
    cerr << "\n";
    */
    return *this; 
}

END_OLIGOFAR_SCOPES

#endif
