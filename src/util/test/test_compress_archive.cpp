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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test/demo program for the Compression API (CArchive class)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <util/compress/archive.hpp>
#include <common/test_assert.h>  // This header must go last

USING_NCBI_SCOPE;


// Text to compress
const char* kStr = "This is a text to compress";


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
    void RunInternalTest(void);
    void RunInternalTestFormat(CArchive::EFormat format);
private:
    string GetDir(const string& dirname);
    void   CreateSourceTree(void);
    void   CreateFile(const string& filename, size_t offset, size_t size);
    void   PrintEntries(CArchive::TEntries* entries); 

private:
    string  m_TestDir;
    char*   m_Buf;
    size_t  m_BufLen;
};


void CTest::Init(void)
{
    const size_t kUsageWidth = 90;

    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    //SetDiagPostLevel(eDiag_Trace);
    DisableArgDescriptions(fDisableStdArgs);
    
    // Create command-line arguments
    auto_ptr<CCommandArgDescriptions> cmd(new 
        CCommandArgDescriptions(true, 0, CCommandArgDescriptions::eCommandMandatory | 
                                         CCommandArgDescriptions::eNoSortCommands));
    cmd->SetUsageContext(GetArguments().GetProgramBasename(),
        "Test compression library (CArchive class)");
    // test
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Run standard internal test.", false, kUsageWidth);
        arg->AddDefaultPositional("fmt", "Compression format to test", CArgDescriptions::eString, "all");
        arg->SetConstraint("fmt", &(*new CArgAllow_Strings, "all", "zip"));
        cmd->AddCommand("test", arg.release());
    }}
    SetupArgDescriptions(cmd.release());

    // Set base directory for file tests
    m_TestDir = CDir::GetCwd();
}


int CTest::Run(void)
{
    string cmd(GetArgs().GetCommand());
    if (cmd == "test") {
        RunInternalTest();
        _TRACE("\nTEST execution completed successfully!\n");
        return 0;
    }
    _TROUBLE;
    return 0;
}


void CTest::RunInternalTest(void)
{
    CArgs args = GetArgs();
    string test = args["fmt"].AsString();

    // Set a random starting point
    unsigned int seed = (unsigned int)time(0);
    //LOG_POST("Random seed = " << seed);
    srand(seed);

    // Preparing test data
    m_BufLen = 10*100*1024;  // 10MB
    AutoArray<char> src_buf_arr(m_BufLen);
    m_Buf = src_buf_arr.get();
    assert(m_Buf);
    for (size_t i=0; i<m_BufLen; i++) {
        m_Buf[i] = (char)(rand() % 255);
    }
    // Prepare source tree
    CreateSourceTree();

    // Run tests
    if (test == "all"  ||  test == "zip") {
        _TRACE("-------------- Zip ---------------\n");
        RunInternalTestFormat(CArchive::eZip);
    }

    // Cleanup
    CDir(GetDir("test_archive_in")).Remove();
    CDir(GetDir("test_archive_out")).Remove();

}


string CTest::GetDir(const string& dirname)
{
    return CDir::ConcatPath(m_TestDir, dirname);
}


void CTest::CreateSourceTree(void)
{
    // Main directory
    string basedir = GetDir("test_archive_in");
    assert(CDir(basedir).Create());

    // Files
    CreateFile(basedir + "/file0_1.txt",   0, 1000);
    CreateFile(basedir + "/file0_2.txt",  10, 2000);
    CreateFile(basedir + "/file0_3.bin",  20, 3000);
    CreateFile(basedir + "/file0_4.txt", 100, 4000);
    CreateFile(basedir + "/file0_5.bin", 200, 5000);
    CreateFile(basedir + "/file0_6.bin", 300, 6000);
    CreateFile(basedir + "/file0_7.txt", 700, 7000);
    CreateFile(basedir + "/file0_8.dat",  80, 8000);

    // Subdirectory
    assert(CDir(basedir + "/subdir1").Create());
    CreateFile(basedir + "/subdir1/file1_1.txt",   1, 1001);
    CreateFile(basedir + "/subdir1/file1_2.txt",  11, 2001);
    CreateFile(basedir + "/subdir1/file1_3.bin",  21, 3001);

    assert(CDir(basedir + "/subdir2").Create());
    CreateFile(basedir + "/subdir2/file2_1.txt",  12, 1002);
    CreateFile(basedir + "/subdir2/file2_2.txt",  12, 2002);
    CreateFile(basedir + "/subdir2/file2_3.bin",  22, 3002);
    CreateFile(basedir + "/subdir2/file2_4.dat", 102, 4002);
}


void CTest::CreateFile(const string& path, size_t offset, size_t size)
{
    CFileIO fio;
    fio.Open(path, CFileIO::eCreate, CFileIO::eReadWrite);
    assert((offset + size) <= m_BufLen);
    size_t n = fio.Write(m_Buf + offset, size);
    assert(n == size);
    fio.Close();
    assert(CFile(path).Exists());
}


void CTest::PrintEntries(CArchive::TEntries* entries)
{
    ITERATE(CArchive::TEntries, it, *entries) {
        cout << *it << endl;
    }
}


void s_TestFileArchive(CArchive::EFormat format,
                       const string& filename,
                       const CDir& src_dir,
                       const CDir& dst_dir)
{
    CArchiveFile a(format, filename);

    // Test
    a.Test();

    // Extract to specified directory (all files by default)
    a.SetBaseDir(dst_dir.GetPath());
    a.SetFlags(CArchive::fOverwrite | CArchive::fPreserveAll);
    auto_ptr<CArchive::TEntries> entries = a.Extract();
    assert(entries->size() == 14);

    // Check extracted entries
    vector<string> dir_entries;
    vector<string> paths;
    vector<string> masks;
    paths.push_back(dst_dir.GetPath());
    FindFiles(dir_entries, paths.begin(), paths.end(), 
                           masks.begin(), masks.end(), 
                           fFF_File | fFF_Dir | fFF_Recursive);
    assert(dir_entries.size() == 13);
/*
    ITERATE(vector<string>, fit, files) {
        cout << CDirEntry::NormalizePath(*fit) << endl;
    }
*/
    // Check some files

    CFile f(dst_dir.GetPath() + "/memory.txt");
    assert(f.GetType()   == CDirEntry::eFile);
    assert(f.GetLength() == (Int8)strlen(kStr));

    f.Reset(dst_dir.GetPath() + "/subdir1/file1_2.txt");
    assert(f.GetType()   == CDirEntry::eFile);
    assert(f.GetLength() == 2001);
    CFile fsrc(src_dir.GetPath()  + "/subdir1/file1_2.txt");
    assert(f.Compare(src_dir.GetPath() + "/subdir1/file1_2.txt"));

    // Remove directory with extracted files
    dst_dir.Remove();
}


size_t s_ExtractCallback(const CArchiveEntryInfo& /*info*/, const void* /*buf*/, size_t n)
{
    // Do nothing, just return 'SUCCESS' state.
    return n;
}


void CTest::RunInternalTestFormat(CArchive::EFormat format)
{
    // Source directory to compress
    CDir src(GetDir("test_archive_in"));
    // Destination directory for archive files
    CDir dst(GetDir("test_archive_out"));
    // Destination directory for extraction
    CDir dst_src(dst.GetPath() + "/src");

    assert(dst.Create());

    // Format-specific extension
    string ext;
    switch (format) {
    case CArchive::eZip:
        ext = ".zip";
    }

    CFileIO fio;
    CArchiveEntryInfo info;
    auto_ptr<CArchive::TEntries> entries;
    auto_ptr<CArchive::TEntries> add;

    //----------------------------------------------------------------------
    // File-based archive
    //----------------------------------------------------------------------

    string a_file = dst.GetPath() + "/file_archive" + ext;
    {{
        CArchiveFile a(format, a_file);
        // Create
        {{
            a.SetFlags(CArchive::fPreserveAll);
            a.Create();
            a.SetBaseDir(src.GetPath());

            // Append directory -- all files, except *.bin
            CMaskFileName mask;
            mask.AddExclusion("*.bin");
            a.SetMask(&mask);
            entries = a.Append(src.GetPath(), CCompression::eLevel_Default);
            assert(entries->size() == 12);

            // Append file.
            // And archive should have two files with the same name. 
            add = a.Append(src.GetPath() + "/subdir1/file1_2.txt",
                           CCompression::eLevel_Best, "file comment");
            assert(add->size() == 1);
            entries->splice(entries->end(), *add);

            // Append memory buffer
            add = a.AppendFileFromMemory("memory.txt", (void*)kStr, strlen(kStr),
                                         CCompression::eLevel_NoCompression,
                                         "memory buffer comment");
            assert(add->size() == 1);
            entries->splice(entries->end(), *add);
            size_t n = entries->size();
            assert(n == 14);

            // List entries
            a.UnsetMask();
            entries = a.List();
            assert(entries->size() == n);
            //PrintEntries(entries.get());

            // Check some entries
            CMaskFileName mask1;
            mask1.Add("memory*");
            a.SetMask(&mask1);
            entries = a.List();
            assert(entries->size() == 1);
            info = entries->front();
            assert(info.GetName() == "memory.txt");
            assert(info.GetType() == CDirEntry::eFile);
            assert(info.GetSize() == strlen(kStr));

            CMaskFileName mask2;
            mask2.Add("subdir1");
            a.UnsetMask(CArchive::eFullPathMask);
            a.SetMask(&mask2, eNoOwnership, CArchive::ePatternMask);
            entries = a.List();
            //PrintEntries(entries.get());
            assert(entries->size() == 4 /* dir entry + 3 files, 2 of them are the same */ );
            info = entries->front();
            assert(info.GetName() == "subdir1/");
            assert(info.GetType() == CDirEntry::eDir);
            assert(info.GetSize() == 0);
        }}
        // Test file
        s_TestFileArchive(format, a_file, src, dst_src);
    }}

    //----------------------------------------------------------------------
    // Memory-based archive
    //----------------------------------------------------------------------

    string a_memory = dst.GetPath() + "/memory_archive" + ext;
    {{
        CArchiveMemory a(format);
        // Create
        {{
            a.SetFlags(CArchive::fPreserveAll);
            a.Create();
            a.SetBaseDir(src.GetPath());

            // Append directory -- all files, except *.bin
            CMaskFileName mask;
            mask.AddExclusion("*.bin");
            a.SetMask(&mask);
            entries = a.Append(src.GetPath(), CCompression::eLevel_Default);
            assert(entries->size() == 12);

            // Append file.
            // And archive should have two files with the same name. 
            add = a.Append(src.GetPath() + "/subdir1/file1_2.txt",
                           CCompression::eLevel_Best, "file comment");
            assert(add->size() == 1);
            entries->splice(entries->end(), *add);

            // Append memory buffer
            add = a.AppendFileFromMemory("memory.txt", (void*)kStr, strlen(kStr),
                                         CCompression::eLevel_NoCompression,
                                         "memory buffer comment");
            assert(add->size() == 1);
            entries->splice(entries->end(), *add);
            size_t n = entries->size();
            assert(n == 14);

            // Finalize archive
            void*  buf;
            size_t buf_size;
            a.Finalize(&buf, &buf_size);
            // Create auto ptr to deallocate buffer automatically
            AutoPtr<char, CDeleter<char> > buf_deleter((char*)buf);
            // or use: free(buf);

            // Save created memory-based archive to file
            a.Save(a_memory);
            assert(CFile(a_memory).Exists());
            // Load created archive back to memory
            // (just to check Save/Load methods)
            a.Load(a_memory);

            // List entries
            a.UnsetMask();
            entries = a.List();
            assert(entries->size() == n);
            //PrintEntries(entries.get());

            // Check some entries
            CMaskFileName mask1;
            mask1.Add("memory*");
            a.SetMask(&mask1);
            entries = a.List();
            assert(entries->size() == 1);
            info = entries->front();
            assert(info.GetName() == "memory.txt");
            assert(info.GetType() == CDirEntry::eFile);
            assert(info.GetSize() == strlen(kStr));

            CMaskFileName mask2;
            mask2.Add("subdir1");
            a.UnsetMask(CArchive::eFullPathMask);
            a.SetMask(&mask2, eNoOwnership, CArchive::ePatternMask);
            entries = a.List();
            //PrintEntries(entries.get());
            assert(entries->size() == 4 /* dir entry + 3 files, 2 of them are the same */ );
            info = entries->front();
            assert(info.GetName() == "subdir1/");
            assert(info.GetType() == CDirEntry::eDir);
            assert(info.GetSize() == 0);
        }}
        // Test file
        s_TestFileArchive(format, a_memory, src, dst_src);

        // Open file from memory
        {{
            // Load archive to memory
            size_t archive_size = (size_t)CFile(a_memory).GetLength();
            AutoArray<char> archive_buf(archive_size);

            fio.Open(a_memory, CFileIO::eOpen, CFileIO::eRead);
            size_t n_read = fio.Read(archive_buf.get(), archive_size);
            assert(n_read == archive_size);
            fio.Close();

            // Open and list all entries
            CArchiveMemory am(format, archive_buf.get(), archive_size);
            entries = am.List();
            //PrintEntries(entries.get());
            assert(entries->size() == 14);

            // Find file
            CMaskFileName mask1;
            mask1.Add("memory*");
            am.SetMask(&mask1);
            entries = am.List();
            assert(entries->size() == 1);
            info = entries->front();
            assert(info.GetName() == "memory.txt");
            assert(info.GetType() == CDirEntry::eFile);
            assert(info.GetSize() == strlen(kStr));

            // ... and extract it to memory using different methods
            char   buf[1024];
            char*  buf_ptr;
            size_t buf_size, n;
            // Extract to heap with automatic memory allocation
            am.ExtractFileToHeap(info, (void**)(&buf_ptr), &buf_size);
            assert(buf_size == strlen(kStr));
            free(buf_ptr);
            // or use: AutoPtr<char, CDeleter<char> > buf_deleter(buf_ptr);
            // Extract to preallocated memory buffer
            am.ExtractFileToMemory(info, buf, sizeof(buf), &n);
            assert(buf_size == n);
            // Extract using callback
            am.ExtractFileToCallback(info, s_ExtractCallback);
        }}
    }}
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv);
}
