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
    typedef TSeqRef<CNcbi8naBase,+1,CSeqCoding::eCoding_ncbi8na> TQueryNcbi8naFwd; \
    typedef TSeqRef<CNcbipnaBase,+5,CSeqCoding::eCoding_ncbipna> TQueryNcbipnaFwd; \
    typedef TSeqRef<CNcbiqnaBase,+1,CSeqCoding::eCoding_ncbiqna> TQueryNcbiqnaFwd; \
    typedef TSeqRef<CColorTwoBase,+1,CSeqCoding::eCoding_colorsp> TQueryColorspFwd; \
    typedef TSeqRef<CNcbi8naBase,-1,CSeqCoding::eCoding_ncbi8na> TQueryNcbi8naRev; \
    typedef TSeqRef<CNcbipnaBase,-5,CSeqCoding::eCoding_ncbipna> TQueryNcbipnaRev; \
    typedef TSeqRef<CNcbiqnaBase,-1,CSeqCoding::eCoding_ncbiqna> TQueryNcbiqnaRev; \
    typedef TSeqRef<CColorTwoBase,-1,CSeqCoding::eCoding_colorsp> TQueryColorspRev; \
    typedef TSeqRef<CNcbi8naBase,+1,CSeqCoding::eCoding_ncbi8na> TSbjNcbi8naRefFwd; \
    typedef TSeqRef<CNcbi8naBase,-1,CSeqCoding::eCoding_ncbi8na> TSbjNcbi8naRefRev; \
    typedef TSeqRef<CColorTwoBase,+1,CSeqCoding::eCoding_colorsp> TSbjColorspRefFwd; \
    typedef TSeqRef<CColorTwoBase,-1,CSeqCoding::eCoding_colorsp> TSbjColorspRefRev; \
    \
    const PvtType& Get ## PvtObj() const { return m_ ## PvtObj; } \
    PvtType& Set ## PvtObj() { return m_ ## PvtObj; }       \
	\
protected: \
    template<class CQuery> \
    void AlignC( const char * q, int qlen, const char * s, int slen, int flags ); \
    template<class CQuery> \
    void AlignQ( const char * q, int qlen, const char * s, int slen, int flags ); \
    template<class CQuery, class CSubject> \
    void AlignQS( const char * q, int qlen, const char * s, int slen, int flags ); \
protected: \
    PvtType m_ ## PvtObj;                      \
};

#define DEFINE_ALIGNER( ALGO, PvtObj )                                         \
inline void CAligner_ ## ALGO::Align( CSeqCoding::ECoding qc, const char * q, int qlen, \
                                      CSeqCoding::ECoding sc, const char * s, int slen, int flags ) \
{ \
    if( qlen > 0 ) { \
        switch( qc ) { \
        case CSeqCoding::eCoding_colorsp: AlignC<TQueryColorspFwd>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbiqna: AlignQ<TQueryNcbiqnaFwd>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbipna: AlignQ<TQueryNcbipnaFwd>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbi8na: AlignQ<TQueryNcbi8naFwd>( q, qlen, s, slen, flags ); break; \
        default: THROW( logic_error, "Query coding other then ncbi8na, ncbiqna or ncbipna is not implemented" ); \
        } \
    } else { \
        switch( qc ) { \
        case CSeqCoding::eCoding_colorsp: AlignC<TQueryColorspRev>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbiqna: AlignQ<TQueryNcbiqnaRev>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbipna: AlignQ<TQueryNcbipnaRev>( q, qlen, s, slen, flags ); break; \
        case CSeqCoding::eCoding_ncbi8na: AlignQ<TQueryNcbi8naRev>( q, qlen, s, slen, flags ); break; \
        default: THROW( logic_error, "Query coding other then ncbi8na, ncbiqna or ncbipna is not implemented" ); \
        } \
    } \
} \
 \
template<class CQuery> \
inline void CAligner_ ## ALGO::AlignC( const char * q, int qlen, const char * s, int slen, int flags ) \
{ \
    if( slen > 0 ) \
        AlignQS<CQuery,TSbjNcbi8naRefFwd>( q, qlen, s, slen, flags ); \
    else \
        AlignQS<CQuery,TSbjNcbi8naRefRev>( q, qlen, s, slen, flags ); \
} \
 \
template<class CQuery> \
inline void CAligner_ ## ALGO::AlignQ( const char * q, int qlen, const char * s, int slen, int flags ) \
{ \
    if( slen > 0 ) \
        AlignQS<CQuery,TSbjNcbi8naRefFwd>( q, qlen, s, slen, flags ); \
    else \
        AlignQS<CQuery,TSbjNcbi8naRefRev>( q, qlen, s, slen, flags ); \
} \
 \
template<class CQuery, class CSubject> \
inline void CAligner_ ## ALGO::AlignQS( const char * q, int qlen, const char * s, int slen, int flags ) \
{ \
    TAligner_ ## ALGO<CQuery,CSubject> aligner( m_ ## PvtObj ); \
    aligner.SetAlignerBase( &m_alignerBase );     \
    aligner.ResetScores(); \
    aligner.SetQuery( q, qlen );  \
    aligner.SetSubject( s, slen );  \
    aligner.Align( flags ); \
    /*if( flags & CAlignerBase::fComputeScore ) aligner.ComputeBestQueryScore(); */ \
	if( flags & CAlignerBase::fComputeScore ) { \
		if( double sc = GetBestPossibleQueryScore() ) aligner.SetBestQueryScore( sc ); \
		/*else aligner.ComputeBestQueryScore();*/ \
	} \
    aligner.ReverseStringsIfNeeded( flags ); \
}

#define DECLARE_AND_DEFINE_ALIGNER( ALGO, PvtType, PvtObj )    \
    DECLARE_ALIGNER( ALGO, PvtType, PvtObj ) DEFINE_ALIGNER( ALGO, PvtObj )

END_OLIGOFAR_SCOPES

#endif
