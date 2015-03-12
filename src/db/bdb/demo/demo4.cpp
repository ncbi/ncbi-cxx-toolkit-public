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
 * File Description: Demo4 application for NCBI Berkeley DB library (BDB).
 *                   BLOB storage demonstration.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <assert.h>

#include <db/bdb/bdb_blob.hpp>

USING_NCBI_SCOPE;


string s_LobDBFileName  = "blobstore.db";
string s_LobDBFileName2 = "blobstore2.db";


const int array1[] = {1, 2, 3, 4, 5, 0};
const int array2[] = {10, 20, 30, 40, 50, 0};

////////////////////////////////////////////////////////////////
// Demo case 1:
// Demonstrates how to use CBDB_LobFile
//   (BLOBs are accessed using integer access key)

//
// Load LOB storage

void LoadBLOB_Table()
{
    CBDB_LobFile lob;
    lob.Open(s_LobDBFileName, CBDB_File::eCreate);


    EBDB_ErrCode ret = lob.Insert(1, array1, sizeof(array1));
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }
    ret = lob.Insert(2, array2, sizeof(array2));
    if (ret != eBDB_Ok) {
        cout << "Insert failed!" << endl;
        exit(1);
    }
}



void PrintBLOB_Table()
{
    int  buffer[256];

    CBDB_LobFile lob;
    lob.Open(s_LobDBFileName, CBDB_File::eReadOnly);

    // The safest way to read a LOB is first to retrieve the correspondent
    // record and use LobSize() to get information about the BLOB size.
    // When the size is known we can allocate memory and fetch the LOB
    // data using GetData() function.


    EBDB_ErrCode ret = lob.Fetch(1);

    if (ret == eBDB_Ok) {
        unsigned size = lob.LobSize();
        if (size > sizeof(buffer)) {
            cout << "Insufficient buffer size!" << endl;
        }
        else {
            ret = lob.GetData(buffer, sizeof(buffer));
            assert(ret == eBDB_Ok);
            for (unsigned int i = 0; i < size / sizeof(buffer[0]); ++i) {
                cout << buffer[i] << " ";
            }
            cout << endl;
        }
    } else {
        cout << "BLOB 1 not found" << endl;
    }


    // In case we know the maximum possible BLOB size we can implement
    // one phase fetch. It should work faster in many cases.

    void* ptr = (void*)buffer;
    ret = lob.Fetch(2,
                    &ptr,
                    sizeof(buffer),
                    CBDB_LobFile::eReallocForbidden);

    if (ret == eBDB_Ok) {
        unsigned size = lob.LobSize();
        for (unsigned int i = 0; i < size / sizeof(buffer[0]); ++i) {
            cout << buffer[i] << " ";
        }
        cout << endl;
    } else {
        cout << "BLOB 2 not found" << endl;
    }

}

////////////////////////////////////////////////////////////////
// Demo case 2:
// Demonstrates how to use custom BLOB storage
// (derived from CBDB_BLobFile)


// @internal
struct SDemoDB : public CBDB_BLobFile
{
    CBDB_FieldString       key;
    CBDB_FieldString       subkey;

    SDemoDB()
    {
        BindKey("key",     &key, 256);
        BindKey("subkey",  &subkey, 256);
    }
};


void LoadDemoDB()
{
    SDemoDB lob;
    lob.Open(s_LobDBFileName2, CBDB_File::eCreate);


    lob.key = "Key1";
    lob.subkey = "SubKey1";

    EBDB_ErrCode ret = lob.Insert(array1, sizeof(array1));
    if (ret != eBDB_Ok) {
        NcbiCout << "Insert failed!" << endl;
        exit(1);
    }

    lob.key = "Key2";
    lob.subkey = "SubKey2";

    ret = lob.Insert(array2, sizeof(array2));
    if (ret != eBDB_Ok) {
        NcbiCerr << "Insert failed!" << endl;
        exit(1);
    }
}

void PrintDemoDB()
{
    int  buffer[256];

    SDemoDB lob;
    lob.Open(s_LobDBFileName2, CBDB_File::eReadOnly);

    lob.key = "Key1";
    lob.subkey = "SubKey1";

    EBDB_ErrCode ret = lob.Fetch();

    if (ret == eBDB_Ok) {
        unsigned size = lob.LobSize();
        if (size > sizeof(buffer)) {
            NcbiCout << "Insufficient buffer size!" << NcbiEndl;
        }
        else {
            ret = lob.GetData(buffer, sizeof(buffer));
            assert(ret == eBDB_Ok);
            for (unsigned int i = 0; i < size / sizeof(buffer[0]); ++i) {
                NcbiCout << buffer[i] << " ";
            }
            NcbiCout << NcbiEndl;
        }
    } else {
        NcbiCout << "BLOB 1 not found" << NcbiEndl;
    }

}



/// BLob Demo application
///
/// @internal
///
class CBDB_BLobDemo1 : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CBDB_BLobDemo1::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("bdb demo1",
                       "Demo1 application for BDB library");
    SetupArgDescriptions(d.release());
}

int CBDB_BLobDemo1::Run(void)
{
    try
    {
        LoadBLOB_Table();
        PrintBLOB_Table();

        LoadDemoDB();
        PrintDemoDB();
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
    return CBDB_BLobDemo1().AppMain(argc, argv);
}
