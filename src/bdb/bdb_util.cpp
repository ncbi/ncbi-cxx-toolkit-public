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
 * File Description: BDB library utilities. 
 *
 */

#include <ncbi_pch.hpp>
#include <bdb/bdb_util.hpp>
#include <util/strsearch.hpp>

BEGIN_NCBI_SCOPE


/// Check if field is a variant of string, returns pointer on the
/// internal buffer (or NULL).
///
/// @internal
static
const char* BDB_GetStringFieldBuf(const CBDB_Field& fld)
{
    const CBDB_FieldString* str_fld = 
          dynamic_cast<const CBDB_FieldString*>(&fld);
    if (str_fld) {
        return (const char*)fld.GetBuffer();
    } else {
        const CBDB_FieldLString* lstr_fld = 
              dynamic_cast<const CBDB_FieldLString*>(&fld);
        if (lstr_fld) {
            return (const char*)fld.GetBuffer();
        }
    }
    return 0;
}

/// Search for value in the field buffer
///
/// @return -1 if not found, otherwise field number
///
/// @internal
static
int BDB_find_field(const CBDB_BufferManager& buffer_man,
                   const CBoyerMooreMatcher& matcher,
                   string*                   tmp_str)
{
    int fidx = -1;
    unsigned int fcount = buffer_man.FieldCount();
    for (unsigned i = 0; i < fcount; ++i) {
        const CBDB_Field& fld = buffer_man.GetField(i);

        if (fld.IsNull()) {
            continue;
        }

        unsigned str_buf_len;
        const char* str_buf = BDB_GetStringFieldBuf(fld);
        
        // for string based fields it should be non-0
        
        if (str_buf) {
            str_buf_len = fld.GetDataLength(str_buf);
            match:
            if (str_buf_len) {
                int pos = matcher.Search(str_buf, 0, str_buf_len);
                if (pos >= 0) {
                    fidx = i;
                    break;
                }
            }
        } else {
            if (tmp_str) {
                fld.ToString(*tmp_str);
                str_buf = tmp_str->c_str();
                str_buf_len = tmp_str->length();
                goto match;
            }
        }
        

    } // for i
    return fidx;
}

CBDB_File::TUnifiedFieldIndex BDB_find_field(const CBDB_File& dbf, 
                                             const CBoyerMooreMatcher& matcher,
                                             string* tmp_str)
{
    CBDB_File::TUnifiedFieldIndex fidx = 0;
    const CBDB_BufferManager* buffer_man;
    buffer_man =  dbf.GetKeyBuffer();
    if (buffer_man) {
        fidx = BDB_find_field(*buffer_man, matcher, tmp_str);
        if (fidx >= 0) {
            fidx = BDB_GetUFieldIdx(fidx, true /* key */);
            return fidx;
        } else {
            fidx = 0;
        }
    }

    buffer_man =  dbf.GetDataBuffer();
    if (buffer_man) {
        fidx = BDB_find_field(*buffer_man, matcher, tmp_str);
        if (fidx >= 0) {
            fidx = BDB_GetUFieldIdx(fidx, false /* key */);
            return fidx;        
        } else {
            fidx = 0;
        }
    }

    return fidx;
}

int BDB_get_rowid(const CBDB_File& dbf)
{
    const CBDB_BufferManager* buffer_man;
    buffer_man =  dbf.GetKeyBuffer();
    if (!buffer_man) {
        return 0;
    }
    const CBDB_Field& fld = buffer_man->GetField(0);
    {{
    const CBDB_FieldInt2* fi = dynamic_cast<const CBDB_FieldInt2*>(&fld);
    if (fi)
        return fi->Get();
    }}
    {{
    const CBDB_FieldInt4* fi = dynamic_cast<const CBDB_FieldInt4*>(&fld);
    if (fi)
        return fi->Get();
    }}
    {{
    const CBDB_FieldUint4* fi = dynamic_cast<const CBDB_FieldUint4*>(&fld);
    if (fi)
        return fi->Get();
    }}

    return 0;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/06/28 12:13:31  kuznets
 * BDB_find_field improved to search in non text fields too
 *
 * Revision 1.4  2004/05/17 20:55:12  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2004/03/11 13:15:40  kuznets
 * Minor bugfix
 *
 * Revision 1.2  2004/03/10 14:03:11  kuznets
 * + BDB_get_rowid
 *
 * Revision 1.1  2004/03/08 13:34:06  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

