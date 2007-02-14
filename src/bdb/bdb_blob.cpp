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
 * File Description:  BDB libarary BLOB implementations.
 *
 */


#include <ncbi_pch.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_trans.hpp>

#include <db.h>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CBDB_BLobFile::
//

CBDB_BLobFile::CBDB_BLobFile(EDuplicateKeys dups,
                             EDBType        db_type)
    : CBDB_File(dups, db_type)
{ 
    DisableDataBufProcessing();
}


EBDB_ErrCode CBDB_BLobFile::Fetch()
{
    return Fetch(0, 0, eReallocForbidden); 
}

EBDB_ErrCode CBDB_BLobFile::Fetch(void**       buf, 
                                  size_t       buf_size, 
                                  EReallocMode allow_realloc) 
{
    m_DBT_Data->data = buf ? *buf : 0;
    m_DBT_Data->ulen = (unsigned)buf_size;
    m_DBT_Data->size = 0;

    if (allow_realloc == eReallocForbidden) {
        m_DBT_Data->flags = DB_DBT_USERMEM;
    } else {
        if (m_DBT_Data->data == 0) {
            m_DBT_Data->flags = DB_DBT_MALLOC;
        } else {
            m_DBT_Data->flags = DB_DBT_REALLOC;
        }
    }

    EBDB_ErrCode ret;

    ret = CBDB_File::Fetch();

    if ( buf )
        *buf = m_DBT_Data->data;

    if (ret == eBDB_NotFound)
        return eBDB_NotFound;

    return eBDB_Ok;
}

 
EBDB_ErrCode 
CBDB_BLobFile::ReadRealloc(TBuffer& buffer)
{
    EBDB_ErrCode ret;
    // use the maximum capacity
    size_t capacity = buffer.capacity();
    if (capacity > buffer.size()) {
        buffer.resize(capacity);
    }
    if (buffer.size() == 0) {
        buffer.resize(10);
    }
    while(1) {
        try {
            void* p = &buffer[0];
            ret = Fetch(&p, buffer.size(), eReallocForbidden);
            if (ret != eBDB_Ok) {
                buffer.resize(0);
                return ret;
            }
            buffer.resize(LobSize());
        }
        catch (CBDB_ErrnoException& ex) {
            // check if we have insufficient buffer
            if (ex.IsBufferSmall() || ex.IsNoMem()) {
                // increase the buffer and re-read
                buffer.resize(LobSize());
            } else {
                throw;
            }
            continue;
        }
        break;
    } // while
    return ret;
}


EBDB_ErrCode CBDB_BLobFile::GetData(void* buf, size_t size) 
{
    return Fetch(&buf, size, eReallocForbidden); 
}


EBDB_ErrCode CBDB_BLobFile::Insert(const void* data, size_t size) 
{
    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    EBDB_ErrCode ret = CBDB_File::Insert();
    return ret;
}

unsigned CBDB_BLobFile::Append(const void* data, size_t size)
{
    _ASSERT(m_DB_Type == eQueue);
    _ASSERT(size <= GetRecLen());

    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    return CBDB_File::Append();
}

EBDB_ErrCode CBDB_BLobFile::UpdateInsert(const void* data, size_t size)
{
    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    EBDB_ErrCode ret = CBDB_File::UpdateInsert();
    return ret;
}

size_t CBDB_BLobFile::LobSize() const
{ 
    return m_DBT_Data->size;
}


CBDB_BLobStream* CBDB_BLobFile::CreateStream() 
{
    EBDB_ErrCode ret = Fetch();

    DBT* dbt = CloneDBT_Key();
    // lob exists, we can read it now (or write)
    if (ret == eBDB_Ok) {
        return new CBDB_BLobStream(m_DB, dbt, LobSize(), GetTxn());
    }
    // no lob yet (write stream)
    return new CBDB_BLobStream(m_DB, dbt, 0, GetTxn()); 
}

CBDB_BlobReaderWriter* CBDB_BLobFile::CreateReaderWriter()
{
    EBDB_ErrCode ret = Fetch();

    DBT* dbt = CloneDBT_Key();
    // lob exists, we can read it now (or write)
    if (ret == eBDB_Ok) {
        return new CBDB_BlobReaderWriter(m_DB, dbt, LobSize(), GetTxn());
    }
    // no lob yet (write stream)
    return new CBDB_BlobReaderWriter(m_DB, dbt, 0, GetTxn()); 
}

IReader* CBDB_BLobFile::CreateReader()
{
    EBDB_ErrCode ret = Fetch();

    DBT* dbt = CloneDBT_Key();
    // lob exists, we can read it now (or write)
    if (ret == eBDB_Ok) {
        return new CBDB_BlobReaderWriter(m_DB, dbt, LobSize(), GetTxn());
    }
    return 0; 
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_IdBlobFile::
//


CBDB_IdBlobFile::CBDB_IdBlobFile(EDuplicateKeys dup_keys,
                                 EDBType        db_type)
: CBDB_BLobFile(dup_keys, db_type)
{
    BindKey("id", &id);
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_BlobReaderWriter::
//


CBDB_BlobReaderWriter::CBDB_BlobReaderWriter(DB*     db, 
                                             DBT*    dbt_key, 
                                             size_t  blob_size, 
                                             DB_TXN* txn)
: m_DB(db),
  m_DBT_Key(dbt_key),
  m_DBT_Data(0),
  m_Txn(txn),
  m_Pos(0),
  m_BlobSize(blob_size)
{
    m_DBT_Data = new DBT;
    ::memset(m_DBT_Data, 0, sizeof(DBT)); 
}


CBDB_BlobReaderWriter::~CBDB_BlobReaderWriter()
{
    CBDB_File::DestroyDBT_Clone(m_DBT_Key);
    delete m_DBT_Data;
}

void CBDB_BlobReaderWriter::SetTransaction(CBDB_Transaction* trans) 
{
    if (trans) {
        m_Txn = trans->GetTxn();
    } else {
        m_Txn = 0;
    }
}

ERW_Result 
CBDB_BlobReaderWriter::PendingCount(size_t* count)
{ 
    if (count) {
        *count = (m_BlobSize > 0 ? (m_BlobSize - m_Pos) : 0); 
    }
    return eRW_Success;
}

ERW_Result CBDB_BlobReaderWriter::Read(void*   buf,
                                       size_t  count,
                                       size_t* bytes_read)
{
    m_DBT_Data->flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    m_DBT_Data->data = buf;
    m_DBT_Data->ulen = (unsigned)count;
    m_DBT_Data->dlen = (unsigned)count;
    m_DBT_Data->doff = m_Pos;
    m_DBT_Data->size = 0;
    
    int ret = m_DB->get(m_DB,
                        0,         // DB_TXN*
                        m_DBT_Key,
                        m_DBT_Data,
                        0);
    BDB_CHECK(ret, "CBDB_BlobReaderWriter");

    m_Pos += m_DBT_Data->size;
    *bytes_read = m_DBT_Data->size;
    return eRW_Success;
    
}

ERW_Result CBDB_BlobReaderWriter::Write(const void* buf,
                                        size_t      count,
                                        size_t*     bytes_written)
{
    m_DBT_Data->flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    m_DBT_Data->data = const_cast<void*> (buf);
    m_DBT_Data->size = (unsigned)count;
    m_DBT_Data->ulen = (unsigned)count;
    m_DBT_Data->doff = m_Pos;
    m_DBT_Data->dlen = (unsigned)count;

    int ret = m_DB->put(m_DB,
                        m_Txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        0);
    BDB_CHECK(ret, "CBDB_BlobReaderWriter");

    m_Pos += (unsigned)count;
    if (bytes_written) {
        *bytes_written = count;
    }
    return eRW_Success;
}

ERW_Result CBDB_BlobReaderWriter::Flush(void)
{
    return eRW_Success;
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_BLobStream::
//

CBDB_BLobStream::CBDB_BLobStream(DB* db, 
                                 DBT* dbt_key, 
                                 size_t blob_size, 
                                 DB_TXN* txn)
: m_DB(db),
  m_DBT_Key(dbt_key),
  m_DBT_Data(0),
  m_Txn(txn),
  m_Pos(0),
  m_BlobSize(blob_size)
{
    m_DBT_Data = new DBT;
    ::memset(m_DBT_Data, 0, sizeof(DBT)); 
}


CBDB_BLobStream::~CBDB_BLobStream()
{
    CBDB_File::DestroyDBT_Clone(m_DBT_Key);
    delete m_DBT_Data;
}


void CBDB_BLobStream::SetTransaction(CBDB_Transaction* trans) 
{
    if (trans) {
        m_Txn = trans->GetTxn();
    } else {
        m_Txn = 0;
    }
}

void CBDB_BLobStream::Read(void *buf, size_t buf_size, size_t *bytes_read) 
{
    m_DBT_Data->flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    m_DBT_Data->data = buf;
    m_DBT_Data->ulen = (unsigned)buf_size;
    m_DBT_Data->dlen = (unsigned)buf_size;
    m_DBT_Data->doff = m_Pos;
    m_DBT_Data->size = 0;
    
    int ret = m_DB->get(m_DB,
                        0,         // DB_TXN*
                        m_DBT_Key,
                        m_DBT_Data,
                        0);
    BDB_CHECK(ret, "BLOBStream");

    m_Pos += m_DBT_Data->size;
    *bytes_read = m_DBT_Data->size;
}

void CBDB_BLobStream::Write(const void* buf, size_t buf_size) 
{
/*    if (m_Pos == 0)
        m_DBT_Data->flags = 0;
    else*/
        m_DBT_Data->flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;

    m_DBT_Data->data = const_cast<void*> (buf);
    m_DBT_Data->size = (unsigned)buf_size;
    m_DBT_Data->ulen = (unsigned)buf_size;
    m_DBT_Data->doff = m_Pos;
    m_DBT_Data->dlen = (unsigned)buf_size;

    int ret = m_DB->put(m_DB,
                        m_Txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        0);
    BDB_CHECK(ret, "BLOBStream");

    m_Pos += (unsigned)buf_size;
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_LobFile::
//


CBDB_LobFile::CBDB_LobFile()
 : m_LobKey(0)
{
    m_DBT_Key->data = &m_LobKey;
    m_DBT_Key->size = (unsigned)sizeof(m_LobKey);
    m_DBT_Key->ulen = (unsigned)sizeof(m_LobKey);
    m_DBT_Key->flags = DB_DBT_USERMEM;
}


EBDB_ErrCode CBDB_LobFile::x_Put(unsigned int lob_id,
                                 const void*  data,
                                 size_t       size,
				 bool can_update)
{
    _ASSERT(lob_id);
    _ASSERT(size);
    _ASSERT(m_DB);

    // paranoia check
    _ASSERT(m_DBT_Key->data == &m_LobKey);
    _ASSERT(m_DBT_Key->size == sizeof(m_LobKey));

    if (IsByteSwapped()) {
        m_LobKey = (unsigned int) CByteSwap::GetInt4((unsigned char*)&lob_id);
    } else {
        m_LobKey = lob_id;
    }

    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    int ret = m_DB->put(m_DB,
                        0,     // DB_TXN*
                        m_DBT_Key,
                        m_DBT_Data,
                        can_update?0:DB_NOOVERWRITE /*| DB_NODUPDATA*/
                        );
    if (ret == DB_KEYEXIST)
        return eBDB_KeyDup;
    BDB_CHECK(ret, FileName().c_str());
    return eBDB_Ok;
}


EBDB_ErrCode CBDB_LobFile::Insert(unsigned int lob_id,
                                  const void*  data,
                                  size_t       size) 
{
	return x_Put(lob_id, data, size, false);
}

EBDB_ErrCode CBDB_LobFile::InsertUpdate(unsigned int lob_id,
                                        const void*  data,
                                        size_t       size) 
{
	return x_Put(lob_id, data, size, true);
}

// v 4.3.xx introduced new error code DB_BUFFER_SMALL
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 3
        #define BDB_CHECK_BUFFER_SMALL
    #endif
#endif

EBDB_ErrCode CBDB_LobFile::Fetch(unsigned int lob_id,
                                 void**       buf,
                                 size_t       buf_size,
                                 EReallocMode allow_realloc) 
{
    _ASSERT(lob_id);
    _ASSERT(m_DB);

    // paranoia check
    _ASSERT(m_DBT_Key->data  == &m_LobKey);
    _ASSERT(m_DBT_Key->size  == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key->ulen  == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key->flags == DB_DBT_USERMEM);
    
    if (IsByteSwapped()) {
        m_LobKey = (unsigned int) CByteSwap::GetInt4((unsigned char*)&lob_id);
    } else {
        m_LobKey = lob_id;
    }
    
    // Here we attempt to read only key value and get information
    // about LOB size. In this case get operation fails with ENOMEM
    // error message (ignored)

    m_DBT_Data->data = buf ? *buf : 0;
    m_DBT_Data->ulen = (unsigned)buf_size;
    m_DBT_Data->size = 0;

    if (m_DBT_Data->data == 0  &&  m_DBT_Data->ulen != 0) {
        _ASSERT(0);
    }

    if (allow_realloc == eReallocForbidden) {
        m_DBT_Data->flags = DB_DBT_USERMEM;
    } else {
        if (m_DBT_Data->data == 0) {
            m_DBT_Data->flags = DB_DBT_MALLOC;
        } else {
            m_DBT_Data->flags = DB_DBT_REALLOC;
        }
    }

    int ret = m_DB->get(m_DB,
                        0,          // DB_TXN*
                        m_DBT_Key,
                        m_DBT_Data,
                        0
                        );

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    if (ret == ENOMEM) {
        if (m_DBT_Data->data == 0)
            return eBDB_Ok;  // to be retrieved later using GetData()
    }

# ifdef BDB_CHECK_BUFFER_SMALL
    if ((ret == DB_BUFFER_SMALL) && (m_DBT_Data->data == 0)) {
    } else {
        BDB_CHECK(ret, FileName().c_str());
    }
# else
    BDB_CHECK(ret, FileName().c_str());
# endif


    if ( buf )
        *buf = m_DBT_Data->data;

    return eBDB_Ok;
}



EBDB_ErrCode CBDB_LobFile::GetData(void* buf, size_t size) 
{
    _ASSERT(m_LobKey);
    _ASSERT(m_DB);
    _ASSERT(size >= m_DBT_Data->size);
    _ASSERT(m_DBT_Data->size);

    // paranoia check
    _ASSERT(m_DBT_Key->data == &m_LobKey);
    _ASSERT(m_DBT_Key->size == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key->ulen == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key->flags == DB_DBT_USERMEM);

    m_DBT_Data->data  = buf;
    m_DBT_Data->ulen  = (unsigned)size;
    m_DBT_Data->flags = DB_DBT_USERMEM;

    int ret = m_DB->get(m_DB,
                        0,          // DB_TXN*
                        m_DBT_Key,
                        m_DBT_Data,
                        0
                        );

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    BDB_CHECK(ret, FileName().c_str());
    return eBDB_Ok;
}

size_t CBDB_LobFile::LobSize() const
{
    return m_DBT_Data->size;
}

void CBDB_LobFile::SetCmp(DB*)
{
    BDB_CompareFunction func = BDB_Uint4Compare;
    if (IsByteSwapped()) {
        func = BDB_ByteSwap_Uint4Compare;
    }

    _ASSERT(func);
    int ret = m_DB->set_bt_compare(m_DB, func);
    BDB_CHECK(ret, 0);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/11/30 14:16:11  kuznets
 * Removed buf_size parameter (ReadRealloc()) size passed as vector property
 *
 * Revision 1.28  2006/11/30 12:42:08  dicuccio
 * Standardize buffer handling around CBDB_RawFile::TBuffer, a typedef for
 * vector<unsigned char>
 *
 * Revision 1.27  2006/11/30 11:09:21  kuznets
 * Tweaked buffer read to resize vector to full capacity
 *
 * Revision 1.26  2006/11/14 13:27:38  dicuccio
 * Use field widths in comparators
 *
 * Revision 1.25  2006/03/28 16:37:28  kuznets
 * +ReadRealloc()
 *
 * Revision 1.24  2006/01/13 14:42:44  vasilche
 * Fixed comparison in assert() call.
 *
 * Revision 1.23  2005/12/14 19:26:42  kuznets
 * Added support for queue db type
 *
 * Revision 1.22  2005/09/19 13:02:41  dicuccio
 * Add fag to permit duplicate keys in a BLob file
 *
 * Revision 1.21  2004/12/28 16:45:44  kuznets
 * +CBDB_BlobFile::UpdateInsert()
 *
 * Revision 1.20  2004/12/07 16:09:17  kuznets
 * Compatibility changes (berkelye db v4.3)
 *
 * Revision 1.19  2004/07/14 20:00:30  rotmistr
 * InsertUpdate added
 *
 * Revision 1.18  2004/06/04 16:10:32  kuznets
 * Fixed bug with byteswapping when writin/reading LOBs
 *
 * Revision 1.17  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.16  2004/03/12 15:08:04  kuznets
 * Removed unnecessary DB_NODUPDATA flag (db->put)
 *
 * Revision 1.15  2003/12/29 17:06:24  kuznets
 * +CBDB_BlobStream::SetTransaction()
 *
 * Revision 1.14  2003/12/29 16:52:29  kuznets
 * Added transaction support for BLOB stream
 *
 * Revision 1.13  2003/10/24 13:40:32  kuznets
 * Implemeneted PendingCount
 *
 * Revision 1.12  2003/09/29 16:44:56  kuznets
 * Reimplemented SetCmp to fix cross-platform byte swapping bug
 *
 * Revision 1.11  2003/09/29 16:27:06  kuznets
 * Cleaned up 64-bit compilation warnings
 *
 * Revision 1.10  2003/09/26 18:48:30  kuznets
 * Removed dead SetCmp function
 *
 * Revision 1.9  2003/09/17 18:18:42  kuznets
 * Implemented BLOB streaming
 *
 * Revision 1.8  2003/07/02 17:55:34  kuznets
 * Implementation modifications to eliminated direct dependency from <db.h>
 *
 * Revision 1.7  2003/06/10 20:08:27  kuznets
 * Fixed function names.
 *
 * Revision 1.6  2003/05/09 13:44:57  kuznets
 * Fixed a bug in cursors based on BLOB storage
 *
 * Revision 1.5  2003/05/08 13:43:40  kuznets
 * Bug fix.
 *
 * Revision 1.4  2003/05/05 20:15:32  kuznets
 * Added CBDB_BLobFile
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
