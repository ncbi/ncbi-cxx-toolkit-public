#ifndef CHECK_NC_MIRROR_CHECK_NC_MIRROR_HPP
#define CHECK_NC_MIRROR_CHECK_NC_MIRROR_HPP

USING_NCBI_SCOPE;

#include <corelib/ncbiapp.hpp>
//#include <corelib/ncbiargs.hpp>
//#include <corelib/ncbienv.hpp>
//#include <corelib/ncbireg.hpp>
//#include <corelib/ncbi_system.hpp>
//#include <corelib/ncbistd.hpp>
//#include <corelib/ncbimisc.hpp>
//#include <corelib/ncbithr.hpp>
//#include <corelib/rwstream.hpp>
//#include <corelib/request_ctx.hpp>

//#include <connect/services/netcache_api.hpp>
//#include <connect/ncbi_socket.hpp>
//#include <connect/ncbi_types.h>
//#include <connect/ncbi_core_cxx.hpp>

#include "MirrorsPool.h"

///////////////////////////////////////////////////////////////////////

class CCheckNcMirrorApp: public CNcbiApplication
{
	CMirrorsPool m_MirrorsPool;

public:

	static const string m_AppName;

    void Init(void);

    int Run(void);
};

#endif /* CHECK_NC_MIRROR_CHECK_NC_MIRROR_HPP */