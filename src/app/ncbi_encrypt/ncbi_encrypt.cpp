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
 *   Encryption/decryption utility and key generator for CNcbiEncrypt.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/resource_info.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////////
//
// NCBI encryption application
//


class CNcbiEncryptApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
private:
    void GenerateKey(void);
    void Encrypt(void);
    void Decrypt(void);
};


void CNcbiEncryptApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    string prog_description = "NCBI encryption utility\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    arg_desc->AddFlag("encrypt", "Encrypt input data (default action)", true);
    arg_desc->AddFlag("decrypt", "Decrypt input data", true);
    arg_desc->AddFlag("generate_key", "Generate encryption key", true);

    arg_desc->AddDefaultKey("i", "Input",
        "input data file", CArgDescriptions::eInputFile, "-");
    arg_desc->AddDefaultKey("o", "Output",
        "output data file", CArgDescriptions::eOutputFile, "-");
    arg_desc->AddOptionalKey("password", "Password",
        "Password used for key generation", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("severity", "Severity",
        "Log message severity when reporting an outdated key usage",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("severity",
        CArgAllow_Strings()
        .AllowValue("Info")
        .AllowValue("Warning")
        .AllowValue("Error")
        .AllowValue("Critical"));

    arg_desc->SetDependency("encrypt", CArgDescriptions::eExcludes, "decrypt");
    arg_desc->SetDependency("generate_key", CArgDescriptions::eExcludes, "i");
    arg_desc->SetDependency("generate_key", CArgDescriptions::eExcludes, "encrypt");
    arg_desc->SetDependency("generate_key", CArgDescriptions::eExcludes, "decrypt");
    arg_desc->SetDependency("severity", CArgDescriptions::eRequires, "generate_key");

    SetupArgDescriptions(arg_desc.release());
}


int CNcbiEncryptApp::Run(void)
{
    const CArgs& args = GetArgs();
    if ( args["generate_key"] ) {
        GenerateKey();
    }
    else if ( args["decrypt"] ) {
        Decrypt();
    }
    else { // Do not check 'encrypt' flag - this is the default operation
        Encrypt();
    }
    return 0;
}


void CNcbiEncryptApp::GenerateKey(void)
{
    const CArgs& args = GetArgs();
    string seed;
    if ( args["password"] ) {
        seed = args["password"].AsString();
    }
    else {
        CRandom rand;
        rand.Randomize();
        seed.resize(32, 0);
        for (size_t i = 0; i < seed.size(); i++) {
            seed[i] = rand.GetRand(0, 255);
        }
    }
    string key = CNcbiEncrypt::GenerateKey(seed);
    CNcbiOstream& out = args["o"].AsOutputFile();
    out << key;
    if ( args["severity"] ) {
        out << "/" << args["severity"].AsString();
    }
    out << endl;
}


void CNcbiEncryptApp::Encrypt(void)
{
    const CArgs& args = GetArgs();
    string encr;
    CNcbiIstream& in = args["i"].AsInputFile();
    CNcbiOstrstream tmp;
    tmp << in.rdbuf();
    string data = CNcbiOstrstreamToString(tmp);

    if ( args["password"] ) {
        encr = CNcbiEncrypt::Encrypt(data, args["password"].AsString());
    }
    else {
        encr = CNcbiEncrypt::Encrypt(data);
    }

    CNcbiOstream& out = args["o"].AsOutputFile();
    out << encr;
    out.flush();
}


void CNcbiEncryptApp::Decrypt(void)
{
    const CArgs& args = GetArgs();
    string decr;
    CNcbiIstream& in = args["i"].AsInputFile();
    CNcbiOstrstream tmp;
    tmp << in.rdbuf();
    string data = CNcbiOstrstreamToString(tmp);
    NStr::TruncateSpacesInPlace(data);

    if ( args["password"] ) {
        decr = CNcbiEncrypt::Decrypt(data, args["password"].AsString());
    }
    else {
        decr = CNcbiEncrypt::Decrypt(data);
    }

    CNcbiOstream& out = args["o"].AsOutputFile();
    out << decr;
    out.flush();
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CNcbiEncryptApp().AppMain(argc, argv);
}
