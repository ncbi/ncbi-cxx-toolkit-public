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
 * File Description: Demo3 application for NCBI Berkeley DB library (BDB).
 *                   Shows how to create additional BTree index file to
 *                   improve search performance.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <db/bdb/bdb_cursor.hpp>
#include <assert.h>
#include <list>

USING_NCBI_SCOPE;

//////////////////////////////////////////////////////////////////
//
// Structure implements database table record in "phone book"
// database table.
//
//
struct SPhoneBookDB : public CBDB_File
{
    CBDB_FieldInt4      person_id;

    CBDB_FieldString    first_name;
    CBDB_FieldString    last_name;
    CBDB_FieldInt4      zip_code;
    CBDB_FieldString    phone;

    // Constructor binds field variables.
    SPhoneBookDB()
    {
        // Bind int primary key. Cannot be NULL.
        BindKey("PersonId", &person_id);

        //  Bind non-PK data fields
        BindData("FirstName", &first_name, 64, eNotNullable);
        BindData("LastName",  &last_name, 64, eNotNullable);
        BindData("Zip",       &zip_code);  // NULLable by default
        BindData("Phone",     &phone, 32); // NULLable by default
    }
};

const char* s_PhoneBookDBFileName = "phone.db";


//////////////////////////////////////////////////////////////////
//
// Structure implements index record for "phone book"
// database table. BTree index stores Zip code and correspondent
// primary key in the main table.
//

struct SPhoneBookZipIDX : public CBDB_File
{
    CBDB_FieldInt4      zip_code;
    CBDB_FieldInt4      person_id;

    SPhoneBookZipIDX()
    {
        BindKey("Zip", &zip_code);
        BindKey("PersonId", &person_id);
    }
};

const char* s_PhoneBookZipIDXFileName = "phone_zip.idx";


void LoadPhoneBook()
{
    SPhoneBookDB  dbf;

    dbf.Open(s_PhoneBookDBFileName, CBDB_File::eCreate);

    dbf.person_id = 1;
    dbf.first_name = "John";
    dbf.last_name = "Doe";
    dbf.zip_code = 10785;
    dbf.phone = "+1(240)123456";

    EBDB_ErrCode ret = dbf.Insert();
    // Check Insert result, it can be 'eBDB_KeyDup'
    // if primary key already exists
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }

    dbf.person_id = 2;
    dbf.first_name = "Antony";
    dbf.last_name = "Smith";
    dbf.zip_code = 10785;
    dbf.phone = "(240)234567";

    ret = dbf.Insert();
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }

    dbf.person_id = 3;
    dbf.first_name = "John";
    dbf.last_name = "Public";
    dbf.zip_code = 40000;

    ret = dbf.Insert();
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }
}

//
// Function builds Zip code index.
// Indexing procedure creates a cursor on the main data file, fetches all data
// and adds corresponding entries to the index. From the technical standpoint
// both data and index are both BTree Berkeley DB files.
//
void BuildZipIndex(SPhoneBookDB& dbf)
{
    SPhoneBookZipIDX idx;
    idx.Open(s_PhoneBookZipIDXFileName, CBDB_File::eCreate);

    CBDB_FileCursor cur(dbf);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        if (dbf.zip_code.IsNull())  // NULL values are dropped
            continue;

        idx.zip_code = dbf.zip_code;
        idx.person_id = dbf.person_id;

        EBDB_ErrCode ret = idx.Insert();
        if (ret != eBDB_Ok) {
            cout << "Insert failed!" << endl;
            exit(1);
        }
    }

}


// Function prints the database record.
void PrintRecord(const SPhoneBookDB& rec)
{
    cout << rec.person_id.GetName() << ": " << rec.person_id.Get() << endl;

    cout << rec.first_name.GetName()<< ": ";
    if (rec.first_name.IsNull()) {
        cout << "NULL" << endl;
    } else {
        cout << (const char*) rec.first_name << endl;
    }

    cout << rec.last_name.GetName()<< ": ";
    if (rec.last_name.IsNull()) {
        cout << "NULL" << endl;
    } else {
        cout << (const char*) rec.last_name << endl;
    }

    cout << rec.zip_code.GetName()<< ": ";
    if (rec.zip_code.IsNull()) {
        cout << "NULL" << endl;
    } else {
        cout << (Int4) rec.zip_code << endl;
    }

    cout << rec.phone.GetName()<< ": ";
    if (rec.phone.IsNull()) {
        cout << "NULL" << endl;
    } else {
        cout << (const char*) rec.phone << endl;
    }
}


//
// Search function uses Zip code index for fast data retrieval.
// Actual data seach is two phase process.
// First we seach the index values and collect the parent table primary key.
// Then we go and get the primary data table records.
//
void SearchPhoneBook(SPhoneBookDB& dbf, SPhoneBookZipIDX& idx, int zip_code)
{
    list<int>  person_id;

    // Index search. Here we store seach values in the list.
    {{
        CBDB_FileCursor cur(idx);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << zip_code;

        while (cur.Fetch() == eBDB_Ok) {
            person_id.push_back(idx.person_id.Get());
        }

    }}

    if (person_id.empty()) {
        cout << "No records found." << endl;
        return;
    }

    // Data retrieval using index seach values.
    for (list<int>::const_iterator it = person_id.begin();
         it != person_id.end(); ++it) {

        dbf.person_id = *it;
        EBDB_ErrCode ret = dbf.Fetch();
        if (ret != eBDB_Ok) {
            assert(0);
        }

        cout << "====================================" << endl;
        PrintRecord(dbf);
    }
}


////////////////////////////////
// Demo3 application
//

class CBDB_PhoneBookDemo3 : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_PhoneBookDemo3::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("bdb demo3",
                       "Demo3 application for BDB library");
    SetupArgDescriptions(d.release());
}

int CBDB_PhoneBookDemo3::Run(void)
{
    try
    {
        LoadPhoneBook();

        SPhoneBookDB dbf;
        dbf.Open(s_PhoneBookDBFileName, CBDB_File::eReadOnly);

        BuildZipIndex(dbf);

        SPhoneBookZipIDX idx;
        idx.Open(s_PhoneBookZipIDXFileName, CBDB_File::eReadOnly);

        SearchPhoneBook(dbf, idx, 10785);
    }
    catch (CBDB_ErrnoException& ex)
    {
        cout << "Error! DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        cout << "Error! DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CBDB_PhoneBookDemo3().AppMain(argc, argv);
}
