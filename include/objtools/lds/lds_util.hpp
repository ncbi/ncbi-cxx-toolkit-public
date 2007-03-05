#ifndef LDS_UTIL_HPP__
#define LDS_UTIL_HPP__
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
 * File Description:  LDS utilities.
 *
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define LDS_GETMAXID(id, db, id_name) \
    { \
        id = 0; \
        CBDB_FileCursor cur(db); \
        cur.SetCondition(CBDB_FileCursor::eLast); \
        if (cur.Fetch() == eBDB_Ok) { \
            id = db.id_name; \
        } else { \
            id = 0; \
        } \
    }

// Return TRUE if id belongs to the set
inline bool LDS_SetTest(const CLDS_Set& s, int id)
{
    return s[id];
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
