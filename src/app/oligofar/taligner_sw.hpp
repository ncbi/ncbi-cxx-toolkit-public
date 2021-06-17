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

    int GetDiagCnt() const { return m_matrix->size()/2; }

    const TValue& GetMatrix( int q, int s, bool check = true ) const { 
        int d = GetDiagCnt();
        int b = s + d - q;
        ++q; // since value -1 is allowed
        if( check ) {
            try {
                ASSERT( b >= 0 );
                ASSERT( b < (int)m_matrix->size() );
                ASSERT( q >= 0 );
                ASSERT( q < (int)(*m_matrix)[0].size() );
            } catch(...) {
                cerr << __FUNCTION__ << "( " << (q-1) << ", " << s << " ) : " << DISPLAY( d ) << DISPLAY( b ) << "\n";
                throw;
            }
        } else if( b < 0 || q < 0 || b >= (int)m_matrix->size() || q >= (int)(*m_matrix)[0].size() ) {
            static TValue kBad( -99, eNull );
            return kBad;
        }
        return (*m_matrix)[b][q];
    }

    TValue& SetMatrix( int q, int s ) {
        int d = GetDiagCnt();
        int b = s + d - q;
        ++q; // since value -1 is allowed
        try {
            ASSERT( b >= 0 );
            ASSERT( b < (int)m_matrix->size() );
            ASSERT( q >= 0 );
            ASSERT( q < (int)(*m_matrix)[0].size() );
        } catch(...) {
            cerr << __FUNCTION__ << "( " << (q-1) << ", " << s << " ) : " << DISPLAY( d ) << DISPLAY( b ) << "\n";
            throw;
        }
        return (*m_matrix)[b][q];
    }

    TValue& SetMatrix( int q, int s, const TValue& v ) { return SetMatrix( q, s ) = v; }
    TValue& SetMatrix( int q, int s, double score, ECode code ) { return SetMatrix( q, s ) = TValue( score, code ); }

    void x_Reset();
    void x_BuildMatrix();
    void x_ExtendToQueryEnd();
    void x_BackTrace( int flags );
    void x_PrintMatrix();

    static void x_Reverse( string& s ) { reverse( s.begin(), s.end() ); }
            
    TMatrix& SetMatrix() { return *m_matrix; }
    const TMatrix& GetMatrix() const { return *m_matrix; }
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
    
    int xdropOff = GetMatrix().size()/2;
    double score = 0;

    const CScoreTbl& scoreTbl = TSuper::GetScoreTbl();

    for( int i = 0; i < (int) SetMatrix().size(); ++i ) SetMatrix()[i].resize( qsize + 2 );
    SetMatrix( -1, -1, 0, eMismatch );
    do { // init penalties for alignment starting at wrong position
        double f = scoreTbl.GetGapOpeningScore();
        if( xdropOff > 0 ) {
            SetMatrix(  0, -1, f, eDeletion );
            SetMatrix( -1,  0, f, eInsertion );
        }
        f += scoreTbl.GetGapExtentionScore();
        for( int k = 1; k < xdropOff; ++k, (f += scoreTbl.GetGapExtentionScore()) ) {
            SetMatrix(  k, -1, f, eDeletion );
            SetMatrix( -1,  k, f, eInsertion );
        }
    } while(0);

    for( int q = 0; q < qsize; ++q ) {
        
        int b = std::max( xdropOff - q, 0 );
        int B = std::min( int( GetMatrix().size() ), int( ssize ) - q + xdropOff );

        for( ; b < B ; ++b ) {
            
            int s = q + b - xdropOff;
            
            double insScore = -std::numeric_limits<double>::max(), delScore = -std::numeric_limits<double>::max();
            double matchScore = Score( query + q, subject + s );
            bool match = matchScore > 0; // or Match( query + s, subject + s ) 

            matchScore += GetMatrix( q - 1, s - 1).first;
            if( b < (int)GetMatrix().size() - 1 ) {
                const TValue& val = GetMatrix( q - 1, s );
                insScore = val.first + ( val.second == eInsertion ? scoreTbl.GetGapExtentionScore() : scoreTbl.GetGapOpeningScore() );
            }
            
            if( b > 0 ) {
                const TValue& val = GetMatrix( q, s - 1 );
                delScore = val.first + ( val.second == eDeletion ? scoreTbl.GetGapExtentionScore() : scoreTbl.GetGapOpeningScore() );
            }
            
            if( matchScore >= delScore ) {
                if( matchScore >= insScore ) {
                    SetMatrix( q, s,  matchScore, match ? eIdentity : eMismatch );
                    if( match && matchScore > score ) {
                        score = matchScore;
                        qend = q;
                        send = s;
//                        cerr << DISPLAY( qend ) << DISPLAY( send ) << DISPLAY( score ) << endl;
                    }
                }
                else SetMatrix( q, s, insScore, eInsertion );
            } else if ( delScore > insScore ) {
                SetMatrix( q, s, delScore, eDeletion );
            } else if ( insScore > delScore ) {
                SetMatrix( q, s, insScore, eInsertion );
            } else { // insScore == delScore > mismScore, choose any of the two best
                SetMatrix( q, s, insScore, eMismatch );
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
inline void TAligner_SW<CQuery,CSubject>::x_PrintMatrix()
{
    cerr << "\x1b[31mNULL\t\x1b[32mIdentity\t\x1b[33mMismatch\t\x1b[34mInsertion\t\x1b[35mDeletion\t\x1b[36mOTHER\x1b[0m\n";
    cerr << string(78,'=') << endl;
    for( int q = -1; q < (int)((*m_matrix)[0].size() - 1); ++q ) {
        for( int s = -1; s < (int)((*m_matrix)[0].size() - 1); ++s ) {
            if( s >= 0 ) cerr << "\t";
            switch( GetMatrix( q, s, false ).second ) {
            case eNull: cerr << "\x1b[31m"; break;
            case eIdentity: cerr << "\x1b[32m"; break;
            case eMismatch: cerr << "\x1b[33m"; break;
            case eInsertion: cerr << "\x1b[34m"; break;
            case eDeletion: cerr << "\x1b[35m"; break;
            default: cerr << "\x1b[36m"; break;
            }
            cerr << GetMatrix( q, s, false ).first << "\x1b[0m";
        }
        cerr << "\n";
    }
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
    if( CAlignerBase::GetPrintDebug() ) x_PrintMatrix();
    const CQuery query = TSuper::GetQueryBegin();
    const CSubject subject = TSuper::GetSubjectBegin();
    
    int q = TSuper::m_query - query - 1;
    int s = TSuper::m_subject - subject - 1;

    TSuper::m_query -= TSuper::SetQueryAligned();
    TSuper::m_subject -= TSuper::SetSubjectAligned();
    
    //int xdropOff = SetMatrix().size()/2;

    while( q >= 0 && s >= 0 ) {
        //int b = s - q + xdropOff;
        const TValue& val = GetMatrix( q, s ); //SetMatrix()[b][q+1];
        
//        cerr << DISPLAY( q ) << DISPLAY( s ) << DISPLAY( val.first ) << DISPLAY( char( val.second ) ) << endl;

        switch( val.second ) {
        case eDeletion: 
            if( flags & CAlignerBase::fComputePicture ) {
                TSuper::SetQueryString() += '-';
                TSuper::SetSubjectString() += TSuper::GetIupacnaSubject( subject + s + 1, flags );
                TSuper::SetAlignmentString() += ' ';
            }
            --s; ++TSuper::SetDeletions(); 
            break;
        case eInsertion: 
            if( flags & CAlignerBase::fComputePicture ) {
                TSuper::SetQueryString() += TSuper::GetIupacnaQuery( query + q + 1, flags );
                TSuper::SetSubjectString() += '-';
                TSuper::SetAlignmentString() += ' ';
            }
            --q;  ++TSuper::SetInsertions(); 
            break;
        default: --s, --q;  
            if( flags & CAlignerBase::fComputePicture ) {

                char qb = TSuper::GetIupacnaQuery( query + q + 1, flags );
                char sb = TSuper::GetIupacnaSubject( subject + s + 1, flags );
                TSuper::SetQueryString()   += qb;
                TSuper::SetSubjectString() += sb;
                TSuper::SetAlignmentString() += ( val.second == eIdentity ? qb == sb ? '|' : ':' : ' ' );
            }
            ++( val.second == eIdentity ? TSuper::SetIdentities() : TSuper::SetMismatches() ); 
            break;
        }
    }

    TSuper::m_query = query;
    TSuper::m_subject = subject;
    
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
