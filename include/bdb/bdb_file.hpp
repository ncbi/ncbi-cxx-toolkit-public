#ifndef BDB_FILE_HPP__
#define BDB_FILE_HPP__

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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Berkeley DB File classes.
 *
 */

/// @file bdb_file.hpp
/// BDB File management.

#include <bdb/bdb_types.hpp>
#include <stdio.h>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Files
 *
 * @{
 */


/// BDB Return codes
///


enum EBDB_ErrCode {
    eBDB_Ok,
    eBDB_NotFound,
    eBDB_KeyDup
};


class CBDB_Env;
class CBDB_Transaction;


/// Raw file class wraps up basic Berkeley DB operations. 
///

class NCBI_BDB_EXPORT CBDB_RawFile
{
public:
    static const char kDefaultDatabase[];  // = "_table"

    enum EOpenMode {
        eReadWrite,
        eReadOnly,
        eCreate,         //!< implies 'eReadWrite' too
        eReadWriteCreate //!< read-write, create if it doesn't exist
    };

    enum EReallocMode {
        eReallocAllowed,
        eReallocForbidden
    };

    enum EDuplicateKeys {
        eDuplicatesDisable,
        eDuplicatesEnable
    };

    enum EIgnoreError {
        eIgnoreError,
        eThrowOnError
    };

public:
    CBDB_RawFile(EDuplicateKeys dup_keys=eDuplicatesDisable);
    virtual ~CBDB_RawFile();

    /// Associate file with environment. Should be called before 
    /// file opening.
    void SetEnv(CBDB_Env& env);

    /// Get pointer on file environment
    /// Return NULL if no environment has been set
    CBDB_Env* GetEnv() { return m_Env; }

    /// Open file with specified access mode
    void Open(const char* filename, EOpenMode open_mode);
    /// Open file with specified filename and database name.
    /// (Berkeley DB supports having several database tables in one file.) 
    void Open(const char* filename, const char* database,
              EOpenMode open_mode);
    /// Attach class to external BerkeleyDB file instance.
    /// Note: Should be already open.
    void Attach(CBDB_RawFile& bdb_file);
    /// Close file
    void Close();
    /// Reopen database file. (Should be already open).
    void Reopen(EOpenMode open_mode);

    /// Remove the database specified by the filename and database arguments
    void Remove(const char* filename, const char* database = 0);
    /// Empty the database. Return number of records removed.
    unsigned int Truncate();

    // Set Berkeley DB page size value. By default OS default is used.
    void SetPageSize(unsigned int page_size);

    /// Set Berkeley DB memory cache size for the file (default is 256K).
    void SetCacheSize(unsigned int cache_size);

    const string& FileName() const;
    const string& Database() const;

    /// Set comparison function. Default implementation installs bdb_types based
    /// function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*) = 0;

    /// Return TRUE if the file is open
    bool IsOpen() const;

    // Return TRUE if the file is attached to some other BDB file
    bool IsAttached() const;

    /// Return TRUE if the if the underlying database files were created 
    /// on an architecture of the different byte order 
    bool IsByteSwapped() const { return m_ByteSwapped; }

    /// Return TRUE if file can contain duplicate keys
    bool DuplicatesAllowed() const { return m_DuplicateKeys == eDuplicatesEnable; }

    /// Return the key duplicate mode value
    EDuplicateKeys GetDupKeysMode() const { return m_DuplicateKeys; }

    /// Return file name
    const string& GetFileName() const { return m_FileName; }

    /// Return the file open mode
    EOpenMode GetOpenMode() const { return m_OpenMode; }

    /// Flush any cached information to disk
    void Sync();

    /// Compute database statistic, return number of records.
    /// (Can be time consuming)
    unsigned CountRecs();
    
    /// Set current transaction
    void SetTransaction(CBDB_Transaction* trans);

    /// Get current transaction
    CBDB_Transaction* GetTransaction() { return m_Trans; }

private:
    CBDB_RawFile(const CBDB_RawFile&);
    CBDB_RawFile& operator= (const CBDB_RawFile&);

protected:
    void x_Open(const char* filename, const char* database,
                EOpenMode open_mode);
    void x_Create(const char* filename, const char* database);

    void x_Close(EIgnoreError close_mode);

    // Create m_DB member, set page, cache parameters
    void x_CreateDB();

    void x_RemoveTransaction(CBDB_Transaction* trans);
    
    /// Get transaction handler.
    ///
    /// Function returns NULL if no transaction has been set.
    ///
    /// @sa SetTransaction
    DB_TXN* GetTxn();

    /// Create DB cursor
    DBC* CreateCursor(CBDB_Transaction* trans = 0, 
                      unsigned int      flags = 0) const;

	/// Set byte order swapping. Can be overloaded on derived classes
	/// @note When overloading DO call parent::x_SetByteSwapped
	virtual void x_SetByteSwapped(bool bswp);

protected:
    DB*               m_DB;
    DBT*              m_DBT_Key;
    DBT*              m_DBT_Data;
    CBDB_Env*         m_Env;
    CBDB_Transaction* m_Trans;
    int               m_TransAssociation;

private:
    bool             m_DB_Attached;  //!< TRUE if m_DB doesn't belong here
    bool             m_ByteSwapped;  //!< TRUE if file created on a diff.arch.
    string           m_FileName;     //!< filename
    string           m_Database;     //!< db name in file (optional)
    unsigned         m_PageSize;
    unsigned         m_CacheSize;
    EDuplicateKeys   m_DuplicateKeys;
    EOpenMode        m_OpenMode;

    static const int kOpenFileMask;
    
    friend class CBDB_Transaction;
    friend class CBDB_FileCursor;
};




/// Berkeley DB file class. 
/// Implements primary key and fields functionality.
///

class NCBI_BDB_EXPORT CBDB_File : public CBDB_RawFile
{
public:
    CBDB_File(EDuplicateKeys dup_keys = eDuplicatesDisable);

    /// Open file with specified access mode
    void Open(const char* filename, EOpenMode open_mode);

    /// Open file with specified filename and database name.
    /// (Berkeley DB supports having several database tables in one file.) 
    void Open(const char* filename, const char* database,
              EOpenMode open_mode);
    /// Reopen the db file
    void Reopen(EOpenMode open_mode);

    /// Attach external Berkeley DB file.
    /// Note: Should be already open.
    void Attach(CBDB_File& db_file);

    /// Fetches the record corresponding to the current key value.
    EBDB_ErrCode Fetch() { return x_Fetch(0); }
    
    /// Fetche the record corresponding to the current key value.
    /// Acquire write lock instead of read lock when doing the retrieval.
    /// Meaningful only in the presence of transactions.
    EBDB_ErrCode FetchForUpdate();
    
    enum EAfterWrite {
        eKeepData,    //!< Keep the inserted data for a while
        eDiscardData  //!< Invalidate the inserted data immediately after write
    };

    /// Insert new record
    EBDB_ErrCode Insert(EAfterWrite write_flag = eDiscardData);

    /// Delete record corresponding to the current key value.
    EBDB_ErrCode Delete(EIgnoreError on_error=eThrowOnError);

    /// Update record corresponding to the current key value. If record does not exist
    /// it will be inserted.
    EBDB_ErrCode UpdateInsert(EAfterWrite write_flag = eDiscardData);
    
    
    void BindKey (const char* field_name, 
                  CBDB_Field* key_field,  
                  size_t buf_size = 0);

    void BindData(const char* field_name,
                  CBDB_Field* data_field, 
                  size_t buf_size = 0, 
                  ENullable is_null = eNullable);
	
	/// Create the same fieldset as in dbf and bind them to the current file
	void DuplicateStructure(const CBDB_File& dbf);

    /// Get Buffer manager for key section of the file
    const CBDB_BufferManager* GetKeyBuffer() const { return m_KeyBuf.get(); }

    /// Get Buffer manager for data section of the file
    const CBDB_BufferManager* GetDataBuffer() const { return m_DataBuf.get(); }

    /// Get Buffer manager for key section of the file
    CBDB_BufferManager* GetKeyBuffer() { return m_KeyBuf.get(); }

    /// Get Buffer manager for data section of the file
    CBDB_BufferManager* GetDataBuffer() { return m_DataBuf.get(); }

    /// Sets maximum number of key fields participating in comparison
    /// Should be less than total number of key fields
    void SetFieldCompareLimit(unsigned int n_fields);

    /// Create new copy of m_DBT_Key.
    /// Caller is responsible for proper deletion. See also: DestroyDBT_Clone
    DBT* CloneDBT_Key();

    /// Free the DBT structure created by CloneDBT_Key.
    static void DestroyDBT_Clone(DBT* dbt);

    /// Set C-str detection
    void SetLegacyStringsCheck(bool value);

    /// CBDB_File keeps data in two buffers (key buffer and data buffer).
    /// TUnifiedFieldIndex is used to address fields in a non-ambigiuos manner.
    /// Negative index addresses fields in the key buffer, positive - data buffer
    /// Numbers are 1 based, 0 - means non-existing field
    typedef int TUnifiedFieldIndex;

    /// Get field index by name.
    /// @param
    ///   name field name to find (case insensitive)
    /// @return 
    ///   Field index (0 if not found)
    /// @sa TUnifiedFieldIndex
    TUnifiedFieldIndex GetFieldIdx(const string& name) const;

    /// Return field by field index
    /// @param
    ///    idx field index
    /// @return field reference
    const CBDB_Field& GetField(TUnifiedFieldIndex idx) const;
    CBDB_Field& GetField(TUnifiedFieldIndex idx);

	/// Fields deletion is managed by the class when own_fields is TRUE
	void SetFieldOwnership(bool own_fields);
	
	/// Return fields ownership flag
	bool IsOwnFields() const { return m_OwnFields; }
	
	/// Copy record (fields) from another BDB file 
	/// (MUST have the same structure)
	void CopyFrom(const CBDB_File& dbf);

    /// Run database verification (DB->verify)
    void Verify(const char* filename, const char* database, FILE* backup);

protected:
    /// Unpack internal record buffers
    void Discard();

    /// Set comparison function. Default implementation installs bdb_types based
    /// function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*);

    /// Read DB cursor
    EBDB_ErrCode ReadCursor(DBC* dbc, unsigned int bdb_flag);

    /// Write DB cursor
    EBDB_ErrCode WriteCursor(DBC* dbc, unsigned int bdb_flag, 
                             EAfterWrite write_flag);

    /// Write BLOB to DB cursor
    EBDB_ErrCode WriteCursor(const void* data, 
                             size_t      size, 
                             DBC* dbc, unsigned int bdb_flag, 
                             EAfterWrite write_flag);

    /// Delete DB cursor
    EBDB_ErrCode DeleteCursor(DBC* dbc, EIgnoreError);

    /// Check if all NOT NULL fields are assigned. 
    /// Throw an exception if constraint check failed.
    void CheckNullDataConstraint() const;

    /// Function disables processing of m_DBT_data. 
    /// This function can be used when creating custom BDB file
    /// data structures (BLOB storage, etc.) Caller takes full
    /// responsibility for filling m_DBT_Data with correct values.
    void DisableDataBufProcessing() { m_DataBufDisabled = true; }

    /// Disable NULL/not NULL in data fields
    /// (Performance tweak)
    void DisableNull() { m_DisabledNull = true; }

    /// Wrapper around get operation.    
    EBDB_ErrCode x_Fetch(unsigned int flags);    

	virtual void x_SetByteSwapped(bool bswp);

private:
    CBDB_File(const CBDB_File&);
    CBDB_File& operator= (const CBDB_File&);

    /// Record reading prolog function
    void x_StartRead();
    /// Record reading epilog function
    void x_EndRead();

    EBDB_ErrCode x_Write(unsigned int flags, EAfterWrite write_flag, 
                         DBC * dbc = 0);

    void x_CheckConstructBuffers();
	
	void x_ConstructKeyBuf();
	void x_ConstructDataBuf();

private:
    auto_ptr<CBDB_BufferManager>   m_KeyBuf;
    auto_ptr<CBDB_BufferManager>   m_DataBuf;
    bool                           m_BufsAttached;
    bool                           m_BufsCreated;
    bool                           m_DataBufDisabled;
    bool                           m_LegacyString;
	bool                           m_OwnFields;
    bool                           m_DisabledNull;

    friend class CBDB_FileCursor;
};



/// Berkeley DB file class optimized to work with 
/// tables having int as the primary key.
///

class NCBI_BDB_EXPORT CBDB_IdFile : public CBDB_File
{
public:
    CBDB_FieldInt4  IdKey;

public:
    CBDB_IdFile();
    virtual void SetCmp(DB* db);  
};



/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/// Make field index in CBDB_File format
///
/// @internal
inline
CBDB_File::TUnifiedFieldIndex BDB_GetUFieldIdx(int fidx, bool key)
{
    _ASSERT(fidx >= 0);
    ++fidx;
    return key ? (-fidx) : (fidx);
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//


inline 
void CBDB_RawFile::Open(const char* filename, EOpenMode open_mode)
{
    Open(filename, 0, open_mode);
}


inline 
const string& CBDB_RawFile::FileName() const
{
    return m_FileName;
}


inline 
const string& CBDB_RawFile::Database() const
{
    return m_Database;
}


inline 
bool CBDB_RawFile::IsOpen() const
{
    return !m_FileName.empty();
}

inline 
bool CBDB_RawFile::IsAttached() const
{
    return m_DB_Attached;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//



inline 
void CBDB_File::Open(const char* filename, EOpenMode open_mode)
{
    Open(filename, 0, open_mode);
}


inline 
void CBDB_File::CheckNullDataConstraint() const
{
    if ( !m_DisabledNull && m_DataBuf.get() )
        m_DataBuf->CheckNullConstraint();
}

inline
void CBDB_File::SetFieldCompareLimit(unsigned int n_fields)
{
    m_KeyBuf->SetFieldCompareLimit(n_fields);
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.38  2005/02/23 18:34:44  kuznets
 * CBDB_Transaction: added flag for non-associated transactions (perf.tuning)
 *
 * Revision 1.37  2004/11/23 17:08:53  kuznets
 * Implemented BLOB update in cursor
 *
 * Revision 1.36  2004/11/08 16:00:06  kuznets
 * Added option to disable NULL values for data fileds (performance optimization)
 *
 * Revision 1.35  2004/08/12 20:17:42  ucko
 * #include <stdio.h> so we can use FILE.
 *
 * Revision 1.34  2004/08/12 19:13:18  kuznets
 * +CBDB_File::Verify()
 *
 * Revision 1.33  2004/06/29 12:26:34  kuznets
 * Added functions to bulk copy fields and field structures
 *
 * Revision 1.32  2004/06/17 16:26:32  kuznets
 * + field ownership flag to file
 *
 * Revision 1.31  2004/06/03 11:46:11  kuznets
 *
 * +x_SetByteSwapped
 *
 * Revision 1.30  2004/05/06 18:17:58  rotmistr
 * Cursor Update/Delete implemented
 *
 * Revision 1.29  2004/03/08 13:31:58  kuznets
 * CBDB_File some methods made public
 *
 * Revision 1.28  2004/02/13 15:00:12  kuznets
 * + typedef TUnifiedFieldIndex plus methods to work with fields in CBDB_File
 *
 * Revision 1.27  2003/12/29 13:23:01  kuznets
 * Added support for transaction protected cursors.
 *
 * Revision 1.26  2003/12/22 18:53:50  kuznets
 * Added legacy(c-string) string format check flag.
 *
 * Revision 1.25  2003/12/16 13:42:58  kuznets
 * Added internal method to close association between transaction object
 * and BDB file object when transaction goes out of scope.
 *
 * Revision 1.24  2003/12/10 19:13:27  kuznets
 * Added support of berkeley db transactions
 *
 * Revision 1.23  2003/10/27 14:20:31  kuznets
 * + Buffer manager accessor functions for CBDB_File
 *
 * Revision 1.22  2003/10/16 19:26:07  kuznets
 * Added field comparison limit to the fields manager
 *
 * Revision 1.21  2003/10/15 18:10:09  kuznets
 * Several functions(Close, Delete) received optional parameter to ignore
 * errors (if any).
 *
 * Revision 1.20  2003/09/26 21:08:30  kuznets
 * Comments reformatting for doxygen
 *
 * Revision 1.19  2003/09/17 18:16:21  kuznets
 * Some class functions migrated into the public scope.
 *
 * Revision 1.18  2003/09/16 20:17:29  kuznets
 * CBDB_File: added methods to clone (and then destroy) DBT Key.
 *
 * Revision 1.17  2003/09/11 16:34:13  kuznets
 * Implemented byte-order independence.
 *
 * Revision 1.16  2003/08/27 20:02:10  kuznets
 * Added DB_ENV support
 *
 * Revision 1.15  2003/07/23 20:21:27  kuznets
 * Implemented new improved scheme for setting BerkeleyDB comparison function.
 * When table has non-segmented key the simplest(and fastest) possible function
 * is assigned (automatically without reloading CBDB_File::SetCmp function).
 *
 * Revision 1.14  2003/07/23 18:07:56  kuznets
 * Implemented couple new functions to work with attached files
 *
 * Revision 1.13  2003/07/22 19:21:04  kuznets
 * Implemented support of attachable berkeley db files
 *
 * Revision 1.12  2003/07/22 15:14:33  kuznets
 * + RawFile::CountRecs() function
 *
 * Revision 1.11  2003/07/18 20:11:05  kuznets
 * Added ReadWrite or Create open mode, added several accessor methods, Sync() method.
 *
 * Revision 1.10  2003/07/09 14:29:01  kuznets
 * Added support of files with duplicate access keys. (DB_DUP mode)
 *
 * Revision 1.9  2003/07/02 17:53:59  kuznets
 * Eliminated direct dependency from <db.h>
 *
 * Revision 1.8  2003/06/27 18:57:16  dicuccio
 * Uninlined strerror() adaptor.  Changed to use #include<> instead of #include ""
 *
 * Revision 1.7  2003/06/10 20:07:27  kuznets
 * Fixed header files not to repeat information from the README file
 *
 * Revision 1.6  2003/06/03 18:50:09  kuznets
 * Added dll export/import specifications
 *
 * Revision 1.5  2003/05/27 16:13:21  kuznets
 * Destructors of key classes declared virtual to make GCC happy
 *
 * Revision 1.4  2003/05/05 20:14:41  kuznets
 * Added CBDB_BLobFile, CBDB_File changed to support more flexible data record
 * management.
 *
 * Revision 1.3  2003/05/02 14:14:18  kuznets
 * new method UpdateInsert
 *
 * Revision 1.2  2003/04/29 16:48:31  kuznets
 * Fixed minor warnings in Sun Workshop compiler
 *
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
