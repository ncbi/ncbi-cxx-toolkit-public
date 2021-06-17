#include "ncbi_pch.hpp"
#include "SimpleThreadPool.h"
#include "ReadTest.h"

///////////////////////////////////////////////////////////////////////

void* CReadTest::Main()
{
	m_Server.m_Readable = false;

	try	
	{
		if( m_Server.m_Connectable )
		{
			//1. Retrieve its "master" IC blob by key: <mirror-id>__<writer-hostname>
			
			//2. Verify that the "master" blob isn't outdated (see conf param outdated)
			
			//3. Check whether the blob has been updated since it was last read. If it wasn't updated, then just quit with no action.
			
			//3.1 Use IC blob with key <mirror-id>__<writer-hostname>__<reader-hostname> to keep timestamp from the version of the master blob that has already been processed (read).
			
			//4. Store the timestamp of the new version of the "master" blob in the <writer-hostname>__<reader-hostname> blob.
			
			//5. Verify that all blobs stored (and/or overridden or deleted) by the writer are in the "right" (expected) state, on all:
			
			//5.1 Connectable servers from the servers list
			
			//5.2 Services listed in nc_service and ic_service

			m_Server.m_Readable = true;
		}
	}
	catch( ... )
	{}

	return NULL;
}

void CReadTest::DoMirrorsCheck( CMirrorsPool &mirrors_pool )
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
							CReadTest *read_test = new CReadTest( server );

							test_thread_pool.AddThread( read_test );
							
							read_test->Run();
						}
						else
							domain.m_Readable |= server.m_Readable;
					}

					if( test_state == finalization )
						mirror.m_Readable &= domain.m_Readable;
				}

				if( test_state == finalization )
				{
					if( ! mirror.m_Readable )
						ERR_POST( "'" + mirror.m_MirrorName + "' mirror read check failure" );
					else
						LOG_POST( "Info: '" + mirror.m_MirrorName + "' mirror read check success" );

				}
			}
		}

		if( test_state == check ) test_thread_pool.WaitAll();
	}
}