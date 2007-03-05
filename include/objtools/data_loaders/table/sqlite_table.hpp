#ifndef OBJTOOLS_DATA_LOADERS___SQLITE_TABLE__HPP
#define OBJTOOLS_DATA_LOADERS___SQLITE_TABLE__HPP

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

#include <objtools/data_loaders/table/table.hpp>


BEGIN_NCBI_SCOPE


class CSQLite;


//
// class CSQLiteTable implements the core functionality for reading a
// demilited file as a SQLite database
//
class NCBI_XLOADER_TABLE_EXPORT CSQLiteTable : public ITableData
{
public:

    CSQLiteTable(const string& input_file,
                     const string& temp_file,
                     bool delete_file = true);

    virtual ~CSQLiteTable();

    // retrieve the number of rows in the table
    int GetNumRows(void) const;

    // retrieve the number of columns in the table
    int GetNumCols(void) const;

    // retrieve a single, numbered row
    virtual void GetRow        (int row, list<string>& data) const;

    // retrieve the title for a given column
    virtual void GetColumnTitle(int col, string& title) const;

    // retrieve a list of the columns in the table
    virtual void GetColumnTitles(list<string>& titles) const;

    // access an iterator for the entire table
    virtual TIterator Begin(void);

    // obtain an iterator valid for a given ID
    virtual TIterator Begin(const objects::CSeq_id& id);

    // obtain an iterator valid for a given location
    virtual TIterator Begin(const objects::CSeq_loc& id);

    // obtain an iterator valid for a given SQL query
    virtual TIterator Begin(const string& query);

    // retrieve the SQLite object behind the database
    CSQLite& SetDB(void);

protected:
    // the SQLite db we manage
    CRef<CSQLite> m_Sqlite;

    // the list of columns in the SQLite DB.
    vector<string> m_Cols;

    // counter for the number of rows in the database
    int m_NumRows;

    // flag: should the SQLite DB be deleted on exit?
    bool m_DeleteFile;

    // the name of the file we parsed
    string m_FileName;

    // the name of the temporary file we've created
    string m_TmpFileName;

private:
    void x_CleanUp();

    // prohibit copying!
    CSQLiteTable(const CSQLiteTable&);
    CSQLiteTable& operator=(const CSQLiteTable&);
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS___SQLITE_TABLE__HPP
