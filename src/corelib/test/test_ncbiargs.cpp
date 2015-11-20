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


//----------------------------------------------------------------------------
// Extended test for all different types of arguments  [default test]
//----------------------------------------------------------------------------

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
        ("barfooetc",
         CArgAllow_Strings()
         .AllowValue("foo")
         .AllowValue("bar")
         .AllowValue("etc"));

    arg_desc.AddDefaultKey
        ("kd8", "DefaultKey8",
         "This is an optional Int8 key argument, with default value",
         CArgDescriptions::eInt8, "123456789012");
    arg_desc.SetConstraint
        ("kd8",
         CArgAllow_Int8s(-2, NCBI_CONST_INT8(123456789022))
         .AllowRange(-10,-9)
         .Allow(-15));

    arg_desc.AddDefaultKey
        ("kd", "DefaultKey",
         "This is an optional integer key argument, with default value",
         CArgDescriptions::eInteger, "123");
    arg_desc.SetConstraint
        ("kd",
         CArgAllow_Integers(0, 200)
         .AllowRange(300, 310)
         .Allow(400));

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
        ("k", CArgAllow_String(CArgAllow_Symbols::eAlnum).Allow("$#"));
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
        ("one_symbol",
         CArgAllow_Symbols(" aB\tCd").Allow(CArgAllow_Symbols::eDigit));

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

    assert(args["kd8"].GetDefault() == "123456789012");
    assert(args["kd"].GetDefault()  == "123");

    if ( !args["logfile"] ) {
        return;
    }

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

    CArgValue::TArgValueFlags flags = 0;

    args["f1"].GetDefault(&flags);
    assert((flags & CArgValue::fArgValue_HasDefault) != 0);
    if ( args["f1"] ) {
        assert(args["f1"].AsBoolean());
        assert((flags & CArgValue::fArgValue_FromDefault) == 0);
    } else {
        assert((flags & CArgValue::fArgValue_FromDefault) != 0);
    }
    args["f2"].GetDefault(&flags);
    assert((flags & CArgValue::fArgValue_HasDefault) != 0);
    if ( args["f2"] ) {
        assert(args["f2"].AsBoolean());
        assert((flags & CArgValue::fArgValue_FromDefault) != 0);
    } else {
        assert((flags & CArgValue::fArgValue_FromDefault) == 0);
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
        lg << "(no unnamed positional arguments passed in the cmd-line)" << endl;
    }
}


//----------------------------------------------------------------------------
// The simplest test
//----------------------------------------------------------------------------

static void s_InitTest1(CArgDescriptions& arg_desc)
{
    arg_desc.AddOpening("first", "First opening arg", CArgDescriptions::eString);
    arg_desc.AddKey("k",
                    "key", "This is a key argument",
                    CArgDescriptions::eString);
}

static void s_RunTest1(const CArgs& args, ostream& os)
{
    os << "first = " << args["first"].AsString() << endl;
    os << "k = "     << args["k"].AsString() << endl;
    
    assert( args["first"].AsString() == "abc-123-def" );
    assert( args["k"].AsString()     == "kvalue" );
}


//----------------------------------------------------------------------------
// Data types
//----------------------------------------------------------------------------

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
    os << "first = " << args["first"].AsInteger() << endl;
    os << "ka = "    << args["ka"].AsString() << endl;
    os << "kb = "    << NStr::BoolToString( args["kb"].AsBoolean() ) << endl;
    os << "ki = "    << args["ki"].AsInteger() << endl;
    os << "kd = "    << args["kd"].AsDouble() << endl;
    
    assert( args["first"].AsInteger() == 123654 );
    assert( args["ka"].AsString()  == "alpha2" );
    assert( args["kb"].AsBoolean() == true );
    assert( args["ki"].AsInteger() == 34 );
    assert( NStr::DoubleToString(args["kd"].AsDouble()) == "1.567" );
}


//----------------------------------------------------------------------------
// Optional
//----------------------------------------------------------------------------

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
    if (args["k1"]) {
        os << "k1=" << args["k1"].AsString() << endl;
    }
    if (args["k2"]) {
        os << "k2=" << args["k2"].AsString() << endl;
    }
    if (args["f1"]) {
        os << "f1=" << args["f1"].AsString() << endl;
    }
    if (args["f2"]) {
        os << "f2=" << args["f2"].AsString() << endl;
    }
    assert(  args["k1"]  &&  args["k1"].AsString() == "v1" );
    assert( !args["k2"] );
    assert( !args["f1"] );
    assert(  args["f2"]  &&  args["f2"].AsBoolean() == true );
}


//----------------------------------------------------------------------------
// Files
//----------------------------------------------------------------------------

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


//----------------------------------------------------------------------------
// Files - advanced
//----------------------------------------------------------------------------

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


//----------------------------------------------------------------------------
// Position arguments
//----------------------------------------------------------------------------

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
    
    assert( args["p1"].AsString()  == "1.23" );
    assert( args["p2"].AsString()  == "No" );
    assert( args["p2"].AsBoolean() == false );
}


//----------------------------------------------------------------------------
// Position arguments - advanced
//----------------------------------------------------------------------------

static void s_InitTest7(CArgDescriptions& arg_desc)
{
    arg_desc.SetPositionalMode(CArgDescriptions::ePositionalMode_Loose);

    arg_desc.AddPositional
        ("p2",
         "This is a plain argument",  CArgDescriptions::eString);
    arg_desc.AddExtra
        (1, 3,
         "These are extra arguments", CArgDescriptions::eInteger);
    arg_desc.AddPositional
        ("p1",
         "This is a plain argument",  CArgDescriptions::eBoolean);

    arg_desc.AddFlag("f1", "Flag 1");
    arg_desc.AddFlag("f2", "Flag 2");
    arg_desc.AddFlag("f3", "Flag 3");

    arg_desc.SetDependency("p2", CArgDescriptions::eRequires, "#1");
    arg_desc.SetDependency("f1", CArgDescriptions::eExcludes, "#1");
    arg_desc.SetDependency("f2", CArgDescriptions::eRequires, "#2");
    arg_desc.SetDependency("f3", CArgDescriptions::eExcludes, "#2");
}

static void s_RunTest7(const CArgs& args, ostream& os)
{
    os << "p1=" << args["p1"].AsString()  << endl;
    os << "p2=" << args["p2"].AsString()  << endl;

    if ( args.Exist("#1") ) {
        os << "#1=" << args["#1"].AsInteger() << endl;
    }
    if ( args.Exist("#2") ) {
        os << "#2=" << args["#2"].AsInteger() << endl;
    }
    if ( args.Exist("#3") ) {
        os << "#3=" << args["#3"].AsInteger() << endl;
    }
}


//----------------------------------------------------------------------------
// Argument with default walue
//----------------------------------------------------------------------------

static void s_InitTest8(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey
        ("k", "alphaNumericKey",
         "This is an optional argument with default value",
         CArgDescriptions::eString, "CORELIB",
         CArgDescriptions::fOptionalSeparator | CArgDescriptions::fConfidential,
         "", "xncbi core library");

    arg_desc.AddKey
        ("datasize", "MandatoryKey",
         "This is a mandatory DataSize key argument",
         CArgDescriptions::eDataSize);

    arg_desc.AddKey
        ("datetime", "MandatoryKey",
         "This is a mandatory DateTime key argument",
         CArgDescriptions::eDateTime);
}

static void s_RunTest8(const CArgs& args, ostream& os)
{
    os << "k=" << args["k"].AsString()  << endl;

    os << "datasize(str)=" << args["datasize"].AsString() << endl;
    os << "datasize(val)=" << args["datasize"].AsInt8()   << endl;
    os << "datetime(str)=" << args["datetime"].AsString() << endl;
    os << "datetime(val)=" << (args["datetime"].AsDateTime()).AsString()  << endl;

    assert( args["datasize"].AsInt8() == 1000 );
    assert( (args["datetime"].AsDateTime()).AsString() == "02/01/2015 00:00:00" );
    assert( args["k"].AsString() == "CORELIB" );
}


//----------------------------------------------------------------------------
// Constraints with allowing
//----------------------------------------------------------------------------

static void s_InitTest9(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("k",
                    "stringsKey",
                    "This is a test of set-of-strings argument",
                    CArgDescriptions::eString);

    arg_desc.AddKey("ki",
                    "strings_nocase_Key",
                    "This is a test of set-of-strings arg (case-insensitive)",
                    CArgDescriptions::eString);

    arg_desc.AddKey("i",
                    "integerKey",
                    "This is a test of integer argument",
                    CArgDescriptions::eInteger);

    arg_desc.SetConstraint("k", &(*new CArgAllow_Strings, "foo", "bar", "qq"));
    arg_desc.SetConstraint("ki",&(*new CArgAllow_Strings(NStr::eNocase),
                                  "Foo", "bAr", "qQ"));
    arg_desc.SetConstraint("i", new CArgAllow_Integers(-3, 34));
}

static void s_RunTest9(const CArgs& args, ostream& os)
{
    os << "k"   << args["k"].AsString()  << endl;
    os << "ki=" << args["ki"].AsString()  << endl;
    os << "i="  << args["i"].AsInteger() << endl;
}


//----------------------------------------------------------------------------
// Date/time
//         
//   "M/D/Y h:m:s",  // CTime default
//   "Y-M-DTh:m:g",  // ISO8601
//   "Y/M/D h:m:g",  // 
//   "Y-M-D h:m:g",  // NCBI SQL server default
//
//----------------------------------------------------------------------------

static void s_InitTest10(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("dt1", "DateTime",
                    "This is a mandatory DateTime key argument",
                    CArgDescriptions::eDateTime);
    arg_desc.AddKey("dt2", "DateTime",
                    "This is a mandatory DateTime key argument",
                    CArgDescriptions::eDateTime);
    arg_desc.AddKey("dt3", "DateTime",
                    "This is a mandatory DateTime key argument",
                    CArgDescriptions::eDateTime);
    arg_desc.AddKey("dt4", "DateTime",
                    "This is a mandatory DateTime key argument",
                    CArgDescriptions::eDateTime);
}

static void s_RunTest10(const CArgs& args, ostream& os)
{
    string dt1 = (args["dt1"].AsDateTime()).AsString();
    string dt2 = (args["dt2"].AsDateTime()).AsString();
    string dt3 = (args["dt3"].AsDateTime()).AsString();
    string dt4 = (args["dt4"].AsDateTime()).AsString();

    os << "datetime1 = " << dt1 << endl;
    os << "datetime2 = " << dt2 << endl;
    os << "datetime3 = " << dt3 << endl;
    os << "datetime4 = " << dt4 << endl;
    
    assert(dt1 == dt2  &&  dt2 == dt3  &&  dt3==dt4);
}


//////////////////////////////////////////////////////////////////////////////
//  Tests' array

struct STest {
    void (*init)(CArgDescriptions& arg_desc);
    void (*run)(const CArgs& args, ostream& os);
};


// Tests 1-9 can be executed with specifying env variable TEST_NO=X
//
static STest s_Test[] = 
{
    { s_InitTest0,  s_RunTest0 },  // default
    { s_InitTest1,  s_RunTest1 },
    { s_InitTest2,  s_RunTest2 },
    { s_InitTest3,  s_RunTest3 },
    { s_InitTest4,  s_RunTest4 },
    { s_InitTest5,  s_RunTest5 },
    { s_InitTest6,  s_RunTest6 },
    { s_InitTest7,  s_RunTest7 },
    { s_InitTest8,  s_RunTest8 },
    { s_InitTest9,  s_RunTest9 },
    { s_InitTest10, s_RunTest10}
};



//////////////////////////////////////////////////////////////////////////////
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


CArgTestApplication::CArgTestApplication()
{
    SetVersion(CVersionInfo(1,2,3,"NcbiArgTest"));
} 


// Choose the test to run, and setup arg.descriptions accordingly

void CArgTestApplication::Init(void)
{
    // Set err.-posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Get test # from env.variable $TEST_NO
    m_TestNo = 0;
    size_t max_test = sizeof(s_Test) / sizeof(s_Test[0]) - 1;
    const string& test_str = GetEnvironment().Get("TEST_NO");
    if ( !test_str.empty() ) {
        m_TestNo = NStr::StringToNumeric<size_t>(test_str);
        assert(m_TestNo <= max_test);
    }

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
    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, true);

    cmd_desc->SetDetailedDescription(prog_description +
        "\nIt also illustrates creating command groups");

    // Describe cmd-line arguments according to the chosen test #
    s_Test[m_TestNo].init(*cmd_desc);

    // Add commands and groups
    // - create 2 groups (odd/even).
    // - create command for each test except default (0);
    for (size_t i=1; i<=max_test; ++i) {
        string test = NStr::NumericToString(i);
        auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
        string ctx = "Testing CCommandArgDescriptions: RunTest" + test;
        string detailed = ctx + "\nthis will run specified test";
        arg_desc->SetUsageContext("", ctx, true);
        arg_desc->SetDetailedDescription(detailed);
        s_Test[i].init(*arg_desc);
        cmd_desc->SetCurrentCommandGroup(i%2 ? "Second command group" : "First command group");
        cmd_desc->AddCommand("runtest" + test, arg_desc.release(), "rt" + test);
    }

    // Setup arg.descriptions for this application
    SetupArgDescriptions(cmd_desc.release());
}


int CArgTestApplication::Run(void)
{
    cout << string(72, '=') << endl;

    size_t max_test = sizeof(s_Test) / sizeof(s_Test[0]) - 1;
    string cmd_str(GetArgs().GetCommand());
    
    if (!cmd_str.empty()) {
        cout << "command: " << cmd_str << endl;
        NStr::TrimPrefixInPlace(cmd_str, "runtest");
        size_t cmd_num = NStr::StringToNumeric<size_t>(cmd_str);
        assert(cmd_num <= max_test);
        m_TestNo = cmd_num;
    }
    
    // Run a test
    s_Test[m_TestNo].run(GetArgs(), cout);

    // Printout arguments obtained from cmd.-line
    string str;
    cout << endl << "Raw arguments:" << endl;
    cout << GetArgs().Print(str) << endl;

    return 0;
}


void CArgTestApplication::Exit(void)
{
    SetDiagStream(0);
}



//////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CArgTestApplication().AppMain(argc, argv);
}
