#ifndef CHECK_NC_MIRROR_TESTPARAMETERS_HPP
#define CHECK_NC_MIRROR_TESTPARAMETERS_HPP

#include <corelib/ncbireg.hpp>

USING_NCBI_SCOPE;

#include "CncException.h"

///////////////////////////////////////////////////////////////////////

class CTestParameters 
{
public:
	unsigned m_BaseTestFreq;
	unsigned m_ReadTestFreq;
	unsigned m_WriteTestFreq;
	unsigned m_Outdated;

protected:
	CTestParameters();
	CTestParameters( const CTestParameters &ini );

	void load( const string &section_name, const CNcbiRegistry &config );
};

#endif /* CHECK_NC_MIRROR_TESTPARAMETERS_HPP */