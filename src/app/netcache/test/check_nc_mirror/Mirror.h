#ifndef CHECK_NC_MIRROR_MIRROR_HPP
#define CHECK_NC_MIRROR_MIRROR_HPP

#include <corelib/ncbireg.hpp>

USING_NCBI_SCOPE;

#include "CncException.h"
#include "TestParameters.h"
#include "Domain.h"
#include "Service.h"

///////////////////////////////////////////////////////////////////////

class CMirror: public CTestParameters, public CCheckedNode
{
public:
	//static const string m_PrefixCfgEntrieMirror;

	string m_MirrorName;

	bool m_Connectable;

	map< string, shared_ptr< CDomain > > m_Domains;
	map< string, shared_ptr< CService > > m_Services;

	CMirror( const string &section_name, const CTestParameters &default_params, const CNcbiRegistry &config );

	//bool test_connectivity();
	//bool test_read();
	//bool test_write();
};

#endif /* CHECK_NC_MIRROR_MIRROR_HPP */