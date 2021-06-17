#include "ncbi_pch.hpp"
#include "SimpleThreadPool.h"
#include "WriteTest.h"

///////////////////////////////////////////////////////////////////////

void* CWriteTest::Main()
{
	m_Server.m_Writable = false;

	try	
	{
		if( m_Server.m_Connectable )
		{
			//1. Write a blob

			//2. Write+Override a blob with random delay between the ops [0..5] sec to m_Server

			//2.1 Write+Override a blob with random delay between the ops [0..5] sec overriding from random server

			//3. Write+Delete a blob with random delay between the ops [0..5] sec

			//3.1 Write+Delete a blob with random delay between the ops [0..5] sec deleting from random server

			m_Server.m_Writable = true;
		}
	}
	catch( ... )
	{}

	return NULL;
}

void CWriteTest::DoMirrorsCheck( CMirrorsPool &mirrors_pool )
{
	time_t systime = time(NULL);

	CSimpleThreadPool test_thread_pool;

	for( int test_state=check; test_state<=finalization; test_state++ )
	{
		for( map< string, shared_ptr< CMirror > >::iterator itr=mirrors_pool.m_Mirrors.begin(); itr!=mirrors_pool.m_Mirrors.end(); itr++ )
		{
			CMirror &mirror = *itr->second;

			if( on_schedule( mirror.m_BaseTestFreq, mirror.m_BaseTestFreq, systime ) )
			{
				bool mirror_writable = true;

				for( map< string, shared_ptr< CDomain > >::iterator itr=mirror.m_Domains.begin(); itr!=mirror.m_Domains.end(); itr++ )
				{
					CDomain &domain = *itr->second;
				
					for( map< string, shared_ptr< CServer > >::iterator itr=domain.m_Servers.begin(); itr!=domain.m_Servers.end(); itr++ )
					{
						CServer &server = *itr->second;

						if( test_state == check )
						{
							CWriteTest *write_test = new CWriteTest( server );

							test_thread_pool.AddThread( write_test );
							
							write_test->Run();
						}
						else
							domain.m_Writable |= server.m_Writable;
					}

					if( test_state == finalization )
						mirror.m_Writable &= domain.m_Writable;
				}

				if( test_state == finalization )
				{
					if( ! mirror.m_Writable )
						ERR_POST( "'" + mirror.m_MirrorName + "' mirror write check failure" );
					else
						LOG_POST( "Info: '" + mirror.m_MirrorName + "' mirror write check success" );
				}
			}
		}

		if( test_state == check ) test_thread_pool.WaitAll();
	}
}
