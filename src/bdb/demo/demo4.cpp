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

#include <bdb/bdb_blob.hpp>

USING_NCBI_SCOPE;


const char* s_LobDBFileName = "blobstore.db";


const int array1[] = {1, 2, 3, 4, 5, 0};
const int array2[] = {10, 20, 30, 40, 50, 0};



void LoadBLOB_Table()
{
    CBDB_LobFile lob;
    lob.Open(s_LobDBFileName, CBDB_LobFile::eCreate);


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
    lob.Open(s_LobDBFileName, CBDB_LobFile::eReadOnly);

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


////////////////////////////////
// BLob Demo application
//

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
    return CBDB_BLobDemo1().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/06/22 12:04:32  kuznets
 * Use single table in file variant of Open
 *
 * Revision 1.4  2004/05/17 20:55:18  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/05/27 18:07:12  kuznets
 * Fixed compilation warnings
 *
 * Revision 1.2  2003/05/02 16:23:14  kuznets
 * Cosmetic fixes
 *
 * Revision 1.1  2003/05/01 19:51:46  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
