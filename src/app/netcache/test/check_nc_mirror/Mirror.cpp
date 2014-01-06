#include "ncbi_pch.hpp"
#include <sstream>
#include <iterator>
#include "Mirror.h"

///////////////////////////////////////////////////////////////////////

//const string CMirror::prefix_cfg_entrie_mirror = "mirror_";

template< class T > pair< string, shared_ptr< CService > > create_service_ref( string name )
{ 
	return pair< string, shared_ptr< CService > >( name, new T( name ) ); 
}

CMirror::CMirror( const string &section_name, const CTestParameters &default_params, const CNcbiRegistry &config ):
	CTestParameters( default_params )
{
	//_ASSERT( section_name.find( CMirror::prefix_cfg_entrie_mirror ) == 0 );

	m_MirrorName = section_name; //section_name.substr( CMirror::prefix_cfg_entrie_mirror.size() );

	CTestParameters::load( section_name, config );

	list<string> config_entries;

	config.EnumerateEntries( /*prefix_cfg_entrie_mirror + */m_MirrorName, &config_entries );

	for( list<string>::const_iterator itr_entries = config_entries.begin(); itr_entries != config_entries.end(); itr_entries++ )
	{
		string entrie_name = *itr_entries;

		if( entrie_name.find( CDomain::prefix_cfg_section_domain ) == 0 )
		{
			shared_ptr< CDomain > ptr_domain( new CDomain( section_name, entrie_name, config, *this ) );

			m_Domains[ ptr_domain->get_name() ] = ptr_domain;
		}
		
		else if( entrie_name == CIcService::tag_cfg_entrie_services_ic )
			transform(
				istream_iterator<string>( istringstream( config.Get( section_name, entrie_name ) ) ),
				istream_iterator<string>(),
				inserter( m_Services, m_Services.begin() ),
				create_service_ref<CIcService>
			);

		else if( entrie_name == CNcService::tag_cfg_entrie_services_nc )
			transform(
				istream_iterator<string>( istringstream( config.Get( section_name, entrie_name ) ) ),
				istream_iterator<string>(),
				inserter( m_Services, m_Services.begin() ),
				create_service_ref<CNcService>
			);
//???
/*
		else
			NCBI_THROW( 
				CCncException, eConfiguration, 
				"configuration entrie '" + entrie_name + "' is invalid"
			);
*/
	}
}
/*
bool CMirror::test_connectivity()
{
	bool result = true;

	for_each( m_Domains.begin(), m_Domains.end(), 
		[&]( pair< string, shared_ptr< CDomain > > pair_domain )
	{
		result &= pair_domain.second->test_connectivity();
	});

	if( ! result )
		ERR_POST( "'" + m_MirrorName + "' mirror connectivity check failure" );
	else
		LOG_POST( "Info: '" + m_MirrorName + "' mirror connectivity check success" );

	return result;
}

bool CMirror::test_read()
{
	bool result = true;

	for_each( m_Domains.begin(), m_Domains.end(), 
		[&]( pair< string, shared_ptr< CDomain > > pair_domain )
	{
		result &= pair_domain.second->test_read();
	});

	if( ! result )
		ERR_POST( "'" + m_MirrorName + "' mirror read check failure" );
	else
		LOG_POST( "Info: '" + m_MirrorName + "' mirror read check success" );

	return true;
}

bool CMirror::test_write()
{
	bool result = true;

	for_each( m_Domains.begin(), m_Domains.end(), 
		[&]( pair< string, shared_ptr< CDomain > > pair_domain )
	{
		result &= pair_domain.second->test_write();
	});

	if( ! result )
		ERR_POST( "'" + m_MirrorName + "' mirror write check failure" );
	else
		LOG_POST( "Info: '" + m_MirrorName + "' mirror write check success" );

	return true;
}
*/