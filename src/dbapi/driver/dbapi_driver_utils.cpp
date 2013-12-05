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
 * File Description:  Small utility classes common to all drivers.
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/error_codes.hpp>


#ifdef NCBI_OS_MSWIN
    #include <sqlext.h>
#else
    #include <dbapi/driver/odbc/unix_odbc/sqlext.h>
#endif

#include <stdio.h>

#define NCBI_USE_ERRCODE_X   Dbapi_DrvrUtil

BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
CMsgHandlerGuard::CMsgHandlerGuard(I_DriverContext& conn)
: m_Conn(conn)
{
    m_Conn.PushCntxMsgHandler(&m_Handler);
    m_Conn.PushDefConnMsgHandler(&m_Handler);
}

CMsgHandlerGuard::~CMsgHandlerGuard(void)
{
    m_Conn.PopDefConnMsgHandler(&m_Handler);
    m_Conn.PopCntxMsgHandler(&m_Handler);
}


namespace impl
{

string ConvertN2A(Uint4 host)
{
    const unsigned char* b = (const unsigned char*) &host;
    char str[16/*sizeof("255.255.255.255")*/];
    int len;

    len = sprintf(str, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    _ASSERT((size_t) len < sizeof(str));

    return string(str, len);
}

////////////////////////////////////////////////////////////////////////////////
CDBBindedParams::CDBBindedParams(CDB_Params& bindings) 
: m_Bindings(&bindings)
{
}


unsigned int CDBBindedParams::GetNum(void) const
{
    return m_Bindings->NofParams();
}

const string& 
CDBBindedParams::GetName(
        const CDBParamVariant& param, 
        CDBParamVariant::ENameFormat format) const
{
    if (param.IsPositional()) {
        return m_Bindings->GetParamName(param.GetPosition());
    } else {
        return param.GetName(); // XXX - confirm presence?
    }
}

unsigned int CDBBindedParams::GetIndex(const CDBParamVariant& param) const
{
    if (param.IsPositional()) {
        return param.GetPosition(); // XXX - check range?
    } else {
        return m_Bindings->GetParamNum(param.GetName());
    }
}

size_t CDBBindedParams::GetMaxSize(const CDBParamVariant& param) const
{
    DATABASE_DRIVER_ERROR( "Methods GetMaxSize is not implemented yet.", 122002 );
    return 0;
}

EDB_Type CDBBindedParams::GetDataType(const CDBParamVariant& param) const
{
    const CDB_Object* v = GetValue(param);
    return v == NULL ? eDB_UnsupportedType : v->GetType();
}

const CDB_Object* CDBBindedParams::GetValue(const CDBParamVariant& param) const
{
    try {
        int i = GetIndex(param);
        return m_Bindings->GetParam(i);
    } catch (exception&) {
        return NULL;
    }
}

CDBParams::EDirection CDBBindedParams::GetDirection(const CDBParamVariant& param) const
{
    DATABASE_DRIVER_ERROR( "Methods GetDirection is not implemented yet.", 122002 );
    return CDBParams::eIn;
}

CDBParams& CDBBindedParams::Bind(
    const CDBParamVariant& param, 
    CDB_Object* value, 
    bool out_param
    )
{
    if (param.IsPositional()) {
        unsigned int pos = param.GetPosition();

        m_Bindings->BindParam(
                pos, 
                kEmptyStr, 
                value, 
                out_param
                );
    } else {
        m_Bindings->BindParam(
                CDB_Params::kNoParamNumber, 
                param.GetName(), 
                value, 
                out_param
                );
    }

    return *this;
}

CDBParams& CDBBindedParams::Set(
    const CDBParamVariant& param, 
    CDB_Object* value, 
    bool out_param
    )
{
    if (param.IsPositional()) {
        unsigned int pos = param.GetPosition();

        m_Bindings->SetParam(
                pos, 
                kEmptyStr, 
                value, 
                out_param
                );
    } else {
        m_Bindings->SetParam(
                CDB_Params::kNoParamNumber, 
                param.GetName(), 
                value, 
                out_param
                );
    }

    return *this;
}

////////////////////////////////////////////////////////////////////////////////
CCachedRowInfo::SInfo::SInfo(void)
: m_MaxSize(0) 
, m_DataType(eDB_UnsupportedType)
, m_Direction(eOut)
{
}

CCachedRowInfo::SInfo::SInfo(const string& name, 
        size_t max_size, 
        EDB_Type data_type, 
        EDirection direction
        )
: m_Name(name) 
, m_MaxSize(max_size) 
, m_DataType(data_type)
, m_Direction(direction)
{
}

////////////////////////////////////////////////////////////////////////////////
CCachedRowInfo::CCachedRowInfo(impl::CDB_Params& bindings)
: CDBBindedParams(bindings)
, m_Initialized(false)
{
}

CCachedRowInfo::~CCachedRowInfo(void)
{
    return;
}

unsigned int 
CCachedRowInfo::GetNum(void) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    unsigned int num = GetNumInternal(); // For debugging purposes ...
    return num;
}

unsigned int CCachedRowInfo::FindParamPosInternal(const string& name) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    const size_t param_num = m_Info.size();

    for (size_t i = 0; i < param_num; ++i) {
        if (m_Info[i].m_Name == name) {
            return i;
        }
    }

    DATABASE_DRIVER_ERROR("Invalid parameter name " + name, 20001);
    return 0;
}

const string& 
CCachedRowInfo::GetName(
        const CDBParamVariant& param, 
        CDBParamVariant::ENameFormat format) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();

        if (num < GetNumInternal()) {
            return m_Info[num].m_Name;
        }
    } else {
        return m_Info[FindParamPosInternal(param.GetName(format))].m_Name;
    }

    return kEmptyStr;
}


unsigned int 
CCachedRowInfo::GetIndex(const CDBParamVariant& param) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    if (param.IsPositional()) {
        return param.GetPosition();
    } else {
        return FindParamPosInternal(param.GetName());
    }

    DATABASE_DRIVER_ERROR("Parameter name not found: " + param.GetName(), 1);

    return 0;
}


size_t 
CCachedRowInfo::GetMaxSize(const CDBParamVariant& param) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();

        if (num < GetNumInternal()) {
            return m_Info[num].m_MaxSize;
        }
    } else {
        return m_Info[FindParamPosInternal(param.GetName())].m_MaxSize;
    }

    return 0;
}

EDB_Type 
CCachedRowInfo::GetDataType(const CDBParamVariant& param) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();

        if (num < GetNumInternal()) {
            return m_Info[num].m_DataType;
        }
    } else {
        return m_Info[FindParamPosInternal(param.GetName())].m_DataType;
    }

    return eDB_UnsupportedType;
}

CDBParams::EDirection 
CCachedRowInfo::GetDirection(const CDBParamVariant& param) const
{
    if (!IsInitialized()) {
        Initialize();
    }

    if (param.IsPositional()) {
        unsigned int num = param.GetPosition();

        if (num < GetNumInternal()) {
            return m_Info[num].m_Direction;
        }
    } else {
        return m_Info[FindParamPosInternal(param.GetName())].m_Direction;
    }

    return eOut;
}


////////////////////////////////////////////////////////////////////////////////
CRowInfo_SP_SQL_Server::CRowInfo_SP_SQL_Server(
        const string& sp_name,
        impl::CConnection& conn, 
        impl::CDB_Params& bindings
        )
: CCachedRowInfo(bindings)
, m_SPName(sp_name)
, m_Conn(conn)
{
}


CRowInfo_SP_SQL_Server::~CRowInfo_SP_SQL_Server(void)
{
}

void 
CRowInfo_SP_SQL_Server::Initialize(void) const
{
	impl::CConnection& conn = GetCConnection();
	unsigned int step = 0;
    CDBConnParams::EServerType server_type = conn.GetServerType();

	if (server_type == CDBConnParams::eUnknown) {
		server_type = conn.CalculateServerType(server_type);
		++step;
	}

	while (step++ < 3) {
		if (server_type == CDBConnParams::eSybaseSQLServer
            || server_type == CDBConnParams::eMSSqlServer) 
		{
			string sql; 
			string db_name;
			string db_owner;
			string sp_name;
			auto_ptr<CDB_LangCmd> cmd;

			{
				vector<string> arr_param;

				NStr::Tokenize(GetSPName(), ".", arr_param);
				size_t pos = 0;

				switch (arr_param.size()) {
                    case 3:
                        db_name = arr_param[pos++];
                    case 2:
                        db_owner = arr_param[pos++];
                    case 1:
                        sp_name = arr_param[pos++];
                        break;
                    default:
                        DATABASE_DRIVER_ERROR("Invalid format of stored procedure's name: " + GetSPName(), 1);
				}
			}

			if (db_name.empty()) {
				sql = 
                    "SELECT '' from sysobjects WHERE name = @name \n"
                    "UNION \n"
                    "SELECT 'master' from master..sysobjects WHERE name = @name \n"
                    ;

				if (server_type == CDBConnParams::eSybaseSQLServer) {
                    sql +=
                        "UNION \n"
						"SELECT 'sybsystemprocs' from sybsystemprocs..sysobjects WHERE name = @name \n"
						"UNION \n"
						"SELECT 'sybsystemdb' from sybsystemdb..sysobjects WHERE name = @name"
						;
				}

				CMsgHandlerGuard guard(conn);
				cmd.reset(conn.LangCmd(sql));
				CDB_VarChar sp_name_value(sp_name);

				try {
                    cmd->GetBindParams().Bind("@name", &sp_name_value);
                    cmd->Send();

                    while (cmd->HasMoreResults()) {
                        auto_ptr<CDB_Result> res(cmd->Result());

                        if (res.get() != NULL && res->ResultType() == eDB_RowResult ) {
                            CDB_VarChar db_name_value;

                            while (res->Fetch()) {
                                res->GetItem(&db_name_value);

                                if (!db_name_value.IsNULL()) {
                                    db_name = db_name_value.AsString();
                                }
                            }
                        }
                    }
				} catch (const CDB_Exception&) {
                    // Something is wrong. Probably we do not have enough permissios.
                    // We assume that the object is located in the current database. What
                    // else can we do?

                    // Probably, this method was supplied with a wrong
                    // server_type value;
                    if (step < 2) {
                        server_type = conn.CalculateServerType(CDBConnParams::eUnknown);
                        ++step;
                        continue;
                    }
				}
			}

			// auto_ptr<CDB_RPCCmd> sp(conn.RPC("sp_sproc_columns"));
			// We cannot use CDB_RPCCmd here because of recursion ...
			sql = "exec " + db_name + "." + db_owner + ".sp_sproc_columns @procedure_name";
			cmd.reset(conn.LangCmd(sql));
			CDB_VarChar name_value(sp_name);

			try {
                cmd->GetBindParams().Bind("@procedure_name", &name_value);
                cmd->Send();

                while (cmd->HasMoreResults()) {
                    auto_ptr<CDB_Result> res(cmd->Result());

                    if (res.get() != NULL && res->ResultType() == eDB_RowResult ) {
                        CDB_VarChar column_name;
                        CDB_SmallInt column_type;
                        CDB_SmallInt data_type;
                        CDB_Int data_len = 0;

                        while (res->Fetch()) {
                            res->SkipItem();
                            res->SkipItem();
                            res->SkipItem();
                            res->GetItem(&column_name);
                            res->GetItem(&column_type);
                            res->GetItem(&data_type);
                            res->SkipItem();
                            res->SkipItem();
                            res->GetItem(&data_len);

                            // Decode data_type
                            EDB_Type edb_data_type(eDB_UnsupportedType);
                            switch (data_type.Value()) {
                                case SQL_LONGVARCHAR:
                                    edb_data_type = eDB_VarChar;
                                    break;
                                case SQL_BINARY:
                                    edb_data_type = eDB_Binary;
                                    break;
                                case SQL_VARBINARY:
                                    edb_data_type = eDB_VarBinary;
                                    break;
                                case SQL_LONGVARBINARY:
                                    edb_data_type = eDB_Binary;
                                    break;
                                case SQL_BIGINT:
                                    edb_data_type = eDB_BigInt;
                                    break;
                                case SQL_TINYINT:
                                    edb_data_type = eDB_TinyInt;
                                    break;
                                case SQL_BIT:
                                    edb_data_type = eDB_Bit;
                                    break;
                                    // case SQL_GUID:
                                case -9:
                                    edb_data_type = eDB_VarChar;
                                    break;
                                case SQL_CHAR:
                                    edb_data_type = eDB_Char;
                                    break;
                                case SQL_NUMERIC:
                                case SQL_DECIMAL:
                                    edb_data_type = eDB_Numeric;
                                    break;
                                case SQL_INTEGER:
                                    edb_data_type = eDB_Int;
                                    break;
                                case SQL_SMALLINT:
                                    edb_data_type = eDB_SmallInt;
                                    break;
                                case SQL_FLOAT:
                                case SQL_REAL:
                                    edb_data_type = eDB_Float;
                                    break;
                                case SQL_DOUBLE:
                                    edb_data_type = eDB_Double;
                                    break;
                                case SQL_DATETIME:
                                case SQL_TIME:
                                case SQL_TIMESTAMP:
                                    edb_data_type = eDB_DateTime;
                                    break;
                                case SQL_VARCHAR:
                                    edb_data_type = eDB_VarChar;
                                    break;

                                    // case SQL_TYPE_DATE:
                                    // case SQL_TYPE_TIME:
                                    // case SQL_TYPE_TIMESTAMP:
                                default:
                                    edb_data_type = eDB_UnsupportedType;
                            }

                            EDirection direction = CDBParams::eIn;

                            if (column_type.Value() == 2 /*SQL_PARAM_TYPE_OUTPUT*/ ||
                                    column_type.Value() == 4 /*SQL_PARAM_OUTPUT*/ ||
                                    column_type.Value() == 5 /*SQL_RETURN_VALUE*/ ) 
                            {
                                direction = CDBParams::eOut;
                            }

                            Add(column_name.AsString(),
                                    size_t(data_len.Value()),
                                    edb_data_type,
                                    direction
                               );
                        }
                    } // if ...
                } // while HasMoreresults ...

                // Break the loop, Everything seems to be fine. ...
                break;
			} catch (const CDB_Exception&) {
                // Something is wrong ...
                // We may not have permissions to run stored procedures ...

                // Probably, this method was supplied with a wrong
                // server_type value;
                if (step < 2) {
                    server_type = conn.CalculateServerType(CDBConnParams::eUnknown);
                    ++step;
                }
			}
		} // if server_type
	}

    SetInitialized();
}

////////////////////////////////////////////////////////////////////////////////
CMsgHandlerGuard::CMsgHandlerGuard(impl::CConnection& conn)
: m_Conn(conn)
{
    m_Conn.PushMsgHandler(&m_Handler);
}

CMsgHandlerGuard::~CMsgHandlerGuard(void)
{
    m_Conn.PopMsgHandler(&m_Handler);
}


}

END_NCBI_SCOPE


