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
 * File Description: Demo2 application for NCBI Berkeley DB library (BDB).
 *                   Application shows how to fetch a single known record from
 *                   the datafile. Also demonstrated how to delete records and handle
 *                   record not found situations.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <db/bdb/bdb_types.hpp>
#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_cursor.hpp>

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
    dbf.phone = "(240)234567";

    ret = dbf.Insert();
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }

    dbf.person_id = 3;
    dbf.first_name = "John";
    dbf.last_name = "Public";

    ret = dbf.Insert();
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }
}

//
// Delete record function.
// NOTE: When deleting record Berkeley DB does not free the space in the datafile.
// File does not shrink in size. Empty block will be reused by the next insert.
//
void DeletePhoneBookRecord(SPhoneBookDB& dbf, int person_id)
{
    dbf.person_id = person_id;
    dbf.Delete();
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
// Function seeks the requested datafile record and prints it.
// In case of only one record to fetch we do not need to explicitly create
// a cursor. Instead we can position datafile class on the requested record
// using method "Fetch". If record cannot be found 'Fetch' returns eBDB_NotFound
// return code.
//

void PrintPhoneBookRecord(SPhoneBookDB& dbf, int person_id)
{

    dbf.person_id = person_id;
    EBDB_ErrCode ret = dbf.Fetch();

    if (ret == eBDB_NotFound) {
        cout << "Record " << person_id << " cannot be found." << endl;
        return;
    }

    PrintRecord(dbf);
}


////////////////////////////////
// Demo2 application
//

class CBDB_PhoneBookDemo2 : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_PhoneBookDemo2::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("bdb demo2",
                       "Demo2 application for BDB library");
    SetupArgDescriptions(d.release());
}

int CBDB_PhoneBookDemo2::Run(void)
{
    try
    {
        LoadPhoneBook();

        SPhoneBookDB  dbf;
        dbf.Open(s_PhoneBookDBFileName, CBDB_File::eReadWrite);

        PrintPhoneBookRecord(dbf, 3);

        cout << "====================================" << endl;
        cout << "Deleting record 3." << endl;

        DeletePhoneBookRecord(dbf, 3);

        PrintPhoneBookRecord(dbf, 3);
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
    return CBDB_PhoneBookDemo2().AppMain(argc, argv);
}
