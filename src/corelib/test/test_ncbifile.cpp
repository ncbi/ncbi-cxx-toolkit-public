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
 * Revision 1.2  2001/11/01 20:06:50  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.1  2001/09/19 13:08:15  ivanov
 * Initial revision
 *
 *
 * ===========================================================================
 */

#define _DEBUG 1

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/files.hpp>
#include <stdio.h>


BEGIN_NCBI_SCOPE

END_NCBI_SCOPE


USING_NCBI_SCOPE;

#ifdef __MACOS__
#define main testfiles
extern int testfiles(int, const char *[]);
#endif


/////////////////////////////////
// File name spliting tests
//

static void s_TEST_SplitPath(void)
{
    string path, dir, title, ext;
    
#if defined NCBI_OS_MSWIN
    CFile::SplitPath("c:\\windows\\command\\win.ini", &dir, &title, &ext);
    _VERIFY( dir   == "c:\\windows\\command\\" );
    _VERIFY( title == "win" );
    _VERIFY( ext   == ".ini" );

    path = CFile::MakePath("c:\\windows\\", "win.ini");
    _VERIFY( path == "c:\\windows\\win.ini" );

    CFile f("c:\\dir\\subdir\\file.ext");
    _VERIFY( f.GetDir()  == "c:\\dir\\subdir\\" );
    _VERIFY( f.GetName() == "file.ext" );
    _VERIFY( f.GetBase() == "file" );
    _VERIFY( f.GetExt()  == ".ext" );

#elif defined NCBI_OS_UNIX

    CFile::SplitPath("/usr/lib/any.other.lib", &dir, &title, &ext);
    _VERIFY( dir   == "/usr/lib/" );
    _VERIFY( title == "any.other" );
    _VERIFY( ext   == ".lib" );
    
    path = CFile::MakePath("/dir/subdir", "file", "ext");
    _VERIFY( path == "/dir/subdir/file.ext" );

    CFile f("/usr/lib/any.other.lib");
    _VERIFY( f.GetDir()  == "/usr/lib/" );
    _VERIFY( f.GetName() == "any.other.lib" );
    _VERIFY( f.GetBase() == "any.other" );
    _VERIFY( f.GetExt()  == ".lib" );

#elif defined NCBI_OS_MAC
	CFile::SplitPath("Hard Disk:Folder:1984.mov", &dir, &title, &ext);
    _VERIFY( dir  == "Hard Disk:Folder:" );
    _VERIFY( title == "1984" );
    _VERIFY( ext  == ".mov" );
    
    path = CFile::MakePath("Hard Disk:Folder:",  "file", "ext");
    _VERIFY( path == "Hard Disk:Folder:file.ext" );
    path = CFile::MakePath("Hard Disk:Folder",   "file", "ext");
    _VERIFY( path == "Hard Disk:Folder:file.ext" );
    
    //CFile f(":Not likely to exist.tar.gz");
    //cout << f.GetDir() << endl;
    //_VERIFY( f.GetName() == "Not likely to exist.tar.gz" );
    //_VERIFY( f.GetBase() == "Not likely to exist.tar" );
    //_VERIFY( f.GetExt()  == ".gz" );
	
#endif

}



/////////////////////////////////
// Work with files
//

static void s_TEST_File(void)
{
    // Create test file 
    FILE* file = fopen("file_1", "w+");
    _VERIFY( file );
    fputs("test data", file);
    fclose(file);
    
    CFile f("file_1");

    // Get file size
    _VERIFY( f.GetLength() == 9);
    _VERIFY( CFile("file_2").GetLength() == -1);

    // Check the file exists
    _VERIFY( f.Exists() );
    _VERIFY( !CFile("file_2").Exists() );

    // Rename the file
    _VERIFY( f.Rename("file_2") );

    // Status
    CDirEntry::EType file_type;
    file_type = f.GetType(); 
    CDirEntry::TMode user, group, other;
    user = group = other = 0;
    _VERIFY ( f.GetMode(&user, &group, &other) );
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
    _VERIFY( f.Remove() );

    // Check the file exists
    _VERIFY( !CFile("file_1").Exists() );
    _VERIFY( !CFile("file_2").Exists() );

    // Create temporary file
    fstream* stream = CFile::CreateTmpFile();
    _VERIFY( stream );
    *stream << "test data";
    delete stream;

    stream = CFile::CreateTmpFile("test.tmp");
    _VERIFY( stream );
    *stream << "test data";
    delete stream;

    // Get temporary file name
    cout << CFile::GetTmpName() << endl;
    cout << CFile::GetTmpNameExt(".","tmp_") << endl;
}


/////////////////////////////////
// Work with directories
//

#ifdef NCBI_OS_MAC
#define REL ":"
#define SEP ":"
#define CWD ":"
#else
#define REL ""
#define SEP "/"
#define CWD "."
#endif

static void s_TEST_Dir(void)
{
    // Delete the directory if it exists before we start testing
    CDirEntry("dir_1").Remove();
    
    // Create directory
    _VERIFY( CDir("dir_1").Create() );
    _VERIFY( !CDir("dir_1").Create() );

    // Create file in the directory
    FILE* file = fopen(REL "dir_1" SEP "file_1", "w+");
    _VERIFY( file );
    fclose(file);

    // Check the directory exists
    _VERIFY( CDir("dir_1").Exists() );
    _VERIFY( !CDir("dir_2").Exists() );
    _VERIFY( CDirEntry("dir_1").Exists() );
    _VERIFY( CDirEntry(REL"dir_1" SEP "file_1").Exists() );

    // Check entry type
    _VERIFY( CDirEntry("dir_1").IsDir() );
    _VERIFY( !CDirEntry("dir_1").IsFile() );
    _VERIFY( CDirEntry(REL "dir_1" SEP "file_1").IsFile() );
    _VERIFY( !CDirEntry(REL "dir_1" SEP "file_1").IsDir() );
    
    // Rename the directory
    _VERIFY( CDir("dir_1").Rename("dir_2") );

    // Remove the directory
    _VERIFY( !CDirEntry("dir_2").Remove() );
    _VERIFY( CFile(REL "dir_2" SEP "file_1").Remove() );
    _VERIFY( CDir("dir_2").Remove(CDir::eOnlyEmpty) );

    // Check the directory exists
    _VERIFY( !CDir("dir_1").Exists() );
    _VERIFY( !CDir("dir_2").Exists() );

    // Current directory list
    CDir dir(CWD);

    cout << endl;
    vector<CDirEntry> contents = dir.Contents();
    iterate(vector<CDirEntry>, i, contents) {
        string entry = i->GetPath();
        cout << entry << endl;
    }
    cout << endl;

    // Create dir structure for deletion
    _VERIFY( CDir("dir_3").Create() );
    _VERIFY( CDir(REL "dir_3" SEP "subdir_1").Create() );
    _VERIFY( CDir(REL "dir_3" SEP "subdir_2").Create() );
    
    file = fopen(REL "dir_3" SEP "file", "w+");
    _VERIFY( file );
    fclose(file);
    file = fopen(REL "dir_3" SEP "subdir_1" SEP "file", "w+");
    _VERIFY( file );
    fclose(file);

    // Delete dir
    dir.Reset("dir_3");
    _VERIFY( !dir.Remove(CDir::eOnlyEmpty) );
    _VERIFY( dir.Exists() );

    _VERIFY( !dir.Remove(CDir::eNonRecursive) );
    _VERIFY( dir.Exists() );
    _VERIFY( CDir(REL "dir_3" SEP "subdir_1").Exists() );
    _VERIFY( CFile(REL "dir_3" SEP "subdir_1" SEP "file").Exists() );
    _VERIFY( !CFile(REL "dir_3" SEP "file").Exists() );

    _VERIFY( dir.Remove(CDir::eRecursive) );
    _VERIFY( !dir.Exists() );
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
