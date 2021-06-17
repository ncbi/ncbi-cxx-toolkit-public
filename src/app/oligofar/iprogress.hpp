#ifndef OLIGOFAR_IPROGRESS__HPP
#define OLIGOFAR_IPROGRESS__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class IProgressIndicator
{
public:
    virtual ~IProgressIndicator() {}
    virtual void SetCurrentValue( double val ) = 0;
    virtual void Increment() = 0;
    virtual void Summary() = 0;
};

END_OLIGOFAR_SCOPES

#endif
