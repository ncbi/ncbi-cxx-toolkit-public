#include "ncbi_pch.hpp"
#include "check_nc_mirror.h"
#include "Service.h"

///////////////////////////////////////////////////////////////////////

const string CIcService::tag_cfg_entrie_services_ic = "ic_services";
const string CNcService::tag_cfg_entrie_services_nc = "nc_services";

CService::CService( string name ):
	m_Name( name )
{
	m_NcService = CNetCacheAPI( m_Name, CCheckNcMirrorApp::m_AppName ).GetService();
}

bool CService::test_write_via( string host, unsigned int port )
{
	CNetService stickedNcService( m_NcService );

	stickedNcService.GetServerPool().StickToServer( host, port );

	//???

	return true;
}