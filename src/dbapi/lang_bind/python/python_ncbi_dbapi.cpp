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

#define PYTHONPP_DEFINE_GLOBALS 1
#include "python_ncbi_dbapi.hpp"
#include "pythonpp/pythonpp_pdt.hpp"
#if PY_VERSION_HEX >= 0x02040000
#  include "pythonpp/pythonpp_date.hpp"
#endif
#include <structmember.h>

#include <common/ncbi_package_ver.h>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <util/static_map.hpp>
#include <util/value_convert_policy.hpp>
#include <dbapi/error_codes.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
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


/// Flag showing whether all pythonpp::CString objects should be created as
/// Python unicode strings (TRUE value) or regular strings (FALSE value)
bool g_PythonStrDefToUnicode = false;


namespace python
{

// A striped version of IResultSet because it is hard to implement all member
// functions from IResultSet in case of cached data.
class CVariantSet : public CObject
{
public:
    /// Destructor.
    ///
    /// Clean up the resultset.
    virtual ~CVariantSet(void) {}

    /// Get result type.
    ///
    /// @sa
    ///   See in <dbapi/driver/interfaces.hpp> for the list of result types.
    virtual EDB_ResType GetResultType(void) = 0;

    /// Get next row.
    ///
    /// NOTE: no results are fetched before first call to this function.
    virtual bool Next(void) = 0;

    /// Retrieve a CVariant class describing the data stored in a given column.
    /// Note that the index supplied is one-based, not zero-based; the first
    /// column is column 1.
    ///
    /// @param param
    ///   Column number (one-based) or name
    /// @return
    ///   All data (for BLOB data see below) is returned as CVariant.
    virtual const CVariant& GetVariant(const CDBParamVariant& param) = 0;

    /// Get total columns.
    ///
    /// @return
    ///   Returns total number of columns in the resultset
    virtual unsigned int GetTotalColumns(void) = 0;

    /// Get Metadata.
    ///
    /// @return
    ///   Pointer to result metadata.
    virtual const IResultSetMetaData& GetMetaData(void) const = 0;
};

//////////////////////////////////////////////////////////////////////////////
class CCachedResultSet : public CVariantSet
{
public:
    CCachedResultSet(IResultSet& other);
    virtual ~CCachedResultSet(void);

    virtual EDB_ResType GetResultType(void);
    virtual bool Next(void);
    virtual const CVariant& GetVariant(const CDBParamVariant& param);
    virtual unsigned int GetTotalColumns(void);
    virtual const IResultSetMetaData& GetMetaData(void) const;

private:
    typedef deque<CVariant> TRecord;
    typedef deque<TRecord>  TRecordSet;

    const EDB_ResType   m_ResType;
    const unsigned int  m_ColumsNum;

    TRecordSet  m_RecordSet;
    auto_ptr<const IResultSetMetaData> m_MetaData;
    size_t      m_CurRowNum;
    size_t      m_CurColNum;
};

CCachedResultSet::CCachedResultSet(IResultSet& other)
: m_ResType(other.GetResultType())
, m_ColumsNum(other.GetTotalColumns())
, m_MetaData(other.GetMetaData(eTakeOwnership))
, m_CurRowNum(0)
, m_CurColNum(0)
{
    while (other.Next()) {
        m_RecordSet.push_back(TRecord());
        TRecordSet::reference record = m_RecordSet.back();

        for (unsigned int col = 1; col <= m_ColumsNum; ++col) {
            record.push_back(other.GetVariant(col));
        }
    }
}

CCachedResultSet::~CCachedResultSet(void)
{
}

EDB_ResType
CCachedResultSet::GetResultType(void)
{
    return m_ResType;
}

bool
CCachedResultSet::Next(void)
{
    if (m_CurRowNum < m_RecordSet.size()) {
        ++m_CurRowNum;
        return true;
    }

    return false;
}


class CVariant_Callbacks
{
public:
    CVariant* Create(void) {
        return new CVariant(eDB_UnsupportedType);
    }
    void Cleanup(CVariant& value) {}
};


const CVariant&
CCachedResultSet::GetVariant(const CDBParamVariant& param)
{
    if (param.IsPositional()) {
        unsigned int col_num = param.GetPosition();

        if (col_num > 0
            && col_num <= GetTotalColumns()
            && m_CurRowNum <= m_RecordSet.size()
            ) {
            return m_RecordSet[m_CurRowNum - 1][col_num - 1];
        }
    }

    static CSafeStatic<CVariant, CVariant_Callbacks> value;
    return value.Get();
}

unsigned int
CCachedResultSet::GetTotalColumns(void)
{
    return m_ColumsNum;
}

const IResultSetMetaData&
CCachedResultSet::GetMetaData(void) const
{
    return *m_MetaData;
}


//////////////////////////////////////////////////////////////////////////////
class CRealResultSet : public CVariantSet
{
public:
    CRealResultSet(IResultSet* other);
    virtual ~CRealResultSet(void);

    virtual EDB_ResType GetResultType(void);
    virtual bool Next(void);
    virtual const CVariant& GetVariant(const CDBParamVariant& param);
    virtual unsigned int GetTotalColumns(void);
    virtual const IResultSetMetaData& GetMetaData(void) const;

private:
    // Lifetime of m_RS shouldn't be managed by auto_ptr
    IResultSet* m_RS;
};

CRealResultSet::CRealResultSet(IResultSet* other)
: m_RS(other)
{
    _ASSERT(other);
}

CRealResultSet::~CRealResultSet(void)
{
}

EDB_ResType
CRealResultSet::GetResultType(void)
{
    return m_RS->GetResultType();
}

bool
CRealResultSet::Next(void)
{
    return m_RS->Next();
}

const CVariant&
CRealResultSet::GetVariant(const CDBParamVariant& param)
{
    return m_RS->GetVariant(param);
}

unsigned int
CRealResultSet::GetTotalColumns(void)
{
    return m_RS->GetTotalColumns();
}

const IResultSetMetaData&
CRealResultSet::GetMetaData(void) const
{
    return *m_RS->GetMetaData();
}

//////////////////////////////////////////////////////////////////////////////
class CRealSetProxy : public CResultSetProxy
{
public:
    CRealSetProxy(ICallableStatement& stmt);
    virtual ~CRealSetProxy(void);

    virtual bool MoveToNextRS(void);
    virtual bool MoveToLastRS(void);
    virtual CVariantSet& GetRS(void);
    virtual const CVariantSet& GetRS(void) const;
    virtual bool HasRS(void) const;
    virtual void DumpResult(void);

private:
    ICallableStatement* m_Stmt;
    auto_ptr<CVariantSet> m_VariantSet;
    bool m_HasRS;
};

CRealSetProxy::CRealSetProxy(ICallableStatement& stmt)
: m_Stmt(&stmt)
, m_HasRS(false)
{
}

CRealSetProxy::~CRealSetProxy(void)
{
}


bool CRealSetProxy::MoveToNextRS(void)
{
    m_HasRS = false;

    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

    while (m_Stmt->HasMoreResults()) {
        if ( m_Stmt->HasRows() ) {
            m_VariantSet.reset(new CRealResultSet(m_Stmt->GetResultSet()));
            m_HasRS = true;
            break;
        }
    }

    return m_HasRS;
}

bool
CRealSetProxy::MoveToLastRS(void)
{
    return false;
}

CVariantSet&
CRealSetProxy::GetRS(void)
{
    return *m_VariantSet;
}

const CVariantSet& CRealSetProxy::GetRS(void) const
{
    return *m_VariantSet;
}

bool CRealSetProxy::HasRS(void) const
{
    return m_HasRS;
}

void
CRealSetProxy::DumpResult(void)
{
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
    while ( m_Stmt->HasMoreResults() ) {
        if ( m_Stmt->HasRows() ) {
            // Keep very last ResultSet in case somebody calls GetRS().
            m_VariantSet.reset(new CRealResultSet(m_Stmt->GetResultSet()));
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
class CVariantSetProxy : public CResultSetProxy
{
public:
    CVariantSetProxy(ICallableStatement& stmt);
    virtual ~CVariantSetProxy(void);

    virtual bool MoveToNextRS(void);
    virtual bool MoveToLastRS(void);
    virtual CVariantSet& GetRS(void);
    virtual const CVariantSet& GetRS(void) const;
    virtual bool HasRS(void) const;
    virtual void DumpResult(void);

private:
    typedef deque<CRef<CVariantSet> > TCachedSet;

    TCachedSet m_CachedSet;
    CRef<CVariantSet> m_CurResultSet;
    bool m_HasRS;
};

CVariantSetProxy::CVariantSetProxy(ICallableStatement& stmt)
: m_HasRS(false)
{
    while (stmt.HasMoreResults()) {
        if (stmt.HasRows()) {
            auto_ptr<IResultSet> rs(stmt.GetResultSet());
            m_CachedSet.push_back(CRef<CVariantSet>(new CCachedResultSet(*rs)));
        }
    }
}

CVariantSetProxy::~CVariantSetProxy(void)
{
}


bool CVariantSetProxy::MoveToNextRS(void)
{
    m_HasRS = false;

    if (!m_CachedSet.empty()) {
        m_CurResultSet.Reset(m_CachedSet.front());
        m_CachedSet.pop_front();
        m_HasRS = true;
    }

    return m_HasRS;
}

bool
CVariantSetProxy::MoveToLastRS(void)
{
    m_HasRS = false;

    if (!m_CachedSet.empty()) {
        m_CurResultSet.Reset(m_CachedSet.back());
        m_CachedSet.pop_back();
        m_HasRS = true;
    }

    return m_HasRS;
}

CVariantSet& CVariantSetProxy::GetRS(void)
{
    return *m_CurResultSet;
}

const CVariantSet& CVariantSetProxy::GetRS(void) const
{
    return *m_CurResultSet;
}

bool CVariantSetProxy::HasRS(void) const
{
    return m_HasRS;
}

void CVariantSetProxy::DumpResult(void)
{
    while (MoveToNextRS()) {;}
}


//////////////////////////////////////////////////////////////////////////////
CBinary::CBinary(void)
{
    PrepareForPython(this);
}

CBinary::CBinary(const string& value)
: m_Value(value)
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
CStringType::CStringType(void)
{
    PrepareForPython(this);
}

CStringType::~CStringType(void)
{
}

//////////////////////////////////////////////////////////////////////////////
CDateTimeType::CDateTimeType(void)
{
    PrepareForPython(this);
}

CDateTimeType::~CDateTimeType(void)
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
    static CSafeStatic<CDataSourcePool> ds_pool;

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
        throw CDatabaseError( e.what() );
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
        if ( cur_size = GetModuleFileNameA( mh, buff, buff_size ) ) {
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

// Attempt to reclassify C++ DBAPI exceptions per the Python DBAPI
// ontology from PEP 249.  It's not always clear which classification
// is most appropriate; these tables take the approach of using
// InterfaceError only for generic initial setup considerations, and
// ProgrammingError for other issues caught at the client side even if
// they'd be a problem for any DB or server.  The line between
// internal and operational errors is even less clear.

typedef void (*FRethrow)(const CDB_Exception&);
typedef CStaticArrayMap<int, FRethrow> TDBErrCodeMap;
typedef SStaticPair    <int, FRethrow> TDBErrCodePair;

#define DB_ERR_CODE(value, type) { value, &C##type##Error::Rethrow }

static const TDBErrCodePair kClientErrCodes[] =
{
    DB_ERR_CODE(0,       Database),     // no specific code given
    DB_ERR_CODE(1,       Programming),  // malformatted name
    DB_ERR_CODE(2,       Programming),  // unknown or wrong type
    DB_ERR_CODE(100,     Programming),  // illegal precision
    DB_ERR_CODE(101,     Programming),  // scale > precision
    DB_ERR_CODE(102,     Data),         // string cannot be converted
    DB_ERR_CODE(103,     Data),         // conversion overflow
    DB_ERR_CODE(300,     Interface),    // couldn't find/load driver
    DB_ERR_CODE(10005,   Programming),  // bit params not supported here
    DB_ERR_CODE(20001,   Database),     // disparate uses
    DB_ERR_CODE(100012,  Interface),    // circular dep. in resource info file
    DB_ERR_CODE(101100,  Data),         // invalid type conversion
    DB_ERR_CODE(110092,  Programming),  // wrong (zero) data size
    DB_ERR_CODE(110100,  Interface),    // no server or host name
    DB_ERR_CODE(111000,  Interface),    // no server or pool name
    DB_ERR_CODE(120010,  Programming),  // want result from unsent command
    DB_ERR_CODE(122002,  NotSupported), // unimplemented by CDB*Params
    DB_ERR_CODE(123015,  Programming),  // bad BCP hint type
    DB_ERR_CODE(123016,  Programming),  // bad BCP hint value
    DB_ERR_CODE(125000,  NotSupported), // can't cancel BCP w/Sybase ctlib
    DB_ERR_CODE(130020,  Programming),  // wrong CDB_Object type
    DB_ERR_CODE(130120,  Programming),  // wrong CDB_Object type
    DB_ERR_CODE(190000,  Programming),  // wrong (zero) arguments
    DB_ERR_CODE(200001,  Database),     // disparate uses
    DB_ERR_CODE(200010,  Interface),    // insufficient credential info
    DB_ERR_CODE(210092,  Programming),  // wrong (zero) data size
    DB_ERR_CODE(220010,  Programming),  // want result from unsent command
    DB_ERR_CODE(221010,  Programming),  // want result from unsent command
    DB_ERR_CODE(230020,  Programming),  // wrong CDB_Object type
    DB_ERR_CODE(230021,  Data),         // too long for CDB_VarChar
    DB_ERR_CODE(290000,  Programming),  // wrong (zero) arguments
    DB_ERR_CODE(300011,  Programming),  // bad protocol version for FreeTDS
    DB_ERR_CODE(410055,  Programming),  // invalid text encoding
    DB_ERR_CODE(420010,  Programming),  // want result from unsent command
    DB_ERR_CODE(430020,  Programming),  // wrong CDB_Object type
    DB_ERR_CODE(500001,  NotSupported), // GetLowLevelHandle unimplemented
    DB_ERR_CODE(1000010, Programming)   // NULL ptr. to driver context
};

static const TDBErrCodePair kSybaseErrCodes[] =
{
    DB_ERR_CODE(102,  Programming), // incorrect syntax
    DB_ERR_CODE(207,  Programming), // invalid column name
    DB_ERR_CODE(208,  Programming), // invalid object name
    DB_ERR_CODE(515,  Integrity),   // cannot insert NULL
    DB_ERR_CODE(547,  Integrity),   // constraint violation (foreign key?)
    DB_ERR_CODE(2601, Integrity),   // duplicate key (unique index)
    DB_ERR_CODE(2627, Integrity),   // dup. key (constr. viol., primary key?)
    DB_ERR_CODE(2812, Programming), // unknown stored procedure
    DB_ERR_CODE(4104, Programming)  // multi-part ID couldn't be bound
};

#undef DB_ERR_CODE

DEFINE_STATIC_ARRAY_MAP(TDBErrCodeMap, sc_ClientErrCodes, kClientErrCodes);
DEFINE_STATIC_ARRAY_MAP(TDBErrCodeMap, sc_SybaseErrCodes, kSybaseErrCodes);

static NCBI_NORETURN
void s_ThrowDatabaseError(const CException& e)
{
    const CDB_Exception* dbe = dynamic_cast<const CDB_Exception*>(&e);
    if (dbe == NULL) {
        if (dynamic_cast<const CInvalidConversionException*>(&e) != NULL) {
            throw CDataError(e.what());
        } else {
            throw CDatabaseError(e.what());
        }
    }
    
    if (dbe->GetSybaseSeverity() != 0  ||  dbe->GetSeverity() == eDiag_Info) {
        // Sybase (including OpenServer) or MSSQL.  There are theoretical
        // false positives in the eDiag_Info case, but (at least with the
        // standard drivers) they will only ever arise for the ODBC driver,
        // and only accompanied by a DB error code of zero.
        TDBErrCodeMap::const_iterator it
            = sc_SybaseErrCodes.find(dbe->GetDBErrCode());
        if (it != sc_SybaseErrCodes.end()) {
            it->second(*dbe);
        }
        // throw COperationalError(*dbe);
    }

    switch (dbe->GetErrCode()) {
    // case CDB_Exception::eDeadlock: // Could stem from a programming error
    case CDB_Exception::eDS:
    case CDB_Exception::eTimeout:
        throw COperationalError(*dbe);

    case CDB_Exception::eClient:
    {
        TDBErrCodeMap::const_iterator it
            = sc_SybaseErrCodes.find(dbe->GetDBErrCode());
        if (it != sc_SybaseErrCodes.end()) {
            it->second(*dbe);
        }
        throw COperationalError(*dbe);
    }

    default:
        break;
    }

    throw CDatabaseError(*dbe); // no more specific classification
}

//////////////////////////////////////////////////////////////////////////////
CConnection::CConnection(
    const string& driver_name,
    const string& db_type,
    const string& server_name,
    const string& db_name,
    const string& user_name,
    const string& user_pswd,
    const pythonpp::CObject& extra_params
    )
: m_DefParams(server_name, user_name, user_pswd)
, m_Params(m_DefParams)
, m_DM(CDriverManager::GetInstance())
, m_DS(NULL)
, m_DefTransaction( NULL )
, m_ConnectionMode(eSimpleMode)
{
    try {
        m_DefParams.SetDriverName(driver_name);
        m_DefParams.SetDatabaseName(db_name);

        // Set up a server type ...
        string db_type_uc = db_type;
        NStr::ToUpper(db_type_uc);

        if ( db_type_uc == "SYBASE"  ||  db_type_uc == "SYB" ) {
            m_DefParams.SetServerType(CDBConnParams::eSybaseSQLServer);
        } else if ( db_type_uc == "MYSQL" ) {
            m_DefParams.SetServerType(CDBConnParams::eMySQL);
        } else if ( db_type_uc == "MSSQL" ||
                db_type_uc == "MS_SQL" ||
                db_type_uc == "MS SQL")
        {
            m_DefParams.SetServerType(CDBConnParams::eMSSqlServer);
            m_DefParams.SetEncoding(eEncoding_UTF8);
        }


        // Handle extra-parameters ...
        if (!pythonpp::CNone::HasSameType(extra_params)) {
            if (pythonpp::CDict::HasSameType(extra_params)) {
                const pythonpp::CDict dict = extra_params;

                // Iterate over a dict.
                pythonpp::py_ssize_t i = 0;
                PyObject* key;
                PyObject* value;
                while ( PyDict_Next(dict, &i, &key, &value) ) {
                    // Refer to borrowed references in key and value.
                    const string param_name = pythonpp::CString(key);
                    const string param_value = pythonpp::CString(value);

                    m_DefParams.SetParam(param_name, param_value);
                }
            } else if (pythonpp::CBool::HasSameType(extra_params)) {
                bool support_standard_interface = pythonpp::CBool(extra_params);
                m_ConnectionMode = (support_standard_interface ? eStandardMode : eSimpleMode);
            } else {
                throw CNotSupportedError("Expected dictionary as an argument.");
            }
        }

        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
        // Make a datasource ...
        m_DS = m_DM.MakeDs(m_Params);

        // Set up message handlers ...
        I_DriverContext* drv_context = m_DS->GetDriverContext();

        drv_context->PushCntxMsgHandler(
                new CDB_UserHandler_Exception,
                eTakeOwnership
                );

        drv_context->PushDefConnMsgHandler(
                new CDB_UserHandler_Exception,
                eTakeOwnership
                );
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    ROAttr( "__class__", GetTypeObject() );
    PrepareForPython(this);

    try {
        // Create a default transaction ...
        // m_DefTransaction = new CTransaction(this, pythonpp::eBorrowed, m_ConnectionMode);
        m_DefTransaction = new CTransaction(this, pythonpp::eAcquired, m_ConnectionMode);
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
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
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

    _ASSERT(m_DS);
    // !!! eTakeOwnership !!!
    IConnection* connection = m_DS->CreateConnection( eTakeOwnership );
    connection->Connect(m_Params);
    return connection;
}

CTransaction*
CConnection::CreateTransaction(void)
{
    CTransaction* trans = NULL;

    {{
        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
        trans = new CTransaction(this, pythonpp::eOwned, m_ConnectionMode);
    }}

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
CConnection::__enter__(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(this);
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
            pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
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
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
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
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

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
        catch(const CException& e) {
            s_ThrowDatabaseError(e);
        }
    }
}

void
CDMLConnPool::rollback(void) const
{
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

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
        catch(const CException& e) {
            s_ThrowDatabaseError(e);
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

    ROAttr( "__class__", GetTypeObject() );
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
CTransaction::__enter__(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(this);
}

pythonpp::CObject
CTransaction::close(const pythonpp::CTuple& args)
{
    try {
        CloseInternal();
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    // Unregister this transaction with the parent connection ...
    // I'm not absolutely shure about this ... 1/24/2005 5:31PM
    // GetConnection().DestroyTransaction(this);

    return pythonpp::CNone();
}

pythonpp::CObject
CTransaction::cursor(const pythonpp::CTuple& args)
{
    try {
        return pythonpp::CObject(CreateCursor(), pythonpp::eTakeOwnership);
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

pythonpp::CObject
CTransaction::commit(const pythonpp::CTuple& args)
{
    try {
        CommitInternal();
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CTransaction::rollback(const pythonpp::CTuple& args)
{
    try {
        RollbackInternal();
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

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
        string::size_type pos_end = stmt.find_first_of(" \t\n", pos);
        if (pos_end == string::npos)
            pos_end = stmt.size();
        CTempString first_word(&stmt[pos], pos_end - pos);

        // "CREATE" should be before DML ...
        if (NStr::EqualNocase(first_word, "CREATE"))
        {
            stmtType = estCreate;
        } else if (NStr::EqualNocase(first_word, "SELECT"))
        {
            stmtType = estSelect;
        } else if (NStr::EqualNocase(first_word, "UPDATE"))
        {
            stmtType = estUpdate;
        } else if (NStr::EqualNocase(first_word, "DELETE"))
        {
            stmtType = estDelete;
        } else if (NStr::EqualNocase(first_word, "INSERT"))
        {
            stmtType = estInsert;
        } else if (NStr::EqualNocase(first_word, "DROP"))
        {
            stmtType = estDrop;
        } else if (NStr::EqualNocase(first_word, "ALTER"))
        {
            stmtType = estAlter;
        } else if (NStr::EqualNocase(first_word, "BEGIN"))
        {
            stmtType = estTransaction;
        } else if (NStr::EqualNocase(first_word, "COMMIT"))
        {
            stmtType = estTransaction;
        } else if (NStr::EqualNocase(first_word, "ROLLBACK"))
        {
            stmtType = estTransaction;
        }
    }

    return stmtType;
}

//////////////////////////////////////////////////////////////////////////////
CStmtHelper::CStmtHelper(CTransaction* trans)
: m_ParentTransaction( trans )
, m_RS(NULL)
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
, m_UserHandler(NULL)
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
, m_UserHandler(NULL)
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    CreateStmt(NULL);
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
    pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

    if ( m_Stmt.get() && m_Executed ) {
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS.reset(m_Stmt->GetResultSet());
            }
        }
    }
    m_RS.reset();
}

void
CStmtHelper::ReleaseStmt(void)
{
    if ( m_Stmt.get() ) {
        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

        IConnection* conn = m_Stmt->GetParentConn();

        // Release the statement before a connection release because it is a child object for a connection.
        m_RS.reset();
        m_Stmt.reset();

        _ASSERT( m_StmtStr.GetType() != estNone );

        if (m_UserHandler) {
            conn->GetCDB_Connection()->PopMsgHandler(m_UserHandler);
            m_UserHandler = NULL;
        }

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
CStmtHelper::CreateStmt(CDB_UserHandler* handler)
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

    if (handler) {
        m_Stmt->GetParentConn()->GetCDB_Connection()->PushMsgHandler(handler);
        m_UserHandler = handler;
    }
}

void
CStmtHelper::SetStr(const CStmtStr& stmt, CDB_UserHandler* handler)
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
            CreateStmt(handler);
        } else {
            DumpResult();
            m_Stmt->ClearParamList();
        }
    } else {
        CreateStmt(handler);
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
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

void
CStmtHelper::SetParam(size_t index, const CVariant& value)
{
    _ASSERT( m_Stmt.get() );

    try {
        m_Stmt->SetParam( value, static_cast<unsigned int>(index) );
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

void
CStmtHelper::Execute(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

        m_RS.reset();
        switch ( m_StmtStr.GetType() ) {
        case estSelect :
            m_Stmt->Execute ( m_StmtStr.GetStr() );
            break;
        default:
            m_Stmt->ExecuteUpdate ( m_StmtStr.GetStr() );
        }
        m_Executed = true;
    }
    catch(const bad_cast&) {
        throw CInternalError("std::bad_cast exception within 'CStmtHelper::Execute'");
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
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
    if ( m_RS.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

const IResultSet&
CStmtHelper::GetRS(void) const
{
    if ( m_RS.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return *m_RS;
}

bool
CStmtHelper::HasRS(void) const
{
    return m_RS.get() != NULL;
}

int
CStmtHelper::GetReturnStatus(void)
{
    if ( !m_ResultStatusAvailable ) {
        throw CProgrammingError("Procedure return code is not defined within this context.");
    }
    return m_ResultStatus;
}

bool
CStmtHelper::MoveToNextRS(void)
{
    _ASSERT( m_Stmt.get() );

    try {
        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
        while ( m_Stmt->HasMoreResults() ) {
            if ( m_Stmt->HasRows() ) {
                m_RS.reset(m_Stmt->GetResultSet());
                if ( m_RS->GetResultType() == eDB_StatusResult ) {
                    m_RS->Next();
                    m_ResultStatus = m_RS->GetVariant(1).GetInt4();
                    m_ResultStatusAvailable = true;
                    m_RS.reset();
                }
                else {
                    return true;
                }
            }
        }
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    return false;
}

static void
s_FillDescription(pythonpp::CList& descr, const IResultSetMetaData* data)
{
    descr.Clear();

    unsigned int cnt = data->GetTotalColumns();
    for (unsigned int i = 1; i <= cnt; ++i) {
        pythonpp::CList col_list;
        IncRefCount(col_list);
        col_list.Append(pythonpp::CString(data->GetName(i)));
        switch (data->GetType(i)) {
        case eDB_Int:
        case eDB_SmallInt:
        case eDB_TinyInt:
        case eDB_BigInt:
        case eDB_Float:
        case eDB_Double:
        case eDB_Bit:
        case eDB_Numeric:
            col_list.Append((PyObject*) &python::CNumber::GetType());
            break;
        case eDB_VarChar:
        case eDB_Char:
        case eDB_LongChar:
        case eDB_Text:
            col_list.Append((PyObject*) &python::CStringType::GetType());
            break;
        case eDB_VarBinary:
        case eDB_Binary:
        case eDB_LongBinary:
        case eDB_Image:
            col_list.Append((PyObject*) &python::CBinary::GetType());
            break;
        case eDB_DateTime:
        case eDB_SmallDateTime:
            col_list.Append((PyObject*) &python::CDateTimeType::GetType());
            break;
        default:
            throw CInternalError("Invalid type of the column: "
                                 + NStr::IntToString(int(data->GetType(i))));
        };
        col_list.Append(pythonpp::CNone());  // display_size
        col_list.Append(pythonpp::CInt(data->GetMaxSize(i)));  // internal_size
        col_list.Append(pythonpp::CNone());  // precision
        col_list.Append(pythonpp::CNone());  // scale
        col_list.Append(pythonpp::CNone());  // null_ok

        descr.Append(col_list);
    }
}

void
CStmtHelper::FillDescription(pythonpp::CList& descr)
{
    s_FillDescription(descr, m_RS->GetMetaData());
}

//////////////////////////////////////////////////////////////////////////////
CCallableStmtHelper::CCallableStmtHelper(CTransaction* trans)
: m_ParentTransaction( trans )
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
, m_UserHandler(NULL)
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }
}

CCallableStmtHelper::CCallableStmtHelper(CTransaction* trans, const CStmtStr& stmt)
: m_ParentTransaction( trans )
, m_StmtStr( stmt )
, m_Executed( false )
, m_ResultStatus( 0 )
, m_ResultStatusAvailable( false )
, m_UserHandler(NULL)
{
    if ( m_ParentTransaction == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    CreateStmt(NULL);
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
    if ( m_Stmt.get() ) {
        if (m_RSProxy.get()) {
            m_RSProxy->DumpResult();
        }
    }
}

void
CCallableStmtHelper::ReleaseStmt(void)
{
    if ( m_Stmt.get() ) {
        pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;

        IConnection* conn = m_Stmt->GetParentConn();

        // Release the statement before a connection release because it is a child object for a connection.
        m_Stmt.reset();

        _ASSERT( m_StmtStr.GetType() != estNone );

        if (m_UserHandler) {
            conn->GetCDB_Connection()->PopMsgHandler(m_UserHandler);
            m_UserHandler = NULL;
        }

        // Release DML Connection ...
        m_ParentTransaction->DestroyDMLConnection( conn );
        m_Executed = false;
        m_ResultStatus = 0;
        m_ResultStatusAvailable = false;
    }
}

void
CCallableStmtHelper::CreateStmt(CDB_UserHandler* handler)
{
    _ASSERT( m_StmtStr.GetType() == estFunction );

    ReleaseStmt();
    m_Stmt.reset( m_ParentTransaction->CreateDMLConnection()->GetCallableStatement(m_StmtStr.GetStr()) );

    if (handler) {
        m_Stmt->GetParentConn()->GetCDB_Connection()->PushMsgHandler(handler);
        m_UserHandler = handler;
    }
}

void
CCallableStmtHelper::SetStr(const CStmtStr& stmt, CDB_UserHandler* handler)
{
    m_StmtStr = stmt;

    DumpResult();
    CreateStmt(handler);

    m_Executed = false;
    m_ResultStatus = 0;
    m_ResultStatusAvailable = false;
}

void
CCallableStmtHelper::SetParam(const string& name, const CVariant& value, bool& output_param)
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
            output_param = false;
        } else {
            if (value.IsNull()) {
                CVariant temp_val(m_Stmt->GetParamsMetaData().GetType(name));
                m_Stmt->SetOutputParam( temp_val, param_name );
            }
            else {
                m_Stmt->SetOutputParam( value, param_name );
            }
            output_param = true;
        }
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

void
CCallableStmtHelper::SetParam(size_t index, const CVariant& value, bool& output_param)
{
    _ASSERT( m_Stmt.get() );

    try {
        unsigned int ind = static_cast<unsigned int>(index);
        if (m_Stmt->GetParamsMetaData().GetDirection(ind) == CDBParams::eIn) {
            m_Stmt->SetParam( value, ind );
            output_param = false;
        } else {
            if (value.IsNull()) {
                CVariant temp_val(m_Stmt->GetParamsMetaData().GetType(ind));
                m_Stmt->SetOutputParam( temp_val, ind );
            }
            else {
                m_Stmt->SetOutputParam( value, ind );
            }
            output_param = true;
        }
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

void
CCallableStmtHelper::Execute(bool cache_results)
{
    _ASSERT( m_Stmt.get() );

    try {
        m_ResultStatus = 0;
        m_ResultStatusAvailable = false;

        {{
            pythonpp::CThreadingGuard ALLOW_OTHER_THREADS;
            m_Stmt->Execute();
        }}

        // Retrieve a resut if there is any ...
        if (cache_results) {
            m_RSProxy.reset(new CVariantSetProxy(*m_Stmt));
        } else {
            m_RSProxy.reset(new CRealSetProxy(*m_Stmt));
        }
        m_Executed = true;
    }
    catch(const bad_cast&) {
        throw CInternalError("std::bad_cast exception within 'CCallableStmtHelper::Execute'");
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
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

CVariantSet&
CCallableStmtHelper::GetRS(void)
{
    if ( m_RSProxy.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return m_RSProxy->GetRS();
}

const CVariantSet&
CCallableStmtHelper::GetRS(void) const
{
    if ( m_RSProxy.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return m_RSProxy->GetRS();
}

bool
CCallableStmtHelper::HasRS(void) const
{
    if (m_RSProxy.get()) {
        return m_RSProxy->HasRS();
    }

    return false;
}

int
CCallableStmtHelper::GetReturnStatus(void)
{
    if ( !m_ResultStatusAvailable ) {
        throw CProgrammingError("Procedure return code is not defined within this context.");
    }
    return m_Stmt->GetReturnStatus();
}

bool
CCallableStmtHelper::MoveToNextRS(void)
{
    if ( m_RSProxy.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    bool result = m_RSProxy->MoveToNextRS();

    if (!result) {
        m_ResultStatusAvailable = true;
    }

    return result;
}

bool
CCallableStmtHelper::MoveToLastRS(void)
{
    if ( m_RSProxy.get() == NULL ) {
        throw CProgrammingError("The previous call to executeXXX() did not produce any result set or no call was issued yet");
    }

    return m_RSProxy->MoveToLastRS();
}

void
CCallableStmtHelper::FillDescription(pythonpp::CList& descr)
{
    s_FillDescription(descr, &m_RSProxy->GetRS().GetMetaData());
}


//////////////////////////////////////////////////////////////////////////////
pythonpp::CObject
ConvertCVariant2PCObject(const CVariant& value)
{
    if ( value.IsNull() ) {
    return pythonpp::CObject();
    }

    switch ( value.GetType() ) {
    case eDB_Int :
        return pythonpp::CInt( value.GetInt4() );
    case eDB_SmallInt :
        return pythonpp::CInt( value.GetInt2() );
    case eDB_TinyInt :
        return pythonpp::CInt( value.GetByte() );
    case eDB_BigInt :
        return pythonpp::CLong( value.GetInt8() );
    case eDB_Float :
        return pythonpp::CFloat( value.GetFloat() );
    case eDB_Double :
        return pythonpp::CFloat( value.GetDouble() );
    case eDB_Bit :
        // BIT --> BOOL ...
        return pythonpp::CBool( value.GetBit() );
#if PY_VERSION_HEX >= 0x02040000
    case eDB_DateTime :
    case eDB_SmallDateTime :
        {
        const CTime& cur_time = value.GetCTime();
        return pythonpp::CDateTime(
            cur_time.Year(),
            cur_time.Month(),
            cur_time.Day(),
            cur_time.Hour(),
            cur_time.Minute(),
            cur_time.Second(),
            cur_time.NanoSecond() / 1000
            );
        }
#endif
    case eDB_VarChar :
    case eDB_Char :
    case eDB_LongChar :
        {
        string str = value.GetString();
        return pythonpp::CString( str );
        }
    case eDB_LongBinary :
    case eDB_VarBinary :
    case eDB_Binary :
    case eDB_Numeric :
        return pythonpp::CString( value.GetString() );
    case eDB_Text :
    case eDB_Image :
        {
        size_t lob_size = value.GetBlobSize();
        string tmp_str;

        tmp_str.resize(lob_size);
        value.Read( (void*)tmp_str.data(), lob_size );
        return pythonpp::CString(tmp_str);
        }
    case eDB_UnsupportedType :
        break;
    default:
        // All cases are supposed to be handled.
        // In case of PY_VERSION_HEX < 0x02040000 eDB_DateTime and
        // eDB_SmallDateTime will be missed.
        break;
    }

    return pythonpp::CObject();
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

        tuple[i] = ConvertCVariant2PCObject(value);
    }

    return tuple;
}

//////////////////////////////////////////////////////////////////////////////
pythonpp::CTuple
MakeTupleFromResult(CVariantSet& rs)
{
    // Set data. Make a sequence (tuple) ...
    int col_num = rs.GetTotalColumns();

    pythonpp::CTuple tuple(col_num);

    for ( int i = 0; i < col_num; ++i) {
        const CVariant& value = rs.GetVariant (i + 1);

        tuple[i] = ConvertCVariant2PCObject(value);
    }

    return tuple;
}


bool CInfoHandler_CursorCollect::HandleIt(CDB_Exception* ex)
{
    if (ex->GetSybaseSeverity() <= 10) {
        m_Cursor->AddInfoMessage(ex->GetMsg());
        return true;
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////////
CCursor::CCursor(CTransaction* trans)
: m_PythonConnection( &trans->GetParentConnection() )
, m_PythonTransaction( trans )
, m_ParentTransaction( trans )
, m_NumOfArgs( 0 )
, m_RowsNum( -1 )
, m_InfoHandler( this )
, m_ArraySize( 1 )
, m_StmtHelper( trans )
, m_CallableStmtHelper( trans )
, m_AllDataFetched( false )
, m_AllSetsFetched( false )
, m_Closed( false )
{
    if ( trans == NULL ) {
        throw CInternalError("Invalid CTransaction object");
    }

    ROAttr( "__class__", GetTypeObject() );
    // The following list should reflect exactly members set to CCursor type
    // in init_common().
    ROAttr( "rowcount", m_RowsNum );
    ROAttr( "messages", m_InfoMessages );
    ROAttr( "description", m_Description );

    IncRefCount(m_InfoMessages);
    IncRefCount(m_DescrList);

    m_Description = pythonpp::CNone();

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
    m_Closed = true;
}

void
CCursor::AddInfoMessage(const string& message)
{
    m_InfoMessages.Append(pythonpp::CString(message));
}

pythonpp::CObject
CCursor::callproc(const pythonpp::CTuple& args)
{
    if (m_Closed) {
        throw CProgrammingError("Cursor is closed");
    }

    try {
        const size_t args_size = args.size();

        m_RowsNum = -1;                     // As required by the specification ...
        m_AllDataFetched = false;
        m_AllSetsFetched = false;
        vector<size_t> out_params;

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
            m_CallableStmtHelper.SetStr(m_StmtStr, &m_InfoHandler);

            // Setup parameters ...
            if ( args_size > 1 ) {
                pythonpp::CObject obj( args[1] );

                if ( pythonpp::CDict::HasSameType(obj) ) {
                    const pythonpp::CDict dict(obj);
                    if (SetupParameters(dict, m_CallableStmtHelper)) {
                        // Put any number as below only size is used in this case
                        out_params.push_back(0);
                    }
                } else  if ( pythonpp::CList::HasSameType(obj)
                             ||  pythonpp::CTuple::HasSameType(obj) )
                {
                    const pythonpp::CSequence seq(obj);
                    SetupParameters(seq, m_CallableStmtHelper, &out_params);
                } else {
                    throw CNotSupportedError("Inappropriate type for parameter binding");
                }
            }
        }

        m_InfoMessages.Clear();
        m_CallableStmtHelper.Execute(out_params.size() != 0);
        m_RowsNum = m_CallableStmtHelper.GetRowCount();

        pythonpp::CObject output_args;
        if (args_size > 1  &&  out_params.size() != 0) {
            // If we have input parameters ...
            output_args.Set(args[1]);

            if (m_CallableStmtHelper.MoveToLastRS() && m_CallableStmtHelper.HasRS() ) {
                // We can have out/inout arguments ...
                CVariantSet& rs = m_CallableStmtHelper.GetRS();

                if ( rs.GetResultType() == eDB_ParamResult ) {
                    // We've got ParamResult with output arguments ...
                    if ( rs.Next() ) {
                        int col_num = rs.GetTotalColumns();
                        const IResultSetMetaData& md = rs.GetMetaData();

                        for ( int i = 0; i < col_num; ++i) {
                            const CVariant& value = rs.GetVariant (i + 1);

                            if ( pythonpp::CDict::HasSameType(output_args) ) {
                                // Dictionary ...
                                pythonpp::CDict dict(output_args);
                                const string param_name = md.GetName(i + 1);

                                dict.SetItem(param_name, ConvertCVariant2PCObject(value));
                            } else  if ( pythonpp::CList::HasSameType(output_args) ) {
                                pythonpp::CList lst(output_args);
                                lst.SetItem(out_params[i], ConvertCVariant2PCObject(value));
                            } else {
                                throw CNotSupportedError("Inappropriate type for parameter binding");
                            }
                        }
                    }
                }
            }
        }

        // Get RowResultSet ...
        if (m_CallableStmtHelper.MoveToNextRS()) {
            m_CallableStmtHelper.FillDescription(m_DescrList);
            m_Description = m_DescrList;
        }
        else {
            m_AllDataFetched = m_AllSetsFetched = true;
            m_Description = pythonpp::CNone();
        }

        return output_args;
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }
}

pythonpp::CObject
CCursor::__enter__(const pythonpp::CTuple& args)
{
    return pythonpp::CObject(this);
}

pythonpp::CObject
CCursor::close(const pythonpp::CTuple& args)
{
    try {
        CloseInternal();

        // Unregister this cursor with the parent transaction ...
        GetTransaction().DestroyCursor(this);
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    return pythonpp::CNone();
}

pythonpp::CObject
CCursor::execute(const pythonpp::CTuple& args)
{
    if (m_Closed) {
        throw CProgrammingError("Cursor is closed");
    }

    try {
        const size_t args_size = args.size();

        m_AllDataFetched = false;
        m_AllSetsFetched = false;

        // Process function's arguments ...
        if ( args_size == 0 ) {
            throw CProgrammingError("An SQL statement string is expected as a parameter");
        } else if ( args_size > 0 ) {
            pythonpp::CObject obj(args[0]);

            if ( pythonpp::CString::HasSameType(obj) ) {
                m_StmtStr.SetStr(pythonpp::CString(args[0]), estSelect);
            } else {
                throw CProgrammingError("An SQL statement string is expected as a parameter");
            }

            m_CallableStmtHelper.Close();
            m_StmtHelper.SetStr(m_StmtStr, &m_InfoHandler);

            // Setup parameters ...
            if ( args_size > 1 ) {
                pythonpp::CObject obj(args[1]);

                if ( pythonpp::CDict::HasSameType(obj) ) {
                    const pythonpp::CDict dict = obj;
                    SetupParameters(dict, m_StmtHelper);
                } else  if ( pythonpp::CList::HasSameType(obj)
                             ||  pythonpp::CTuple::HasSameType(obj) )
                {
                    const pythonpp::CSequence seq = obj;
                    SetupParameters(seq, m_StmtHelper);
                } else {
                    throw CNotSupportedError("Inappropriate type for parameter binding");
                }
            }
        }

        m_InfoMessages.Clear();
        m_StmtHelper.Execute();
        m_RowsNum = m_StmtHelper.GetRowCount();

        if (m_StmtHelper.MoveToNextRS()) {
            m_StmtHelper.FillDescription(m_DescrList);
            m_Description = m_DescrList;
        }
        else {
            m_AllDataFetched = m_AllSetsFetched = true;
            m_Description = pythonpp::CNone();
        }
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
    }

    return pythonpp::CObject(this);
}

PyObject*
CCursor::CreateIter(void)
{
    return new CCursorIter(this);
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
CCursor::SetupParameters(const pythonpp::CSequence& seq, CStmtHelper& stmt)
{
    // Iterate over a sequence.
    size_t sz = seq.size();
    for (size_t i = 0; i < sz; ++i) {
        const pythonpp::CObject value_obj(seq.GetItem(i));
        stmt.SetParam(i + 1, GetCVariant(value_obj));
    }
}

bool
CCursor::SetupParameters(const pythonpp::CDict& dict, CCallableStmtHelper& stmt)
{
    // Iterate over a dict.
    pythonpp::py_ssize_t i = 0;
    PyObject* key;
    PyObject* value;
    bool result = false;
    bool output_param = false;

    while ( PyDict_Next(dict, &i, &key, &value) ) {
        // Refer to borrowed references in key and value.
        const pythonpp::CObject key_obj(key);
        const pythonpp::CObject value_obj(value);
        string param_name = pythonpp::CString(key_obj);

        stmt.SetParam(param_name, GetCVariant(value_obj), output_param);
        result |= output_param;
    }
    return result;
}

void
CCursor::SetupParameters(const pythonpp::CSequence& seq,
                         CCallableStmtHelper&       stmt,
                         vector<size_t>*            out_params)
{
    // Iterate over a sequence.
    size_t sz = seq.size();

    for (size_t i = 0; i < sz; ++i) {
        // Refer to borrowed references in key and value.
        const pythonpp::CObject value_obj(seq.GetItem(i));

        bool output_param = false;
        stmt.SetParam(i + 1, GetCVariant(value_obj), output_param);
        if (output_param)
            out_params->push_back(i);
    }
}

CVariant
CCursor::GetCVariant(const pythonpp::CObject& obj) const
{
    if ( pythonpp::CNone::HasSameType(obj) ) {
        return CVariant(eDB_VarChar);
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
        const string std_str(python_str);
        return CVariant( std_str );
#if PY_VERSION_HEX >= 0x02040000
    } else if ( pythonpp::CDateTime::HasSameType(obj) ) {
        const pythonpp::CDateTime python_date(obj);
        const CTime std_date(python_date.GetYear(),
                             python_date.GetMonth(),
                             python_date.GetDay(),
                             python_date.GetHour(),
                             python_date.GetMinute(),
                             python_date.GetSecond(),
                             python_date.GetMicroSecond() * 1000);
        return CVariant( std_date, eLong );
    } else if ( pythonpp::CDate::HasSameType(obj) ) {
        const pythonpp::CDate python_date(obj);
        const CTime std_date(python_date.GetYear(),
                             python_date.GetMonth(),
                             python_date.GetDay());
        return CVariant( std_date, eLong );
    } else if ( pythonpp::CTime::HasSameType(obj) ) {
        const pythonpp::CTime python_time(obj);
        CTime std_date(CTime::eCurrent);
        std_date.SetHour(python_time.GetHour());
        std_date.SetMinute(python_time.GetMinute());
        std_date.SetSecond(python_time.GetSecond());
        std_date.SetMicroSecond(python_time.GetMicroSecond());
        return CVariant( std_date, eLong );
#endif
    } else if (obj == CBinary::GetType()) {
        const string value = static_cast<CBinary*>(obj.Get())->GetValue();
        return CVariant::VarBinary(value.data(), value.size());
    }

    return CVariant(eDB_UnsupportedType);
}

pythonpp::CObject
CCursor::executemany(const pythonpp::CTuple& args)
{
    if (m_Closed) {
        throw CProgrammingError("Cursor is closed");
    }

    try {
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
                    m_StmtHelper.SetStr( m_StmtStr, &m_InfoHandler );
                    m_RowsNum = 0;
                    m_InfoMessages.Clear();

                    for ( citer = params.begin(); citer != cend; ++citer ) {
                        if ( pythonpp::CDict::HasSameType(*citer) ) {
                            const pythonpp::CDict dict = *citer;
                            SetupParameters(dict, m_StmtHelper);
                        } else  if ( pythonpp::CList::HasSameType(*citer)
                                     ||  pythonpp::CTuple::HasSameType(*citer) )
                        {
                            const pythonpp::CSequence seq = *citer;
                            SetupParameters(seq, m_StmtHelper);
                        } else {
                            throw CNotSupportedError("Inappropriate type for parameter binding");
                        }
                        m_StmtHelper.Execute();
                        m_RowsNum += m_StmtHelper.GetRowCount();
                    }

                    if (m_StmtHelper.MoveToNextRS()) {
                        m_StmtHelper.FillDescription(m_DescrList);
                        m_Description = m_DescrList;
                    }
                    else {
                        m_AllDataFetched = m_AllSetsFetched = true;
                        m_Description = pythonpp::CNone();
                    }
                } else {
                    throw CProgrammingError("Sequence of parameters should be provided either as a list or as a tuple data type");
                }
            } else {
                throw CProgrammingError("A sequence of parameters is expected with the 'executemany' function");
            }
        }
    }
    catch(const CException& e) {
        s_ThrowDatabaseError(e);
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
            CVariantSet& rs = m_CallableStmtHelper.GetRS();

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
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
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
            CVariantSet& rs = m_CallableStmtHelper.GetRS();

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
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
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
            if (m_CallableStmtHelper.HasRS()) {
                CVariantSet& rs = m_CallableStmtHelper.GetRS();

                while ( rs.Next() ) {
                    py_list.Append(MakeTupleFromResult(rs));
                }

                m_RowsNum = m_CallableStmtHelper.GetRowCount();
            }
        } else {
            if (m_StmtHelper.HasRS()) {
                IResultSet& rs = m_StmtHelper.GetRS();

                while ( rs.Next() ) {
                    py_list.Append(MakeTupleFromResult(rs));
                }

                m_RowsNum = m_StmtHelper.GetRowCount();
            }
        }
    }
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
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
                if (m_CallableStmtHelper.HasRS()) {
                    if ( m_CallableStmtHelper.MoveToNextRS() ) {
                        m_AllDataFetched = false;
                        return true;
                    }
                } else {
                    return false;
                }
            } else {
                if (m_StmtHelper.HasRS()) {
                    if ( m_StmtHelper.MoveToNextRS() ) {
                        m_AllDataFetched = false;
                        return true;
                    }
                } else {
                    return false;
                }
            }
        }
    }
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
    }

    m_AllDataFetched = true;
    m_AllSetsFetched = true;

    return false;
}

pythonpp::CObject
CCursor::nextset(const pythonpp::CTuple& args)
{
    try {
        if ( NextSetInternal() ) {
            if (m_StmtStr.GetType() == estFunction) {
                m_CallableStmtHelper.FillDescription(m_DescrList);
            }
            else {
                m_StmtHelper.FillDescription(m_DescrList);
            }
            m_Description = m_DescrList;
            return pythonpp::CBool(true);
        }
        m_Description = pythonpp::CNone();
    }
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
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
    if (m_Closed) {
        throw CProgrammingError("Cursor is closed");
    }

    try {
        if ( !m_AllDataFetched ) {
            throw CProgrammingError("Call get_proc_return_status after you retrieve all data.");
        }

        NextSetInternal();

        if ( !m_AllSetsFetched ) {
            throw CProgrammingError("Call get_proc_return_status after you retrieve all data.");
        }

        if ( m_StmtStr.GetType() == estFunction ) {
            return pythonpp::CInt( m_CallableStmtHelper.GetReturnStatus() );
        } else {
            return pythonpp::CInt( m_StmtHelper.GetReturnStatus() );
        }
    }
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
    }

    return pythonpp::CNone();
}


extern "C"
PyObject*
s_GetCursorIter(PyObject* curs_obj)
{
    CCursor* cursor = (CCursor*)curs_obj;
    return cursor->CreateIter();
}


CCursorIter::CCursorIter(CCursor* cursor)
    : m_PythonCursor(cursor),
      m_Cursor(cursor),
      m_StopIter(false)
{
    PrepareForPython(this);
}

CCursorIter::~CCursorIter(void)
{}

PyObject*
CCursorIter::GetNext(void)
{
    if (!m_StopIter) {
        pythonpp::CObject row = m_Cursor->fetchone(pythonpp::CTuple());
        if (!pythonpp::CNone::HasExactSameType(row))
            return IncRefCount(row);
        m_StopIter = true;
    }
    return NULL;
}


extern "C"
PyObject*
s_GetCursorIterFromIter(PyObject* iter_obj)
{
    return iter_obj;
}

extern "C"
PyObject*
s_CursorIterNext(PyObject* iter_obj)
{
    CCursorIter* iter = (CCursorIter*)iter_obj;
    return iter->GetNext();
}


///////////////////////////////////////////////////////////////////////////////
CWarning::CWarning(const string& msg)
: pythonpp::CUserError<CWarning>( msg )
{
}

///////////////////////////////////////////////////////////////////////////////
CError::CError(const string& msg, PyObject* err_type)
: pythonpp::CUserError<CError>(msg, err_type)
{
}

void CError::x_Init(const CDB_Exception& e, PyObject* err_type)
{
    const CException* cur_exception = &e;
    string srv_msg;

    for (; cur_exception; cur_exception = cur_exception->GetPredecessor()) {
        /* Collect all messages ...
        if (!srv_msg.empty() && !cur_exception->GetMsg().empty()) {
            srv_msg += " ";
        }

        srv_msg += cur_exception->GetMsg();
        */

        // Get the last message only ...
        srv_msg = cur_exception->GetMsg();
    }

    x_Init(e.what(), e.GetDBErrCode(), srv_msg, err_type);
}

void CError::x_Init(const string& msg, long db_errno, const string& db_msg,
                    PyObject* err_type)
{
    PyObject *exc_ob = NULL;
    PyObject *errno_ob = NULL;
    PyObject *msg_ob = NULL;

    // Make an integer for the error code.
    errno_ob = PyInt_FromLong(db_errno);
    if (errno_ob == NULL) {
        return;
    }

    msg_ob = PyString_FromStringAndSize(db_msg.data(), db_msg.size());
    if (errno_ob == NULL) {
        Py_DECREF(errno_ob);
        return;
    }

    // Instantiate a Python exception object.
    exc_ob = PyObject_CallFunction(err_type, (char *)"s", (char*)msg.c_str());
    if (exc_ob == NULL)
    {
        Py_DECREF(errno_ob);
        Py_DECREF(msg_ob);
        return;
    }

    //  Set the "db_errno" attribute of the exception to our error code.
    if (PyObject_SetAttrString(exc_ob, (char *)"srv_errno", errno_ob) == -1)
    {
        Py_DECREF(errno_ob);
        Py_DECREF(msg_ob);
        Py_DECREF(exc_ob);
        return;
    }

    //  Finished with the errno_ob object.
    Py_DECREF(errno_ob);

    //  Set the "db_msg" attribute of the exception to our message.
    if (PyObject_SetAttrString(exc_ob, (char *)"srv_msg", msg_ob) == -1)
    {
        Py_DECREF(msg_ob);
        Py_DECREF(exc_ob);
        return;
    }

    //  Finished with the msg_ob object.
    Py_DECREF(msg_ob);

    //  Set the error state to our exception object.
    PyErr_SetObject(err_type, exc_ob);

    //  Finished with the exc_ob object.
    Py_DECREF(exc_ob);
}

///////////////////////////////////////////////////////////////////////////////
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
    catch (const CException& e) {
        s_ThrowDatabaseError(e);
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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

// Genetal declarations ...
namespace python
{
//////////////////////////////////////////////////////////////////////////////
// return_strs_as_unicode(flag_value)
static
PyObject*
ReturnStrsAsUnicode(PyObject *self, PyObject *args)
{
    try{
        const pythonpp::CTuple func_args(args);
        g_PythonStrDefToUnicode = pythonpp::CBool(func_args[0]);
    }
    catch (const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch (...) {
        pythonpp::CError::SetString("Unknown error in python_ncbi_dbapi::Connect");
    }

    return pythonpp::CNone().Get();
}

static
PyObject*
ReleaseGlobalLock(PyObject *self, PyObject *args)
{
    try{
        const pythonpp::CTuple func_args(args);
        pythonpp::CThreadingGuard::SetMayRelease
            (pythonpp::CBool(func_args[0]));
    }
    catch (const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch (...) {
        pythonpp::CError::SetString
            ("Unknown error in python_ncbi_dbapi::ReleaseGlobalLock");
    }

    return pythonpp::CNone().Get();
}

//////////////////////////////////////////////////////////////////////////////
// connect(driver_name, db_type, db_name, user_name, user_pswd)
static
PyObject*
Connect(PyObject *self, PyObject *args)
{
    CConnection* conn = NULL;

    try {
        // Debugging ...
        // throw  python::CDatabaseError("oops ...");
        // throw  python::CDatabaseError("oops ...", 200, "Blah-blah-blah");

        string driver_name;
        string db_type;
        string server_name;
        string db_name;
        string user_name;
        string user_pswd;
        pythonpp::CObject extra_params = pythonpp::CNone();

        try {
            const pythonpp::CTuple func_args(args);

            driver_name = pythonpp::CString(func_args[0]);
            db_type = pythonpp::CString(func_args[1]);
            server_name = pythonpp::CString(func_args[2]);
            db_name = pythonpp::CString(func_args[3]);
            user_name = pythonpp::CString(func_args[4]);
            user_pswd = pythonpp::CString(func_args[5]);
            if ( func_args.size() > 6 ) {
                extra_params = func_args[6];
            }
        } catch (const pythonpp::CError&) {
            throw CProgrammingError("Invalid parameters within 'connect' function");
        }

        conn = new CConnection(
            driver_name,
            db_type,
            server_name,
            db_name,
            user_name,
            user_pswd,
            extra_params
            );
    }
    catch (const CDB_Exception& e) {
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
    }
    catch (const pythonpp::CError&) {
        // An error message is already set by an exception ...
        return NULL;
    }
    catch (...) {
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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

        obj = new CBinary(value);
    }
    catch (const CDB_Exception& e) {
        s_ThrowDatabaseError(e);
    }
    catch (const CException& e) {
        pythonpp::CError::SetString(e.what());
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
    {(char*)"return_strs_as_unicode", (PyCFunction) python::ReturnStrsAsUnicode, METH_VARARGS, (char*)
        "return_strs_as_unicode(bool_flag_value) "
        "-- set global flag indicating that all strings returned from database "
        "should be presented to Python as unicode strings (if value is True) or "
        " as regular strings in UTF-8 encoding (if value is False - the default one). "
        "NOTE:  This is not a part of the Python Database API Specification v2.0."
    },
    {(char*)"release_global_lock", (PyCFunction) python::ReleaseGlobalLock, METH_VARARGS, (char*)
        "release_global_lock(bool_flag_value) "
        "-- set global flag indicating that blocking database operations "
        "should run with Python's global interpreter lock released (if value "
        "is True) or with it held (if value is False, the default for now)."
        "NOTE:  This is not a part of the Python Database API Specification "
        "v2.0."
    },
    {(char*)"connect", (PyCFunction) python::Connect, METH_VARARGS, (char*)
        "connect(driver_name, db_type, server_name, database_name, userid, password) "
        "-- connect to the "
        "driver_name; db_type; server_name; database_name; userid; password;"
    },
    {(char*)"Date", (PyCFunction) python::Date, METH_VARARGS, (char*)"Date"},
    {(char*)"Time", (PyCFunction) python::Time, METH_VARARGS, (char*)"Time"},
    {(char*)"Timestamp", (PyCFunction) python::Timestamp, METH_VARARGS, (char*)"Timestamp"},
    {(char*)"DateFromTicks", (PyCFunction) python::DateFromTicks, METH_VARARGS, (char*)"DateFromTicks"},
    {(char*)"TimeFromTicks", (PyCFunction) python::TimeFromTicks, METH_VARARGS, (char*)"TimeFromTicks"},
    {(char*)"TimestampFromTicks", (PyCFunction) python::TimestampFromTicks, METH_VARARGS, (char*)"TimestampFromTicks"},
    {(char*)"Binary", (PyCFunction) python::Binary, METH_VARARGS, (char*)"Binary"},
    { NULL, NULL }
};

///////////////////////////////////////////////////////////////////////////////
// DatabaseErrorExt extends PyBaseExceptionObject
/* DO NOT delete this code ...
struct PyDatabaseErrorExtObject : public PyBaseExceptionObject
{
    PyObject *db_message;
    PyObject *db_code;
};


static int
DatabaseErrorExt_init(PyDatabaseErrorExtObject *self, PyObject *args, PyObject *kwds)
{
    if (PyExc_BaseException->ob_type->tp_init((PyObject *)self, args, kwds) < 0) {
        return -1;
    }

    PyObject* message = NULL;
    PyObject *db_message = NULL, *db_code = NULL, *tmp;

    static char *kwlist[] = {"message", "db_message", "db_code", NULL};

    if (! PyArg_ParseTupleAndKeywords(
            args,
            kwds,
            "S|SO",
            kwlist,
            &message,
            &db_message,
            &db_code
            )
        )
    {
        return -1;
    }

    if (db_message) {
        tmp = self->db_message;
        Py_INCREF(db_message);
        self->db_message = db_message;
        Py_DECREF(tmp);
    }

    if (db_code) {
        // We are reading an object. Let us check the type.
        if (! PyInt_CheckExact(db_code)) {
            PyErr_SetString(PyExc_TypeError,
                    "The second attribute value must be an int or a long");
            return -1;
        }

        tmp = self->db_code;
        Py_INCREF(db_code);
        self->db_code = db_code;
        Py_DECREF(tmp);
    }

    return 0;
}

static PyObject *
DatabaseErrorExt_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyDatabaseErrorExtObject *self;

    self = (PyDatabaseErrorExtObject *)type->tp_alloc(type, 0);
    // self = (PyDatabaseErrorExtObject *)type->tp_new(type, args, kwds);
    if (self != NULL) {
        self->db_message = PyString_FromString("");
        if (self->db_message == NULL)
        {
            Py_DECREF(self);
            return NULL;
        }

        self->db_code = PyLong_FromLong(0);
        if (self->db_code == NULL)
        {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}

static PyObject *
DatabaseErrorExt_get_db_message(PyDatabaseErrorExtObject *self, void* )
{
    Py_INCREF(self->db_message);
    return self->db_message;
}

static int
DatabaseErrorExt_set_db_message(PyDatabaseErrorExtObject *self, PyObject *value, void* )
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the db_message attribute");
        return -1;
    }

    if (! PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError,
                "The first attribute value must be a string");
        return -1;
    }

    Py_DECREF(self->db_message);
    Py_INCREF(value);
    self->db_message = value;

    return 0;
}

static PyObject *
DatabaseErrorExt_get_db_code(PyDatabaseErrorExtObject *self, void *closure)
{
    Py_INCREF(self->db_code);
    return self->db_code;
}

static int
DatabaseErrorExt_set_db_code(PyDatabaseErrorExtObject *self, PyObject *value, void *closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the db_code attribute");
        return -1;
    }

    if (! PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError,
                "The second attribute value must be a long");
        return -1;
    }

    Py_DECREF(self->db_code);
    Py_INCREF(value);
    self->db_code = value;

    return 0;
}

static PyGetSetDef DatabaseErrorExt_getseters[] = {
    {"db_message", (getter)DatabaseErrorExt_get_db_message, (setter)DatabaseErrorExt_set_db_message, "Database message", NULL},
    {"db_code", (getter)DatabaseErrorExt_get_db_code, (setter)DatabaseErrorExt_set_db_code, "Database error code", NULL},
    {NULL}
};

static int
DatabaseErrorExt_traverse(PyDatabaseErrorExtObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->db_message);
    Py_VISIT(self->db_code);

    return 0;
}

static int
DatabaseErrorExt_clear(PyDatabaseErrorExtObject *self)
{
    Py_CLEAR(self->db_message);
    Py_CLEAR(self->db_code);

    return 0;
}

static void
DatabaseErrorExt_dealloc(PyDatabaseErrorExtObject *self)
{
    // ???
    // PyExc_BaseException->ob_type->tp_dealloc((PyObject *)self);

    DatabaseErrorExt_clear(self);

    self->ob_type->tp_free((PyObject *)self);
}
*/


// static PyTypeObject DatabaseErrorExtType = {
//     PyObject_HEAD_INIT(NULL)
//     0,                       /* ob_size */
//     "python_ncbi_dbapi.DatabaseErrorExt", /* tp_name */
//     sizeof(PyDatabaseErrorExtObject), /* tp_basicsize */
//     0,                       /* tp_itemsize */
//     (destructor)DatabaseErrorExt_dealloc,  /* tp_dealloc */
//     0,                       /* tp_print */
//     0,                       /* tp_getattr */
//     0,                       /* tp_setattr */
//     0,                       /* tp_compare */
//     0,                       /* tp_repr */
//     0,                       /* tp_as_number */
//     0,                       /* tp_as_sequence */
//     0,                       /* tp_as_mapping */
//     0,                       /* tp_hash */
//     0,                       /* tp_call */
//     0,                       /* tp_str */
//     0,                       /* tp_getattro */
//     0,                       /* tp_setattro */
//     0,                       /* tp_as_buffer */
//     Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,   /* tp_flags */
//     "Database error",        /* tp_doc */
//     (traverseproc)DatabaseErrorExt_traverse, /* tp_traverse */
//     (inquiry)DatabaseErrorExt_clear, /* tp_clear */
//     0,                       /* tp_richcompare */
//     0,                       /* tp_weaklistoffset */
//     0,                       /* tp_iter */
//     0,                       /* tp_iternext */
//     0,                       /* tp_methods */
//     0,                       /* tp_members */
//     DatabaseErrorExt_getseters, /* tp_getset */
//     0,                       /* tp_base */
//     0,                       /* tp_dict */
//     0,                       /* tp_descr_get */
//     0,                       /* tp_descr_set */
//     0,                       /* tp_dictoffset */
//     (initproc)DatabaseErrorExt_init, /* tp_init */
//     0,                       /* tp_alloc */
//     0, // DatabaseErrorExt_new,    /* tp_new */
// };
// static PyObject *PyExc_DatabaseErrorExtType = (PyObject *)&DatabaseErrorExtType;
// static PyObject* PyExc_DatabaseErrorExt = NULL;

///////////////////////////////////////////////////////////////////////////////
static
void init_common(const string& module_name)
{
    DBLB_INSTALL_DEFAULT();

    const char* rev_str = "$Revision$";
    PyObject *module;

    // Fix plugin manager ...
    CFile file(python::RetrieveModuleFileName());
    string module_dir = file.GetDir()
                        + "python_ncbi_dbapi/" NCBI_PACKAGE_VERSION;
    CDriverManager::GetInstance().AddDllSearchPath(module_dir);


    pythonpp::CModuleExt::Declare(module_name, python_ncbi_dbapi_methods);

    // Define module attributes ...
    pythonpp::CModuleExt::AddConst("apilevel", "2.0");
    pythonpp::CModuleExt::AddConst("__version__", string( rev_str + 11, strlen( rev_str + 11 ) - 2 ));
    pythonpp::CModuleExt::AddConst("threadsafety", 1);
    pythonpp::CModuleExt::AddConst("paramstyle", "named");

    module = pythonpp::CModuleExt::GetPyModule();

    ///////////////////////////////////

#if PY_VERSION_HEX >= 0x02040000
    // Initialize DateTime module ...
    PyDateTime_IMPORT;      // NCBI_FAKE_WARNING
#endif

    // Declare CBinary
    python::CBinary::Declare(module_name + ".BINARY");
    python::CBinary::GetType().SetName("BINARY");

    // Declare CNumber
    python::CNumber::Declare(module_name + ".NUMBER");
    python::CNumber::GetType().SetName("NUMBER");

    // Declare CRowID
    python::CRowID::Declare(module_name + ".ROWID");
    python::CRowID::GetType().SetName("ROWID");

    // Declare CString
    python::CStringType::Declare(module_name + ".STRING");
    python::CStringType::GetType().SetName("STRING");

    // Declare CString
    python::CDateTimeType::Declare(module_name + ".DATETIME");
    python::CDateTimeType::GetType().SetName("DATETIME");

    // Declare CConnection
    const string connection_name(module_name + ".Connection");
    python::CConnection::
        Def("__enter__",    &python::CConnection::__enter__,    "__enter__").
        Def("__exit__",     &python::CConnection::close,        "__exit__").
        Def("close",        &python::CConnection::close,        "close").
        Def("commit",       &python::CConnection::commit,       "commit").
        Def("rollback",     &python::CConnection::rollback,     "rollback").
        Def("cursor",       &python::CConnection::cursor,       "cursor").
        Def("transaction",  &python::CConnection::transaction,  "transaction");
    python::CConnection::Declare(connection_name);

    // Declare CTransaction
    const string transaction_name(module_name + ".Transaction");
    python::CTransaction::
        Def("__enter__",    &python::CTransaction::__enter__,    "__enter__").
        Def("__exit__",     &python::CTransaction::close,        "__exit__").
        Def("close",        &python::CTransaction::close,        "close").
        Def("cursor",       &python::CTransaction::cursor,       "cursor").
        Def("commit",       &python::CTransaction::commit,       "commit").
        Def("rollback",     &python::CTransaction::rollback,     "rollback");
    python::CTransaction::Declare(transaction_name);

    // Declare CCursor
    const string cursor_name(module_name + ".Cursor");
    python::CCursor::
        Def("callproc",     &python::CCursor::callproc,     "callproc").
        Def("__enter__",    &python::CCursor::__enter__,    "__enter__").
        Def("__exit__",     &python::CCursor::close,        "__exit__").
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
    python::CCursor::Declare(cursor_name);

    // Declare CCursorIter
    const string cursor_iter_name(module_name + ".__CursorIterator__");
    python::CCursorIter::Declare(cursor_iter_name);

    ///////////////////////////////////
    // Declare types ...

    // Declare BINARY
    if ( PyType_Ready(&python::CBinary::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("BINARY"), (PyObject*)&python::CBinary::GetType() ) == -1 ) {
        return;
    }

    // Declare NUMBER
    if ( PyType_Ready(&python::CNumber::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("NUMBER"), (PyObject*)&python::CNumber::GetType() ) == -1 ) {
        return;
    }

    // Declare ROWID
    if ( PyType_Ready(&python::CRowID::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("ROWID"), (PyObject*)&python::CRowID::GetType() ) == -1 ) {
        return;
    }

    // Declare STRING
    if ( PyType_Ready(&python::CStringType::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("STRING"), (PyObject*)&python::CStringType::GetType() ) == -1 ) {
        return;
    }

    // Declare DATETIME
    if ( PyType_Ready(&python::CDateTimeType::GetType()) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("DATETIME"), (PyObject*)&python::CDateTimeType::GetType() ) == -1 ) {
        return;
    }

    pythonpp::CExtType* extt = &python::CConnection::GetType();
    static char str_class[] = "__class__";
    static PyMemberDef conn_members[] = {
        {str_class, T_OBJECT_EX, 0, READONLY, NULL},
        {NULL}
    };
    extt->tp_members = conn_members;
    if ( PyType_Ready(extt) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("Connection"), (PyObject*)extt ) == -1 ) {
        return;
    }
    extt = &python::CTransaction::GetType();
    extt->tp_members = conn_members;
    if ( PyType_Ready(extt) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("Transaction"), (PyObject*)extt ) == -1 ) {
        return;
    }
    extt = &python::CCursor::GetType();
    // This list should reflect exactly attributes added in CCursor constructor
    static char str_rowcount[] = "rowcount";
    static char str_messages[] = "messages";
    static char str_description[] = "description";
    static PyMemberDef members[] = {
        {str_rowcount, T_LONG, 0, READONLY, NULL},
        {str_messages, T_OBJECT_EX, 0, READONLY, NULL},
        {str_description, T_OBJECT_EX, 0, READONLY, NULL},
        {str_class, T_OBJECT_EX, 0, READONLY, NULL},
        {NULL}
    };
    extt->tp_members = members;
    extt->tp_iter = &python::s_GetCursorIter;
    if ( PyType_Ready(extt) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("Cursor"), (PyObject*)extt ) == -1 ) {
        return;
    }
    extt = &python::CCursorIter::GetType();
    extt->tp_iter = &python::s_GetCursorIterFromIter;
    extt->tp_iternext = &python::s_CursorIterNext;
    if ( PyType_Ready(extt) == -1 ) {
        return;
    }
    if ( PyModule_AddObject(module, const_cast<char*>("__CursorIterator__"), (PyObject*)extt ) == -1 ) {
        return;
    }

    ///////////////////////////////////
    // Add exceptions ...

    /* DO NOT delete this code ...
    {
        DatabaseErrorExtType.tp_new = DatabaseErrorExt_new;
        DatabaseErrorExtType.tp_base = python::CError::GetPyException()->ob_type;

        if (PyType_Ready(&DatabaseErrorExtType) < 0) {
            Py_FatalError("exceptions bootstrapping error.");
            return;
        }

        PyObject* dict = PyModule_GetDict(module);
        if (dict == NULL) {
            Py_FatalError("exceptions bootstrapping error.");
        }

        Py_INCREF(PyExc_DatabaseErrorExtType);
        PyModule_AddObject(module, "DatabaseErrorExt", PyExc_DatabaseErrorExtType);
        if (PyDict_SetItemString(dict, "DatabaseErrorExt", PyExc_DatabaseErrorExtType)) {
            Py_FatalError("Module dictionary insertion problem.");
        }

        PyExc_DatabaseErrorExt = (PyObject*) PyErr_NewException("python_ncbi_dbapi.DatabaseErrorExt2", PyExc_DatabaseErrorExtType, NULL);
        python::CError::Check( PyExc_DatabaseErrorExt );
        if ( PyModule_AddObject( module, "DatabaseErrorExt2", PyExc_DatabaseErrorExt ) == -1 ) {
            throw pythonpp::CSystemError( "Unable to add an object to a module" );
        }
        if (PyDict_SetItemString(dict, "DatabaseErrorExt2", PyExc_DatabaseErrorExt)) {
            Py_FatalError("Module dictionary insertion problem.");
        }
    }
    */

    /* DO NOT delete this code ...

    python::CDatabaseError::Declare(
        module_name + ".DatabaseErrorExt",
        NULL,
        python::CError::GetPyException()->ob_type
        );
    */

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


#ifdef NCBI_OS_DARWIN
// force more of corelib to make it in
PYDBAPI_MODINIT_FUNC(initncbi_dbapi_darwin_kludge)
{
    CFastMutexGuard GUARD(CPluginManagerGetterImpl::GetMutex());
    CConfig config(NULL, eNoOwnership);
}
#endif

END_NCBI_SCOPE


