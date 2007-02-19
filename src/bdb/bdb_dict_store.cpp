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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <bdb/bdb_dict_store.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////


CBDB_BlobDictionary<string>::CBDB_BlobDictionary()
    : m_MaxUid(0)
{
    BindKey ("key", &m_Key);
    BindData("uid", &m_Uid);
}

EBDB_ErrCode CBDB_BlobDictionary<string>::Read(const string& key, Uint4* val)
{
    m_Key = key;
    return Read(val);
}


EBDB_ErrCode CBDB_BlobDictionary<string>::Read(Uint4* val)
{
    EBDB_ErrCode err = Fetch();
    if (err == eBDB_Ok  &&  val) {
        *val = m_Uid;
    }
    return err;
}


EBDB_ErrCode CBDB_BlobDictionary<string>::Write(const string& key, Uint4 val)
{
    m_Key = key;
    m_Uid = val;
    return UpdateInsert();
}


string CBDB_BlobDictionary<string>::GetCurrentKey() const
{
    return (string)m_Key;
}


Uint4 CBDB_BlobDictionary<string>::GetCurrentUid() const
{
    return (Uint4)m_Uid;
}


Uint4 CBDB_BlobDictionary<string>::GetKey(const string& key)
{
    Uint4 uid = 0;
    Read(key, &uid);
    return uid;
}


Uint4 CBDB_BlobDictionary<string>::PutKey(const string& key)
{
    if (m_MaxUid == 0) {
        /// scan the database looking for the maximal UID
        CBDB_FileCursor cursor(*this);
        cursor.SetCondition(CBDB_FileCursor::eFirst);
        Uint4 this_uid = 0;
        void* p = &this_uid;
        while (cursor.Fetch(CBDB_FileCursor::eDefault,
                            &p, sizeof(this_uid),
                            CBDB_RawFile::eReallocForbidden) == eBDB_Ok) {
            m_MaxUid = max(m_MaxUid, this_uid);
        }
    }

    Uint4 next_uid = m_MaxUid + 1;
    if (Write(key, next_uid) == eBDB_Ok) {
        m_MaxUid = next_uid;
        return next_uid;
    } else {
        return 0;
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/14 13:11:31  dicuccio
 * Added components for creating split blob stores with arbitrarily defined key
 * spaces (including multi-component keys)
 *
 * Revision 1.1  2006/11/21 13:53:08  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
