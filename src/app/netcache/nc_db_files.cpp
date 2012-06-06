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

#include "nc_pch.hpp"

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>

#include "nc_db_files.hpp"
#include "nc_utils.hpp"



BEGIN_NCBI_SCOPE


#define DBINDEX_TABLENAME   "NCX"
#define DBINDEX_FILEID      "id"
#define DBINDEX_FILENAME    "nm"
#define DBINDEX_CREATEDTIME "tm"

#define SETTINGS_TABLENAME  "NCS"
#define SETTINGS_NAME       "nm"
#define SETTINGS_VALUE      "v"



CNCDBIndexFile::~CNCDBIndexFile(void)
{}

void
CNCDBIndexFile::CreateDatabase(void)
{
    CSQLITE_Statement stmt(this);

    const char* sql;
    sql = "create table if not exists " DBINDEX_TABLENAME
          "(" DBINDEX_FILEID      " integer primary key,"
              DBINDEX_FILENAME    " varchar not null,"
              DBINDEX_CREATEDTIME " int not null"
          ")";
    stmt.SetSql(sql);
    stmt.Execute();

    sql = "create table if not exists " SETTINGS_TABLENAME
          "(" SETTINGS_NAME   " varchar not null,"
              SETTINGS_VALUE  " varchar not null,"
              "unique(" SETTINGS_NAME ")"
          ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

void
CNCDBIndexFile::NewDBFile(Uint4 file_id, const string& file_name)
{
    const char* sql;
    sql = "insert into " DBINDEX_TABLENAME
          "(" DBINDEX_FILEID      ","
              DBINDEX_FILENAME    ","
              DBINDEX_CREATEDTIME
          ")values(?1,?2,?3)";

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, file_id);
    stmt.Bind(2, file_name.data(), file_name.size());
    stmt.Bind(3, CSrvTime::CurSecs());
    stmt.Execute();
}

void
CNCDBIndexFile::DeleteDBFile(Uint4 file_id)
{
    const char* sql;
    sql = "delete from " DBINDEX_TABLENAME
          " where " DBINDEX_FILEID "=?1";

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, file_id);
    stmt.Execute();
}

void
CNCDBIndexFile::GetAllDBFiles(TNCDBFilesMap* files_map)
{
    const char* sql;
    sql = "select " DBINDEX_FILEID      ","
                    DBINDEX_FILENAME    ","
                    DBINDEX_CREATEDTIME
           " from " DBINDEX_TABLENAME
          " order by " DBINDEX_CREATEDTIME;

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
    const char* sql = "delete from " DBINDEX_TABLENAME;
    CSQLITE_Statement stmt(this, sql);
    stmt.Execute();
}

Uint8
CNCDBIndexFile::GetMaxSyncLogRecNo(void)
{
    const char* sql;
    sql = "select " SETTINGS_VALUE
           " from " SETTINGS_TABLENAME
          " where " SETTINGS_NAME "='log_rec_no'";

    CSQLITE_Statement stmt(this, sql);
    if (stmt.Step())
        return Uint8(stmt.GetInt8(0));
    return 0;
}

void
CNCDBIndexFile::SetMaxSyncLogRecNo(Uint8 rec_no)
{
    const char* sql;
    sql = "insert or replace into " SETTINGS_TABLENAME
          " values('log_rec_no',?1)"; 

    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, rec_no);
    stmt.Execute();
}

END_NCBI_SCOPE
