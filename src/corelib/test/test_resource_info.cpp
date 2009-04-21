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
#include <corelib/resource_info.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

/*
class CTestCryptApp : public CNcbiApplication
{
};
*/


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

    arg_desc->AddOptionalKey("res", "ResourceName",
        "Name of resource",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("pwd", "Password",
        "Password for accessing the resource",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("set", "NewValue",
        "Set new value for the selected resource",
        CArgDescriptions::eString);
    arg_desc->SetDependency("set", CArgDescriptions::eRequires, "res");
    arg_desc->SetDependency("set", CArgDescriptions::eRequires, "pwd");
    arg_desc->AddOptionalKey("extra", "ExtraValues",
        "Add extra values for the selected resource",
        CArgDescriptions::eString);
    arg_desc->SetDependency("extra", CArgDescriptions::eRequires, "set");
    arg_desc->AddFlag("del", "Delete the selected resource info", true);
    arg_desc->SetDependency("del", CArgDescriptions::eExcludes, "set");
    arg_desc->SetDependency("del", CArgDescriptions::eRequires, "res");
    arg_desc->SetDependency("del", CArgDescriptions::eRequires, "pwd");
    arg_desc->AddOptionalKey("file", "FileName",
        "Name of file to use",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("plain", "PlainTextFile",
        "Encrypt data from the plaintext input file. "
        "File format is <password> <res_name> <main_value> <extras> "
        "where password, res_name and main_value are URL-encoded, "
        "and <extras> have usual query-string format.",
        CArgDescriptions::eString);
    arg_desc->SetDependency("plain", CArgDescriptions::eExcludes, "set");
    arg_desc->SetDependency("plain", CArgDescriptions::eExcludes, "del");

    string prog_description = "Test for CNcbiResourceInfo\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CResInfoTest::Run(void)
{
    const CArgs& args = GetArgs();

    string filename = "resinfo";

    if ( args["file"] ) {
        filename = args["file"].AsString();
    }

    CNcbiResourceInfoFile info_file(filename);

    if (args["res"]  &&  args["pwd"]) {
        string res = args["res"].AsString();
        string pwd = args["pwd"].AsString();
        const CNcbiResourceInfo& info = info_file.GetResourceInfo(res, pwd);
        cout << "Current value: ";
        if ( info ) {
            cout << info.GetName() << " = '" << info.GetValue()
                << "' & '" << info.GetExtraValues().Merge() << "'" << endl;
        }
        else {
            cout << "not set" << endl;
        }
        if (args["set"] ) {
            CNcbiResourceInfo& nc_info = info_file.GetResourceInfo_NC(res, pwd);
            nc_info.SetValue(args["set"].AsString());
            if ( args["extra"] ) {
                nc_info.GetExtraValues_NC().GetPairs().insert(
                    CNcbiResourceInfo::TExtraValuesMap::value_type("extra",
                    args["extra"].AsString()));
            }
            _ASSERT( info_file.GetResourceInfo(res, pwd) );
            info_file.SaveFile();
            cout << "New value:     "
                << nc_info.GetName() << " = '" << nc_info.GetValue()
                << "' & '" << nc_info.GetExtraValues().Merge() << "'" << endl;
        }
        if ( args["del"] ) {
            info_file.DeleteResourceInfo(res, pwd);
            _ASSERT( !info_file.GetResourceInfo(res, pwd) );
            info_file.SaveFile();
            cout << "Deleted resource info for " << res << endl;
        }
    }

    if ( args["plain"] ) {
        info_file.ParsePlainTextFile(args["plain"].AsString());
        info_file.SaveFile();
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CResInfoTest().AppMain(argc, argv);
}
