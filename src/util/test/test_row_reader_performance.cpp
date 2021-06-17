/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author: Sergey Satskiy
 *
 * File Description:
 *   Test program to collect performance data of the row reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/row_reader_char_delimited.hpp>
#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



class CRowReaderPerfTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
    void Read(const string& fname);
};


void CRowReaderPerfTest::Init(void)
{
    SetDiagPostLevel(eDiag_Error);

    unique_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->AddDefaultKey("f", "file",
                     "file to generate test data into",
                     CArgDescriptions::eString,
                     "test_row_reader_performance.dat");
    d->AddDefaultKey("r", "rows",
                     "number of rows generated",
                     CArgDescriptions::eInteger, "1000000");
    d->AddDefaultKey("i", "items",
                     "data items in each row",
                     CArgDescriptions::eInteger, "20");
    SetupArgDescriptions(d.release());
}


int CRowReaderPerfTest::Run(void)
{
    const CArgs& args = GetArgs();

    Int8    row_count = args["r"].AsInteger();
    Int8    item_count = args["i"].AsInteger();
    string  fname = args["f"].AsString();

    NcbiCout << "Number of rows: " << row_count << NcbiEndl
             << "Number of items in each row: " << item_count << NcbiEndl
             << "Data file: " << fname << NcbiEndl;

    // Generate the data file
    FILE *  f = fopen(fname.c_str(), "w");
    if (f == NULL) {
        NcbiCout << "Could not open data file " << fname << NcbiEndl;
        return 1;
    }

    for (Int8 row = 0; row < row_count; ++row) {
        if (row != 0)
            fputc('\n', f);
        for (Int8 item = 0; item < item_count; ++item) {
            if (item != 0)
                fputc(' ', f);
            fprintf(f, "%ld", (long)(item));
        }
    }
    fclose(f);

    Read(fname);

    remove(fname.c_str());
    return 0;
}

void CRowReaderPerfTest::Read(const string& fname)
{
    Int8    read_row_count = 0;
    Int8    read_field_count = 0;
    typedef CRowReader<TRowReaderStream_SingleSpaceDelimited>
                                                        TSpaceDelimitedStream;
    TSpaceDelimitedStream   src_stream(fname);
    for (auto &  row : src_stream) {
        ++read_row_count;

        auto field_count = row.GetNumberOfFields();
        for (TFieldNo  fno = 0; fno < field_count; ++fno) {
            ++read_field_count;

            if (row[fno].Get<int>() != static_cast<int>(fno))
                NcbiCerr << "Error reading integer field number " << fno
                         << " Read value: " << row[fno].Get<int>() << NcbiEndl;
            if (row[fno].Get<CTempString>().length() > 1000000)
                NcbiCerr << "Error reading field number " << fno
                         << " as CTempString." << NcbiEndl;
        }
    }

    NcbiCout << "Number of rows read: " << read_row_count << NcbiEndl
             << "Number of fields read: " << read_field_count << NcbiEndl;
}


int main(int argc, const char* argv[])
{
    CRowReaderPerfTest app;
    return app.AppMain(argc, argv);
}
