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

#if defined(NDEBUG)
#  undef  NDEBUG
#endif 

#if !defined(_DEBUG)  &&  !defined(_MSC_VER)
#  define _DEBUG
#endif 

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <stdio.h>


USING_NCBI_SCOPE;

#if defined(__MACOS__)
#  define main testfiles
extern int testfiles(int, const char *[]);
#endif


/////////////////////////////////
// File name spliting tests
//

static void s_TEST_SplitPath(void)
{
    string path, dir, title, ext;
    
#if defined(NCBI_OS_MSWIN)
    CFile::SplitPath("c:\\windows\\command\\win.ini", &dir, &title, &ext);
    _VERIFY( dir   == "c:\\windows\\command\\" );
    _VERIFY( title == "win" );
    _VERIFY( ext   == ".ini" );

    path = CFile::MakePath("c:\\windows\\", "win.ini");
    _VERIFY( path == "c:\\windows\\win.ini" );

    CFile::SplitPath("c:win.ini", &dir, &title, &ext);
    _VERIFY( dir   == "c:" );
    _VERIFY( title == "win" );
    _VERIFY( ext   == ".ini" );

    path = CFile::MakePath("c:", "win", "ini");
    _VERIFY( path == "c:win.ini" );

    CFile f("c:\\dir\\subdir\\file.ext");
    _VERIFY( f.GetDir()  == "c:\\dir\\subdir\\" );
    _VERIFY( f.GetName() == "file.ext" );
    _VERIFY( f.GetBase() == "file" );
    _VERIFY( f.GetExt()  == ".ext" );

#elif defined(NCBI_OS_UNIX)

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

#elif defined(NCBI_OS_MAC)
	CFile::SplitPath("Hard Disk:Folder:1984.mov", &dir, &title, &ext);
    _VERIFY( dir  == "Hard Disk:Folder:" );
    _VERIFY( title == "1984" );
    _VERIFY( ext  == ".mov" );
    
    path = CFile::MakePath("Hard Disk:Folder:",  "file", "ext");
    _VERIFY( path == "Hard Disk:Folder:file.ext" );
    path = CFile::MakePath("Hard Disk:Folder",   "file", "ext");
    _VERIFY( path == "Hard Disk:Folder:file.ext" );
    
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
    _VERIFY( d.IsAbsolutePath("c:\\") );
    _VERIFY( d.IsAbsolutePath("c:\\file") );
    _VERIFY( d.IsAbsolutePath("c:file") );
    _VERIFY( d.IsAbsolutePath("\\\\machine\\dir") );
    _VERIFY( d.IsAbsolutePath("\\file") );
    _VERIFY(!d.IsAbsolutePath("file") );
    _VERIFY(!d.IsAbsolutePath(".\\file") );
    _VERIFY(!d.IsAbsolutePath("..\\file") );
    _VERIFY(!d.IsAbsolutePath("dir\\file") );
#elif defined(NCBI_OS_UNIX)
    _VERIFY( d.IsAbsolutePath("/") );
    _VERIFY( d.IsAbsolutePath("/file") );
    _VERIFY( d.IsAbsolutePath("/dir/dir") );
    _VERIFY(!d.IsAbsolutePath("file") );
    _VERIFY(!d.IsAbsolutePath("./file") );
    _VERIFY(!d.IsAbsolutePath("../file") );
    _VERIFY(!d.IsAbsolutePath("dir/file") );
#elif defined(NCBI_OS_MAC)
    _VERIFY( d.IsAbsolutePath("HD:") );
    _VERIFY( d.IsAbsolutePath("HD:file") );
    _VERIFY(!d.IsAbsolutePath("file") );
    _VERIFY(!d.IsAbsolutePath(":file") );
    _VERIFY(!d.IsAbsolutePath(":file:file") );
#endif

    // Convert path to OS dependent test

    _VERIFY( d.ConvertToOSPath("")             == "" );
    _VERIFY( d.ConvertToOSPath("c:\\file")     == "c:\\file" );
    _VERIFY( d.ConvertToOSPath("/dir/file")    == "/dir/file" );
    _VERIFY( d.ConvertToOSPath("dir:file")     == "dir:file" );
#if defined(NCBI_OS_MSWIN)
    _VERIFY( d.ConvertToOSPath("dir")          == "dir" );
    _VERIFY( d.ConvertToOSPath("dir\\file")    == "dir\\file" );
    _VERIFY( d.ConvertToOSPath("dir/file")     == "dir\\file" );
    _VERIFY( d.ConvertToOSPath(":dir:file")    == "dir\\file" );
    _VERIFY( d.ConvertToOSPath(":dir::file")   == "dir\\..\\file" );
    _VERIFY( d.ConvertToOSPath(":dir:::file")  == "dir\\..\\..\\file" );
    _VERIFY( d.ConvertToOSPath("./dir/file")   == ".\\dir\\file" );
    _VERIFY( d.ConvertToOSPath("../file")      == "..\\file" );
    _VERIFY( d.ConvertToOSPath("../../file")   == "..\\..\\file" );
#elif defined(NCBI_OS_UNIX)
    _VERIFY( d.ConvertToOSPath("dir")          == "dir" );
    _VERIFY( d.ConvertToOSPath("dir\\file")    == "dir/file" );
    _VERIFY( d.ConvertToOSPath("dir/file")     == "dir/file" );
    _VERIFY( d.ConvertToOSPath(":dir:file")    == "dir/file" );
    _VERIFY( d.ConvertToOSPath(":dir::file")   == "dir/../file" );
    _VERIFY( d.ConvertToOSPath(".\\dir\\file") == "./dir/file" );
    _VERIFY( d.ConvertToOSPath("..\\file")     == "../file" );
    _VERIFY( d.ConvertToOSPath("..\\..\\file") == "../../file" );
#elif defined(NCBI_OS_MAC)
    _VERIFY( d.ConvertToOSPath("dir")          == ":dir" );
    _VERIFY( d.ConvertToOSPath("dir\\file")    == ":dir:file" );
    _VERIFY( d.ConvertToOSPath("dir/file")     == ":dir:file" );
    _VERIFY( d.ConvertToOSPath(":dir:file")    == ":dir:file" );
    _VERIFY( d.ConvertToOSPath("./dir/file")   == ":dir:file" );
    _VERIFY( d.ConvertToOSPath("../file")      == "::file" );
    _VERIFY( d.ConvertToOSPath("../../file")   == ":::file" );
    _VERIFY( d.ConvertToOSPath("../.././../file")   == "::::file" );
#endif

    // ConcatPath() test

#if defined(NCBI_OS_MSWIN)
    _VERIFY( d.ConcatPath("c:", "file")     == "c:file");
    _VERIFY( d.ConcatPath("dir", "file")    == "dir\\file");
    _VERIFY( d.ConcatPath("dir", "\\file")  == "dir\\file");
    _VERIFY( d.ConcatPath("dir\\", "file")  == "dir\\file");
    _VERIFY( d.ConcatPath("\\dir\\", "file")== "\\dir\\file");
    _VERIFY( d.ConcatPath("", "file")       == "file");
    _VERIFY( d.ConcatPath("dir", "")        == "dir\\");
    _VERIFY( d.ConcatPath("", "")           == "");
#elif defined(NCBI_OS_UNIX)
    _VERIFY( d.ConcatPath("dir", "file")    == "dir/file");
    _VERIFY( d.ConcatPath("dir", "/file")   == "dir/file");
    _VERIFY( d.ConcatPath("dir/", "file")   == "dir/file");
    _VERIFY( d.ConcatPath("/dir/", "file")  == "/dir/file");
    _VERIFY( d.ConcatPath("", "file")       == "file");
    _VERIFY( d.ConcatPath("dir", "")        == "dir/");
    _VERIFY( d.ConcatPath("", "")           == "");
#elif defined(NCBI_OS_MAC)
    _VERIFY( d.ConcatPath("HD", "dir")      == "HD:dir");
    _VERIFY( d.ConcatPath(":dir", "file")   == ":dir:file");
    _VERIFY( d.ConcatPath("dir:", "file")   == "dir:file");
    _VERIFY( d.ConcatPath("dir", ":file")   == "dir:file");
    _VERIFY( d.ConcatPath("dir::", "file")  == "dir::file");
    _VERIFY( d.ConcatPath("dir", "::file")  == "dir::file");
    _VERIFY( d.ConcatPath("", "file")       == ":file");
    _VERIFY( d.ConcatPath(":file", "")      == ":file:");
    _VERIFY( d.ConcatPath("", "")           == ":");
#endif
    // Concat any OS paths
    _VERIFY( d.ConcatPathEx("dir/", "file")     == "dir/file");
    _VERIFY( d.ConcatPathEx("/dir/", "file")    == "/dir/file");
    _VERIFY( d.ConcatPathEx("dir\\", ":file")   == "dir\\file");
    _VERIFY( d.ConcatPathEx("dir\\dir", "file") == "dir\\dir\\file");
    _VERIFY( d.ConcatPathEx("dir/", ":file")    == "dir/file");
    _VERIFY( d.ConcatPathEx("dir/dir", "file")  == "dir/dir/file");
    _VERIFY( d.ConcatPathEx("dir:dir", "file")  == "dir:dir:file");
}


/////////////////////////////////
// File name maching test
//

static void s_TEST_MatchesMask(void)
{
     CDirEntry d;

    _ASSERT( d.MatchesMask(""        ,""));
    _ASSERT( d.MatchesMask("file"    ,"*"));
    _ASSERT(!d.MatchesMask("file"    ,"*.*"));
    _ASSERT( d.MatchesMask("file.cpp","*.cpp"));
    _ASSERT( d.MatchesMask("file.cpp","*.c*"));
    _ASSERT( d.MatchesMask("file"    ,"file*"));
    _ASSERT( d.MatchesMask("file"    ,"f*"));
    _ASSERT( d.MatchesMask("file"    ,"f*le"));
    _ASSERT( d.MatchesMask("file"    ,"f**l*e"));
    _ASSERT(!d.MatchesMask("file"    ,"???"));
    _ASSERT( d.MatchesMask("file"    ,"????"));
    _ASSERT(!d.MatchesMask("file"    ,"?????"));
    _ASSERT( d.MatchesMask("file"    ,"?i??"));
    _ASSERT(!d.MatchesMask("file"    ,"?l??"));
    _ASSERT(!d.MatchesMask("file"    ,"?i?"));
    _ASSERT( d.MatchesMask("file"    ,"?*?"));
    _ASSERT( d.MatchesMask("file"    ,"?***?"));
    _ASSERT( d.MatchesMask("file"    ,"?*"));
    _ASSERT( d.MatchesMask("file"    ,"*?"));
    _ASSERT( d.MatchesMask("file"    ,"********?"));
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
    CDir::TEntries contents = dir.GetEntries("*");
    iterate(CDir::TEntries, i, contents) {
        string entry = (*i)->GetPath();
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
    
    // Home dir
    string homedir = CDir::GetHome();
    _VERIFY ( !homedir.empty() );
    cout << homedir << endl;
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

/*
 * ===========================================================================
 * $Log$
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

