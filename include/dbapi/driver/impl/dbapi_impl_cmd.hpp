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

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(impl)

class NCBI_DBAPIDRIVER_EXPORT CCommand
{
public:
    virtual ~CCommand(void)
    {
    }

    void Release(void)
    {
        // Temporarily solution ...
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
public:
    CBaseCmd(void);
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
    virtual void DumpResults(void)= 0;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CLangCmd::
///  CRPCCmd::
///  CBCPInCmd::
///  CCursorCmd::
///  CSendDataCmd::
///
/// "Command" interface classes.
///


class NCBI_DBAPIDRIVER_EXPORT CLangCmd : public CBaseCmd
{
    friend class CDB_LangCmd; // Because of AttachTo

public:
    CLangCmd(void);
    virtual ~CLangCmd(void);

protected:
    /// Add more text to the language command
    virtual bool More(const string& query_text) = 0;

    /// Bind cmd parameter with name "name" to the object pointed by "value"
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

    /// Set cmd parameter with name "name" to the object pointed by "value"
    virtual bool SetParam(const string& name, CDB_Object* param_ptr) = 0;

    void DetachInterface(void);

private:
    void AttachTo(CDB_LangCmd* interface);

    CInterfaceHook<CDB_LangCmd> m_Interface;
};



class NCBI_DBAPIDRIVER_EXPORT CRPCCmd : public CBaseCmd
{
    friend class CDB_RPCCmd; // Because of AttachTo

public:
    CRPCCmd(void);
    virtual ~CRPCCmd(void);

protected:
    /// Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr,
                           bool out_param = false) = 0;

    /// Setting
    virtual bool SetParam(const string& name, CDB_Object* param_ptr,
                          bool out_param = false) = 0;

    /// Set the "recompile before execute" flag for the stored proc
    virtual void SetRecompile(bool recompile = true) = 0;

    void DetachInterface(void);

private:
    void AttachTo(CDB_RPCCmd* interface);

    CInterfaceHook<CDB_RPCCmd> m_Interface;
};



class NCBI_DBAPIDRIVER_EXPORT CBCPInCmd : public CCommand
{
    friend class CDB_BCPInCmd; // Because of AttachTo

public:
    CBCPInCmd(void);
    virtual ~CBCPInCmd(void);

protected:
    /// Binding
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr) = 0;

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

    void DetachInterface(void);

private:
    void AttachTo(CDB_BCPInCmd* interface);

    CInterfaceHook<CDB_BCPInCmd> m_Interface;
};



class NCBI_DBAPIDRIVER_EXPORT CCursorCmd : public CCommand
{
    friend class CDB_CursorCmd; // Because of AttachTo

public:
    CCursorCmd(void);
    virtual ~CCursorCmd(void);

protected:
    /// Binding
    virtual bool BindParam(const string& name, CDB_Object* param_ptr) = 0;

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

private:
    void AttachTo(CDB_CursorCmd* interface);

    CInterfaceHook<CDB_CursorCmd> m_Interface;
};



class NCBI_DBAPIDRIVER_EXPORT CSendDataCmd : public CCommand
{
    friend class CDB_SendDataCmd; // Because of AttachTo

public:
    CSendDataCmd(void);
    virtual ~CSendDataCmd(void);

protected:
    /// Send chunk of data to the server.
    /// Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* pChunk, size_t nofBytes) = 0;
    virtual bool Cancel(void) = 0;

    void DetachInterface(void);

private:
    void AttachTo(CDB_SendDataCmd* interface);

    CInterfaceHook<CDB_SendDataCmd> m_Interface;
};


END_SCOPE(impl)

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
