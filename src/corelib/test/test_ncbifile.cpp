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
 * File Description:   Test program for file's accessory functions
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////
// File name spliting tests
//

static void s_TEST_SplitPath(void)
{
    string path, dir, title, ext;
    
#if defined(NCBI_OS_MSWIN)
    CFile::SplitPath("c:\\windows\\command\\win.ini", &dir, &title, &ext);
    assert( dir   == "c:\\windows\\command\\" );
    assert( title == "win" );
    assert( ext   == ".ini" );

    path = CFile::MakePath("c:\\windows\\", "win.ini");
    assert( path == "c:\\windows\\win.ini" );

    CFile::SplitPath("c:win.ini", &dir, &title, &ext);
    assert( dir   == "c:" );
    assert( title == "win" );
    assert( ext   == ".ini" );

    path = CFile::MakePath("c:", "win", "ini");
    assert( path == "c:win.ini" );

    CFile f("c:\\dir\\subdir\\file.ext");
    assert( f.GetDir()  == "c:\\dir\\subdir\\" );
    assert( f.GetName() == "file.ext" );
    assert( f.GetBase() == "file" );
    assert( f.GetExt()  == ".ext" );

#elif defined(NCBI_OS_UNIX)

    CFile::SplitPath("/usr/lib/any.other.lib", &dir, &title, &ext);
    assert( dir   == "/usr/lib/" );
    assert( title == "any.other" );
    assert( ext   == ".lib" );
    
    path = CFile::MakePath("/dir/subdir", "file", "ext");
    assert( path == "/dir/subdir/file.ext" );

    CFile f("/usr/lib/any.other.lib");
    assert( f.GetDir()  == "/usr/lib/" );
    assert( f.GetName() == "any.other.lib" );
    assert( f.GetBase() == "any.other" );
    assert( f.GetExt()  == ".lib" );

#elif defined(NCBI_OS_MAC)
	CFile::SplitPath("Hard Disk:Folder:1984.mov", &dir, &title, &ext);
    assert( dir  == "Hard Disk:Folder:" );
    assert( title == "1984" );
    assert( ext  == ".mov" );
    
    path = CFile::MakePath("Hard Disk:Folder:",  "file", "ext");
    assert( path == "Hard Disk:Folder:file.ext" );
    path = CFile::MakePath("Hard Disk:Folder",   "file", "ext");
    assert( path == "Hard Disk:Folder:file.ext" );
    
#endif
}


/////////////////////////////////
//  Path checking tests
//

static void s_TEST_CheckPath(void)
{
     CDirEntry d;

    // IsAbsolutePath() test

#if defined(NCBI_OS_MSWIN)
    assert( d.IsAbsolutePath("c:\\") );
    assert( d.IsAbsolutePath("c:\\file") );
    assert( d.IsAbsolutePath("c:file") );
    assert( d.IsAbsolutePath("\\\\machine\\dir") );
    assert( d.IsAbsolutePath("\\file") );
    assert(!d.IsAbsolutePath("file") );
    assert(!d.IsAbsolutePath(".\\file") );
    assert(!d.IsAbsolutePath("..\\file") );
    assert(!d.IsAbsolutePath("dir\\file") );
#elif defined(NCBI_OS_UNIX)
    assert( d.IsAbsolutePath("/") );
    assert( d.IsAbsolutePath("/file") );
    assert( d.IsAbsolutePath("/dir/dir") );
    assert(!d.IsAbsolutePath("file") );
    assert(!d.IsAbsolutePath("./file") );
    assert(!d.IsAbsolutePath("../file") );
    assert(!d.IsAbsolutePath("dir/file") );
#elif defined(NCBI_OS_MAC)
    assert( d.IsAbsolutePath("HD:") );
    assert( d.IsAbsolutePath("HD:file") );
    assert(!d.IsAbsolutePath("file") );
    assert(!d.IsAbsolutePath(":file") );
    assert(!d.IsAbsolutePath(":file:file") );
#endif

    // Convert path to OS dependent test

    assert( d.ConvertToOSPath("")             == "" );
    assert( d.ConvertToOSPath("c:\\file")     == "c:\\file" );
    assert( d.ConvertToOSPath("/dir/file")    == "/dir/file" );
    assert( d.ConvertToOSPath("dir:file")     == "dir:file" );
#if defined(NCBI_OS_MSWIN)
    assert( d.ConvertToOSPath("dir")          == "dir" );
    assert( d.ConvertToOSPath("dir\\file")    == "dir\\file" );
    assert( d.ConvertToOSPath("dir/file")     == "dir\\file" );
    assert( d.ConvertToOSPath(":dir:file")    == "dir\\file" );
    assert( d.ConvertToOSPath(":dir::file")   == "dir\\..\\file" );
    assert( d.ConvertToOSPath(":dir:::file")  == "dir\\..\\..\\file" );
    assert( d.ConvertToOSPath("./dir/file")   == ".\\dir\\file" );
    assert( d.ConvertToOSPath("../file")      == "..\\file" );
    assert( d.ConvertToOSPath("../../file")   == "..\\..\\file" );
#elif defined(NCBI_OS_UNIX)
    assert( d.ConvertToOSPath("dir")          == "dir" );
    assert( d.ConvertToOSPath("dir\\file")    == "dir/file" );
    assert( d.ConvertToOSPath("dir/file")     == "dir/file" );
    assert( d.ConvertToOSPath(":dir:file")    == "dir/file" );
    assert( d.ConvertToOSPath(":dir::file")   == "dir/../file" );
    assert( d.ConvertToOSPath(".\\dir\\file") == "./dir/file" );
    assert( d.ConvertToOSPath("..\\file")     == "../file" );
    assert( d.ConvertToOSPath("..\\..\\file") == "../../file" );
#elif defined(NCBI_OS_MAC)
    assert( d.ConvertToOSPath("dir")          == ":dir" );
    assert( d.ConvertToOSPath("dir\\file")    == ":dir:file" );
    assert( d.ConvertToOSPath("dir/file")     == ":dir:file" );
    assert( d.ConvertToOSPath(":dir:file")    == ":dir:file" );
    assert( d.ConvertToOSPath("./dir/file")   == ":dir:file" );
    assert( d.ConvertToOSPath("../file")      == "::file" );
    assert( d.ConvertToOSPath("../../file")   == ":::file" );
    assert( d.ConvertToOSPath("../.././../file")   == "::::file" );
#endif

    // ConcatPath() test

#if defined(NCBI_OS_MSWIN)
    assert( d.ConcatPath("c:", "file")     == "c:file");
    assert( d.ConcatPath("dir", "file")    == "dir\\file");
    assert( d.ConcatPath("dir", "\\file")  == "dir\\file");
    assert( d.ConcatPath("dir\\", "file")  == "dir\\file");
    assert( d.ConcatPath("\\dir\\", "file")== "\\dir\\file");
    assert( d.ConcatPath("", "file")       == "file");
    assert( d.ConcatPath("dir", "")        == "dir\\");
    assert( d.ConcatPath("", "")           == "");
#elif defined(NCBI_OS_UNIX)
    assert( d.ConcatPath("dir", "file")    == "dir/file");
    assert( d.ConcatPath("dir", "/file")   == "dir/file");
    assert( d.ConcatPath("dir/", "file")   == "dir/file");
    assert( d.ConcatPath("/dir/", "file")  == "/dir/file");
    assert( d.ConcatPath("", "file")       == "file");
    assert( d.ConcatPath("dir", "")        == "dir/");
    assert( d.ConcatPath("", "")           == "");
#elif defined(NCBI_OS_MAC)
    assert( d.ConcatPath("HD", "dir")      == "HD:dir");
    assert( d.ConcatPath(":dir", "file")   == ":dir:file");
    assert( d.ConcatPath("dir:", "file")   == "dir:file");
    assert( d.ConcatPath("dir", ":file")   == "dir:file");
    assert( d.ConcatPath("dir::", "file")  == "dir::file");
    assert( d.ConcatPath("dir", "::file")  == "dir::file");
    assert( d.ConcatPath("", "file")       == ":file");
    assert( d.ConcatPath(":file", "")      == ":file:");
    assert( d.ConcatPath("", "")           == ":");
#endif
    // Concat any OS paths
    assert( d.ConcatPathEx("dir/", "file")     == "dir/file");
    assert( d.ConcatPathEx("/dir/", "file")    == "/dir/file");
    assert( d.ConcatPathEx("dir\\", ":file")   == "dir\\file");
    assert( d.ConcatPathEx("dir\\dir", "file") == "dir\\dir\\file");
    assert( d.ConcatPathEx("dir/", ":file")    == "dir/file");
    assert( d.ConcatPathEx("dir/dir", "file")  == "dir/dir/file");
    assert( d.ConcatPathEx("dir:dir", "file")  == "dir:dir:file");
}


/////////////////////////////////
// File name maching test
//

static void s_TEST_MatchesMask(void)
{
     CDirEntry d;

    assert( d.MatchesMask(""        ,""));
    assert( d.MatchesMask("file"    ,"*"));
    assert(!d.MatchesMask("file"    ,"*.*"));
    assert( d.MatchesMask("file.cpp","*.cpp"));
    assert( d.MatchesMask("file.cpp","*.c*"));
    assert( d.MatchesMask("file"    ,"file*"));
    assert( d.MatchesMask("file"    ,"f*"));
    assert( d.MatchesMask("file"    ,"f*le"));
    assert( d.MatchesMask("file"    ,"f**l*e"));
    assert(!d.MatchesMask("file"    ,"???"));
    assert( d.MatchesMask("file"    ,"????"));
    assert(!d.MatchesMask("file"    ,"?????"));
    assert( d.MatchesMask("file"    ,"?i??"));
    assert(!d.MatchesMask("file"    ,"?l??"));
    assert(!d.MatchesMask("file"    ,"?i?"));
    assert( d.MatchesMask("file"    ,"?*?"));
    assert( d.MatchesMask("file"    ,"?***?"));
    assert( d.MatchesMask("file"    ,"?*"));
    assert( d.MatchesMask("file"    ,"*?"));
    assert( d.MatchesMask("file"    ,"********?"));
}


/////////////////////////////////
// Work with files
//

static void s_TEST_File(void)
{
    // Create test file 
    FILE* file = fopen("file_1", "w+");
    assert( file );
    fputs("test data", file);
    fclose(file);
    
    CFile f("file_1");

    // Get file size
    assert( f.GetLength() == 9);
    assert( CFile("file_2").GetLength() == -1);

    // Check the file exists
    assert( f.Exists() );
    assert( !CFile("file_2").Exists() );

    // Rename the file
    assert( f.Rename("file_2") );

    // Status
    CDirEntry::EType file_type;
    file_type = f.GetType(); 
    CDirEntry::TMode user, group, other;
    user = group = other = 0;
    assert ( f.GetMode(&user, &group, &other) );
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
    assert( f.Remove() );

    // Check the file exists
    assert( !CFile("file_1").Exists() );
    assert( !CFile("file_2").Exists() );

    // Create temporary file
    fstream* stream = CFile::CreateTmpFile();
    assert( stream );
    *stream << "test data";
    delete stream;

    stream = CFile::CreateTmpFile("test.tmp");
    assert( stream );
    *stream << "test data";
    delete stream;

    // Get temporary file name
    cout << CFile::GetTmpName() << endl;
    cout << CFile::GetTmpNameExt(".","tmp_") << endl;
}


/////////////////////////////////
// Work with directories
//

#if defined(NCBI_OS_MAC)
#   define REL ":"
#   define SEP ":"
#   define CWD ":"
#else
#   define REL ""
#   define SEP "/"
#   define CWD "."
#endif

static void s_TEST_Dir(void)
{
    // Delete the directory if it exists before we start testing
    CDirEntry("dir_1").Remove();
    
    // Create directory
    assert( CDir("dir_1").Create() );
    assert( !CDir("dir_1").Create() );

    // Create file in the directory
    FILE* file = fopen(REL "dir_1" SEP "file_1", "w+");
    assert( file );
    fclose(file);

    // Check the directory exists
    assert( CDir("dir_1").Exists() );
    assert( !CDir("dir_2").Exists() );
    assert( CDirEntry("dir_1").Exists() );
    assert( CDirEntry(REL"dir_1" SEP "file_1").Exists() );

    // Check entry type
    assert( CDirEntry("dir_1").IsDir() );
    assert( !CDirEntry("dir_1").IsFile() );
    assert( CDirEntry(REL "dir_1" SEP "file_1").IsFile() );
    assert( !CDirEntry(REL "dir_1" SEP "file_1").IsDir() );
    
    // Rename the directory
    assert( CDir("dir_1").Rename("dir_2") );

    // Remove the directory
    assert( !CDirEntry("dir_2").Remove(CDirEntry::eOnlyEmpty) );
    assert( CFile(REL "dir_2" SEP "file_1").Remove() );
    assert( CDir("dir_2").Remove(CDir::eOnlyEmpty) );

    // Check the directory exists
    assert( !CDir("dir_1").Exists() );
    assert( !CDir("dir_2").Exists() );

    // Current directory list
    CDir dir(CWD);

    cout << endl;
    CDir::TEntries contents = dir.GetEntries("*");
    iterate(CDir::TEntries, i, contents) {
        string entry = (*i)->GetPath();
        cout << entry << endl;
    }
    cout << endl;

    // Create dir structure for deletion
    assert( CDir("dir_3").Create() );
    assert( CDir(REL "dir_3" SEP "subdir_1").Create() );
    assert( CDir(REL "dir_3" SEP "subdir_2").Create() );
    
    file = fopen(REL "dir_3" SEP "file", "w+");
    assert( file );
    fclose(file);
    file = fopen(REL "dir_3" SEP "subdir_1" SEP "file", "w+");
    assert( file );
    fclose(file);

    // Delete dir
    dir.Reset("dir_3");
    assert( !dir.Remove(CDir::eOnlyEmpty) );
    assert( dir.Exists() );

    assert( !dir.Remove(CDir::eNonRecursive) );
    assert( dir.Exists() );
    assert( CDir(REL "dir_3" SEP "subdir_1").Exists() );
    assert( CFile(REL "dir_3" SEP "subdir_1" SEP "file").Exists() );
    assert( !CFile(REL "dir_3" SEP "file").Exists() );

    assert( dir.Remove(CDir::eRecursive) );
    assert( !dir.Exists() );
    
    // Home dir
    string homedir = CDir::GetHome();
    assert ( !homedir.empty() );
    cout << homedir << endl;
}


/////////////////////////////////
// Memory mapping
//

static void s_TEST_MemoryFile(void)
{
    static const char   s_FileName[] = "file.tmp";
    static const char   s_Data[]     = "test data";
    static const size_t s_DataLen   = sizeof(s_Data) - 1;

    // Create test file 
    FILE* file = fopen(s_FileName, "w+");
    assert( file );
    fputs("test data", file);
    fclose(file);
    
    // Check if the file exists now
    CFile f(s_FileName);
    assert( f.Exists() );
    assert( f.GetLength() == s_DataLen );

    {{
        // Map file into memory
        CMemoryFile fm1(s_FileName);
        assert( fm1.GetSize() == s_DataLen );
        assert( memcmp(fm1.GetPtr(), s_Data, s_DataLen) == 0 );

        // Map second copy of file into memory
        CMemoryFile fm2(s_FileName);
        assert( fm1.GetSize() == fm2.GetSize() );
        assert( memcmp(fm1.GetPtr(), fm2.GetPtr(), fm2.GetSize()) == 0 );

        // Unmap second copy
        assert( fm2.Unmap() );
        assert( fm2.GetSize() == -1 );
        assert( fm2.GetPtr()    ==  0 );
        assert( fm2.Unmap() );
    }}
  
    // Remove the file
    assert( f.Remove() );
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

    // CDirEntry
    s_TEST_SplitPath();
    s_TEST_CheckPath();
    s_TEST_MatchesMask();

    // CFile
    s_TEST_File();
    // CDir
    s_TEST_Dir();

    // CMemoryFile
    s_TEST_MemoryFile();

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
     // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2002/04/16 18:49:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.13  2002/04/01 18:50:18  ivanov
 * Added test for class CMemoryFile
 *
 * Revision 1.12  2002/03/25 17:08:19  ucko
 * Centralize treatment of Cygwin as Unix rather than Windows in configure.
 *
 * Revision 1.11  2002/03/22 20:00:24  ucko
 * Tweak to work on Cygwin.  (Use Unix rather than MSWIN code).
 *
 * Revision 1.10  2002/03/01 16:06:24  kans
 * removed erroneous __MACOS__ section
 *
 * Revision 1.9  2002/01/24 22:18:52  ivanov
 * Changed tests for CDirEntry::Remove() and CDir::Remove()
 *
 * Revision 1.8  2002/01/22 19:29:09  ivanov
 * Added test for ConcatPathEx()
 *
 * Revision 1.7  2002/01/21 04:48:15  vakatov
 * Use #_MSC_VER instead of #NCBI_OS_MSWIN
 *
 * Revision 1.6  2002/01/20 04:42:22  vakatov
 * Do not #define _DEBUG on NCBI_OS_MSWIN
 *
 * Revision 1.5  2002/01/10 16:48:06  ivanov
 * Added test s_TEST_CheckPath()
 *
 * Revision 1.4  2001/11/19 18:13:10  juran
 * Add s_TEST_MatchesMask().
 * Use GetEntries() instead of Contents().
 *
 * Revision 1.3  2001/11/15 16:36:29  ivanov
 * Moved from util to corelib
 *
 * Revision 1.2  2001/11/01 20:06:50  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.1  2001/09/19 13:08:15  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

