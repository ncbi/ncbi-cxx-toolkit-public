#ifndef SIMPLETHREADPOOL_HPP
#define SIMPLETHREADPOOL_HPP

USING_NCBI_SCOPE;

#include <corelib/ncbithr.hpp>

class CSimpleThreadPool
{
private:
	//list< shared_ptr< CThread > > m_Threads1;
	list< CThread* > m_Threads;

public:
	void AddThread( CThread *p_thread );

	void WaitAll();
};

#endif /* SIMPLETHREADPOOL_HPP */

