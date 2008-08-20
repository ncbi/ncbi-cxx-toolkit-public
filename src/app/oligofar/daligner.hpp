#ifndef OLIGOFAR_DALIGNER__HPP
#define OLIGOFAR_DALIGNER__HPP

#include "ialigner.hpp"

BEGIN_OLIGOFAR_SCOPES

#define DECLARE_ALIGNER( ALGO, PvtType, PvtObj ) \
class CAligner_ ## ALGO : public IAligner \
{ \
public: \
    virtual void Align( CSeqCoding::ECoding qc, const char * q, int qlen,  \
                        CSeqCoding::ECoding sc, const char * s, int slen, int flags ); \
    typedef TSeqRef<CNcbi8naBase,+1,CSeqCoding::eCoding_ncbi8na> TQueryNcbi8naRef; \
    typedef TSeqRef<CNcbipnaBase,+5,CSeqCoding::eCoding_ncbipna> TQueryNcbipnaRef; \
    typedef TSeqRef<CNcbi8naBase,+1,CSeqCoding::eCoding_ncbi8na> TSubjectRefFwd; \
    typedef TSeqRef<CNcbi8naBase,-1,CSeqCoding::eCoding_ncbi8na> TSubjectRefRev; \
    \
    const PvtType& Get ## PvtObj() const { return m_ ## PvtObj; } \
    PvtType& Set ## PvtObj() { return m_ ## PvtObj; }       \
	\
protected: \
    template<class CQuery> \
    void Align( const char * q, int qlen, const char * s, int slen, int flags ); \
    template<class CQuery, class CSubject> \
    void Align( const char * q, int qlen, const char * s, int slen, int flags ); \
protected: \
    PvtType m_ ## PvtObj;                      \
};

#define DEFINE_ALIGNER( ALGO, PvtObj )                                         \
inline void CAligner_ ## ALGO::Align( CSeqCoding::ECoding qc, const char * q, int qlen, \
                                      CSeqCoding::ECoding sc, const char * s, int slen, int flags ) \
{ \
    ASSERT( sc == CSeqCoding::eCoding_ncbi8na ); \
    ASSERT( qlen >= 0 ); \
    switch( qc ) { \
    case CSeqCoding::eCoding_ncbipna: Align<TQueryNcbipnaRef>( q, qlen, s, slen, flags ); break; \
    case CSeqCoding::eCoding_ncbi8na: Align<TQueryNcbi8naRef>( q, qlen, s, slen, flags ); break; \
    default: THROW( logic_error, "Query coding other then ncbi8na or ncbipna is not implemented" ); \
    } \
} \
 \
template<class CQuery> \
inline void CAligner_ ## ALGO::Align( const char * q, int qlen, const char * s, int slen, int flags ) \
{ \
    if( slen > 0 ) \
        Align<CQuery,TSubjectRefFwd>( q, qlen, s, slen, flags ); \
    else \
        Align<CQuery,TSubjectRefRev>( q, qlen, s, slen, flags ); \
} \
 \
template<class CQuery, class CSubject> \
inline void CAligner_ ## ALGO::Align( const char * q, int qlen, const char * s, int slen, int flags ) \
{ \
    TAligner_ ## ALGO<CQuery,CSubject> aligner( m_ ## PvtObj ); \
    aligner.SetAlignerBase( &m_alignerBase );     \
    aligner.ResetScores(); \
    aligner.SetQuery( q, qlen );  \
    aligner.SetSubject( s, slen );  \
    aligner.Align( flags ); \
    if( flags & CAlignerBase::fComputeScore ) aligner.ComputeBestQueryScore(); \
    aligner.ReverseStringsIfNeeded( flags ); \
}

#define DECLARE_AND_DEFINE_ALIGNER( ALGO, PvtType, PvtObj )    \
    DECLARE_ALIGNER( ALGO, PvtType, PvtObj ) DEFINE_ALIGNER( ALGO, PvtObj )

END_OLIGOFAR_SCOPES

#endif
