#ifndef READ_TEST_HPP
#define READ_TEST_HPP

USING_NCBI_SCOPE;

#include "NetcacheTest.h"

///////////////////////////////////////////////////////////////////////

class CReadTest: public CNetcacheTest
{
public:
	static void DoMirrorsCheck( CMirrorsPool &mirrors_pool );

	CReadTest( CServer &server ): CNetcacheTest( server ) {}

	virtual void* Main();
};

#endif /* READ_TEST_HPP */