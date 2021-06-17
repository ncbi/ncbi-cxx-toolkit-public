#include "ncbi_pch.hpp"
#include <sstream>
#include <iterator>
#include "check_nc_mirror.h"
#include "Domain.h"
#include "Server.h"

///////////////////////////////////////////////////////////////////////

CServer::CServer( string address, const CDomain &domain ):
	m_Domain( domain )
{
	char host[256];
	int port;

	if( sscanf( address.c_str(), "%[^:]:%d", host, &port ) != 2 )
		NCBI_THROW( 
			CCncException, eConfiguration, 
			"server address '" + address + "' is invalid"
		);

	m_Host = string(host) + "." + m_Domain.get_name();
	m_Port = port;

	stringstream ss; ss << m_Host << ":" << m_Port;

	try{
		m_NcClient = CNetCacheAPI( ss.str(), CCheckNcMirrorApp::m_AppName );

		m_NcServer = m_NcClient.GetService().Iterate().GetServer();
	}
	catch(...)
	{}

	//string key;

	//m_NcClient.CreateOStream( key );

	//m_nc_client.GetService().GetServerPool().StickToServer( m_Host, m_Port );
	//shared_ptr<CNcbiOstream> os( m_nc_client.CreateOStream(key) );
}
/*
bool CServer::test_connectivity()
{
	m_Connectable = true;

	try	{
		m_NcServer.GetServerInfo();
	}
	catch( CNetSrvConnException & )	{ 
		m_Connectable = false;
	}

	return m_Connectable;
}

bool CServer::test_read()
{
	bool result = true;

	return result;
}

bool CServer::test_write()
{
	bool result = true;

	if( m_Connectable )
	{
		for_each( m_Domain.m_Mirror.m_Services.begin(), m_Domain.m_Mirror.m_Services.end(), 
			[&]( pair< string, shared_ptr< CService > > pair_service )
		{
			pair_service.second->test_write_via( m_Host, m_Port );
		});
	}

	return result;
}
*/