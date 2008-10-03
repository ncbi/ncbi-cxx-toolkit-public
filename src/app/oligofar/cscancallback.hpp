#ifndef OLIGOFAR_CSCANCALLBACK__HPP
#define OLIGOFAR_CSCANCALLBACK__HPP

#include "cqueryhash.hpp"
#include "cseqscanner.hpp"

BEGIN_OLIGOFAR_SCOPES

class CScanCallback
{
public:
    typedef CSeqScanner::TMatches TMatches;
    CScanCallback( TMatches& matches, CQueryHash& queryHash ) :
        m_matches( matches ), m_queryHash( queryHash ) {}
    void operator () ( Uint8 hash, Uint4 mism, Uint8 alt ) {
        m_mism = mism;
        m_queryHash.ForEach( hash, *this );
    }
    void operator () ( const CHashAtom& a ) { m_matches.insert( a ); }
protected:
    Uint4 m_mism;
    TMatches& m_matches;
    CQueryHash& m_queryHash;
};

END_OLIGOFAR_SCOPES    

#endif
