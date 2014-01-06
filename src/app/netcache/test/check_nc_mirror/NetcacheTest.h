#ifndef NETCACHE_TEST_HPP
#define NETCACHE_TEST_HPP

USING_NCBI_SCOPE;

//#include <util/thread_pool.hpp>

#include "MirrorsPool.h"

///////////////////////////////////////////////////////////////////////
/*
class CNetcacheTestThreadPoolController: public CThreadPool_Controller
{
public:
	CNetcacheTestThreadPoolController( unsigned int max_threads, unsigned int min_threads ):
		CThreadPool_Controller( max_threads, min_threads )
	{}

protected:
	virtual void OnEvent(EEvent event);
};
*/
class CNetcacheTest: public CThread
{
protected:
	enum ENetcacheTestState { 
		check, finalization 
	};

	//static string m_LocalHostFullName;
	//static string m_LocalHostName;
	//static string m_LocalHostDomain;

	//static CNetcacheTestThreadPoolController s_ThreadPoolController;
	//static CThreadPool s_TestThreadPool;

	CServer &m_Server;

public:
	CNetcacheTest( CServer &server ): m_Server( server ) {}

	//static void init();

	static bool on_schedule( unsigned freq, unsigned base_freq, time_t systime = time(NULL) );
};

#endif /* NETCACHE_TEST_HPP */