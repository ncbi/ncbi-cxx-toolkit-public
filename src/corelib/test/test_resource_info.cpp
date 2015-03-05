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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Test for secure resources API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/resource_info.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//


class CResInfoTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CResInfoTest::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    string prog_description = "Test for CNcbiResourceInfo\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


const char*  src_name  = "resinfo_plain.txt";

const char*  test1_res = "resource_info/some_user@some_server";
const char*  test1_pwd = "resinfo_password";
const char*  test1_val = "server_password";

const char*  test2_res = "resource_info/some_user@some_server";
const char*  test2_pwd = "resinfo_password2";
const char*  test2_val = "server_password2";
const char*  test2_ex  = "username=anyone&server=server_name";

const char*  test3_res = "resource_info2/another_user@another_server";
const char*  test3_pwd = "resinfo_password";
const char*  test3_val = "server_password";
const char*  test3_ex  = "username=onemore&server=server_name";

typedef CNcbiResourceInfo::TExtraValuesMap TExtraValuesMap;
typedef CNcbiResourceInfo::TExtraValues    TExtraValues;


void CheckExtra(const CNcbiResourceInfo& info, const string& ref)
{
    // The order of extra values is undefined
    const TExtraValuesMap& info_ex = info.GetExtraValues().GetPairs();
    TExtraValues ref_ex_pairs;
    ref_ex_pairs.Parse(ref);
    const TExtraValuesMap& ref_ex = ref_ex_pairs.GetPairs();
    _ASSERT(info_ex.size() == ref_ex.size());
    ITERATE(TExtraValuesMap, it, info_ex) {
        TExtraValuesMap::const_iterator match = ref_ex.find(it->first);
        _ASSERT(match != ref_ex.end());
        _ASSERT(match->second == it->second);
    }
}


int CResInfoTest::Run(void)
{
    CFileDeleteAtExit file_guard; /* NCBI_FAKE_WARNING */
    string enc_name = CFile::GetTmpName();
    _ASSERT(!enc_name.empty());
    //file_guard.Add(enc_name);

    {{
        // Encode and save the source plaintext file
        CNcbiResourceInfoFile newfile(enc_name);
        newfile.ParsePlainTextFile(src_name);
        newfile.SaveFile();
    }}

    // Load the created file and get some resource info
    CNcbiResourceInfoFile resfile(enc_name);

    const CNcbiResourceInfo& info1 =
        resfile.GetResourceInfo(test1_res, test1_pwd);
    _ASSERT(info1); // success?
    _ASSERT(info1.GetValue() == test1_val); // check main value
    _ASSERT(info1.GetExtraValues().GetPairs().empty()); // no extra data

    const CNcbiResourceInfo& info2 =
        resfile.GetResourceInfo(test2_res, test2_pwd);
    _ASSERT(info2); // success?
    _ASSERT(info2.GetValue() == test2_val); // check main value
    CheckExtra(info2, test2_ex);

    const CNcbiResourceInfo& info3 =
        resfile.GetResourceInfo(test3_res, test3_pwd);
    _ASSERT(info3); // success?
    _ASSERT(info3.GetValue() == test3_val); // check main value
    CheckExtra(info3, test3_ex);

    // Test string encryption/decrypton using an explicit password.
    string data = "Test CNcbiEncrypt class";
    string key = CNcbiEncrypt::GenerateKey("foobar");
    _ASSERT(key == "1BCB50C9A5FC53A1608242FE827BAE228:0AA2C441A5F2F3DB7E9565E9349C18AB");
    string encr = CNcbiEncrypt::Encrypt(data, "foobar");
    _ASSERT(encr == "1BCB50C9A5FC53A1608242FE827BAE228:A8F7030E91CCF4E5FDF7C0F3F734BEBBDA2AAB8583729E9E5F438D1E569F2F21");
    string decr = CNcbiEncrypt::Decrypt(encr, "foobar");
    _ASSERT(decr == data);

    // Test decryption using ncbi keys file, the key is a non-default one
    // (not the first key in the file).
    decr = CNcbiEncrypt::Decrypt(encr);
    _ASSERT(decr == data);

    // Test automatic key selection.
    encr = CNcbiEncrypt::Encrypt(data);
    _ASSERT(encr == "1E5606B599D707645329ABE4E0E3F6AC9:52CE4D659462C9ABA48CF7588D8E1FD9D5CD52EFCE578B0A40AF9B4D1208CF9F");
    decr = CNcbiEncrypt::Decrypt(encr);
    _ASSERT(decr == data);

    // Test domain encryption.
    encr = CNcbiEncrypt::EncryptForDomain(data, "domain");
    _ASSERT(encr == "11BDF0BA7079A8C2BD6656D3CF2D79160:3F7930D402567001F058086D263539596792628CEEF15AFE9D7E84FCD9C7BC14/domain");
    // Automatic domain key selection.
    decr = CNcbiEncrypt::Decrypt(encr);
    _ASSERT(decr == data);
    // Explicit domain
    decr = CNcbiEncrypt::DecryptForDomain(encr, "domain");
    _ASSERT(decr == data);
    // Two domains
    decr = CNcbiEncrypt::DecryptForDomain(encr, "domain2");
    _ASSERT(decr == data);

    // Test IsEncrypted()
    // Encrypted data
    _ASSERT(CNcbiEncrypt::IsEncrypted("1E5606B599D707645329ABE4E0E3F6AC9:52CE4D659462C9ABA48CF7588D8E1FD9D5CD52EFCE578B0A40AF9B4D1208CF9F"));
    // Encrypted data with domain
    _ASSERT(CNcbiEncrypt::IsEncrypted("11BDF0BA7079A8C2BD6656D3CF2D79160:3F7930D402567001F058086D263539596792628CEEF15AFE9D7E84FCD9C7BC14/domain"));
    // False-positive - the format is correct, but the string contains garbage.
    _ASSERT(CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // False-positive - the format is correct (with domain), but the string contains garbage.
    _ASSERT(CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF/not an actual domain"));
    // Empty domain
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF/"));
    // Missing version
    _ASSERT(!CNcbiEncrypt::IsEncrypted("0123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // Invalid version
    _ASSERT(!CNcbiEncrypt::IsEncrypted("00123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // Wrong checksum length
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDE:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEFF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // Checksum is not a HEX value
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEz:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // Missing checksum separator
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    // Empty encrypted part
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:"));
    // Data has wrong length
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDE"));
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFF"));
    // Data is not a HEX string
    _ASSERT(!CNcbiEncrypt::IsEncrypted("10123456789ABCDEF0123456789ABCDEF:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEz"));
    cout << "All tests passed" << endl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CResInfoTest().AppMain(argc, argv);
}
