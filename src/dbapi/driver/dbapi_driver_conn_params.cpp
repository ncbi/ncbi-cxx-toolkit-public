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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
CDBDefaultConnParams::CDBDefaultConnParams(
        const string&   srv_name,
        const string&   user_name,
        const string&   passwd,
        const CRef<IConnValidator>& validator,
        Uint4 host,
        Uint2 port,
        I_DriverContext::TConnectionMode mode,
        bool            reusable,
        const string&   pool_name)
: m_ProtocolVersion(0)
, m_ServerName(srv_name)
, m_UserName(user_name)
, m_Password(passwd)
, m_Host(host)
, m_PortNumber(port)
, m_Validator(validator)
, m_PoolName(pool_name)
, m_IsSequreLogin((mode & I_DriverContext::fPasswordEncrypted) != 0)
, m_IsPooled(reusable)
, m_IsDoNotConnect((mode & I_DriverContext::fDoNotConnect) != 0)
{
}

CDBDefaultConnParams::~CDBDefaultConnParams(void)
{
}

string CDBDefaultConnParams::GetDriverName(void) const
{
    if (m_DriverName.empty()) {
        // Return blessed driver name ...
	switch (GetServerType()) {
	    case eMSSqlServer:
		return "ftds";
	    case eSybaseOpenServer:
	    case eSybaseSQLServer:
		return "ctlib";
	    default:
		return "unknown_driver";
	}
    }

    return m_DriverName;
}

Uint4  CDBDefaultConnParams::GetProtocolVersion(void) const
{
    if (!m_ProtocolVersion) {
	// Artificial intelligence ...
	switch (GetServerType()) {
	case eSybaseOpenServer:
	case eSybaseSQLServer:
	    if (NStr::Compare(GetDriverName(), "ftds") == 0) {
                return 42;
	    } else if (NStr::Compare(GetDriverName(), "dblib") == 0) {
		// Due to the bug in the Sybase 12.5 server, DBLIB cannot do
		// BcpIn to it using protocol version other than "100".
		return 100;
	    } else if (NStr::Compare(GetDriverName(), "ftds_odbc") == 0) {
		return 50;
	    } else if (NStr::Compare(GetDriverName(), "ftds63") == 0) {
		// ftds8 works with Sybase databases using protocol v42 only ...
		return 42;
	    } else if (NStr::Compare(GetDriverName(), "ftds_dblib") == 0) {
		// ftds8 works with Sybase databases using protocol v42 only ...
		return 42;
	    }
	default:
	    break;
	}
    }

    return m_ProtocolVersion;
}

string 
CDBDefaultConnParams::GetServerName(void) const
{
    return m_ServerName;
}

string 
CDBDefaultConnParams::GetUserName(void) const
{
    return m_UserName;
}

string 
CDBDefaultConnParams::GetPassword(void) const
{
    return m_Password;
}

CDBConnParams::EServerType 
CDBDefaultConnParams::GetServerType(void) const
{
    // Artificial intelligence ...
    if (   NStr::CompareNocase(GetServerName(), 0, 6, "MS_DEV") == 0
        || NStr::CompareNocase(GetServerName(), 0, 5, "MSSQL") == 0
        || NStr::CompareNocase(GetServerName(), 0, 7, "OAMSDEV") == 0
        || NStr::CompareNocase(GetServerName(), 0, 6, "QMSSQL") == 0
        || NStr::CompareNocase(GetServerName(), 0, 6, "BLASTQ") == 0
        || NStr::CompareNocase(GetServerName(), 0, 4, "GENE") == 0
        || NStr::CompareNocase(GetServerName(), 0, 5, "GPIPE") == 0
        || NStr::CompareNocase(GetServerName(), 0, 7, "MAPVIEW") == 0
        || NStr::CompareNocase(GetServerName(), 0, 5, "MSSNP") == 0
        || NStr::CompareNocase(GetServerName(), 0, 4, "STRC") == 0
        || NStr::CompareNocase(GetServerName(), 0, 4, "SUBS") == 0
        )
    {
        return eMSSqlServer;
    } else if ( NStr::CompareNocase(GetServerName(), "TAPER") == 0
        || NStr::CompareNocase(GetServerName(), "THALBERG") == 0
        || NStr::CompareNocase(GetServerName(), 0, 8, "SCHUMANN") == 0
        || NStr::CompareNocase(GetServerName(), 0, 6, "BARTOK") == 0
        || NStr::CompareNocase(GetServerName(), 0, 8, "SCHUBERT") == 0
        || NStr::CompareNocase(GetServerName(), 0, 8, "SYB_TEST") == 0
        ) 
    {
        return eSybaseSQLServer;
    } else if ( NStr::CompareNocase(GetServerName(), 0, 7, "LINK_OS") == 0 
        || NStr::CompareNocase(GetServerName(), 0, 7, "MAIL_OS") == 0
        || NStr::CompareNocase(GetServerName(), 0, 9, "PUBSEQ_OS") == 0
        || NStr::CompareNocase(GetServerName(), 0, 7, "TEST_OS") == 0
        || NStr::CompareNocase(GetServerName(), 0, 8, "TRACE_OS") == 0
        || NStr::CompareNocase(GetServerName(), 0, 7, "TROS_OS") == 0
        ) 
    {
        return eSybaseOpenServer;
    }

    return eUnknown;
}

Uint4 
CDBDefaultConnParams::GetHost(void) const
{
    return m_Host;
}

Uint2 
CDBDefaultConnParams::GetPort(void) const
{
    return m_PortNumber;
}

CRef<IConnValidator> 
CDBDefaultConnParams::GetConnValidator(void) const
{
    return m_Validator;
}

bool 
CDBDefaultConnParams::IsSequreLogin(void) const
{
    return m_IsSequreLogin;
}

bool 
CDBDefaultConnParams::IsPooled(void) const
{
    return m_IsPooled;
}

bool 
CDBDefaultConnParams::IsDoNotConnect(void) const
{
    return m_IsDoNotConnect;
}

string 
CDBDefaultConnParams::GetPoolName(void) const
{
    return m_PoolName;
}


END_NCBI_SCOPE

