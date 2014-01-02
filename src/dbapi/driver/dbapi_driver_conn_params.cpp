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

#include <corelib/ncbifile.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, dbapi, can_use_kerberos, false);


///////////////////////////////////////////////////////////////////////////////
namespace impl {

CDBConnParamsBase::CDBConnParamsBase(void)
: m_ProtocolVersion(0)
, m_Encoding(eEncoding_Unknown)
, m_ServerType(eUnknown)
, m_Host(0)
, m_PortNumber(0)
{
    SetParam("secure_login", "false");
    SetParam("is_pooled", "false");
    SetParam("do_not_connect", "false");
}

CDBConnParamsBase::~CDBConnParamsBase(void)
{
}

string CDBConnParamsBase::GetDriverName(void) const
{
    if (m_DriverName.empty()) {
        // Return a blessed driver name ...
        switch (GetThis().GetServerType()) {
            case eSybaseOpenServer:
            case eSybaseSQLServer:
#ifdef HAVE_LIBSYBASE
                return "ctlib";
#else
                return "ftds";
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
        const string driver_name = GetThis().GetDriverName();

        // Artificial intelligence ...
        switch (GetThis().GetServerType()) {
            case eSybaseOpenServer:
                if (NStr::Compare(driver_name, "ftds") == 0) {
                    return 125;
                }
            case eSybaseSQLServer:
                // ftds64 can autodetect tds version by itself.

                if (NStr::Compare(driver_name, "dblib") == 0) {
                    // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
                    // BcpIn to it using protocol version other than "100".
                    return 100;
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
    if (m_UserName.empty()  &&  !TDbapi_CanUseKerberos::GetDefault()) {
        return "anyone";
    }

    return m_UserName;
}

string
CDBConnParamsBase::GetPassword(void) const
{
    if (m_Password.empty()  &&  !TDbapi_CanUseKerberos::GetDefault()) {
        return "allowed";
    }

    return m_Password;
}

CDBConnParams::EServerType
CDBConnParamsBase::GetServerType(void) const
{
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
        switch (GetThis().GetServerType()) {
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

const CDBHandlerStack& CDBConnParamsBase::GetOpeningMsgHandlers(void) const
{
    return m_OpeningMsgHandlers;
}

string
CDBConnParamsBase::GetParam(const string& key) const
{
    TUnclassifiedParamMap::const_iterator it = m_UnclassifiedParamMap.find(key);

    if (it != m_UnclassifiedParamMap.end()) {
        return it->second;
    }

    return string();
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

    SetParam(
        "pool_name",
        pool_name
        );

    SetParam(
        "secure_login",
        ((mode & I_DriverContext::fPasswordEncrypted) != 0) ? "true" : "false"
        );

    SetParam(
        "is_pooled",
        reusable ? "true" : "false"
        );

    SetParam(
        "do_not_connect",
        (mode & I_DriverContext::fDoNotConnect) != 0 ? "true" : "false"
        );
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
            SetParam(key, value);
        } else {
            key = *it;
            NStr::TruncateSpacesInPlace(key);
            SetParam(key, key);
        }
    }
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
            x_MapPairToParam(key, value);
        } else {
            key = *it;
            NStr::TruncateSpacesInPlace(key);
            x_MapPairToParam(key, key);
        }
    }
}


void CDB_ODBC_ConnParams::x_MapPairToParam(const string& key, const string& value)
{
    // MS SQL Server related attributes ...
    if (NStr::Equal(key, "SERVER", NStr::eNocase)) {
        SetServerName(value);
    } else if (NStr::Equal(key, "UID", NStr::eNocase)) {
        SetUserName(value);
    } else if (NStr::Equal(key, "PWD", NStr::eNocase)) {
        SetPassword(value);
    } else if (NStr::Equal(key, "DRIVER", NStr::eNocase)) {
        SetDriverName(value);
    } else if (NStr::Equal(key, "DATABASE", NStr::eNocase)) {
        SetDatabaseName(value);
    } else if (NStr::Equal(key, "ADDRESS", NStr::eNocase)) {
        string host;
        string port;

        NStr::SplitInTwo(value, ",", host, port);
        NStr::TruncateSpacesInPlace(host);
        NStr::TruncateSpacesInPlace(port);

        // SetHost(host);
        SetPort(static_cast<Uint2>(NStr::StringToInt(port)));
    } else {
        SetParam(key, value);
    }
}


CDB_ODBC_ConnParams::~CDB_ODBC_ConnParams(void)
{
}


/////////////////////////////////////////////////////////////////////////////
CDBEnvConnParams::CDBEnvConnParams(
        const CDBConnParams& other,
        const string& server_name_env,
        const string& database_name_env,
        const string& user_name_env,
        const string& passwd_env
        )
: CDBConnParamsDelegate(other)
, m_ServerNameEnv(server_name_env)
, m_DatabaseNameEnv(database_name_env)
, m_UserNameEnv(user_name_env)
, m_PasswordEnv(passwd_env)
{
}

CDBEnvConnParams::~CDBEnvConnParams(void)
{
}


string CDBEnvConnParams::GetServerName(void) const
{
    const string& value = m_Env.Get(m_ServerNameEnv);

    if (!value.empty()) {
        return value;
    }

    return CDBConnParamsDelegate::GetServerName();
}

string CDBEnvConnParams::GetDatabaseName(void) const
{
    const string& value = m_Env.Get(m_DatabaseNameEnv);

    if (!value.empty()) {
        return value;
    }

    return CDBConnParamsDelegate::GetDatabaseName();
}

string CDBEnvConnParams::GetUserName(void) const
{
    const string& value = m_Env.Get(m_UserNameEnv);

    if (!value.empty()) {
        return value;
    }

    return CDBConnParamsDelegate::GetUserName();
}

string CDBEnvConnParams::GetPassword(void) const
{
    const string& value = m_Env.Get(m_PasswordEnv);

    if (!value.empty()) {
        return value;
    }

    return CDBConnParamsDelegate::GetPassword();
}



/////////////////////////////////////////////////////////////////////////////
CDBInterfacesFileConnParams::CDBInterfacesFileConnParams(
        const CDBConnParams& other,
        const string& file
        )
: CDBConnParamsDelegate(other)
{
    string file_name;

    if (!file.empty() && CFile(file).Exists()) {
        file_name = file;
    } else {
        const CNcbiEnvironment env;

        // Get it from a default place ...
        file_name = env.Get("SYBASE") + "/interfaces";
        if (!CFile(file_name).Exists()) {
            file_name = env.Get("HOME") + "/.interfaces";
            if (!CFile(file_name).Exists()) {
                return;
            }
        }
    }

    CNcbiIfstream istr(file_name.c_str());

    if (!istr) {
        return;
    }

    string line;
    string key;
    string host_str;
    string port_str;

    vector<string> arr_param;
    enum EState {eInitial, eKeyRead,  eValueRead};
    EState state = eInitial;
    bool tli_format = false;

    while (NcbiGetlineEOL(istr, line)) {
        if (line[0] == '#' || line.empty()) {
            continue;
        } else if (line[0] == '\t') {
            if (state == eKeyRead) {
                NStr::TruncateSpacesInPlace(line);
                arr_param.clear();
                NStr::Tokenize(line, "\t ", arr_param);

                if (NStr::Equal(arr_param[0], "query")) {
                    if (NStr::Equal(arr_param[1], "tli")) {
                        tli_format = true;
                        const string tli_str = arr_param[arr_param.size() - 1];
                        host_str = tli_str.substr(10 - 1, 8);
                        port_str = tli_str.substr(6 - 1, 4);
                    } else {
                        host_str = arr_param[arr_param.size() - 2];
                        port_str = arr_param[arr_param.size() - 1];
                    }

                    state = eValueRead;
                }
            } else {
                // Skip all values except the first one ...
                continue;
            }
        } else {
            if (state == eInitial) {
                key = line;
                NStr::TruncateSpacesInPlace(key);
                state = eKeyRead;
            } else {
                // Error ...
                DATABASE_DRIVER_ERROR("Invalid interfaces file line: " + line, 20001);
            }
        }

        if (state == eValueRead) {
            Uint4 host = 0;
            unsigned char* b = (unsigned char*) &host;

            if (!host_str.empty() && !port_str.empty()) {
                if (tli_format) {
                    b[0] = NStr::StringToUInt(host_str.substr(0, 2), 0, 16);
                    b[1] = NStr::StringToUInt(host_str.substr(2, 2), 0, 16);
                    b[2] = NStr::StringToUInt(host_str.substr(4, 2), 0, 16);
                    b[3] = NStr::StringToUInt(host_str.substr(6, 2), 0, 16);

                    m_Records[key] = SIRecord(host, NStr::StringToUInt(port_str, 0, 16));
                } else {
                    NStr::TruncateSpacesInPlace(host_str);
                    arr_param.clear();
                    NStr::Tokenize(host_str, ".", arr_param);

                    b[0] = NStr::StringToUInt(arr_param[0]);
                    b[1] = NStr::StringToUInt(arr_param[1]);
                    b[2] = NStr::StringToUInt(arr_param[2]);
                    b[3] = NStr::StringToUInt(arr_param[3]);

                    m_Records[key] = SIRecord(host, NStr::StringToUInt(port_str));
                }
            }

            state = eInitial;
        }
    }
}

CDBInterfacesFileConnParams::~CDBInterfacesFileConnParams(void)
{
}


CDBConnParams::EServerType
CDBInterfacesFileConnParams::GetServerType(void) const
{
    const string server_name = GetThis().GetServerName();
    records_type::const_iterator it = m_Records.find(server_name);

    if (it != m_Records.end()) {
        switch(it->second.m_Port) {
            case 2133U:
                return eSybaseOpenServer;
            case 2158U:
                return eSybaseSQLServer;
            case 1433:
                return eMSSqlServer;
            default:
                break;
        }
    }

    return CDBConnParamsDelegate::GetServerType();
}

Uint4 CDBInterfacesFileConnParams::GetHost(void) const
{
    const string server_name = GetThis().GetServerName();
    records_type::const_iterator it = m_Records.find(server_name);

    if (it != m_Records.end()) {
        return it->second.m_Host;
    }

    return CDBConnParamsDelegate::GetHost();
}

Uint2 CDBInterfacesFileConnParams::GetPort(void) const
{
    const string server_name = GetThis().GetServerName();
    records_type::const_iterator it = m_Records.find(server_name);

    if (it != m_Records.end()) {
        return it->second.m_Port;
    }

    return CDBConnParamsDelegate::GetPort();
}


////////////////////////////////////////////////////////////////////////////
CCPPToolkitConnParams::CCPPToolkitConnParams(const CDBConnParams& other)
    : CDBConnParamsDelegate(other)
{
}


CCPPToolkitConnParams::~CCPPToolkitConnParams(void)
{
}


CDBConnParams::EServerType
CCPPToolkitConnParams::GetServerType(void) const
{
    EServerType type = GetServerType(GetThis().GetServerName());
    return (type == eUnknown) ? CDBConnParamsDelegate::GetServerType() : type;
}


CDBConnParams::EServerType
CCPPToolkitConnParams::GetServerType(const CTempString& server_name)
{
    // Artificial intelligence ...
    if (NStr::CompareNocase(server_name, 0, 13, "DBAPI_MS_TEST") == 0
        || NStr::CompareNocase(server_name, 0, 5, "MSSQL") == 0
        || NStr::CompareNocase(server_name, 0, 5, "MSDEV") == 0
        || NStr::CompareNocase(server_name, 0, 7, "OAMSDEV") == 0
        || NStr::CompareNocase(server_name, 0, 6, "QMSSQL") == 0
        || NStr::CompareNocase(server_name, 0, 6, "BLASTQ") == 0
        || NStr::CompareNocase(server_name, 0, 4, "GENE") == 0
        || NStr::CompareNocase(server_name, 0, 5, "GPIPE") == 0
        || NStr::CompareNocase(server_name, 0, 7, "MAPVIEW") == 0
        || NStr::CompareNocase(server_name, 0, 5, "MSSNP") == 0
        )
    {
        return eMSSqlServer;
    } else if ( NStr::CompareNocase(server_name, 0, 5, "GLUCK") == 0
        || NStr::CompareNocase(server_name, 0, 8, "SCHUMANN") == 0
        || NStr::CompareNocase(server_name, 0, 9, "DBAPI_DEV") == 0
        || NStr::CompareNocase(server_name, 0, 8, "SCHUBERT") == 0
        || NStr::CompareNocase(server_name, 0, 9, "DBAPI_SYB") == 0
        )
    {
        return eSybaseSQLServer;
    } else if ( NStr::CompareNocase(server_name, 0, 7, "LINK_OS") == 0
        || NStr::CompareNocase(server_name, 0, 7, "MAIL_OS") == 0
        || NStr::CompareNocase(server_name, 0, 9, "PUBSEQ_OS") == 0
        || NStr::CompareNocase(server_name, 0, 6, "IDFLOW") == 0
        || NStr::CompareNocase(server_name, 0, 6, "IDLOAD") == 0
        || NStr::CompareNocase(server_name, 0, 6, "IDPROD") == 0
        || NStr::CompareNocase(server_name, 0, 4, "IDQA") == 0
        )
    {
        return eSybaseOpenServer;
    }

    return eUnknown;
}



END_NCBI_SCOPE

