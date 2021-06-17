#include "ncbi_pch.hpp"
#include <time.h>
#include "NetcacheTest.h"

///////////////////////////////////////////////////////////////////////
/*
void CNetcacheTest::init()
{
	m_LocalHostFullName = CSocketAPI::gethostbyaddr( CSocketAPI::GetLocalHostAddress() );

	char host[256];
	char domain[256];
	char sfx[256];

	if( sscanf( m_LocalHostFullName.c_str(), "%[^.].%[^.].%s", host, domain, sfx ) != 3 )
		NCBI_THROW( 
			CCncException, eConfiguration, 
			"server address '" + m_LocalHostFullName + "' is unrecognizable"
		);

	m_LocalHostName = host;
	m_LocalHostDomain = domain;
}
*/

//CNetcacheTestThreadPoolController CNetcacheTest::s_ThreadPoolController( 0, 16 );
//CThreadPool CNetcacheTest::s_TestThreadPool( 128, &s_ThreadPoolController ); //???
/*
void CNetcacheTestThreadPoolController::OnEvent( EEvent event )
{
	if( event == eSuspend && GetPool()->GetThreadsCount() == 0 )
	{
		// analyse the results
	}
}
*/
bool CNetcacheTest::on_schedule( unsigned freq, unsigned base_freq, time_t systime )
{
	return int( systime / base_freq ) * base_freq <= int( systime / freq ) * freq &&
		   int( systime / base_freq + 1 ) * base_freq > int( systime / freq ) * freq;
}
