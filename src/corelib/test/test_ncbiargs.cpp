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
 * Revision 6.15  2000/11/29 00:09:19  vakatov
 * Added test #10 -- to auto-sort flag and key args alphabetically
 *
 * Revision 6.14  2000/11/24 23:37:46  vakatov
 * The test is now "CNcbiApplication" based (rather than "bare main()")
 * -- to use and to test standard processing of cmd.-line arguments implemented
 * in "CNcbiApplication".
 * Also, test for CArgValue::CloseFile().
 *
 * Revision 6.13  2000/11/22 22:04:32  vakatov
 * Added special flag "-h" and special exception CArgHelpException to
 * force USAGE printout in a standard manner
 *
 * Revision 6.12  2000/11/22 19:40:51  vakatov
 * s_Test3() -- fixed:  Exist() --> IsProvided()
 *
 * Revision 6.11  2000/11/20 19:49:40  vakatov
 * Test0::  printout all arg values
 *
 * Revision 6.10  2000/11/17 22:04:31  vakatov
 * CArgDescriptions::  Switch the order of optional args in methods
 * AddOptionalKey() and AddPlain(). Also, enforce the default value to
 * match arg. description (and constraints, if any) at all times.
 *
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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

USING_NCBI_SCOPE;


// Extended test for all different types of arguments  [default test]
static void s_InitTest0(CArgDescriptions& arg_desc)
{
    // Describe the expected command-line arguments
    arg_desc.AddKey
        ("k", "MandatoryKey",
         "This is a mandatory alpha-num key argument",
         CArgDescriptions::eAlnum);

    arg_desc.AddOptionalKey
        ("ko", "OptionalKey",
         "This is an optional integer key argument",
         CArgDescriptions::eInteger, "123");
    arg_desc.SetConstraint
        ("ko", new CArgAllow_Integers(0, 200));

    arg_desc.AddOptionalKey
        ("ko2", "OptionalKey2",
         "This is another optional key argument",
         CArgDescriptions::eBoolean, "T");

    arg_desc.AddFlag
        ("f1",
         "This is a flag argument:  TRUE if set, FALSE if not set");

    arg_desc.AddFlag
        ("f2",
         "This is another flag argument:  FALSE if set, TRUE if not set",
         false);

    arg_desc.AddPlain
        ("barfooetc",
         "This is a mandatory plain (named positional) argument",
         CArgDescriptions::eString);
    arg_desc.SetConstraint
        ("barfooetc", &(*new CArgAllow_Strings, "foo", "bar", "etc"));

    arg_desc.AddPlain
        ("logfile",
         "This is an optional plain (named positional) argument",
         CArgDescriptions::eOutputFile, "-",
         CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);

    arg_desc.AddExtra
        ("These are the optional extra (unnamed positional) arguments. "
         "They will be printed out to the file specified by the "
         "2nd positional argument,\n\"logfile\"",
         CArgDescriptions::eBoolean);


    // Put constraint on the # of positional arguments (named and unnamed)
    arg_desc.SetConstraint(CArgDescriptions::eMoreOrEqual, 1);
}

static void s_RunTest0(const CArgs& args, ostream& os)
{
    if ( !args.IsProvided("logfile") )
        return;

    // Printout argument values
    os << "Printing arguments to file `"
       << args["logfile"].AsString() << "'..." << endl;

    ostream& lg = args["logfile"].AsOutputFile();

    lg << "k:         " << args["k"].AsString() << endl;
    lg << "ko:        " << args["ko"].AsInteger() << endl;
    lg << "ko2:       " << NStr::BoolToString(args["ko2"].AsBoolean()) << endl;
    lg << "barfooetc: " << args["barfooetc"].AsString() << endl;
    lg << "logfile:   " << args["logfile"].AsString() << endl;

    // Extra (unnamed positional) arguments
    if ( args.GetNExtra() ) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            lg << "#" << extra << ":        "
               << NStr::BoolToString(args[extra].AsBoolean())
               << "   (passed as `" << args[extra].AsString() << "')"
               << endl;
        }
    } else {
        lg << "...no unnamed positional arguments in the cmd-line" << endl;
    }

    // Separator
    lg << string(44, '-') << endl;
}



// Allowing
static void s_InitTest9(CArgDescriptions& arg_desc)
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
}

static void s_RunTest9(const CArgs& args, ostream& os)
{
    os << "a=" << args["a"].AsString()  << endl;
    os << "i=" << args["i"].AsInteger() << endl;
}



// Argument with default walue
static void s_InitTest8(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("k",
                            "alphaNumericKey",
                            "This is an optional argument",
                            CArgDescriptions::eAlnum,
                            "CORELIB");
}

static void s_RunTest8(const CArgs& args, ostream& os)
{

    if ( args.IsProvided("k") )
        os << "argument value was provided" << endl;
    else
        os << "default argument value used" << endl;

    os << "k=" << args["k"].AsString()  << endl;
}



// Position arguments - advanced
static void s_InitTest7(CArgDescriptions& arg_desc)
{
    arg_desc.AddPlain("p1",
                      "This is a plain argument",  CArgDescriptions::eAlnum);
    arg_desc.AddPlain("p2",
                      "This is a plain argument",  CArgDescriptions::eAlnum);
    arg_desc.AddExtra("This is an extra argument", CArgDescriptions::eInteger);

    arg_desc.SetConstraint(CArgDescriptions::eMoreOrEqual, 4);
}

static void s_RunTest7(const CArgs& args, ostream& os)
{
    os << "p1=" << args["p1"].AsString()  << endl;
    os << "p2=" << args["p2"].AsString()  << endl;
    os << "#1=" << args["#1"].AsInteger() << endl;
    os << "#2=" << args["#2"].AsInteger() << endl;
}


// Position arguments
static void s_InitTest6(CArgDescriptions& arg_desc)
{
    arg_desc.AddPlain("p1",
                      "This is a plain argument", CArgDescriptions::eAlnum);
    arg_desc.AddPlain("p2",
                      "This is a plain argument", CArgDescriptions::eAlnum);
}

static void s_RunTest6(const CArgs& args, ostream& os)
{
    os << "p1=" << args["p1"].AsString() << endl;
    os << "p2=" << args["p2"].AsString() << endl;
}



// Files - advanced
static void s_InitTest5(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("if",
                    "inputFile", "This is an input file argument",
                    CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc.AddKey("of",
                    "outputFile", "This is an output file argument",
                    CArgDescriptions::eOutputFile, CArgDescriptions::fAppend);
}

static void s_RunTest5(const CArgs& args, ostream& /*os*/)
{
    // Close
    args["if"].CloseFile();
    (void) args["of"].AsOutputFile();
    args["of"].CloseFile();

    // Auto-reopen (first time only)
    while ( !args["if"].AsInputFile().eof() ) {
        string tmp;
        args["if"].AsInputFile () >> tmp;
        args["of"].AsOutputFile() << tmp << endl;
    }
}


// Files
static void s_InitTest4(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("if",
                    "inputFile", "This is an input file argument",
                    CArgDescriptions::eInputFile);

    arg_desc.AddKey("of",
                    "outputFile", "This is an output file argument",
                    CArgDescriptions::eOutputFile);
}

static void s_RunTest4(const CArgs& args, ostream& /*os*/)
{
    while ( !args["if"].AsInputFile().eof() ) {
        string tmp;
        args["if"].AsInputFile () >> tmp;
        args["of"].AsOutputFile() << tmp << endl;
    }
}



// Optional
static void s_InitTest3(CArgDescriptions& arg_desc)
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
}

static void s_RunTest3(const CArgs& args, ostream& os)
{
    if (args.IsProvided("k1"))
        os << "k1=" << args["k1"].AsString() << endl;
    if (args.IsProvided("k2"))
        os << "k2=" << args["k2"].AsString() << endl;

    if (args.IsProvided("f1"))
        os << "f1 was provided" << endl;
    if (args.IsProvided("f2"))
        os << "f2 was provided" << endl;
}



// Data types
static void s_InitTest2(CArgDescriptions& arg_desc)
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
}

static void s_RunTest2(const CArgs& args, ostream& os)
{
    os << "ka=" << args["ka"].AsString() << endl;
    os << "kb=" << NStr::BoolToString( args["kb"].AsBoolean() ) << endl;
    os << "ki=" << args["ki"].AsInteger() << endl;
    os << "kd=" << args["kd"].AsDouble() << endl;
}


// The simplest test
static void s_InitTest1(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("k",
                    "key", "This is a key argument", CArgDescriptions::eAlnum);
}

static void s_RunTest1(const CArgs& args, ostream& os)
{
    os << "k=" << args["k"].AsString() << endl;
}



/////////////////////////////////////////////////////////////////////////////
//  Tests' array

struct STest {
    void (*init)(CArgDescriptions& arg_desc);
    void (*run)(const CArgs& args, ostream& os);
};


static STest s_Test[] =
{
    {s_InitTest0, s_RunTest0},  // default
    {s_InitTest1, s_RunTest1},
    {s_InitTest2, s_RunTest2},
    {s_InitTest3, s_RunTest3},
    {s_InitTest4, s_RunTest4},
    {s_InitTest5, s_RunTest5},
    {s_InitTest6, s_RunTest6},
    {s_InitTest7, s_RunTest7},
    {s_InitTest8, s_RunTest8},
    {s_InitTest9, s_RunTest9},
    {s_InitTest0, s_RunTest0},
    {0,           0}
};



/////////////////////////////////////////////////////////////////////////////
//  CArgTestApplication::


class CArgTestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    size_t m_TestNo;
};


// Choose the test to run, and
// Setup arg.descriptions accordingly
void CArgTestApplication::Init(void)
{
    // Set err.-posting and tracing on maximum
    // SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Get test # from env.variable $TEST_NO
    m_TestNo = 0;
    size_t max_test = sizeof(s_Test) / sizeof(s_Test[0]) - 2;
    const string& test_str = GetEnvironment().Get("TEST_NO");
    if ( !test_str.empty() ) {
        try {
            m_TestNo = NStr::StringToULong(test_str);
        } catch (...) {
            m_TestNo = 0;
        }

        if (m_TestNo < 0  ||  m_TestNo > max_test) {
            m_TestNo = 0;
        }
    }

    // The "no-test" case
    if ( !s_Test[m_TestNo].init )
        return;

    // Create cmd-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    string prog_description =
        "This is a test program for command-line argument processing.\n"
        "TEST #" + NStr::UIntToString(m_TestNo) +
        "    (To run another test, set env.variable $TEST_NO to 0.." +
        NStr::UIntToString(max_test) + ")";
    bool usage_sort_args = (m_TestNo == 10);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, usage_sort_args);

    // Describe cmd-line arguments according to the chosen test #
    s_Test[m_TestNo].init(*arg_desc);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


// Printout arguments obtained from cmd.-line
int CArgTestApplication::Run(void)
{
    cout << string(72, '=') << endl;

    // The "no-test" case
    if ( !s_Test[m_TestNo].run ) {
        cout << "No arguments described." << endl;
        return 0;
    }

    // Do run
    s_Test[m_TestNo].run(GetArgs(), cout);

    // Printout obtained argument values
    string str;
    cout << GetArgs().Print(str) << endl;

    return 0;
}


// Cleanup
void CArgTestApplication::Exit(void)
{
    SetDiagStream(0);
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CArgTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
