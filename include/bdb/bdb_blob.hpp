#ifndef BDB_BLOB_HPP__
#define BDB_BLOB_HPP__

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
 *                   Berkeley DB BLOB support.
 *
 */

#include "bdb_file.hpp"


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
// Berkeley DB Large OBject File class. 
// Implements simple BLOB storage.
//

class CBDB_LobFile : public CBDB_RawFile
{
public:
    enum EReallocMode {
        eReallocAllowed,
        eReallocForbidden
    };

    CBDB_LobFile();

    EBDB_ErrCode Insert(unsigned int lob_id, const void* data, size_t size);

    // Fetch LOB record. On success, LOB size becomes available
    // (see LobSize()), and the value can be obtained using GetData().
    //
    // <pre>
    // Typical usage for this function is:
    // 1. Call Fetch()
    // 2. Allocate LobSize() chunk of memory
    // 3. Use GetData() to retrive lob value
    // </pre>
    EBDB_ErrCode Fetch(unsigned int lob_id);

    // Fetch LOB record directly into the provided '*buf'.
    // If size of the LOB is greater than 'buf_size', then
    // if reallocation is allowed -- '*buf' will be reallocated
    // to fit the LOB size; otherwise, a exception will be thrown.
    EBDB_ErrCode Fetch(unsigned int lob_id, 
                       void**       buf, 
                       size_t       buf_size, 
                       EReallocMode allow_realloc);

    // Get LOB size. Becomes available right after successfull Fetch.
    size_t LobSize() const;

    // Copy LOB data into the 'buf'.
    // Throw an exception if buffer size 'size' is less than LOB size. 
    EBDB_ErrCode GetData(void* buf, size_t size);

protected:
    // Sets custom comparison function
    virtual void SetCmp(DB* db);

private:
    unsigned int  m_LobKey;  
};




/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CBDB_LobFile::
//


inline EBDB_ErrCode CBDB_LobFile::Fetch(unsigned int lob_id)
{
    return Fetch(lob_id, 0, 0, eReallocForbidden);
}


inline size_t CBDB_LobFile::LobSize() const
{
    return m_DBT_Data.size;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

#endif