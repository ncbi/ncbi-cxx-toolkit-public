/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * This simple program illustrates how to use the RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>


USING_NCBI_SCOPE;


class CDriverTest  : public CNcbiApplication
{
private:
	virtual int  Run(void);
};


int CDriverTest::Run()
{
	auto_ptr<IConnection> conSyb;
	auto_ptr<IConnection> conMS;

	{
		DBAPI_RegisterDriver_CTLIB();
		DBAPI_RegisterDriver_FTDS();
	}

	if (true) {
		 IDataSource *ds = 0;
		 CDriverManager &dm = CDriverManager::GetInstance();
		 ds = dm.CreateDs("ftds");
		 conMS.reset(ds->CreateConnection());
		 conMS->Connect("anyone", "allowed", "GPIPE_QA", "GPIPE_INIT");
	}

	{
		IDataSource *ds = 0;
		CDriverManager &dm = CDriverManager::GetInstance();
		ds = dm.CreateDs("ctlib");
		conSyb.reset(ds->CreateConnection());
		conSyb->Connect("anyone", "allowed", "REFTRACK_DEV", "locusXref");
	}

	return 0;
}

int main(int argc, char * argv[])
{
    return CDriverTest().AppMain(argc, argv);
}


