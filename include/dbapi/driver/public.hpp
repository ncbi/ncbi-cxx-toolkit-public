#ifndef DBAPI_DRIVER___PUBLIC__HPP
#define DBAPI_DRIVER___PUBLIC__HPP

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
 * File Description:  Data Server public interfaces
 *
 */

#include <dbapi/driver/interfaces.hpp>


/** @addtogroup DbPubInterfaces
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_DBAPIDRIVER_EXPORT CDB_Connection : public I_Connection
{
public:
    // Check out if connection is alive (this function doesn't ping the server,
    // it just checks the status of connection which was set by the last
    // i/o operation)
    virtual bool IsAlive();

    // These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    // create and return a "command" object, register it for later use with
    // this (and only this!) connection.
    // On error, an exception will be thrown (they never return NULL!).
    // It is the user's responsibility to delete the returned "command" object.

    // Language command
    virtual CDB_LangCmd*     LangCmd(const string& lang_query,
                                     unsigned int  nof_params = 0);
    // Remote procedure call
    virtual CDB_RPCCmd*      RPC(const string& rpc_name,
                                 unsigned int  nof_args);
    // "Bulk copy in" command
    virtual CDB_BCPInCmd*    BCPIn(const string& table_name,
                                   unsigned int  nof_columns);
    // Cursor
    virtual CDB_CursorCmd*   Cursor(const string& cursor_name,
                                    const string& query,
                                    unsigned int  nof_params,
                                    unsigned int  batch_size = 1);
    // "Send-data" command
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true);

    // Shortcut to send text and image to the server without using the
    // "Send-data" command (SendDataCmd)
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text& txt,
                          bool log_it = true);
    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true);

    // Reset the connection to the "ready" state (cancel all active commands)
    virtual bool Refresh();

    // Get the server name, user login name, and password
    virtual const string& ServerName() const;
    virtual const string& UserName() const;
    virtual const string& Password() const;

    // Get the bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode() const;

    // Check if this connection is a reusable one
    virtual bool IsReusable() const;

    // Find out which connection pool this connection belongs to
    virtual const string& PoolName() const;

    // Get pointer to the driver context
    virtual I_DriverContext* Context() const;

    // Put the message handler into message handler stack
    virtual void PushMsgHandler(CDB_UserHandler* h);

    // Remove the message handler (and all above it) from the stack
    virtual void PopMsgHandler(CDB_UserHandler* h);

    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);

    // Destructor
    virtual ~CDB_Connection();

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

private:
    I_Connection* m_Connect;

    // The constructor should be called by "I_DriverContext" only!
    friend class I_DriverContext;
    CDB_Connection(I_Connection* c);

    // Prohibit default- and copy- constructors, and assignment
    CDB_Connection();
    CDB_Connection& operator= (const CDB_Connection&);
    CDB_Connection(const CDB_Connection&);
};



class NCBI_DBAPIDRIVER_EXPORT CDB_Result : public I_Result
{
public:
    // Get type of the result
    virtual EDB_ResType ResultType() const;

    // Get # of items (columns) in the result
    virtual unsigned int NofItems() const;

    // Get name of a result item.
    // Return NULL if "item_num" >= NofItems().
    virtual const char* ItemName(unsigned int item_num) const;

    // Get size (in bytes) of a result item.
    // Return zero if "item_num" >= NofItems().
    virtual size_t ItemMaxSize(unsigned int item_num) const;

    // Get datatype of a result item.
    // Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const;

    // Fetch next row.
    // Return FALSE if no more rows to fetch. Throw exception on any error.
    virtual bool Fetch();

    // Return current item number we can retrieve (0,1,...)
    // Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo() const;

    // Get a result item (you can use either GetItem or ReadItem).
    // If "item_buf" is not NULL, then use "*item_buf" (its type should be
    // compatible with the type of retrieved item!) to retrieve the item to;
    // otherwise allocate new "CDB_Object".
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0);

    // Read a result item body (for text/image mostly).
    // Return number of successfully read bytes.
    // Set "*is_null" to TRUE if the item is <NULL>.
    // Throw an exception on any error.
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0);

    // Get a descriptor for text/image column (for SendData).
    // Return NULL if this result does not (or can't) have img/text descriptor.
    // NOTE: you need to call ReadItem (maybe even with buffer_size == 0)
    //       before calling this method!
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    // Skip result item
    virtual bool SkipItem();

    // Destructor
    virtual ~CDB_Result();

private:
    I_Result* m_Res;

    // The constructor should be called by "I_***Cmd" only!
    friend class CDB_BaseEnt;
    CDB_Result(I_Result* r);

    // Prohibit default- and copy- constructors, and assignment
    CDB_Result& operator= (const CDB_Result&);
    CDB_Result(const CDB_Result&);
    CDB_Result();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_LangCmd : public I_LangCmd
{
public:
    // Add more text to the language command
    virtual bool More(const string& query_text);

    // Bind cmd parameter with name "name" to the object pointed by "value"
    virtual bool BindParam(const string& name, CDB_Object* value);

    // Set cmd parameter with name "name" to the object pointed by "value"
    virtual bool SetParam(const string& name, CDB_Object* value);

    // Send command to the server
    virtual bool Send();
    virtual bool WasSent() const;

    // Cancel the command execution
    virtual bool Cancel();
    virtual bool WasCanceled() const;

    // Get result set
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;

    // Check if command has failed
    virtual bool HasFailed() const;

    // Get the number of rows affected by the command.
    // Special case:  negative on error or if there is no way that this
    //                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    // Dump the results of the command
    // If result processor is installed for this connection, then it will be
    // called for each result set
    virtual void DumpResults();

    // Destructor
    virtual ~CDB_LangCmd();

private:
    I_LangCmd* m_Cmd;

    // The constructor should be called by "I_Connection" only!
    friend class I_Connection;
    CDB_LangCmd(I_LangCmd* cmd);

    // Prohibit default- and copy- constructors, and assignment
    CDB_LangCmd& operator= (const CDB_LangCmd&);
    CDB_LangCmd(const CDB_LangCmd&);
    CDB_LangCmd();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_RPCCmd : public I_RPCCmd
{
public:
    // Binding
    virtual bool BindParam(const string& name, CDB_Object* value,
                           bool out_param = false);

    // Setting
    virtual bool SetParam(const string& name, CDB_Object* value,
                          bool out_param = false);


    // Send command to the server
    virtual bool Send();
    virtual bool WasSent() const;

    // Cancel the command execution
    virtual bool Cancel();
    virtual bool WasCanceled() const;

    // Get result set.
    // Return NULL if no more results left to read.
    // Throw exception on error or if attempted to read after NULL was returned
    virtual CDB_Result* Result();

    // Return TRUE if it makes sense (at all) to call Result()
    virtual bool HasMoreResults() const;

    // Check if command has failed
    virtual bool HasFailed() const;

    // Get the number of rows affected by the command
    // Special case:  negative on error or if there is no way that this
    //                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    // Dump the results of the command
    // If result processor is installed for this connection, then it will be
    // called for each result set
    virtual void DumpResults();

    // Set the "recompile before execute" flag for the stored proc
    virtual void SetRecompile(bool recompile = true);

    // Destructor
    virtual ~CDB_RPCCmd();
    
private:
    I_RPCCmd* m_Cmd;

    // The constructor should be called by "I_Connection" only!
    friend class I_Connection;
    CDB_RPCCmd(I_RPCCmd* rpc);

    // Prohibit default- and copy- constructors, and assignment
    CDB_RPCCmd& operator= (const CDB_RPCCmd&);
    CDB_RPCCmd(const CDB_RPCCmd&);
    CDB_RPCCmd();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_BCPInCmd : public I_BCPInCmd
{
public:
    // Binding
    virtual bool Bind(unsigned int column_num, CDB_Object* value);

    // Send row to the server
    virtual bool SendRow();

    // Complete batch -- to store all rows transferred by far in this batch
    // into the table
    virtual bool CompleteBatch();

    // Cancel the BCP command
    virtual bool Cancel();

    // Complete the BCP and store all rows transferred in last batch into
    // the table
    virtual bool CompleteBCP();

    // Destructor
    virtual ~CDB_BCPInCmd();

private:
    I_BCPInCmd* m_Cmd;

    // The constructor should be called by "I_Connection" only!
    friend class I_Connection;
    CDB_BCPInCmd(I_BCPInCmd* bcp);

    // Prohibit default- and copy- constructors, and assignment
    CDB_BCPInCmd& operator= (const CDB_BCPInCmd&);
    CDB_BCPInCmd(const CDB_BCPInCmd&);
    CDB_BCPInCmd();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_CursorCmd : public I_CursorCmd
{
public:
    // Binding
    virtual bool BindParam(const string& name, CDB_Object* value);

    // Open the cursor.
    // Return NULL if cursor resulted in no data.
    // Throw exception on error.
    virtual CDB_Result* Open();

    // Update the last fetched row.
    // NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data, 
                                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size, 
                                         bool log_it = true);

    // Delete the last fetched row.
    // NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
    virtual bool Delete(const string& table_name);

    // Get the number of fetched rows
    // Special case:  negative on error or if there is no way that this
    //                command could ever affect any rows (like PRINT).
    virtual int RowCount() const;

    // Close the cursor.
    // Return FALSE if the cursor is closed already (or not opened yet)
    virtual bool Close();

    // Destructor
    virtual ~CDB_CursorCmd();

private:
    I_CursorCmd* m_Cmd;

    // The constructor should be called by "I_Connection" only!
    friend class I_Connection;
    CDB_CursorCmd(I_CursorCmd* cur);

    // Prohibit default- and copy- constructors, and assignment
    CDB_CursorCmd& operator= (const CDB_CursorCmd&);
    CDB_CursorCmd(const CDB_CursorCmd&);
    CDB_CursorCmd();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_SendDataCmd : public I_SendDataCmd
{
public:
    // Send chunk of data to the server.
    // Return number of bytes actually transferred to server.
    virtual size_t SendChunk(const void* data, size_t size);

    // Destructor
    virtual ~CDB_SendDataCmd();

private:
    I_SendDataCmd* m_Cmd;

    // The constructor should be called by "I_Connection" only!
    friend class I_Connection;
    CDB_SendDataCmd(I_SendDataCmd* c);

    // Prohibit default- and copy- constructors, and assignment
    CDB_SendDataCmd& operator= (const CDB_SendDataCmd&);
    CDB_SendDataCmd(const CDB_CursorCmd&);
    CDB_SendDataCmd();
};



class NCBI_DBAPIDRIVER_EXPORT CDB_ITDescriptor : public I_ITDescriptor
{
public:
    CDB_ITDescriptor(const string& table_name,
                     const string& column_name, 
                     const string& search_conditions)
        : m_TableName(table_name),
          m_ColumnName(column_name), 
          m_SearchConditions(search_conditions)
    {}

    virtual int DescriptorType() const;

    const string& TableName()        const { return m_TableName;        }
    const string& ColumnName()       const { return m_ColumnName;       }
    const string& SearchConditions() const { return m_SearchConditions; }
    virtual ~CDB_ITDescriptor();

protected:
    string m_TableName;
    string m_ColumnName;
    string m_SearchConditions;
};



class NCBI_DBAPIDRIVER_EXPORT CDB_ResultProcessor : public CDB_BaseEnt
{
public:
    CDB_ResultProcessor(CDB_Connection* c);
    virtual ~CDB_ResultProcessor();

    // The default implementation just dumps all rows.
    // To get the data you will need to override this method.
    virtual void ProcessResult(CDB_Result& res);

private:
    // Prohibit default- and copy- constructors, and assignment
    CDB_ResultProcessor();
    CDB_ResultProcessor& operator= (const CDB_ResultProcessor&);
    CDB_ResultProcessor(const CDB_ResultProcessor&);

    CDB_ResultProcessor* m_Prev;
    CDB_Connection*      m_Con;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/02/23 21:36:38  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.10  2004/06/08 18:13:01  soussov
 * adds CDB_BaseEnt as a base class for CDB_ResultProcessor to allow crossreferences between processors
 *
 * Revision 1.9  2003/06/20 19:11:23  vakatov
 * CDB_ResultProcessor::
 *  - added MS-Win DLL export macro
 *  - made the destructor virtual
 *  - moved code from the header to the source file
 *  - formally reformatted the code
 *
 * Revision 1.8  2003/06/05 15:54:43  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method
 * for Connection interface, adds CDB_ResultProcessor class
 *
 * Revision 1.7  2003/04/11 17:46:09  siyan
 * Added doxygen support
 *
 * Revision 1.6  2002/12/26 19:29:12  dicuccio
 * Added Win32 export specifier for base DBAPI library
 *
 * Revision 1.5  2002/03/26 15:25:17  soussov
 * new image/text operations added
 *
 * Revision 1.4  2002/02/13 22:11:16  sapojnik
 * several methods changed from private to protected to allow derived
 * classes (needed for rdblib)
 *
 * Revision 1.3  2001/11/06 17:58:03  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/09/27 20:08:29  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.1  2001/09/21 23:39:52  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___PUBLIC__HPP */
