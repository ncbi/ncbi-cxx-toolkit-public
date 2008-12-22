#ifndef OLIGOFAR_CALIGNTEST__HPP
#define OLIGOFAR_CALIGNTEST__HPP

#include "capp.hpp"
#include "util.hpp"
#include <corelib/ncbireg.hpp>
#include <util/value_convert.hpp>
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CScoreTbl;
class CAlignTest : public CApp
{
public:
    CAlignTest( int argc, char ** argv );
    enum EAlignmentAlgo {
        eAlignment_HSP   = 'h',
        eAlignment_SW    = 's',
        eAlignment_fast  = 'f',
    };

    enum {
        fQuery_reverse = 0x01,
        fSubject_reverse = 0x02,
        fAlign_colorspace = 0x10
    };
    const option* GetLongOptions() const;
    const char * GetOptString() const;
    int ParseArg( int, const char *, int );

    unsigned SetFlags( unsigned flags, bool to ) { if( to ) m_flags |= flags; else m_flags &= ~flags; return m_flags; }
    unsigned SetFlags( unsigned flags, const string& arg ) { return SetFlags( flags, bool(Convert( arg )) ); }
    unsigned GetFlags( unsigned flags ) const { return m_flags & flags; }

protected:
    virtual void Help( const char * );
    virtual int  Execute();
    IAligner * CreateAligner( EAlignmentAlgo algo, CScoreTbl * scoreTbl ) const ;
    IAligner * NewAligner( EAlignmentAlgo algo, int ) const ;

protected:
    string m_query;
    string m_subject;
    EAlignmentAlgo m_algo;
    unsigned m_flags;
    int m_offsetQuery;
    int m_offsetSubject;
    int m_xDropoff;
    double m_identityScore;
    double m_mismatchScore;
    double m_gapOpeningScore;
    double m_gapExtentionScore;
};

END_OLIGOFAR_SCOPES

#endif
