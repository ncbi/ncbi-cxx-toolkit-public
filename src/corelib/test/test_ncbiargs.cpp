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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


// Extended test for all different types of arguments  [default test]
static void s_InitTest0(CArgDescriptions& arg_desc)
{
    // Describe the expected command-line arguments
    arg_desc.AddOptionalPositional
        ("logfile",
         "This is an optional named positional argument without default value",
         CArgDescriptions::eOutputFile,
         CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);

    arg_desc.AddFlag
        ("f2",
         "This is another flag argument:  FALSE if set, TRUE if not set",
         false);

    arg_desc.AddPositional
        ("barfooetc",
         "This is a mandatory plain (named positional) argument",
         CArgDescriptions::eString);
    arg_desc.SetConstraint
        ("barfooetc", &(*new CArgAllow_Strings, "foo", "bar", "etc"));

    arg_desc.AddDefaultKey
        ("kd", "DefaultKey",
         "This is an optional integer key argument, with default value",
         CArgDescriptions::eInteger, "123");
    arg_desc.SetConstraint
        ("kd", new CArgAllow_Integers(0, 200));


    arg_desc.AddExtra
        (0,  // no mandatory extra args
         3,  // up to 3 optional extra args
         "These are the optional extra (unnamed positional) arguments. "
         "They will be printed out to the file specified by the "
         "2nd positional argument,\n\"logfile\"",
         CArgDescriptions::eBoolean);

    arg_desc.AddKey
        ("k", "MandatoryKey",
         "This is a mandatory alpha-num key argument",
         CArgDescriptions::eString);
    arg_desc.SetConstraint
        ("k", new CArgAllow_String(CArgAllow_Symbols::eAlnum));

    arg_desc.AddOptionalKey
        ("ko", "OptionalKey",
         "This is another optional key argument, without default value",
         CArgDescriptions::eBoolean);

    arg_desc.AddFlag
        ("f1",
         "This is a flag argument:  TRUE if set, FALSE if not set");

    arg_desc.AddDefaultPositional
        ("one_symbol",
         "This is an optional named positional argument with default value",
         CArgDescriptions::eString, "a");
    arg_desc.SetConstraint
        ("one_symbol", new CArgAllow_Symbols(" aB\tCd"));
}

static void s_RunTest0(const CArgs& args, ostream& os)
{
    assert(!args.Exist(kEmptyStr));  // never exists;  use #1, #2, ... instead
    assert(args.Exist("f1"));
    assert(args.Exist("logfile"));
    assert(args["barfooetc"]);

    if ( !args["logfile"] )
        return;

    // Printout argument values
    os << "Printing arguments to file `"
       << args["logfile"].AsString() << "'..." << endl;

    ostream& lg = args["logfile"].AsOutputFile();

    lg << "k:         " << args["k"].AsString() << endl;
    lg << "barfooetc: " << args["barfooetc"].AsString() << endl;
    lg << "logfile:   " << args["logfile"].AsString() << endl;

    if ( args["ko"] ) {
        lg << "ko:        " << NStr::BoolToString(args["ko"].AsBoolean())
           << endl;
    } else {
        lg << "ko:        not provided" << endl;
        bool is_thrown = false;
        try {
            (void) args["ko"].AsString();
        } catch (CArgException& e) {
            is_thrown = true;
            NCBI_REPORT_EXCEPTION("CArgException is thrown:",e);
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
}



// Allowing
static void s_InitTest9(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("a",
                    "alphaNumericKey",
                    "This is a test alpha-num argument",
                    CArgDescriptions::eString);

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
    arg_desc.AddDefaultKey
        ("k", "alphaNumericKey",
         "This is an optional argument with default value",
         CArgDescriptions::eString, "CORELIB");
}

static void s_RunTest8(const CArgs& args, ostream& os)
{
    os << "k=" << args["k"].AsString()  << endl;
}



// Position arguments - advanced
static void s_InitTest7(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional
        ("p2",
         "This is a plain argument",  CArgDescriptions::eString);
    arg_desc.AddExtra
        (1, 3,
         "These are extra arguments", CArgDescriptions::eInteger);
    arg_desc.AddPositional
        ("p1",
         "This is a plain argument",  CArgDescriptions::eBoolean);
}

static void s_RunTest7(const CArgs& args, ostream& os)
{
    os << "p1=" << args["p1"].AsString()  << endl;
    os << "p2=" << args["p2"].AsString()  << endl;

    os << "#1=" << args["#1"].AsInteger() << endl;
    if ( args["#2"] ) {
        os << "#2=" << args["#2"].AsInteger() << endl;
    }
    if ( args["#3"] ) {
        os << "#3=" << args["#3"].AsInteger() << endl;
    }
}


// Position arguments
static void s_InitTest6(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultPositional
        ("p1", "This is a positional argument with default value",
         CArgDescriptions::eDouble, "1.23");
    arg_desc.AddPositional
        ("p2", "This is a mandatory positional argument",
         CArgDescriptions::eBoolean);
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
                            CArgDescriptions::eString);
    arg_desc.AddOptionalKey("k2",
                            "secondOptionalKey",
                            "This is an optional argument",
                            CArgDescriptions::eString);

    arg_desc.AddFlag("f1", "Flag 1");
    arg_desc.AddFlag("f2", "Flag 2");
}

static void s_RunTest3(const CArgs& args, ostream& os)
{
    if (args["k1"])
        os << "k1=" << args["k1"].AsString() << endl;
    if (args["k2"])
        os << "k2=" << args["k2"].AsString() << endl;
}



// Data types
static void s_InitTest2(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("ka",
                    "alphaNumericKey", "This is a test alpha-num key argument",
                    CArgDescriptions::eString);

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
                    "key", "This is a key argument",
                    CArgDescriptions::eString);
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
public:
    CArgTestApplication();
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    size_t m_TestNo;
};


// Constructor -- setting version test
CArgTestApplication::CArgTestApplication()
{
    SetVersion(CVersionInfo(1,2,3,"NcbiArgTest"));
} 


// Choose the test to run, and
// Setup arg.descriptions accordingly
void CArgTestApplication::Init(void)
{
    // Set err.-posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
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

        if (m_TestNo > max_test) {
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


/*
 * ===========================================================================
 * $Log$
 * Revision 6.22  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.21  2002/12/26 17:13:28  ivanov
 * Added version info test
 *
 * Revision 6.20  2002/07/15 18:17:25  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.19  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.18  2002/04/16 18:49:07  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.17  2001/03/26 16:52:19  vakatov
 * Fixed a minor warning
 *
 * Revision 6.16  2000/12/24 00:13:00  vakatov
 * Radically revamped NCBIARGS.
 * Introduced optional key and posit. args without default value.
 * Added new arg.value constraint classes.
 * Passed flags to be detected by HasValue() rather than AsBoolean.
 * Simplified constraints on the number of mandatory and optional extra args.
 * Improved USAGE info and diagnostic messages. Etc...
 *
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
