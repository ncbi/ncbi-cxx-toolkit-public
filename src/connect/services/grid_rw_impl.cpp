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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/grid_rw_impl.hpp>

BEGIN_NCBI_SCOPE

CStringOrBlobStorageWriter::
CStringOrBlobStorageWriter(size_t max_string_size, IBlobStorage& storage,
                           string& data)
    : m_MaxBuffSize(max_string_size), m_Storage(storage), m_BlobOstr(NULL),
      m_Data(data)
{
}

CStringOrBlobStorageWriter::~CStringOrBlobStorageWriter() 
{
    m_Storage.Reset();
}

ERW_Result CStringOrBlobStorageWriter::Write(const void* buf,
                                             size_t      count,
                                             size_t*     bytes_written)
{
    if (!count) {
        if (bytes_written)
            *bytes_written = 0;
        return eRW_Success;
    }
    if (m_BlobOstr) {
        return writeToStream(buf,count,bytes_written);
    }
    if (m_Data.size()+count > m_MaxBuffSize) {
        _ASSERT(!m_BlobOstr);
        string tmp = m_Data;
        m_Data = "";
        m_BlobOstr = &m_Storage.CreateOStream(m_Data);
        ERW_Result ret = eRW_Success;
        if (m_Data.size() > 0) {
            ret = writeToStream(&*tmp.begin(), tmp.size());
            if( ret != eRW_Success ) {
                m_Data = tmp;
                if (bytes_written)
                    *bytes_written = 0;
                return ret;
            }
        }
        return writeToStream(buf,count,bytes_written);            
    }
    m_Data.append( (const char*)buf, count);
    if (bytes_written)
        *bytes_written = count;
    return eRW_Success;
}

ERW_Result CStringOrBlobStorageWriter::Flush(void)
{
    m_Storage.Reset();
    return eRW_Success;       
}


ERW_Result CStringOrBlobStorageWriter::writeToStream(const void* buf,
                                                     size_t      count,
                                                     size_t*     bytes_written)
{
    _ASSERT(m_BlobOstr);
    if (bytes_written)
        *bytes_written = 0;
    m_BlobOstr->write((const char*)buf, count);
    if (m_BlobOstr->good()) {
        if (bytes_written)
            *bytes_written = count;
        return eRW_Success;
    }
    return eRW_Error;
}


////////////////////////////////////////////////////////////////////////////
//

CStringOrBlobStorageReader::
CStringOrBlobStorageReader(const string& data, IBlobStorage& storage, 
                           IBlobStorage::ELockMode lock_mode,
                           size_t& data_size)
    : m_Data(data), m_Storage(storage), m_BlobIstr(NULL)
{
    if (m_Storage.IsKeyValid(data)) {
        try {
            m_BlobIstr = &m_Storage.GetIStream(m_Data, &data_size, lock_mode);
        } catch (CBlobStorageException& ex) {
            if (ex.GetErrCode() != CBlobStorageException::eBlobNotFound)
                throw;
            data_size = m_Data.size();
        }
    } else {
        m_CurPos = m_Data.begin();
        data_size = m_Data.size();
    }

}

CStringOrBlobStorageReader::~CStringOrBlobStorageReader()
{
    m_Storage.Reset();
}
   
ERW_Result CStringOrBlobStorageReader::Read(void*   buf,
                                            size_t  count,
                                            size_t* bytes_read)
{
    if (m_BlobIstr) {
        if (m_BlobIstr->eof()) {
            if (bytes_read) *bytes_read = 0;
            return eRW_Eof;
        }
        if (!m_BlobIstr->good()) {
            if (bytes_read) *bytes_read = 0;
            return eRW_Error;
        }
        m_BlobIstr->read((char*) buf, count);
        if (bytes_read) *bytes_read = m_BlobIstr->gcount();
        return eRW_Success;
    }
    if (m_CurPos == m_Data.end()) {
        if (bytes_read) *bytes_read = 0;
        return eRW_Eof;
    }
    size_t bytes_rest = distance(m_CurPos, m_Data.end());
    if (count > bytes_rest)
        count = bytes_rest;
    memcpy(buf, &*m_CurPos, count);
    advance(m_CurPos, count);
    if (bytes_read) *bytes_read = count;
    return eRW_Success;
}

ERW_Result CStringOrBlobStorageReader::PendingCount(size_t* count)
{
    if (m_BlobIstr) {
        if (m_BlobIstr->good() && !m_BlobIstr->eof())
            *count = m_BlobIstr->rdbuf()->in_avail();
        else 
            *count = 0;
    }
    else 
        *count = distance(m_CurPos, m_Data.end());
    return eRW_Success;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2006/03/15 17:22:25  didenko
 * Added CStringOrBlobStorage{Reader,Writer} classes
 *
 * ===========================================================================
 */
 
