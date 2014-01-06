#ifndef CHECK_NC_MIRROR_SERVER_HPP
#define CHECK_NC_MIRROR_SERVER_HPP

USING_NCBI_SCOPE;

#include <connect/services/netcache_api.hpp>
//#include "CncException.h"

///////////////////////////////////////////////////////////////////////

class CCheckedNode
{
public:
	bool m_Connectable;
	bool m_Writable;
	bool m_Readable;

protected:
	CCheckedNode():
		m_Connectable( false ), m_Writable( false ), m_Readable( false )
	{}
};

class CDomain;

class CServer: public CCheckedNode
{
private:
	const CDomain &m_Domain;

public:
	bool m_Connectable;

	string m_Host;
	unsigned int m_Port;

	CNetCacheAPI m_NcClient;
	CNetServer m_NcServer;

	CServer( string address, const CDomain &domain );

	//bool test_connectivity();
	//bool test_read();
	//bool test_write();

	inline CNetCacheAPI &operator*() { return m_NcClient;	}
};

#endif /* CHECK_NC_MIRROR_SERVER_HPP */