#ifndef DBAPI_DRIVER___INTERFACES__HPP
#define DBAPI_DRIVER___INTERFACES__HPP

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
 * Author:  Vladimir Soussov
 *
 * File Description:  Data Server interfaces
 *
 */

#include <corelib/ncbi_param.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>

#include <dbapi/driver/types.hpp>
#include <dbapi/driver/exception.hpp>

#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#else // NCBI_OS_UNIX
#  include <unistd.h>
#endif

#include <map>


/** @addtogroup DbInterfaces
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class I_BaseCmd;

class I_DriverContext;

class I_Connection;
class I_Result;
class I_LangCmd;
class I_RPCCmd;
class I_BCPInCmd;
class I_CursorCmd;
class I_SendDataCmd;

class CDB_Connection;
class CDB_Result;
class CDB_LangCmd;
class CDB_RPCCmd;
class CDB_BCPInCmd;
class CDB_CursorCmd;
class CDB_SendDataCmd;
class CDB_ResultProcessor;

class IConnValidator;

namespace impl
{
    class CResult;
    class CBaseCmd;
    class CSendDataCmd;
}


class NCBI_DBAPIDRIVER_EXPORT CDBParamVariant
{
public:
    enum ENameFormat {
        ePlainName,
        eQMarkName,    // '...WHERE name=?'
        eNumericName,  // '...WHERE name=:1'
        eNamedName,    // '...WHERE name=:name'
        eFormatName,   // ANSI C printf format codes, e.g. '...WHERE name=%s'
        eSQLServerName // '...WHERE name=@name'
    };

public:
    CDBParamVariant(int pos);
    CDBParamVariant(unsigned int pos);
    CDBParamVariant(const char* name);
    CDBParamVariant(const string& name);
    ~CDBParamVariant(void);

public:
    bool IsPositional(void) const
    {
        return m_IsPositional;
    }
    unsigned int GetPosition(void) const
    {
        return m_Pos;
    }

    
    ENameFormat GetFormat(void) const 
    {
        return m_Format;
    }
    const string& GetName(void) const
    {
        return m_Name;
    }
    string GetName(ENameFormat format) const;
    
    static string MakePlainName(const CTempString& name);

private:
    static CTempString MakeName(const CTempString& name, ENameFormat& format);

private:
    bool         m_IsPositional;
    unsigned int m_Pos;
    ENameFormat  m_Format;
    const string m_Name;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBParams

class NCBI_DBAPIDRIVER_EXPORT CDBParams
{
public:
    virtual ~CDBParams(void);

public:
    enum EDirection {eIn, eOut, eInOut};

    /// Get total number of columns in resultset.
    /// 
    /// @return 
    ///   total number of columns in resultset
    virtual unsigned int GetNum(void) const = 0;

    /// Get name of column.
    /// This method is returning const reference because meta-info MUST be
    /// cached for performance reasons.
    ///
    /// @param param
    ///   Column number or name
    virtual const string& GetName(
        const CDBParamVariant& param, 
        CDBParamVariant::ENameFormat format = 
            CDBParamVariant::eSQLServerName) const = 0;

    /// @brief 
    /// 
    /// @param param 
    ///   Column number or name
    /// 
    /// @return 
    ///   Number of a columnn, which is corresponding to a name.
    virtual unsigned int GetIndex(const CDBParamVariant& param) const = 0;
    
    /// Get maximum size in bytes for column.
    ///
    /// @param col
    ///   Column number or name
    /// 
    /// @return 
    ///   Max number of bytes needed to hold the returned data. 
    virtual size_t GetMaxSize(const CDBParamVariant& param) const = 0;

    /// Get data type for column in the resultset.
    ///
    /// @param param
    ///   Column number or name
    virtual EDB_Type GetDataType(const CDBParamVariant& param) const = 0;

    /// Get value of column.
    ///
    /// @param param
    ///   Column number or name
    virtual const CDB_Object* GetValue(const CDBParamVariant& param) const = 0;

    /// Get parameter's direction (in/out/inout).
    ///
    /// @param param
    ///   Column number or name
    virtual EDirection GetDirection(const CDBParamVariant& param) const = 0;

    /// This method stores pointer to data.
    ///
    /// @param param
    ///   Column number or name
    ///
    /// @param value
    ///   Binded object
    ///
    /// @param out_param
    ///   true if this parameter is an output parameter
    virtual CDBParams& Bind(
        const CDBParamVariant& param, 
        CDB_Object* value, 
        bool out_param = false
        );

    /// This method stores copy of data.
    ///
    /// @param param
    ///   Column number or name
    ///
    /// @param value
    ///   Binded object
    ///
    /// @param out_param
    ///   true if this parameter is an output parameter
    virtual CDBParams& Set(
        const CDBParamVariant& param, 
        CDB_Object* value, 
        bool out_param = false
        );
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBConnParams::
///

class CDBConnParams 
{
public:
    CDBConnParams(void);
    virtual ~CDBConnParams(void);

public:
    enum EServerType {
        eUnknown,          //< Server type is not known
        eMySQL,            //< MySQL server
        eSybaseOpenServer, //< Sybase Open server
        eSybaseSQLServer,  //< Sybase SQL server
        eMSSqlServer       //< Microsoft SQL server
    };

    virtual string GetDriverName(void) const = 0;
    virtual Uint4  GetProtocolVersion(void) const = 0;
    virtual EEncoding GetEncoding(void) const = 0;

    virtual string GetServerName(void) const = 0;
    virtual string GetDatabaseName(void) const = 0;
    virtual string GetUserName(void) const = 0;
    virtual string GetPassword(void) const = 0;

    virtual EServerType GetServerType(void) const = 0;
    virtual Uint4  GetHost(void) const = 0;
    virtual Uint2  GetPort(void) const = 0;

    virtual CRef<IConnValidator> GetConnValidator(void) const = 0;
    virtual const impl::CDBHandlerStack& GetOpeningMsgHandlers(void) const = 0;
    
    /// Parameters, which are not listed above explicitly, should be retrieved via
    /// SetParam() method.
    virtual string GetParam(const string& key) const = 0;

protected:
    void SetChildObj(const CDBConnParams& child_obj) const
    {
        _ASSERT(!m_ChildObj);
        m_ChildObj = &child_obj;
    }
    void ReleaseChildObj(void) const
    {
        m_ChildObj = NULL;
    }

protected:
    const CDBConnParams& GetThis(void) const
    {
        if (m_ChildObj) {
            return m_ChildObj->GetThis();
        }

        return *this;
    }

private:
    // Non-copyable.
    CDBConnParams(const CDBConnParams& other);
    CDBConnParams& operator =(const CDBConnParams& other);

private:
    mutable const CDBConnParams* m_ChildObj;

    friend class CDBConnParamsDelegate;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBConnParamsDelegate : public CDBConnParams
{
public:
    CDBConnParamsDelegate(const CDBConnParams& other);
    virtual ~CDBConnParamsDelegate(void);

public:
    virtual string GetDriverName(void) const;
    virtual Uint4  GetProtocolVersion(void) const;
    virtual EEncoding GetEncoding(void) const;

    virtual string GetServerName(void) const;
    virtual string GetDatabaseName(void) const;
    virtual string GetUserName(void) const;
    virtual string GetPassword(void) const;

    virtual EServerType GetServerType(void) const;
    virtual Uint4  GetHost(void) const;
    virtual Uint2  GetPort(void) const;

    virtual CRef<IConnValidator> GetConnValidator(void) const;
    virtual const impl::CDBHandlerStack& GetOpeningMsgHandlers(void) const;

    virtual string GetParam(const string& key) const;

private:
    // Non-copyable.
    CDBConnParamsDelegate(const CDBConnParamsDelegate& other);
    CDBConnParamsDelegate& operator =(const CDBConnParamsDelegate& other);

private:
    const CDBConnParams& m_Other;
};


/////////////////////////////////////////////////////////////////////////////
///
///  I_ITDescriptor::
///
/// Image or Text descriptor.
///

class NCBI_DBAPIDRIVER_EXPORT I_ITDescriptor
{
public:
    virtual int DescriptorType(void) const = 0;
    virtual ~I_ITDescriptor(void);
};


/////////////////////////////////////////////////////////////////////////////
///
///  EDB_ResType::
///
/// Type of result set
///

enum EDB_ResType {
    eDB_RowResult,
    eDB_ParamResult,
    eDB_ComputeResult,
    eDB_StatusResult,
    eDB_CursorResult
};


/////////////////////////////////////////////////////////////////////////////
///
///  CParamStmt::
///  Parametrized statement.

class CParamStmt
{
public:
    CParamStmt(void);
    virtual ~CParamStmt(void);

public:
    /// Get meta-information about binded parameters. 
    virtual CDBParams& GetBindParams(void) = 0;
};

/////////////////////////////////////////////////////////////////////////////
///
///  CParamRecordset::
///  Parametrized recordset.

class CParamRecordset : public CParamStmt
{
public:
    CParamRecordset(void);
    virtual ~CParamRecordset(void);

public:
    /// Get meta-information about defined parameters. 
    virtual CDBParams& GetDefineParams(void) = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  I_BaseCmd::
///
/// Abstract base class for most "command" interface classes.
///

class NCBI_DBAPIDRIVER_EXPORT I_BaseCmd : public CParamRecordset
{
public:
    I_BaseCmd(void);
    virtual ~I_BaseCmd(void);

public:
    /// Send command to the server
    virtual bool Send(void) = 0;
    /// Implementation-specific.
    /// @deprecated
    virtual bool WasSent(void) const = 0;

    /// Cancel the command execution
    virtual bool Cancel(void) = 0;
    /// Implementation-specific.
    /// @deprecated
    virtual bool WasCanceled(void) const = 0;

    /// Get result set
    virtual CDB_Result* Result(void) = 0;
    virtual bool HasMoreResults(void) const = 0;

    // Check if command has failed
    virtual bool HasFailed(void) const = 0;

    /// Get the number of rows affected by the command
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount(void) const = 0;

    /// Dump the results of the command
    /// if result processor is installed for this connection, it will be called for
    /// each result set
    virtual void DumpResults(void) = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  I_LangCmd::
///  I_RPCCmd::
///  I_BCPInCmd::
///  I_CursorCmd::
///  I_SendDataCmd::
///
/// "Command" interface classes.
///


class NCBI_DBAPIDRIVER_EXPORT I_LangCmd : public I_BaseCmd
{
public:
    I_LangCmd(void);
    virtual ~I_LangCmd(void);

protected:
    /// Add more text to the language command
    /// @deprecated
    virtual bool More(const string& query_text) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_RPCCmd : public I_BaseCmd
{
public:
    I_RPCCmd(void);
    virtual ~I_RPCCmd(void);

protected:
    /// Set the "recompile before execute" flag for the stored proc
    /// Implementation-specific.
    virtual void SetRecompile(bool recompile = true) = 0;

    /// Get a name of the procedure.
    virtual const string& GetProcName(void) const = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_BCPInCmd : public CParamStmt
{
public:
    I_BCPInCmd(void);
    virtual ~I_BCPInCmd(void);

protected:
    /// Send row to the server
    virtual bool SendRow(void) = 0;

    /// Complete batch -- to store all rows transferred by far in this batch
    /// into the table
    virtual bool CompleteBatch(void) = 0;

    /// Cancel the BCP command
    virtual bool Cancel(void) = 0;

    /// Complete the BCP and store all rows transferred in last batch into
    /// the table
    virtual bool CompleteBCP(void) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_CursorCmd : public CParamRecordset
{
public:
    I_CursorCmd(void);
    virtual ~I_CursorCmd(void);

protected:
    /// Open the cursor.
    /// Return NULL if cursor resulted in no data.
    /// Throw exception on error.
    virtual CDB_Result* Open(void) = 0;

    /// Update the last fetched row.
    /// NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query) = 0;

    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                 bool log_it = true) = 0;

    /// @brief 
    ///   Create send-data command.
    /// 
    /// @param item_num 
    ///   Column number to rewrite.
    /// @param size 
    ///   Maximal data size.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// @param discard_results
    ///   Discard all resultsets that might be returned from server
    ///   if this value is set to true.
    /// 
    /// @return 
    ///   Newly created send-data object.
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true,
                                         bool discard_results = true) = 0;
    /// Delete the last fetched row.
    /// NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
    virtual bool Delete(const string& table_name) = 0;

    /// Get the number of fetched rows
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount(void) const = 0;

    /// Close the cursor.
    /// Return FALSE if the cursor is closed already (or not opened yet)
    virtual bool Close(void) = 0;
};



class NCBI_DBAPIDRIVER_EXPORT I_SendDataCmd
{
public:
    I_SendDataCmd(void);
    virtual ~I_SendDataCmd(void);

protected:
    /// Send chunk of data to the server.
    /// Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* pChunk, size_t nofBytes) = 0;
    virtual bool Cancel(void) = 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  I_Result::
///

class NCBI_DBAPIDRIVER_EXPORT I_Result
{
public:
    I_Result(void);
    virtual ~I_Result(void);

public:
    enum EGetItem {eAppendLOB, eAssignLOB};

    /// @brief 
    ///   Get type of the result
    /// 
    /// @return 
    ///   Result type
    virtual EDB_ResType ResultType(void) const = 0;

    /// Get meta-information about rows in resultset. 
    virtual const CDBParams& GetDefineParams(void) const = 0;

    /// Get # of items (columns) in the result
    /// @brief 
    ///   Get # of items (columns) in the result.
    /// 
    /// @return 
    ///   Number of items (columns) in the result.
    virtual unsigned int NofItems(void) const = 0;

    /// @brief 
    ///   Get name of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    ///
    /// @return 
    ///    NULL if "item_num" >= NofItems(), otherwise item name.
    virtual const char* ItemName(unsigned int item_num) const = 0;

    /// @brief 
    ///   Get size (in bytes) of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    /// 
    /// @return 
    ///   Return zero if "item_num" >= NofItems().
    virtual size_t ItemMaxSize(unsigned int item_num) const = 0;

    /// @brief 
    ///   Get datatype of a result item.
    /// 
    /// @param item_num 
    ///   Number of item, starting from 0.
    /// 
    /// @return 
    ///   Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const = 0;

    /// @brief 
    ///   Fetch next row
    /// 
    /// @return
    ///   - true if a record was fetched.
    ///   - false if no more record can be fetched.
    virtual bool Fetch(void) = 0;

    /// @brief 
    ///   Return current item number we can retrieve (0,1,...)
    /// 
    /// @return 
    ///   Return current item number we can retrieve (0,1,...)
    ///   Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo(void) const = 0;

    /// @brief 
    ///   Return number of columns in the recordset.
    /// 
    /// @return 
    ///   number of columns in the recordset.
    virtual int GetColumnNum(void) const = 0;

    /// @brief 
    ///   Get a result item (you can use either GetItem or ReadItem).
    /// 
    /// @param item_buf 
    ///   If "item_buf" is not NULL, then use "*item_buf" (its type should be
    ///   compatible with the type of retrieved item!) to retrieve the item to;
    ///   otherwise allocate new "CDB_Object".
    ///   In case of "CDB_Image" and "CDB_Text" data types value will be *appended*
    ///   to the "item_buf" by default (policy == eAppendLOB).
    /// 
    /// @param policy
    ///   Data retrieval policy. If policy == eAppendLOB and "item_buf" is an
    ///   object of CDB_Image or CDB_Text type, then data will be *appended* to
    ///   the end of previously assigned data. If policy == eAssignLOB and "item_buf" is an
    ///   object of CDB_Image or CDB_Text type, then new value will be *assigned*
    ///   to the "item_buf" object.
    /// 
    /// @return 
    ///   a result item
    ///
    /// @sa
    ///   ReadItem, SkipItem
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0, EGetItem policy = eAppendLOB) = 0;

    /// @brief 
    ///   Read a result item body (for text/image mostly).
    ///   Throw an exception on any error.
    /// 
    /// @param buffer 
    ///   Buffer to fill with data.
    /// @param buffer_size 
    ///   Buffere size.
    /// @param is_null 
    ///   Set "*is_null" to TRUE if the item is <NULL>.
    /// 
    /// @return 
    ///   number of successfully read bytes.
    ///
    /// @sa
    ///   GetItem, SkipItem
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0) = 0;

    /// @brief 
    ///   Get a descriptor for text/image column (for SendData).
    /// 
    /// @return 
    ///   Return NULL if this result doesn't (or can't) have img/text descriptor.
    ///
    /// @note
    ///   You need to call ReadItem (maybe even with buffer_size == 0)
    ///   before calling this method!
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void) = 0;

    /// @brief 
    /// Skip result item
    /// 
    /// @return 
    ///   TRUE on success.
    /// 
    /// @sa
    ///   GetItem, ReadItem
    virtual bool SkipItem(void) = 0;
};


/////////////////////////////////////////////////////////////////////////////
///
///  I_DriverContext::
///

NCBI_PARAM_DECL_EXPORT(NCBI_DBAPIDRIVER_EXPORT,
                       bool, dbapi, conn_use_encrypt_data);
typedef NCBI_PARAM_TYPE(dbapi, conn_use_encrypt_data) TDbapi_ConnUseEncryptData;


class CDBConnParams;

class NCBI_DBAPIDRIVER_EXPORT I_DriverContext
{
protected:
    I_DriverContext(void);

public:
    virtual ~I_DriverContext(void);


    /// Connection mode
    enum EConnectionMode {
        fBcpIn             = 0x1,  //< Enable BCP
        fPasswordEncrypted = 0x2,  //< Encript password
        fDoNotConnect      = 0x4   //< Use just connections from NotInUse pool
        // all driver-specific mode flags > 0x100
    };

    typedef int TConnectionMode;   //< holds a binary OR of "EConnectionMode"

    
    /// Set login timeout.
    ///
    /// @param nof_secs
    ///   Timeout in seconds. If "nof_secs" is zero or is "too big" 
    ///    (depends on the underlying DB API), then set the timeout to infinite.
    /// @return
    ///   FALSE on error.
    ///
    /// @sa
    ///   GetLoginTimeout()
    virtual bool SetLoginTimeout(unsigned int nof_secs = 0) = 0;

    /// Set connection timeouts.
    ///
    /// @param nof_secs
    ///   Timeout in seconds. If "nof_secs" is zero or is "too big" 
    ///   (depends on the underlying DB API), then set the timeout to infinite.
    /// @return
    ///   FALSE on error.
    ///
    /// @sa
    ///   GetTimeout()
    virtual bool SetTimeout(unsigned int nof_secs = 0) = 0;

    /// Get login timeout
    ///
    /// @return
    ///   Login timeout.
    /// @sa
    ///   SetLoginTimeout()
    virtual unsigned int GetLoginTimeout(void) const = 0;

    /// Get connection timeout
    ///
    /// @return
    ///   Connection timeout.
    ///
    /// @sa
    ///   SetTimeout()
    virtual unsigned int GetTimeout(void) const = 0;

    /// Set maximal size for Text and Image objects. 
    ///
    /// @param nof_bytes
    ///   Maximal size for Text and Image objects. Text and Image objects 
    ///   exceeding this size will be truncated.
    /// @return 
    ///   FALSE on error (e.g. if "nof_bytes" is too big).
    virtual bool SetMaxTextImageSize(size_t nof_bytes) = 0;


    /// @brief 
    ///   Create new connection to specified server (or service) within this context.
    /// 
    /// @param srv_name
    ///   Server/Service name. This parameter can be a SERVER name (host name,
    ///   DNS alias name, or one of the names found in interfaces file) or an
    ///   IP address. This parameter can also be a SERVICE name, which is
    ///   defined by Load Balancer framework. In order to enable using of LB
    ///   service names you have to call DBLB_INSTALL_DEFAULT() macro before
    ///   any other DBAPI method.
    /// @param user_name
    ///   User name.
    /// @param passwd 
    ///   User password.
    /// @param mode 
    ///   Connection mode.
    /// @param reusable 
    ///   If set to TRUE, then return connection into a connection pool on deletion.
    /// @param pool_name 
    ///   Name of a pool to which this connection is going to belong.
    /// 
    /// @return 
    ///   Connection object on success, NULL on error.
    ///
    /// NOTE:
    ///
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    /// 
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    ///
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    ///
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    /// 
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    ///
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    CDB_Connection* Connect(
            const string&   srv_name,
            const string&   user_name,
            const string&   passwd,
            TConnectionMode mode,
            bool            reusable  = false,
            const string&   pool_name = kEmptyStr);
    
    /// @brief 
    ///   Create new connection to specified server (within this context).
    /// 
    /// @param srv_name 
    ///   Server/Service name. This parameter can be a SERVER name (host name,
    ///   DNS alias name, or one of the names found in interfaces file) or an
    ///   IP address. This parameter can also be a SERVICE name, which is
    ///   defined by Load Balancer framework. In order to enable using of LB
    ///   service names you have to call DBLB_INSTALL_DEFAULT() macro before
    ///   any other DBAPI method.
    /// @param user_name 
    ///   User name.
    /// @param passwd
    ///   User password.
    /// @param validator
    ///   Connection validation object.
    /// @param mode 
    ///   Connection mode.
    /// @param reusable 
    ///   If set to true put connection into a connection pool on deletion.
    /// @param pool_name 
    ///   Name of a pool to which this connection is going to belong.
    /// 
    /// @return 
    ///   Connection object on success, NULL on error.
    ///
    /// NOTE:
    /// 
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    ///
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    /// 
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    ///
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    ///
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    ///
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    CDB_Connection* ConnectValidated(
            const string&   srv_name,
            const string&   user_name,
            const string&   passwd,
            IConnValidator& validator,
            TConnectionMode mode      = 0,
            bool            reusable  = false,
            const string&   pool_name = kEmptyStr);

    /// @brief 
    ///   Create connection object using Load Balancer / connection factory.
    /// 
    /// @param params 
    ///   Connection parameters.
    /// 
    /// @return 
    ///   Connection object.
    virtual CDB_Connection* MakeConnection(const CDBConnParams& params) = 0;

    /// @brief 
    ///   Return number of currently open connections in this context.
    /// 
    /// @param srv_name 
    ///   Server/Service name. If not empty, then return # of connection 
    ///   open to that server.
    /// @param pool_name 
    ///   Name of connection pool.
    /// 
    /// @return 
    ///   Return number of currently open connections in this context.
    ///   If "srv_name" is not NULL, then return # of conn. open to that server.
    virtual unsigned int NofConnections(const string& srv_name  = "",
                                        const string& pool_name = "")
        const = 0;
    virtual unsigned int NofConnections(const TSvrRef& svr_ref,
                                        const string& pool_name = "") const = 0;

    /// @brief 
    ///   Add message handler "h" to process 'context-wide' (not bound
    ///   to any particular connection) error messages.
    /// 
    /// @param h 
    ///   Error message handler.
    /// @param ownership 
    ///   If set to eNoOwnership, it is user's responsibility to unregister
    ///   and delete the error message handler.
    ///   If set to eTakeOwnership, then DBAPI will take ownership of the
    ///   error message handler and delete it itself.
    ///
    /// @sa 
    ///   PopCntxMsgHandler()
    virtual void PushCntxMsgHandler(CDB_UserHandler* h,
                            EOwnership ownership = eNoOwnership) = 0;

    /// @brief 
    ///   Remove message handler "h" and all handlers above it in the stack.
    /// 
    /// @param h 
    ///   Error message handler to be removed.
    ///
    /// @sa 
    ///   PushCntxMsgHandler()
    virtual void PopCntxMsgHandler(CDB_UserHandler* h) = 0;

    /// @brief 
    ///   Add `per-connection' err.message handler "h" to the stack of default
    ///   handlers which are inherited by all newly created connections.
    /// 
    /// @param h 
    ///   Error message handler.
    /// @param ownership 
    ///   If set to eNoOwnership, it is user's responsibility to unregister
    ///   and delete the error message handler.
    ///   If set to eTakeOwnership, then DBAPI will take ownership of the
    ///   error message handler and delete it itself.
    ///
    /// @sa
    ///   PopDefConnMsgHandler()
    virtual void PushDefConnMsgHandler(CDB_UserHandler* h,
                               EOwnership ownership = eNoOwnership) = 0;

    /// @brief 
    ///   Remove `per-connection' mess. handler "h" and all above it in the stack.
    /// 
    /// @param h 
    ///   Error message handler.
    ///
    /// @sa
    ///   PushDefConnMsgHandler()
    virtual void PopDefConnMsgHandler(CDB_UserHandler* h) = 0;

    /// Report if the driver supports this functionality
    enum ECapability {
        eBcp,                 //< Is able to run BCP operations.
        eReturnITDescriptors, //< Is able to return ITDescriptor.
        eReturnComputeResults //< Is able to return compute results.
    };

    /// @brief
    ///   Check if a driver is acle to provide necessary functionality.
    /// 
    /// @param cpb 
    ///   Functionality to query about.
    /// 
    /// @return 
    ///   - true if functionality is present
    ///   - false if no such functionality.
    virtual bool IsAbleTo(ECapability cpb) const = 0;

    /// @brief 
    ///   Close reusable deleted connections for specified server and/or pool.
    /// 
    /// @param srv_name 
    ///   Server/Service name.
    /// @param pool_name 
    ///   Name of connection pool.
    virtual void CloseUnusedConnections(const string& srv_name  = kEmptyStr,
                                const string& pool_name = kEmptyStr) = 0;

    /// @brief
    ///   Set application name.
    /// 
    /// @param app_name 
    ///   defines the application name that a connection will use when
    ///   connecting to a server.
    ///
    /// @sa
    ///   GetApplicationName()
    virtual void SetApplicationName(const string& app_name) = 0;
    
    /// @brief 
    ///   Return application name.
    /// 
    /// @return
    ///   Application name.
    ///
    /// @sa
    ///   SetApplicationName()
    virtual string GetApplicationName(void) const = 0;

    /// @brief 
    ///   Set host name.
    /// 
    /// @param host_name
    ///   Host name
    ///
    /// @sa
    ///   GetHostName()
    virtual void SetHostName(const string& host_name) = 0;
    
    /// @brief 
    ///  Get host name.
    /// 
    /// @return 
    ///  Host name.
    ///
    /// @sa
    ///   SetHostName()
    virtual string GetHostName(void) const = 0;

    virtual string GetDriverName(void) const;

protected:
    /// @brief 
    ///   Create connection object WITHOUT using of Load Balancer / connection factory.
    /// 
    /// @param params 
    ///   Connection parameters.
    /// 
    /// @return 
    ///   Connection object.
    virtual CDB_Connection* MakePooledConnection(const CDBConnParams& params) = 0;

private:
    friend class IDBConnectionFactory;
};



class I_ConnectionExtra;

/////////////////////////////////////////////////////////////////////////////
///
///  I_Connection::
///

class NCBI_DBAPIDRIVER_EXPORT I_Connection
{
public:
    I_Connection(void);
    virtual ~I_Connection(void);

protected:
    /// @brief 
    /// Check out if connection is alive. 
    ///
    /// This function doesn't ping the server,
    /// it just checks the status of connection which was set by the last
    /// i/o operation.
    /// 
    /// @return 
    ///   - true if connection is alive
    ///   - false in other case.
    virtual bool IsAlive(void) = 0;

    /// These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    /// create and return a "command" object, register it for later use with
    /// this (and only this!) connection.
    /// On error, an exception will be thrown (they never return NULL!).
    /// It is the user's responsibility to delete the returned "command" object.

    /// Language command
    virtual CDB_LangCmd* LangCmd(const string& lang_query) = 0;
    /// Remote procedure call
    virtual CDB_RPCCmd* RPC(const string& rpc_name) = 0;
    /// "Bulk copy in" command
    virtual CDB_BCPInCmd* BCPIn(const string& table_name) = 0;
    /// Cursor
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int  batch_size) = 0;
    CDB_CursorCmd* Cursor(const string& cursor_name,
                          const string& query)
    {
        return Cursor(cursor_name, query, 1);
    }
    /// @brief 
    ///   Create send-data command.
    /// 
    /// @param desc 
    ///   Lob descriptor.
    /// @param data_size 
    ///   Maximal data size.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// @param discard_results
    ///   Discard all resultsets that might be returned from server
    ///   if this value is set to true.
    /// 
    /// @return 
    ///   Newly created send-data object.
    ///
    /// @sa SendData
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true,
                                         bool            discard_results = true) = 0;

    /// @brief 
    ///   Shortcut to send text and image to the server without using the
    ///   "Send-data" command (SendDataCmd)
    /// 
    /// @param desc 
    ///   Lob descriptor.
    /// @param lob 
    ///   Text or Image object.
    /// @param log_it 
    ///   Log LOB operation if this value is set to true.
    /// 
    /// @return 
    ///   - true on success.
    ///
    /// @sa
    ///   SendDataCmd
    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true) = 0;

    /// @brief 
    /// Reset the connection to the "ready" state (cancel all active commands)
    /// 
    /// @return 
    ///   - true on success.
    virtual bool Refresh(void) = 0;

    /// @brief 
    ///   Get the server name.
    /// 
    /// @return
    ///   Server/Service name.
    virtual const string& ServerName(void) const = 0;
    
    /// @brief 
    ///   Get the user user.
    /// 
    /// @return
    ///   User name.
    virtual const string& UserName(void) const = 0;

    /// @brief 
    ///   Get the  password.
    /// 
    /// @return 
    ///   Password value.
    virtual const string& Password(void) const = 0;

    /// @brief 
    ///   Get the database name.
    /// 
    /// @return 
    ///   Password value.
    virtual const string& DatabaseName(void) const = 0;

    /// @brief 
    /// Get the bitmask for the connection mode (BCP, secure login, ...)
    /// 
    /// @return 
    ///  bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const = 0;

    /// @brief 
    ///   Check if this connection is a reusable one
    /// 
    /// @return 
    ///   - true if this connection is a reusable one.
    virtual bool IsReusable(void) const = 0;

    /// @brief 
    ///   Find out which connection pool this connection belongs to
    /// 
    /// @return 
    ///   connection pool
    virtual const string& PoolName(void) const = 0;

    /// @brief 
    ///   Get pointer to the driver context
    /// 
    /// @return 
    ///   pointer to the driver context
    virtual I_DriverContext* Context(void) const = 0;

    /// @brief 
    ///   Put the message handler into message handler stack
    /// 
    /// @param h 
    ///   Error message handler.
    /// @param ownership 
    ///   If set to eNoOwnership, it is user's responsibility to unregister
    ///   and delete the error message handler.
    ///   If set to eTakeOwnership, then DBAPI will take ownership of the
    ///   error message handler and delete it itself.
    ///
    /// @sa
    ///   PopMsgHandler
    virtual void PushMsgHandler(CDB_UserHandler* h,
                                EOwnership ownership = eNoOwnership) = 0;

    /// @brief 
    ///   Remove the message handler (and all above it) from the stack
    /// 
    /// @param h 
    ///   Error message handler.
    /// @sa
    ///   PushMsgHandler
    virtual void PopMsgHandler(CDB_UserHandler* h) = 0;

    /// @brief 
    ///   Set new result-processor.
    /// 
    /// @param rp 
    ///   New result-processor.
    /// 
    /// @return 
    ///   Old result-processor
    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp) = 0;

    /// Abort the connection
    /// 
    /// @return 
    ///  TRUE - if succeeded, FALSE if not
    ///
    /// @note
    ///   Attention: it is not recommended to use this method unless you 
    ///   absolutely have to.  The expected implementation is - close 
    ///   underlying file descriptor[s] without destroing any objects 
    ///   associated with a connection.
    ///
    /// @sa
    ///   Close
    virtual bool Abort(void) = 0;

    ///  Close an open connection.
    ///  This method will return connection (if it is created as reusable) to 
    ///  its connection pool
    ///   
    /// @return 
    ///  TRUE - if succeeded, FALSE if not
    ///
    /// @sa
    ///   Abort, I_DriverContext::Connect
    virtual bool Close(void) = 0;

    /// @brief 
    ///   Set connection timeout.    
    /// 
    /// @param nof_secs 
    ///   Number of seconds.  If "nof_secs" is zero or is "too big" 
    ///   (depends on the underlying DB API), then set the timeout to infinite.
    virtual void SetTimeout(size_t nof_secs) = 0;

    /// Get interface for extra features that could be implemented in the driver.
    virtual I_ConnectionExtra& GetExtraFeatures(void) = 0;

public:
    // Deprecated legacy methods.

    /// @deprecated 
    CDB_LangCmd* LangCmd(const string& lang_query, unsigned int /*unused*/)
    {
        return LangCmd(lang_query);
    }
    /// @deprecated 
    CDB_RPCCmd* RPC(const string& rpc_name, unsigned int /*unused*/)
    {
        return RPC(rpc_name);
    }
    /// @deprecated 
    CDB_BCPInCmd* BCPIn(const string& table_name, unsigned int /*unused*/)
    {
        return BCPIn(table_name);
    }
    /// @deprecated 
    CDB_CursorCmd* Cursor(const string& cursor_name,
                          const string& query,
                          unsigned int /*unused*/,
                          unsigned int  batch_size)
    {
        return Cursor(cursor_name, query, batch_size);
    }

};


class NCBI_DBAPIDRIVER_EXPORT I_ConnectionExtra
{
public:
#ifdef NCBI_OS_MSWIN
    typedef SOCKET  TSockHandle;
#else
    typedef int     TSockHandle;
#endif

    /// Get OS handle of the socket represented by the connection
    virtual TSockHandle GetLowLevelHandle(void) const = 0;

    virtual ~I_ConnectionExtra(void);
};


END_NCBI_SCOPE



/* @} */


#endif  /* DBAPI_DRIVER___INTERFACES__HPP */
