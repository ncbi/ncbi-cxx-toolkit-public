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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *   Test program for file's accessory functions
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/09/19 13:08:15  ivanov
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/files.hpp>
#include <stdio.h>


BEGIN_NCBI_SCOPE

END_NCBI_SCOPE


USING_NCBI_SCOPE;


/////////////////////////////////
// File name spliting tests
//

static void s_TEST_SplitPath(void)
{
    string path, dir, title, ext;
    
#if defined NCBI_OS_MSWIN
    CFile::SplitPath("c:\\windows\\command\\win.ini", &dir, &title, &ext);
    _ASSERT( dir   == "c:\\windows\\command\\" );
    _ASSERT( title == "win" );
    _ASSERT( ext   == ".ini" );

    path = CFile::MakePath("c:\\windows\\", "win.ini");
    _ASSERT( path == "c:\\windows\\win.ini" );

    CFile f("c:\\dir\\subdir\\file.ext");
    _ASSERT( f.GetDir()  == "c:\\dir\\subdir\\" );
    _ASSERT( f.GetName() == "file.ext" );
    _ASSERT( f.GetBase() == "file" );
    _ASSERT( f.GetExt()  == ".ext" );

#elif defined NCBI_OS_UNIX

    CFile::SplitPath("/usr/lib/any.other.lib", &dir, &title, &ext);
    _ASSERT( dir   == "/usr/lib/" );
    _ASSERT( title == "any.other" );
    _ASSERT( ext   == ".lib" );
    
    path = CFile::MakePath("/dir/subdir", "file", "ext");
    _ASSERT( path == "/dir/subdir/file.ext" );

    CFile f("/usr/lib/any.other.lib");
    _ASSERT( f.GetDir()  == "/usr/lib/" );
    _ASSERT( f.GetName() == "any.other.lib" );
    _ASSERT( f.GetBase() == "any.other" );
    _ASSERT( f.GetExt()  == ".lib" );

#elif defined NCBI_OS_MAC
        ?;
#endif

}



/////////////////////////////////
// Work with files
//

static void s_TEST_File(void)
{
    // Create test file 
    FILE* file = fopen("file_1", "w+");
    _ASSERT( file );
    fputs("test data", file);
    fclose(file);
    
    CFile f("file_1");

    // Get file size
    _ASSERT( f.GetLength() == 9);
    _ASSERT( CFile("file_2").GetLength() == -1);

    // Check the file exists
    _ASSERT( f.Exists() );
    _ASSERT( !CFile("file_2").Exists() );

    // Rename the file
    _ASSERT( f.Rename("file_2") );

    // Status
    CDirEntry::EType file_type;
    file_type = f.GetType(); 
    CDirEntry::TMode user, group, other;
    _ASSERT ( f.GetMode(&user, &group, &other) );
    cout << "File type : " << file_type << endl;
    cout << "File mode : "
         << ((user  & CDirEntry::fRead)    ? "r" : "-")
         << ((user  & CDirEntry::fWrite)   ? "w" : "-")
         << ((user  & CDirEntry::fExecute) ? "x" : "-")
         << ((group & CDirEntry::fRead)    ? "r" : "-")
         << ((group & CDirEntry::fWrite)   ? "w" : "-")
         << ((group & CDirEntry::fExecute) ? "x" : "-")
         << ((other & CDirEntry::fRead)    ? "r" : "-")
         << ((other & CDirEntry::fWrite)   ? "w" : "-")
         << ((other & CDirEntry::fExecute) ? "x" : "-")
         << endl;
    
    // Remove the file
    _ASSERT( f.Remove() );

    // Check the file exists
    _ASSERT( !CFile("file_1").Exists() );
    _ASSERT( !CFile("file_2").Exists() );

    // Create temporary file
    fstream* stream = CFile::CreateTmpFile();
    _ASSERT( stream );
    *stream << "test data";
    delete stream;

    stream = CFile::CreateTmpFile("test.tmp");
    _ASSERT( stream );
    *stream << "test data";
    delete stream;

    // Get temporary file name
    cout << CFile::GetTmpName() << endl;
    cout << CFile::GetTmpNameExt(".","tmp_") << endl;
}


/////////////////////////////////
// Work with directories
//

static void s_TEST_Dir(void)
{
    // Create directory
    _ASSERT( CDir("dir_1").Create() );
    _ASSERT( !CDir("dir_1").Create() );

    // Create file in the directory
    FILE* file = fopen("dir_1/file_1", "w+");
    _ASSERT( file );
    fclose(file);

    // Check the directory exists
    _ASSERT( CDir("dir_1").Exists() );
    _ASSERT( !CDir("dir_2").Exists() );
    _ASSERT( CDirEntry("dir_1").Exists() );
    _ASSERT( CDirEntry("dir_1/file_1").Exists() );

    // Check entry type
    _ASSERT( CDirEntry("dir_1").IsDir() );
    _ASSERT( !CDirEntry("dir_1").IsFile() );
    _ASSERT( CDirEntry("dir_1/file_1").IsFile() );
    _ASSERT( !CDirEntry("dir_1/file_1").IsDir() );
    
    // Rename the directory
    _ASSERT( CDir("dir_1").Rename("dir_2") );

    // Remove the directory
    _ASSERT( !CDirEntry("dir_2").Remove() );
    _ASSERT( CFile("dir_2/file_1").Remove() );
    _ASSERT( CDir("dir_2").Remove(CDir::eOnlyEmpty) );

    // Check the directory exists
    _ASSERT( !CDir("dir_1").Exists() );
    _ASSERT( !CDir("dir_2").Exists() );

    // Current directory list
    CDir dir(".");

    cout << endl;
    iterate(CDir, i, dir) {
        string entry = *i;
        cout << entry << endl;
    }
    cout << endl;

    // Create dir structure for deletion
    _ASSERT( CDir("dir_3").Create() );
    _ASSERT( CDir("dir_3/subdir_1").Create() );
    _ASSERT( CDir("dir_3/subdir_2").Create() );
    
    file = fopen("dir_3/file", "w+");
    _ASSERT( file );
    fclose(file);
    file = fopen("dir_3/subdir_1/file", "w+");
    _ASSERT( file );
    fclose(file);

    // Delete dir
    dir.Reset("dir_3");
    _ASSERT( !dir.Remove(CDir::eOnlyEmpty) );
    _ASSERT( dir.Exists() );

    _ASSERT( !dir.Remove(CDir::eNonRecursive) );
    _ASSERT( dir.Exists() );
    _ASSERT( CDir("dir_3/subdir_1").Exists() );
    _ASSERT( CFile("dir_3/subdir_1/file").Exists() );
    _ASSERT( !CFile("dir_3/file").Exists() );

    _ASSERT( dir.Remove(CDir::eRecursive) );
    _ASSERT( !dir.Exists() );
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_files",
                       "test file's accessory functions");
    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    cout << "Run test" << endl << endl;

    s_TEST_SplitPath();
    s_TEST_File();
    s_TEST_Dir();

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
