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
 * File Description: Berkeley DB support library. 
 *                   Berkeley DB File classes.
 *
 */


#include "bdb_types.hpp"

#include <db.h>


BEGIN_NCBI_SCOPE


extern "C" 
{

// Simple and fast comparison function for tables with 
// non-segmented "unsigned int" keys
int bdb_uint_cmp(DB*, const DBT* val1, const DBT* val2);


// Simple and fast comparison function for tables with 
// non-segmented "int" keys
int bdb_int_cmp(DB*, const DBT* val1, const DBT* val2);


// General purpose DBD comparison function
int bdb_cmp(DB* db, const DBT* val1, const DBT* val2);

} // extern "C"




//////////////////////////////////////////////////////////////////
//
// Return codes
//


enum EBDB_ErrCode {
    eBDB_Ok        = 0,
    eBDB_NotFound  = DB_NOTFOUND,
    eBDB_KeyDup    = DB_KEYEXIST
};




//////////////////////////////////////////////////////////////////
//
// Raw file class wraps up basic Berkeley DB operations. 
//

class CBDB_RawFile
{
public:
    static const char kDefaultDatabase[];  // = "_table"

    enum EOpenMode {
        eReadWrite   = 0,
        eReadOnly    = DB_RDONLY,
        eCreate      = DB_CREATE  // implies 'eReadWrite' too
    };

    enum EReallocMode {
        eReallocAllowed,
        eReallocForbidden
    };

public:
    CBDB_RawFile();
    ~CBDB_RawFile();

    // Open file with specified access mode
    void Open(const char* filename, EOpenMode open_mode);
    // Open file with specified filename and database name.
    // (Berkeley DB supports having several database tables in one file.) 
    void Open(const char* filename, const char* database,
              EOpenMode open_mode);
    // Close file
    void Close();
    // Reopen database file. (Should be already open).
    void Reopen(EOpenMode open_mode);

    // Remove the database specified by the filename and database arguments
    void Remove(const char* filename, const char* database = 0);
    // Empty the database. Return number of records removed.
    unsigned int Truncate();

    // Set Berkeley DB page size value. By default OS default is used.
    void SetPageSize(unsigned int page_size);

    // Set Berkeley DB memory cache size for the file (default is 256K).
    void SetCacheSize(unsigned int cache_size);

    const string& FileName() const;
    const string& Database() const;

    // Set comparison function. Default implementation installs bdb_types based
    // function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*) = 0;

    // Return TRUE if the file is open
    bool IsOpen() const;

protected:
    // Embedded auto-pointer style guard class for DB structure
	class CDB_guard
	{
	public:
		CDB_guard(DB** db) : m_DB(db) {}
		~CDB_guard()
		{ 
			if (m_DB  &&  *m_DB) {
                (*m_DB)->close(*m_DB, 0);
                *m_DB = 0;
            }
		}
		void release() { m_DB = 0; }
    private:
		DB** m_DB;
	};


private:
    CBDB_RawFile(const CBDB_RawFile&);
    CBDB_RawFile& operator= (const CBDB_RawFile&);

protected:
    void x_Open(const char* filename, const char* database,
                EOpenMode open_mode);
    void x_Create(const char* filename, const char* database);

    enum ECloseMode {
        eIgnoreError,
        eThrowOnError
    };
    void x_Close(ECloseMode close_mode);

    // Create m_DB member, set page, cache parameters
    void x_CreateDB();

protected:
    DB*              m_DB;
    DBT              m_DBT_Key;
    DBT              m_DBT_Data;

private:
    string           m_FileName;       // filename
    string           m_Database;       // db name in file (optional)
    unsigned         m_PageSize;
    unsigned         m_CacheSize;

    static const int kOpenFileMask;
};




//////////////////////////////////////////////////////////////////
//
// Berkeley DB file class. 
// Implements primary key and fields functionality.
//

class CBDB_File : public CBDB_RawFile
{
public:
    CBDB_File();

    // Open file with specified access mode
    void Open(const char* filename, EOpenMode open_mode);

    // Open file with specified filename and database name.
    // (Berkeley DB supports having several database tables in one file.) 
    void Open(const char* filename, const char* database,
              EOpenMode open_mode);
    // Reopen the db file
    void Reopen(EOpenMode open_mode);

    // Fetches the record corresponding to the current key value.
    EBDB_ErrCode Fetch();

    enum EAfterWrite {
        eKeepData,    // Keep the inserted data for a while
        eDiscardData  // Invalidate the inserted data immediately after write
    };

    // Insert new record
    EBDB_ErrCode Insert(EAfterWrite write_flag = eDiscardData);

    // Delete record corresponding to the current key value.
    EBDB_ErrCode Delete();

    // Update record corresponding to the current key value. If record does not exist
    // it will be inserted.
    EBDB_ErrCode UpdateInsert(EAfterWrite write_flag = eDiscardData);
    
    
    void BindKey (const char* field_name, 
                  CBDB_Field* key_field,  
                  size_t buf_size = 0);

    void BindData(const char* field_name,
                  CBDB_Field* data_field, 
                  size_t buf_size = 0, 
                  ENullable is_null = eNullable);

protected:
    // Unpack internal record buffers
    void Discard();

    // Set comparison function. Default implementation installs bdb_types based
    // function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*);

    // Create DB cursor
    DBC* CreateCursor() const;

    // Read DB cursor
    EBDB_ErrCode ReadCursor(DBC* dbc, unsigned int bdb_flag);

    // Check if all NOT NULL fields are assigned. 
    // Throw an exception if constraint check failed.
    void CheckNullDataConstraint() const;

    // Function disables processing of m_DBT_data. 
    // This function can be used when creating custom BDB file
    // data structures (BLOB storage, etc.) Caller takes full
    // responsibility for filling m_DBT_Data with correct values.
    void DisableDataBufProcessing() { m_DataBufDisabled = true; }

private:
    CBDB_File(const CBDB_File&);
    CBDB_File& operator= (const CBDB_File&);

    void x_Open  (const char* filename, const char* database,
                  EOpenMode open_mode);
    void x_Create(const char* filename, const char* database);

    // Record reading prolog function
    void x_StartRead();
    // Record reading epilog function
    void x_EndRead();

    EBDB_ErrCode x_Write(unsigned int flags, EAfterWrite write_flag);

private:
    auto_ptr<CBDB_BufferManager>   m_KeyBuf;
    auto_ptr<CBDB_BufferManager>   m_DataBuf;
    bool                           m_BufsAttached;
    bool                           m_BufsCreated;
    bool                           m_DataBufDisabled;

    friend class CBDB_FileCursor;
};



//////////////////////////////////////////////////////////////////
//
// Berkeley DB file class optimized to work with 
// tables having int as the primary key.
//

class CBDB_IdFile : public CBDB_File
{
public:
    CBDB_FieldInt4  IdKey;

public:
    CBDB_IdFile() : CBDB_File()
    {
        BindKey("id", &IdKey);
    }

    virtual void SetCmp(DB* db)
    {
        int ret = db->set_bt_compare(db, bdb_int_cmp);
        BDB_CHECK(ret, 0);
    }
    
};



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//



inline void CBDB_RawFile::Open(const char* filename, EOpenMode open_mode)
{
    Open(filename, 0, open_mode);
}


inline const string& CBDB_RawFile::FileName() const
{
    return m_FileName;
}


inline const string& CBDB_RawFile::Database() const
{
    return m_Database;
}


inline bool CBDB_RawFile::IsOpen() const
{
    return !m_FileName.empty();
}



/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//



inline void CBDB_File::Open(const char* filename, EOpenMode open_mode)
{
    Open(filename, 0, open_mode);
}


inline void CBDB_File::CheckNullDataConstraint() const
{
    if ( m_DataBuf.get() )
        m_DataBuf->CheckNullConstraint();
}



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
