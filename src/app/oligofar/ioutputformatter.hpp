#ifndef OLIGOFAR_IOUTPUTFORMATTER__HPP
#define OLIGOFAR_IOUTPUTFORMATTER__HPP

#include "chit.hpp"

BEGIN_OLIGOFAR_SCOPES

class IOutputFormatter
{
public:
    virtual ~IOutputFormatter() {}
    virtual bool ShouldFormatAllHits() const = 0;
    virtual void FormatQueryHits( const CQuery * query ) = 0;
    virtual void FormatHit( const CHit* hit ) = 0;
    virtual bool NeedSeqids() const = 0;
};

END_OLIGOFAR_SCOPES

#endif
