/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Alexey Rafanovich
 *
 * File Description:  NetCache mirror check
 *
 */

#include "ncbi_pch.hpp"
#include "check_nc_mirror.h"

#include "NetcacheTest.h"
#include "ConnectivityTest.h"
#include "WriteTest.h"
#include "ReadTest.h"

///////////////////////////////////////////////////////////////////////

const string CCheckNcMirrorApp::m_AppName = "check_nc_mirror";

void CCheckNcMirrorApp::Init()
{
	CNcbiRegistry& reg( CNcbiApplication::Instance()->GetConfig() );

	m_MirrorsPool.load( GetConfig() );

	reg.Set( "netservice_api", "connection_max_retries", "0" );
	reg.Set( "netcache_api", "connection_timeout", "0.001" );
}

int CCheckNcMirrorApp::Run()
{
	//CNetcacheTest::init();
/*
	shared_ptr<CNetcacheTest> val_tests[] = {
		shared_ptr<CNetcacheTest>( new CConnectivityTest() ),
		shared_ptr<CNetcacheTest>( new CReadTest() ),
		shared_ptr<CNetcacheTest>( new CWriteTest() )
	};
	vector< shared_ptr<CNetcacheTest> > tests( val_tests, val_tests + sizeof val_tests / sizeof val_tests[0] );

	for_each( tests.begin(), tests.end(), [&](shared_ptr<CNetcacheTest> test) {
		test->DoMirrorsCheck( m_MirrorsPool );
	});
*/
	CConnectivityTest::DoMirrorsCheck( m_MirrorsPool );
	
	CReadTest::DoMirrorsCheck( m_MirrorsPool );
	
	CWriteTest::DoMirrorsCheck( m_MirrorsPool );

	return 0;
}

int main(int argc, const char* argv[])
{
	return CCheckNcMirrorApp().AppMain(argc, argv, 0, eDS_Default);
}