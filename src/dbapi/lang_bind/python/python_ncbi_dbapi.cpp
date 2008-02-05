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
* Status: *Initial*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_safe_static.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <dbapi/error_codes.hpp>

#include "python_ncbi_dbapi.hpp"
#include "pythonpp/pythonpp_pdt.hpp"
#if PY_VERSION_HEX >= 0x02040000
#  include "pythonpp/pythonpp_date.hpp"
#endif
#include "../../ds_impl.hpp"

#if defined(NCBI_OS_CYGWIN)
#include <corelib/ncbicfg.h>
#elif !defined(NCBI_OS_MSWIN)
#include <dlfcn.h>
#endif

//////////////////////////////////////////////////////////////////////////////
// Compatibility macros
//
// From Python 2.2 to 2.3, the way to export the module init function
// has changed. These macros keep the code compatible to both ways.
//
#if PY_VERSION_HEX >= 0x02030000
#  define PYDBAPI_MODINIT_FUNC(name)         PyMODINIT_FUNC name(void)
#else
#  define PYDBAPI_MODINIT_FUNC(name)         DL_EXPORT(void) name(void)
#endif


#define NCBI_USE_ERRCODE_X   Dbapi_Python

BEGIN_NCBI_SCOPE

namespace python
{

//////////////////////////////////////////////////////////////////////////////
CBinary::CBinary(void)
{
    PrepareForPython(this);
}

CBinary::~CBinary(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CNumber::CNumber(void)
{
    PrepareForPython(this);
}

CNumber::~CNumber(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CRowID::CRowID(void)
{
    PrepareForPython(this);
}

CRowID::~CRowID(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CParamFmt::CParamFmt(TFormat user_fmt, TFormat drv_fmt)
: m_UserFmt(user_fmt)
, m_DrvFmt(drv_fmt)
{
}

const char*
CParamFmt::GetName(TFormat fmt)
{
    switch (fmt) {
    case eTSQL:
        return "TSQL";
    case eQmark:
        return "qmark";
    case eNumeric:
        return "numeric";
    case eNamed:
        return "named";
    case eFormat:
        return "format";
    case ePyFormat:
        return "pyformat";
    }

    return "unknown";
}

//////////////////////////////////////////////////////////////////////////////
void
CStmtStr::SetStr(const string& str,
                 EStatementType default_type,
                 const CParamFmt& fmt
                 )
{
    m_StmType = RetrieveStatementType(str, default_type);

    /* Do not delete this code ...
    static char const* space_characters = " \t\n";

    if ( GetType() == estFunction ) {
        // Cut off the "EXECUTE" prefix if any ...

        string::size_type pos;
        string str_uc = str;

        NStr::ToUpper(str_uc);
        pos = str_uc.find_first_not_of(space_characters);
        if (pos != string::npos) {
            if (str_uc.compare(pos, sizeof("EXEC") - 1, "EXEC") == 0) {
                // We have the "EXECUTE" prefix ...
                pos = str_uc.find_first_of(space_characters, pos);
                if (pos != string::npos) {
                    pos = str_uc.find_first_not_of(space_characters, pos);
                    if (pos != string::npos) {
                        m_StmtStr = str.substr(pos);
                        return;
                    }
                }
            }
        }
    }
    */

    m_StmtStr = str;

    if (fmt.GetDriverFmt() != fmt.GetUserFmt()) {
        // Replace parameters ...
        if (fmt.GetUserFmt() == CParamFmt::eQmark) {
            if (fmt.GetDriverFmt() == CParamFmt::eNumeric) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;

                if ((pos = m_StmtStr.find('?', pos)) != string::npos) {
                    string tmp_stmt;
                    int pos_num = 1;

                    while (pos != string::npos) {
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += ":" + NStr::IntToString(pos_num++);
                        prev_pos = pos + 1;

                        pos = m_StmtStr.find('?', prev_pos);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            } else if (fmt.GetDriverFmt() == CParamFmt::eTSQL) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;

                if ((pos = m_StmtStr.find('?', pos)) != string::npos) {
                    string tmp_stmt;
                    int pos_num = 1;

                    while (pos != string::npos) {
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += "@" + NStr::IntToString(pos_num++);
                        prev_pos = pos + 1;

                        pos = m_StmtStr.find('?', prev_pos);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            }
        } else if (fmt.GetUserFmt() == CParamFmt::eNumeric) {
            if (fmt.GetDriverFmt() == CParamFmt::eQmark) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_numeric(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;

                    while (pos != string::npos) {
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += "?";
                        prev_pos = pos + param_len;

                        pos = find_numeric(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            }
        } else if (fmt.GetUserFmt() == CParamFmt::eNamed) {
            if (fmt.GetDriverFmt() == CParamFmt::eQmark) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_named(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;

                    while (pos != string::npos) {
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += "?";
                        prev_pos = pos + param_len;

                        pos = find_named(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            } else if (fmt.GetDriverFmt() == CParamFmt::eNumeric) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_named(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;
                    int pos_num = 1;
                    typedef map<string, string> name_map_t;
                    name_map_t name2num;

                    while (pos != string::npos) {
                        string param_name = m_StmtStr.substr(pos + 1, param_len - 1);
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += ":";

                        // Get number ...
                        name_map_t::iterator it = name2num.find(param_name);
                        if (it == name2num.end()) {
                            it = name2num.insert(
                                name_map_t::value_type(
                                    param_name,
                                    NStr::IntToString(pos_num++)
                                    )
                                ).first;
                        }
                        tmp_stmt += it->second;

                        prev_pos = pos + param_len;

                        pos = find_named(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            } else if (fmt.GetDriverFmt() == CParamFmt::eTSQL) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_named(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;

                    while (pos != string::npos) {
                        string param_name = m_StmtStr.substr(pos + 1, param_len - 1);
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += "@" + param_name;
                        prev_pos = pos + param_len;

                        pos = find_named(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            }
        } else if (fmt.GetUserFmt() == CParamFmt::eTSQL) {
            if (fmt.GetDriverFmt() == CParamFmt::eQmark) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_TSQL(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;

                    while (pos != string::npos) {
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += "?";
                        prev_pos = pos + param_len;

                        pos = find_TSQL(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            } else if (fmt.GetDriverFmt() == CParamFmt::eNumeric) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_TSQL(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;
                    int pos_num = 1;
                    typedef map<string, string> name_map_t;
                    name_map_t name2num;

                    while (pos != string::npos) {
                        string param_name = m_StmtStr.substr(pos + 1, param_len - 1);
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += ":";

                        // Get number ...
                        name_map_t::iterator it = name2num.find(param_name);
                        if (it == name2num.end()) {
                            it = name2num.insert(
                                name_map_t::value_type(
                                    param_name,
                                    NStr::IntToString(pos_num++)
                                    )
                                ).first;
                        }
                        tmp_stmt += it->second;

                        prev_pos = pos + param_len;

                        pos = find_TSQL(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            } else if (fmt.GetDriverFmt() == CParamFmt::eNamed) {
                string::size_type pos = 0;
                string::size_type prev_pos = pos;
                int param_len = 0;

                if ((pos = find_TSQL(m_StmtStr, pos, param_len)) != string::npos) {
                    string tmp_stmt;

                    while (pos != string::npos) {
                        string param_name = m_StmtStr.substr(pos + 1, param_len - 1);
                        tmp_stmt += m_StmtStr.substr(prev_pos, pos - prev_pos);
                        tmp_stmt += ":" + param_name;
                        prev_pos = pos + param_len;

                        pos = find_TSQL(m_StmtStr, prev_pos, param_len);
                    }

                    tmp_stmt += m_StmtStr.substr(prev_pos);
                    m_StmtStr = tmp_stmt;
                }

                return;
            }
        }

        string err = "Cannot convert '";
        err += CParamFmt::GetName(fmt.GetUserFmt());
        err += "' parameter format to '";
        err += CParamFmt::GetName(fmt.GetDriverFmt());
        err += "'";
        throw CInterfaceError(err);
    }
}


string::size_type
CStmtStr::find_numeric(const string& str,
                       string::size_type offset,
                       int& param_len
                       )
{
    string::size_type pos = 0;
    static const char* num_characters = "0123456789";

    pos = str.find(':', offset);
    if ((pos != string::npos) && (pos + 1 != string::npos)) {
        string::size_type tmp_pos = 0;

        tmp_pos = str.find_first_not_of(num_characters, pos + 1);
        if (tmp_pos != string::npos) {
            // We've got the end of the number ...
            param_len = tmp_pos - pos;
        } else if (str.find_first_of(num_characters, pos + 1) == pos + 1) {
            // Number till the end of the string ...
            param_len = str.size() - pos;
        }
    }

    return pos;
}


string::size_type
CStmtStr::find_named(const string& str,
                     string::size_type offset,
                     int& param_len
                     )
{
    string::size_type pos = 0;
    static char const* sep_characters = " \t\n,.()-+<>=";

    pos = str.find(':', offset);
    if ((pos != string::npos) && (pos + 1 != string::npos)) {
        string::size_type tmp_pos = 0;

        tmp_pos = str.find_first_of(sep_characters, pos + 1);
        if (tmp_pos != string::npos) {
            // We've got the end of the number ...
            param_len = tmp_pos - pos;
        } else if ((str[pos + 1] >= 'A' && str[pos + 1] <= 'Z') ||
                   (str[pos + 1] >= 'a' && str[pos + 1] <= 'z')
                   ) {
            // Number till the end of the string ...
            param_len = str.size() - pos;
        }
    }

    return pos;
}

string::size_type
CStmtStr::find_TSQL(const string& str,
                    string::size_type offset,
                    int& param_len
                    )
{
    string::size_type pos = 0;
    static char const* sep_characters = " \t\n,.()-+<>=";

    pos = str.find('@', offset);
    if ((pos != string::npos) && (pos + 1 != string::npos)) {
        string::size_type tmp_pos = 0;

        tmp_pos = str.find_first_of(sep_characters, pos + 1);
        if (tmp_pos != string::npos) {
            // We've got the end of the number ...
            param_len = tmp_pos - pos;
        } else if ((str[pos + 1] >= 'A' && str[pos + 1] <= 'Z') ||
                   (str[pos + 1] >= 'a' && str[pos + 1] <= 'z')
                   ) {
            // Number till the end of the string ...
            param_len = str.size() - pos;
        }
    }

    return pos;
}

//////////////////////////////////////////////////////////////////////////////
CConnParam::CConnParam(
    const string& driver_name,
    const string& db_type,
    const string& server_name,
    const string& db_name,
    const string& user_name,
    const string& user_pswd
    )
: m_driver_name(driver_name)
, m_db_type(db_type)
, m_server_name(server_name)
, m_db_name(db_name)
, m_user_name(user_name)
, m_user_pswd(user_pswd)
, m_ServerType(eUnknown)
{
    // Setup a server type ...
    string db_type_uc = db_type;
    NStr::ToUpper(db_type_uc);

    if ( db_type_uc == "SYBASE" ) {
        m_ServerType = eSybase;
    } else if ( db_type_uc == "ORACLE" ) {
        m_ServerType = eOracle;
    } else if ( db_type_uc == "MYSQL" ) {
        m_ServerType = eMySql;
    } else if ( db_type_uc == "SQLITE" ) {
        m_ServerType = eSqlite;
    } else if ( db_type_uc == "MSSQL" ||
            db_type_uc == "MS_SQL" ||
            db_type_uc == "MS SQL") {
        m_ServerType = eMsSql;
    }

    if ( GetDriverName() == "ctlib" ) {
        m_DatabaseParameters["version"] = "125";
    } else if ( GetDriverName() == "dblib"  &&  GetServerType() == eSybase ) {
        // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
        // BcpIn to it using protocol version other than "100".
        m_DatabaseParameters["version"] = "125";
    } else if ( (GetDriverName() == "ftds" || GetDriverName() == "ftds63")  &&
                GetServerType() == eSybase ) {
        // ftds forks with Sybase databases using protocol v42 only ...
        m_DatabaseParameters["version"] = "42";
        // m_DatabaseParameters["version"] = "46";
    }

}

CConnParam::~CConnParam(void)
{
}

/* Future development
//////////////////////////////////////////////////////////////////////////////
struct DataSourceDeleter
{
    /// Default delete function.
    static void Delete(ncbi::IDataSource* const object)
    {
        CDriverManager::DeleteDs( object );
    }
};

//////////////////////////////////////////////////////////////////////////////
class CDataSourcePool
{
public:
    CDataSourcePool(void);
    ~CDataSourcePool(void);

public:
    static CDataSourcePool& GetInstance(void);

public:
    IDataSource& GetDataSource(
        const string& driver_name,
        const TPluginManagerParamTree* const params = NULL);

private:
    typedef CPluginManager<I_DriverContext> TContextManager;
    typedef CPluginManagerGetter<I_DriverContext> TContextManagerStore;
    typedef map<string, AutoPtr<IDataSource, DataSourceDeleter> > TDSMap;

    CRef<TContextManager>   m_ContextManager;
    TDSMap                  m_DataSourceMap;
};

CDataSourcePool::CDataSourcePool(void)
{
    m_ContextManager.Reset( TContextManagerStore::Get() );
    _ASSERT( m_ContextManager );

#if defined(NCBI_OS_MSWIN)
    // Add an additional search path ...
#endif
}

CDataSourcePool::~CDataSourcePool(void)
{
}

void
DataSourceCleanup(void* ptr)
{
    CDriverManager::DeleteDs( static_cast<ncbi::IDataSource *const>(ptr) );
}

CDataSourcePool&
CDataSourcePool::GetInstance(void)
{
    static CSafeStaticPtr<CDataSourcePool> ds_pool;

    return *ds_pool;
}

IDataSource&
CDataSourcePool::GetDataSource(
    const string& driver_name,
    const TPluginManagerParamTree* const params)
{
    TDSMap::const_iterator citer = m_DataSourceMap.find( driver_name );

    if ( citer != m_DataSourceMap.end() ) {
        return *citer->second;
    }

    // Build a new context ...
    I_DriverContext* drv = NULL;

    try {
        drv = m_ContextManager->CreateInstance(
            driver_name,
            NCBI_INTERFACE_VERSION(I_DriverContext),
            params
            );
        _ASSERT( drv );
    }
    catch( const CPluginManagerException& e ) {
        throw CDatabaseError( e.GetMsg() );
    }
    catch ( const exception& e ) {
        throw CDatabaseError( driver_name + " is not available :: " + e.what() );
    }
    catch( ... ) {
        throw CDatabaseError( driver_name + " was unable to load due an unknown error" );
    }

    // Store new value
    IDataSource* ds = CDriverManager::CreateDs( drv );
    m_DataSourceMap[driver_name] = ds;

    return *ds;
}
    */

//////////////////////////////////////////////////////////////////////////////
static
string RetrieveModuleFileName(void)
{
    string file_name;

#if defined(NCBI_OS_CYGWIN)
    // no dladdr; just return the standard location
    file_name = NCBI_GetDefaultRunpath() + string("libpython_ncbi_dbapi.a");

#elif defined(NCBI_OS_MSWIN)
    // Add an additional search path ...
    const DWORD buff_size = 1024;
    DWORD cur_size = 0;
    char buff[buff_size];
    HMODULE mh = NULL;
    const char* module_name = NULL;

// #ifdef NDEBUG
//     module_name = "python_ncbi_dbapi.pyd";
// #else
//     module_name = "python_ncbi_dbapi_d.pyd";
// #endif

    // Get module handle ...
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery((const void*)RetrieveModuleFileName, &mbi, sizeof(mbi));
    mh = (HINSTANCE)mbi.AllocationBase;

//     if ( mh = GetModuleHandle( module_name ) ) {
    if (mh) {
        if ( cur_size = GetModuleFileName( mh, buff, buff_size ) ) {
            if ( cur_size < buff_size ) {
                file_name = buff;
            }
        }
    }

#else

    ::Dl_info dli;

    // if (::dladdr(&ncbi::python::CConnection::CConnection, &dli) != 0) {
    void* method_ptr = (void*)RetrieveModuleFileName;
    if (::dladdr(method_ptr, &dli) != 0) {
        file_name = dli.dli_fname;
    }

#endif

    return file_name;
}

//////////////////////////////////////////////////////////////////////////////
CConnection::CConnection(
    const CConnParam& conn_param,
    EConnectionMode conn_mode
    )
: m_ConnParam(conn_param)
, m_DM(CDriverManager::GetInstance())
, m_DS(NULL)
, m_DefTransaction( NULL )
, m_ConnectionMode( conn_mode )
{
//--------------------------------------------------
//     CFile file(RetrieveModuleFileName());
//     m_ModuleName = file.GetBase();
//     m_DM.AddDllSearchPath(file.GetDir().c_str());
//--------------------------------------------------

    try {
        m_DS = m_DM.CreateDs( m_ConnParam.GetDriverName(), &m_ConnParam.GetDatabaseParameters() );
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

//    m_DS = &CDataSourcePool::GetInstance().GetDataSource( m_ConnParam.GetDriverName() );

    PrepareForPython(this);

    try {
        // Create a default transaction ...
        // m_DefTransaction = new CTransaction(this, pythonpp::eBorrowed, m_ConnectionMode);
        m_DefTransaction = new CTransaction(this, pythonpp::eAcquired, m_ConnectionMode);
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }
}

CConnection::~CConnection(void)
{
    try {
        // This DecRefCount caused a lot of problems for some reason ...
        DecRefCount( m_DefTransaction );
        // delete m_DefTransaction;

        _ASSERT( m_TransList.empty() );

        _ASSERT(m_DS);

        // DO NOT destroy data source because there is only one data source per
        // driver in Kholodov's API.
        // Destroying data source will cause closed and reopened connection to
        // crash ...
        // m_DM.DestroyDs(m_DS);
        m_DS = NULL;                        // ;-)
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}

IConnection*
CConnection::MakeDBConnection(void) const
{
    _ASSERT(m_DS);
    // !!! eTakeOwnership !!!
    IConnection* connection = m_DS->CreateConnection( eTakeOwnership );
//     connection->SetMode(IConnection::eBulkInsert);
    connection->Connect(
        m_ConnParam.GetUserName(),
        m_ConnParam.GetUserPswd(),
        m_ConnParam.GetServerName(),
        m_ConnParam.GetDBName()
        );
    return connection;
}

CTransaction*
CConnection::CreateTransaction(void)
{
    CTransaction* trans = NULL;

    trans = new CTransaction(this, pythonpp::eOwned, m_ConnectionMode);

    m_TransList.insert(trans);
    return trans;
}

void
CConnection::DestroyTransaction(CTransaction* trans)
{
    if (m_DefTransaction == trans) {
        m_DefTransaction = NULL;
    }

    // Python will take care of the object deallocation ...
    m_TransList.erase(trans);
}

pythonpp::CObject
CConnection::close(const pythonpp::CTuple& args)
{
    TTransList::const_iterator citer;
    TTransList::const_iterator cend = m_TransList.end();

    for ( citer = m_TransList.begin(); citer != cend; ++citer) {
        (*citer)->close(args);
    }
    return GetDefaultTransaction().close(args);
}

pythonpp::CObject
CConnection::cursor(const pythonpp::CTuple& args)
{
    return GetDefaultTransaction().cursor(args);
}

pythonpp::CObject
CConnection::commit(const pythonpp::CTuple& args)
{
    return GetDefaultTransaction().commit(args);
}

pythonpp::CObject
CConnection::rollback(const pythonpp::CTuple& args)
{
    return GetDefaultTransaction().rollback(args);
}

pythonpp::CObject
CConnection::transaction(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(CreateTransaction(), pythonpp::eTakeOwnership);
}

//////////////////////////////////////////////////////////////////////////////
CSelectConnPool::CSelectConnPool(CTransaction* trans, size_t size)
: m_Transaction(trans)
, m_PoolSize(size)
{
}

IConnection*
CSelectConnPool::Create(void)
{
    IConnection* db_conn = NULL;

    if ( m_ConnPool.empty() ) {
        db_conn = GetConnection().MakeDBConnection();
        m_ConnList.insert(db_conn);
    } else {
        db_conn = *m_ConnPool.begin();
        m_ConnPool.erase(m_ConnPool.begin());
    }

    return db_conn;
}

void
CSelectConnPool::Destroy(IConnection* db_conn)
{
    if ( m_ConnPool.size() < m_PoolSize ) {
        m_ConnPool.insert(db_conn);
    } else {
        if ( m_ConnList.erase(db_conn) == 0) {
            _ASSERT(false);
        }
        delete db_conn;
    }
}

void
CSelectConnPool::Clear(void)
{
    if ( !Empty() ) {
        throw CInternalError("Unable to close a transaction. There are open cursors in use.");
    }

    if ( !m_ConnList.empty() )
    {
        // Close all open connections ...
        TConnectionList::const_iterator citer;
        TConnectionList::const_iterator cend = m_ConnList.end();

        // Delete all allocated "SELECT" database connections ...
        for ( citer = m_ConnList.begin(); citer != cend; ++citer) {
            delete *citer;
        }
        m_ConnList.clear();
        m_ConnPool.clear();
    }
}

//////////////////////////////////////////////////////////////////////////////
CDMLConnPool::CDMLConnPool(
    CTransaction* trans,
    ETransType trans_type
    )
: m_Transaction( trans )
, m_NumOfActive( 0 )
, m_Started( false )
, m_TransType( trans_type )
{
}

IConnection*
CDMLConnPool::Create(void)
{
    // Delayed connection creation ...
    if ( m_DMLConnection.get() == NULL ) {
        m_DMLConnection.reset( GetConnection().MakeDBConnection() );
        _ASSERT( m_LocalStmt.get() == NULL );

        if ( m_TransType == eImplicitTrans ) {
            m_LocalStmt.reset( m_DMLConnection->GetStatement() );
            // Begin transaction ...
            GetLocalStmt().ExecuteUpdate( "BEGIN TRANSACTION" );
            m_Started = true;
        }
    }

    ++m_NumOfActive;
    return m_DMLConnection.get();
}

void
CDMLConnPool::Destroy(IConnection* db_conn)
{
    --m_NumOfActive;
}

void
CDMLConnPool::Clear(void)
{
    if ( !Empty() ) {
        throw CInternalError("Unable to close a transaction. There are open cursors in use.");
    }

    // Close the DML connection ...
    m_LocalStmt.reset();
    m_DMLConnection.reset();
    m_Started = false;
}

IStatement&
CDMLConnPool::GetLocalStmt(void) const
{
    _ASSERT(m_LocalStmt.get() != NULL);
    return *m_LocalStmt;
}

void
CDMLConnPool::commit(void) const
{
    if (
        m_TransType == eImplicitTrans &&
        m_Started &&
        m_DMLConnection.get() != NULL &&
        m_DMLConnection->IsAlive()
    ) {
        try {
            GetLocalStmt().ExecuteUpdate( "COMMIT TRANSACTION" );
            GetLocalStmt().ExecuteUpdate( "BEGIN TRANSACTION" );
        }
        catch(const CDB_Exception& e) {
            throw CDatabaseError(e.GetMsg());
        }
    }
}

void
CDMLConnPool::rollback(void) const
{
    if (
        m_TransType == eImplicitTrans &&
        m_Started &&
        m_DMLConnection.get() != NULL &&
        m_DMLConnection->IsAlive()
    ) {
        try {
            GetLocalStmt().ExecuteUpdate( "ROLLBACK TRANSACTION" );
            GetLocalStmt().ExecuteUpdate( "BEGIN TRANSACTION" );
        }
        catch(const CDB_Exception& e) {
            throw CDatabaseError(e.GetMsg());
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
CTransaction::CTransaction(
    CConnection* conn,
    pythonpp::EOwnershipFuture ownnership,
    EConnectionMode conn_mode
    )
: m_ParentConnection( conn )
, m_DMLConnPool( this, (conn_mode == eSimpleMode ? eExplicitTrans : eImplicitTrans) )
, m_SelectConnPool( this )
, m_ConnectionMode( conn_mode )
{
    if ( conn == NULL ) {
        throw CInternalError("Invalid CConnection object");
    }

    if ( ownnership != pythonpp::eBorrowed ) {
        m_PythonConnection = conn;
    }
    PrepareForPython(this);
}

CTransaction::~CTransaction(void)
{
    try {
        CloseInternal();

        // Unregister this transaction with the parent connection ...
        GetParentConnection().DestroyTransaction(this);
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}

pythonpp::CObject
CTransaction::close(const pythonpp::CTuple& args)
{
    CloseInternal();

    // Unregister this transaction with the parent connection ...
    // I'm not absolutely shure about this ... 1/24/2005 5:31PM
    // GetConnection().DestroyTransaction(this);

    return pythonpp::CNone();
}

pythonpp::CObject
CTransaction::cursor(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(CreateCursor(), pythonpp::eTakeOwnership);
}

pythonpp::CObject
CTransaction::commit(const pythonpp::CTuple& args)
{
    CommitInternal();

    return pythonpp::CNone();
}

pythonpp::CObject
CTransaction::rollback(const pythonpp::CTuple& args)
{
    RollbackInternal();

    return pythonpp::CNone();
}

void
CTransaction::CloseInternal(void)
{
    // Close all cursors ...
    CloseOpenCursors();

    // Double check ...
    // Check for the DML connection also ...
    // if ( !m_SelectConnPool.Empty() || !m_DMLConnPool.Empty() ) {
    //     throw CInternalError("Unable to close a transaction. There are open cursors in use.");
    // }

    // Rollback transaction ...
    RollbackInternal();

    // Close all open connections ...
    m_SelectConnPool.Clear();
    // Close the DML connection ...
    m_DMLConnPool.Clear();
}

void
CTransaction::CloseOpenCursors(void)
{
    if ( !m_CursorList.empty() ) {
        // Make a copy of m_CursorList because it will be modified ...
        TCursorList tmp_CursorList = m_CursorList;
        TCursorList::const_iterator citer;
        TCursorList::const_iterator cend = tmp_CursorList.end();

        for ( citer = tmp_CursorList.begin(); citer != cend; ++citer ) {
            (*citer)->CloseInternal();
        }
    }
}

CCursor*
CTransaction::CreateCursor(void)
{
    CCursor* cursor = new CCursor(this);

    m_CursorList.insert(cursor);
    return cursor;
}

void
CTransaction::DestroyCursor(CCursor* cursor)
{
    // Python will take care of the object deallocation ...
    m_CursorList.erase(cursor);
}

IConnection*
CTransaction::CreateSelectConnection(void)
{
    IConnection* conn = NULL;

    if ( m_ConnectionMode == eSimpleMode ) {
        conn = m_DMLConnPool.Create();
    } else {
        conn = m_SelectConnPool.Create();
    }
    return conn;
}

void
CTransaction::DestroySelectConnection(IConnection* db_conn)
{
    if ( m_ConnectionMode == eSimpleMode ) {
        m_DMLConnPool.Destroy(db_conn);
    } else {
        m_SelectConnPool.Destroy(db_conn);
    }
}

//////////////////////////////////////////////////////////////////////////////
EStatementType
RetrieveStatementType(const string& stmt, EStatementType default_type)
{
    EStatementType stmtType = default_type;

    string::size_type pos = stmt.find_first_not_of(" \t\n");
    if (pos != string::npos)
    {
        // "CREATE" should be before DML ...
        if (NStr::EqualNocase(stmt, pos, sizeof("CREATE") - 1, "CREATE"))
        {
            stmtType = estCreate;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("SELECT") - 1, "SELECT"))
        {
            stmtType = estSelect;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("UPDATE") - 1, "UPDATE"))
        {
            stmtType = estUpdate;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("DELETE") - 1, "DELETE"))
        {
            stmtType = estDelete;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("INSERT") - 1, "INSERT"))
        {
            stmtType = estInsert;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("DROP") - 1, "DROP"))
        {
            stmtType = estDrop;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("ALTER") - 1, "ALTER"))
        {
            stmtType = estAlter;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("BEGIN") - 1, "BEGIN"))
        {
            stmtType = estTransaction;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("COMMIT") - 1, "COMMIT"))
        {
            stmtType = estTransaction;
        } else if (NStr::EqualNocase(stmt, pos, sizeof("ROLLBACK") - 1, "ROLLBACK"))
        {
            stmtType = estTransaction;
        // } else if (NStr::EqualNocase(stmt, pos, sizeof("EXEC") - 1, "EXEC"))
        // {
        //    stmtType = estFunction;
        }
    }

    return stmtType;
}

//////////////////////////////////////////////////////////////////////////////
CStmtHelper::CStmtHelper(CTransaction* trans)
: m_ParentTransaction( trans )
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }
}

CStmtHelper::CStmtHelper(CTransaction* trans, const CStmtStr& stmt)
: m_ParentTransaction( trans )
, m_StmtStr( stmt )
, m_Executed(false)
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    CreateStmt();
}

CStmtHelper::~CStmtHelper(void)
{
    try {
        Close();
    }
    NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )
}

void
CStmtHelper::Close(void)
{
    DumpResult();
    ReleaseStmt();
    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;
}

void
CStmtHelper::DumpResult(void)
{
    if ( m_Stmt.get() && m_Executed ) {
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS = m_Stmt->GetResultSet();
            }
        }
    }
}

void
CStmtHelper::ReleaseStmt(void)
{
    if ( m_Stmt.get() ) {
        IConnection* conn = m_Stmt->GetParentConn();

        // Release the statement before a connection release because it is a child object for a connection.
        m_Stmt.reset();

        _ASSERT( m_StmtStr.GetType() != estNone );

        if ( m_StmtStr.GetType() == estSelect ) {
            // Release SELECT Connection ...
            m_ParentTransaction->DestroySelectConnection( conn );
        } else {
            // Release DML Connection ...
            m_ParentTransaction->DestroyDMLConnection( conn );
        }
        m_Executed = false;
        m_ResultStatus = 0;
        m_ResultStatusAvailable = false;
    }
}

void
CStmtHelper::CreateStmt(void)
{
    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;

    if ( m_StmtStr.GetType() == estSelect ) {
        // Get a SELECT connection ...
        m_Stmt.reset( m_ParentTransaction->CreateSelectConnection()->GetStatement() );
    } else {
        // Get a DML connection ...
        m_Stmt.reset( m_ParentTransaction->CreateDMLConnection()->GetStatement() );
    }
}

void
CStmtHelper::SetStr(const CStmtStr& stmt)
{
    EStatementType oldStmtType = m_StmtStr.GetType();
    EStatementType currStmtType = stmt.GetType();
    m_StmtStr = stmt;

    if ( m_Stmt.get() ) {
        // If a new statement type needs a different connection type then release an old one.
        if (
            (oldStmtType == estSelect && currStmtType != estSelect) ||
            (oldStmtType != estSelect && currStmtType == estSelect)
        ) {
            DumpResult();
            ReleaseStmt();
            CreateStmt();
        } else {
            DumpResult();
            m_Stmt->ClearParamList();
        }
    } else {
        CreateStmt();
    }

    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;
}

void
CStmtHelper::SetParam(const string& name, const CVariant& value)
{
    _ASSERT( m_Stmt.get() );

    string param_name = name;

    if ( param_name.size() > 0) {
        if ( param_name[0] != '@') {
            param_name = "@" + param_name;
        }
    } else {
        throw CProgrammingError("Invalid SQL parameter name");
    }

    try {
        m_Stmt->SetParam( value, param_name );
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError( e.GetMsg() );
    }
}

void
CStmtHelper::Execute(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        switch ( m_StmtStr.GetType() ) {
        case estSelect :
            m_RS = m_Stmt->ExecuteQuery ( m_StmtStr.GetStr() );
            if (m_RS) {
                m_RS->BindBlobToVariant(true);
            }
            break;
        default:
            m_Stmt->ExecuteUpdate ( m_StmtStr.GetStr() );
        }
        m_Executed = true;
    }
    catch(const bad_cast&) {
        throw CInternalError("std::bad_cast exception within 'CStmtHelper::Execute'");
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }
    catch(const exception&) {
        throw CInternalError("std::exception exception within 'CStmtHelper::Execute'");
    }
}

long
CStmtHelper::GetRowCount(void) const
{
    if ( m_Executed ) {
        return m_Stmt->GetRowCount();
    }
    return -1;                          // As required by the specification ...
}

IResultSet&
CStmtHelper::GetRS(void)
{
    if ( m_RS == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

const IResultSet&
CStmtHelper::GetRS(void) const
{
    if ( m_RS == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

bool
CStmtHelper::HasRS(void) const
{
    return m_RS != NULL;
}

int
CStmtHelper::GetReturnStatus(void)
{
    if ( !m_ResultStatusAvailable ) {
        throw CDatabaseError("Procedure return code is not defined within this context.");
    }
    return m_ResultStatus;
}

bool
CStmtHelper::NextRS(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS = m_Stmt->GetResultSet();
                if ( m_RS->GetResultType() == eDB_StatusResult ) {
                    m_ResultStatus = m_RS->GetVariant(1).GetInt4();
                    m_ResultStatusAvailable = true;
                    return false;
                }
                return true;
            }
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
CCallableStmtHelper::CCallableStmtHelper(CTransaction* trans)
: m_ParentTransaction( trans )
, m_RS(NULL)
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }
}

CCallableStmtHelper::CCallableStmtHelper(CTransaction* trans, const CStmtStr& stmt)
: m_ParentTransaction( trans )
, m_RS(NULL)
, m_StmtStr( stmt )
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    CreateStmt();
}

CCallableStmtHelper::~CCallableStmtHelper(void)
{
    try {
        Close();
    }
    NCBI_CATCH_ALL_X( 4, NCBI_CURRENT_FUNCTION )
}

void
CCallableStmtHelper::Close(void)
{
    DumpResult();
    ReleaseStmt();
    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;
}

void
CCallableStmtHelper::DumpResult(void)
{
    if ( m_Stmt.get() && m_Executed ) {
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS = m_Stmt->GetResultSet();
            }
        }
    }
}

void
CCallableStmtHelper::ReleaseStmt(void)
{
    if ( m_Stmt.get() ) {
        IConnection* conn = m_Stmt->GetParentConn();

        // Release the statement before a connection release because it is a child object for a connection.
        m_Stmt.reset();

        _ASSERT( m_StmtStr.GetType() != estNone );

        // Release DML Connection ...
        m_ParentTransaction->DestroyDMLConnection( conn );
        m_Executed = false;
        m_ResultStatus = 0;
        m_ResultStatusAvailable = false;
    }
}

void
CCallableStmtHelper::CreateStmt(void)
{
    _ASSERT( m_StmtStr.GetType() == estFunction );

    ReleaseStmt();
    m_Stmt.reset( m_ParentTransaction->CreateDMLConnection()->GetCallableStatement(m_StmtStr.GetStr()) );
}

void
CCallableStmtHelper::SetStr(const CStmtStr& stmt)
{
    m_StmtStr = stmt;

    DumpResult();
    CreateStmt();

    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;
}

void
CCallableStmtHelper::SetParam(const string& name, const CVariant& value)
{
    _ASSERT( m_Stmt.get() );

    string param_name = name;

    if ( param_name.size() > 0) {
        if ( param_name[0] != '@') {
            param_name = "@" + param_name;
        }
    } else {
        throw CProgrammingError("Invalid SQL parameter name");
    }

    try {
	if (m_Stmt->GetParamsMetaData().GetDirection(name) == CDBParams::eIn) {
		m_Stmt->SetParam( value, param_name );
	} else {
		m_Stmt->SetOutputParam( value, param_name );
	}
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError( e.GetMsg() );
    }
}

void
CCallableStmtHelper::Execute(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        m_ResultStatus = 0;
        m_ResultStatusAvailable = false;

        m_Stmt->Execute();
        // Retrieve a resut if there is any ...
        NextRS();
        m_Executed = true;
    }
    catch(const bad_cast&) {
        throw CInternalError("std::bad_cast exception within 'CCallableStmtHelper::Execute'");
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }
    catch(const exception&) {
        throw CInternalError("std::exception exception within 'CCallableStmtHelper::Execute'");
    }
}

long
CCallableStmtHelper::GetRowCount(void) const
{
    if ( m_Executed ) {
        return m_Stmt->GetRowCount();
    }
    return -1;                          // As required by the specification ...
}

IResultSet&
CCallableStmtHelper::GetRS(void)
{
    if ( m_RS == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

const IResultSet&
CCallableStmtHelper::GetRS(void) const
{
    if ( m_RS == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

bool
CCallableStmtHelper::HasRS(void) const
{
    return m_RS != NULL;
}

int
CCallableStmtHelper::GetReturnStatus(void)
{
    if ( !m_ResultStatusAvailable ) {
        throw CDatabaseError("Procedure return code is not defined within this context.");
    }
    return m_Stmt->GetReturnStatus();
}

bool
CCallableStmtHelper::NextRS(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS = m_Stmt->GetResultSet();
                m_RS->BindBlobToVariant(true);
                return true;
            }
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    m_ResultStatusAvailable = true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////
pythonpp::CTuple
MakeTupleFromResult(IResultSet& rs)
{
    // Previous implementation of GetColumnNo/GetTotalColumns used to return
    // invalid value ...
    // col_num = (col_num > 0 ? col_num - 1 : col_num);

    // Set data. Make a sequence (tuple) ...
    int col_num = rs.GetTotalColumns();

    pythonpp::CTuple tuple(col_num);

    for ( int i = 0; i < col_num; ++i) {
        const CVariant& value = rs.GetVariant (i + 1);

        if ( value.IsNull() ) {
            continue;
        }

        switch ( value.GetType() ) {
        case eDB_Int :
            tuple[i] = pythonpp::CInt( value.GetInt4() );
            break;
        case eDB_SmallInt :
            tuple[i] = pythonpp::CInt( value.GetInt2() );
            break;
        case eDB_TinyInt :
            tuple[i] = pythonpp::CInt( value.GetByte() );
            break;
        case eDB_BigInt :
            tuple[i] = pythonpp::CLong( value.GetInt8() );
            break;
        case eDB_Float :
            tuple[i] = pythonpp::CFloat( value.GetFloat() );
            break;
        case eDB_Double :
            tuple[i] = pythonpp::CFloat( value.GetDouble() );
            break;
        /*
        case eDB_Numeric :
            tuple[i] = pythonpp::CFloat( value.GetDouble() );
            break;
        */
        case eDB_Bit :
            // BIT --> BOOL ...
            tuple[i] = pythonpp::CBool( value.GetBit() );
            break;
#if PY_VERSION_HEX >= 0x02040000
        case eDB_DateTime :
        case eDB_SmallDateTime :
            {
                const CTime& cur_time = value.GetCTime();
                tuple[i] = pythonpp::CDateTime(
                    cur_time.Year(),
                    cur_time.Month(),
                    cur_time.Day(),
                    cur_time.Hour(),
                    cur_time.Minute(),
                    cur_time.Second(),
                    cur_time.NanoSecond() / 1000
                    );
            }
            break;
#endif
        case eDB_VarChar :
        case eDB_Char :
        case eDB_LongChar :
            {
                string str = value.GetString();
                tuple[i] = pythonpp::CString( str );
            }
            // tuple[i] = pythonpp::CString( value.GetString() );
            break;
        case eDB_LongBinary :
        case eDB_VarBinary :
        case eDB_Binary :
        case eDB_Numeric :
            tuple[i] = pythonpp::CString( value.GetString() );
            break;
        case eDB_Text :
        case eDB_Image :
            {
                size_t lob_size = value.GetBlobSize();
                string tmp_str;

                tmp_str.resize(lob_size);
                value.Read( (void*)tmp_str.c_str(), lob_size );
                tuple[i] = pythonpp::CString(tmp_str);
            }
            break;
        case eDB_UnsupportedType :
            break;
        default:
            // All cases are supposed to be handled.
            // In case of PY_VERSION_HEX < 0x02040000 eDB_DateTime and
            // eDB_SmallDateTimewill be missed.
            break;
        }
    }

    return tuple;
}

//////////////////////////////////////////////////////////////////////////////
CCursor::CCursor(CTransaction* trans)
: m_PythonConnection( &trans->GetParentConnection() )
, m_PythonTransaction( trans )
, m_ParentTransaction( trans )
, m_NumOfArgs( 0 )
, m_RowsNum( -1 )
, m_ArraySize( 1 )
, m_StmtHelper( trans )
, m_CallableStmtHelper( trans )
, m_AllDataFetched( false )
, m_AllSetsFetched( false )
{
    if ( trans == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    ROAttr( "rowcount", m_RowsNum );

    PrepareForPython(this);
}

CCursor::~CCursor(void)
{
    try {
        CloseInternal();

        // Unregister this cursor with the parent transaction ...
        GetTransaction().DestroyCursor(this);
    }
    NCBI_CATCH_ALL_X( 5, NCBI_CURRENT_FUNCTION )
}

void
CCursor::CloseInternal(void)
{
    m_StmtHelper.Close();
    m_CallableStmtHelper.Close();
    m_RowsNum = -1;                     // As required by the specification ...
    m_AllDataFetched = false;
    m_AllSetsFetched = false;
}

pythonpp::CObject
CCursor::callproc(const pythonpp::CTuple& args)
{
    const size_t args_size = args.size();

    m_RowsNum = -1;                     // As required by the specification ...
    m_AllDataFetched = false;
    m_AllSetsFetched = false;

    if ( args_size == 0 ) {
        throw CProgrammingError("A stored procedure name is expected as a parameter");
    } else if ( args_size > 0 ) {
        pythonpp::CObject obj(args[0]);

        if ( pythonpp::CString::HasSameType(obj) ) {
            m_StmtStr.SetStr(pythonpp::CString(args[0]), estFunction);
        } else {
            throw CProgrammingError("A stored procedure name is expected as a parameter");
        }

        m_StmtHelper.Close();

        // Setup parameters ...
        if ( args_size > 1 ) {
            pythonpp::CObject obj( args[1] );

            if ( pythonpp::CDict::HasSameType(obj) ) {
                const pythonpp::CDict dict = obj;

                m_CallableStmtHelper.SetStr(m_StmtStr);
                SetupParameters(dict, m_CallableStmtHelper);
            } else  {
                // Curently, NCBI DBAPI supports pameter binding by name only ...
//            pythonpp::CSequence sequence;
//            if ( pythonpp::CList::HasSameType(obj) ) {
//            } else if ( pythonpp::CTuple::HasSameType(obj) ) {
//            } else if ( pythonpp::CSet::HasSameType(obj) ) {
//            }
                throw CNotSupportedError("NCBI DBAPI supports pameter binding by name only");
            }
        } else {
            m_CallableStmtHelper.SetStr(m_StmtStr);
        }
    }

    m_CallableStmtHelper.Execute();
    m_RowsNum = m_StmtHelper.GetRowCount();

    if ( m_CallableStmtHelper.HasRS() ) {
        IResultSet& rs = m_CallableStmtHelper.GetRS();

        if ( rs.GetResultType() == eDB_ParamResult ) {
            if ( rs.Next() ) {
                return MakeTupleFromResult( rs );
            }
        }
    }

    return pythonpp::CTuple();
}

pythonpp::CObject
CCursor::close(const pythonpp::CTuple& args)
{
    try {
        CloseInternal();

        // Unregister this cursor with the parent transaction ...
        GetTransaction().DestroyCursor(this);
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::execute(const pythonpp::CTuple& args)
{
    const size_t args_size = args.size();

    m_AllDataFetched = false;
    m_AllSetsFetched = false;

    // Process function's arguments ...
    if ( args_size == 0 ) {
        throw CProgrammingError("A SQL statement string is expected as a parameter");
    } else if ( args_size > 0 ) {
        pythonpp::CObject obj(args[0]);

        if ( pythonpp::CString::HasSameType(obj) ) {
            m_StmtStr.SetStr(pythonpp::CString(args[0]), estSelect);
        } else {
            throw CProgrammingError("A SQL statement string is expected as a parameter");
        }

        m_CallableStmtHelper.Close();
        m_StmtHelper.SetStr(m_StmtStr);

        // Setup parameters ...
        if ( args_size > 1 ) {
            pythonpp::CObject obj(args[1]);

            if ( pythonpp::CDict::HasSameType(obj) ) {
                SetupParameters(obj, m_StmtHelper);
            } else  {
                // Curently, NCBI DBAPI supports pameter binding by name only ...
//            pythonpp::CSequence sequence;
//            if ( pythonpp::CList::HasSameType(obj) ) {
//            } else if ( pythonpp::CTuple::HasSameType(obj) ) {
//            } else if ( pythonpp::CSet::HasSameType(obj) ) {
//            }
                throw CNotSupportedError("NCBI DBAPI supports pameter binding by name only");
            }
        }
    }

    m_StmtHelper.Execute();
    m_RowsNum = m_StmtHelper.GetRowCount();

    return pythonpp::CNone();
}

void
CCursor::SetupParameters(const pythonpp::CDict& dict, CStmtHelper& stmt)
{
    // Iterate over a dict.
    pythonpp::py_ssize_t i = 0;
    PyObject* key;
    PyObject* value;
    while ( PyDict_Next(dict, &i, &key, &value) ) {
        // Refer to borrowed references in key and value.
        const pythonpp::CObject key_obj(key);
        const pythonpp::CObject value_obj(value);
        string param_name = pythonpp::CString(key_obj);

        stmt.SetParam(param_name, GetCVariant(value_obj));
    }
}

void
CCursor::SetupParameters(const pythonpp::CDict& dict, CCallableStmtHelper& stmt)
{
    // Iterate over a dict.
    pythonpp::py_ssize_t i = 0;
    PyObject* key;
    PyObject* value;
    while ( PyDict_Next(dict, &i, &key, &value) ) {
        // Refer to borrowed references in key and value.
        const pythonpp::CObject key_obj(key);
        const pythonpp::CObject value_obj(value);
        string param_name = pythonpp::CString(key_obj);

        stmt.SetParam(param_name, GetCVariant(value_obj));
    }
}

CVariant
CCursor::GetCVariant(const pythonpp::CObject& obj) const
{
    if ( pythonpp::CNone::HasSameType(obj) ) {
        return CVariant(eDB_UnsupportedType);
    } else if ( pythonpp::CBool::HasSameType(obj) ) {
        return CVariant( pythonpp::CBool(obj) );
    } else if ( pythonpp::CInt::HasSameType(obj) ) {
        return CVariant( static_cast<int>(pythonpp::CInt(obj)) );
    } else if ( pythonpp::CLong::HasSameType(obj) ) {
        return CVariant( static_cast<Int8>(pythonpp::CLong(obj)) );
    } else if ( pythonpp::CFloat::HasSameType(obj) ) {
        return CVariant( pythonpp::CFloat(obj) );
    } else if ( pythonpp::CString::HasSameType(obj) ) {
        const pythonpp::CString python_str(obj);
        string std_str(python_str);

        if ( std_str.size() <= 255 ) {
            return CVariant( std_str );
        } else {
            return CVariant::LongChar(
                std_str.c_str(),
                std_str.size()
                );
        }
    }

    return CVariant(eDB_UnsupportedType);
}

pythonpp::CObject
CCursor::executemany(const pythonpp::CTuple& args)
{
    const size_t args_size = args.size();

    m_AllDataFetched = false;
    m_AllSetsFetched = false;

    // Process function's arguments ...
    if ( args_size == 0 ) {
        throw CProgrammingError("A SQL statement string is expected as a parameter");
    } else if ( args_size > 0 ) {
        pythonpp::CObject obj(args[0]);

        if ( pythonpp::CString::HasSameType(obj) ) {
            m_StmtStr.SetStr(pythonpp::CString(args[0]), estSelect);
        } else {
            throw CProgrammingError("A SQL statement string is expected as a parameter");
        }

        // Setup parameters ...
        if ( args_size > 1 ) {
            pythonpp::CObject obj(args[1]);

            if ( pythonpp::CList::HasSameType(obj) || pythonpp::CTuple::HasSameType(obj) ) {
                const pythonpp::CSequence params(obj);
                pythonpp::CList::const_iterator citer;
                pythonpp::CList::const_iterator cend = params.end();

                //
                m_CallableStmtHelper.Close();
                m_StmtHelper.SetStr( m_StmtStr );
                m_RowsNum = 0;

                for ( citer = params.begin(); citer != cend; ++citer ) {
                    SetupParameters(*citer, m_StmtHelper);
                    m_StmtHelper.Execute();
                    m_RowsNum += m_StmtHelper.GetRowCount();
                }
            } else {
                throw CProgrammingError("Sequence of parameters should be provided either as a list or as a tuple data type");
            }
        } else {
            throw CProgrammingError("A sequence of parameters is expected with the 'executemany' function");
        }
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::fetchone(const pythonpp::CTuple& args)
{
    try {
        if ( m_AllDataFetched ) {
            return pythonpp::CNone();
        }
        if ( m_StmtStr.GetType() == estFunction ) {
            IResultSet& rs = m_CallableStmtHelper.GetRS();

            if ( rs.Next() ) {
                m_RowsNum = m_CallableStmtHelper.GetRowCount();
                return MakeTupleFromResult( rs );
            }
        } else {
            IResultSet& rs = m_StmtHelper.GetRS();

            if ( rs.Next() ) {
                m_RowsNum = m_StmtHelper.GetRowCount();
                return MakeTupleFromResult( rs );
            }
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    m_AllDataFetched = true;
    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::fetchmany(const pythonpp::CTuple& args)
{
    size_t array_size = m_ArraySize;

    try {
        if ( args.size() > 0 ) {
            array_size = static_cast<unsigned long>(pythonpp::CLong(args[0]));
        }
    } catch (const pythonpp::CError&) {
        throw CProgrammingError("Invalid parameters within 'CCursor::fetchmany' function");
    }

    pythonpp::CList py_list;
    try {
        if ( m_AllDataFetched ) {
            return py_list;
        }
        if ( m_StmtStr.GetType() == estFunction ) {
            IResultSet& rs = m_CallableStmtHelper.GetRS();

            size_t i = 0;
            for ( ; i < array_size && rs.Next(); ++i ) {
                py_list.Append(MakeTupleFromResult(rs));
            }

            // We fetched less than expected ...
            if ( i < array_size ) {
                m_AllDataFetched = true;
            }

            m_RowsNum = m_CallableStmtHelper.GetRowCount();
        } else {
            IResultSet& rs = m_StmtHelper.GetRS();

            size_t i = 0;
            for ( ; i < array_size && rs.Next(); ++i ) {
                py_list.Append(MakeTupleFromResult(rs));
            }

            // We fetched less than expected ...
            if ( i < array_size ) {
                m_AllDataFetched = true;
            }

            m_RowsNum = m_StmtHelper.GetRowCount();
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    return py_list;
}

pythonpp::CObject
CCursor::fetchall(const pythonpp::CTuple& args)
{
    pythonpp::CList py_list;

    try {
        if ( m_AllDataFetched ) {
            return py_list;
        }
        if ( m_StmtStr.GetType() == estFunction ) {
            IResultSet& rs = m_CallableStmtHelper.GetRS();

            while ( rs.Next() ) {
                py_list.Append(MakeTupleFromResult(rs));
            }

            m_RowsNum = m_CallableStmtHelper.GetRowCount();
        } else {
            IResultSet& rs = m_StmtHelper.GetRS();

            while ( rs.Next() ) {
                py_list.Append(MakeTupleFromResult(rs));
            }

            m_RowsNum = m_StmtHelper.GetRowCount();
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }
    catch(...) {
        throw CDatabaseError("Unknown error in CCursor::fetchall");
    }

    m_AllDataFetched = true;
    return py_list;
}

bool
CCursor::NextSetInternal(void)
{
    try {
        m_RowsNum = 0;

        if ( !m_AllSetsFetched ) {
            if ( m_StmtStr.GetType() == estFunction ) {
                if ( m_CallableStmtHelper.NextRS() ) {
                    m_AllDataFetched = false;
                    return true;
                }
            } else {
                if ( m_StmtHelper.NextRS() ) {
                    m_AllDataFetched = false;
                    return true;
                }
            }
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    m_AllDataFetched = true;
    m_AllSetsFetched = true;

    return false;
}

pythonpp::CObject
CCursor::nextset(const pythonpp::CTuple& args)
{
    if ( NextSetInternal() ) {
        return pythonpp::CBool(true);
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::setinputsizes(const pythonpp::CTuple& args)
{
    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::setoutputsize(const pythonpp::CTuple& args)
{
    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::get_proc_return_status(const pythonpp::CTuple& args)
{
    try {
        if ( !m_AllDataFetched ) {
            throw CDatabaseError("Call get_proc_return_status after you retrieve all data.");
        }

        NextSetInternal();

        if ( !m_AllSetsFetched ) {
            throw CDatabaseError("Call get_proc_return_status after you retrieve all data.");
        }

        if ( m_StmtStr.GetType() == estFunction ) {
            return pythonpp::CInt( m_CallableStmtHelper.GetReturnStatus() );
        } else {
            return pythonpp::CInt( m_StmtHelper.GetReturnStatus() );
        }
    }
    catch(const CDB_Exception& e) {
        throw CDatabaseError(e.GetMsg());
    }

    return pythonpp::CNone();
}


/* Future development ... 2/4/2005 12:05PM
//////////////////////////////////////////////////////////////////////////////
// Future development ...
class CModuleDBAPI : public pythonpp::CExtModule<CModuleDBAPI>
{
public:
    CModuleDBAPI(const char* name, const char* descr = 0)
    : pythonpp::CExtModule<CModuleDBAPI>(name, descr)
    {
        PrepareForPython(this);
    }

public:
    // connect(driver_name, db_type, db_name, user_name, user_pswd)
    pythonpp::CObject connect(const pythonpp::CTuple& args);
    pythonpp::CObject Binary(const pythonpp::CTuple& args);
    pythonpp::CObject TimestampFromTicks(const pythonpp::CTuple& args);
    pythonpp::CObject TimeFromTicks(const pythonpp::CTuple& args);
    pythonpp::CObject DateFromTicks(const pythonpp::CTuple& args);
    pythonpp::CObject Timestamp(const pythonpp::CTuple& args);
    pythonpp::CObject Time(const pythonpp::CTuple& args);
    pythonpp::CObject Date(const pythonpp::CTuple& args);
};

pythonpp::CObject
CModuleDBAPI::connect(const pythonpp::CTuple& args)
{
    string driver_name;
    string db_type;
    string server_name;
    string db_name;
    string user_name;
    string user_pswd;

    try {
        try {
            const pythonpp::CTuple func_args(args);

            driver_name = pythonpp::CString(func_args[0]);
            db_type = pythonpp::CString(func_args[1]);
            server_name = pythonpp::CString(func_args[2]);
            db_name = pythonpp::CString(func_args[3]);
            user_name = pythonpp::CString(func_args[4]);
            user_pswd = pythonpp::CString(func_args[5]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'connect' function");
        }

        CConnection* conn = new CConnection( CConnParam(
            driver_name,
            db_type,
            server_name,
            db_name,
            user_name,
            user_pswd
            ));

        // Feef the object to the Python interpreter ...
        return pythonpp::CObject(conn, pythonpp::eTakeOwnership);
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a date value.
// Date(year,month,day)
pythonpp::CObject
CModuleDBAPI::Date(const pythonpp::CTuple& args)
{
    try {
        int year;
        int month;
        int day;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Date' function");
        }

        // Feef the object to the Python interpreter ...
        return pythonpp::CDate(year, month, day);
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a time value.
// Time(hour,minute,second)
pythonpp::CObject
CModuleDBAPI::Time(const pythonpp::CTuple& args)
{
    try {
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            hour = pythonpp::CInt(func_args[0]);
            minute = pythonpp::CInt(func_args[1]);
            second = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Time' function");
        }

        // Feef the object to the Python interpreter ...
        return pythonpp::CTime(hour, minute, second, 0);
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a time stamp
// value.
// Timestamp(year,month,day,hour,minute,second)
pythonpp::CObject
CModuleDBAPI::Timestamp(const pythonpp::CTuple& args)
{
    try {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
            hour = pythonpp::CInt(func_args[3]);
            minute = pythonpp::CInt(func_args[4]);
            second = pythonpp::CInt(func_args[5]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Timestamp' function");
        }

        // Feef the object to the Python interpreter ...
        return pythonpp::CDateTime(year, month, day, hour, minute, second, 0);
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a date value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// DateFromTicks(ticks)
pythonpp::CObject
CModuleDBAPI::DateFromTicks(const pythonpp::CTuple& args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a time value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// TimeFromTicks(ticks)
pythonpp::CObject
CModuleDBAPI::TimeFromTicks(const pythonpp::CTuple& args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object holding a time stamp
// value from the given ticks value (number of seconds since
// the epoch; see the documentation of the standard Python
// time module for details).
// TimestampFromTicks(ticks)
pythonpp::CObject
CModuleDBAPI::TimestampFromTicks(const pythonpp::CTuple& args)
{
    try {
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    // Return a dummy object ...
    return pythonpp::CNone();
}

// This function constructs an object capable of holding a
// binary (long) string value.
// Binary(string)
pythonpp::CObject
CModuleDBAPI::Binary(const pythonpp::CTuple& args)
{

    try {
        string value;

        try {
            const pythonpp::CTuple func_args(args);

            value = pythonpp::CString(func_args[0]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Binary' function");
        }

        CBinary* obj = new CBinary(
            );

        // Feef the object to the Python interpreter ...
        return pythonpp::CObject(obj, pythonpp::eTakeOwnership);
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }

    return pythonpp::CNone();
}

*/

}

/* Future development ... 2/4/2005 12:05PM
// Module initialization
PYDBAPI_MODINIT_FUNC(initpython_ncbi_dbapi)
{
    // Initialize DateTime module ...
    PyDateTime_IMPORT;

    // Declare CBinary
    python::CBinary::Declare("python_ncbi_dbapi.BINARY");

    // Declare CNumber
    python::CNumber::Declare("python_ncbi_dbapi.NUMBER");

    // Declare CRowID
    python::CRowID::Declare("python_ncbi_dbapi.ROWID");

    // Declare CConnection
    python::CConnection::
        Def("close",        &python::CConnection::close,        "close").
        Def("commit",       &python::CConnection::commit,       "commit").
        Def("rollback",     &python::CConnection::rollback,     "rollback").
        Def("cursor",       &python::CConnection::cursor,       "cursor").
        Def("transaction",  &python::CConnection::transaction,  "transaction");
    python::CConnection::Declare("python_ncbi_dbapi.Connection");

    // Declare CTransaction
    python::CTransaction::
        Def("close",        &python::CTransaction::close,        "close").
        Def("cursor",       &python::CTransaction::cursor,       "cursor").
        Def("commit",       &python::CTransaction::commit,       "commit").
        Def("rollback",     &python::CTransaction::rollback,     "rollback");
    python::CTransaction::Declare("python_ncbi_dbapi.Transaction");

    // Declare CCursor
    python::CCursor::
        Def("callproc",     &python::CCursor::callproc,     "callproc").
        Def("close",        &python::CCursor::close,        "close").
        Def("execute",      &python::CCursor::execute,      "execute").
        Def("executemany",  &python::CCursor::executemany,  "executemany").
        Def("fetchone",     &python::CCursor::fetchone,     "fetchone").
        Def("fetchmany",    &python::CCursor::fetchmany,    "fetchmany").
        Def("fetchall",     &python::CCursor::fetchall,     "fetchall").
        Def("nextset",      &python::CCursor::nextset,      "nextset").
        Def("setinputsizes", &python::CCursor::setinputsizes, "setinputsizes").
        Def("setoutputsize", &python::CCursor::setoutputsize, "setoutputsize");
    python::CCursor::Declare("python_ncbi_dbapi.Cursor");


    // Declare CModuleDBAPI
    python::CModuleDBAPI::
        Def("connect",    &python::CModuleDBAPI::connect, "connect").
        Def("Date", &python::CModuleDBAPI::Date, "Date").
        Def("Time", &python::CModuleDBAPI::Time, "Time").
        Def("Timestamp", &python::CModuleDBAPI::Timestamp, "Timestamp").
        Def("DateFromTicks", &python::CModuleDBAPI::DateFromTicks, "DateFromTicks").
        Def("TimeFromTicks", &python::CModuleDBAPI::TimeFromTicks, "TimeFromTicks").
        Def("TimestampFromTicks", &python::CModuleDBAPI::TimestampFromTicks, "TimestampFromTicks").
        Def("Binary", &python::CModuleDBAPI::Binary, "Binary");
//    python::CModuleDBAPI module_("python_ncbi_dbapi");
    // Python interpreter will tale care of deleting module object ...
    python::CModuleDBAPI* module2 = new python::CModuleDBAPI("python_ncbi_dbapi");
}
*/

namespace python
{
//////////////////////////////////////////////////////////////////////////////
// connect(driver_name, db_type, db_name, user_name, user_pswd)
static
PyObject*
Connect(PyObject *self, PyObject *args)
{
    CConnection* conn = NULL;

    try {
        string driver_name;
        string db_type;
        string server_name;
        string db_name;
        string user_name;
        string user_pswd;
        bool support_standard_interface = false;

        try {
            const pythonpp::CTuple func_args(args);

            driver_name = pythonpp::CString(func_args[0]);
            db_type = pythonpp::CString(func_args[1]);
            server_name = pythonpp::CString(func_args[2]);
            db_name = pythonpp::CString(func_args[3]);
            user_name = pythonpp::CString(func_args[4]);
            user_pswd = pythonpp::CString(func_args[5]);
            if ( func_args.size() > 6 ) {
                support_standard_interface = pythonpp::CBool(func_args[6]);
            }
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'connect' function");
        }

        conn = new CConnection( CConnParam(
            driver_name,
            db_type,
            server_name,
            db_name,
            user_name,
            user_pswd
            ),
            ( support_standard_interface ? eStandardMode : eSimpleMode )
            );
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Connect");
    }

    return conn;
}

// This function constructs an object holding a date value.
// Date(year,month,day)
static
PyObject*
Date(PyObject *self, PyObject *args)
{
#if PY_VERSION_HEX < 0x02040000
    pythonpp::CError::SetString("python_ncbi_dbapi::Date requires Python 2.4 or newer.");
#else
    try {
        int year;
        int month;
        int day;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Date' function");
        }

        return IncRefCount(pythonpp::CDate(year, month, day));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Date");
    }
#endif

    return NULL;
}

// This function constructs an object holding a time value.
// Time(hour,minute,second)
static
PyObject*
Time(PyObject *self, PyObject *args)
{
#if PY_VERSION_HEX < 0x02040000
    pythonpp::CError::SetString("python_ncbi_dbapi::Time requires Python 2.4 or newer.");
#else
    try {
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            hour = pythonpp::CInt(func_args[0]);
            minute = pythonpp::CInt(func_args[1]);
            second = pythonpp::CInt(func_args[2]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Time' function");
        }

        return IncRefCount(pythonpp::CTime(hour, minute, second, 0));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Time");
    }
#endif

    return NULL;
}

// This function constructs an object holding a time stamp
// value.
// Timestamp(year,month,day,hour,minute,second)
static
PyObject*
Timestamp(PyObject *self, PyObject *args)
{
#if PY_VERSION_HEX < 0x02040000
    pythonpp::CError::SetString("python_ncbi_dbapi::Timestamp requires Python 2.4 or newer.");
#else
    try {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;

        try {
            const pythonpp::CTuple func_args(args);

            year = pythonpp::CInt(func_args[0]);
            month = pythonpp::CInt(func_args[1]);
            day = pythonpp::CInt(func_args[2]);
            hour = pythonpp::CInt(func_args[3]);
            minute = pythonpp::CInt(func_args[4]);
            second = pythonpp::CInt(func_args[5]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Timestamp' function");
        }

        return IncRefCount(pythonpp::CDateTime(year, month, day, hour, minute, second, 0));
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Timestamp");
    }
#endif

    return NULL;
}

// This function constructs an object holding a date value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// DateFromTicks(ticks)
static
PyObject*
DateFromTicks(PyObject *self, PyObject *args)
{
    try {
        throw CNotSupportedError("Function DateFromTicks");
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::DateFromTicks");
    }

    return NULL;
}

// This function constructs an object holding a time value
// from the given ticks value (number of seconds since the
// epoch; see the documentation of the standard Python time
// module for details).
// TimeFromTicks(ticks)
static
PyObject*
TimeFromTicks(PyObject *self, PyObject *args)
{
    try {
        throw CNotSupportedError("Function TimeFromTicks");
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::TimeFromTicks");
    }

    return NULL;
}

// This function constructs an object holding a time stamp
// value from the given ticks value (number of seconds since
// the epoch; see the documentation of the standard Python
// time module for details).
// TimestampFromTicks(ticks)
static
PyObject*
TimestampFromTicks(PyObject *self, PyObject *args)
{
    try {
        throw CNotSupportedError("Function TimestampFromTicks");
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::TimestampFromTicks");
    }

    return NULL;
}

// This function constructs an object capable of holding a
// binary (long) string value.
// Binary(string)
static
PyObject*
Binary(PyObject *self, PyObject *args)
{
    CBinary* obj = NULL;

    try {
        string value;

        try {
            const pythonpp::CTuple func_args(args);

            value = pythonpp::CString(func_args[0]);
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'Binary' function");
        }

        obj = new CBinary(
            );
    }
    catch (const CDB_Exception& e) {
        pythonpp::CError::SetString(e.GetMsg());
    }
    catch(const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch(...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Binary");
    }

    return obj;
}

class CDBAPIModule
{
public:
    static void Declare(const char* name, PyMethodDef* methods);

private:
    static PyObject* m_Module;
};
PyObject* CDBAPIModule::m_Module = NULL;

void
CDBAPIModule::Declare(const char* name, PyMethodDef* methods)
{
    m_Module = Py_InitModule(const_cast<char*>(name), methods);
}

}

static struct PyMethodDef python_ncbi_dbapi_methods[] = {
    {"connect", (PyCFunction) python::Connect, METH_VARARGS,
        "connect(driver_name, db_type, server_name, database_name, userid, password) "
        "-- connect to the "
        "driver_name; db_type; server_name; database_name; userid; password;"
    },
    {"Date", (PyCFunction) python::Date, METH_VARARGS, "Date"},
    {"Time", (PyCFunction) python::Time, METH_VARARGS, "Time"},
    {"Timestamp", (PyCFunction) python::Timestamp, METH_VARARGS, "Timestamp"},
    {"DateFromTicks", (PyCFunction) python::DateFromTicks, METH_VARARGS, "DateFromTicks"},
    {"TimeFromTicks", (PyCFunction) python::TimeFromTicks, METH_VARARGS, "TimeFromTicks"},
    {"TimestampFromTicks", (PyCFunction) python::TimestampFromTicks, METH_VARARGS, "TimestampFromTicks"},
    {"Binary", (PyCFunction) python::Binary, METH_VARARGS, "Binary"},
    { NULL, NULL }
};

static
void init_common(const string& module_name)
{
    char* rev_str = "$Revision$";
    PyObject *module;

    // Fix plugin manager ...
    CFile file(python::RetrieveModuleFileName());
    string module_dir = file.GetDir() + "python_ncbi_dbapi/1.6";
    CDriverManager::GetInstance().AddDllSearchPath(module_dir.c_str());


    pythonpp::CModuleExt::Declare(module_name, python_ncbi_dbapi_methods);

    // Define module attributes ...
    pythonpp::CModuleExt::AddConst("apilevel", "2.0");
    pythonpp::CModuleExt::AddConst("__version__", string( rev_str + 11, strlen( rev_str + 11 ) - 2 ));
    pythonpp::CModuleExt::AddConst("threadsafety", 0);
    pythonpp::CModuleExt::AddConst("paramstyle", "named");

    module = pythonpp::CModuleExt::GetPyModule();

    ///////////////////////////////////

#if PY_VERSION_HEX >= 0x02040000
    // Initialize DateTime module ...
    PyDateTime_IMPORT;
#endif

    // Declare CBinary
    python::CBinary::Declare(string(module_name + ".BINARY").c_str());

    // Declare CNumber
    python::CNumber::Declare(string(module_name + ".NUMBER").c_str());

    // Declare CRowID
    python::CRowID::Declare(string(module_name + ".ROWID").c_str());

    // Declare CConnection
    python::CConnection::
        Def("close",        &python::CConnection::close,        "close").
        Def("commit",       &python::CConnection::commit,       "commit").
        Def("rollback",     &python::CConnection::rollback,     "rollback").
        Def("cursor",       &python::CConnection::cursor,       "cursor").
        Def("transaction",  &python::CConnection::transaction,  "transaction");
    python::CConnection::Declare(string(module_name + ".Connection").c_str());

    // Declare CTransaction
    python::CTransaction::
        Def("close",        &python::CTransaction::close,        "close").
        Def("cursor",       &python::CTransaction::cursor,       "cursor").
        Def("commit",       &python::CTransaction::commit,       "commit").
        Def("rollback",     &python::CTransaction::rollback,     "rollback");
    python::CTransaction::Declare(string(module_name + ".Transaction").c_str());

    // Declare CCursor
    python::CCursor::
        Def("callproc",     &python::CCursor::callproc,     "callproc").
        Def("close",        &python::CCursor::close,        "close").
        Def("execute",      &python::CCursor::execute,      "execute").
        Def("executemany",  &python::CCursor::executemany,  "executemany").
        Def("fetchone",     &python::CCursor::fetchone,     "fetchone").
        Def("fetchmany",    &python::CCursor::fetchmany,    "fetchmany").
        Def("fetchall",     &python::CCursor::fetchall,     "fetchall").
        Def("nextset",      &python::CCursor::nextset,      "nextset").
        Def("setinputsizes", &python::CCursor::setinputsizes, "setinputsizes").
        Def("setoutputsize", &python::CCursor::setoutputsize, "setoutputsize").
        Def("get_proc_return_status", &python::CCursor::get_proc_return_status, "get_proc_return_status");
    python::CCursor::Declare(string(module_name + ".Cursor").c_str());

    ///////////////////////////////////
    // Dclare types ...

    // Declare BINARY
    if ( PyType_Ready(&python::CBinary::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, "BINARY", (PyObject*)&python::CBinary::GetType() ) == -1 ) {
        return;
    }

    // Declare NUMBER
    if ( PyType_Ready(&python::CNumber::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, "NUMBER", (PyObject*)&python::CNumber::GetType() ) == -1 ) {
        return;
    }

    // Declare ROWID
    if ( PyType_Ready(&python::CRowID::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, "ROWID", (PyObject*)&python::CRowID::GetType() ) == -1 ) {
        return;
    }

    ///////////////////////////////////
    // Add exceptions ...

    python::CWarning::Declare("Warning");
    python::CError::Declare("Error");
    python::CInterfaceError::Declare("InterfaceError");
    python::CDatabaseError::Declare("DatabaseError");
    python::CInternalError::Declare("InternalError");
    python::COperationalError::Declare("OperationalError");
    python::CProgrammingError::Declare("ProgrammingError");
    python::CIntegrityError::Declare("IntegrityError");
    python::CDataError::Declare("DataError");
    python::CNotSupportedError::Declare("NotSupportedError");
}

// Module initialization
PYDBAPI_MODINIT_FUNC(initpython_ncbi_dbapi)
{
    init_common("python_ncbi_dbapi");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi)
{
    init_common("ncbi_dbapi");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_current)
{
    init_common("ncbi_dbapi_current");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_frozen)
{
    init_common("ncbi_dbapi_frozen");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_metastable)
{
    init_common("ncbi_dbapi_metastable");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_potluck)
{
    init_common("ncbi_dbapi_potluck");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_production)
{
    init_common("ncbi_dbapi_production");
}

PYDBAPI_MODINIT_FUNC(initncbi_dbapi_stable)
{
    init_common("ncbi_dbapi_stable");
}

END_NCBI_SCOPE


