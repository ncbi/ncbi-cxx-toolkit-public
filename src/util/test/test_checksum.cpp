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
* Authors:  Aaron Ucko, Vladimir Ivanov
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

/// Checksum computation results

union SChecksumCRC {
    Uint8 crc64;
    Uint4 crc32;
};

struct SChecksumCase
{
    CChecksum::EMethod method;
    SChecksumCRC       crc;
    const char*        md5;
};


// Test application

class CChecksumTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);

public:
    // Get method name
    string GetMethodName(const CChecksum& sum);
    // Check that method is an one-pass hash method (to use with Calculate())
    bool IsHashMethod(CChecksum::EMethod method);
    // Compute checksum for a given string
    void ComputeStrSum(const char* data, size_t size, CChecksum& sum);
    // Compute checksum for a big data
    void ComputeBigSum(CRandom& random, const CFileData& file_data, size_t offset, CChecksum& sum);
    // Verify computed checksum against known values
    bool VerifySum(const string& data_origin, CChecksum& sum, SChecksumCase& expected);

    // Self tests

    // CRC/MD5 string tests
    bool SelfTest_Str();
    // Additional Adler32 test
    bool SelfTest_Adler32();
    // Big data test
    bool SelfTest_Big();

    // Speed tests

    // Run speed tests on a file data (depends on arguments)
    void RunChecksums(const CFileData& file_data);
    // Run a specific speed test on a file data
    void RunChecksum(CChecksum::EMethod method, const CFileData& file_data);

private:
/* -- not used (yet)
    size_t m_ChunkSize;
    size_t m_ChunkCount;
    size_t m_ChunkOffset;
*/
};


void CChecksumTestApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CChecksum test program");

    arg_desc->AddFlag("selftest",   "Verify behavior on internal test data");

    /* -- not used (yet)
    arg_desc->AddDefaultKey("size", "size",
                      "Process chunk size",
                      CArgDescriptions::eInteger, "8192");
    arg_desc->AddDefaultKey("count", "count",
                      "Process chunk count",
                      CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("offset", "offset",
                      "Process chunk unalignment offset",
                      CArgDescriptions::eInteger, "0");
*/
    arg_desc->AddExtra(0, kMax_UInt, "files to process (stdin if none given)",
                       CArgDescriptions::eInputFile,
                       CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);
    arg_desc->AddFlag("print_tables",
                      "Print CRC table for inclusion in C++ code");

    SetupArgDescriptions(arg_desc.release());
}


string CChecksumTestApp::GetMethodName(const CChecksum& sum)
{
    switch ( sum.GetMethod() ) {
    case CChecksum::eCRC32:      return "CRC32";
    case CChecksum::eCRC32ZIP:   return "CRC32ZIP";
    case CChecksum::eCRC32INSD:  return "CRC32INSD";
    case CChecksum::eCRC32CKSUM: return "CRC32CKSUM";
    case CChecksum::eCRC32C:     return "CRC32C";
    case CChecksum::eAdler32:    return "Adler32";
    case CChecksum::eMD5:        return "MD5";
    case CChecksum::eCityHash32: return "CityHash32";
    case CChecksum::eCityHash64: return "CityHash64";
    case CChecksum::eFarmHash32: return "FarmHash32";
    case CChecksum::eFarmHash64: return "FarmHash64";
    default: ;
    }
    _TROUBLE;
    return "";
}

bool CChecksumTestApp::IsHashMethod(CChecksum::EMethod method)
{
    switch ( method ) {
    case CChecksum::eCityHash32:
    case CChecksum::eCityHash64:
    case CChecksum::eFarmHash32:
    case CChecksum::eFarmHash64:
        return true;
    default: ;
    }
    return false;
}


bool CChecksumTestApp::VerifySum(const string& data_origin, CChecksum& sum, SChecksumCase& expected)
{
    string method = GetMethodName(sum);

    if ( sum.GetMethod() == CChecksum::eMD5 ) {
        if ( sum.GetHexSum() != expected.md5 ) {
            cerr << "FAILED "<< method << " for " << data_origin << endl;
            cerr << "    Expected: " << hex << expected.md5  << endl;
            cerr << "    Computed: " << hex << sum.GetHexSum() << endl;
            return false;
        }
    }
    else 
    if (sum.GetChecksumBits() == 32) {
        if ( sum.GetChecksum32() != expected.crc.crc32 ) {
            cerr << "FAILED "<< method << " for " << data_origin << endl;
            cerr << "    Expected: " << hex << expected.crc.crc32  << endl;
            cerr << "    Computed: " << hex << sum.GetChecksum32() << endl;
            return false;
        }
    }
    else 
    if (sum.GetChecksumBits() == 64) {
        if ( sum.GetChecksum64() != expected.crc.crc64 ) {
            cerr << "FAILED "<< method << " for " << data_origin << endl;
            cerr << "    Expected: " << hex << expected.crc.crc64  << endl;
            cerr << "    Computed: " << hex << sum.GetChecksum64() << endl;
            return false;
        }
    } else{
        _TROUBLE;
    }
    return true;
}


// Not testing CRCs for now, since I can't find an external
// implementation that agrees with what we have.
// Note: MD5 test cases from RFC 1321

struct SChecksumStrTest
{
    string str;
    vector<SChecksumCase> cases;
};

vector<SChecksumStrTest> s_StrTests = {

    { // CChecksum::Reset() test, except hash methods, that compute hash even for empty strings
      "", {
        { CChecksum::eCRC32,       { 0 },           "" },
        { CChecksum::eCRC32ZIP,    { 0 },           "" },
        { CChecksum::eCRC32INSD,   { 0xffffffff },  "" },
        { CChecksum::eCRC32CKSUM,  { 0xffffffff },  "" },
        { CChecksum::eCRC32C,      { 0 },           "" },
        { CChecksum::eAdler32,     { 0x00000001 },  "" },
        { CChecksum::eCityHash32,  { 0xdc56d17a },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x9ae16a3b2f90404f) }, "" },
        { CChecksum::eMD5,         { 0 }, "d41d8cd98f00b204e9800998ecf8427e" }}
    },
    { "a", {
        { CChecksum::eCRC32,       { 0xa864db20 },  "" },
        { CChecksum::eCRC32ZIP,    { 0xe8b7be43 },  "" },
        { CChecksum::eCRC32INSD,   { 0x174841bc },  "" },
        { CChecksum::eCRC32CKSUM,  { 0x48c279fe },  "" },
        { CChecksum::eCRC32C,      { 0xc1d04330 },  "" },
        { CChecksum::eAdler32,     { 0x00620062 },  "" },
        { CChecksum::eCityHash32,  { 0x3c973d4d },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0xb3454265b6df75e3) }, "" },
        { CChecksum::eMD5,         { 0 }, "0cc175b9c0f1b6a831c399e269772661" }}
    },
    { "abc", {
        { CChecksum::eCRC32,       { 0x2c17398c },  "" },
        { CChecksum::eCRC32ZIP,    { 0x352441c2 },  "" },
        { CChecksum::eCRC32INSD,   { 0xcadbbe3d },  "" },
        { CChecksum::eCRC32CKSUM,  { 0x48aa78a2 },  "" },
        { CChecksum::eCRC32C,      { 0x364b3fb7 },  "" },
        { CChecksum::eAdler32,     { 0x024d0127 },  "" },
        { CChecksum::eCityHash32,  { 0x2f635ec7 },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x24a5b3a074e7f369) }, "" },
        { CChecksum::eMD5,         { 0 }, "900150983cd24fb0d6963f7d28e17f72" }}
    },
    { "message digest", {
        { CChecksum::eCRC32,       { 0x5c57dedc },  "" },
        { CChecksum::eCRC32ZIP,    { 0x20159d7f },  "" },
        { CChecksum::eCRC32INSD,   { 0xdfea6280 },  "" },
        { CChecksum::eCRC32CKSUM,  { 0xd934b396 },  "" },
        { CChecksum::eCRC32C,      { 0x02bd79d0 },  "" },
        { CChecksum::eAdler32,     { 0x29750586 },  "" },
        { CChecksum::eCityHash32,  { 0x246f52b3 },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x8db193972bf98c6a) }, "" },
        { CChecksum::eMD5,         { 0 }, "f96b697d7cb7938d525a2f31aaf161d0" }}
    },
    { "abcdefghijklmnopqrstuvwxyz", {
        { CChecksum::eCRC32,       { 0x3bc2a463 },  "" },
        { CChecksum::eCRC32ZIP,    { 0x4c2750bd },  "" },
        { CChecksum::eCRC32INSD,   { 0xb3d8af42 },  "" },
        { CChecksum::eCRC32CKSUM,  { 0xa1b937a8 },  "" },
        { CChecksum::eCRC32C,      { 0x9ee6ef25 },  "" },
        { CChecksum::eAdler32,     { 0x90860b20 },  "" },
        { CChecksum::eCityHash32,  { 0xaa02c5c1 },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x5ead741ce7ac31bd) }, "" },
        { CChecksum::eMD5,         { 0 }, "c3fcd3d76192e4007dfb496cca67e13b" }}
    },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {
        { CChecksum::eCRC32,       { 0x26910730 },  "" },
        { CChecksum::eCRC32ZIP,    { 0x1fc2e6d2 },  "" },
        { CChecksum::eCRC32INSD,   { 0xe03d192d },  "" },
        { CChecksum::eCRC32CKSUM,  { 0x04e1f937 },  "" },
        { CChecksum::eCRC32C,      { 0xa245d57d },  "" },
        { CChecksum::eAdler32,     { 0x8adb150c },  "" },
        { CChecksum::eCityHash32,  { 0xa77b8219 },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x4566c1e718836cd6) }, "" },
        { CChecksum::eMD5,         { 0 }, "d174ab98d277d9f5a5611c2c9f419d9f" }}
    },
    { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", {
        { CChecksum::eCRC32,       { 0x7110bde7 },  "" },
        { CChecksum::eCRC32ZIP,    { 0x7ca94a72 },  "" },
        { CChecksum::eCRC32INSD,   { 0x8356b58d },  "" },
        { CChecksum::eCRC32CKSUM,  { 0x73a0b3a8 },  "" },
        { CChecksum::eCRC32C,      { 0x477a6781 },  "" },
        { CChecksum::eAdler32,     { 0x97b61069 },  "" },
        { CChecksum::eCityHash32,  { 0x725594c0 },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0x791a4c16629ee4cd) }, "" },
        { CChecksum::eMD5,         { 0 }, "57edf4a22be3c955ac49da2e2107b67a" }}
    },
    { string("\1\xc0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\xfe\x60\xac\0\0\0"
             "\x8\0\0\0\x4\0\0\0\x9\x25\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 48), {
        { CChecksum::eCRC32,       { 0xfbcf2b84 },  "" },
        { CChecksum::eCRC32ZIP,    { 0xc897a166 },  "" },
        { CChecksum::eCRC32INSD,   { 0x37685e99 },  "" },
        { CChecksum::eCRC32CKSUM,  { 0x46152007 },  "" },
        { CChecksum::eCRC32C,      { 0x99b08a14 },  "" },
        { CChecksum::eAdler32,     { 0x65430307 },  "" },
        { CChecksum::eCityHash32,  { 0x5a50eecd },  "" },
        { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0xca8a7f96d3b805dd) }, "" },
        { CChecksum::eMD5,         { 0 }, "f16de75fd4137d2b5b12f35f247a6214" }}
    }
};


bool CChecksumTestApp::SelfTest_Str()
{
    bool ok = true;
    for (auto test : s_StrTests) {
        for (auto testcase : test.cases) {
            CChecksum sum(testcase.method);
            if ( IsHashMethod(testcase.method) ) {
                sum.Calculate(test.str.data(), test.str.size());
            }
            else {
                ComputeStrSum(test.str.data(), test.str.size(), sum);
            }
            ok &= VerifySum("string '" + NStr::PrintableString(test.str) + "'", sum, testcase);
        }
    }
    cout << "CRC/MD5: " << (ok ? "passed" : "failed") << endl;
    return ok;
}


void CChecksumTestApp::ComputeStrSum(const char* data, size_t size, CChecksum& sum)
{
    char buf[289];  // use a weird size to force varying phases
    size_t test_size = 3;
    while (size) {
        size_t count = min(test_size, size);
        memcpy(buf, data, count);
        data += count;
        size -= count;
        sum.AddChars(buf, count);
        test_size = sizeof(buf);
    }
}


bool CChecksumTestApp::SelfTest_Adler32()
{
    // 1000, 6000, ..., 96000 arrays filled with value = (byte) index.
    const int kNumTests = 20;
    const Uint4 expected[kNumTests] = {
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
            bs[j] = (char)j;
        }
        for ( int c = length; c > 0; c >>= 1 ) {
            CChecksum sum(CChecksum::eAdler32);
            for ( int p = 0; p < length; p += c ) {
                sum.AddChars(&bs[p], min(c, length-p));
            }
            if ( sum.GetChecksum() != expected[l] ) {
                cerr << "FAILED Adler32 test "<<l
                     << " length: " << length
                     << " chunk: "  << c
                     << hex
                     << " expected: "   << expected[l]
                     << " calculated: " << sum.GetChecksum()
                     << dec << endl;
                ok = false;
            }
        }
    }
    cout << "Adler32: " << (ok ? "passed" : "failed") << endl;
    return ok;
}


class CFileData
{
public:
    CFileData(const string& file_name) : m_data(0), m_size(0) {
        Load(file_name);
    }
    CFileData(CNcbiIstream& is) : m_data(0), m_size(0) {
        Load(is);
    }
    ~CFileData() {
        Reset();
    }
    void Load(const string& file_name);
    void Load(CNcbiIstream& is);
    void Reset(void);

    const char* data() const {
        return m_data;
    }
    size_t size() const {
        return m_size;
    }
private:
    const char*             m_data;
    size_t                  m_size;
    vector<char>            m_buffer;
    unique_ptr<CMemoryFile> m_file;

    // disable copying and assignment
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


void CFileData::Load(const string& file_name)
{
    static size_t dummy_counter;

    CStopWatch sw(CStopWatch::eStart);
    m_file.reset(new CMemoryFile(file_name));
    m_size = m_file->GetSize();
    m_file->Map(0, m_size);
    m_data = (const char*)m_file->GetPtr();
    dummy_counter += count(data(), data()+size(), '\0');
    string time = sw.AsString();
    cout << ", mapped in "<< time;
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



vector<SChecksumCase> s_BigTests = {
    { CChecksum::eCRC32,       { 0xa0b29e2f },  "" },
    { CChecksum::eCRC32ZIP,    { 0x39a90823 },  "" },
    { CChecksum::eCRC32INSD,   { 0xc656f7dc },  "" },
    { CChecksum::eCRC32CKSUM,  { 0x4a0a1bdb },  "" },
    { CChecksum::eCRC32C,      { 0x7adb1cd3 },  "" },
    { CChecksum::eAdler32,     { 0x528a7135 },  "" },
    { CChecksum::eCityHash32,  { 0xca83e47e },  "" },
    { CChecksum::eCityHash64,  { NCBI_CONST_UINT8(0xb2195ac46438d77d) }, "" },
    { CChecksum::eMD5,         { 0 }, "a1ed665e33b6feb5a645738b4384ca25" }
};


bool CChecksumTestApp::SelfTest_Big()
{
    const char*  kFileName = "test_data/checksum.dat";
    const size_t kOffsets  = 16;

    bool ok = true;
    CRandom random;

    cout << "File: " << kFileName;
    CFileData file_data(kFileName);
    cout << ", size: " << file_data.size() << " bytes" << endl;

    for (auto testcase : s_BigTests) {
        if ( IsHashMethod(testcase.method) ) {
            CChecksum sum(testcase.method);
            sum.Calculate(file_data.data(), file_data.size());
            ok &= VerifySum("file (hash)", sum, testcase);
        }
        else {
            for (size_t offset = 0; offset < kOffsets; ++offset) {
                CChecksum sum(testcase.method);
                ComputeBigSum(random, file_data, offset, sum);
                ok &= VerifySum("file (" + NStr::NumericToString(offset) + ")", sum, testcase);
            }
        }
    }
    cout << "File "<< kFileName << ": " << (ok ? "passed" : "failed") << endl;
    return ok;
}


void CChecksumTestApp::ComputeBigSum(CRandom& random, const CFileData& file_data, size_t offset, CChecksum& sum)
{
    const char*  data = file_data.data();
    size_t       size = file_data.size();
    vector<char> buf;
    const char*  cur_data = data;

    if ( offset ) {
        buf.resize(offset);
        buf.reserve(offset+size);
        buf.insert(buf.end(), data, data+size);
        cur_data = &buf[offset];
    }
    for ( size_t cur_size = size; cur_size > 0; ) {
        size_t chunk = random.GetRand(1, 20000);
        chunk = random.GetRand(1, (CRandom::TValue)chunk);
        chunk = min(chunk, cur_size);
        sum.AddChars(cur_data, chunk);
        cur_data += chunk;
        cur_size -= chunk;
    }
}


void CChecksumTestApp::RunChecksums(const CFileData& data)
{
    RunChecksum(CChecksum::eCRC32,      data);
    RunChecksum(CChecksum::eCRC32ZIP,   data);
    RunChecksum(CChecksum::eCRC32CKSUM, data);
    RunChecksum(CChecksum::eCRC32C,     data);
    RunChecksum(CChecksum::eAdler32,    data);
    RunChecksum(CChecksum::eCityHash32, data);
    RunChecksum(CChecksum::eFarmHash32, data);
    RunChecksum(CChecksum::eCityHash64, data);
    RunChecksum(CChecksum::eFarmHash64, data);
    RunChecksum(CChecksum::eMD5,        data);
}


void CChecksumTestApp::RunChecksum(CChecksum::EMethod method,  const CFileData& file_data)
{
    CStopWatch timer(CStopWatch::eStart);
    CChecksum sum(method);
    sum.Calculate(file_data.data(), file_data.size());
    string time = timer.AsString();
    cout << "Processed in " << time << ": ";
    cout << setw(10) << GetMethodName(sum) << ": "
         << setw(8)  << sum.GetHexSum() << endl;
}


int CChecksumTestApp::Run(void)
{
    const CArgs& args = GetArgs();

/* -- not used (yet)
    m_ChunkSize   = args["size"].AsInteger();
    m_ChunkCount  = args["count"].AsInteger();
    m_ChunkOffset = args["offset"].AsInteger();
    m_ChunkOffset %= 16;
*/

    if (args["selftest"]) {
        bool ok = true;
        ok &= SelfTest_Str();
        ok &= SelfTest_Adler32();
        ok &= SelfTest_Big();
        cout << (ok ? "All tests passed" : "Errors detected") << endl;
        return ok ? 0 : 1;
    }
    
    // Special/internal
    else if ( args["print_tables"] ) {
        CChecksum::PrintTables(NcbiCout);
    }
    
    // Run speed tests for specified methods
    else {
        if (args.GetNExtra()) {
            for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
                if (extra > 1) {
                    cout << endl;
                }
                const string& filename = args[extra].AsString();
                cout << "File: " << filename;
                CFileData data(filename);
                cout << ", size: " << data.size() << " bytes" << endl;
                RunChecksums(data);
            }
        }
        else {
            CFileData data(cin);
            RunChecksums(data);
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    return CChecksumTestApp().AppMain(argc, argv);
}
