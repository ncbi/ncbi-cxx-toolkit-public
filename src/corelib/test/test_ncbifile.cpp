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
#include <corelib/ncbierror.hpp>
#include <stdio.h>
#include <limits.h>

// for getrlimit()
#if defined(NCBI_OS_UNIX)
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <unistd.h>
#endif       

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


// Create test file 
static void s_CreateTestFile(const string& file)
{
    CNcbiOfstream fs(file.c_str(), ios::out | ios::trunc | ios::binary);
    assert( fs.good() );
    fs << "test data";
    assert( fs.good() );
    fs.close();
}


//----------------------------------------------------------------------------
//  File name spliting tests
//----------------------------------------------------------------------------

static void s_TEST_SplitPath(void)
{
    string path, disk, dir, title, ext;
    
#if defined(NCBI_OS_MSWIN)
    CFile::SplitPath("c:\\dir\\subdir\\file.ext", &dir, &title, &ext);
    assert( dir   == "c:\\dir\\subdir\\" );
    assert( title == "file" );
    assert( ext   == ".ext" );
    path = CFile::MakePath("c:\\dir\\", "file.ext");
    assert( path == "c:\\dir\\file.ext" );

    CFile::SplitPath("c:file.ext", &dir, &title, &ext);
    assert( dir   == "c:" );
    assert( title == "file" );
    assert( ext   == ".ext" );
    path = CFile::MakePath("c:", "file", "ext");
    assert( path == "c:file.ext" );

    CFile::SplitPath("c:\\file.ext", &dir, &title, &ext);
    assert( dir   == "c:\\" );
    assert( title == "file" );
    assert( ext   == ".ext" );
    path = CFile::MakePath("c:\\", "file", "ext");
    assert( path == "c:\\file.ext" );
    path = CFile::MakePath("c:\\", "file", ".ext");
    assert( path == "c:\\file.ext" );

    {{
        CFile f("c:\\dir\\subdir\\file.ext");
        assert( f.GetDir()  == "c:\\dir\\subdir\\" );
        assert( f.GetName() == "file.ext" );
        assert( f.GetBase() == "file" );
        assert( f.GetExt()  == ".ext" );
    }}
    {{
        CFile f("file");
        assert( f.GetDir()  == ".\\" );
        assert( f.GetDir(CDirEntry::eIfEmptyPath_Current)  == ".\\" );
        assert( f.GetDir(CDirEntry::eIfEmptyPath_Empty)    == "" );
        assert( f.GetName() == "file" );
        assert( f.GetBase() == "file" );
    }}

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
    path = CFile::MakePath("/dir/subdir", "file", ".ext");
    assert( path == "/dir/subdir/file.ext" );

    {{
        CFile f("/usr/lib/any.other.lib");
        assert( f.GetDir()  == "/usr/lib/" );
        assert( f.GetName() == "any.other.lib" );
        assert( f.GetBase() == "any.other" );
        assert( f.GetExt()  == ".lib" );
    }}
    {{
        CFile f("file");
        assert( f.GetDir()  == "./" );
        assert( f.GetDir(CDirEntry::eIfEmptyPath_Current)  == "./" );
        assert( f.GetDir(CDirEntry::eIfEmptyPath_Empty)    == "" );
        assert( f.GetName() == "file" );
        assert( f.GetBase() == "file" );
    }}

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

    // SplitPathEx
    {{
        struct TSplitPathEx
        {
            string path, disk, dir, title, ext;
        };
        TSplitPathEx test[] = {
            { "c:file", "c:", "", "file",  "" },
            { "", "", "", "", "" },
            { "file.ext", "", "", "file", ".ext" },
            { "\\file.ext", "", "\\", "file", ".ext" },
            { "/file.ext", "", "/", "file", ".ext" },
            { "c:\\", "c:", "\\", "",  "" },
            { "c:\\windows\\win.ini", "c:", "\\windows\\", "win",  ".ini" },
            { "windows\\win.ini", "", "windows\\", "win",  ".ini" },
            { "/usr/lib/file.ext", "", "/usr/lib/", "file", ".ext" },
            { "usr/lib/file.ext", "", "usr/lib/", "file", ".ext" },
            { "c:file", "c:", "", "file",  "" },
            { "c:file.ext", "c:", "", "file",  ".ext" }
        };

        for (size_t i = 0;  i < sizeof(test) / sizeof(test[0]);  ++i) {
            CFile::SplitPathEx(test[i].path, &disk, &dir, &title, &ext);
            assert( disk  == test[i].disk );
            assert( dir   == test[i].dir );
            assert( title == test[i].title );
            assert( ext   == test[i].ext );
        }
    }}
}


//----------------------------------------------------------------------------
//  Path checking tests
//----------------------------------------------------------------------------

static void s_TEST_CheckPath(void)
{
    CDirEntry d;

    // IsAbsolutePath() test

    // Common cases
    assert(!d.IsAbsolutePath("file") );
    assert(!d.IsAbsolutePath("./file") );
    assert(!d.IsAbsolutePath("../file") );
    assert(!d.IsAbsolutePath("dir/file") );

    // See difference between Unix and MS Windows
#if defined(NCBI_OS_MSWIN)
    // MS Windows paths
    assert( d.IsAbsolutePath("c:\\") );
    assert( d.IsAbsolutePath("c:\\file") );
    assert(!d.IsAbsolutePath("c:file") );
    assert( d.IsAbsolutePath("\\\\machine\\dir") );
    assert(!d.IsAbsolutePath("\\file") );
    assert(!d.IsAbsolutePath(".\\file") );
    // Unix paths
    assert(!d.IsAbsolutePath("/") );
    assert(!d.IsAbsolutePath("/file") );
    assert(!d.IsAbsolutePath("/dir/dir") );

#elif defined(NCBI_OS_UNIX)
    // MS Windows paths
    assert(!d.IsAbsolutePath("c:\\") );
    assert(!d.IsAbsolutePath("c:\\file") );
    assert(!d.IsAbsolutePath("c:file") );
    assert(!d.IsAbsolutePath("\\\\machine\\dir") );
    assert(!d.IsAbsolutePath("\\file") );
    assert(!d.IsAbsolutePath(".\\file") );
    // Unix paths
    assert( d.IsAbsolutePath("/") );
    assert( d.IsAbsolutePath("/file") );
    assert( d.IsAbsolutePath("/dir/dir") );
#endif

    // Universal check for any OS
    assert( d.IsAbsolutePathEx("c:\\") );
    assert( d.IsAbsolutePathEx("c:\\file") );
    assert(!d.IsAbsolutePathEx("c:") );
    assert( d.IsAbsolutePathEx("\\\\machine\\dir") );
    assert(!d.IsAbsolutePathEx("file") );
    assert(!d.IsAbsolutePathEx(".\\file") );
    assert(!d.IsAbsolutePathEx("./file") );
    assert(!d.IsAbsolutePathEx("dir/file") );
    assert(!d.IsAbsolutePathEx("dir\\file") );
    assert(!d.IsAbsolutePathEx("dir/file") );
    // UNIX absolute path
    assert( d.IsAbsolutePathEx("/file") );
    // MS Windows relative path
    assert(!d.IsAbsolutePathEx("\\file") );


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
    assert( d.NormalizePath("\\\\?\\x")         == "x" );
    assert( d.NormalizePath("\\\\?\\UNC\\m\\d") == "\\\\m\\d" );
    assert( d.NormalizePath("dir/file")         == "dir\\file" );
    assert( d.NormalizePath("/")                == "\\" );
    assert( d.NormalizePath("\\")               == "\\" );
    assert( d.NormalizePath("\\.\\")            == "\\" );
    assert( d.NormalizePath("\\..\\")           == "\\" );
    assert( d.NormalizePath(".")                == "." );
    assert( d.NormalizePath(".\\")              == "." );
    assert( d.NormalizePath(".\\.")             == "." );
    assert( d.NormalizePath(".\\dir\\..")       == "." );
    assert( d.NormalizePath("dir\\..")          == "." );
    assert( d.NormalizePath("dir\\..\\")        == "." );
    
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
    assert( d.NormalizePath(".")                == "." );
    assert( d.NormalizePath("./")               == "." );
    assert( d.NormalizePath("./.")              == "." );
    assert( d.NormalizePath("./dir/..")         == "." );
    assert( d.NormalizePath("dir/..")           == "." );
    assert( d.NormalizePath("dir/../")          == "." );
#endif


    // Convert path to OS dependent test

    assert( d.ConvertToOSPath("")               == "" );
    assert( d.ConvertToOSPath("c:\\file")       == "c:\\file" );
    assert( d.ConvertToOSPath("/dir/file")      == "/dir/file" );
#if defined(NCBI_OS_MSWIN)
    assert( d.ConvertToOSPath("dir")            == "dir" );
    assert( d.ConvertToOSPath("dir\\file")      == "dir\\file" );
    assert( d.ConvertToOSPath("dir/file")       == "dir\\file" );
    assert( d.ConvertToOSPath("./dir/file")     == "dir\\file" );
    assert( d.ConvertToOSPath("../file")        == "..\\file" );
    assert( d.ConvertToOSPath("../../file")     == "..\\..\\file" );
#elif defined(NCBI_OS_UNIX)
    assert( d.ConvertToOSPath("dir")            == "dir" );
    assert( d.ConvertToOSPath("dir\\file")      == "dir/file" );
    assert( d.ConvertToOSPath("dir/file")       == "dir/file" );
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
    assert( d.ConcatPathEx("dir\\dir", "file")  == "dir\\dir\\file" );
    assert( d.ConcatPathEx("dir/dir", "file")   == "dir/dir/file" );
}


//----------------------------------------------------------------------------
//  File name maching test
//----------------------------------------------------------------------------

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


//----------------------------------------------------------------------------
//  String to permission mode conversion test
//----------------------------------------------------------------------------

struct SStrToModeTest
{
    CDirEntry::TSpecialModeBits special; // special bits
    CDirEntry::TMode user;               // user permissions
    CDirEntry::TMode group;              // group permissions
    CDirEntry::TMode other;              // other permissions
    const char* str_list;                // eModeFormat_List (check string is the same)
    const char* str_octal;               // eModeFormat_Octal
    const char* str_octal_check;         // eModeFormat_Octal check string
    const char* str_symbolic;            // eModeFormat_Symbolic
    const char* str_symbolic_check;      // eModeFormat_Symbolic check string
};

static const SStrToModeTest s_StrToMode[] = {
    // mode    list           octal  octal_c   symbolic             symbolic check
    { 0,0,0,1, "--------x",     "1",   "001",  "o=x",               "u=,g=,o=x"         },
    { 0,0,3,0, "----wx---",    "30",   "030",  "g=wx",              "u=,g=wx,o="        },
    { 0,0,6,6, "---rw-rw-",    "66",   "066",  "go=rw",             "u=,g=rw,o=rw"      },
    { 0,0,4,2, "---r---w-",    "42",   "042",  "g=r,o=w",           "u=,g=r,o=w"        },
    { 0,6,4,2, "rw-r---w-",  "0642",   "642",  "u=rw,g=r,o=w",      "u=rw,g=r,o=w"      },
    { 0,6,4,4, "rw-r--r--",   "644",   "644",  "u=rw,go=r",         "u=rw,g=r,o=r"      },
    { 0,7,7,7, "rwxrwxrwx",   "777",   "777",  "u=rwx,g=rwx,o=rwx", "u=rwx,g=rwx,o=rwx" },
    { 0,7,7,5, "rwxrwxr-x",   "775",   "775",  "ug=rwx,o=rx",       "u=rwx,g=rwx,o=rx"  },
    { 0,7,0,0, "rwx------",   "700",   "700",  "u=rwx",             "u=rwx,g=,o="       },
    { 0,7,0,1, "rwx-----x",   "701",   "701",  "u=rwx,o=x",         "u=rwx,g=,o=x"      },
    { 0,7,0,5, "rwx---r-x",   "705",   "705",  "u=rwx,o=rx",        "u=rwx,g=,o=rx"     },
    { 0,7,5,4, "rwxr-xr--",   "754",   "754",  "u=rwx,g=rx,o=r",    "u=rwx,g=rx,o=r"    },
    { 1,7,7,5, "rwxrwxr-t",  "1775",  "1775",  "ug=rwx,o=rt",       "u=rwx,g=rwx,o=rt"  },
    { 4,7,7,5, "rwsrwxr-x",  "4775",  "4775",  "u=rws,g=rwx,o=rx",  "u=rws,g=rwx,o=rx"  },
    { 7,7,7,5, "rwsrwsr-t",  "7775",  "7775",  "ug=rws,o=rt",       "u=rws,g=rws,o=rt"  },
    { 4,7,0,0, "rws------",  "4700",  "4700",  "u=rws",             "u=rws,g=,o="       },
    { 4,6,0,0, "rwS------",  "4600",  "4600",  "u=rwS",             "u=rwS,g=,o="       },
    { 2,0,7,0, "---rws---",  "2070",  "2070",  "g=rws",             "u=,g=rws,o="       },
    { 2,0,6,0, "---rwS---",  "2060",  "2060",  "g=rwS",             "u=,g=rwS,o="       },
    { 1,0,0,5, "------r-t",  "1005",  "1005",  "o=rt",              "u=,g=,o=rt"        },
    { 1,0,0,4, "------r-T",  "1004",  "1004",  "o=rT",              "u=,g=,o=rT"        },
    { 7,7,4,1, "rwsr-S--t",  "7741",  "7741",  "u=rws,g=rS,o=t",    "u=rws,g=rS,o=t"    },
    { 0,0,0,0, NULL, NULL, NULL, NULL, NULL }
};

static void s_TEST_StrToMode(void)
{
    for (int i = 0; s_StrToMode[i].str_list; ++i) {
        SStrToModeTest test = s_StrToMode[i];
        CDirEntry::TMode u, g, o, s;
        string str;
        // Octal (default)
        {{
            assert(CDirEntry::StringToMode(test.str_octal, &u, &g, &o, &s));
            assert(test.user    == u);
            assert(test.group   == g);
            assert(test.other   == o);
            assert(test.special == s);
            str = CDirEntry::ModeToString(u, g, o, s /*, CDirEntry::eModeFormat_Octal*/);
            assert(str == test.str_octal_check);
        }}
        // List
        {{
            assert(CDirEntry::StringToMode(test.str_octal, &u, &g, &o, &s));
            assert(test.user    == u);
            assert(test.group   == g);
            assert(test.other   == o);
            assert(test.special == s);
            str = CDirEntry::ModeToString(u, g, o, s, CDirEntry::eModeFormat_List);
            assert(str == test.str_list);
        }}
        // Symbolic
        {{
            assert(CDirEntry::StringToMode(test.str_symbolic, &u, &g, &o, &s));
            assert(test.user    == u);
            assert(test.group   == g);
            assert(test.other   == o);
            assert(test.special == s);
            str = CDirEntry::ModeToString(u, g, o, s, CDirEntry::eModeFormat_Symbolic);
            assert(str == test.str_symbolic_check);
        }}
    }
}



//----------------------------------------------------------------------------
//  Work with files
//----------------------------------------------------------------------------

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
        if ( f2.Exists() ) {
            f2.Remove();
        }

        // errors
        f2.GetLength();
        CNcbiError err( CNcbiError::GetLast() );
        assert( err );
        cout << "Expected error " << err << endl;
        if ( f3.Exists() ) {
            f3.Remove();
        }
        f2.Copy(f3.GetPath());
        err = CNcbiError::GetLast();
        assert( err );
        cout << "Expected error " << err << endl;

        s_CreateTestFile(f2.GetPath());
        assert( !f.IsNewer(f2.GetPath(), 0) );
        f2.Remove();
        assert( f2.GetLength() == -1);
        assert( f.IsNewer(f2.GetPath(), CDirEntry::fHasThisNoPath_Newer) );
        assert( f2.IsNewer(f.GetPath(), CDirEntry::fNoThisHasPath_Newer) );
        assert( !f.IsNewer(f2.GetPath(), CDirEntry::fHasThisNoPath_NotNewer) );
        assert( f3.IsNewer(f2.GetPath(), CDirEntry::fNoThisNoPath_Newer) );

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
        assert( CNcbiError::GetLast() );
        cout << "Expected error " << CNcbiError::GetLast() << endl;
        assert( CFile("file1").Exists() );
        assert( f2.Exists() );

        assert( f1.Rename(f2.GetPath(), CFile::fRF_Overwrite) );
        assert( !CFile("file1").Exists() );
        assert( f1.Exists() );
        assert( f2.Exists() );

        assert( f2.Rename(f3.GetPath(), CFile::fRF_Backup) );
        assert( f2.Exists() );
        assert( f3.Exists() );
        assert( CFile("file3" + string(f3.GetBackupSuffix())).Exists() );
        assert( CFile("file3" + string(f3.GetBackupSuffix())).Remove() );

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
        CTime mtime, atime, ctime;
        assert( f.GetTime(&mtime, &atime, &ctime) );
        cout << "File creation time     : " << ctime.AsString() << endl;
        cout << "File modification time : " << mtime.AsString() << endl;
        cout << "File last access time  : " << atime.AsString() << endl;
        assert( f.GetTime(&mtime, &atime) );
        CTime mtime_new(mtime), atime_new(atime);
        // Account daylight saving time for file local times
        mtime_new.SetTimeZonePrecision(CTime::eDay);
        atime_new.SetTimeZonePrecision(CTime::eDay);
        mtime_new.AddDay(-2);
        atime_new.AddDay(-1);
        assert( f.SetTime(&mtime_new, &atime_new) );
        assert( f.GetTime(&mtime, &atime, &ctime) );
        cout << "File creation time     : " << ctime.AsString() << endl;
        cout << "File modification time : " << mtime.AsString() << endl;
        cout << "File last access time  : " << atime.AsString() << endl;
        // Use 1 hour to compare modification times of the file,
        // because mtime_new and SetTime() can be affected by
        // Daylight Saving Time changes.
        assert(mtime - mtime_new <= CTimeSpan(3600,0));
        
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


//----------------------------------------------------------------------------
//  Work with directories
//----------------------------------------------------------------------------

static void s_TEST_Dir(void)
{
    // Delete the directory if it exists before we start testing
    CDirEntry("dir1").Remove();

    // errors
    CDir::SetCwd("dir1");
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
    {
        CDir::TEntries contents = CDir("dir1").GetEntries("*.*");
        assert ( CNcbiError::GetLast() );
        cout << "Expected error " << CNcbiError::GetLast() << endl;
    }
    CDir("???").Create();
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
    CDir("???").Remove();
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
    CDir("???").Copy("*");
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
    
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
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
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
    assert ( CNcbiError::GetLast() );
    cout << "Expected error " << CNcbiError::GetLast() << endl;
    assert( CFile("dir2/file1").Remove() );
    assert( CDir("dir2").Remove(CDir::eOnlyEmpty) );
    assert( CDir("dir3").Remove() );

    // Check the directory exists
    assert( !CDir("dir1").Exists() );
    assert( !CDir("dir2").Exists() );
    assert( !CDir("dir3").Exists() );

    const string kFFTestPath = "..";
    CDir dir(kFFTestPath);

    // Current directory list
    {{
        cout << endl;
        CDir::TEntries contents = dir.GetEntries("*", CDir::fIgnoreRecursive);
        ITERATE(CDir::TEntries, i, contents) {
            string entry = (*i)->GetPath();
            cout << entry << endl;
        }
        cout << endl;

        // The same but using vector of masks
        vector<string> masks;
        masks.push_back("*");
        CDir::TEntries contents2 = dir.GetEntries(masks, CDir::fIgnoreRecursive);
        assert(contents.size() == contents2.size());

        vector<string> files;
        vector<string> paths;
        paths.push_back(kFFTestPath);

        FindFiles(files, paths.begin(), paths.end(), 
                         masks.begin(), masks.end());
        assert(contents.size() == contents2.size());
        assert( files.size() == contents2.size() );
        for (unsigned int i = 0; i < files.size(); ++i) {
            const CDirEntry& entry1 = *contents.front();
            const CDirEntry& entry2 = *contents2.front();
            string ep1 = entry1.GetPath();
            string ep2 = entry2.GetPath();
            const string& f = files[i];
            assert(ep1 == ep2);
            assert(ep1 == f);
            contents.pop_front();
            contents2.pop_front();
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
        assert ( CNcbiError::GetLast() );
        cout << "Expected error " << CNcbiError::GetLast() << endl;
        assert( dir.Exists() );

        assert( !dir.Remove(CDir::eNonRecursive) );
        assert ( CNcbiError::GetLast() );
        cout << "Expected error " << CNcbiError::GetLast() << endl;
        assert( dir.Exists() );
        assert( CDir("dir3/subdir1").Exists() );
        assert( CFile("dir3/subdir1/file").Exists() );

        assert( dir.Remove(CDir::eRecursive) );
        assert( !dir.Exists() );
    }}
        
    // Home dir
    {{
        string homedir = CDir::GetHome();
        assert ( !homedir.empty() );
        cout << "Home dir: " << homedir << endl;
    }}

    // Create relative path from 2 absolute paths:
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

        // Expect an exceptions
        try {
            assert( CDirEntry::CreateRelativePath(
                    "\\x\\y\\z\\", 
                    "\\x\\y\\") == "..\\" );
        } catch (CFileException&) { }
        try {
            assert( CDirEntry::CreateRelativePath(
                    "C:\\x\\",
                    "D:\\y\\") == "impossible" );
        } catch (CFileException&) { }

#elif defined(NCBI_OS_UNIX)
        assert( CDirEntry::CreateRelativePath(
                "/usr/bin/", "/usr/" ) == "../" );
        assert( CDirEntry::CreateRelativePath(
                "/usr/bin/", "/etc/" ) == "../../etc/" );
#endif
    }}

    // Creation of absolute path from relative path
    {{
        string cwd = CDir::GetCwd();
        assert( CDirEntry::CreateAbsolutePath("")   == cwd );
        assert( CDirEntry::CreateAbsolutePath(".")  == cwd );
#if defined(NCBI_OS_MSWIN)
        assert( CDirEntry::CreateAbsolutePath("C:\\path\\") == "C:\\path\\" );
#elif defined(NCBI_OS_UNIX)
        assert( CDirEntry::CreateAbsolutePath("/usr/path") == "/usr/path" );
#endif
    }}
}


//----------------------------------------------------------------------------
//  Work with symbolic links
//----------------------------------------------------------------------------

static void s_TEST_Link(void)
{
#if defined(NCBI_OS_UNIX)
    {{
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
        assert( !link.Copy("link2") );
        assert( CNcbiError::GetLast());
        cout << "Expected error " << CNcbiError::GetLast() << endl;
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
    }}
    
    // DereferencePath() test
    {{
        // Create structure
        assert( CDir("dir").Create() );
        assert( CDir("dir/dir1").Create() );
        assert( CSymLink("dir/dir2").Create("dir1") );
        assert( CDir("dir/dir2/entry").Create() );
        assert( CSymLink("dir/dir1/link").Create("entry") );
        
        // Compare DereferenceLink() and DereferencePath()
        CDirEntry e("dir/dir2/link");
        e.DereferenceLink();
        assert( e.GetPath() == "dir/dir2/entry");
        e.Reset("dir/dir2/link");
        e.DereferencePath();
        assert( e.GetPath() == "dir/dir1/entry");
        
        // Cleanup
        assert( CDir("dir").Remove(CDir::eRecursive) );
        assert( !CDir("dir").Exists() );
    }}
#endif
}


//----------------------------------------------------------------------------
// Memory mapping
//----------------------------------------------------------------------------

static void s_TEST_MemoryFile(void)
{
    static const char   s_FileName[] = "test.tmp";
    static const char   s_Data[]     = "test data";
    static const char   s_DataTest[] = "teen data";
    static const size_t s_DataLen    = sizeof(s_Data) - 1;

    CFile f(s_FileName);
    f.Remove();

    // Create empty test file 
    {{
        assert( !f.Exists() );
        CNcbiOfstream fs(s_FileName, ios::out | ios::binary);
        assert( fs.good() );
        fs.close();
        // Check if the file exists now and it size is 0
        assert( f.Exists() );
        assert( f.GetLength() == 0 );
    }}

    // Common file checks
    {{
        // Map absent file in open mode, expect an exception.
        try {
            CMemoryFile m("some_absent_file_name");
            _TROUBLE;
        } catch (CFileException&) { }
        {{
            // Map empty file.
            // Do not expect an exception here (special case)
            CMemoryFile m(s_FileName);
            assert( m.GetPtr() == 0);
            assert( m.GetSize() == 0);

            // Create new file
            CMemoryFile m1(s_FileName, CMemoryFile::eMMP_ReadWrite,
                        CMemoryFile::eMMS_Shared, 0, s_DataLen,
                        CMemoryFile::eCreate, s_DataLen * 2);
            assert( f.GetLength() == s_DataLen * 2 );

            // Extend file size of existent file
            CMemoryFile m2(s_FileName, CMemoryFile::eMMP_ReadWrite,
                        CMemoryFile::eMMS_Shared, 0, s_DataLen,
                        CMemoryFile::eExtend, s_DataLen * 3);
            assert( f.GetLength() == s_DataLen * 3 );

            // Full unmapping is necessary to reduce file size.
        }}
        // Rewrite old file.
        CMemoryFile m3(s_FileName, CMemoryFile::eMMP_ReadWrite,
                    CMemoryFile::eMMS_Shared, 0, s_DataLen,
                    CMemoryFile::eCreate, 0);
        assert( f.GetLength() == 0 );
    }}

    // Write data into out test file
    {{
        CNcbiOfstream fs(s_FileName, ios::out | ios::binary);
        fs.write(s_Data, s_DataLen);
        assert( fs.good() );
        fs.close();
        // Check if the file exists now
        assert( f.Exists() );
        assert( f.GetLength() == (Int8)s_DataLen );
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
        assert( m1.GetFileSize() == (Int8)s_DataLen );
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
        memcpy(ptr, s_Data + m1.GetOffset(), m1.GetSize());
        // Flushing data to disk at memory unmapping in the destructor
    }}

    // Extend() test
    {{
        off_t offset = 2;
        CMemoryFile m(s_FileName, CMemoryFile::eMMP_ReadWrite,
                      CMemoryFile::eMMS_Shared, offset, 3);
        assert( m.GetFileSize() == (Int8)s_DataLen );
        assert( m.GetSize() == 3 );
        assert( m.GetOffset() == offset );

        // Extend() to the end of file
        char* p1 = (char*)m.Extend();
        assert( m.GetFileSize() == (Int8)s_DataLen );
        assert( m.GetSize() == s_DataLen - offset );
        assert( m.GetOffset() == offset );
        assert( memcmp(s_Data + offset, p1, m.GetSize()) == 0 );
        
        // Extend() with increasing file size
        char* p2 = (char*)m.Extend(1000);
        assert( m.GetFileSize() == 1000 + offset );
        assert( m.GetSize() == 1000 );
        assert( m.GetOffset() == offset );
        assert( memcmp(s_Data + offset, p2, 4) == 0 );
    }}

    // Remove test file
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
        assert( fs.good() );
        fs.close();
        assert( f.GetLength() == (Int8)s_BufLen );

        // Map some segments of the file
        CMemoryFileMap m(s_FileName, CMemoryFile::eMMP_ReadWrite,
                         CMemoryFile::eMMS_Shared);
    
        char* ptr = (char*)m.Map(0,s_BufLen);
        assert( m.GetSize(ptr) == s_BufLen );
        char* ptrs[s_PartCount];

        // Map segments
        for (size_t i=0; i<s_PartCount; i++) {
            char* p = (char*)m.Map((off_t)i*1024,1024);
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
        delete [] buf;
    }}
    
#if defined(NCBI_OS_UNIX)
    // Check mapping file with file decriptor == 0
    {{
        ::fclose(stdin);
        // Next used file descriptor possible will be 0
        CMemoryFile m(s_FileName, CMemoryFile::eMMP_ReadWrite,
                        CMemoryFile::eMMS_Shared, 0, s_DataLen,
                        CMemoryFile::eCreate, s_DataLen);
        char* p = (char*)m.GetPtr();
        assert( p );
    }}
#endif
    // Remove the file
    assert( f.Remove() );
}


//----------------------------------------------------------------------------
//  FileIO API test
//----------------------------------------------------------------------------

static void s_TEST_FileIO(void)
{
    const char*  filename = "test_fio.tmp";
    const char   data[]   = "test data";
    const size_t data_len = sizeof(data) - 1;
    char         buf[100];
    size_t       n;

    CFileIO fio;
    CFile   f(filename);

    // Check that file doesn't exists
    assert( !f.Exists() );

    // Try to open file, should have exception here,
    // because the file do not exists
    try {
        fio.Open(filename, CFileIO::eOpen, CFileIO::eRead);
        _TROUBLE;
    } catch (CFileException&) { }
    try {
        fio.Open(filename, CFileIO::eTruncate, CFileIO::eWrite);
        _TROUBLE;
    } catch (CFileException&) { }

    // Create test file 
    {{
        fio.Open(filename, CFileIO::eCreate, CFileIO::eWrite);
        assert( f.Exists() );
        n = fio.Write(data, data_len);
        assert( n == data_len );
        try {
            // We opened file for write only
            n = fio.Read(buf, sizeof(buf));
            _TROUBLE;
        } catch (CFileException&) { }
        fio.Close();
        // Check if the file exists now
        assert( f.Exists() );
        assert( f.GetLength() == (Int8)data_len );
    }}

    // Try to create new file, should have exception here,
    // because file already exists
    try {
        fio.Open(filename, CFileIO::eCreateNew, CFileIO::eWrite);
        _TROUBLE;
    } catch (CFileException&) { }

    // eOpenAlways
    {{
        // Open existing file
        fio.Open(filename, CFileIO::eOpenAlways, CFileIO::eWrite);
        assert( f.Exists() );
        n = fio.Write(data, data_len);
        assert( n == data_len );
        fio.Close();
        assert( f.GetLength() == (Int8)data_len );
        // Remove file
        assert( f.Remove() );
        assert( !f.Exists() );
        // Open new file
        fio.Open(filename, CFileIO::eOpenAlways, CFileIO::eWrite);
        assert( f.Exists() );
        n = fio.Write(data, data_len);
        assert( n == data_len );
        fio.Close();
        assert( f.GetLength() == (Int8)data_len );
    }}

    // Recreate file with RW permissions
    {{
        assert( f.Remove() );
        fio.Open(filename, CFileIO::eCreate, CFileIO::eReadWrite);
        n = fio.Write(data, data_len);
        assert( n == data_len );
        fio.Close();
        // Check if the file exists now
        assert( f.Exists() );
        assert( f.GetLength() == (Int8)data_len );
    }}

    // Open test file and read from it
    {{
        fio.Open(filename, CFileIO::eOpen, CFileIO::eRead);
        n = fio.Read(buf, sizeof(buf));
        assert( n == data_len );
        assert( memcmp(buf, data, data_len) == 0 );
        try {
            // We opened file for reading only
            n = fio.Write(data, data_len);
            _TROUBLE;
        } catch (CFileException&) { }
        fio.Close();
    }}

    // Truncate existent file
    {{
        fio.Open(filename, CFileIO::eTruncate, CFileIO::eWrite);
        fio.Close();
        assert( f.Exists() );
        assert( f.GetLength() == 0 );
    }}

    // File seek test
    {{
        fio.Open(filename, CFileIO::eCreate, CFileIO::eReadWrite);
        assert( fio.GetFilePos() == 0 );

        // Write data
        n = fio.Write(data, data_len);
        assert( n == data_len );
        assert( fio.GetFilePos() == data_len );

        // Go to begin of the file and read written data
        fio.SetFilePos(0, CFileIO::eBegin);
        assert( fio.GetFilePos() == 0 );
        n = fio.Read(buf, sizeof(buf));
        assert( n == data_len );
        assert( memcmp(buf, data, data_len) == 0 );
        assert( fio.GetFilePos() == data_len );

        // Change position in the file
        fio.SetFilePos(-1, CFileIO::eCurrent);
        assert( fio.GetFilePos() == (data_len - 1) );

        // Write more data
        n = fio.Write(data, data_len);
        assert( n == data_len );
        assert( fio.GetFilePos() == (data_len*2-1) );

        // Go back and read data again
        off_t ofs = data_len;
        fio.SetFilePos(-ofs, CFileIO::eEnd);
        n = fio.Read(buf, sizeof(buf));
        assert( n == data_len );
        assert( memcmp(buf, data, data_len) == 0 );
        assert( fio.GetFilePos() == (data_len*2-1) );

        // Set position beyond of the EOF -- it is allowed
        fio.SetFilePos(100, CFileIO::eBegin);
        // and write '\0' here
        n = fio.Write("", 1);
        assert( n == 1 );
        fio.Close();
        // File size must increase
        assert( f.GetLength() == 101 );
    }}

    // SetFileSize() test
    {{
        fio.Open(filename, CFileIO::eOpen, CFileIO::eReadWrite);
        assert( fio.GetFilePos() == 0 );
        // Extend file
        fio.SetFileSize(200, CFileIO::eEnd);
        assert( fio.GetFilePos() == 200 );
        assert( f.GetLength() == 200 );
        // Truncate file
        fio.SetFileSize(50);
        assert( fio.GetFilePos() == 200 );
        assert( f.GetLength() == 50 );
        // Extend file again
        fio.SetFilePos(50, CFileIO::eBegin);
        fio.SetFileSize(100 /*, CFileIO::eCurrent */);
        assert( fio.GetFilePos() == 50 );
        assert( f.GetLength() == 100 );
        // Close file
        fio.Close();
    }}

    // Remove test file
    assert( f.Remove() );
}


static void s_TEST_FileIO_LargeFiles(void)
{
    const Uint8  kSize_1GB = NCBI_CONST_UINT8(1*1024*1024*1024);
    const Uint8  kSize_5GB = NCBI_CONST_UINT8(5*1024*1024*1024);
    const Uint8  kSize_6GB = NCBI_CONST_UINT8(6*1024*1024*1024);
    const size_t kSize_Buf = 64*1024;

    CFileUtil::SFileSystemInfo info;
    CFileUtil::GetFileSystemInfo(".", &info);
    
    // Check free space
    assert(info.free_space > kSize_6GB);

    // Check file system limits
    cout << endl;
    cout << "File system type = " << info.fs_type << endl;
#if defined(NCBI_OS_MSWIN)
    assert(info.fs_type == CFileUtil::eNTFS);
#elif defined(NCBI_OS_UNIX)
    rlimit rl;
    assert(getrlimit(RLIMIT_FSIZE, &rl) == 0);
    cout << "File size limits = (" << rl.rlim_cur << ", " 
                                   << rl.rlim_max << ")" << endl;
    assert((Uint8)rl.rlim_cur > kSize_6GB);
#endif
    cout << endl;

    const char*  filename = "test_fio.tmp";
    char         buf_src[kSize_Buf];
    char         buf_cmp[kSize_Buf];
    size_t       n;

    // Init buffer size
    srand((unsigned)time(NULL));
    for (size_t i=0; i<kSize_Buf; i++) {
        buf_src[i] = (char)(rand() % sizeof(char));
    }

    CFileIO fio;
    CFile   f(filename);

    // Check that file doesn't exists
    assert( !f.Exists() );

    // Create new file
    fio.Open(filename, CFileIO::eCreate, CFileIO::eReadWrite);
    assert( fio.GetFilePos() == 0 );

    // Write file >4GB (5GB)
    for (size_t i = 0; i < (kSize_5GB / kSize_Buf); i++) {
        n = fio.Write(buf_src, kSize_Buf);
        assert(n == kSize_Buf);
    }
    assert( fio.GetFilePos()  == kSize_5GB);
    assert( fio.GetFileSize() == kSize_5GB);
    fio.Close();
    // We can check real file length via "file name" only after closing
    assert( f.GetLength() == (Int8)kSize_5GB );

    fio.Open(filename, CFileIO::eOpen, CFileIO::eReadWrite);
    assert( fio.GetFilePos() == 0 );

    // Read file and compare with buffer
    for (size_t i = 0; i < (kSize_5GB / kSize_Buf); i++) {
        // Read next kSize_Buf
        size_t pos = 0;
        do {
            n = fio.Read(buf_cmp + pos, kSize_Buf - pos);
            assert(n != 0);
            pos += n;
        } while (pos < kSize_Buf);
        // Compare buffers
        assert(pos == kSize_Buf);
        assert( memcmp(buf_cmp, buf_src, kSize_Buf) == 0 );
    }
    assert( fio.GetFilePos() == kSize_5GB );
    // EOF
    n = fio.Read(buf_cmp, kSize_Buf);
    assert(n == 0);
    assert( fio.GetFilePos() == kSize_5GB );

    // Extend file size to 6GB
    fio.SetFileSize(kSize_6GB, CFileIO::eEnd);
    assert( f.GetLength()     == (Int8)kSize_6GB );
    assert( fio.GetFileSize() == kSize_6GB );
    assert( fio.GetFilePos()  == kSize_6GB );
    // Move file position back (small and big values)
    fio.SetFilePos(kSize_6GB - 1, CFileIO::eBegin);
    assert( fio.GetFilePos() == kSize_6GB - 1 );
    fio.SetFilePos(-(Int8)kSize_5GB, CFileIO::eCurrent);
    assert( fio.GetFilePos() == kSize_1GB - 1 );

    // Shrink file to 100 bytes in size
    fio.SetFileSize(100);
    assert( fio.GetFileSize() == 100 );
    assert( f.GetLength() == 100 );
    fio.SetFilePos(0, CFileIO::eBegin);
    n = fio.Read(buf_cmp, kSize_Buf);
    assert(n == 100);
    assert( memcmp(buf_cmp, buf_src, n) == 0 );

    // Close file
    fio.Close();

    // Remove test file
    assert( f.Remove() );
}



//----------------------------------------------------------------------------
//  Test application
//----------------------------------------------------------------------------

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_All);
    
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_files",
                       "test file's accessory functions");
    d->AddFlag("largefiles",
               "Enable CFileIO test with files > 4Gb (turned off by default).");

    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

    if (args["largefiles"]) {
        // Run as separate test
        s_TEST_FileIO_LargeFiles();
    } else {
        // CDirEntry
        s_TEST_SplitPath();
        s_TEST_CheckPath();
        s_TEST_MatchesMask();
        s_TEST_StrToMode();
        // CFile
        s_TEST_File();
        // CDir
        s_TEST_Dir();
        // CSymLink
        s_TEST_Link();
        // CMemoryFile
        s_TEST_MemoryFile();
        // CFileIO
        s_TEST_FileIO();
    }
    NcbiCout << endl;
    NcbiCout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


int main(int argc, const char* argv[])
{
    // Enable FileAPI logging, if necessary
    // CFileAPI::SetLogging(eOn);
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
