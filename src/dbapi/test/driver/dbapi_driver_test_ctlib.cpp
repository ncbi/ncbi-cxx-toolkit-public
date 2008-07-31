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
 * Author: Sergey Sikorskiy
 *
 * File Description: 
 *      Unit tests for expresiion parsing and evaluation.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////////////
class CTestLOB : public CTL_ITDescriptor
{
public:
	CTestLOB(void);

public:
	void CallSendData(void);
	void Check(void);

private:
	auto_ptr<I_DriverContext>	m_DC;
	auto_ptr<CDB_Connection>	m_Conn;
};

CTestLOB::CTestLOB(void)
{
	DBAPI_RegisterDriver_CTLIB();

	const CDBDefaultConnParams def_params("CLEMENTI", "anyone", "allowed");
	const CCPPToolkitConnParams params(def_params);

	auto_ptr<I_DriverContext> dc(MakeDriverContext(params));
	m_Conn.reset(dc->MakeConnection(params)); 
}

void CTestLOB::CallSendData(void)
{
	const char* data = "test lob data ...";

	m_Desc.iotype = CS_IODATA;
	m_Desc.datatype = CS_IMAGE_TYPE;
	m_Desc.locale = NULL;
	m_Desc.total_txtlen =  sizeof(data) - 1;
	strcpy (m_Desc.name,"asn1");
	m_Desc.namelen = 4;
	m_Desc.timestamplen = CS_TS_SIZE;

	auto_ptr<CDB_SendDataCmd> s_cmd(m_Conn->SendDataCmd (*this, sizeof(data) - 1));
}

void CTestLOB::Check(void)
{
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(LOB)
{
	CTestLOB lob;

	lob.CallSendData();
	lob.Check();
}


