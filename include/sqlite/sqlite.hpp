#ifndef SQLITE___SQLITE__HPP
#define SQLITE___SQLITE__HPP

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


#include <corelib/ncbiobj.hpp>

// predeclaration saves us from polluting the global namespace with
// all of sqlite
struct sqlite;
struct sqlite_vm;


BEGIN_NCBI_SCOPE


//
// struct to handle SQLite's callback mechanism
//
struct ISQLiteCallback
{
    virtual ~ISQLiteCallback() {}
    virtual int Callback(int argc, char** argv, char** cols) = 0;
};


class CSQLiteQuery;

//
// main SQLite interface class
//
class NCBI_XSQLITE_EXPORT CSQLite : public CObject
{
public:

    // create a new SQLite interface class around a file.  This file will
    // optionally be deleted on destruction (i.e., temporary out-of-core
    // database)
    NCBI_DEPRECATED CSQLite(const string& dbname, bool delete_file = false);

    // create a new SQLite interface class for an in-memory database
    NCBI_DEPRECATED CSQLite();

    virtual ~CSQLite();

    // Open a database.  Databases with the name ':memory:' are opened as
    // in-core databases.  The delete flag is optionally used to specify
    // that a call to Close() will destroy the file as well.
    void Open(const string& dbname, bool delete_file = false);

    // close the current database
    void Close();

    // compile a query for execution
    CSQLiteQuery* Compile(const string& query) const;

    // execute our commands, no callback
    void Execute(const string& str);

    // execute our commands, but ping a callback in the referenced class
    void Execute(const string& str, ISQLiteCallback& cb);

    // get the row index / key for the last insert statement.  This returns -1
    // if the database is invalid
    int GetLastRowID(void);

    // returns the number of rows affected by the last execute statement
    int GetRowsAffected(void);

    // interrupt the current execution
    void StopExecute(void);

    // test to see if a file is a SQLite database
    static bool IsValidDB(const string& str);

private:
    // abstract DB pointer
    sqlite* m_DB;

    // name of the file we use
    string m_Filename;

    // flag: should we delete the file on destruction?
    bool m_DeleteFile;

    // forbidden
    CSQLite(const CSQLite&);
    CSQLite& operator=(const CSQLite&);
};



///////////////////////////////////////////////////////////////////////////
//
// class CSQLiteQuery is a simple interface to a query object

class NCBI_XSQLITE_EXPORT CSQLiteQuery : public CObject
{
    friend class CSQLite;

public:

    ~CSQLiteQuery();

    // retrieve the next row from our compiled query, returning false if there
    // isn't one
    bool NextRow(int& count, const char **& data, const char **& cols) const;

protected:
    sqlite* m_DB;
    mutable sqlite_vm* m_Query;

    // ctor protected - created only by CSQLite
    CSQLiteQuery(sqlite_vm* query);

    // clean-up
    void x_CleanUp() const;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/12/04 13:47:51  dicuccio
 * Deprecate sqlite
 *
 * Revision 1.1  2003/09/29 12:24:40  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // SQLITE___SQLITE__HPP
