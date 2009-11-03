#ifndef OLIGOFAR_CPROGRESSINDICATOR__HPP
#define OLIGOFAR_CPROGRESSINDICATOR__HPP

#include "iprogress.hpp"
#ifndef _WIN32
#include <sys/time.h>
#else
#include <time.h>
#endif

BEGIN_OLIGOFAR_SCOPES

class CProgressIndicator : public IProgressIndicator
{
public:
    enum EIndicate { e_current, e_final };

    CProgressIndicator( const string& message = "", const string& units = "op" ) : 
        m_t0( time(0) ), m_t1( m_t0 ), 
        m_currentValue(0), m_initialValue(0), m_finalValue(0), m_increment(1),
        m_message( message ), m_units( units ) {}
    virtual void Increment() { m_currentValue += m_increment; Indicate(); }
    virtual void Summary() { Format( e_final ); }
            
    void ResetTimer() { m_t0 = m_t1 = time(0); }
    void SetMessage( const string& message ) { m_message = message; Indicate(); }
    void SetUnits( const string& units ) { m_units = units; Indicate(); }
    void AddCurrentValue( double value ) { m_currentValue += value; Indicate(); }
    void SetCurrentValue( double value ) { m_currentValue = value; Indicate(); }
    void SetInitialValue( double value ) { m_initialValue = value; }
    void SetFinalValue( double value ) { m_finalValue = value; }
    void SetIncrement( double value ) { m_increment = value; }
    void Format( EIndicate i = e_current ) { Format( i, time(0) ); }
    void Indicate( EIndicate i = e_current ) { time_t t2 = time(0); if( t2 != m_t1 ) Format( i, t2 ); }
protected:
    void Format( EIndicate, time_t );
    static ostream& FormatBigNumber( ostream&, double );
protected:
    time_t m_t0;
    time_t m_t1;
    double m_currentValue;
    double m_initialValue;
    double m_finalValue;
    double m_increment;
    string m_message;
    string m_units;
};

END_OLIGOFAR_SCOPES

#endif
