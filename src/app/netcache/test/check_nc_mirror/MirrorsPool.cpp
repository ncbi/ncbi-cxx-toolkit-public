#include "ncbi_pch.hpp"
#include "MirrorsPool.h"

///////////////////////////////////////////////////////////////////////

void CMirrorsPool::load( const CNcbiRegistry &config )
{
	string prefix_defaults = "defaults";

	CTestParameters::load( prefix_defaults, config );

	list<string> config_sections;

	config.EnumerateSections( &config_sections );

	for( list<string>::const_iterator itr_sections = config_sections.begin(); itr_sections != config_sections.end(); itr_sections++ )
	{
		string section_name = *itr_sections;

		if( section_name == prefix_defaults ) 
			continue;

		else //if( section_name.find( CMirror::prefix_cfg_entrie_mirror ) == 0 ) 
			m_Mirrors[section_name] = shared_ptr< CMirror >( new CMirror( section_name, *this, config ) );
		/*
		else
			NCBI_THROW( 
				CCncException, eConfiguration, 
				"configuration section name [" + section_name + "] is invalid"
			);
		*/
	}

	return;
}