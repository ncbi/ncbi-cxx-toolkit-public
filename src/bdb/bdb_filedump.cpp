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
 * File Description:  BDB File covertion into text.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_filedump.hpp>

BEGIN_NCBI_SCOPE

void CBDB_FileDumper::Dump(const string& dump_file_name, CBDB_File& db)
{
    CNcbiOfstream out(dump_file_name.c_str());
    if (!out) {
        string err = "Cannot open text file:";
        err.append(dump_file_name);
        BDB_THROW(eInvalidValue, err);
    }

    Dump(out, db);
}

void CBDB_FileDumper::Dump(CNcbiOstream& out, CBDB_File& db)
{
    const CBDB_BufferManager* key  = db.GetKeyBuffer();
    const CBDB_BufferManager* data = db.GetDataBuffer();

    // Print header
    if (m_PrintNames == ePrintNames) {
        if (key) {
            for (unsigned i = 0; i < key->FieldCount(); ++i) {
                const CBDB_Field& fld = key->GetField(i);
                if (i != 0)
                    out << m_ColumnSeparator;
                out << fld.GetName();
            }
        }

        if (data) {
            for (unsigned i = 0; i < data->FieldCount(); ++i) {
                const CBDB_Field& fld = data->GetField(i);
                out << m_ColumnSeparator << fld.GetName();
            }
        }
        out << endl;
    }

    // Print values
    CBDB_FileCursor cur(db);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {

        if (key) {
            for (unsigned i = 0; i < key->FieldCount(); ++i) {
                const CBDB_Field& fld = key->GetField(i);
                if (i != 0)
                    out << m_ColumnSeparator;
                out << fld.GetString();
            }
        }

        if (data) {
            for (unsigned i = 0; i < data->FieldCount(); ++i) {
                const CBDB_Field& fld = data->GetField(i);
                out << m_ColumnSeparator << fld.GetString();
            }
        }
        out << endl;

    }
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/10/28 14:57:13  kuznets
 * Implemeneted field names printing
 *
 * Revision 1.1  2003/10/27 14:18:22  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
