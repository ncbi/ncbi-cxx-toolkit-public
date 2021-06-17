#include "ncbi_pch.hpp"
#include <sstream>
#include <iterator>
#include "Mirror.h"
#include "Domain.h"

///////////////////////////////////////////////////////////////////////

const string CDomain::prefix_cfg_section_domain  = "servers_";

CDomain::CDomain( const string &section_name, const string &entrie_name, const CNcbiRegistry &config, const CMirror &mirror ):
	m_Mirror( mirror )
{
	m_Name = entrie_name.substr( prefix_cfg_section_domain.size() );

	transform(
		istream_iterator<string>( istringstream( config.Get( section_name, entrie_name ) ) ),
		istream_iterator<string>(),
		inserter( m_Servers, m_Servers.begin() ),
		[&](string address)
	{
		return pair< string, shared_ptr< CServer > >( address, new CServer( address, *this ) );
	});
}
/*
bool CDomain::test_connectivity()
{
	m_Connectable = false;

	for_each( m_Servers.begin(), m_Servers.end(), 
		[&]( pair< string, shared_ptr< CServer > > pair_server ) 
	{
		m_Connectable |= pair_server.second->test_connectivity();
	});

	return m_Connectable;
}

bool CDomain::test_read()
{
	bool result = true;

	for_each( m_Servers.begin(), m_Servers.end(), 
		[&]( pair< string, shared_ptr< CServer > > pair_server ) 
	{
		result &= pair_server.second->test_read();
	});

	return result;
}

bool CDomain::test_write()
{
	bool result = true;

	for_each( m_Servers.begin(), m_Servers.end(), 
		[&]( pair< string, shared_ptr< CServer > > pair_server ) 
	{
		result &= pair_server.second->test_write();
	});

	return result;
}
*/