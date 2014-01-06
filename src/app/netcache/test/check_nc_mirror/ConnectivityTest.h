#ifndef CONNECTIVITY_TEST_HPP
#define CONNECTIVITY_TEST_HPP

USING_NCBI_SCOPE;

#include "NetcacheTest.h"

///////////////////////////////////////////////////////////////////////

class CConnectivityTest: public CNetcacheTest
{
public:
	static void DoMirrorsCheck( CMirrorsPool &mirrors_pool );

	CConnectivityTest( CServer &server ): CNetcacheTest( server ) {}

	virtual void* Main();
};

#endif /* CONNECTIVITY_TEST_HPP */