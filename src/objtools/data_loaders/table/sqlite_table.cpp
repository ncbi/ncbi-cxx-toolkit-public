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
#include <objtools/data_loaders/table/sqlite_table.hpp>
#include <sqlite/sqlite.hpp>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//
// class CSQliteTableIterator is private - no external class can refer to
// this class directly
//
class CSQLiteTableIterator : public ITableIterator
{
public:
    CSQLiteTableIterator(CSQLiteQuery& q)
        : m_Query(&q)
    {
        Next();
    }

    // retrieve the current row's data
    void GetRow(list<string>& data) const
    {
        data = m_ThisRow;
    }

    // advance to the next row
    void Next(void)
    {
        int count = 0;
        const char** data = NULL;
        const char** cols = NULL;
        if ( !m_Query->NextRow(count, data, cols) ) {
            m_Query.Reset();
        }

        m_ThisRow.clear();
        if (data  &&  count) {
            // remember that our data contains the row_idx
            // as the first entry!
            for (int i = 1;  i < count;  ++i) {
                m_ThisRow.push_back(data[i]);
            }
        }
    }

    // check to see if the current iterator is valid
    bool IsValid(void) const
    {
        return m_Query;
    }

private:
    CRef<CSQLiteQuery> m_Query;

    list<string> m_ThisRow;
};




CSQLiteTable::CSQLiteTable(const string& input_file,
                           const string& temp_file,
                           bool delete_file)
    : m_NumRows(0),
      m_DeleteFile(delete_file),
      m_FileName(input_file),
      m_TmpFileName(temp_file)
{
    //
    // first, see if our temporary file is already a SQLite DB
    //
    CFile file(temp_file);
    if (file.Exists()) {
        try {
            m_Sqlite.Reset(new CSQLite(temp_file, false));

            //
            // retrieve our columns
            //
            CRef<CSQLiteQuery> q
                (m_Sqlite->Compile("select col, name from TableCols"));

            int count;
            const char** data = NULL;
            const char** cols = NULL;
            while (q.GetPointer()  &&  q->NextRow(count, data, cols)) {
                string col(data[0]);
                string str(data[1]);

                string::size_type pos = 0;
                while ( (pos = str.find_first_of("'", pos)) != string::npos) {
                    if (pos  &&  str[pos-1] == '\\') {
                        str.erase(pos - 1, 1);
                        --pos;
                    }
                    ++pos;
                }
                m_Cols.push_back(str);
            }

            return;
        }
        catch (...) {
            // exception thrown on trying to access the data
            // the file isn't valid
            x_CleanUp();
        }
    }

    //
    // oops, not so - must create anew...
    //
    try {
        _TRACE("CSQLiteTable: input = " << input_file
               << " db = " << temp_file);
        string line;
        vector<string> toks;

        m_Sqlite.Reset(new CSQLite(temp_file, delete_file));

        //
        // first, parse the header line
        // this contains the column descriptions
        //
        CNcbiIfstream istr(input_file.c_str());
        int cols = 0;
        while (NcbiGetlineEOL(istr, line)) {
            if (line.empty()  ||  line[0] == '#') {
                continue;
            }

            NStr::Tokenize(line, "\t", m_Cols);
            if (m_Cols.size() == 0) {
                continue;
            }

            //
            // first readable line is column headers
            // we create two tables - the column header meta-table and
            // the main data table
            //
            // create our schema
            //

            m_Sqlite->Execute("create table TableCols ("
                              "col VARCHAR(32) NOT NULL, "
                              "name VARCHAR(255) NOT NULL, "
                              "desc VARCHAR(255) NULL)");

            int col_idx = 0;
            string schema;
            ITERATE(vector<string>, iter, m_Cols) {
                string str(*iter);
                NStr::ToLower(str);

                string::size_type pos = 0;
                while ( (pos = str.find_first_of("'", pos)) != string::npos) {
                    str.insert(pos, "\\");
                    pos += 2;
                }

                m_Sqlite->Execute("insert into TableCols (col, name) "
                                  "values ('col" +
                                  NStr::IntToString(col_idx) + "', '" +
                                  str + "')");

                if ( !schema.empty() ) {
                    schema += ", ";
                }
                schema += "col" + NStr::IntToString(col_idx);
                ++col_idx;
            }

            schema = "create table TableData (\n"
                     "row_idx INTEGER PRIMARY KEY,\n" + schema + ");";
            m_Sqlite->Execute(schema);
            cols = m_Cols.size();
            break;
        }

        //
        // now, process the actual data
        //
        m_Sqlite->Execute("begin");

        int row_idx = 0;
        while (NcbiGetlineEOL(istr, line)) {
            if (line.empty()  ||  line[0] == '#') {
                continue;
            }

            toks.clear();
            NStr::Tokenize(line, "\t", toks);
            if (toks.size() == 0) {
                continue;
            }

            //
            // parse a row
            //
            string schema(NStr::IntToString(row_idx++));
            ITERATE(vector<string>, iter, toks) {
                string str(*iter);
                string::size_type pos = 0;
                while ( (pos = str.find_first_of("'")) != string::npos) {
                    // escape single quotes
                    str.insert(pos, "\\");
                    pos += 2;
                }
                if ( !schema.empty() ) {
                    schema += ",\n";
                }
                schema += "'" + *iter + "'";
            }
            for (int i = toks.size(); i < cols;  ++i) {
                if ( !schema.empty() ) {
                    schema += ",\n";
                }
                schema += "''";
            }

            schema = "insert into TableData values (\n" + schema + ");";
            m_Sqlite->Execute(schema);
        }

        m_Sqlite->Execute("end");

        CRef<CSQLiteQuery> q
            (m_Sqlite->Compile("select count(*) from TableData;"));
        int count = 0;
        const char** data = NULL;
        const char** col_names = NULL;
        if (q->NextRow(count, data, col_names)) {
            m_NumRows = NStr::StringToInt(data[0]);
            LOG_POST(Info << "CSQLiteTable: inserted "
                     << data[0] << " items into local db");
        } else {
            LOG_POST(Info << "compiled query failed");
        }
    }
    catch (...) {
        x_CleanUp();
        throw;
    }
}


CSQLiteTable::~CSQLiteTable()
{
    x_CleanUp();
}


void CSQLiteTable::x_CleanUp()
{
    m_Sqlite.Reset();

    // now, we can (potentially) delete our file
    if (m_DeleteFile) {
        CFile file(m_TmpFileName);
        file.Remove();
    }
}


CSQLite& CSQLiteTable::SetDB(void)
{
    return *m_Sqlite;
}


int CSQLiteTable::GetNumRows(void) const
{
    return m_NumRows;
}


int CSQLiteTable::GetNumCols(void) const
{
    return m_Cols.size();
}


void CSQLiteTable::GetRow(int row, list<string>& data) const
{
    _ASSERT(row >= 0  &&  row < m_NumRows);
    CRef<CSQLiteQuery> q
        (m_Sqlite->Compile("select * from TableData where row_idx = "
                           + NStr::IntToString(row)));

    data.clear();
    if ( !q ) {
        return;
    }

    int count = 0;
    const char** row_data = NULL;
    const char** cols = NULL;
    q->NextRow(count, row_data, cols);
    if ( !row_data ) {
        return;
    }

    for (int i = 1;  i < count;  ++i) {
        data.push_back(row_data[i]);
    }
}


void CSQLiteTable::GetColumnTitle(int col, string& title) const
{
    _ASSERT(col >= 0  && col < m_Cols.size());
    CRef<CSQLiteQuery> q
        (m_Sqlite->Compile("select name from TableCols where col = 'col"
                           + NStr::IntToString(col) + "'"));

    title.erase();
    if ( !q ) {
        return;
    }

    int count = 0;
    const char** row_data = NULL;
    const char** cols = NULL;
    q->NextRow(count, row_data, cols);
    if ( !row_data ) {
        return;
    }
    title = row_data[0];
}


void CSQLiteTable::GetColumnTitles(list<string>& titles) const
{
    CRef<CSQLiteQuery> q
        (m_Sqlite->Compile("select name from TableCols "
                           "where col like 'col%'"));

    titles.clear();
    if ( !q ) {
        return;
    }

    int count = 0;
    const char** row_data = NULL;
    const char** cols = NULL;
    while (q->NextRow(count, row_data, cols)) {
        titles.push_back(row_data[0]);
    }
}


// access an iterator for the entire table
CSQLiteTable::TIterator CSQLiteTable::Begin(void)
{
    return Begin("select * from TableData");
}


// obtain an iterator valid for a given ID
CSQLiteTable::TIterator CSQLiteTable::Begin(const CSeq_id& id)
{
    return Begin();
}


// obtain an iterator valid for a given location
CSQLiteTable::TIterator CSQLiteTable::Begin(const CSeq_loc& id)
{
    return Begin();
}


// obtain an iterator valid for a given SQL query
CSQLiteTable::TIterator CSQLiteTable::Begin(const string& query)
{
    CRef<CSQLiteQuery> q(m_Sqlite->Compile(query));
    if ( !q ) {
        return TIterator();
    }
    return TIterator(new CSQLiteTableIterator(*q));
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/21 21:42:53  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/10/02 18:21:20  dicuccio
 * Added Unix projects.  Compilation fixes for gcc on Linux
 *
 * Revision 1.1  2003/10/02 17:50:48  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
