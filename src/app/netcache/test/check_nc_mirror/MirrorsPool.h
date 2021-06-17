#ifndef CHECK_NC_MIRROR_MIRRORSPOOL_HPP
#define CHECK_NC_MIRROR_MIRRORSPOOL_HPP

USING_NCBI_SCOPE;

#include <corelib/ncbireg.hpp>

#include "CncException.h"
#include "Mirror.h"

///////////////////////////////////////////////////////////////////////

class CMirrorsPool: public CTestParameters
{
public:
	map< string, shared_ptr< CMirror > > m_Mirrors;

	void load( const CNcbiRegistry &config );
};

#endif /* CHECK_NC_MIRROR_MIRRORSPOOL_HPP */