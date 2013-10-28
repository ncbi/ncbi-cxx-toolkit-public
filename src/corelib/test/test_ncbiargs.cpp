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

#include <common/test_assert.h>  /* This header must go last */


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
        ("kd8", "DefaultKey8",
         "This is an optional Int8 key argument, with default value",
         CArgDescriptions::eInt8, "123456789012");
    arg_desc.SetConstraint
        ("kd8", new CArgAllow_Int8s(-2, NStr::StringToInt8("123456789022")));


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
    arg_desc.AddAlias("-key", "k");

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

    arg_desc.AddOptionalPositional
        ("notakey",
         "This is an optional plain (named positional) argument",
         CArgDescriptions::eString);
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
                    "stringsKey",
                    "This is a test of set-of-strings argument",
                    CArgDescriptions::eString);

    arg_desc.AddKey("ai",
                    "strings_nocase_Key",
                    "This is a test of set-of-strings arg (case-insensitive)",
                    CArgDescriptions::eString);

    arg_desc.AddKey("i",
                    "integerKey",
                    "This is a test of integer argument",
                    CArgDescriptions::eInteger);

    arg_desc.SetConstraint("a", &(*new CArgAllow_Strings, "foo", "bar", "qq"));
    arg_desc.SetConstraint("ai", &(*new CArgAllow_Strings(NStr::eNocase),
                                   "Foo", "bAr", "qQ"));
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
         CArgDescriptions::eString, "CORELIB",
         CArgDescriptions::fOptionalSeparator);
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
                    CArgDescriptions::eOutputFile,
                        CArgDescriptions::fPreOpen |
                        CArgDescriptions::fAppend);

    arg_desc.AddKey("iof",
                    "ioFile", "This is an I/O file argument",
                    CArgDescriptions::eIOFile,
                        CArgDescriptions::fPreOpen |
                        CArgDescriptions::fTruncate |
                        CArgDescriptions::fCreatePath |
                        CArgDescriptions::fNoCreate);
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
    CNcbiIostream& io = args["iof"].AsIOFile();
    string t("abc");
    io << t;
    io.seekg(0);
    io >> t;
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
    arg_desc.AddOpening("first", "First opening arg", CArgDescriptions::eInteger);
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
    os << "first=" << args["first"].AsInteger() << endl;
    os << "ka=" << args["ka"].AsString() << endl;
    os << "kb=" << NStr::BoolToString( args["kb"].AsBoolean() ) << endl;
    os << "ki=" << args["ki"].AsInteger() << endl;
    os << "kd=" << args["kd"].AsDouble() << endl;
}


// The simplest test
static void s_InitTest1(CArgDescriptions& arg_desc)
{
    arg_desc.AddOpening("first", "First opening arg", CArgDescriptions::eString);
    arg_desc.AddKey("k",
                    "key", "This is a key argument",
                    CArgDescriptions::eString);
}

static void s_RunTest1(const CArgs& args, ostream& os)
{
    os << "first=" << args["first"].AsString() << endl;
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

#if 0
    // Create cmd-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    string prog_description =
        "This is a test program for command-line argument processing.\n"
        "TEST #" + NStr::SizetToString(m_TestNo) +
        "    (To run another test, set env.variable $TEST_NO to 0.." +
        NStr::SizetToString(max_test) + ")";
    bool usage_sort_args = (m_TestNo == 10);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, usage_sort_args);

    // Describe cmd-line arguments according to the chosen test #
    s_Test[m_TestNo].init(*arg_desc);
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
#else
    // Create cmd-line argument descriptions class
    auto_ptr<CCommandArgDescriptions> cmd_desc(
        new CCommandArgDescriptions(true,0, CCommandArgDescriptions::eCommandOptional |
                CCommandArgDescriptions::eNoSortCommands));

    // Specify USAGE context
    string prog_description =
        "This is a test program for command-line argument processing.\n"
        "TEST #" + NStr::SizetToString(m_TestNo) +
        "    (To run another test, set env.variable $TEST_NO to 0.." +
        NStr::SizetToString(max_test) + ")";
    bool usage_sort_args = (m_TestNo == 10);
    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, usage_sort_args);

    string detailed_description = prog_description +
        "\nIt also illustrates creating command groups";
    cmd_desc->SetDetailedDescription(detailed_description);

    // Describe cmd-line arguments according to the chosen test #
    s_Test[m_TestNo].init(*cmd_desc);

    // add few commands
#if 1
    for (int a=3; a>=0; --a) {

        auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
        string ctx = "Testing CCommandArgDescriptions: RunTest" + NStr::IntToString(a);
        string detailed = ctx + "\nthis will run specified test";
        arg_desc->SetUsageContext("", ctx, true);
        arg_desc->SetDetailedDescription(detailed);
        s_Test[a].init(*arg_desc);
        cmd_desc->SetCurrentCommandGroup(a%2 ? "Second command group" : "Command group #1");
        cmd_desc->AddCommand("runtest" + NStr::IntToString(a), arg_desc.release(),
                             "rt" + NStr::IntToString(a));

    }
    {
        auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext("","RunTest4 description");
        s_Test[4].init(*arg_desc);
        cmd_desc->AddCommand("d4", arg_desc.release());
    }
#endif

    // Setup arg.descriptions for this application
    SetupArgDescriptions(cmd_desc.release());
#endif
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

    string command( GetArgs().GetCommand());
    if (!command.empty()) {
        cout << "command: " << command << endl;
    }
    // Do run
    if (command == "runtest0") {
        s_Test[0].run(GetArgs(), cout);
    } else if (command == "runtest1") {
        s_Test[1].run(GetArgs(), cout);
    } else if (command == "runtest2") {
        s_Test[2].run(GetArgs(), cout);
    } else if (command == "runtest3") {
        s_Test[3].run(GetArgs(), cout);
    } else if (command == "runtest4") {
        s_Test[4].run(GetArgs(), cout);
    } else {
        s_Test[m_TestNo].run(GetArgs(), cout);
    }

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
    return CArgTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
