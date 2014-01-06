#include "ncbi_pch.hpp"
#include "TestParameters.h"

///////////////////////////////////////////////////////////////////////

CTestParameters::CTestParameters(): 
	m_BaseTestFreq( 10 ),
	m_ReadTestFreq( 10 ), 
	m_WriteTestFreq( 300 ), 
	m_Outdated( 500 )
{ }

CTestParameters::CTestParameters( const CTestParameters &ini ):
	m_BaseTestFreq( ini.m_BaseTestFreq ), 
	m_ReadTestFreq( ini.m_ReadTestFreq ), 
	m_WriteTestFreq( ini.m_WriteTestFreq ), 
	m_Outdated( ini.m_Outdated )
{ }

void CTestParameters::load( const string &section_name, const CNcbiRegistry &config )
{
	typedef map<string, unsigned *> map_str_uint;

	const map_str_uint::value_type val_defaults[] = 
	{
		map_str_uint::value_type("base_freq", &m_BaseTestFreq),
		map_str_uint::value_type("read_freq", &m_ReadTestFreq),
		map_str_uint::value_type("write_freq", &m_WriteTestFreq),
		map_str_uint::value_type("outdated", &m_Outdated)
	};

	map_str_uint map_defaults( val_defaults, val_defaults + sizeof val_defaults / sizeof val_defaults[0] );

	for( map_str_uint::const_iterator itr = map_defaults.begin(); itr != map_defaults.end(); itr++ )
	{
		string val_entrie = config.Get( section_name, itr->first );

		if( ! val_entrie.empty() )
			if( sscanf( val_entrie.c_str(), "%d", &(*(itr->second)) ) != 1 ) 
				NCBI_THROW( 
					CCncException, eConvert, 
					"value of configuration parameter '" + string(itr->first) + "' must be of the integer type"
				);
	}
}
