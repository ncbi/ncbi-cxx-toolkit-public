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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CArgTestApplication::


class CArgTestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CArgTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    // Describe the expected command-line arguments
    arg_desc->AddOptionalPositional
        ("logfile",
         "This is an optional named positional argument without default value",
         CArgDescriptions::eOutputFile,
         CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);

    arg_desc->AddFlag
        ("f1",
         "This is a flag argument: TRUE if set, FALSE if not set");

    arg_desc->AddPositional
        ("barfooetc",
         "This is a mandatory plain (named positional) argument",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("barfooetc", 
        &(*new CArgAllow_Strings, "foo", "bar", "etc"),
        CArgDescriptions::eConstraint);

    arg_desc->AddDefaultKey
        ("kd", "DefaultKey",
         "This is an optional integer key argument, with default value",
         CArgDescriptions::eInteger, "123");
    arg_desc->SetConstraint
        ("kd", new CArgAllow_Integers(0, 200));

    arg_desc->AddExtra
        (0,  // no mandatory extra args
         3,  // up to 3 optional extra args
         "These are the optional extra (unnamed positional) arguments. "
         "They will be printed out to the file specified by the "
         "2nd positional argument,\n\"logfile\"",
         CArgDescriptions::eBoolean);

    arg_desc->AddKey
        ("k", "MandatoryKey",
         "This is a mandatory alpha-num key argument",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("k", new CArgAllow_String(CArgAllow_Symbols::eAlnum));

    arg_desc->AddOptionalKey
        ("ko", "OptionalKey",
         "This is another optional key argument, without default value",
         CArgDescriptions::eBoolean,
         CArgDescriptions::fAllowMultiple);

    arg_desc->AddFlag
        ("f2",
         "This is another flag argument: FALSE if set, TRUE if not set",
         false);

    arg_desc->AddDefaultPositional
        ("one_symbol",
         "This is an optional named positional argument with default value",
         CArgDescriptions::eString, "a");
    arg_desc->SetConstraint
        ("one_symbol", new CArgAllow_Symbols(" aB\tCd"));


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CArgTestApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    // Do run
    cout << string(72, '=') << endl;

    // Self test
    assert(args.Exist("f1"));
    assert(args.Exist("logfile"));
    assert(args["barfooetc"]);

    // Stream to result output
    // (NOTE: "x_lg" is just a workaround for bug in SUN WorkShop 5.1 compiler)
    ostream* x_lg = args["logfile"] ? &args["logfile"].AsOutputFile() : &cout;
    ostream& lg = *x_lg;

    if ( args["logfile"] )
        cout << "Printing arguments to file `"
             << args["logfile"].AsString() << "'..." << endl;

    // Printout argument values
    lg << "k:         " << args["k"].AsString() << endl;
    lg << "barfooetc: " << args["barfooetc"].AsString() << endl;
    if ( args["logfile"] )
        lg << "logfile:   " << args["logfile"].AsString() << endl;

    if ( args["ko"] ) {
        lg << "ko:        " << NStr::BoolToString(args["ko"].AsBoolean())
           << endl;

        const CArgValue::TStringArray& ko_values = 
            args["ko"].GetStringList();
        if (!ko_values.empty()) {
            lg << "ko list:";
            ITERATE(CArgValue::TStringArray, it, ko_values) {
                lg << *it << ", ";
            }
            lg << endl;
        }

    } else {
        lg << "ko:        not provided" << endl;
        bool is_thrown = false;
        try {
            (void) args["ko"].AsString();
        } catch (CArgException& e) {
            NCBI_REPORT_EXCEPTION("CArgException is thrown:",e);
            is_thrown = true;
        }
        assert(is_thrown);
    }

    if ( args["f1"] ) {
        assert(args["f1"].AsBoolean());
    }
    if ( args["f2"] ) {
        assert(args["f2"].AsBoolean());
    }

    // Extra (unnamed positional) arguments
    if ( args.GetNExtra() ) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            lg << "#" << extra << ":        "
               << NStr::BoolToString(args[extra].AsBoolean())
               << "   (passed as `" << args[extra].AsString() << "')"
               << endl;
        }
    } else {
        lg << "(no unnamed positional arguments passed in the cmd-line)"
           << endl;
    }

    // Separator
    lg << string(44, '-') << endl;

    // Printout obtained argument values
    string str;
    cout << args.Print(str) << endl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup

void CArgTestApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CArgTestApplication().AppMain(argc, argv);
}
