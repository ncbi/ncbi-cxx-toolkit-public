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
#include <test/test_assert.h>

USING_NCBI_SCOPE;

class CChecksumTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run (void);
};

void CChecksumTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CChecksum test program");

    arg_desc->AddFlag("selftest",
                      "Just verify behavior on internal test data");
    arg_desc->AddExtra(0, kMax_UInt, "files to process (stdin if none given)",
                       CArgDescriptions::eInputFile,
                       CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);

    SetupArgDescriptions(arg_desc.release());
}

static void s_ComputeSums(CNcbiIstream& is, CChecksum& crc32,
                          CChecksum& md5)
{
    while (!is.eof()) {
        char buf[289]; // use a weird size to force varying phases
        is.read(buf, sizeof(buf));

        size_t count = is.gcount();

        if (count) {
            crc32.AddChars(buf, count);
            md5  .AddChars(buf, count);
        }
    }
}

static bool s_VerifySum(const string& s, const string& md5hex)
{
    cerr << "Input: \"" << NStr::PrintableString(s) << '\"' << endl;
    bool ok = true;
    CNcbiIstrstream is(s.data(), s.size());
    CChecksum       new_crc32(CChecksum::eCRC32), new_md5(CChecksum::eMD5);
    s_ComputeSums(is, new_crc32, new_md5);
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
        return !ok;
    } else if (args.GetNExtra()) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            if (extra > 1) {
                cout << endl;
            }
            cout << "File: " << args[extra].AsString() << endl;
            CNcbiIstream& is = args[extra].AsInputFile();
            CChecksum crc32(CChecksum::eCRC32), md5(CChecksum::eMD5);
            s_ComputeSums(is, crc32, md5);
            crc32.WriteChecksumData(cout) << endl;
            md5.  WriteChecksumData(cout) << endl;
        }
    } else {
        CChecksum crc32(CChecksum::eCRC32), md5(CChecksum::eMD5);
        s_ComputeSums(cin, crc32, md5);
        crc32.WriteChecksumData(cout) << endl;
        md5.  WriteChecksumData(cout) << endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return CChecksumTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/05/17 21:09:26  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/07/29 21:29:26  ucko
* Add MD5 support (cribbed from the C Toolkit)
*
*
* ===========================================================================
*/
