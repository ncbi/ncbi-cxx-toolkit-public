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
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for file's accessory functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_mask.hpp>
#include <stdio.h>
#include <limits.h>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


// Create test file 
static void s_CreateTestFile(const string& file)
{
    CNcbiOfstream fs(file.c_str(), ios::out | ios::trunc | ios::binary);
    assert(fs.good());
    fs << "test data";
    assert(fs.good());
    fs.close();
}


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

    assert( CFile::AddTrailingPathSeparator("dir")   == "dir\\");
    assert( CFile::AddTrailingPathSeparator("dir\\") == "dir\\");
    assert( CFile::DeleteTrailingPathSeparator("dir\\path\\/")=="dir\\path");

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

    assert( CFile::AddTrailingPathSeparator("dir")  == "dir/");
    assert( CFile::AddTrailingPathSeparator("dir/") == "dir/");
    assert( CFile::DeleteTrailingPathSeparator("dir/path////")=="dir/path");

#endif

    CFile::SplitPath("name", &dir, &title, &ext);
    assert( dir.empty() );
    assert( title == "name" );
    assert( ext.empty() );

    path = CFile::MakePath(dir, title, ext);
    assert( path == "name" );

    CFile::SplitPath(kEmptyStr, &dir, &title, &ext);
    assert( dir.empty() );
    assert( title.empty() );
    assert( ext.empty() );

    path = CFile::MakePath(dir, title, ext);
    assert( path.empty() );
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
#endif

    // Convert path to OS dependent test

    assert( d.ConvertToOSPath("")               == "" );
    assert( d.ConvertToOSPath("c:\\file")       == "c:\\file" );
    assert( d.ConvertToOSPath("/dir/file")      == "/dir/file" );
    assert( d.ConvertToOSPath("dir:file")       == "dir:file" );
#if defined(NCBI_OS_MSWIN)
    assert( d.ConvertToOSPath("dir")            == "dir" );
    assert( d.ConvertToOSPath("dir\\file")      == "dir\\file" );
    assert( d.ConvertToOSPath("dir/file")       == "dir\\file" );
    assert( d.ConvertToOSPath(":dir:file")      == "dir\\file" );
    assert( d.ConvertToOSPath(":dir::file")     == "file" );
    assert( d.ConvertToOSPath(":dir:::file")    == "..\\file" );
    assert( d.ConvertToOSPath("./dir/file")     == "dir\\file" );
    assert( d.ConvertToOSPath("../file")        == "..\\file" );
    assert( d.ConvertToOSPath("../../file")     == "..\\..\\file" );
#elif defined(NCBI_OS_UNIX)
    assert( d.ConvertToOSPath("dir")            == "dir" );
    assert( d.ConvertToOSPath("dir\\file")      == "dir/file" );
    assert( d.ConvertToOSPath("dir/file")       == "dir/file" );
    assert( d.ConvertToOSPath(":dir:file")      == "dir/file" );
    assert( d.ConvertToOSPath(":dir::file")     == "file" );
    assert( d.ConvertToOSPath(":dir:::file")    == "../file" );
    assert( d.ConvertToOSPath(".\\dir\\file")   == "dir/file" );
    assert( d.ConvertToOSPath("..\\file")       == "../file" );
    assert( d.ConvertToOSPath("..\\..\\file")   == "../../file" );
#endif

    // ConcatPath() test

#if defined(NCBI_OS_MSWIN)
    assert( d.ConcatPath("c:", "file")          == "c:file" );
    assert( d.ConcatPath("dir", "file")         == "dir\\file" );
    assert( d.ConcatPath("dir", "\\file")       == "dir\\file" );
    assert( d.ConcatPath("dir\\", "file")       == "dir\\file" );
    assert( d.ConcatPath("\\dir\\", "file")     == "\\dir\\file" );
    assert( d.ConcatPath("", "file")            == "file" );
    assert( d.ConcatPath("dir", "")             == "dir\\" );
    assert( d.ConcatPath("", "")                == "" );
#elif defined(NCBI_OS_UNIX)
    assert( d.ConcatPath("dir", "file")         == "dir/file" );
    assert( d.ConcatPath("dir", "/file")        == "dir/file" );
    assert( d.ConcatPath("dir/", "file")        == "dir/file" );
    assert( d.ConcatPath("/dir/", "file")       == "/dir/file" );
    assert( d.ConcatPath("", "file")            == "file" );
    assert( d.ConcatPath("dir", "")             == "dir/" );
    assert( d.ConcatPath("", "")                == "" );
#endif
    // Concat any OS paths
    assert( d.ConcatPathEx("dir/", "file")      == "dir/file" );
    assert( d.ConcatPathEx("/dir/", "file")     == "/dir/file" );
    assert( d.ConcatPathEx("dir\\", ":file")    == "dir\\file" );
    assert( d.ConcatPathEx("dir\\dir", "file")  == "dir\\dir\\file" );
    assert( d.ConcatPathEx("dir/", ":file")     == "dir/file" );
    assert( d.ConcatPathEx("dir/dir", "file")   == "dir/dir/file" );
    assert( d.ConcatPathEx("dir:dir", "file")   == "dir:dir:file" );

    // Normalize path test

#if defined(NCBI_OS_MSWIN)
    assert( d.NormalizePath("")                 == "" );
    assert( d.NormalizePath("c:\\")             == "c:\\" );
    assert( d.NormalizePath("c:\\file")         == "c:\\file" );
    assert( d.NormalizePath("c:file")           == "c:file" );
    assert( d.NormalizePath("c:\\dir\\..\\file")== "c:\\file" );
    assert( d.NormalizePath("c:..\\file")       == "c:..\\file" );
    assert( d.NormalizePath("c\\")              == "c" );
    assert( d.NormalizePath("..\\file")         == "..\\file" );
    assert( d.NormalizePath("\\..\\file")       == "\\file" );
    assert( d.NormalizePath(".\\..\\dir\\..")   == ".." );
    assert( d.NormalizePath(".\\dir\\.")        == "dir" );
    assert( d.NormalizePath(".\\.\\.\\.")       == "." );
    assert( d.NormalizePath("..\\..\\..\\..")   == "..\\..\\..\\.." );
    assert( d.NormalizePath("dir\\\\dir\\\\")   == "dir\\dir" );
    assert( d.NormalizePath("\\\\machine\\dir") == "\\\\machine\\dir");
    assert( d.NormalizePath("\\\\?\\x")         == "\\x" );
    assert( d.NormalizePath("\\\\?\\UNC\\m\\d") == "\\\\m\\d" );
    assert( d.NormalizePath("dir/file")         == "dir\\file" );
    assert( d.NormalizePath("/")                == "\\" );
    assert( d.NormalizePath("\\")               == "\\" );
    assert( d.NormalizePath("\\.\\")            == "\\" );
    assert( d.NormalizePath("\\..\\")           == "\\" );

#elif defined(NCBI_OS_UNIX )
    assert( d.NormalizePath("")                 == "" );
    assert( d.NormalizePath("/file")            == "/file" );
    assert( d.NormalizePath("./file")           == "file" );
    assert( d.NormalizePath("dir1/dir2/../file")== "dir1/file" );
    assert( d.NormalizePath("../file")          == "../file" );
    assert( d.NormalizePath("/../file")         == "/file" );
    assert( d.NormalizePath("dir/")             == "dir" );
    assert( d.NormalizePath("./../dir/..")      == ".." );
    assert( d.NormalizePath("./dir/.")          == "dir" );
    assert( d.NormalizePath("./././.")          == "." );
    assert( d.NormalizePath("../../../..")      == "../../../.." );
    assert( d.NormalizePath("dir//dir//")       == "dir/dir" );
    assert( d.NormalizePath("///dir//")         == "/dir" );
    assert( d.NormalizePath("dir\\file")        == "dir\\file" );
    assert( d.NormalizePath("\\")               == "\\" );
    assert( d.NormalizePath("/")                == "/" );
    assert( d.NormalizePath("/./")              == "/" );
    assert( d.NormalizePath("/../")             == "/" );
#endif
}


/////////////////////////////////
// File name maching test
//

static void s_TEST_MatchesMask(void)
{
    CDirEntry d;
    
    assert( d.MatchesMask(""        , ""));
    assert( d.MatchesMask("file"    , "*"));
    assert(!d.MatchesMask("file"    , "*.*"));
    assert( d.MatchesMask("file.cpp", "*.cpp"));
    assert(!d.MatchesMask("file.cpp", "*.CPP"));
    assert( d.MatchesMask("file.cpp", "*.CPP", NStr::eNocase));
    assert( d.MatchesMask("file.cpp", "*.c*"));
    assert( d.MatchesMask("file"    , "file*"));
    assert( d.MatchesMask("file"    , "f*"));
    assert( d.MatchesMask("file"    , "f*le"));
    assert( d.MatchesMask("file"    , "f**l*e"));
    assert(!d.MatchesMask("file"    , "???"));
    assert( d.MatchesMask("file"    , "????"));
    assert(!d.MatchesMask("file"    , "?????"));
    assert( d.MatchesMask("file"    , "?i??"));
    assert(!d.MatchesMask("file"    , "?l??"));
    assert(!d.MatchesMask("file"    , "?i?"));
    assert( d.MatchesMask("file"    , "?*?"));
    assert( d.MatchesMask("file"    , "?***?"));
    assert( d.MatchesMask("file"    , "?*"));
    assert( d.MatchesMask("file"    , "*?"));
    assert( d.MatchesMask("file"    , "********?"));

    CMaskFileName mask;
    assert( d.MatchesMask(""        , mask));
    assert( d.MatchesMask("file"    , mask));

    mask.Add("*.c");
    mask.Add("*.cpp");
    mask.Add("????.h");
    mask.AddExclusion("e*.cpp");

    assert( d.MatchesMask("file.c"      , mask));
    assert( d.MatchesMask("file.cpp"    , mask));
    assert( d.MatchesMask("dir/file.cpp", mask));
    assert(!d.MatchesMask("exclude.cpp" , mask));
    assert(!d.MatchesMask("e.cpp"       , mask));
    assert( d.MatchesMask("file.h"      , mask));
    assert(!d.MatchesMask("include.h"   , mask));

    mask.Remove("*.cpp");
    assert(!d.MatchesMask("file.cpp"    , mask));
}


/////////////////////////////////
// Work with files
//

static void s_TEST_File(void)
{
    {{
        // Create test file 
        s_CreateTestFile("file1");
	
        CFile f("file1");
        CFile f1 = f;
        CFile f2("file2");
        CFile f3("file3");

        // Get file size
        assert( f.Exists() );
        assert( f.GetLength() == 9);
        f2.Remove();
        assert( f2.GetLength() == -1);

        // Copy/rename/backup and other file operations

        assert( !f2.Exists() );
        assert( f1.Copy(f2.GetPath()) );
        assert( f2.Exists() );
        assert( f1.Compare(f2.GetPath()) );

        assert( !f3.Exists() );
        assert( f1.Copy(f3.GetPath(), CFile::fCF_PreserveAll) );
        assert( f3.Exists() );

        assert( f1.Backup() );
        assert( CFile("file1").Exists() );
        assert( CFile("file1.bak").Exists() );
        assert( CFile("file1.bak").Remove() );
        assert( !CFile("file1.bak").Exists() );

        assert( f1.Backup(".bak", CFile::eBackup_Rename) );
        assert( !CFile("file1").Exists() );
        assert( CFile("file1.bak").Exists() );
        assert( f1.Rename(f.GetPath()) );

        assert( !f1.Rename(f2.GetPath()) );
        assert( CFile("file1").Exists() );
        assert( f2.Exists() );

        assert( f1.Rename(f2.GetPath(), CFile::fRF_Overwrite) );
        assert( !CFile("file1").Exists() );
        assert( f1.Exists() );
        assert( f2.Exists() );

        assert( f2.Rename(f3.GetPath(), CFile::fRF_Backup |
	                                CFile::fRF_Overwrite) );
        assert( f2.Exists() );
        assert( f3.Exists() );
        assert( CFile("file3" + f3.GetBackupSuffix()).Exists() );
        assert( CFile("file3" + f3.GetBackupSuffix()).Remove() );

        f = f3;
        assert( f.Exists() );
        assert( f.GetPath() == f3.GetPath() );

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
        string owner_name, group_name;
        assert( f.GetOwner(&owner_name, &group_name) );
        cout << "File owner: " << owner_name << ":" << group_name << endl;

        // Get/set file modification time
        CTime::SetFormat("M/D/Y h:m:s Z");
        CTime mtime, ctime, atime;
        assert( f.GetTime(&mtime, &ctime , &atime) );
        cout << "File creation time     : " << ctime.AsString() << endl;
        cout << "File modification time : " << mtime.AsString() << endl;
        cout << "File last access time  : " << atime.AsString() << endl;
        assert( f.GetTime(&mtime, 0 , &atime) );
        CTime mtime_new(mtime), atime_new(atime);
        mtime_new.AddDay(-2);
        atime_new.AddDay(-1);
        assert( f.SetTime(&mtime_new, &atime_new) );
        assert( f.GetTime(&mtime, &atime) );
        cout << "File modification time : " << mtime.AsString() << endl;
        cout << "File last access time  : " << atime.AsString() << endl;
        assert( mtime == mtime_new );

        // Remove the file
        assert( f.Remove() );
        assert( !f.Exists() );
    }}
    
    {{
        // Create temporary file
        const string kTestData = "testdata";
        fstream* stream = CFile::CreateTmpFile();
        assert( stream );
        assert( (*stream).good() );
        *stream << kTestData;
        assert( (*stream).good() );
        (*stream).flush();
        (*stream).seekg(0);
        string str;
        (*stream) >> str;
        assert( (*stream).eof() );
        assert( str == kTestData );
        delete stream;

        stream = CFile::CreateTmpFile("test.tmp");
        assert( stream );
        assert( (*stream).good() );
        *stream << kTestData;
        assert( (*stream).good() );
        (*stream).flush();
        (*stream).seekg(0);
        (*stream) >> str;
        assert( (*stream).eof() );
        assert( str == kTestData );
        delete stream;

        // Get temporary file name
        string fn;
        fn = CFile::GetTmpName();
        assert( !fn.empty() );
        assert( !CFile(fn).Exists() );
        fn = CFile::GetTmpNameEx(".", "tmp_");
        assert( !fn.empty() );
        assert( !CFile(fn).Exists() );

#  if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
        fn = CFile::GetTmpName(CFile::eTmpFileCreate);
        assert( !fn.empty() );
        assert( CFile(fn).Exists() );
        CFile(fn).Remove();
        fn = CFile::GetTmpNameEx(".", "tmp_", CFile::eTmpFileCreate);
        assert( !fn.empty() );
        assert( CFile(fn).Exists() );
        CFile(fn).Remove();
#  endif
    }}
}


/////////////////////////////////
// Work with directories
//

static void s_TEST_Dir(void)
{
    // Delete the directory if it exists before we start testing
    CDirEntry("dir1").Remove();
    
    // Create directory
    assert( CDir("dir1").Create() );
    // Try to create it second time -- should be OK
    assert( CDir("dir1").Create() );

    // Create file in the directory
    s_CreateTestFile("dir1/file1");

    // Check the directory exists
    assert( CDir("dir1").Exists() );
    assert( !CDir("dir2").Exists() );
    assert( CDirEntry("dir1").Exists() );
    assert( CDirEntry("dir1/file1").Exists() );

    // Check entry type
    assert( CDirEntry("dir1").IsDir() );
    assert( !CDirEntry("dir1").IsFile() );
    assert( CDirEntry("dir1/file1").IsFile() );
    assert( !CDirEntry("dir1/file1").IsDir() );

    // Copy the directory
    assert( CDir("dir1").Copy("dir3") );
    assert( !CDir("dir1").Copy("dir3") );
    assert( CDir("dir1").Copy("dir3", CDir::fCF_Overwrite) );

    assert( CDir("dir1").Copy("dir3", CDir::fCF_Backup) );
    assert( CFile("dir3/file1.bak").Exists() );
    assert( !CDir("dir3.bak").Exists() );
    assert( CDir("dir1").Copy("dir3", CDir::fCF_Backup | CDir::fCF_TopDirOnly) );
    assert( !CFile("dir3/file1.bak").Exists() );
    assert( CDir("dir3.bak").Exists() );
    assert( CDir("dir3.bak").Remove() );

    assert( CDir("dir1").CopyToDir("dir3") );
    assert( CDir("dir3/dir1").Exists() );
    assert( CFile("dir3/dir1/file1").Exists() );

    // Rename the directory
    assert( CDir("dir1").Rename("dir2") );

    // Remove the directory
    assert( !CDirEntry("dir2").Remove(CDirEntry::eOnlyEmpty) );
    assert( CFile("dir2/file1").Remove() );
    assert( CDir("dir2").Remove(CDir::eOnlyEmpty) );
    assert( CDir("dir3").Remove() );

    // Check the directory exists
    assert( !CDir("dir1").Exists() );
    assert( !CDir("dir2").Exists() );
    assert( !CDir("dir3").Exists() );

    CDir dir(".");

    // Current directory list
    {{
        cout << endl;
        CDir::TEntries contents = dir.GetEntries("*", CDir::eIgnoreRecursive);
        ITERATE(CDir::TEntries, i, contents) {
            string entry = (*i)->GetPath();
            cout << entry << endl;
        }
        cout << endl;

        // The same but using vector of masks
        vector<string> masks;
        masks.push_back("*");
        CDir::TEntries contents2 = dir.GetEntries(masks, CDir::eIgnoreRecursive);
        assert(contents.size() == contents2.size());

        vector<string> files;
        vector<string> paths;
        paths.push_back(".");

        FindFiles(files, paths.begin(), paths.end(), 
                         masks.begin(), masks.end());

        for (unsigned int i = 0; i < contents.size(); ++i) {
            const CDirEntry& entry1 = *contents[i];
            const CDirEntry& entry2 = *contents2[i];
            string ep1 = entry1.GetPath();
            string ep2 = entry2.GetPath();
            const string& f = files[i];
            assert(ep1 == ep2);
            assert(ep1 == f);
        }

        // Recursive content
        cout << "Recursive content" << endl;
        files.clear();
        FindFiles(files, paths.begin(), paths.end(), 
                        masks.begin(), masks.end(), 
                        fFF_File | fFF_Dir | fFF_Recursive);
        ITERATE(vector<string>, fit, files) {
            cout << *fit << endl;
        }
    }}

    // Create directory structure for deletion
    {{
        assert( CDir("dir3").Create() );
        assert( CDir("dir3/subdir1").Create() );
        assert( CDir("dir3/subdir2").Create() );
        
	s_CreateTestFile("dir3/file");
	s_CreateTestFile("dir3/subdir1/file");
	
        // Delete dir
        dir.Reset("dir3");
        assert( !dir.Remove(CDir::eOnlyEmpty) );
        assert( dir.Exists() );

        assert( !dir.Remove(CDir::eNonRecursive) );
        assert( dir.Exists() );
        assert( CDir("dir3/subdir1").Exists() );
        assert( CFile("dir3/subdir1/file").Exists() );
        assert( !CFile("dir3/file").Exists() );

        assert( dir.Remove(CDir::eRecursive) );
        assert( !dir.Exists() );
    }}
        
    // Home dir
    {{
        string homedir = CDir::GetHome();
        assert ( !homedir.empty() );
        cout << "Home dir: " << homedir << endl;
    }}

    // Creation of relative path from 2 absolute pathes:
    {{
        string rel_path;

#if defined(NCBI_OS_MSWIN)
        assert( CDirEntry::CreateRelativePath(
                "C:\\x\\y\\z\\",
                "C:\\x\\a\\b\\") == "..\\..\\a\\b\\" );

        assert( CDirEntry::CreateRelativePath(
                "C:\\x\\y\\z\\",
                "C:\\x\\a\\b\\file.txt") == "..\\..\\a\\b\\file.txt" );
     
        assert( CDirEntry::CreateRelativePath(
                "C:\\x\\y\\z\\", 
                "C:\\x\\y\\z\\").empty() );

        assert( CDirEntry::CreateRelativePath(
                "C:\\x\\y\\z\\", 
                "C:\\x\\y\\a\\") == "..\\a\\" );

        assert( CDirEntry::CreateRelativePath(
                "\\x\\y\\z\\", 
                "\\x\\y\\") == "..\\" );

#elif defined(NCBI_OS_UNIX)
        assert( CDirEntry::CreateRelativePath(
                "/usr/bin/", "/usr/" ) == "../" );
        assert( CDirEntry::CreateRelativePath(
                "/usr/bin/", "/etc/" ) == "../../etc/" );
#endif
    }}
}


/////////////////////////////////
// Work with symbolic links
//

static void s_TEST_Link(void)
{
#if defined(NCBI_OS_UNIX)
    CDir dir("dir1");
    assert( !dir.Exists() );
    assert( dir.Create() );
    assert( dir.Exists() );
    
    // Create link
    CSymLink link("link1");
    assert( !link.Exists() );
    assert( link.Create("dir1") );
    assert( link.Exists() );
    
    // Check link
    assert( link.IsDir() );
    assert( link.IsDir(eFollowLinks) );
    assert( !link.IsDir(eIgnoreLinks) );
    assert( link.GetType(eIgnoreLinks) == CDirEntry::eLink );
    assert( link.IsLink() );
    assert( link.LookupLink() == "dir1");

    // Copy link with fCF_FollowLinks flag, set by default
    CDir dir2("dir2");
    assert( link.Copy("dir2") );
    assert( dir2.IsDir() );
    assert( dir2.Remove() );
    assert( !dir2.Exists() );

    // Copy only link
    CSymLink link2("link2");
    assert( !link2.Exists() );
    assert( link.Copy("link2", CDirEntry::fCF_Overwrite) );
    assert( link2.Exists() );
    assert( link2.IsLink() );
    assert( link2.LookupLink() == "dir1");
    assert( link2.Remove() );
    assert( !link2.Exists() );
    assert( dir.Exists() );
    
    // Create link to link
    assert( link2.Create("link1") );
    assert( link2.Exists() );

    // Dereference nested link   
    CDirEntry e = link2;
    assert( e.IsLink() );
    assert( e.LookupLink() == "link1");
    e.DereferenceLink();
    assert( e.IsDir() );
    assert( e.GetPath() == "dir1");
    
    // Remove
    assert( link2.Remove() );
    assert( !link2.Exists() );
    assert( link.Exists() );
    assert( link.Remove() );
    assert( !link.Exists() );
    assert( dir.Exists() );
    assert( e.Remove() );
    assert( !dir.Exists() );
    
#endif
}


/////////////////////////////////
// Memory mapping
//

static void s_TEST_MemoryFile(void)
{
    static const char   s_FileName[] = "test.tmp";
    static const char   s_Data[]     = "test data";
    static const char   s_DataTest[] = "teen data";
    static const size_t s_DataLen    = sizeof(s_Data) - 1;

    CFile f(s_FileName);

    // Create test file 
    {{
        CNcbiOfstream fs(s_FileName, ios::out | ios::binary);
        assert(fs.good());
        fs.close();
        // Check if the file exists now and it size is 0
        assert( f.Exists() );
        assert( f.GetLength() == 0 );
    }}

    // Common file checks
    {{
        // Map absent file, expect an exception.
        try {
            CMemoryFile m("some_absent_file_name");
            _TROUBLE;
        } catch (CFileException&) { }

        // Map empty file, expect an exception also.
        CMemoryFile m(s_FileName);
        assert( m.GetPtr() == 0);
        // Do not expect an exception here!!! (special case)
        assert( m.GetSize() == 0);
    }}

    // Write something into out test file
    {{
        CNcbiOfstream fs(s_FileName, ios::out | ios::binary);
        fs.write(s_Data, s_DataLen);
        assert(fs.good());
        fs.close();
        // Check if the file exists now
        assert( f.Exists() );
        assert( f.GetLength() == s_DataLen );
    }}

    // eMMP_Read [+ eMMS_Private]
    {{
        // Map file into memory (for reading only by default)
        CMemoryFile m1(s_FileName);
        assert( m1.GetSize() == s_DataLen );
        assert( memcmp(m1.GetPtr(), s_Data, s_DataLen) == 0 );

        // Map second copy of the file into the memory
        CMemoryFile m2(s_FileName);
        assert( m1.GetSize() == m2.GetSize() );
        assert( memcmp(m1.GetPtr(), m2.GetPtr(), (size_t)m2.GetSize()) == 0 );

        // Unmap second copy
        assert( m2.Unmap() );
        assert( m2.GetPtr() == 0);

        try {
            // Expect an exception now
            m2.GetSize();
            _TROUBLE;
        } catch (CFileException&) { }
        assert( m2.Unmap() );
    }}

    // eMMP_ReadWrite + eMMS_Shared
    {{
        // Map file into memory
        CMemoryFile m1(s_FileName, CMemoryFile::eMMP_ReadWrite,
                       CMemoryFile::eMMS_Shared);
        assert( m1.GetSize() == s_DataLen );
        assert( memcmp(m1.GetPtr(), s_Data, s_DataLen) == 0 );
        char* ptr = (char*)m1.GetPtr();
        *ptr = '@';
        assert( m1.Flush() );

        // Map second copy of the file into the memory
        CMemoryFile m2(s_FileName, CMemoryFile::eMMP_Read,
                       CMemoryFile::eMMS_Shared);
        assert( m1.GetSize() == m2.GetSize() );
        // m2 must contain changes made via m1 
        assert( memcmp(m2.GetPtr(), ptr, s_DataLen) == 0 );

        // Restore previous data
        memcpy(ptr, s_Data, s_DataLen);
        // Flushing data to disk at memory unmapping in the destructor
    }}
    {{
        // Checking the previous flush
        CMemoryFile m1(s_FileName);
        assert( m1.GetSize() == s_DataLen );
        assert( memcmp(m1.GetPtr(), s_Data, s_DataLen) == 0 );
    }}

    // eMMP_ReadWrite + eMMS_Private
    {{
        // Map file into memory
        CMemoryFile m1(s_FileName, CMemoryFile::eMMP_ReadWrite,
                       CMemoryFile::eMMS_Private);
        assert( m1.GetSize() == s_DataLen );
        assert( memcmp(m1.GetPtr(), s_Data, s_DataLen) == 0 );
        char* ptr = (char*)m1.GetPtr();
        *ptr = '@';
        assert( m1.Flush() );

        // Map second copy of the file into the memory
        CMemoryFile m2(s_FileName, CMemoryFile::eMMP_Read,
                       CMemoryFile::eMMS_Shared);
        assert( m1.GetSize() == m2.GetSize() );
        // m2 must contain the original data
        assert( memcmp(m2.GetPtr(), s_Data, s_DataLen) == 0 );
    }}

    // Length and offset test
    {{
        // Map file into memory
        CMemoryFile m1(s_FileName, CMemoryFile::eMMP_ReadWrite,
                       CMemoryFile::eMMS_Shared, 2, 3);
        assert( m1.GetFileSize() == s_DataLen );
        assert( m1.GetSize() == 3 );
        assert( m1.GetOffset() == 2 );

        // Map second copy of the file into the memory
        CMemoryFile m2(s_FileName, CMemoryFile::eMMP_Read,
                       CMemoryFile::eMMS_Shared);

        // Change something in first copy
        char* ptr = (char*)m1.GetPtr();
        memcpy(ptr,"en ", 3);
        assert( m1.Flush() );
        assert( m2.GetSize() == s_DataLen );

        // m2 must contain changes made via m1 
        assert( memcmp(m2.GetPtr(), s_DataTest, s_DataLen) == 0 );

        // Restore previous data
        memcpy(ptr, s_Data, s_DataLen);
        // Flushing data to disk at memory unmapping in the destructor
    }}

    // Remove the file
    assert( f.Remove() );
  
    // CMemoryFileMap test  
    {{
        static const size_t s_PartCount  = 10;
        static const size_t s_BufLen     = s_PartCount * 1024;

        // Generate random file with size s_FileLen;
        char* buf = new char[s_BufLen];
        assert(buf);

        unsigned int seed = (unsigned int)time(0);
        srand(seed);
        for (size_t i=0; i<s_BufLen; i++) {
            buf[i] = (char)((double)rand()/RAND_MAX*255);
        }

        CNcbiOfstream fs(s_FileName, ios::out | ios::binary);
        fs.write(buf, s_BufLen);
        assert(fs.good());
        fs.close();
        assert(f.GetLength() == s_BufLen);

        // Map some segments of the file
        CMemoryFileMap m(s_FileName, CMemoryFile::eMMP_ReadWrite,
                         CMemoryFile::eMMS_Shared);
    
        char* ptr = (char*)m.Map(0,s_BufLen);
        assert( m.GetSize(ptr) == s_BufLen );
        char* ptrs[s_PartCount];

        // Map segments
        for (size_t i=0; i<s_PartCount; i++) {
            char* p = (char*)m.Map(i*1024,1024);
            assert( p );
            assert( m.GetSize(p) == 1024 );
            ptrs[i] = p;
        }

        // Write to mapped file by one pointer and read from another
        for (size_t i=0; i<s_PartCount; i++) {
            memcpy(ptr + i*1024, s_Data, s_DataLen);
            assert( memcmp(ptrs[i], s_Data, s_DataLen) == 0 );
        }

        // Unmap all even segments (0,2,4...)
        for (size_t i=0; i<s_PartCount; i+=2) {
            assert( m.Flush(ptrs[i]) );
            assert( m.Unmap(ptrs[i]) );
        }
        // Unmap all other segments
        assert( m.UnmapAll() );
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
    // CSymLink
    s_TEST_Link();

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
 * Revision 1.45  2005/03/22 14:22:32  ivanov
 * Added CSymLink test. Added tests for Copy/Rename methods.
 * Dropped MacOS 9 support.
 *
 * Revision 1.44  2005/01/31 11:50:29  ivanov
 * Added tests for CMaskFileName. Some cosmetics.
 *
 * Revision 1.43  2004/12/22 12:27:35  ivanov
 * Removed redundant variable declaration
 *
 * Revision 1.42  2004/12/14 17:50:28  ivanov
 * CDir::Create(): return TRUE if creating directory already exists
 *
 * Revision 1.41  2004/10/08 12:44:54  ivanov
 * Added more tests for CDirEntry::NormalizePath()
 *
 * Revision 1.40  2004/08/03 12:05:57  ivanov
 * Added test for CMemoryFileMap.
 *
 * Revision 1.39  2004/07/28 15:49:27  ivanov
 * Changed s_TEST_MemoryFile() accordingly last changes in the CMemoryFile,
 * added some new tests.
 *
 * Revision 1.38  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.37  2004/04/29 15:15:00  kuznets
 * Modifications of FindFile test
 *
 * Revision 1.36  2004/03/17 15:41:28  ivanov
 * Expanded temporary file name generation test
 *
 * Revision 1.35  2004/01/05 22:10:33  gorelenk
 * += UNIX test for CDirEntry::CreateRelativePath()
 *
 * Revision 1.34  2004/01/05 21:41:26  gorelenk
 * += Exception throwing in CDirEntry::CreateRelativePath()
 *
 * Revision 1.33  2004/01/05 20:05:06  gorelenk
 * + Test for CDirEntry::CreateRelativePath()
 *
 * Revision 1.32  2003/12/15 15:42:48  ivanov
 * Removed incorrect last access time check
 *
 * Revision 1.31  2003/12/01 12:17:35  ivanov
 * Fixed comparison operator for assert( atime >= ctime )
 *
 * Revision 1.30  2003/11/28 16:23:42  ivanov
 * Added tests for CDirEntry::SetTime()
 *
 * Revision 1.29  2003/11/05 16:27:45  kuznets
 * + test for FindFile
 *
 * Revision 1.28  2003/11/05 15:38:14  kuznets
 * Added test for new GetEntries()
 *
 * Revision 1.27  2003/10/08 15:45:53  ivanov
 * Added tests for [Add|Delete]TrailingPathSeparator()
 *
 * Revision 1.26  2003/09/30 15:09:05  ucko
 * Reworked CDirEntry::NormalizePath, which now handles .. correctly in
 * all cases and optionally resolves symlinks (on Unix).
 *
 * Revision 1.25  2003/09/16 18:55:16  ivanov
 * Added more test for NormalizePath()
 *
 * Revision 1.24  2003/09/16 16:28:08  ivanov
 * Added tests for SplitPath(). Minor cosmetics.
 *
 * Revision 1.23  2003/08/08 14:30:06  meric
 * Accommodate rename of CFile::GetTmpNameExt() to CFile::GetTmpNameEx()
 *
 * Revision 1.22  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.21  2003/03/06 21:22:30  ivanov
 * Rollback to R1.19 -- accidentally commit work version
 *
 * Revision 1.20  2003/03/06 21:12:35  ivanov
 * *** empty log message ***
 *
 * Revision 1.19  2003/02/14 19:26:51  ivanov
 * Added more checks for CreateTmpFile()
 *
 * Revision 1.18  2003/02/05 22:08:26  ivanov
 * Enlarged CMemoryFile test
 *
 * Revision 1.17  2002/06/29 06:45:50  vakatov
 * Get rid of some compilation warnings
 *
 * Revision 1.16  2002/06/07 16:11:38  ivanov
 * Chenget GetTime(): using CTime instead time_t, modification time by default
 *
 * Revision 1.15  2002/06/07 15:21:33  ivanov
 * Added CDirEntry::GetTime() test example
 *
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

