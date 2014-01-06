#include "ncbi_pch.hpp"
#include "SimpleThreadPool.h"

void CSimpleThreadPool::AddThread( CThread *thread )
{
	m_Threads.push_back( thread );
}

void CSimpleThreadPool::WaitAll()
{
	//for_each( m_Threads.begin(), m_Threads.end(), [&]( shared_ptr<CThread> thread ){ 
	for_each( m_Threads.begin(), m_Threads.end(), [&]( CThread *thread ){ 
		thread->Join(); 
	} );
}