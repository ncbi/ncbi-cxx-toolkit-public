/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is sqlite_freememly available
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
#include <sqlite/sqlite.hpp>
#include <sqlite/sqlite_exception.hpp>
#include <corelib/ncbifile.hpp>
#include <sqlite.h>


BEGIN_NCBI_SCOPE



CSQLite::CSQLite()
    : m_DB(NULL)
    , m_DeleteFile(false)
{
    Open(":memory:");
}


CSQLite::CSQLite(const string& dbname, bool delete_file)
    : m_DB(NULL)
    , m_DeleteFile(false)
{
    Open(dbname, delete_file);
}


CSQLite::~CSQLite()
{
    Close();
}


void CSQLite::Open(const string& dbname, bool delete_file)
{
    Close();

    char* msg = NULL;
    m_DB = sqlite_open(dbname.c_str(), 0, &msg);
    if ( !m_DB ) {
        string str("error opening database ");
        str += dbname;
        str += ": ";
        str += msg;
        sqlite_freemem(msg);
        NCBI_THROW(CSqliteException, eOpen, str);
    }

    m_Filename = dbname;
    m_DeleteFile = delete_file;
    if (dbname == ":memory:") {
        m_DeleteFile = false;
    }
}


void CSQLite::Close()
{
    if (m_DB) {
        sqlite_close(m_DB);
    }
    m_DB = NULL;

    if (m_DeleteFile) {
        CFile file(m_Filename);
        if ( !file.Remove() ) {
            LOG_POST(Error << "CSQLite::Close(): Cannot delete file "
                     << m_Filename);
        }

        m_DeleteFile = false;
        m_Filename.erase();
    }
}


bool CSQLite::IsValidDB(const string& str)
{
    CFile file(str);
    if ( !file.Exists() ) {
        return false;
    }

    try {
        CRef<CSQLite> sqlite(new CSQLite(str));
        CRef<CSQLiteQuery> q(sqlite->Compile("select * from sqlite_master"));

        int count;
        const char** data;
        const char** cols;
        return q->NextRow(count, data, cols);
    }
    catch (...) {
    }

    return false;
}



struct SNullCallback : public ISQLiteCallback
{
    int Callback(int, char**, char**)
    {
        return 1;
    }
};


static int s_CallbackHandler(void* data,
                             int argc, char** argv, char** cols)
{
    ISQLiteCallback* ptr = static_cast<ISQLiteCallback*>(data);
    if (ptr) {
        return ptr->Callback(argc, argv, cols);
    }
    return 1;
}


void CSQLite::Execute(const string& cmd)
{
    SNullCallback cb;
    Execute(cmd, cb);
}


CSQLiteQuery* CSQLite::Compile(const string& sql) const
{
    //_TRACE("compiling: " << sql);
    sqlite_vm* vm = NULL;
    const char* tail = NULL;
    char* msg = NULL;
    if (SQLITE_OK != sqlite_compile(m_DB, sql.c_str(), &tail, &vm, &msg)) {
        string str("Error: can't compile query: >>");
        str += sql + "<<: ";
        str += msg;
        sqlite_freemem(msg);
        NCBI_THROW(CSqliteException, eExecute, str);
    }

    return new CSQLiteQuery(vm);
}



void CSQLite::Execute(const string& cmd, ISQLiteCallback& cb)
{
    //_TRACE("executing: " << cmd);
    if ( !m_DB ) {
        NCBI_THROW(CSqliteException, eExecute,
                   "Error: database not open");
    }

    char* msg = NULL;
    int rc = sqlite_exec(m_DB, cmd.c_str(), s_CallbackHandler, &cb, &msg);
    if ( rc != SQLITE_OK ) {
        string str("Error executing command >>");
        str += cmd;
        str += "<<: ";
        str += msg;
        sqlite_freemem(msg);
        NCBI_THROW(CSqliteException, eExecute, str);
    }
}


int CSQLite::GetLastRowID(void)
{
    if ( !m_DB ) {
        return -1;
    }

    return sqlite_last_insert_rowid(m_DB);
}


int CSQLite::GetRowsAffected(void)
{
    if ( !m_DB ) {
        return -1;
    }
    return sqlite_changes(m_DB);
}


void CSQLite::StopExecute(void)
{
    if ( !m_DB ) {
        return;
    }
    sqlite_interrupt(m_DB);
}


//////////////////////////////////////////////////////////////
//
// query interface
//


CSQLiteQuery::CSQLiteQuery(sqlite_vm* query)
    : m_Query(query)
{
}


CSQLiteQuery::~CSQLiteQuery()
{
    x_CleanUp();
}


bool CSQLiteQuery::NextRow(int& count,
                           const char**& data,
                           const char**& cols) const
{
    if ( !m_Query ) {
        return false;
    }

    switch (sqlite_step(m_Query, &count, &data, &cols)) {
    case SQLITE_DONE:
        x_CleanUp();
        return false;

    case SQLITE_ROW:
        return true;

    case SQLITE_BUSY:
        return false;

    default:
        x_CleanUp();
        return false;
    }
}


void CSQLiteQuery::x_CleanUp() const
{
    if ( !m_Query ) {
        return;
    }

    char* msg = NULL;
    if (SQLITE_OK != sqlite_finalize(m_Query, &msg) ) {
        LOG_POST(Error << "failed to clean up query: " << msg);
        sqlite_freemem(msg);
    }
    m_Query = NULL;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/17 21:05:24  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/09/29 12:24:17  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
