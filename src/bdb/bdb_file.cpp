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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary file implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_trans.hpp>

#include <db.h>

#ifdef verify
#undef verify
#endif

BEGIN_NCBI_SCOPE

const char CBDB_RawFile::kDefaultDatabase[] = "_table";
const int  CBDB_RawFile::kOpenFileMask      = 0664;



// Auto-pointer style guard class for DB structure
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


/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//



CBDB_RawFile::CBDB_RawFile(EDuplicateKeys dup_keys)
: m_DB(0),
  m_DBT_Key(0),
  m_DBT_Data(0),
  m_Env(0),
  m_Trans(0),
  m_TransAssociation(CBDB_Transaction::eFullAssociation),
  m_DB_Attached(false),
  m_ByteSwapped(false),
  m_PageSize(0),
  m_CacheSize(256 * 1024),
  m_DuplicateKeys(dup_keys)
{
    try
    {
        m_DBT_Key = new DBT;
        m_DBT_Data = new DBT;
    }
    catch (...)
    {
        delete m_DBT_Key;
        delete m_DBT_Data;
        throw;
    }

    ::memset(m_DBT_Key,  0, sizeof(DBT));
    ::memset(m_DBT_Data, 0, sizeof(DBT));
}


CBDB_RawFile::~CBDB_RawFile()
{
    x_Close(eIgnoreError);
    delete m_DBT_Key;
    delete m_DBT_Data;

    // It's illegal to close a file involved in active transactions
    
    if (m_Trans && (m_Trans->IsInProgress())) {
        _ASSERT(0);
        
        // If we are here we can try to communicate by throwing
        // an exception. It's illegal, but situation is bad enough already
        
        BDB_THROW(eTransInProgress, 
                  "Cannot close the file while transaction is in progress.");
    }
}


void CBDB_RawFile::Close()
{
    x_Close(eThrowOnError);
}

void CBDB_RawFile::Attach(CBDB_RawFile& bdb_file)
{
   Close();
   m_DB = bdb_file.m_DB;
   m_DB_Attached = true; 
}

void CBDB_RawFile::SetEnv(CBDB_Env& env)
{
    m_Env = &env;
}

DB_TXN* CBDB_RawFile::GetTxn()
{
    if (m_Trans)
        return m_Trans->GetTxn();
    return 0;
}

void CBDB_RawFile::x_Close(EIgnoreError close_mode)
{
    if ( m_FileName.empty() )
        return;

    if (m_DB_Attached) {
        m_DB = 0;
        m_DB_Attached = false;
    }
    else
    if (m_DB) {
        int ret = m_DB->close(m_DB, 0);
        m_DB = 0;
        if (close_mode == eThrowOnError) {
            BDB_CHECK(ret, m_FileName.c_str());
        }
    }

    m_FileName.erase();
    m_Database.erase();
}


void CBDB_RawFile::Open(const char* filename,
                        const char* database,
                        EOpenMode   open_mode)
{
    if ( !m_FileName.empty() )
        Close();

    if (!database  ||  !*database)
        database = 0; // database = kDefaultDatabase;

    x_Open(filename, database, open_mode);

    m_FileName = filename;
    if (database)
        m_Database = database;
    else
        m_Database = "";
}


void CBDB_RawFile::Reopen(EOpenMode open_mode)
{
    _ASSERT(!m_FileName.empty());

    if (m_DB_Attached) {
        BDB_THROW(eInvalidOperation, "Cannot reopen attached object");
    }

    int ret = m_DB->close(m_DB, 0);
    m_DB = 0;

    BDB_CHECK(ret, m_FileName.c_str());
    x_Open(m_FileName.c_str(),
           !m_Database.empty() ? m_Database.c_str() : 0,
           open_mode);
}


void CBDB_RawFile::Remove(const char* filename, const char* database)
{
    if (!database  ||  !*database)
        database = 0; // database = kDefaultDatabase;

    if (m_DB_Attached) {
        BDB_THROW(eInvalidOperation, "Cannot remove attached object");
    }

    // temporary DB is used here, because BDB remove call invalidates the
    // DB argument redardless of the result.
    DB* db = 0;
    CDB_guard guard(&db);
    int ret = db_create(&db, m_Env ? m_Env->GetEnv() : 0, 0);
    BDB_CHECK(ret, 0);

    ret = db->remove(db, filename, database, 0);
    guard.release();
    if (ret == ENOENT || ret == EINVAL)
        return;  // Non existent table cannot be removed

    BDB_CHECK(ret, filename);
}


unsigned int CBDB_RawFile::Truncate()
{
    _ASSERT(m_DB != 0);
    u_int32_t count;
    DB_TXN* txn = GetTxn();
    int ret = m_DB->truncate(m_DB,
                             txn,
                             &count,
                             0);

    BDB_CHECK(ret, FileName().c_str());
    return count;
}


void CBDB_RawFile::SetCacheSize(unsigned int cache_size)
{
    if (m_DB) {
        int ret = m_DB->set_cachesize(m_DB, 0, m_CacheSize, 1);
        BDB_CHECK(ret, 0);
    }
    m_CacheSize = cache_size;
}


void CBDB_RawFile::SetTransaction(CBDB_Transaction* trans)
{
    if (m_Trans) {
        if (m_TransAssociation == (int) CBDB_Transaction::eFullAssociation) {
            m_Trans->RemoveFile(this);
        }
    }

    m_Trans = trans;
    if (m_Trans) {
        m_TransAssociation = m_Trans->GetAssociationMode();
        if (m_TransAssociation == (int) CBDB_Transaction::eFullAssociation) {
            m_Trans->AddFile(this);
        }
    }
}

void CBDB_RawFile::x_RemoveTransaction(CBDB_Transaction* trans)
{
    if (trans == m_Trans) {
        m_Trans = 0;
    }
}



void CBDB_RawFile::x_CreateDB()
{
    _ASSERT(m_DB == 0);
    _ASSERT(!m_DB_Attached);

    CDB_guard guard(&m_DB);

    int ret = db_create(&m_DB, m_Env ? m_Env->GetEnv() : 0, 0);
    BDB_CHECK(ret, 0);

    SetCmp(m_DB);

    if ( m_PageSize ) {
        ret = m_DB->set_pagesize(m_DB, m_PageSize);
        BDB_CHECK(ret, 0);
    }

    if (!m_Env) {
        ret = m_DB->set_cachesize(m_DB, 0, m_CacheSize, 1);
        BDB_CHECK(ret, 0);
    }

    if (DuplicatesAllowed()) {
        ret = m_DB->set_flags(m_DB, DB_DUP);
        BDB_CHECK(ret, 0);
    }

    guard.release();
}


void CBDB_RawFile::x_Open(const char* filename,
                          const char* database,
                          EOpenMode   open_mode)
{   
    int ret;    
    if (m_DB == 0) {
        x_CreateDB();
    }

    if (open_mode == eCreate) {
        Remove(filename, database);
        x_Create(filename, database);
    }
    else {
        u_int32_t open_flags;

        switch (open_mode)
        {
        case eReadOnly:
            open_flags = DB_RDONLY;
            break;
        case eCreate:
            open_flags = DB_CREATE;
            break;
        default:
            open_flags = 0;
            break;
        }

        DB_TXN* txn = 0; // GetTxn();

        if (m_Env) {
            if (m_Env->IsTransactional()) {
                open_flags |= DB_THREAD | DB_AUTO_COMMIT;
            }
        }

        ret = m_DB->open(m_DB,
                         txn,
                         filename,
                         database,             // database name
                         DB_BTREE,
                         open_flags,
                         kOpenFileMask
                        );
        if ( ret ) {
            if (open_mode == eCreate || 
                open_mode == eReadWriteCreate)
            {
                x_Create(filename, database);
            }
            else {
                m_DB->close(m_DB, 0);
                m_DB = 0;
                BDB_CHECK(ret, filename);
            }
        } else {
            // file opened succesfully, check if it needs
            // a byte swapping (different byteorder)

            int isswapped;
            ret = m_DB->get_byteswapped(m_DB, &isswapped);
            BDB_CHECK(ret, filename);

            m_ByteSwapped = (isswapped!=0);
            if (m_ByteSwapped) {
                // re-open the file
                m_DB->close(m_DB, 0);
                m_DB = 0;

                x_SetByteSwapped(m_ByteSwapped);
                x_CreateDB();

                ret = m_DB->open(m_DB,
                                 txn,
                                 filename,
                                 database, // database name
                                 DB_BTREE,
                                 open_flags,
                                 kOpenFileMask);
                BDB_CHECK(ret, filename);

            }

        }
    } // else open_mode == Create

    m_OpenMode = open_mode;

}


void CBDB_RawFile::SetPageSize(unsigned int page_size)
{
    _ASSERT(m_DB == 0); // we can set page size only before opening the file
    if (((page_size - 1) & page_size) != 0) {
        BDB_THROW(eInvalidValue, "Page size must be power of 2");
    }
    m_PageSize = page_size;
}

void CBDB_RawFile::Sync()
{
    int ret = m_DB->sync(m_DB, 0);
    BDB_CHECK(ret, FileName().c_str());
}


// check BDB version 4.3 changed DB->stat signature

#ifndef BDB_USE_NEW_STAT
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 3
        #define BDB_USE_NEW_STAT
    #endif
#endif
#endif
    

unsigned CBDB_RawFile::CountRecs()
{
    DB_BTREE_STAT* stp;
#ifdef BDB_USE_NEW_STAT
    CBDB_Transaction* trans = GetTransaction();
    DB_TXN* txn = trans ? trans->GetTxn() : 0;
    int ret = m_DB->stat(m_DB, txn, &stp, 0);
#else
    int ret = m_DB->stat(m_DB, &stp, 0);
#endif

    BDB_CHECK(ret, FileName().c_str());
    u_int32_t rc = stp->bt_ndata;

    ::free(stp);

    return rc;
}

void CBDB_RawFile::x_Create(const char* filename, const char* database)
{
    _ASSERT(!m_DB_Attached);
    u_int32_t open_flags = DB_CREATE;

    DB_TXN* txn = 0; //GetTxn();

    if (m_Env) {
        if (m_Env->IsTransactional()) {
            open_flags |= DB_THREAD | DB_AUTO_COMMIT;
        }
    }

    int ret = m_DB->open(m_DB,
                         txn,
                         filename,
                         database,        // database name
                         DB_BTREE,
                         open_flags,
                         kOpenFileMask);
    if ( ret ) {
        m_DB->close(m_DB, 0);
        m_DB = 0;
        BDB_CHECK(ret, filename);
    }
}


DBC* CBDB_RawFile::CreateCursor(CBDB_Transaction* trans, 
                                unsigned int      flags) const
{
    DBC* cursor;

    if (!m_DB) {
        BDB_THROW(eInvalidValue, "Cannot create cursor for unopen file.");
    }

    DB_TXN* txn = 0; // GetTxn();
    if (trans) {
        txn = trans->GetTxn();
    }

    int ret = m_DB->cursor(m_DB,
                           txn,
                           &cursor,
                           flags);
    BDB_CHECK(ret, FileName().c_str());
    return cursor;
}

void CBDB_RawFile::x_SetByteSwapped(bool bswp)
{
    m_ByteSwapped = bswp;
}

/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//


CBDB_File::CBDB_File(EDuplicateKeys dup_keys)
    : CBDB_RawFile(dup_keys),
      m_KeyBuf(new CBDB_BufferManager),
      m_BufsAttached(false),
      m_BufsCreated(false),
      m_DataBufDisabled(false),
      m_LegacyString(false),
      m_OwnFields(false),
      m_DisabledNull(false)
{
}

void CBDB_File::SetFieldOwnership(bool own_fields) 
{ 
    m_OwnFields = own_fields; 
    
    m_KeyBuf->SetFieldOwnership(own_fields);
    if (m_DataBuf.get() != 0) {
        m_DataBuf->SetFieldOwnership(own_fields);
    }
}

void CBDB_File::x_ConstructKeyBuf()
{
	m_KeyBuf.reset(new CBDB_BufferManager);
    m_KeyBuf->SetLegacyStringsCheck(m_LegacyString);
    m_KeyBuf->SetFieldOwnership(m_OwnFields);
}		

void CBDB_File::x_ConstructDataBuf()
{
	m_DataBuf.reset(new CBDB_BufferManager);
    if (!m_DisabledNull) {
        m_DataBuf->SetNullable();
    }
    m_DataBuf->SetLegacyStringsCheck(m_LegacyString);
    m_DataBuf->SetFieldOwnership(m_OwnFields);
}		

void CBDB_File::BindKey(const char* field_name,
                        CBDB_Field* key_field,
                        size_t      buf_size)
{
    _ASSERT(!IsOpen());
    _ASSERT(m_KeyBuf.get());
    _ASSERT(key_field);

    key_field->SetName(field_name);
    m_KeyBuf->Bind(key_field);
    if ( buf_size )
        key_field->SetBufferSize(buf_size);
}


void CBDB_File::BindData(const char* field_name,
                         CBDB_Field* data_field,
                         size_t      buf_size,
                         ENullable   is_nullable)
{
    _ASSERT(!IsOpen());
    _ASSERT(data_field);

    data_field->SetName(field_name);

    if (m_DataBuf.get() == 0) {  // data buffer is not yet created 
		x_ConstructDataBuf();
    }

    m_DataBuf->Bind(data_field);
    if ( buf_size > 0) {
        data_field->SetBufferSize(buf_size);
    }
    if (is_nullable == eNullable && !m_DisabledNull) {
        data_field->SetNullable();
    }
}


void CBDB_File::Open(const char* filename,
                     const char* database,
                     EOpenMode   open_mode)
{
    if ( IsOpen() )
        Close();

    CBDB_RawFile::Open(filename, database, open_mode);
    x_CheckConstructBuffers();

    m_DB->app_private = (void*) m_KeyBuf.get();

}


void CBDB_File::Reopen(EOpenMode open_mode)
{
    CBDB_RawFile::Reopen(open_mode);
    m_DB->app_private = (void*) m_KeyBuf.get();
    if ( m_DataBuf.get() ) {
        m_DataBuf->SetAllNull();
    }
    bool byte_swapped = IsByteSwapped();
    m_KeyBuf->SetByteSwapped(byte_swapped);
    if (m_DataBuf.get()) {
        m_DataBuf->SetByteSwapped(byte_swapped);
    }
}


void CBDB_File::Attach(CBDB_File& db_file)
{
    CBDB_RawFile::Attach(db_file);
    x_CheckConstructBuffers();
    SetLegacyStringsCheck(db_file.m_LegacyString);
}

void CBDB_File::SetLegacyStringsCheck(bool value) 
{ 
    m_LegacyString = value; 
    if (m_KeyBuf.get()) {
        m_KeyBuf->SetLegacyStringsCheck(value);
    }
    if (m_DataBuf.get()) {
        m_DataBuf->SetLegacyStringsCheck(value);
    }
}

void CBDB_File::x_SetByteSwapped(bool bswp)
{
    CBDB_RawFile::x_SetByteSwapped(bswp);
    m_KeyBuf->SetByteSwapped(bswp);
    if (m_DataBuf.get()) {
        m_DataBuf->SetByteSwapped(bswp);
    }
}

void CBDB_File::Verify(const char* filename, 
                       const char* database, 
                       FILE* backup)
{
    if (m_DB == 0) {
        x_CreateDB();
    }
    x_CheckConstructBuffers();
    m_DB->app_private = (void*) m_KeyBuf.get();

    /*int ret = */
    m_DB->verify(m_DB, filename, database, backup, backup ? DB_SALVAGE: 0);
}

// v 4.3.xx introduced new error code DB_BUFFER_SMALL
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 3
        #define BDB_CHECK_BUFFER_SMALL
    #endif
#endif


EBDB_ErrCode CBDB_File::x_Fetch(unsigned int flags)
{
    x_StartRead();

    DB_TXN* txn = GetTxn();

    int ret = m_DB->get(m_DB,
                        txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        flags);
                        
    if (ret == DB_NOTFOUND) {
        return eBDB_NotFound;
    }
    
    // Disable error reporting for custom m_DBT_data management

# ifdef BDB_CHECK_BUFFER_SMALL
    if ((ret == ENOMEM || ret == DB_BUFFER_SMALL) 
           && m_DataBufDisabled && m_DBT_Data->data == 0) {
        ret = 0;
    }
# else
    if (ret == ENOMEM && m_DataBufDisabled && m_DBT_Data->data == 0) {
        ret = 0;
    }
# endif
    BDB_CHECK(ret, FileName().c_str());

    x_EndRead();
    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::FetchForUpdate()
{
    return x_Fetch(DB_RMW);
}


DBT* CBDB_File::CloneDBT_Key()
{
    x_StartRead();
    x_EndRead();

    DBT* dbt = new DBT;
    ::memset(dbt,  0, sizeof(DBT));

    // Clone the "data" area (needs to be properly deleted!)
    if (m_DBT_Key->size) {
        dbt->size = m_DBT_Key->size;
        unsigned char* p = (unsigned char*)malloc(dbt->size);
        ::memcpy(p, m_DBT_Key->data, m_DBT_Key->size);
        dbt->data = p;
    }
    return dbt;
}

void CBDB_File::DestroyDBT_Clone(DBT* dbt)
{
    unsigned char* p = (unsigned char*)dbt->data;
    free(p);
    delete dbt;
}


EBDB_ErrCode CBDB_File::Insert(EAfterWrite write_flag)
{
    CheckNullDataConstraint();

    unsigned int flags;
    if (DuplicatesAllowed()) {
        flags = 0;
    } else {
        flags = /*DB_NODUPDATA |*/ DB_NOOVERWRITE;
    }
    return x_Write(flags, write_flag);
}


EBDB_ErrCode CBDB_File::UpdateInsert(EAfterWrite write_flag)
{
    CheckNullDataConstraint();
    return x_Write(0, write_flag);
}


EBDB_ErrCode CBDB_File::Delete(EIgnoreError on_error)
{
    m_KeyBuf->PrepareDBT_ForWrite(m_DBT_Key);
    DB_TXN* txn = GetTxn();

    int ret = m_DB->del(m_DB,
                        txn,
                        m_DBT_Key,
                        0);
    if (on_error != eIgnoreError) {
        BDB_CHECK(ret, FileName().c_str());
    }
    Discard();
    return eBDB_Ok;
}


void CBDB_File::Discard()
{
    m_KeyBuf->ArrangePtrsUnpacked();
    if ( m_DataBuf.get() ) {
        m_DataBuf->ArrangePtrsUnpacked();
        m_DataBuf->SetAllNull();
    }
}


void CBDB_File::SetCmp(DB* db)
{
    BDB_CompareFunction func = m_KeyBuf->GetCompareFunction();
    _ASSERT(func);
    int ret = db->set_bt_compare(db, func);
    BDB_CHECK(ret, 0);
}


CBDB_File::TUnifiedFieldIndex 
CBDB_File::GetFieldIdx(const string& name) const
{
    int fidx = 0;
    if (m_KeyBuf.get()) {
        fidx = m_KeyBuf->GetFieldIndex(name);
        if (fidx >= 0) {    //  field name found
            return BDB_GetUFieldIdx(fidx, true /*key*/);
        }
    }

    if (m_DataBuf.get()) {
        fidx = m_DataBuf->GetFieldIndex(name);
        if (fidx >= 0) {    //  field name found
            return BDB_GetUFieldIdx(fidx, false /*non-key*/);
        }
    }
    return 0;
}

const CBDB_Field& CBDB_File::GetField(TUnifiedFieldIndex idx) const
{
    _ASSERT(idx != 0);

    const CBDB_BufferManager* buffer;

    if (idx < 0) { // key buffer
        idx = -idx;
        --idx;
        buffer = m_KeyBuf.get();
    } else {  // data buffer
        --idx;
        buffer = m_DataBuf.get();
    }

    _ASSERT(buffer);

    const CBDB_Field& fld = buffer->GetField(idx);
    return fld;
}


CBDB_Field& CBDB_File::GetField(TUnifiedFieldIndex idx)
{
    _ASSERT(idx != 0);

    CBDB_BufferManager* buffer;

    if (idx < 0) {     // key buffer
        idx = -idx;
        --idx;
        buffer = m_KeyBuf.get();
    } else {          // data buffer
        --idx;
        buffer = m_DataBuf.get();
    }

    _ASSERT(buffer);

    CBDB_Field& fld = buffer->GetField(idx);
    return fld;
}


void CBDB_File::CopyFrom(const CBDB_File& dbf)
{
    const CBDB_BufferManager* src_key  = dbf.GetKeyBuffer();
    const CBDB_BufferManager* src_data = dbf.GetDataBuffer();
	
    CBDB_BufferManager* key  = GetKeyBuffer();
    CBDB_BufferManager* data = GetDataBuffer();
	
	key->CopyFrom(*src_key);
	if (data) {
		data->CopyFrom(*src_data);
	}
}

void CBDB_File::DuplicateStructure(const CBDB_File& dbf)
{
    const CBDB_BufferManager* src_key  = dbf.GetKeyBuffer();
    const CBDB_BufferManager* src_data = dbf.GetDataBuffer();
	
	_ASSERT(src_key);
	
	x_ConstructKeyBuf();
	m_KeyBuf->DuplicateStructureFrom(*src_key);
	
	if (src_data) {
		x_ConstructDataBuf();
		m_DataBuf->DuplicateStructureFrom(*src_data);
	} else {
		m_DataBuf.reset(0);
	}
}

EBDB_ErrCode CBDB_File::ReadCursor(DBC* dbc, unsigned int bdb_flag)
{
    x_StartRead();

    if (m_DataBufDisabled) {
        m_DBT_Data->size  = 0;
        m_DBT_Data->flags = 0;
        m_DBT_Data->data  = 0;        
    }

    int ret = dbc->c_get(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         bdb_flag);

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    BDB_CHECK(ret, FileName().c_str());
    x_EndRead();
    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::WriteCursor(DBC* dbc, unsigned int bdb_flag, 
                                    EAfterWrite write_flag)
{
    CheckNullDataConstraint();
    return x_Write(bdb_flag, write_flag, dbc);
}

EBDB_ErrCode CBDB_File::WriteCursor(const void* data, 
                                    size_t      size, 
                                    DBC* dbc, 
                                    unsigned int bdb_flag, 
                                    EAfterWrite write_flag)
{
    if (!m_DataBufDisabled) {
        BDB_THROW(eInvalidOperation, "BLOB operation on non BLOB table");
    }

    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    return x_Write(bdb_flag, write_flag, dbc);
}


EBDB_ErrCode CBDB_File::DeleteCursor(DBC* dbc, EIgnoreError on_error)
{
    int ret = dbc->c_del(dbc,0);

    if (on_error != CBDB_File::eIgnoreError) {
        BDB_CHECK(ret, FileName().c_str());
    }

    return eBDB_Ok;
}

void CBDB_File::x_CheckConstructBuffers()
{
    if (!m_BufsAttached  &&  !m_BufsCreated) {
        if (m_KeyBuf->FieldCount() == 0) {
            BDB_THROW(eInvalidValue, "Empty BDB key (no fields defined).");
        }
        
        m_KeyBuf->Construct();
        if ( m_DataBuf.get() ) {
            m_DataBuf->Construct();
            m_DataBuf->SetAllNull();
        }
        m_BufsCreated = 1;
    }
}

void CBDB_File::x_StartRead()
{
    m_KeyBuf->Pack();
    m_KeyBuf->PrepareDBT_ForRead(m_DBT_Key);

    if (!m_DataBufDisabled) {
        if ( m_DataBuf.get() ) {
            m_DataBuf->PrepareDBT_ForRead(m_DBT_Data);
        }
        else {
            m_DBT_Data->size  = 0;
            m_DBT_Data->flags = 0;
            m_DBT_Data->data  = 0;
        }
    }
}


void CBDB_File::x_EndRead()
{
    m_KeyBuf->SetDBT_Size(m_DBT_Key->size);
    m_KeyBuf->ArrangePtrsPacked();
    if ( m_DataBuf.get() ) {
        m_DataBuf->SetDBT_Size(m_DBT_Data->size);
        m_DataBuf->ArrangePtrsPacked();
    }
}


EBDB_ErrCode CBDB_File::x_Write(unsigned int flags, 
                                EAfterWrite write_flag, 
                                DBC * dbc)
{
    m_KeyBuf->PrepareDBT_ForWrite(m_DBT_Key);

    if (!m_DataBufDisabled) {
        if ( m_DataBuf.get() ) {
            m_DataBuf->PrepareDBT_ForWrite(m_DBT_Data);
        }
    }

    int ret=0;
    if(dbc) {
        ret = dbc->c_put(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         flags
            );
        
    } else {
        DB_TXN* txn = GetTxn();
    
        ret = m_DB->put(m_DB,
                        txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        flags
            );
    }
    
    if (ret == DB_KEYEXIST)
        return eBDB_KeyDup;

    BDB_CHECK(ret, FileName().c_str());

    if (write_flag == eDiscardData) {
        Discard();
    }

    return eBDB_Ok;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_IdFile::
//

CBDB_IdFile::CBDB_IdFile() 
: CBDB_File()
{
    BindKey("id", &IdKey);
}

void CBDB_IdFile::SetCmp(DB* /* db */)
{
    BDB_CompareFunction func = BDB_IntCompare;
    if (IsByteSwapped()) {
        func = BDB_ByteSwap_IntCompare;
    }

    _ASSERT(func);
    int ret = m_DB->set_bt_compare(m_DB, func);
    BDB_CHECK(ret, 0);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.48  2005/02/23 18:35:30  kuznets
 * CBDB_Transaction: added flag for non-associated transactions (perf.tuning)
 *
 * Revision 1.47  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.46  2004/12/07 16:09:17  kuznets
 * Compatibility changes (berkelye db v4.3)
 *
 * Revision 1.45  2004/11/23 17:09:11  kuznets
 * Implemented BLOB update in cursor
 *
 * Revision 1.44  2004/11/08 16:00:11  kuznets
 * Added option to disable NULL values for data fileds (performance optimization)
 *
 * Revision 1.43  2004/08/12 20:13:15  ucko
 * Avoid possible interference from macros named verify.
 *
 * Revision 1.42  2004/08/12 19:13:24  kuznets
 * +CBDB_File::Verify()
 *
 * Revision 1.41  2004/06/29 12:26:53  kuznets
 * Added functions to bulk copy fields and field structures
 *
 * Revision 1.40  2004/06/21 15:09:51  kuznets
 * Fixed formatting
 *
 * Revision 1.39  2004/06/17 16:27:02  kuznets
 * + field ownership flag to file
 *
 * Revision 1.38  2004/06/03 11:47:07  kuznets
 * Fixed a bug in setting architecture dependent comparison function
 *
 * Revision 1.37  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.36  2004/05/06 18:18:14  rotmistr
 * Cursor Update/Delete implemented
 *
 * Revision 1.35  2004/03/12 15:08:36  kuznets
 * Removed unnecessary DB_NODUPDATA flag (db->put)
 *
 * Revision 1.34  2004/03/08 13:32:43  kuznets
 * Code clean-up
 *
 * Revision 1.33  2004/02/13 15:01:02  kuznets
 * + methods working with TUnifiedFieldIndex
 *
 * Revision 1.32  2004/02/11 17:56:20  kuznets
 * Assign legacy strings checking flag when attaching to a file
 *
 * Revision 1.31  2003/12/29 13:23:53  kuznets
 * Added support for transaction protected cursors.
 *
 * Revision 1.30  2003/12/22 18:57:02  kuznets
 * Minor implementation changes: DBT size passed to underlying buffer
 * manager in order to better detect c-strings.
 *
 * Revision 1.29  2003/12/16 13:43:35  kuznets
 * + CBDB_RawFile::x_RemoveTransaction
 *
 * Revision 1.28  2003/12/12 19:12:21  kuznets
 * Fixed bug in transactional file opening
 *
 * Revision 1.27  2003/12/12 14:09:12  kuznets
 * Changed file opening code to work correct in transactional environment.
 *
 * Revision 1.26  2003/12/10 19:14:08  kuznets
 * Added support of berkeley db transactions
 *
 * Revision 1.25  2003/10/15 18:11:00  kuznets
 * Several functions(Close, Delete) received optional parameter to ignore
 * errors (if any).
 *
 * Revision 1.24  2003/09/17 18:18:21  kuznets
 * Fixed some memory allocation bug in key cloning.
 *
 * Revision 1.23  2003/09/16 20:17:40  kuznets
 * CBDB_File: added methods to clone (and then destroy) DBT Key.
 *
 * Revision 1.22  2003/09/12 18:06:13  kuznets
 * Commenented out usage of the default sub-database name when no name is supplied.
 * When "database" argument is NULL BerkeleyDB create files of a slightly less-complex
 * structure which should have positive effect on performance and may cure some
 * problems.
 *
 * Revision 1.21  2003/09/11 16:34:35  kuznets
 * Implemented byte-order independence.
 *
 * Revision 1.20  2003/08/27 20:02:57  kuznets
 * Added DB_ENV support
 *
 * Revision 1.19  2003/07/23 20:21:43  kuznets
 * Implemented new improved scheme for setting BerkeleyDB comparison function.
 * When table has non-segmented key the simplest(and fastest) possible function
 * is assigned (automatically without reloading CBDB_File::SetCmp function).
 *
 * Revision 1.18  2003/07/23 18:08:48  kuznets
 * SetCacheSize function implemented
 *
 * Revision 1.17  2003/07/22 19:21:19  kuznets
 * Implemented support of attachable berkeley db files
 *
 * Revision 1.16  2003/07/22 15:15:02  kuznets
 * + RawFile::CountRecs() function
 *
 * Revision 1.15  2003/07/21 18:33:28  kuznets
 * Fixed bug with duplicate key tables
 *
 * Revision 1.14  2003/07/18 20:11:32  kuznets
 * Implemented ReadWrite or Create open mode.
 *
 * Revision 1.13  2003/07/16 13:33:33  kuznets
 * Added error condition if cursor is created on unopen file.
 *
 * Revision 1.12  2003/07/09 14:29:24  kuznets
 * Added support of files with duplicate access keys. (DB_DUP mode)
 *
 * Revision 1.11  2003/07/02 17:55:35  kuznets
 * Implementation modifications to eliminated direct dependency from <db.h>
 *
 * Revision 1.10  2003/06/25 16:35:56  kuznets
 * Bug fix: data file gets created even if eCreate flag was not specified.
 *
 * Revision 1.9  2003/06/10 20:08:27  kuznets
 * Fixed function names.
 *
 * Revision 1.8  2003/05/27 18:43:45  kuznets
 * Fixed some compilation problems with GCC 2.95
 *
 * Revision 1.7  2003/05/09 13:44:57  kuznets
 * Fixed a bug in cursors based on BLOB storage
 *
 * Revision 1.6  2003/05/05 20:15:32  kuznets
 * Added CBDB_BLobFile
 *
 * Revision 1.5  2003/05/02 14:11:59  kuznets
 * + UpdateInsert method
 *
 * Revision 1.4  2003/04/30 20:25:42  kuznets
 * Bug fix
 *
 * Revision 1.3  2003/04/29 19:07:22  kuznets
 * Cosmetics..
 *
 * Revision 1.2  2003/04/28 14:51:55  kuznets
 * #include directives changed to conform the NCBI policy
 *
 * Revision 1.1  2003/04/24 16:34:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
