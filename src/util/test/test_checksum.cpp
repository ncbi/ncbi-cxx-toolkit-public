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


// Test application

class CChecksumTestApp : public CNcbiApplication, CChecksumBase
{
public:
    // Dummy constructor to allow inheritance from CChecksumBase
    // that allow access to EMethodDef
    CChecksumTestApp(void) : CChecksumBase(eCRC32 /*dummy*/) {};

    void Init(void);
    int  Run (void);

public:
    // Get method name
    string GetMethodName(EMethodDef method);

    // Check that method is supported by CHash class
    bool IsHashMethod(EMethodDef method);
    // Check that method is supported by CChecksum class
    bool IsChecksumMethod(EMethodDef method);

    /// Checksum computation results
    union SValue {
        Uint8 v64;  // should go first for proper union initialization from scalar array
        Uint4 v32;
    };
    struct SCase
    {
        CChecksumBase::EMethodDef method;
        SValue      crc;
        const char* md5;
    };

    // Compute checksum for a given string
    void ComputeStrSum(const char* data, size_t size, CChecksum& sum);
    // Compute checksum for a big data
    void ComputeBigSum(CRandom& random, const CFileData& file_data, size_t offset, CChecksum& sum);
    // Verify computed checksum against known values
    bool VerifySum(const string& data_origin, CChecksumBase& sum, SCase& expected);

    // Self tests

    // CRC/MD5 string tests
    bool SelfTest_Str();
    // Additional Adler32 test
    bool SelfTest_Adler32();
    // Big data test
    bool SelfTest_Big();

    // Speed tests

    // Run speed tests on a file data (depends on arguments)
    void SpeedTests(const CFileData& file_data);
    // Run a specific speed test on a file data (for both CHash and CChecksum where available)
    void SpeedTest(CChecksumBase::EMethodDef method, const CFileData& file_data);
};


void CChecksumTestApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CChecksum test program");
    arg_desc->AddFlag("selftest",   "Verify behavior on internal test data");
    arg_desc->AddExtra(0, kMax_UInt, "files to process (stdin if none given)",
                       CArgDescriptions::eInputFile,
                       CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);
    arg_desc->AddFlag("print_tables",
                      "Print CRC table for inclusion in C++ code");
    SetupArgDescriptions(arg_desc.release());
}


string CChecksumTestApp::GetMethodName(EMethodDef method)
{
    switch ( method ) {
    case eCRC32:          return "CRC32";
    case eCRC32ZIP:       return "CRC32ZIP";
    case eCRC32INSD:      return "CRC32INSD";
    case eCRC32CKSUM:     return "CRC32CKSUM";
    case eCRC32C:         return "CRC32C";
    case eAdler32:        return "Adler32";
    case eMD5:            return "MD5";
    case eCityHash32:     return "CityHash32";
    case eCityHash64:     return "CityHash64";
    case eFarmHash32:     return "FarmHash32";
    case eFarmHash64:     return "FarmHash64";
    case eMurmurHash2_32: return "MurmurHash2_32";
    case eMurmurHash2_64: return "MurmurHash2_64";
    case eMurmurHash3_32: return "MurmurHash3_32";
    case eNone:
        ;
    }
    _TROUBLE;
}


bool CChecksumTestApp::IsHashMethod(EMethodDef method)
{
    switch ( method ) {
    case eCRC32:
    case eCRC32ZIP:
    case eCRC32INSD:
    case eCRC32CKSUM:
    case eCRC32C: 
    case eAdler32:
    case eCityHash32:
    case eCityHash64:
    case eFarmHash32:
    case eFarmHash64:
    case eMurmurHash2_32:
    case eMurmurHash2_64:
    case eMurmurHash3_32:
        return true;
    case eMD5:
        return false;
    case eNone:
        ;
    }
    _TROUBLE;
}


bool CChecksumTestApp::IsChecksumMethod(EMethodDef method)
{
    switch ( method ) {
    case eCRC32:
    case eCRC32ZIP:
    case eCRC32INSD:
    case eCRC32CKSUM:
    case eCRC32C: 
    case eAdler32:
    case eMD5:
        return true;
    case eCityHash32:
    case eCityHash64:
    case eFarmHash32:
    case eFarmHash64:
    case eMurmurHash2_32:
    case eMurmurHash2_64:
    case eMurmurHash3_32:
        return false;
    case eNone:
        ;
    }
    _TROUBLE;
}


bool CChecksumTestApp::VerifySum(const string& data_origin, CChecksumBase& sum, SCase& expected)
{
    EMethodDef m = sum.x_GetMethod();
    string method = GetMethodName(m);

    if ( m == eMD5 ) {
        if ( sum.GetResultHex() != expected.md5 ) {
            cerr << "FAILED "<< method << " for " << data_origin  << endl;
            cerr << "    Expected: " << hex << expected.md5       << endl;
            cerr << "    Computed: " << hex << sum.GetResultHex() << endl;
            return false;
        }
    }
    else 
    if (sum.GetBits() == 32) {
        if ( sum.GetResult32() != expected.crc.v32 ) {
            cerr << "FAILED "<< method << " for " << data_origin << endl;
            cerr << "    Expected: " << hex << expected.crc.v32  << endl;
            cerr << "    Computed: " << hex << sum.GetResult32() << endl;
            return false;
        }
    }
    else 
    if (sum.GetBits() == 64) {
        if ( sum.GetResult64() != expected.crc.v64 ) {
            cerr << "FAILED "<< method << " for " << data_origin << endl;
            cerr << "    Expected: " << hex << expected.crc.v64  << endl;
            cerr << "    Computed: " << hex << sum.GetResult64() << endl;
            return false;
        }
    } else{
        _TROUBLE;
    }
    return true;
}


// Note: MD5 test cases from RFC 1321

bool CChecksumTestApp::SelfTest_Str()
{
    struct STest
    {
        string str;
        vector<SCase> cases;
    };
    vector<STest> s_Tests = {
        { 
            // The standard test vector
            "123456789", {{ eCRC32ZIP, { 0xCBF43926 },  "" }}
        },
        { // CChecksum::Reset() test, except hash methods, that compute hash even for empty strings
          "", {
            { eCRC32,          { 0 },           "" },
            { eCRC32ZIP,       { 0 },           "" },
            { eCRC32INSD,      { 0xffffffff },  "" },
            { eCRC32CKSUM,     { 0xffffffff },  "" },
            { eCRC32C,         { 0 },           "" },
            { eAdler32,        { 0x00000001 },  "" },
            { eCityHash32,     { 0xdc56d17a },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x9ae16a3b2f90404f) }, "" },
            { eMurmurHash2_32, { 0x71b7ebf6 },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x1a14f106cd88c604) }, "" },
            { eMurmurHash3_32, { 0x3c46c9dc },  "" },
            { eMD5,            { 0 }, "d41d8cd98f00b204e9800998ecf8427e" }}
        },
        { "a", {
            { eCRC32,          { 0xa864db20 },  "" },
            { eCRC32ZIP,       { 0xe8b7be43 },  "" },
            { eCRC32INSD,      { 0x174841bc },  "" },
            { eCRC32CKSUM,     { 0x48c279fe },  "" },
            { eCRC32C,         { 0xc1d04330 },  "" },
            { eAdler32,        { 0x00620062 },  "" },
            { eCityHash32,     { 0x3c973d4d },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0xb3454265b6df75e3) }, "" },
            { eMurmurHash2_32, { 0xdd6eaa72 },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0xe78727fd27938759) }, "" },
            { eMurmurHash3_32, { 0x95352c35 },  "" },
            { eMD5,            { 0 }, "0cc175b9c0f1b6a831c399e269772661" }}
        },
        { "abc", {
            { eCRC32,          { 0x2c17398c },  "" },
            { eCRC32ZIP,       { 0x352441c2 },  "" },
            { eCRC32INSD,      { 0xcadbbe3d },  "" },
            { eCRC32CKSUM,     { 0x48aa78a2 },  "" },
            { eCRC32C,         { 0x364b3fb7 },  "" },
            { eAdler32,        { 0x024d0127 },  "" },
            { eCityHash32,     { 0x2f635ec7 },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x24a5b3a074e7f369) }, "" },
            { eMurmurHash2_32, { 0xe64e1d8a },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x9d373123f31f4b40) }, "" },
            { eMurmurHash3_32, { 0xcdb7804f },  "" },
            { eMD5,            { 0 }, "900150983cd24fb0d6963f7d28e17f72" }}
        },
        { "message digest", {
            { eCRC32,          { 0x5c57dedc },  "" },
            { eCRC32ZIP,       { 0x20159d7f },  "" },
            { eCRC32INSD,      { 0xdfea6280 },  "" },
            { eCRC32CKSUM,     { 0xd934b396 },  "" },
            { eCRC32C,         { 0x02bd79d0 },  "" },
            { eAdler32,        { 0x29750586 },  "" },
            { eCityHash32,     { 0x246f52b3 },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x8db193972bf98c6a) }, "" },
            { eMurmurHash2_32, { 0x00e65523 },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x9ccccdc1590b7884) }, "" },
            { eMurmurHash3_32, { 0x46558202 },  "" },
            { eMD5,            { 0 }, "f96b697d7cb7938d525a2f31aaf161d0" }}
        },
        { "abcdefghijklmnopqrstuvwxyz", {
            { eCRC32,          { 0x3bc2a463 },  "" },
            { eCRC32ZIP,       { 0x4c2750bd },  "" },
            { eCRC32INSD,      { 0xb3d8af42 },  "" },
            { eCRC32CKSUM,     { 0xa1b937a8 },  "" },
            { eCRC32C,         { 0x9ee6ef25 },  "" },
            { eAdler32,        { 0x90860b20 },  "" },
            { eCityHash32,     { 0xaa02c5c1 },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x5ead741ce7ac31bd) }, "" },
            { eMurmurHash2_32, { 0xb24869d9 },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x31a588f6a77a8210) }, "" },
            { eMurmurHash3_32, { 0x98aa3c36 },  "" },
            { eMD5,            { 0 }, "c3fcd3d76192e4007dfb496cca67e13b" }}
        },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {
            { eCRC32,          { 0x26910730 },  "" },
            { eCRC32ZIP,       { 0x1fc2e6d2 },  "" },
            { eCRC32INSD,      { 0xe03d192d },  "" },
            { eCRC32CKSUM,     { 0x04e1f937 },  "" },
            { eCRC32C,         { 0xa245d57d },  "" },
            { eAdler32,        { 0x8adb150c },  "" },
            { eCityHash32,     { 0xa77b8219 },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x4566c1e718836cd6) }, "" },
            { eMurmurHash2_32, { 0x648c1aef },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x8c72371817be471b) }, "" },
            { eMurmurHash3_32, { 0x2566f7c8 },  "" },
            { eMD5,            { 0 }, "d174ab98d277d9f5a5611c2c9f419d9f" }}
        },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", {
            { eCRC32,          { 0x7110bde7 },  "" },
            { eCRC32ZIP,       { 0x7ca94a72 },  "" },
            { eCRC32INSD,      { 0x8356b58d },  "" },
            { eCRC32CKSUM,     { 0x73a0b3a8 },  "" },
            { eCRC32C,         { 0x477a6781 },  "" },
            { eAdler32,        { 0x97b61069 },  "" },
            { eCityHash32,     { 0x725594c0 },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0x791a4c16629ee4cd) }, "" },
            { eMurmurHash2_32, { 0xf334c676 },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0x48cfebd7cd2eac2f) }, "" },
            { eMurmurHash3_32, { 0x6029055e },  "" },
            { eMD5,            { 0 }, "57edf4a22be3c955ac49da2e2107b67a" }}
        },
        { string("\1\xc0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\xfe\x60\xac\0\0\0"
                 "\x8\0\0\0\x4\0\0\0\x9\x25\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 48), {
            { eCRC32,          { 0xfbcf2b84 },  "" },
            { eCRC32ZIP,       { 0xc897a166 },  "" },
            { eCRC32INSD,      { 0x37685e99 },  "" },
            { eCRC32CKSUM,     { 0x46152007 },  "" },
            { eCRC32C,         { 0x99b08a14 },  "" },
            { eAdler32,        { 0x65430307 },  "" },
            { eCityHash32,     { 0x5a50eecd },  "" },
            { eCityHash64,     { NCBI_CONST_UINT8(0xca8a7f96d3b805dd) }, "" },
            { eMurmurHash2_32, { 0x38abf1aa },  "" },
            { eMurmurHash2_64, { NCBI_CONST_UINT8(0xf47f0a3ee8e9fa45) }, "" },
            { eMurmurHash3_32, { 0x084d2e6b },  "" },
            { eMD5,            { 0 }, "f16de75fd4137d2b5b12f35f247a6214" }}
        }
    };

    bool ok = true;
    for (auto test : s_Tests) {
        for (auto testcase : test.cases) {
            if ( IsHashMethod(testcase.method) ) {
                CHash hash((CHash::EMethod)testcase.method);
                hash.Calculate(test.str.data(), test.str.size());
                ok &= VerifySum("string '" + NStr::PrintableString(test.str) + "'", hash, testcase);
            }
            if ( IsChecksumMethod(testcase.method) ) {
                CChecksum sum((CChecksum::EMethod)testcase.method);
                ComputeStrSum(test.str.data(), test.str.size(), sum);
                ok &= VerifySum("string '" + NStr::PrintableString(test.str) + "'", sum, testcase);
            }
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



bool CChecksumTestApp::SelfTest_Big()
{
    vector<SCase> s_Tests = {
        { eCRC32,          { 0xa0b29e2f },  "" },
        { eCRC32ZIP,       { 0x39a90823 },  "" },
        { eCRC32INSD,      { 0xc656f7dc },  "" },
        { eCRC32CKSUM,     { 0x4a0a1bdb },  "" },
        { eCRC32C,         { 0x7adb1cd3 },  "" },
        { eAdler32,        { 0x528a7135 },  "" },
        { eCityHash32,     { 0xca83e47e },  "" },
        { eCityHash64,     { NCBI_CONST_UINT8(0xb2195ac46438d77d) }, "" },
        { eMurmurHash2_32, { 0x65547016 },  "" },
        { eMurmurHash2_64, { NCBI_CONST_UINT8(0x971d5ff461a89662) },  "" },
        { eMurmurHash3_32, { 0xa7e2d7ae },  "" },
        { eMD5,            { 0 }, "a1ed665e33b6feb5a645738b4384ca25" }
    };
    const char*  kFileName = "test_data/checksum.dat";
    const size_t kOffsets  = 16;

    bool ok = true;
    CRandom random;

    cout << "File: " << kFileName;
    CFileData file_data(kFileName);
    cout << ", size: " << file_data.size() << " bytes" << endl;

    for (auto testcase : s_Tests) {
        if ( IsHashMethod(testcase.method) ) {
            CHash hash((CHash::EMethod)testcase.method);
            hash.Calculate(file_data.data(), file_data.size());
            ok &= VerifySum("Hash for file", hash, testcase);
        }
        if ( IsChecksumMethod(testcase.method) ) {
            for (size_t offset = 0; offset < kOffsets; ++offset) {
                CChecksum sum((CChecksum::EMethod)testcase.method);
                ComputeBigSum(random, file_data, offset, sum);
                ok &= VerifySum("Checksum for file (" + NStr::NumericToString(offset) + ")", sum, testcase);
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


void CChecksumTestApp::SpeedTests(const CFileData& data)
{
    SpeedTest( eCRC32,          data );
    SpeedTest( eCRC32ZIP,       data );
    SpeedTest( eCRC32CKSUM,     data );
    SpeedTest( eCRC32C,         data );
    SpeedTest( eAdler32,        data );
    SpeedTest( eCityHash32,     data );
    SpeedTest( eFarmHash32,     data );
    SpeedTest( eCityHash64,     data );
    SpeedTest( eFarmHash64,     data );
    SpeedTest( eMurmurHash2_32, data );
    SpeedTest( eMurmurHash2_64, data );
    SpeedTest( eMurmurHash3_32, data );
    SpeedTest( eMD5,            data );
}


void CChecksumTestApp::SpeedTest(CChecksumBase::EMethodDef method,  const CFileData& file_data)
{
    CStopWatch timer;
    CChecksumBase* base = NULL;
    CHash hash((CHash::EMethod)method);
    CChecksum sum((CChecksum::EMethod)method);
    
    if ( IsHashMethod(method) ) {
        base = &hash;
        timer.Start();
        hash.Calculate(file_data.data(), file_data.size());
        timer.Stop();
    } else {
        base = &sum;
        timer.Start();
        sum.AddChars(file_data.data(), file_data.size());
        timer.Stop();
    }
    string time = timer.AsString();
    cout << "Processed in " << time << ": ";
    cout << setw(14) << GetMethodName(method) << ": "
         << setw(8)  << base->GetResultHex() << endl;
}


int CChecksumTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    // Optional global seed for hash methods.
    CHash::SetSeed(12345);

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
        // Run tests on all files specified in a command line
        if (args.GetNExtra()) {
            for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
                if (extra > 1) {
                    cout << endl;
                }
                const string& filename = args[extra].AsString();
                cout << "File: " << filename;
                CFileData data(filename);
                cout << ", size: " << data.size() << " bytes" << endl;
                SpeedTests(data);
            }
        }
        // otherwise, use data from standard input
        else {
            CFileData data(cin);
            SpeedTests(data);
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    return CChecksumTestApp().AppMain(argc, argv);
}
