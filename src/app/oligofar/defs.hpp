#ifndef OLIGOFAR_DEFS__HPP
#define OLIGOFAR_DEFS__HPP

#include "scopes.hpp"
#include "debug.hpp"

BEGIN_OLIGOFAR_SCOPES

USING_SCOPE(std);

typedef long long huge;

#ifdef OLIGOFAR_STANDALONE

typedef short Int2;
typedef long  Int4;
typedef long long Int8;

typedef unsigned short Uint2;
typedef unsigned long  Uint4;
typedef unsigned long long Uint8;

#endif

END_OLIGOFAR_SCOPES

#endif
