#ifndef UTIL__BLOBCACHE__HPP
#define UTIL__BLOBCACHE__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BLOB cache interface specs. 
 *
 */

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////
//
// ICacheStream - interface to sequentially read-write cache BLOBs 
//

struct ICacheStream
{
    virtual ~ICacheStream() {}

    virtual void Read(void *buf, size_t buf_size, size_t *bytes_read)=0;
    virtual void Write(const void* buf, size_t buf_size)=0;
};


//////////////////////////////////////////////////////////////////
//
// BLOB cache read/write/maintanance interface.
//

class IBLOB_Cache
{
public:

    enum EKeepOldVersions
    {
        eKeepOld,
        eDropOld
    };

    // Add or replace BLOB
    // flag parameter specifies our approach to old versions of the same BLOB
    // (keep or drop)
    virtual void Store(const string& key,
                       int version, 
                       const void* data, 
                       size_t size,
                       EKeepOldVersions flag=eDropOld) = 0;

    // Check if BLOB exists, return non zero size.
    // 0 value indicates that BLOB does not exist in the cache
    virtual size_t GetSize(const string& key,
                           int version) = 0;

    // Fetch the BLOB
    // Function returns FALSE if BLOB does not exists. 
    // Throws an exception if provided memory is insufficient to read the BLOB
    virtual bool Read(const string& key, 
                      int version, 
                      void* buf, 
                      size_t buf_size) = 0;

    // Return sequential stream interface pointer to read BLOB data.
    // Function returns NULL if BLOB does not exist
    virtual ICacheStream* GetReadStream(const string& key, 
                                        int version) = 0;

    // Return sequential stream interface pointer to write BLOB data.
    virtual ICacheStream* GetWriteStream(const string& key, 
                                         int version,
                                         EKeepOldVersions flag=eDropOld) = 0;

    // Remove all versions of the specified BLOB
    virtual void Remove(const string& key) = 0;

    // Return access time for the specified BLOB
    virtual time_t GetAccessTime(const string& key,
                                 int version)=0;

    // Delete all BLOBs with access time older than specified
    virtual void Purge(time_t access_time) = 0;

    // Delete BLOBs with access time older than specified
    virtual void Purge(const string& key, time_t access_time) = 0;


    virtual ~IBLOB_Cache(){}

};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/09/22 18:40:34  kuznets
 * Minor cosmetics fixed
 *
 * Revision 1.1  2003/09/17 20:51:15  kuznets
 * Local cache interface - first revision
 *
 *
 * ===========================================================================
 */

#endif