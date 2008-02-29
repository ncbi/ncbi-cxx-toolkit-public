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
namespace impl {

CDBConnParamsBase::CDBConnParamsBase(void)
: m_ProtocolVersion(0)
, m_Encoding(eEncoding_Unknown)
, m_ServerType(eUnknown)
, m_Host(0)
, m_PortNumber(0)
, m_IsSecureLogin(false)
, m_IsPooled(false)
, m_IsDoNotConnect(false)
{
}

CDBConnParamsBase::~CDBConnParamsBase(void)
{
}

string CDBConnParamsBase::GetDriverName(void) const
{
    if (m_DriverName.empty()) {
        // Return blessed driver name ...
	switch (GetServerType()) {
	    case eSybaseOpenServer:
	    case eSybaseSQLServer:
#ifdef HAVE_LIBSYBASE
		return "ctlib";
#endif
	    case eMSSqlServer:
		return "ftds";
	    default:
		return "unknown_driver";
	}
    }

    return m_DriverName;
}

Uint4  CDBConnParamsBase::GetProtocolVersion(void) const
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


EEncoding 
CDBConnParamsBase::GetEncoding(void) const
{
    if (m_Encoding == eEncoding_Unknown) {
    	return eEncoding_ISO8859_1;
    }

    return m_Encoding;
}


string 
CDBConnParamsBase::GetServerName(void) const
{
    return m_ServerName;
}

string CDBConnParamsBase::GetDatabaseName(void) const
{
    return m_DatabaseName;
}

string 
CDBConnParamsBase::GetUserName(void) const
{
    if (m_UserName.empty()) {
	return "anyone";
    }

    return m_UserName;
}

string 
CDBConnParamsBase::GetPassword(void) const
{
    if (m_Password.empty()) {
	return "allowed";
    }

    return m_Password;
}

CDBConnParams::EServerType 
CDBConnParamsBase::GetServerType(void) const
{
    if (m_ServerType == eUnknown) {
	// Artificial intelligence ...
	if (   NStr::CompareNocase(GetServerName(), 0, 3, "MS_") == 0
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
    }

    return m_ServerType;
}

Uint4 
CDBConnParamsBase::GetHost(void) const
{
    return m_Host;
}

Uint2 
CDBConnParamsBase::GetPort(void) const
{
    if (!m_PortNumber) {
	// Artificial intelligence ...
	switch (GetServerType()) {
	case eSybaseOpenServer:
	    return 2133U;
	case eSybaseSQLServer:
	    return 2158U;
	case eMSSqlServer:
	    return 1433U;
	default:
	    break;
	}
    }

    return m_PortNumber;
}

CRef<IConnValidator> 
CDBConnParamsBase::GetConnValidator(void) const
{
    return m_Validator;
}

bool 
CDBConnParamsBase::IsSecureLogin(void) const
{
    return m_IsSecureLogin;
}

bool 
CDBConnParamsBase::IsPooled(void) const
{
    return m_IsPooled;
}

bool 
CDBConnParamsBase::IsDoNotConnect(void) const
{
    return m_IsDoNotConnect;
}

string 
CDBConnParamsBase::GetPoolName(void) const
{
    return m_PoolName;
}

}

///////////////////////////////////////////////////////////////////////////////
CDBDefaultConnParams::CDBDefaultConnParams(
        const string&   srv_name,
        const string&   user_name,
        const string&   passwd,
        I_DriverContext::TConnectionMode mode,
        bool            reusable,
        const string&   pool_name)
{
    SetServerName(srv_name);
    SetUserName(user_name);
    SetPassword(passwd);
    SetPoolName(pool_name);
    SetSequreLogin((mode & I_DriverContext::fPasswordEncrypted) != 0);
    SetPooled(reusable);
    SetDoNotConnect((mode & I_DriverContext::fDoNotConnect) != 0);
}


CDBDefaultConnParams::~CDBDefaultConnParams(void)
{
}



///////////////////////////////////////////////////////////////////////////////
CDBUriConnParams::CDBUriConnParams(const string& params)
{
    string::size_type pos = 0;
    string::size_type cur_pos = 0;

    // Check for 'dbapi:' ...
    pos = params.find_first_of(":", pos);
    if (pos == string::npos) {
	DATABASE_DRIVER_ERROR("Invalid database locator format, should start with 'dbapi:'", 20001);
    }

    if (! NStr::StartsWith(params, "dbapi:", NStr::eNocase)) {
	DATABASE_DRIVER_ERROR("Invalid database locator format, should start with 'dbapi:'", 20001);
    }

    cur_pos = pos + 1;

    // Check for driver name ...
    pos = params.find("//", cur_pos);
    if (pos == string::npos) {
	DATABASE_DRIVER_ERROR("Invalid database locator format, should contain driver name", 20001);
    }

    if (pos != cur_pos) {
	string driver_name = params.substr(cur_pos, pos - cur_pos - 1);
	SetDriverName(driver_name);
    }

    cur_pos = pos + 2;

    // Check for user name and password ...
    pos = params.find_first_of(":@", cur_pos);
    if (pos != string::npos) {
	string user_name = params.substr(cur_pos, pos - cur_pos);

	if (params[pos] == '@') {
	    SetUserName(user_name);
	    
	    cur_pos = pos + 1;

	    ParseServer(params, cur_pos);
	} else {
	    // Look ahead, we probably found a host name ...
	    cur_pos = pos + 1;

	    pos = params.find_first_of("@", cur_pos);

	    if (pos != string::npos) {
		// Previous value was an user name ...
		SetUserName(user_name);

		string password = params.substr(cur_pos, pos - cur_pos);
		SetPassword(password);

		cur_pos = pos + 1;
	    }

	    ParseServer(params, cur_pos);
	}
    } else {
	ParseServer(params, cur_pos);
    }

}


CDBUriConnParams::~CDBUriConnParams(void)
{
}


void CDBUriConnParams::ParseServer(const string& params, size_t cur_pos)
{
    string::size_type pos = 0;
    pos = params.find_first_of(":/?", cur_pos);

    if (pos != string::npos) {
	string param_pairs;
	string server_name = params.substr(cur_pos, pos - cur_pos);
	SetServerName(server_name);

	switch (params[pos]) {
	    case ':':
		cur_pos = pos + 1;
		pos = params.find_first_of("/?", cur_pos);

		if (pos != string::npos) {
		    string port = params.substr(cur_pos, pos - cur_pos);
		    SetPort(static_cast<Uint2>(NStr::StringToInt(port)));

		    switch (params[pos]) {
			case '/':
			    cur_pos = pos + 1;
			    ParseSlash(params, cur_pos);

			    break;
			case '?':
			    param_pairs = params.substr(cur_pos);
			    break;
		    }
		} else {
		    string port = params.substr(cur_pos);
		    SetPort(static_cast<Uint2>(NStr::StringToInt(port)));
		}

		break;
	    case '/':
		cur_pos = pos + 1;
		ParseSlash(params, cur_pos);

		break;
	    case '?':
		param_pairs = params.substr(cur_pos);

		break;
	    default:
		break;
	}
    } else {
	string server_name = params.substr(cur_pos);
	SetServerName(server_name);
	// No parameter pairs. We are at the end ...
    }
}

void CDBUriConnParams::ParseSlash(const string& params, size_t cur_pos)
{
    string::size_type pos = 0;
    string param_pairs;

    pos = params.find_first_of("?", cur_pos);
    if (pos != string::npos) {
	string database_name = params.substr(cur_pos, pos - cur_pos);

	SetDatabaseName(database_name);

	cur_pos = pos + 1;
	param_pairs = params.substr(cur_pos);
    } else {
	string database_name = params.substr(cur_pos);

	SetDatabaseName(database_name);
    }
}

void CDBUriConnParams::ParseParamPairs(const string& param_pairs, size_t cur_pos)
{
    vector<string> arr_param;
    string key;
    string value;

    NStr::Tokenize(param_pairs, "&", arr_param);

    ITERATE(vector<string>, it, arr_param) {
	if (NStr::SplitInTwo(*it, "=", key, value)) {
	    NStr::TruncateSpacesInPlace(key);
	    NStr::TruncateSpacesInPlace(value);
	    x_MapPairToParam(NStr::ToUpper(key), value);
	} else {
	    key = *it;
	    NStr::TruncateSpacesInPlace(key);
	    x_MapPairToParam(NStr::ToUpper(key), key);
	}
    }
}


void CDBUriConnParams::x_MapPairToParam(const string& key, const string& value)
{
    // Not ready yet ...
}

///////////////////////////////////////////////////////////////////////////////
CDB_ODBC_ConnParams::CDB_ODBC_ConnParams(const string& params)
{
    vector<string> arr_param;
    string key;
    string value;

    NStr::Tokenize(params, ";", arr_param);

    ITERATE(vector<string>, it, arr_param) {
	if (NStr::SplitInTwo(*it, "=", key, value)) {
	    NStr::TruncateSpacesInPlace(key);
	    NStr::TruncateSpacesInPlace(value);
	    x_MapPairToParam(NStr::ToUpper(key), value);
	} else {
	    key = *it;
	    NStr::TruncateSpacesInPlace(key);
	    x_MapPairToParam(NStr::ToUpper(key), key);
	}
    }
}


void CDB_ODBC_ConnParams::x_MapPairToParam(const string& key, const string& value)
{
    // MS SQL Server related attributes ...
    if (NStr::Equal(key, "SERVER")) {
	SetServerName(value);
    } else if (NStr::Equal(key, "UID")) {
	SetUserName(value);
    } else if (NStr::Equal(key, "PWD")) {
	SetPassword(value);
    } else if (NStr::Equal(key, "DRIVER")) {
	SetDriverName(value);
    } else if (NStr::Equal(key, "DATABASE")) {
	SetDatabaseName(value);
    } else if (NStr::Equal(key, "ADDRESS")) {
	string host;
	string port;

	NStr::SplitInTwo(value, ",", host, port);
	NStr::TruncateSpacesInPlace(host);
	NStr::TruncateSpacesInPlace(port);

	// SetHost(host);
	SetPort(static_cast<Uint2>(NStr::StringToInt(port)));
    }
}


CDB_ODBC_ConnParams::~CDB_ODBC_ConnParams(void)
{
}


///////////////////////////////////////////////////////////////////////////////
CDBConnParamsDelegate::CDBConnParamsDelegate(const CDBConnParams& other)
: m_Other(other)
{
}

CDBConnParamsDelegate::~CDBConnParamsDelegate(void)
{
}


string CDBConnParamsDelegate::GetDriverName(void) const
{
    return m_Other.GetDriverName();
}

Uint4  CDBConnParamsDelegate::GetProtocolVersion(void) const
{
    return m_Other.GetProtocolVersion();
}

EEncoding CDBConnParamsDelegate::GetEncoding(void) const
{
    return m_Other.GetEncoding();
}

string CDBConnParamsDelegate::GetServerName(void) const
{
    return m_Other.GetServerName();
}

string CDBConnParamsDelegate::GetDatabaseName(void) const
{
    return m_Other.GetDatabaseName();
}

string CDBConnParamsDelegate::GetUserName(void) const
{
    return m_Other.GetUserName();
}

string CDBConnParamsDelegate::GetPassword(void) const
{
    return m_Other.GetPassword();
}

CDBConnParams::EServerType 
CDBConnParamsDelegate::GetServerType(void) const
{
    return m_Other.GetServerType();
}

Uint4 CDBConnParamsDelegate::GetHost(void) const
{
    return m_Other.GetHost();
}

Uint2 CDBConnParamsDelegate::GetPort(void) const
{
    return m_Other.GetPort();
}

CRef<IConnValidator> 
CDBConnParamsDelegate::GetConnValidator(void) const
{
    return m_Other.GetConnValidator();
}

bool CDBConnParamsDelegate::IsSecureLogin(void) const
{
    return m_Other.IsSecureLogin();
}


bool CDBConnParamsDelegate::IsPooled(void) const
{
    return m_Other.IsPooled();
}

bool CDBConnParamsDelegate::IsDoNotConnect(void) const
{
    return m_Other.IsDoNotConnect();
}

string CDBConnParamsDelegate::GetPoolName(void) const
{
    return m_Other.GetPoolName();
}


END_NCBI_SCOPE

