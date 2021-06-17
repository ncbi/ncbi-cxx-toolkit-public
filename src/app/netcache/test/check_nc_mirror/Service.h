#ifndef CHECK_NC_MIRROR_SERVICE_HPP
#define CHECK_NC_MIRROR_SERVICE_HPP

USING_NCBI_SCOPE;

#include <connect/services/netcache_api.hpp>

///////////////////////////////////////////////////////////////////////

class CService
{
private:
	CNetService m_NcService;

public:
	string m_Name;

	CService( string name );

	bool test_write_via( string host, unsigned int port );
};

class CIcService: public CService
{
public:
	CIcService( string name ): CService( name ) {}

	static const string tag_cfg_entrie_services_ic;
};

class CNcService: public CService
{
public:
	CNcService( string name ): CService( name ) {}

	static const string tag_cfg_entrie_services_nc;
};

#endif /* CHECK_NC_MIRROR_SERVICE_HPP */