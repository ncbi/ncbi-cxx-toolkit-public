#ifndef DBAPI_DRIVER_IMPL___DBAPI_IMPL_CMD__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_IMPL_CMD__HPP

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
 * File Description:
 *
 */


#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/util/parameters.hpp>

BEGIN_NCBI_SCOPE

class CDB_LangCmd;
class CDB_RPCCmd;
class CDB_BCPInCmd;
class CDB_CursorCmd;
class CDB_SendDataCmd;

BEGIN_SCOPE(impl)

class NCBI_DBAPIDRIVER_EXPORT CCommand
{
public:
    virtual ~CCommand(void)
    {
    }

    void Release(void)
    {
        // Temporary solution ...
        delete this;
    }

    static CDB_Result* Create_Result(CResult& result);
};

/////////////////////////////////////////////////////////////////////////////
///
///  CBaseCmd::
///
/// Abstract base class for most "command" interface classes.
///

class NCBI_DBAPIDRIVER_EXPORT CBaseCmd : public CCommand
{
    friend class ncbi::CDB_LangCmd; // Because of AttachTo
    friend class ncbi::CDB_RPCCmd;  // Because of AttachTo
    friend class ncbi::CDB_BCPInCmd; // Because of AttachTo

public:
    CBaseCmd(const string& query, unsigned int nof_params);
    // Destructor
    virtual ~CBaseCmd(void);

public:
    /// Send command to the server
    virtual bool Send(void) = 0;
    virtual bool WasSent(void) const = 0;

    /// Cancel the command execution
    virtual bool Cancel(void) = 0;
    virtual bool WasCanceled(void) const = 0;

    /// Get result set
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;

    // Check if command has failed
    virtual bool HasFailed(void) const = 0;

    /// Get the number of rows affected by the command
    /// Special case:  negative on error or if there is no way that this
    ///                command could ever affect any rows (like PRINT).
    virtual int RowCount(void) const = 0;

    /// Dump the results of the command
    /// if result processor is installed for this connection, it will be called for
    /// each result set
    virtual void DumpResults(void);

    /// Binding
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool BindParam(const string& name, CDB_Object* param_ptr,
                           bool out_param = false);

    /// Setting
    virtual bool SetParam(const string& name, CDB_Object* param_ptr,
                          bool out_param = false);

    /// Add more text to the language command
    virtual bool More(const string& query_text);


    /// Complete batch -- to store all rows transferred by far in this batch
    /// into the table
    virtual bool CommitBCPTrans(void);

    /// Complete the BCP and store all rows transferred in last batch into
    /// the table
    virtual bool EndBCP(void);

    const string& GetQuery(void) const
    {
        return m_Query;
    }

    const CDB_Params& GetParams(void) const
    {
        return m_Params;
    }
    CDB_Params& GetParams(void)
    {
        return m_Params;
    }

protected:
    void DetachInterface(void);

    /// Set the "recompile before execute" flag for the stored proc
    virtual void SetRecompile(bool recompile = true);
    bool NeedToRecompile(void) const
    {
        return m_Recompile;
    }

private:
    void AttachTo(CDB_LangCmd* interface);
    void AttachTo(CDB_RPCCmd* interface);
    void AttachTo(CDB_BCPInCmd* interface);

    CInterfaceHook<CDB_LangCmd>  m_InterfaceLang;
    CInterfaceHook<CDB_RPCCmd>   m_InterfaceRPC;
    CInterfaceHook<CDB_BCPInCmd> m_InterfaceBCPIn;

    string                       m_Query;
    CDB_Params                   m_Params;
    bool                         m_Recompile; // Temporary. Should be deleted.
};



/////////////////////////////////////////////////////////////////////////////
///
///  CCursorCmd::
///  CSendDataCmd::
///
/// "Command" interface classes.
///


class NCBI_DBAPIDRIVER_EXPORT CCursorCmd : public CCommand
{
    friend class ncbi::CDB_CursorCmd; // Because of AttachTo

public:
    CCursorCmd(const string& cursor_name,
               const string& query,
               unsigned int nof_params);
    virtual ~CCursorCmd(void);

protected:
    /// Binding
    virtual bool BindParam(const string& name,
                           CDB_Object* param_ptr,
                           bool out_param = false);

    /// Open the cursor.
    /// Return NULL if cursor resulted in no data.
    /// Throw exception on error.
    virtual CDB_Result* Open(void) = 0;

    /// Update the last fetched row.
    /// NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query) = 0;

    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                 bool log_it = true) = 0;

    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                                         bool log_it = true) = 0;
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

    void DetachInterface(void);

    const string& GetQuery(void) const
    {
        return m_Query;
    }
    string& GetQuery(void)
    {
        return m_Query;
    }

    const CDB_Params& GetParams(void) const
    {
        return m_Params;
    }

protected:
    bool        m_IsOpen;
    bool        m_IsDeclared;
    // bool m_HasFailed;
    string      m_Name;
    string      m_Query;

private:
    void AttachTo(CDB_CursorCmd* interface);

    CInterfaceHook<CDB_CursorCmd> m_Interface;

    CDB_Params  m_Params;
};



class NCBI_DBAPIDRIVER_EXPORT CSendDataCmd : public CCommand
{
    friend class ncbi::CDB_SendDataCmd; // Because of AttachTo

public:
    CSendDataCmd(size_t nof_bytes);
    virtual ~CSendDataCmd(void);

protected:
    /// Send chunk of data to the server.
    /// Return number of bytes actually transferred to server.
    virtual size_t  SendChunk(const void* pChunk, size_t nofBytes) = 0;
    virtual bool Cancel(void) = 0;

    void            DetachInterface(void);

protected:
    size_t  m_Bytes2go;

private:
    void AttachTo(CDB_SendDataCmd* interface);

    CInterfaceHook<CDB_SendDataCmd> m_Interface;
};


END_SCOPE(impl)

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2006/11/20 17:37:12  ssikorsk
 * Added GetQuery() and GetParams() to CBaseCmd;
 * Added GetQuery() to CCursorCmd;
 *
 * Revision 1.6  2006/07/19 14:09:55  ssikorsk
 * Refactoring of CursorCmd.
 *
 * Revision 1.5  2006/07/18 15:46:00  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.4  2006/07/12 19:15:17  ucko
 * Disambiguate friend declarations, and add corresponding top-level
 * predeclarations, for the sake of GCC 4.x.
 *
 * Revision 1.3  2006/07/12 18:55:28  ssikorsk
 * Moved implementations of DetachInterface and AttachTo into cpp for MIPS sake.
 *
 * Revision 1.2  2006/07/12 17:09:53  ssikorsk
 * Added NCBI_DBAPIDRIVER_EXPORT to CCommand.
 *
 * Revision 1.1  2006/07/12 16:28:48  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * ===========================================================================
 */


#endif  /* DBAPI_DRIVER_IMPL___DBAPI_IMPL_CMD__HPP */
