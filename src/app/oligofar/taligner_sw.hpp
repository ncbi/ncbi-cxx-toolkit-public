#ifndef OLIGOFAR_TALIGNER_SW__HPP
#define OLIGOFAR_TALIGNER_SW__HPP

#include "talignerbase.hpp"

BEGIN_OLIGOFAR_SCOPES

class CAligner_SW_Base
{
public:
    enum ECode { eNull = 0, eIdentity = '=', eMismatch = 'm', eInsertion = 'i', eDeletion = 'd' };

    typedef std::pair<double,ECode> TValue;
    typedef std::vector<std::vector<TValue> > TMatrix;
    CAligner_SW_Base( TMatrix * matrix = 0 ) : m_matrix( matrix ) {}
protected:
    TMatrix * m_matrix;
};

template<class CQuery, class CSubject>
class TAligner_SW : public TAlignerBase<CQuery,CSubject>, public CAligner_SW_Base
{
public:
    TAligner_SW( TMatrix& m = 0 ) : CAligner_SW_Base( &m ) {}

    void SetMatrix( TMatrix * m ) { m_matrix = m; }

	const char * GetName() const { return "Smith-Waterman aligner"; }

    typedef TAlignerBase<CQuery,CSubject> TSuper;
    typedef TAligner_SW<CQuery,CSubject> TSelf;

    void Align( int flags );
protected:

    void x_Reset();
    void x_BuildMatrix();
    void x_ExtendToQueryEnd();
    void x_BackTrace( int flags );

    static void x_Reverse( string& s ) { reverse( s.begin(), s.end() ); }
            
    TMatrix& SetMatrix() { return *m_matrix; }
};

template<class CQuery,class CSubject>
inline void TAligner_SW<CQuery,CSubject>::Align( int flags )
{
    x_Reset();
    x_BuildMatrix();
    x_ExtendToQueryEnd();
    if( flags & CAlignerBase::fComputePicture ) { x_BackTrace( flags ); }
}

template<class CQuery,class CSubject>
inline void TAligner_SW<CQuery,CSubject>::x_BuildMatrix()
{
    const CQuery Q( TSuper::GetQueryEnd() );
    const CSubject S( TSuper::GetSubjectEnd() );

    CQuery query( TSuper::m_query );
    CSubject subject( TSuper::m_subject );

    int qsize = Q - query;
    int ssize = S - subject;

    int qend = 0;
    int send = 0;
    
    int xdropOff = SetMatrix().size()/2;
    double score = 0;

    const CScoreTbl& scoreTbl = TSuper::GetScoreTbl();

    for( int i = 0; i < (int) SetMatrix().size(); ++i ) SetMatrix()[i].resize( qsize );
    
    for( int q = 0; q < qsize; ++q ) {
        
        int b = std::max( xdropOff - q, 0 );
        int B = std::min( int( SetMatrix().size() ), int( ssize ) - q + xdropOff );
        
        for( ; b < B ; ++b ) {
            
            int s = q + b - xdropOff;
            
            double insScore = -std::numeric_limits<double>::max(), delScore = -std::numeric_limits<double>::max();
            double matchScore = Score( query + q, subject + s );
            bool match = matchScore > 0; // or Match( query + s, subject + s ) 

            if( q > 0 ) {
                matchScore += SetMatrix()[b][q-1].first;
                if( b < (int)SetMatrix().size() - 1 ) {
                    const TValue& val = SetMatrix()[b+1][q-1];
                    insScore = val.first + ( val.second == eInsertion ? scoreTbl.GetGapExtentionScore() : scoreTbl.GetGapOpeningScore() );
                }
            }
            
            if( b > 0 ) {
                const TValue& val = SetMatrix()[b-1][q];
                delScore = val.first + ( val.second == eDeletion ? scoreTbl.GetGapExtentionScore() : scoreTbl.GetGapOpeningScore() );
            }
            
            if( matchScore >= delScore ) {
                if( matchScore >= insScore ) {
                    SetMatrix()[b][q] = std::make_pair( matchScore, match ? eIdentity : eMismatch );
                    if( match && matchScore > score ) {
                        score = matchScore;
                        qend = q;
                        send = s;
                    }
                }
                else SetMatrix()[b][q] = std::make_pair( insScore, eInsertion );
            } else if ( delScore > insScore ) {
                SetMatrix()[b][q] = std::make_pair( delScore, eDeletion );
            } else if ( insScore > delScore ) {
                SetMatrix()[b][q] = std::make_pair( insScore, eInsertion );
            } else { // insScore == delScore > mismScore, choose any of the two best
                SetMatrix()[b][q] = std::make_pair( insScore, eMismatch );
            }
        }
    }
    TSuper::SetQueryAligned() = qend + 1;
    TSuper::SetSubjectAligned() = send + 1;
    TSuper::m_query = query + qend + 1;
    TSuper::m_subject = subject + send + 1;
    TSuper::SetScore() = score;
}

template<class CQuery,class CSubject>
inline void TAligner_SW<CQuery,CSubject>::x_ExtendToQueryEnd()
{
    const CQuery Q( TSuper::GetQueryEnd() );
    const CSubject S( TSuper::GetSubjectEnd() );

    CQuery& query( TSuper::m_query );
    CSubject& subject( TSuper::m_subject );

    double score = 0;
    int k = 0;
    
    for( ; query < Q ; ++k ) score += Score( query++, subject++ );

    TSuper::SetScore() += score;
    TSuper::SetQueryAligned() += k;
    TSuper::SetSubjectAligned() += k;
}

template<class CQuery,class CSubject>
inline void TAligner_SW<CQuery,CSubject>::x_BackTrace( int flags )
{
    const CQuery& query = TSuper::GetQueryBegin();
    const CSubject& subject = TSuper::GetSubjectBegin();
    
    int q = TSuper::m_query - query - 1;
    int s = TSuper::m_subject - subject - 1;

    int xdropOff = SetMatrix().size()/2;

    while( q >= 0 && s >= 0 ) {
        int b = s - q + xdropOff;
        const TValue& val = SetMatrix()[b][q];
        
        if( flags & CAlignerBase::fComputePicture ) 
            TSuper::SetAlignmentString() += ( val.second == eIdentity ? '|' : ' ' );

        switch( val.second ) {
        case eDeletion: 
            if( flags & CAlignerBase::fComputePicture ) {
                TSuper::SetQueryString() += '-';
                TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( subject + s + 1, flags );
            }
            --s; ++TSuper::SetDeletions(); 
            break;
        case eInsertion: 
            if( flags & CAlignerBase::fComputePicture ) {
                TSuper::SetQueryString() += TSuper::GetIupacnaQuery( query + q + 1, flags );
                TSuper::SetSubjectString() += '-';
            }
            --q;  ++TSuper::SetInsertions(); 
            break;
        default: --s, --q;  
            if( flags & CAlignerBase::fComputePicture ) {
                TSuper::SetQueryString() += TSuper::GetIupacnaQuery( query + q + 1, flags );
                TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( subject + s + 1, flags );
            }
            ++( val.second == eIdentity ? TSuper::SetIdentities() : TSuper::SetMismatches() ); 
            break;
        }
    }
    if( flags & CAlignerBase::fComputePicture ) {
        x_Reverse( TSuper::SetQueryString() );
        x_Reverse( TSuper::SetSubjectString() );
        x_Reverse( TSuper::SetAlignmentString() );
    }
}

template<class CQuery,class CSubject>
inline void TAligner_SW<CQuery,CSubject>::x_Reset()
{
    for( int i = 0; i < (int)SetMatrix().size(); ++i ) SetMatrix()[i].clear();
}

END_OLIGOFAR_SCOPES

#endif
