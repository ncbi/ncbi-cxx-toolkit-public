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
#include <util/checksum.hpp>

// must be last
#include <common/test_assert.h>

USING_NCBI_SCOPE;

class CChecksumTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
    void RunChecksum(CChecksum::EMethod type);
    void RunChecksum(CChecksum::EMethod type, CNcbiIstream& is);
};

void CChecksumTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CChecksum test program");

    arg_desc->AddFlag("selftest",
                      "Just verify behavior on internal test data");
    arg_desc->AddFlag("CRC32",
                      "Run CRC32 test only on files.");
    arg_desc->AddFlag("CRC32ZIP",
                      "Run CRC32ZIP test only on files.");
    arg_desc->AddFlag("MD5",
                      "Run MD5 test only on files.");
    arg_desc->AddFlag("Adler32",
                      "Run Adler32 test only on files.");
    arg_desc->AddExtra(0, kMax_UInt, "files to process (stdin if none given)",
                       CArgDescriptions::eInputFile,
                       CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);
    arg_desc->AddFlag("print_tables",
                      "Print CRC table for inclusion in C++ code");

    SetupArgDescriptions(arg_desc.release());
}

static void s_ComputeSums(CNcbiIstream& is,
                          CChecksum& crc32,
                          CChecksum& crc32zip,
                          CChecksum& md5)
{
    while (!is.eof()) {
        char buf[289]; // use a weird size to force varying phases
        is.read(buf, sizeof(buf));

        size_t count = is.gcount();

        if (count) {
            crc32.AddChars(buf, count);
            crc32zip.AddChars(buf, count);
            md5  .AddChars(buf, count);
        }
    }
}

static bool s_VerifySum(const string& s, const string& md5hex)
{
    cerr << "Input: \"" << NStr::PrintableString(s) << '\"' << endl;
    bool ok = true;
    CNcbiIstrstream is(s.data(), s.size());
    CChecksum       new_crc32(CChecksum::eCRC32);
    CChecksum       new_crc32zip(CChecksum::eCRC32ZIP);
    CChecksum       new_md5(CChecksum::eMD5);
    s_ComputeSums(is, new_crc32, new_crc32zip, new_md5);
#if 0
    cerr << "Expected CRC32: " << hex << crc32 << endl;
    cerr << "Computed CRC32: " << hex << new_crc32.GetChecksum() << endl;
    if (crc32 != new_crc32.GetChecksum()) {
        cerr << "FAILED!" << endl;
        ok = false;
    }
#endif
    CNcbiOstrstream oss;
    new_md5.WriteChecksumData(oss);
    string new_md5hex = CNcbiOstrstreamToString(oss);
    cerr << "Expected MD5: " << md5hex << endl;
    cerr << "Computed " << new_md5hex << endl;
    if ( !NStr::EndsWith(new_md5hex, md5hex) ) {
        cerr << "FAILED!" << endl;
        ok = false;
    }
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
    if ( ok ) {
        cerr << "Adler32 test passed" << endl;
    }
    else {
        cerr << "Adler32 test failed" << endl;
    }
    return ok;
}

const int BUF_SIZE = 1<<13;
const int BUF_COUNT = 1;

void CChecksumTestApp::RunChecksum(CChecksum::EMethod type,
                                   CNcbiIstream& is)
{
    CStopWatch timer(CStopWatch::eStart);
    CChecksum sum(type);
    while (!is.eof()) {
        char buf[BUF_SIZE]; // reasonable buffer size
        is.read(buf, sizeof(buf));

        size_t count = is.gcount();

        if (count) {
            for ( int k = 0; k < BUF_COUNT; ++k )
                sum.AddChars(buf, count);
        }
    }

    string time = timer.AsString();
    cout << "Processed in "<<time<<": ";
    sum.WriteChecksumData(cout) << endl;
}

void CChecksumTestApp::RunChecksum(CChecksum::EMethod type)
{
    const CArgs& args = GetArgs();
    if (args.GetNExtra()) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            if (extra > 1) {
                cout << endl;
            }
            cout << "File: " << args[extra].AsString() << endl;
            CNcbiIstream& is = args[extra].AsInputFile();
            RunChecksum(type, is);
        }
    }
}

int CChecksumTestApp::Run(void)
{
    CArgs args = GetArgs();
    if (args["selftest"]) {
        // Not testing CRCs for now, since I can't find an external
        // implementation that agrees with what we have.

        // MD5 test cases from RFC 1321
        bool ok = s_VerifySum(kEmptyStr, "d41d8cd98f00b204e9800998ecf8427e");
        ok &= s_VerifySum("a", "0cc175b9c0f1b6a831c399e269772661");
        ok &= s_VerifySum("abc", "900150983cd24fb0d6963f7d28e17f72");
        ok &= s_VerifySum("message digest",
                          "f96b697d7cb7938d525a2f31aaf161d0");
        ok &= s_VerifySum("abcdefghijklmnopqrstuvwxyz",
                          "c3fcd3d76192e4007dfb496cca67e13b");
        ok &= s_VerifySum
            ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
             "d174ab98d277d9f5a5611c2c9f419d9f");
        ok &= s_VerifySum("1234567890123456789012345678901234567890123456789"
                          "0123456789012345678901234567890",
                          "57edf4a22be3c955ac49da2e2107b67a");
        ok &= s_Adler32Test();
        return !ok;
    }
    else if ( args["print_tables"] ) {
        CChecksum::PrintTables(NcbiCout);
    }
    else {
        bool run_crc32 = args["CRC32"];
        bool run_crc32zip = args["CRC32ZIP"];
        bool run_md5 = args["MD5"];
        bool run_adler32 = args["Adler32"];
        if ( run_crc32 | run_crc32zip | run_md5 | run_adler32 ) {
            if ( run_crc32 ) {
                RunChecksum(CChecksum::eCRC32);
            }
            if ( run_crc32zip ) {
                RunChecksum(CChecksum::eCRC32ZIP);
            }
            if ( run_md5 ) {
                RunChecksum(CChecksum::eMD5);
            }
            if ( run_adler32 ) {
                RunChecksum(CChecksum::eAdler32);
            }
        }
        else {
            if (args.GetNExtra()) {
                for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
                    if (extra > 1) {
                        cout << endl;
                    }
                    cout << "File: " << args[extra].AsString() << endl;
                    CNcbiIstream& is = args[extra].AsInputFile();
                    CChecksum crc32(CChecksum::eCRC32);
                    CChecksum crc32zip(CChecksum::eCRC32ZIP);
                    CChecksum md5(CChecksum::eMD5);
                    s_ComputeSums(is, crc32, crc32zip, md5);
                    crc32.WriteChecksumData(cout) << endl;
                    crc32zip.WriteChecksumData(cout) << endl;
                    md5.  WriteChecksumData(cout) << endl;
                }
            } else {
                CChecksum crc32(CChecksum::eCRC32);
                CChecksum crc32zip(CChecksum::eCRC32ZIP);
                CChecksum md5(CChecksum::eMD5);
                s_ComputeSums(cin, crc32, crc32zip, md5);
                crc32.WriteChecksumData(cout) << endl;
                crc32zip.WriteChecksumData(cout) << endl;
                md5.  WriteChecksumData(cout) << endl;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    return CChecksumTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
