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
 * Authors:  Anton Butanayev, Denis Vakatov
 *
 * File Description:
 *   Test for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.9  2000/11/13 20:31:09  vakatov
 * Wrote new test, fixed multiple bugs, ugly "features", and the USAGE.
 *
 * Revision 6.8  2000/10/20 20:26:11  butanaev
 * Modified example #9.
 *
 * Revision 6.7  2000/10/11 21:03:50  vakatov
 * Cleanup to avoid 64-bit to 32-bit values truncation, etc.
 * (reported by Forte6 Patch 109490-01)
 *
 * Revision 6.6  2000/10/06 21:57:07  butanaev
 * Added Allow() function. Added classes CArgAllowValue, CArgAllowIntInterval.
 *
 * Revision 6.5  2000/09/29 17:11:01  butanaev
 * Got rid of IsDefaultValue(), added IsProvided().
 *
 * Revision 6.4  2000/09/28 21:00:21  butanaev
 * fPreOpen with opposite meaning took over fDelayOpen.
 * IsDefaultValue() added which returns true if no
 * value for an optional argument was provided in cmd. line.
 *
 * Revision 6.3  2000/09/22 21:26:28  butanaev
 * Added example with default arg values.
 *
 * Revision 6.2  2000/09/12 15:01:30  butanaev
 * Examples now switching by environment variable EXAMPLE_NUM.
 *
 * Revision 6.1  2000/08/31 23:55:35  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <corelib/ncbiargs.hpp>

USING_NCBI_SCOPE;


// Extended test for all different types of arguments  [default test]
static CArgs* s_Test0(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    // Describe the expected command-line arguments
    arg_desc.AddKey
        ("k", "MandatoryKey",
         "This is a mandatory alpha-num key argument",
         CArgDescriptions::eAlnum);

    arg_desc.AddOptionalKey
        ("ko", "OptionalKey",
         "This is an optional integer key argument",
         CArgDescriptions::eInteger, 0/*no flags*/,
         "123");
    arg_desc.SetConstraint
        ("ko", new CArgAllow_Integers(0, 200));

    arg_desc.AddOptionalKey
        ("ko2", "OptionalKey2",
         "This is another optional key argument",
         CArgDescriptions::eBoolean, 0/*no flags*/,
         "True");

    arg_desc.AddFlag
        ("f1",
         "This is a flag argument:  TRUE if set, FALSE if not set");

    arg_desc.AddFlag
        ("f2",
         "This is another flag argument:  FALSE if set, TRUE if not set",
         false);

    arg_desc.AddPlain
        ("p",
         "This is a mandatory plain (named positional) argument",
         CArgDescriptions::eString);
    arg_desc.SetConstraint
        ("p", &(*new CArgAllow_Strings, "foo", "bar", "etc"));

    arg_desc.AddPlain
        ("po",
         "This is an optional plain (named positional) argument",
         CArgDescriptions::eOutputFile,
         CArgDescriptions::fPreOpen | CArgDescriptions::fBinary,
         "-");

    arg_desc.AddExtra
        ("These are the optional extra (unnamed positional) arguments. "
         "They will be printed out to the file specified by the "
         "2nd positional argument, \"po\"",
         CArgDescriptions::eBoolean);


    // Put constraint on the # of positional arguments (named and unnamed)
    arg_desc.SetConstraint(CArgDescriptions::eMoreOrEqual, 1);


    // Parse command-line arguments ("args", "argv[]") into "CArgs"
    CArgs& args = *arg_desc.CreateArgs(argc, argv);


    // Print all extra (unnamed positional) arguments
    cout << "Printing unnamed positional arguments to file `"
         << args["po"].AsString() << "'..." << endl;
    
    ostream& os = args["po"].AsOutputFile();
    if ( args.GetNExtra() ) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            os << "#" << extra << ":  "
               << NStr::BoolToString(args[extra].AsBoolean())
               << "   (passed as `" << args[extra].AsString() << "')"
               << endl;
        }
    } else {
        os << "...no unnamed positional arguments provided in the cmd-line"
           << endl;
    }


    // Return to Main
    return &args;
}


// Allowing
static CArgs* s_Test9(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddKey("a",
                    "alphaNumericKey",
                    "This is a test alpha-num argument",
                    CArgDescriptions::eAlnum);

    arg_desc.AddKey("i",
                    "integerKey",
                    "This is a test integer argument",
                    CArgDescriptions::eInteger);

    arg_desc.SetConstraint("a", &(*new CArgAllow_Strings, "foo", "bar", "qq"));
    arg_desc.SetConstraint("i", new CArgAllow_Integers(-3, 34));

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    cout << "a=" << (*args)["a"].AsString() << endl;
    cout << "i=" << (*args)["i"].AsInteger() << endl;

    return args;
}


// Argument with default walue
static CArgs* s_Test8(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddOptionalKey("k",
                            "alphaNumericKey",
                            "This is an optional argument",
                            CArgDescriptions::eAlnum,
                            0,
                            "CORELIB");

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    if(args->IsProvided("k"))
        cout << "argument value was provided" << endl;
    else
        cout << "default argument value used" << endl;

    cout << "k=" << (*args)["k"].AsString()  << endl;

    return args;
}


// Position arguments - advanced
static CArgs* s_Test7(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddPlain("p1",
                      "This is a plain argument",  CArgDescriptions::eAlnum);
    arg_desc.AddPlain("p2",
                      "This is a plain argument",  CArgDescriptions::eAlnum);
    arg_desc.AddExtra("This is an extra argument", CArgDescriptions::eInteger);

    arg_desc.SetConstraint(CArgDescriptions::eMoreOrEqual, 4);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    cout << "p1=" << (*args)["p1"].AsString()  << endl;
    cout << "p2=" << (*args)["p2"].AsString()  << endl;
    cout << "#1=" << (*args)["#1"].AsInteger() << endl;
    cout << "#2=" << (*args)["#2"].AsInteger() << endl;

    return args;
}


// Position arguments
static CArgs* s_Test6(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddPlain("p1",
                      "This is a plain argument", CArgDescriptions::eAlnum);
    arg_desc.AddPlain("p2",
                      "This is a plain argument", CArgDescriptions::eAlnum);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    cout << "p1=" << (*args)["p1"].AsString() << endl;
    cout << "p2=" << (*args)["p2"].AsString() << endl;

    return args;
}


// Files - advanced
static CArgs* s_Test5(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddKey("if",
                    "inputFile", "This is an input file argument",
                    CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc.AddKey("of",
                    "outputFile", "This is an output file argument",
                    CArgDescriptions::eOutputFile, CArgDescriptions::fAppend);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    while ( !(*args)["if"].AsInputFile().eof() ) {
        string tmp;
        (*args)["if"].AsInputFile () >> tmp;
        (*args)["of"].AsOutputFile() << tmp << endl;
    }

    return args;
}


// Files
static CArgs* s_Test4(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddKey("if",
                    "inputFile", "This is an input file argument",
                    CArgDescriptions::eInputFile);

    arg_desc.AddKey("of",
                    "outputFile", "This is an output file argument",
                    CArgDescriptions::eOutputFile);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    while( !(*args)["if"].AsInputFile().eof() ) {
        string tmp;
        (*args)["if"].AsInputFile () >> tmp;
        (*args)["of"].AsOutputFile() << tmp << endl;
    }

    return args;
}


// Optionals
static CArgs* s_Test3(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddOptionalKey("k1",
                            "fistOptionalKey",
                            "This is an optional argument",
                            CArgDescriptions::eAlnum);
    arg_desc.AddOptionalKey("k2",
                            "secondOptionalKey",
                            "This is an optional argument",
                            CArgDescriptions::eAlnum);

    arg_desc.AddFlag("f1", "Flag 1");
    arg_desc.AddFlag("f2", "Flag 2");

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    if (args->Exist("k1"))
        cout << "k1=" << (*args)["k1"].AsString() << endl;
    if (args->Exist("k2"))
        cout << "k2=" << (*args)["k2"].AsString() << endl;

    if (args->Exist("f1"))
        cout << "f1 was provided" << endl;
    if (args->Exist("f2"))
        cout << "f2 was provided" << endl;

    return args;
}


// Data types
static CArgs* s_Test2(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddKey("ka",
                    "alphaNumericKey", "This is a test alpha-num key argument",
                    CArgDescriptions::eAlnum);

    arg_desc.AddKey("kb",
                    "booleanKey", "This is a test boolean key argument",
                    CArgDescriptions::eBoolean);

    arg_desc.AddKey("ki",
                    "integerKey", "This is a test integer key argument",
                    CArgDescriptions::eInteger);

    arg_desc.AddKey("kd",
                    "doubleKey", "This is a test double key argument",
                    CArgDescriptions::eDouble);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    cout << "ka=" << (*args)["ka"].AsString() << endl;
    cout << "kb=" << NStr::BoolToString( (*args)["kb"].AsBoolean() ) << endl;
    cout << "ki=" << (*args)["ki"].AsInteger() << endl;
    cout << "kd=" << (*args)["kd"].AsDouble() << endl;

    return args;
}


// The simplest test
static CArgs* s_Test1(CArgDescriptions& arg_desc, int argc, const char* argv[])
{
    arg_desc.AddKey("k",
                    "key", "This is a key argument", CArgDescriptions::eAlnum);

    CArgs* args = arg_desc.CreateArgs(argc, argv);

    cout << "k=" << (*args)["k"].AsString() << endl;

    return args;
}



/////////////////////////////////////////////////////////////////////////////
//  Tests' array

typedef CArgs* (*FTest)(CArgDescriptions& arg_desc,
                        int argc, const char* argv[]);

static FTest s_Test[] =
{
    s_Test0,  // default
    s_Test1,
    s_Test2,
    s_Test3,
    s_Test4,
    s_Test5,
    s_Test6,
    s_Test7,
    s_Test8,
    s_Test9
};



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Get test # from env.variable $TEST_NO, if the latter is set
    size_t test     = 0;
    size_t max_test = sizeof(s_Test) / sizeof(s_Test[0]) - 1;

    const char* test_str = getenv("TEST_NO");
    if ( test_str ) {
        try {
            test = NStr::StringToULong(test_str);
        } catch (...) {
            test = 0;
        }

        if (test < 0  ||  test > max_test) {
            test = 0;
        }
    }
    
    // Run the selected test
    CArgDescriptions arg_desc;

    // Setup USAGE context
    string prog_description =
        "This is a test program for command-line argument processing.\n"
        "TEST #" + NStr::UIntToString(test) +
        "    (To run another test, set env.variable $TEST_NO to 0.." +
        NStr::UIntToString(max_test) + ")";
    arg_desc.SetUsageContext(argv[0], prog_description);

    // Run the selected test
    try {
        // Run test
        auto_ptr<CArgs> args( s_Test[test](arg_desc, argc, argv) );

        // Printout obtained argument values
        cout << string(72, '=') << endl;
        string str;
        cout << args->Print(str) << endl;
    }
    catch (exception& e) {
        // Print USAGE
        string str;
        cerr << string(72, '~') << endl << arg_desc.PrintUsage(str) << endl;

        // Print the exception error message
        cerr << string(72, '=') << endl << "ERROR:  " << e.what() << endl;
    }

    return 0;
}
