#ifndef OLIGOFAR_IGUIDEFILE__HPP
#define OLIGOFAR_IGUIDEFILE__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

class IGuideFile 
{
public:
    virtual ~IGuideFile() {}
    virtual bool NextHit( Uint8 ordinal, CQuery * query ) = 0;
};

END_OLIGOFAR_SCOPES

#endif
