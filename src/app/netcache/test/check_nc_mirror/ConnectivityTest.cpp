#include "ncbi_pch.hpp"
#include "SimpleThreadPool.h"
#include "ConnectivityTest.h"

///////////////////////////////////////////////////////////////////////

void* CConnectivityTest::Main()
{
	m_Server.m_Connectable = false;

	try	{
		if( m_Server.m_NcServer.GetServerAddress()!="" )	//???
		{
			m_Server.m_NcServer.GetServerInfo();
			m_Server.m_Connectable = true;
		}
	}
	catch( ... )
	{}

	return NULL;
}

void CConnectivityTest::DoMirrorsCheck( CMirrorsPool &mirrors_pool )
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
				mirror.m_Connectable = true;

				for( map< string, shared_ptr< CDomain > >::iterator itr=mirror.m_Domains.begin(); itr!=mirror.m_Domains.end(); itr++ )
				{
					CDomain &domain = *itr->second;
				
					domain.m_Connectable = false;

					for( map< string, shared_ptr< CServer > >::iterator itr=domain.m_Servers.begin(); itr!=domain.m_Servers.end(); itr++ )
					{
						CServer &server = *itr->second;

						if( test_state == check )
						{
							//shared_ptr<CConnectivityTest> connectivity_test( new CConnectivityTest( server ) );
							CConnectivityTest *connectivity_test = new CConnectivityTest( server );

							test_thread_pool.AddThread( connectivity_test );
							
							connectivity_test->Run();
						}
						else
							domain.m_Connectable |= server.m_Connectable;
					};

					if( test_state == finalization )	
						mirror.m_Connectable &= domain.m_Connectable;
				};

				if( test_state == finalization )
				{
					if( ! mirror.m_Connectable )
						ERR_POST( "'" + mirror.m_MirrorName + "' mirror connectability check failure" );
					else
						LOG_POST( "Info: '" + mirror.m_MirrorName + "' mirror connectability check success" );
				}
			}
		}

		if( test_state == check ) test_thread_pool.WaitAll();
	}
}
