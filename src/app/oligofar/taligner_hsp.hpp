#ifndef OLIGOFAR_TALIGNER_HSP__HPP
#define OLIGOFAR_TALIGNER_HSP__HPP

#include "talignerbase.hpp"

BEGIN_OLIGOFAR_SCOPES

template<class CQuery,class CSubject>
class TAligner_HSP : public TAlignerBase<CQuery,CSubject>
{
public:
    TAligner_HSP(int = 0) {}

    typedef TAlignerBase<CQuery,CSubject> TSuper;
    typedef TAligner_HSP<CQuery,CSubject> TSelf;

    void Align( int flags );

	const char * GetName() const { return "HSP aligner"; }
};

template<class CQuery,class CSubject>
inline void TAligner_HSP<CQuery,CSubject>::Align( int flags )
{
    CQuery query = TSuper::m_query;
    CSubject subject = TSuper::m_subject;

    TSuper::SetQueryAligned() = TSuper::SetSubjectAligned() = TSuper::GetQueryEnd() - query;
    
    for( ; query < TSuper::GetQueryEnd() && subject < TSuper::GetSubjectEnd() ; ++query, ++subject ) {
        if( flags & CAlignerBase::fComputeScore ) {
            TSuper::SetScore() += Score( query, subject );
            ++( Match( query, subject ) ? TSuper::SetIdentities() : TSuper::SetMismatches() );
        }
        if( flags & CAlignerBase::fComputePicture ) {
            char qb = TSuper::GetIupacnaQuery( query, flags );
            char sb = TSuper::GetIupacnaSubject( subject, flags );
            TSuper::SetQueryString()   += qb;
            TSuper::SetSubjectString() += sb;
            TSuper::SetAlignmentString() += Match( query, subject ) ? qb == sb ? '|' : ':' : ' ';
        }
    }
    TSuper::m_query = query;
    TSuper::m_subject = subject;
}

END_OLIGOFAR_SCOPES

#endif
