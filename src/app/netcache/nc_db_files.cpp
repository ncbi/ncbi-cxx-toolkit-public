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
 * Author: Pavel Ivanov
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include "nc_db_files.hpp"
#include "nc_utils.hpp"



BEGIN_NCBI_SCOPE


static const char* kNCDBIndex_Table           = "NCX";
static const char* kNCDBIndex_FileIdCol       = "id";
static const char* kNCDBIndex_FileNameCol     = "nm";
static const char* kNCDBIndex_CreatedTimeCol  = "tm";

static const char* kNCSettings_Table          = "NCS";
static const char* kNCSettings_NameCol        = "nm";
static const char* kNCSettings_ValueCol       = "v";



CNCDBIndexFile::~CNCDBIndexFile(void)
{}

void
CNCDBIndexFile::CreateDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement stmt(this);

    sql.Clear();
    sql << "create table if not exists " << kNCDBIndex_Table
        << "(" << kNCDBIndex_FileIdCol      << " integer primary key,"
               << kNCDBIndex_FileNameCol    << " varchar not null,"
               << kNCDBIndex_CreatedTimeCol << " int not null"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();

    sql.Clear();
    sql << "create table if not exists " << kNCSettings_Table
        << "(" << kNCSettings_NameCol   << " varchar not null,"
               << kNCSettings_ValueCol  << " varchar not null,"
               << "unique(" << kNCSettings_NameCol << ")"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

void
CNCDBIndexFile::NewDBFile(Uint4 file_id, const string& file_name)
{
    CQuickStrStream sql;
    sql << "insert into " << kNCDBIndex_Table
        << "(" << kNCDBIndex_FileIdCol      << ","
               << kNCDBIndex_FileNameCol    << ","
               << kNCDBIndex_CreatedTimeCol
        << ")values(?1,?2,?3)";

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, file_id);
    stmt.Bind(2, file_name.data(), file_name.size());
    stmt.Bind(3, int(time(NULL)));
    stmt.Execute();
}

void
CNCDBIndexFile::DeleteDBFile(Uint4 file_id)
{
    CQuickStrStream sql;
    sql << "delete from " << kNCDBIndex_Table
        << " where " << kNCDBIndex_FileIdCol << "=?1";

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, file_id);
    stmt.Execute();
}

void
CNCDBIndexFile::GetAllDBFiles(TNCDBFilesMap* files_map)
{
    CQuickStrStream sql;
    sql << "select " << kNCDBIndex_FileIdCol      << ","
                     << kNCDBIndex_FileNameCol    << ","
                     << kNCDBIndex_CreatedTimeCol
        <<  " from " << kNCDBIndex_Table
        << " order by " << kNCDBIndex_CreatedTimeCol;

    CSQLITE_Statement stmt(this, sql);
    while (stmt.Step()) {
        Uint4          file_id   = stmt.GetInt(0);
        SNCDBFileInfo* info      = new SNCDBFileInfo();
        (*files_map)[file_id]    = info;
        info->file_id = file_id;
        info->file_name   = stmt.GetString(1);
        info->create_time = stmt.GetInt   (2);
    }
}

void
CNCDBIndexFile::DeleteAllDBFiles(void)
{
    CQuickStrStream sql;
    sql << "delete from " << kNCDBIndex_Table;

    CSQLITE_Statement stmt(this, sql);
    stmt.Execute();
}

Uint8
CNCDBIndexFile::GetMaxSyncLogRecNo(void)
{
    CQuickStrStream sql;
    sql << "select " << kNCSettings_ValueCol
        <<  " from " << kNCSettings_Table
        << " where " << kNCSettings_NameCol << "='log_rec_no'";

    CSQLITE_Statement stmt(this, sql);
    if (stmt.Step())
        return Uint8(stmt.GetInt8(0));
    return 0;
}

void
CNCDBIndexFile::SetMaxSyncLogRecNo(Uint8 rec_no)
{
    CQuickStrStream sql;
    sql << "insert or replace into " << kNCSettings_Table
        << " values('log_rec_no',?1)"; 

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, rec_no);
    stmt.Execute();
}

END_NCBI_SCOPE
