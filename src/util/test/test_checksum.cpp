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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   checksum computation test
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <util/checksum.hpp>
#include <util/random_gen.hpp>
#include <vector>

// must be last
#include <common/test_assert.h>

USING_NCBI_SCOPE;

class CFileData;

class CChecksumTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
    void RunChecksum(CChecksum::EMethod type, const CFileData& data);

    size_t m_ChunkSize;
    size_t m_ChunkCount;
    size_t m_ChunkOffset;
};

void CChecksumTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CChecksum test program");

    arg_desc->AddFlag("selftest",
                      "Just verify behavior on internal test data");
    arg_desc->AddFlag("CRC32",
                      "Run CRC32 test.");
    arg_desc->AddFlag("CRC32ZIP",
                      "Run CRC32ZIP test.");
    arg_desc->AddFlag("CRC32CKSUM",
                      "Run CRC32 test as cksum does.");
    arg_desc->AddFlag("CRC32C",
                      "Run CRC32C test.");
    arg_desc->AddFlag("MD5",
                      "Run MD5 test.");
    arg_desc->AddFlag("Adler32",
                      "Run Adler32 test.");
    arg_desc->AddDefaultKey("size", "size",
                            "Process chunk size",
                            CArgDescriptions::eInteger, "8192");
    arg_desc->AddDefaultKey("count", "count",
                            "Process chunk count",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("offset", "offset",
                            "Process chunk unalignment offset",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddExtra(0, kMax_UInt, "files to process (stdin if none given)",
                       CArgDescriptions::eInputFile,
                       CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);
    arg_desc->AddFlag("print_tables",
                      "Print CRC table for inclusion in C++ code");

    SetupArgDescriptions(arg_desc.release());
}

static void s_ComputeSums(const char* data,
                          size_t size,
                          CChecksum& crc32,
                          CChecksum& crc32zip,
                          CChecksum& crc32cksum,
                          CChecksum& crc32c,
                          CChecksum& md5)
{
    char buf[289]; // use a weird size to force varying phases
    size_t test_size = 3;
    while (size) {
        size_t count = min(test_size, size);
        memcpy(buf, data, count);
        data += count;
        size -= count;
        crc32.AddChars(buf, count);
        crc32zip.AddChars(buf, count);
        crc32cksum.AddChars(buf, count);
        crc32c.AddChars(buf, count);
        md5.AddChars(buf, count);
        test_size = sizeof(buf);
    }
}

static const char* s_GetMethodName(const CChecksum& checksum)
{
    switch ( checksum.GetMethod() ) {
    case CChecksum::eCRC32: return "CRC32";
    case CChecksum::eCRC32ZIP: return"CRC32ZIP";
    case CChecksum::eCRC32INSD: return "CRC32INSD";
    case CChecksum::eCRC32CKSUM: return "CRC32CKSUM";
    case CChecksum::eCRC32C: return "CRC32C";
    case CChecksum::eAdler32: return "Adler32";
    case CChecksum::eMD5: return "MD5";
    default: return "???";
    }
}

static bool s_VerifySum(const char* test,
                        const CChecksum& checksum,
                        Uint4 expected,
                        const char* expected_md5 = 0)
{
    if ( expected_md5 ) {
        if ( checksum.GetHexSum() != expected_md5 ) {
            const char* method = s_GetMethodName(checksum);
            cerr << "FAILED "<<test<<"!" << endl;
            cerr << "Expected "<<method<<": " << hex << expected_md5 << endl;
            cerr << "Computed "<<method<<": " << hex << checksum.GetHexSum()
                 << endl;
            return false;
        }
    }
    else {
        if ( checksum.GetChecksum() != expected ) {
            const char* method = s_GetMethodName(checksum);
            cerr << "FAILED "<<test<<"!" << endl;
            cerr << "Expected "<<method<<": " << hex << expected << endl;
            cerr << "Computed "<<method<<": " << hex << checksum.GetChecksum()
                 << endl;
            return false;
        }
    }
    return true;
}

static bool s_VerifySums(const string& data,
                         Uint4 expected_crc32,
                         Uint4 expected_crc32zip,
                         Uint4 expected_crc32cksum,
                         Uint4 expected_crc32c,
                         const char* expected_md5)
{
    //cerr << "Input: \"" << Printable(data) << '\"' << endl;
    bool ok = true;
    CChecksum       crc32(CChecksum::eCRC32);
    CChecksum       crc32zip(CChecksum::eCRC32ZIP);
    CChecksum       crc32cksum(CChecksum::eCRC32CKSUM);
    CChecksum       crc32c(CChecksum::eCRC32C);
    CChecksum       md5(CChecksum::eMD5);
    s_ComputeSums(data.data(), data.size(),
                  crc32, crc32zip, crc32cksum, crc32c, md5);
    ok &= s_VerifySum("string", crc32, expected_crc32);
    ok &= s_VerifySum("string", crc32zip, expected_crc32zip);
    ok &= s_VerifySum("string", crc32cksum, expected_crc32cksum);
    ok &= s_VerifySum("string", crc32c, expected_crc32c);
    ok &= s_VerifySum("string", md5, 0, expected_md5);
    cerr << "CRC/MD5 test " << (ok?"passed":"failed")
         << " for \""<< Printable(data) << "\"" << endl;
    return ok;
}

bool s_Adler32Test()
{
    // 1000, 6000, ..., 96000 arrays filled with value = (byte) index.
    static const int kNumTests = 20;
    static const Uint4 expected[kNumTests] = {
        486795068U,
        1525910894U,
        3543032800U,
        2483946130U,
        4150712693U,
        3878123687U,
        3650897945U,
        1682829244U,
        1842395054U,
        460416992U,
        3287492690U,
        479453429U,
        3960773095U,
        2008242969U,
        4130540683U,
        1021367854U,
        4065361952U,
        2081116754U,
        4033606837U,
        1162071911U
    };
    
    bool ok = true;
    for ( int l = 0; l < kNumTests; l++ ) {
        int length = l * 5000 + 1000;
        vector<char> bs(length);
        for (int j = 0; j < length; j++ ) {
            bs[j] = j;
        }
        for ( int c = length; c > 0; c >>= 1 ) {
            CChecksum sum(CChecksum::eAdler32);
            for ( int p = 0; p < length; p += c ) {
                sum.AddChars(&bs[p], min(c, length-p));
            }
            if ( sum.GetChecksum() != expected[l] ) {
                cerr << "Failed Adler32 test "<<l
                     << " length: "<<length
                     << " chunk: "<<c
                     << hex
                     << " expected: "<<expected[l]
                     << " calculated: "<<sum.GetChecksum()
                     << dec << endl;
                ok = false;
            }
        }
    }
    cerr << "Adler32 test "<<(ok?"passed":"failed") << endl;
    return ok;
}


class CFileData
{
public:
    CFileData(const string& file_name)
        : m_data(0), m_size(0)
        {
            Load(file_name);
        }
    CFileData(CNcbiIstream& is)
        : m_data(0), m_size(0)
        {
            Load(is);
        }
    ~CFileData()
        {
            Reset();
        }

    void Load(const string& file_name);
    void Load(CNcbiIstream& is);
    void Reset(void);

    const char* data() const
        {
            return m_data;
        }
    size_t size() const
        {
            return m_size;
        }
private:
    const char* m_data;
    size_t m_size;
    vector<char> m_buffer;
    AutoPtr<CMemoryFile> m_file;
    
    CFileData(const CFileData&);
    void operator=(const CFileData&);
};


void CFileData::Reset(void)
{
    m_data = 0;
    m_size = 0;
    m_buffer.clear();
    m_file.reset();
}


static size_t dummy_counter;


void CFileData::Load(const string& file_name)
{
    CStopWatch sw(CStopWatch::eStart);
    m_file.reset(new CMemoryFile(file_name));
    m_size = m_file->GetSize();
    m_file->Map(0, m_size);
    m_data = (const char*)m_file->GetPtr();
    dummy_counter += count(data(), data()+size(), '\0');
    string time = sw.AsString();
    cout << "Mapped "<<file_name<<" in "<<time<<endl;
}


void CFileData::Load(CNcbiIstream& is)
{
    Reset();
    char buffer[8192];
    while ( is ) {
        is.read(buffer, sizeof(buffer));
        m_buffer.insert(m_buffer.end(), buffer, buffer+is.gcount());
    }
    m_data = m_buffer.data();
    m_size = m_buffer.size();
}


bool s_BigSelfTest(CRandom& random,
                   const CFileData& file_data,
                   CChecksum::EMethod method,
                   Uint4 expected,
                   const char* expected_md5 = 0)
{
    bool ok = true;
    const char* data = file_data.data();
    size_t size = file_data.size();
    vector<char> buf;
    const size_t kOffsets = 16;
    for ( size_t offset = 0; offset < kOffsets; ++offset ) {
        CChecksum checksum(method);
        const char* cur_data = data;
        if ( offset ) {
            buf.resize(offset);
            buf.insert(buf.end(), data, data+size);
            cur_data = &buf[offset];
        }
        for ( size_t cur_size = size; cur_size > 0; ) {
            size_t chunk = random.GetRand(1, 20000);
            chunk = random.GetRand(1, (CRandom::TValue)chunk);
            chunk = min(chunk, cur_size);
            checksum.AddChars(cur_data, chunk);
            cur_data += chunk;
            cur_size -= chunk;
        }
        ok &= s_VerifySum("file", checksum, expected, expected_md5);
    }
    return ok;
}


bool s_BigSelfTest(void)
{
    const char* file_name = "test_data/checksum.dat";
    bool ok = true;
    CFileData data(file_name);
    cerr << "Testing file "<<file_name<<" of "<<data.size()<<" bytes:"<<endl;
    CRandom random;
    ok &= s_BigSelfTest(random, data, CChecksum::eCRC32, 0xa0b29e2f);
    ok &= s_BigSelfTest(random, data, CChecksum::eCRC32ZIP, 0x39a90823);
    ok &= s_BigSelfTest(random, data, CChecksum::eCRC32INSD, 0xc656f7dc);
    ok &= s_BigSelfTest(random, data, CChecksum::eCRC32CKSUM, 0x4a0a1bdb);
    ok &= s_BigSelfTest(random, data, CChecksum::eCRC32C, 0x7adb1cd3);
    ok &= s_BigSelfTest(random, data, CChecksum::eAdler32, 0x528a7135);
    ok &= s_BigSelfTest(random, data, CChecksum::eMD5, 0,
                        "a1ed665e33b6feb5a645738b4384ca25");
    cerr << "File "<<file_name<<" test "<<(ok?"passed":"failed") << endl;
    return ok;
}


static
CNcbiOstream& s_ReportSum(CNcbiOstream& out, const CChecksum& sum)
{
    out <<setw(10)<<s_GetMethodName(sum)<<": ";
    sum.WriteHexSum(out);
    return out;
}

void CChecksumTestApp::RunChecksum(CChecksum::EMethod type,
                                   const CFileData& data)
{
    CStopWatch timer(CStopWatch::eStart);
    CChecksum sum(type);
    for ( size_t i = 0; i < m_ChunkCount; ++i ) {
        sum.AddChars(data.data(), data.size());
    }
    string time = timer.AsString();
    cout << "Processed in "<<time<<": ";
    s_ReportSum(cout, sum);
    cout<<endl;
}

int CChecksumTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    m_ChunkSize = args["size"].AsInteger();
    m_ChunkCount = args["count"].AsInteger();
    m_ChunkOffset = args["offset"].AsInteger();
    m_ChunkOffset %= 16;

    if (args["selftest"]) {
        // Not testing CRCs for now, since I can't find an external
        // implementation that agrees with what we have.

        // MD5 test cases from RFC 1321
        bool ok = true;
        ok &= s_VerifySums(kEmptyStr,
                           0, 0, 0xffffffff, 0,
                           "d41d8cd98f00b204e9800998ecf8427e");
        ok &= s_VerifySums("a",
                           0xa864db20, 0xe8b7be43, 0x48c279fe, 0xc1d04330,
                           "0cc175b9c0f1b6a831c399e269772661");
        ok &= s_VerifySums("abc",
                           0x2c17398c, 0x352441c2, 0x48aa78a2, 0x364b3fb7,
                           "900150983cd24fb0d6963f7d28e17f72");
        ok &= s_VerifySums("message digest",
                           0x5c57dedc, 0x20159d7f, 0xd934b396, 0x02bd79d0,
                           "f96b697d7cb7938d525a2f31aaf161d0");
        ok &= s_VerifySums("abcdefghijklmnopqrstuvwxyz",
                           0x3bc2a463, 0x4c2750bd, 0xa1b937a8, 0x9ee6ef25,
                           "c3fcd3d76192e4007dfb496cca67e13b");
        ok &= s_VerifySums("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz0123456789",
                           0x26910730, 0x1fc2e6d2, 0x4e1f937, 0xa245d57d,
                           "d174ab98d277d9f5a5611c2c9f419d9f");
        ok &= s_VerifySums("1234567890123456789012345678901234567890123456789"
                           "0123456789012345678901234567890",
                           0x7110bde7, 0x7ca94a72, 0x73a0b3a8, 0x477a6781,
                           "57edf4a22be3c955ac49da2e2107b67a");
        ok &= s_VerifySums(string("\1\xc0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                                  "\1\xfe\x60\xac\0\0\0\x8\0\0\0\x4\0\0\0\x9"
                                  "\x25\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 48),
                           0xfbcf2b84, 0xc897a166, 0x46152007, 0x99b08a14,
                           "f16de75fd4137d2b5b12f35f247a6214");
        ok &= s_Adler32Test();
        ok &= s_BigSelfTest();
        cerr << (ok? "All tests passed": "Errors detected") << endl;
        return ok? 0: 1;
    }
    else if ( args["print_tables"] ) {
        CChecksum::PrintTables(NcbiCout);
    }
    else {
        bool run_crc32 = args["CRC32"];
        bool run_crc32zip = args["CRC32ZIP"];
        bool run_crc32cksum = args["CRC32CKSUM"];
        bool run_crc32c = args["CRC32C"];
        bool run_md5 = args["MD5"];
        bool run_adler32 = args["Adler32"];
        if ( run_crc32 | run_crc32zip | run_crc32cksum | run_crc32c |
             run_md5 | run_adler32 ) {
            if (args.GetNExtra()) {
                for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
                    if (extra > 1) {
                        cout << endl;
                    }
                    const string& file_name = args[extra].AsString();
                    cout << "File: " << file_name << endl;
                    CFileData data(file_name);
                    if ( run_crc32 ) {
                        RunChecksum(CChecksum::eCRC32, data);
                    }
                    if ( run_crc32zip ) {
                        RunChecksum(CChecksum::eCRC32ZIP, data);
                    }
                    if ( run_crc32cksum ) {
                        RunChecksum(CChecksum::eCRC32CKSUM, data);
                    }
                    if ( run_crc32c ) {
                        RunChecksum(CChecksum::eCRC32C, data);
                    }
                    if ( run_md5 ) {
                        RunChecksum(CChecksum::eMD5, data);
                    }
                    if ( run_adler32 ) {
                        RunChecksum(CChecksum::eAdler32, data);
                    }
                }
            }
            else {
                CFileData data(cin);
                if ( run_crc32 ) {
                    RunChecksum(CChecksum::eCRC32, data);
                }
                if ( run_crc32zip ) {
                    RunChecksum(CChecksum::eCRC32ZIP, data);
                }
                if ( run_crc32cksum ) {
                    RunChecksum(CChecksum::eCRC32CKSUM, data);
                }
                if ( run_crc32c ) {
                    RunChecksum(CChecksum::eCRC32C, data);
                }
                if ( run_md5 ) {
                    RunChecksum(CChecksum::eMD5, data);
                }
                if ( run_adler32 ) {
                    RunChecksum(CChecksum::eAdler32, data);
                }
            }
        }
        else {
            if (args.GetNExtra()) {
                for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
                    if (extra > 1) {
                        cout << endl;
                    }
                    cout << "File: " << args[extra].AsString() << endl;
                    CFileData data(args[extra].AsString());
                    CChecksum crc32(CChecksum::eCRC32);
                    CChecksum crc32zip(CChecksum::eCRC32ZIP);
                    CChecksum crc32cksum(CChecksum::eCRC32CKSUM);
                    CChecksum crc32c(CChecksum::eCRC32C);
                    CChecksum md5(CChecksum::eMD5);
                    s_ComputeSums(data.data(), data.size(),
                                  crc32, crc32zip, crc32cksum, crc32c, md5);
                    s_ReportSum(cout, crc32) << endl;
                    s_ReportSum(cout, crc32zip) << endl;
                    s_ReportSum(cout, crc32cksum) << endl;
                    s_ReportSum(cout, crc32c) << endl;
                    s_ReportSum(cout, md5) << endl;
                }
            } else {
                CFileData data(cin);
                CChecksum crc32(CChecksum::eCRC32);
                CChecksum crc32zip(CChecksum::eCRC32ZIP);
                CChecksum crc32cksum(CChecksum::eCRC32CKSUM);
                CChecksum crc32c(CChecksum::eCRC32C);
                CChecksum md5(CChecksum::eMD5);
                s_ComputeSums(data.data(), data.size(),
                              crc32, crc32zip, crc32cksum, crc32c, md5);
                s_ReportSum(cout, crc32) << endl;
                s_ReportSum(cout, crc32zip) << endl;
                s_ReportSum(cout, crc32cksum) << endl;
                s_ReportSum(cout, crc32c) << endl;
                s_ReportSum(cout, md5) << endl;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    return CChecksumTestApp().AppMain(argc, argv);
}
