#ifndef WRITE_TEST_HPP
#define WRITE_TEST_HPP

USING_NCBI_SCOPE;

#include "NetcacheTest.h"

///////////////////////////////////////////////////////////////////////

class CWriteTest: public CNetcacheTest
{
public:
	static void DoMirrorsCheck( CMirrorsPool &mirrors_pool );

	CWriteTest( CServer &server ): CNetcacheTest( server ) {}

	virtual void* Main();
};

#endif /* WRITE_TEST_HPP */