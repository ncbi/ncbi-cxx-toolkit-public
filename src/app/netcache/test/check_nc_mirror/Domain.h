#ifndef CHECK_NC_MIRROR_DOMAIN_HPP
#define CHECK_NC_MIRROR_DOMAIN_HPP

USING_NCBI_SCOPE;

#include <corelib/ncbireg.hpp>

#include "Server.h"

///////////////////////////////////////////////////////////////////////

class CMirror;

class CDomain: public CCheckedNode
{
private:
	const CMirror &m_Mirror;

public:
	static const string prefix_cfg_section_domain;

	map< string, shared_ptr< CServer > > m_Servers;
	string m_Name;

	CDomain( const string &section_name, const string &entrie_name, const CNcbiRegistry &config, const CMirror &mirror );

	inline const string &get_name() const { return m_Name; }

	//bool test_connectivity();
	//bool test_read();
	//bool test_write();
};

#endif /* CHECK_NC_MIRROR_DOMAIN_HPP */