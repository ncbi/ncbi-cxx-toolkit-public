#ifndef OLIGOFAR_CSCORING__HPP
#define OLIGOFAR_CSCORING__HPP

#include "cseqcoding.hpp"

BEGIN_OLIGOFAR_SCOPES

class CScoring
{
public:
    CScoring() :
        m_identity( 1 ), m_mismatch( -1 ), m_gapOpening( -3 ), m_gapExtention( -2 ) { x_Verify(); }
    CScoring( double id, double mm, double go, double ge ) :
        m_identity( id ), m_mismatch( mm ), m_gapOpening( go ), m_gapExtention( ge ) { x_Verify(); }
    void SetIdentityScore( double x ) { m_identity = x; }
    void SetMismatchScore( double x ) { m_mismatch = x; }
    void SetGapOpeningScore( double x ) { m_gapOpening = x; }
    void SetGapExtentionScore( double x ) { m_gapExtention = x; }
    double GetGapOpeningScore() const { return m_gapOpening; }
    double GetGapExtentionScore() const { return m_gapExtention; }
    double GetMismatchScore() const { return m_mismatch; }
    double GetIdentityScore() const { return m_identity; }
protected:
    double m_identity;
    double m_mismatch;
    double m_gapOpening;
    double m_gapExtention;
protected:
    void x_Verify() {
        ASSERT( m_identity > 0 );
        ASSERT( m_mismatch <= 0 );
        ASSERT( m_gapOpening <= 0 );
        ASSERT( m_gapExtention <= 0 );
    }
};

END_OLIGOFAR_SCOPES

#endif
