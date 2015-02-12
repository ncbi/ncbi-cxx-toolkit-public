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
 *   Key generator for CNcbiEncrypt.
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
// Key generator application
//


class CKeyGeneratorApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CKeyGeneratorApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    string prog_description = "Key generator for CNcbiEncrypt\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CKeyGeneratorApp::Run(void)
{
    CRandom rand;
    rand.Randomize();
    string key;
    key.resize(32, 0);
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = rand.GetRand(0, 255);
    }
    key = CNcbiEncrypt::GenerateKey(key);
    string checksum = CNcbiEncrypt::GetKeyChecksum(key);
    cout << checksum << ':' << key << endl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CKeyGeneratorApp().AppMain(argc, argv);
}
