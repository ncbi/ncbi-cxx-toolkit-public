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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   TEST for:  NCBI C++ core API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <algorithm>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
// Start-up (cmd.-line args, config file)

BOOST_AUTO_TEST_CASE(TestStartup)
{
    // Command-line arguments
    SIZE_TYPE n_args = CNcbiApplication::Instance()->GetArguments().Size();
    NcbiCout << NcbiEndl << "Total # of CMD.-LINE ARGS: "
             << n_args << NcbiEndl;
    for (SIZE_TYPE i = 0;  i < n_args;  i++) {
        NcbiCout << " [" << i << "] = \""
                 << CNcbiApplication::Instance()->GetArguments()[i]
                 << "\"" << NcbiEndl;
    }
    NcbiCout << NcbiEndl;

    // Config.file (registry) content
    const CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();
    NcbiCout << "REGISTRY (loaded from config file):" <<  NcbiEndl;
    if ( reg.Empty() ) {
        NcbiCout << "  <empty>"  << NcbiEndl;
    } else {
        reg.Write(NcbiCout);
    }
    NcbiCout << NcbiEndl;
}


/////////////////////////////////////////////////////////////////////////////
// Diagnostics
//

class CNcbiTestDiag {
public:
    int i;
    CNcbiTestDiag(void) { i = 4321; }
};
inline CNcbiOstream& operator <<(CNcbiOstream& os, const CNcbiTestDiag& cntd) {
    return os << "Output of an serializable class content = " << cntd.i;
}


static void s_TestDiagHandler(const SDiagMessage& mess)
{ 
    NcbiCout << "<Installed Handler> " << mess << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(TTestDiag)
{
    CNcbiDiag diag;
    double d = 123.45;

    SetDiagPostFlag(eDPF_All);
    diag << "[Unset Diag Stream]  Diagnostics double = " << d << Endm;
    ERR_POST( "[Unset Diag Stream]  ERR_POST double = " << d );
    _TRACE( "[Unset Diag Stream]  Trace double = " << d );

    NcbiCout << NcbiEndl
             << "FLUSHing memory-stored diagnostics (if any):" << NcbiEndl;
    SIZE_TYPE n_flush = CNcbiApplication::Instance()->FlushDiag(&NcbiCout);
    NcbiCout << "FLUSHed diag: " << n_flush << " bytes" << NcbiEndl <<NcbiEndl;

    {{
        CNcbiDiag diagWarning;
        SetDiagHandler(s_TestDiagHandler, 0, 0);
        diagWarning << "*Warning* -- must be printed by Installed Handler!";
    }}

    SetDiagStream(&NcbiCerr);
    diag << "***ERROR*** THIS MESSAGE MUST NOT BE VISIBLE!!!\n\n";
    SetDiagStream(0);
    diag << Endm;
    diag << "01234";

    SetDiagStream(&NcbiCerr);
    BOOST_CHECK(  IsDiagStream(&NcbiCerr) );
    BOOST_CHECK( !IsDiagStream(&NcbiCout) );
    diag << "56789" << Endm;
    diag <<   "[Set Diag Stream(cerr)]  Diagnostics double = " << d << Endm;
    ERR_POST("[Set Diag Stream(cerr)]  Std.Diag. double = "    << d );
    _TRACE  ( "[Set Diag Stream(cerr)]  Trace double = "       << d );

    CNcbiTestDiag cntd;
    SetDiagPostLevel(eDiag_Error);
    diag << Warning << cntd << Endm;
    SetDiagPostLevel(eDiag_Info);
    diag << Warning << cntd << Endm;

    diag << Error << "This message has severity \"Info\"" << Reset
         << Info  << "This message has severity \"Info\"" << Endm;

    diag << Critical << "This message has severity \"Critical\"" << Endm;

    diag << Trace << "1This message has severity \"Trace\"" << Endm;

    SetDiagPostFlag(eDPF_All);
    SetDiagPostPrefix("Foo-Prefix");
    ERR_POST("This is the most detailed " << "error posting");
    SetDiagPostPrefix(0);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_LongFilename);
    ERR_POST(Warning << "No line #, " << "Short filename, " << "No prefix");
    _TRACE("_TRACE:  must have all possible info in the posted message");
    UnsetDiagPostFlag(eDPF_Severity);
    ERR_POST("...and no severity");
    SetDiagPostFlag(eDPF_Line);
    SetDiagPostFlag(eDPF_Severity);
    SetDiagPostPrefix("Bad!!! No Prefix!!!");
    UnsetDiagPostFlag(eDPF_Prefix);
    ERR_POST("Short filename, " << "No prefix");
    UnsetDiagPostFlag(eDPF_All);
    ERR_POST("This is the least detailed error posting");
    SetDiagPostFlag(eDPF_All);
    UnsetDiagPostFlag(eDPF_LongFilename);

    PushDiagPostPrefix("Prefix1");
    ERR_POST("Message with prefix");
    PushDiagPostPrefix("Prefix2");
    ERR_POST("Message with prefixes");
    PushDiagPostPrefix("Prefix3");
    ERR_POST("Message with prefixes");
    PopDiagPostPrefix();
    ERR_POST("Message with prefixes");
    PopDiagPostPrefix();
    PopDiagPostPrefix();
    PopDiagPostPrefix();
    {{
        CDiagAutoPrefix a("AutoPrefix");
        ERR_POST("Message with auto prefix");
    }}
    ERR_POST("Message without prefixes");
    ERR_POST_EX(8, 0, "Message without prefix");
    SetDiagPostPrefix(0);

    PushDiagPostPrefix("ERROR");
    PopDiagPostPrefix();
    ERR_POST_EX(1, 2, "Message with error code, without prefix");

    PushDiagPostPrefix("ERROR");
    SetDiagPostPrefix(0);
    diag << Error << ErrCode(3, 4) << "Message with error code" << Endm;

    UnsetDiagPostFlag(eDPF_DateTime);
    diag << Warning << "Message without datetime stamp" << Endm;
    SetDiagPostFlag(eDPF_DateTime);

    IgnoreDiagDieLevel(true);
    {{
        CNcbiDiag x_diag;
        x_diag << Fatal << "Fatal message w/o exit()/abort()!" << Endm;
    }}
    IgnoreDiagDieLevel(false);

    CStringException ex(DIAG_COMPILE_INFO, NULL,
                        CStringException::eConvert, "A fake exception", 12345);
    ERR_POST("A test on reporting of an exception" << ex);
}


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TTestDiag_ErrCodeInfo)
{
    CNcbiDiag diag;
    SetDiagStream(&NcbiCout);

    CDiagErrCodeInfo        info;
    SDiagErrCodeDescription desc;

    CNcbiIstrstream is("\
# Comment line\n\
\n\
MODULE coretest\n\
\n\
$$ DATE, 1 : Short message for error code (1,0)\n\
\n\
$^   NameOfError, 1, 3 : Short message for error code (1,1)\n\
This is a long message for error code (1,1).\n\
$^   NameOfWarning, 2, WARNING: Some short text with ':' and ',' characters\n\
This is first line of the error explanation.\n\
This is second line of the error explanation.\n\
\n\
$$ PRINT, 9, 2\n\
Error with code (9,0).\n\
$^   EmptyString, 1, info:Error with subcode 1.\n\
$^   BadFormat, 2\n\
Message for warning error PRINT_BadFormat.\n\
");

    info.Read(is);
    info.SetDescription(ErrCode(2,3), 
                        SDiagErrCodeDescription("Err 2.3", "Error 2.3"
                                                /*, default severity */));
    info.SetDescription(ErrCode(2,4), 
                        SDiagErrCodeDescription("Err 2.4", "Error 2.4",
                                                eDiag_Error));

    BOOST_CHECK(info.GetDescription(ErrCode(1,1), &desc));
    BOOST_CHECK_EQUAL(desc.m_Message, "Short message for error code (1,1)");
    BOOST_CHECK_EQUAL(desc.m_Explanation, "This is a long message for error code (1,1).");
    BOOST_CHECK_EQUAL(desc.m_Severity, 3);

    BOOST_CHECK(info.GetDescription(ErrCode(9,0), &desc));
    BOOST_CHECK_EQUAL(desc.m_Severity, 2);
    BOOST_CHECK(info.GetDescription(ErrCode(9,1), &desc));
    BOOST_CHECK_EQUAL(desc.m_Severity, eDiag_Info);
    BOOST_CHECK(info.GetDescription(ErrCode(9,2), &desc));
    BOOST_CHECK_EQUAL(desc.m_Severity, 2);

    BOOST_CHECK(info.GetDescription(ErrCode(2,4), &desc));
    BOOST_CHECK_EQUAL(desc.m_Message, "Err 2.4");
    BOOST_CHECK_EQUAL(desc.m_Explanation, "Error 2.4");
    BOOST_CHECK_EQUAL(desc.m_Severity, eDiag_Error);

    BOOST_CHECK(!info.GetDescription(ErrCode(1,3), &desc));

    SetDiagErrCodeInfo(&info, false);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostPrefix("Prefix");

    ERR_POST("This message has default severity \"Error\"");
    ERR_POST_EX(1,1,"This message has severity \"Critical\"");

    diag << ErrCode(1,1) << Info << "This message has severity \"Critical\"" 
         << Endm;
    UnsetDiagPostFlag(eDPF_ErrCodeMessage);
    diag << ErrCode(1,1) << Info << "This message has severity \"Critical\"" 
         << Endm;
    UnsetDiagPostFlag(eDPF_Prefix);
    diag << ErrCode(1,1) << Info << "This message has severity \"Critical\""
         << Endm;
    UnsetDiagPostFlag(eDPF_ErrCode);
    UnsetDiagPostFlag(eDPF_ErrCodeUseSeverity);
    diag << ErrCode(1,1) << Info << "This message has severity \"Info\""
         << Endm;
    UnsetDiagPostFlag(eDPF_ErrCodeExplanation);
    SetDiagPostFlag(eDPF_ErrCodeUseSeverity);
    diag << ErrCode(1,1) << "This message has severity \"Critical\"" << Endm;
    SetDiagErrCodeInfo(0);
}


/////////////////////////////////////////////////////////////////////////////
// Exceptions
//

static void TE_runtime(void) {
    throw runtime_error("TE_runtime::runtime_error");
}

static void TE_none(void) THROWS_NONE {
    return;
}

static void TE_logic(void) THROWS((runtime_error,logic_error)) {
    throw logic_error("TE_logic::logic_error");
}


class XXX {
    string mess;
public:
    XXX(const XXX&    x);
    XXX(const string& m);
    ~XXX(void);
    const string& what(void) const { return mess; }
};

XXX::XXX(const string& m) : mess(m) {
    NcbiCerr << "XXX: " << (void*)(this) << NcbiEndl;
}
XXX::XXX(const XXX&x) : mess(x.mess) {
    NcbiCerr << "XXX(" << (void*)(&x) << "): " << (void*)(this) << NcbiEndl;
}
XXX::~XXX(void) {
    NcbiCerr << "~XXX: " << (void*)(this) << NcbiEndl;
}

static void s_ThrowXXX(void) {
    NcbiCerr << NcbiEndl << "Throw class XXX" << NcbiEndl;
    throw XXX("s_ThrowXXX message");
   
}

BOOST_AUTO_TEST_CASE(TestException_Features)
{
    CNcbiOstream* prevDiag = GetDiagStream();
    SetDiagStream(&NcbiCout);
    try {
        s_ThrowXXX();
    } catch (XXX& x) {
        NcbiCerr << "Catch (XXX& x): " << x.what() << NcbiEndl;
    }

    try {
        s_ThrowXXX();
    } catch (XXX x) {
        NcbiCerr << "Catch (XXX x): " << x.what() << NcbiEndl << NcbiEndl;
    }
    SetDiagStream(prevDiag);
}


BOOST_AUTO_TEST_CASE(TestException_Std)
{
    CNcbiOstream* prevDiag = GetDiagStream();
    SetDiagStream(&NcbiCout);
    try { TE_runtime(); }
    catch (runtime_error& e) {
        NcbiCerr << "CATCH TE_runtime::runtime_error : " << e.what()<<NcbiEndl;
    }

    try { TE_runtime(); }
    catch (exception& e) {
        NcbiCerr << "CATCH TE_runtime::exception " << e.what() << NcbiEndl;
    }

    try { TE_runtime(); }
    STD_CATCH ("STD_CATCH TE_runtime ");

    try { TE_runtime(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_runtime::logic_error " << e.what() << NcbiEndl;
        _TROUBLE; }
    STD_CATCH_ALL ("STD_CATCH_ALL TE_runtime");

    TE_none();

    try { TE_logic(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_logic " << e.what() << NcbiEndl; }
    STD_CATCH_ALL ("try { TE_logic(); }  SOMETHING IS WRONG!");
    SetDiagStream(prevDiag);
}


BOOST_AUTO_TEST_CASE(TestException_Aux)
{
    CNcbiOstream* prevDiag = GetDiagStream();
    SetDiagStream(&NcbiCout);
    try {
        BOOST_CHECK( !strtod("1e-999999", 0) );
        NCBI_THROW(CErrnoTemplException<CCoreException>,eErrno,
            "Failed strtod(\"1e-999999\", 0)");
    }
    catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("TEST CErrnoTemplException<CCoreException> ---> ",e);
    }
    try {
        NCBI_THROW2(CParseTemplException<CCoreException>,eErr,
            "Failed parsing (at pos. 123)",123);
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("TEST CParseTemplException<CCoreException> ---> ",e);
    }
    catch (...) {
        // Should never be here
        _TROUBLE;
    }
    SetDiagStream(prevDiag);
}

BOOST_AUTO_TEST_CASE(TestStreamposConvert)
{
    Int8 p1 = 1;
    p1 <<= 45;

    CT_POS_TYPE pos = NcbiInt8ToStreampos(p1);
    Int8        p2  = NcbiStreamposToInt8(pos);

    if ( sizeof(CT_POS_TYPE) < sizeof(Int8)  ||
         sizeof(CT_OFF_TYPE) < sizeof(Int8) ) {
        ERR_POST(Warning <<
                 "Large streams are not supported, skipping the test");
        return;
    }
    BOOST_CHECK(p1 == p2);
    BOOST_CHECK(p1 && p2);
}


#if 0
BOOST_AUTO_TEST_CASE(TestException)
{
    SetDiagStream(&NcbiCout);

    TestException_Features();
    TestException_Std();
    TestException_Aux();
    TestStreamposConvert();
}
#endif

/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestException_AuxTrace)
{
    try {
        BOOST_CHECK( !strtod("1e-999999", 0) );
        NCBI_THROW(CErrnoTemplException<CCoreException>,eErrno,
            "Failed strtod('1e-999999', 0)");
    }
    catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("throw CErrnoTemplException<CCoreException> ---> ",e);
    }

    try {
        try {
            // BW___WS53
            // Sigh.  WorkShop 5.3 is stupid, and complains about
            // const char* vs. const char[15] without this cast.
            THROW0_TRACE((const char*) "Throw a string");
        } catch (const char* /*e*/) {
            RETHROW_TRACE;
        }
    } catch (const char* /*e*/) {
    }

    try {
        THROW_TRACE(bad_alloc, ());
    }
    STD_CATCH ("THROW_TRACE  bad_alloc ---> ");

    try {
        THROW_TRACE(runtime_error, ("Some message..."));
    }
    STD_CATCH ("THROW_TRACE  runtime_error ---> ");

    try {
        NCBI_THROW2(CParseTemplException<CCoreException>,eErr,
            "Failed parsing (at pos. 123)", 123);
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION("throw CParseTemplException<CCoreException> ---> ",e);
    }
}


/////////////////////////////////////////////////////////////////////////////
// I/O stream extensions
//

BOOST_AUTO_TEST_CASE(TestIostream)
{
    CNcbiIstrstream is("abc\r\nx0123456789\ny012345\r \tcba");
    string str;

    while (NcbiGetline(is, str, "\r\n")) {
        NcbiCout << str << NcbiEndl;
        BOOST_CHECK(!str.empty());
    }
    is.clear();
    is.seekg(0, IOS_BASE::beg);

    NcbiGetline(is, str, '\n');
    BOOST_CHECK( is.good() );
    BOOST_CHECK( str.compare("abc\r") == 0 );

    is >> str;
    BOOST_CHECK( is.good() );
    BOOST_CHECK( str.compare("x0123456789") == 0 );

    is >> str;
    BOOST_CHECK( is.good() );
    BOOST_CHECK( str.compare("y012345") == 0 );

    is >> str;
    BOOST_CHECK( is.eof() );
    BOOST_CHECK( str.compare("cba") == 0 );

    is >> str;
    BOOST_CHECK( !is.good() );

    is.clear();
    is >> str;
    BOOST_CHECK( !is.good() );

    str = "0 1 2 3 4 5\n6 7 8 9";
    NcbiCout << "String output: "  << str << NcbiEndl;

    const char kNullDev[] =
#ifdef NCBI_OS_MSWIN
        "NUL"
#else
        "/dev/null"
#endif /*NCBI_OS_MSWIN*/
        ;
    CNcbiOfstream ofs(kNullDev);
    CNcbiIfstream ifs(kNullDev);

    BOOST_CHECK(NcbiStreamCopy(ofs, ifs));

    NcbiCout << "NcbiStreamCopy() test passed" << NcbiEndl;
}



/////////////////////////////////////////////////////////////////////////////
// Registry
//

BOOST_AUTO_TEST_CASE(TestRegistry)
{
    CNcbiRegistry reg;
    CConstRef<IRegistry> env_reg
        = reg.FindByName(CNcbiRegistry::sm_EnvRegName);
    if (env_reg.Empty()) {
        ERR_POST("Environment-based subregistry missing");
    } else {
        reg.Remove(*env_reg);
    }
    BOOST_CHECK( reg.Empty() );

    list<string> sections;
    reg.EnumerateSections(&sections);
    BOOST_CHECK( sections.empty() );

    list<string> entries;
    reg.EnumerateEntries(kEmptyStr, &entries);
    BOOST_CHECK( entries.empty() );

    // Compose a test registry
    BOOST_CHECK(  reg.Set("Section0", "Name01", "Val01_BAD!!!") );
    BOOST_CHECK(  reg.Set("Section1 ", "\nName11", "Val11_t") );
    BOOST_CHECK( !reg.Empty() );
    BOOST_CHECK(  reg.Get(" Section1", "Name11\t") == "Val11_t" );
    BOOST_CHECK(  reg.Get("Section1", "Name11",
                      CNcbiRegistry::ePersistent).empty() );
    BOOST_CHECK(  reg.Set("Section1", "Name11", "Val11_t") );
    BOOST_CHECK( !reg.Set("Section1", "Name11", "Val11_BAD!!!",
                      CNcbiRegistry::eNoOverride) );

    BOOST_CHECK(  reg.Set("   Section2", "\nName21  ", "Val21",
                      CNcbiRegistry::ePersistent |
                      CNcbiRegistry::eNoOverride) );
    BOOST_CHECK(  reg.Set("Section2", "Name21", "Val21_t") );
    BOOST_CHECK( !reg.Empty() );
    BOOST_CHECK(  reg.Set("Section3", "Name31", "Val31_t") );

    BOOST_CHECK( reg.Get(" \nSection1", " Name11  ") == "Val11_t" );
    BOOST_CHECK( reg.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
             "Val21" );
    BOOST_CHECK( reg.Get(" Section2", " Name21\n") == "Val21_t" );
    BOOST_CHECK( reg.Get("SectionX", "Name21").empty() );

    BOOST_CHECK( reg.Set("Section4", "Name41", "Val410 Val411 Val413",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK(!reg.Set("Sect ion4", "Name41", "BAD1",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK(!reg.Set("Section4", "Na me41", "BAD2") );
    BOOST_CHECK( reg.Set("SECTION4", "Name42", "V420 V421\nV422 V423 \"",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("Section4", "NAME43",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("\tSection4", "Name43T",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    BOOST_CHECK( reg.Set("Section4", "Name44", "\n V440 V441 \r\n",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("\r Section4", "  \t\rName45", "\r\n V450 V451  \n  ",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("Section4 \n", "  Name46  ", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set(" Section4", "Name46T", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    BOOST_CHECK( reg.Set("Section4", "Name47", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("Section4", "Name47T", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    string xxx("\" V481\" \n\"V482 ");
    BOOST_CHECK( reg.Set("Section4", "Name48", xxx, CNcbiRegistry::ePersistent) );

    BOOST_CHECK( reg.Set("Section5", "Name51", "Section5/Name51",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("_Section_5", "Name51", "_Section_5/Name51",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("_Section_5_", "_Name52", "_Section_5_/_Name52",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("_Section_5_", "Name52", "_Section_5_/Name52",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("_Section_5_", "_Name53_", "_Section_5_/_Name53_",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("Section-5.6", "Name-5.6", "Section-5.6/Name-5.6",
                     CNcbiRegistry::ePersistent) );
    BOOST_CHECK( reg.Set("-Section_5", ".Name.5-3", "-Section_5/.Name.5-3",
                     CNcbiRegistry::ePersistent) );


    // Dump
    CNcbiOstrstream os;
    BOOST_CHECK ( reg.Write(os) );
    string os_string = CNcbiOstrstreamToString(os);
    const char* os_str = os_string.c_str();
    NcbiCerr << "\nRegistry:\n" << os_str << NcbiEndl;

    // "Persistent" load
    CNcbiIstrstream is1(os_str);
    CNcbiRegistry  reg1(is1);
    BOOST_CHECK(  reg1.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
              "Val21" );
    BOOST_CHECK(  reg1.Get("Section2", "Name21") == "Val21" );
    BOOST_CHECK(  reg1.Set("Section2", "Name21", NcbiEmptyString) );
    BOOST_CHECK( !reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    BOOST_CHECK( !reg1.Empty() );
    BOOST_CHECK(  reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );

    // "Transient" load
    CNcbiIstrstream is2(os_str);
    CNcbiRegistry  reg2(is2, CNcbiRegistry::eTransient);
    BOOST_CHECK(  reg2.Get("Section2", "Name21",
                       CNcbiRegistry::ePersistent).empty() );
    BOOST_CHECK(  reg2.Get("Section2", "Name21") == "Val21" );
    BOOST_CHECK( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );
    BOOST_CHECK( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    BOOST_CHECK( !reg2.Empty() );
    BOOST_CHECK(  reg2.Set("Section2", "Name21", NcbiEmptyString) );


    BOOST_CHECK( reg.Get("Sect ion4 ", "Name41 ").empty() );
    BOOST_CHECK( reg.Get("Section4 ", "Na me41 ").empty() );

    BOOST_CHECK( reg.Get("Section4 ", "Name41 ") == "Val410 Val411 Val413" );
    BOOST_CHECK( reg.Get("Section4",  " Name42") == "V420 V421\nV422 V423 \"" );
    BOOST_CHECK( reg.Get("Section4",  "Name43")  == " \tV430 V431  \n V432 V433 ");
    BOOST_CHECK( reg.Get("Section4",  "Name43T") == "V430 V431  \n V432 V433" );
    BOOST_CHECK( reg.Get("Section4",  " Name44") == "\n V440 V441 \r\n" );
    BOOST_CHECK( reg.Get(" SecTIon4", "Name45")  == "\r\n V450 V451  \n  " );
    BOOST_CHECK( reg.Get("SecTion4 ", "Name46")  == "\n\nV460\" \n \t \n\t" );
    BOOST_CHECK( reg.Get("Section4",  "NaMe46T") == "\n\nV460\" \n \t \n" );
    BOOST_CHECK( reg.Get(" Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    BOOST_CHECK( reg.Get("Section4 ", "NAme47T") == "470\n471\\\n 472\\\n473\\" );
    BOOST_CHECK( reg.Get("Section4",  "Name48")  == xxx );

    BOOST_CHECK( reg2.Get("Section4", "Name41")  == "Val410 Val411 Val413" );
    BOOST_CHECK( reg2.Get("Section4", "Name42")  == "V420 V421\nV422 V423 \"" );
    BOOST_CHECK( reg2.Get("Section4", "Name43")  == " \tV430 V431  \n V432 V433 ");
    BOOST_CHECK( reg2.Get("Section4", "Name43T") == "V430 V431  \n V432 V433" );
    BOOST_CHECK( reg2.Get("Section4", "Name44")  == "\n V440 V441 \r\n" );
    BOOST_CHECK( reg2.Get("Section4", "NaMe45")  == "\r\n V450 V451  \n  " );
    BOOST_CHECK( reg2.Get("SecTIOn4", "NAme46")  == "\n\nV460\" \n \t \n\t" );
    BOOST_CHECK( reg2.Get("Section4", "Name46T") == "\n\nV460\" \n \t \n" );
    BOOST_CHECK( reg2.Get("Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    BOOST_CHECK( reg2.Get("Section4", "Name47T") == "470\n471\\\n 472\\\n473\\" );
    BOOST_CHECK( reg2.Get("Section4", "Name48")  == xxx );

    BOOST_CHECK( reg2.Get(" Section5",    "Name51 ")   == "Section5/Name51" );
    BOOST_CHECK( reg2.Get("_Section_5",   " Name51")   == "_Section_5/Name51" );
    BOOST_CHECK( reg2.Get(" _Section_5_", " _Name52")  == "_Section_5_/_Name52");
    BOOST_CHECK( reg2.Get("_Section_5_ ", "Name52")    == "_Section_5_/Name52");
    BOOST_CHECK( reg2.Get("_Section_5_",  "_Name53_ ") == "_Section_5_/_Name53_" );
    BOOST_CHECK( reg2.Get(" Section-5.6", "Name-5.6 ") == "Section-5.6/Name-5.6");
    BOOST_CHECK( reg2.Get("-Section_5",   ".Name.5-3") == "-Section_5/.Name.5-3");

    // Printout of the whole registry content
    BOOST_CHECK( reg.Set("Section0", "Name01", "") );
    reg.EnumerateSections(&sections);
    BOOST_CHECK( find(sections.begin(), sections.end(), "Section0")
             == sections.end() );
    BOOST_CHECK( find(sections.begin(), sections.end(), "Section1")
             != sections.end() );
    BOOST_CHECK( !sections.empty() );
    NcbiCout << "\nRegistry Content:\n";
    for (list<string>::const_iterator itSection = sections.begin();
         itSection != sections.end();   itSection++) {
        NcbiCout << "Section: " << *itSection << NcbiEndl;
        reg.EnumerateEntries(*itSection, &entries);
        for (list<string>::const_iterator itEntry = entries.begin();
             itEntry != entries.end();   itEntry++) {
            NcbiCout << "  Entry: " << *itEntry << NcbiEndl;
            NcbiCout << "    Default:    "
                     << reg.Get(*itSection, *itEntry) << NcbiEndl;
            NcbiCout << "    Persistent: "
                     << reg.Get(*itSection, *itEntry,
                                CNcbiRegistry::ePersistent) << NcbiEndl;
        }
    }

    reg.Clear();
    BOOST_CHECK( reg.Empty() );
    
    // Test read/write registry

    NcbiCout << endl;
    NcbiCout << "---------------------------------------------------" << endl;

    CNcbiIstrstream is("\n\
############################################\n\
#\n\
#  Registry file comment\n\
#\n\
############################################\n\
\n\
; comment for section1\n\
\n\
[section1]\n\
; This is a comment for n11\n\
#\n\
#  File comment also\n\
#\n\
n11 = value11\n\
\n\
; This is a comment for n12 line 1\n\
; This is a comment for n12 line 2\n\
\n\
n12 = value12\n\
\n\
; This is a comment for n13\n\
n13 = value13\n\
; new comment for n13\n\
n13 = new_value13\n\
\n\
[ section2 ]\n\
; This is a comment for n21\n\
n21 = value21\n\
   ; This is a comment for n22\n\
n22 = value22\n\
[section3]\n\
n31 = value31\n\
");

    reg.Read(is);
    reg.Write(NcbiCout);
    NcbiCout << "---------------------------------------------------" << endl;
    NcbiCout << "File comment:" << endl;
    NcbiCout << reg.GetComment() << endl;
    NcbiCout << "Section comment:" << endl;
    NcbiCout << reg.GetComment("section1") << endl;
    NcbiCout << "Entry comment:" << endl;
    NcbiCout << reg.GetComment("section1","n12") << endl;

    reg.SetComment(" new comment\n# for registry\n\n  # ...\n\n\n");
    reg.SetComment(";new comment for section1\n","section1");
    reg.SetComment("new comment for section3","section3");
    reg.SetComment("new comment for entry n11","section1","n11");
    reg.SetComment("  ; new comment for entry n31","section3","n31");
    reg.SetComment("","section2","n21");

    NcbiCout << "---------------------------------------------------" << endl;
    reg.Write(NcbiCout);
    NcbiCout << "---------------------------------------------------" << endl;
}


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestThrowTrace)
{
    SetDiagTrace(eDT_Enable);
    NcbiCerr << "The following lines should be equal pairs:" << NcbiEndl;
    try {
        THROW1_TRACE(runtime_error, "Message");
    }
    catch (...) {
        CDiagCompileInfo dci(__FILE__, __LINE__ - 3, NCBI_CURRENT_FUNCTION);
        CNcbiDiag(dci, eDiag_Trace) << "runtime_error: Message";
    }
    string mess = "ERROR";
    try {
        THROW1_TRACE(runtime_error, mess);
    }
    catch (...) {
        CDiagCompileInfo dci(__FILE__, __LINE__ - 3, NCBI_CURRENT_FUNCTION);
        CNcbiDiag(dci, eDiag_Trace) << "runtime_error: ERROR";
    }
    int i = 123;
    try {
        THROW0p_TRACE(i);
    }
    catch (...) {
        CDiagCompileInfo dci(__FILE__, __LINE__ - 3, NCBI_CURRENT_FUNCTION);
        CNcbiDiag(dci, eDiag_Trace) << "i: 123";
    }
    SetDiagTrace(eDT_Default);
}

#include <corelib/ncbiobj.hpp>

class CObjectInt : public CObject
{
public:
  CObjectInt() : m_Int(0) {}
  CObjectInt(int i) : m_Int(i) {}
  int m_Int;
};

class IInterface
{
public:
    virtual ~IInterface() {}
    virtual void foo() = 0;
    virtual void bar() const = 0;
};

class CImplementation : public CObject, public IInterface
{
public:
    virtual ~CImplementation() {}
    virtual void foo() {}
    virtual void bar() const {}
};


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestHeapStack)
{
    SetDiagTrace(eDT_Enable);
    {
      NcbiCerr << "Test CObjectInt in stack:" << NcbiEndl;
      CObjectInt stackObj;
      BOOST_CHECK(!stackObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObjectInt on heap:" << NcbiEndl;
      CObjectInt* heapObj = new CObjectInt();
      BOOST_CHECK(heapObj->CanBeDeleted());
      delete heapObj; 
    }
    {
      NcbiCerr << "Test static CObjectInt:" << NcbiEndl;
      static CObjectInt staticObj;
      BOOST_CHECK(!staticObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CRef on CObjectInt on heap:" << NcbiEndl;
      CRef<CObjectInt> objRef(new CObjectInt());
      BOOST_CHECK(objRef->CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObject in stack:" << NcbiEndl;
      CObject stackObj;
      BOOST_CHECK(!stackObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObject on heap:" << NcbiEndl;
      CObject* heapObj = new CObject();
      BOOST_CHECK(heapObj->CanBeDeleted());
      delete heapObj; 
    }
    {
      NcbiCerr << "Test static CObject:" << NcbiEndl;
      static CObject staticObj;
      BOOST_CHECK(!staticObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CRef on CObject on heap:" << NcbiEndl;
      CRef<CObject> objRef(new CObject());
      BOOST_CHECK(objRef->CanBeDeleted());
    }
    SetDiagTrace(eDT_Default);

    CIRef<IInterface> iref(new CImplementation);
    iref->foo();
    iref->bar();
    CConstIRef<IInterface> ciref(iref);
    ciref->bar();
    ciref = iref;
    ciref->bar();
    ciref = null;
    ciref = iref;
    ciref->bar();
    ciref = new CImplementation;
    ciref->bar();
}


#if (defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION < 300)) || defined(NCBI_COMPILER_MIPSPRO)
# define NO_EMPTY_BASE_OPTIMIZATION
#endif

class CEmpty
{
};

class CSubclass : CEmpty
{
protected:
    int i;
};


template<class A, class B>
class CObjectSizeTester
{
public:
    static void Test(bool& error)
        {
            size_t a_size = sizeof(A);
            size_t b_size = sizeof(B);
            if ( a_size != b_size ) {
                NcbiCerr
                    << "Object size test failed:" << NcbiEndl
                    << "  sizeof(" << typeid(A).name() << ") = " << a_size
                    << NcbiEndl
                    << "  sizeof(" << typeid(B).name() << ") = " << b_size
                    << NcbiEndl;
                error = true;
            }
        }
};


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestObjectSizes)
{
    NcbiCout <<
        "Check if compiler supports empty-base optimization" << NcbiEndl;
    // check if it works
    bool error = false;
    CObjectSizeTester<CSubclass, int>::Test(error);
    CObjectSizeTester<CRef<CObject>, CObject*>::Test(error);
    CObjectSizeTester<CConstRef<CObject>, const CObject*>::Test(error);
    CObjectSizeTester<CMutexGuard, CMutexGuard::resource_type*>::Test(error);
    if ( !error ) {
        NcbiCout <<
            "This compiler supports empty-base optimization!" << NcbiEndl;
    }
    else {
        NcbiCerr <<
            "Test for empty-base optimization failed!" << NcbiEndl;
#ifndef NO_EMPTY_BASE_OPTIMIZATION
        BOOST_CHECK(!error);
#endif
    }
}


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestBASE64Encoding)
{
    const char test_string[] = "Quick brown fox jumps over the lazy dog";
    char buf1[1024], buf2[1024], buf3[1024];
    size_t read, written, len = 16, i, j;

    BASE64_Encode(test_string, strlen(test_string) + 1, &read,
                  buf1, sizeof(buf1), &written, &len);
    BOOST_CHECK(read == strlen(test_string) + 1);
    BOOST_CHECK(written < sizeof(buf1));
    BOOST_CHECK(buf1[written] == '\0');

    BOOST_CHECK(BASE64_Decode(buf1, written, &read,
                          buf2, sizeof(buf2), &written));
    BOOST_CHECK(strlen(buf1) == read);
    BOOST_CHECK(written == strlen(test_string) + 1);
    BOOST_CHECK(buf2[written - 1] == '\0');
    BOOST_CHECK(strcmp(buf2, test_string) == 0);

    for (i = 0; i < 100; i++) {
        len = rand() % 250;
        memset(buf1, '\0', sizeof(buf1));
        memset(buf2, '\0', sizeof(buf2));
        memset(buf3, '\0', sizeof(buf3));
        for (j = 0; j < len; j++) {
            buf1[j] = rand() & 0xFF;
        }

        j = rand() % 100;
        BASE64_Encode(buf1, len, &read, buf2, sizeof(buf2), &written, &j);
        if (len != read)
            NcbiCerr << "len = " << len << ", read = " << read << NcbiEndl;
        BOOST_CHECK(len == read);
        BOOST_CHECK(written < sizeof(buf2));
        BOOST_CHECK(buf2[written] == '\0');

        if (rand() & 1) {
            buf2[written] = '=';
        }
        j = written;
        BASE64_Decode(buf2, j, &read, buf3, sizeof(buf3), &written);
        if (j != read)
            NcbiCerr << "j = " << len << ", read = " << read << NcbiEndl;
        BOOST_CHECK(j == read);
        BOOST_CHECK(len == written);
        BOOST_CHECK(memcmp(buf1, buf3, len) == 0);
    }
}


/////////////////////////////////////////////////////////////////////////////

class CCharVector
{
public:
    CCharVector(const char* str)
        : m_begin(str),
          m_end(str+strlen(str))
        {
        }

    typedef char value_type;
    typedef char* pointer;
    typedef char& reference;
    typedef const char* const_pointer;
    typedef const char& const_reference;

    typedef char* iterator;
    typedef const char* const_iterator;

    struct const_reverse_iterator {
        const_reverse_iterator(const char* ptr)
            : ptr(ptr)
            {
            }

        bool operator==(const_reverse_iterator rhs) const
            { return ptr == rhs.ptr; }
        bool operator!=(const_reverse_iterator rhs) const
            { return ptr != rhs.ptr; }
        const_reverse_iterator& operator++()
            { --ptr; return *this; }
        const_reverse_iterator& operator--()
            { ++ptr; return *this; }

        char operator*() const { return ptr[-1]; }

    private:
        const char* ptr;
    };
    

    const_iterator begin() const
        {
            return m_begin;
        }
    const_iterator end() const
        {
            return m_end;
        }

    const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator(m_end);
        }
    const_reverse_iterator rend() const
        {
            return const_reverse_iterator(m_begin);
        }

    iterator begin()
        {
            throw runtime_error("non-const begin called");
        }
    iterator end()
        {
            throw runtime_error("non-const end called");
        }

    iterator rbegin()
        {
            throw runtime_error("non-const rbegin called");
        }
    iterator rend()
        {
            throw runtime_error("non-const rend called");
        }

private:
    const char* m_begin;
    const char* m_end;
};

template<class C>
void TestIterateFor(const C&)
{
    typedef C Cont;
    for ( int t = 0; t < 100; ++t ) {
        Cont c;
        for ( int i = 0; i < 100; ++i ) {
            c.insert(c.end(), rand());
        }
        vector<int> pc1, pc2;
        ITERATE ( typename Cont, it, c ) {
            pc1.push_back(*it);
        }
        NON_CONST_ITERATE ( typename Cont, it, c ) {
            *it += 1;
        }
        ITERATE ( typename Cont, it, c ) {
            pc2.push_back(*it);
        }
        NON_CONST_ITERATE ( vector<int>, it, pc1 ) {
            *it += 1;
        }
        vector<int> pc3, pc4;
        REVERSE_ITERATE ( typename Cont, it, c ) {
            pc3.push_back(*it);
        }
        NON_CONST_REVERSE_ITERATE ( typename Cont, it, c ) {
            *it -= 1;
        }
        REVERSE_ITERATE ( typename Cont, it, c ) {
            pc4.push_back(*it);
        }
        NON_CONST_ITERATE ( vector<int>, it, pc4 ) {
            *it += 1;
        }
        reverse(pc3.begin(), pc3.end());
        reverse(pc4.begin(), pc4.end());
        
        BOOST_CHECK(pc1 == pc2);
        BOOST_CHECK(pc3 == pc4);
        BOOST_CHECK(pc1 == pc3);
    }
}


template<class C>
void TestCIterate(C& c)
{
    typedef C Cont;
    vector<int> pc1, pc2;
    ITERATE ( typename Cont, it, c ) {
        pc1.push_back(*it);
    }
    REVERSE_ITERATE ( typename Cont, it, c ) {
        pc2.push_back(*it);
    }
    reverse(pc2.begin(), pc2.end());
    BOOST_CHECK(pc1 == pc2);
}


template<class C>
void TestCIterateFor(const C&)
{
    typedef C Cont;
    for ( int t = 0; t < 100; ++t ) {
        Cont c;
        for ( int i = 0; i < 100; ++i ) {
            c.insert(c.end(), rand());
        }
        TestCIterate(c);
    }
}


template<class C>
void TestEraseIterateFor(const C&)
{
    typedef C Cont;
    for ( int t = 0; t < 100; ++t ) {
        Cont c;
        for ( int i = 0; i < 100; ++i ) {
            c.insert(c.end(), rand());
        }
        while ( !c.empty() ) {
            vector<int> pc;
            ITERATE ( typename Cont, it, c ) {
                pc.push_back(*it);
            }
            size_t p = rand()%pc.size();
            int v = pc[p];
            vector<int> nc;
            ERASE_ITERATE ( typename Cont, it, c ) {
                if ( *it == v ) {
                    c.erase(it);
                }
                else {
                    nc.push_back(*it);
                }
            }
            pc.erase(remove(pc.begin(), pc.end(), v), pc.end());
            BOOST_CHECK(pc == nc);
            nc.clear();
            ITERATE ( typename Cont, it, c ) {
                nc.push_back(*it);
            }
            BOOST_CHECK(pc == nc);
        }
    }
}


template<class C>
void TestEraseIterateForVec(const C&)
{
    typedef C Cont;
    for ( int t = 0; t < 100; ++t ) {
        Cont c;
        for ( int i = 0; i < 100; ++i ) {
            c.insert(c.end(), rand());
        }
        while ( !c.empty() ) {
            vector<int> pc;
            ITERATE ( typename Cont, it, c ) {
                pc.push_back(*it);
            }
            size_t p = rand()%pc.size();
            int v = pc[p];
            vector<int> nc;
            ERASE_ITERATE ( typename Cont, it, c ) {
                if ( *it == v ) {
                    VECTOR_ERASE(it, c);
                }
                else {
                    nc.push_back(*it);
                }
            }
            pc.erase(remove(pc.begin(), pc.end(), v), pc.end());
            BOOST_CHECK(pc == nc);
            nc.clear();
            ITERATE ( typename Cont, it, c ) {
                nc.push_back(*it);
            }
            BOOST_CHECK(pc == nc);
        }
    }
}


BOOST_AUTO_TEST_CASE(TestIterate)
{
    CCharVector cv("test");
    set<int> ts;
    list<int> tl;
    vector<int> tv;
    TestCIterate(cv);
    TestCIterateFor(ts);
    TestIterateFor(tl);
    TestIterateFor(tv);
}


BOOST_AUTO_TEST_CASE(TestEraseIterate)
{
    set<int> ts;
    list<int> tl;
    vector<int> tv;
    TestEraseIterateFor(ts);
    TestEraseIterateFor(tl);
    TestEraseIterateForVec(tv);
}


/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestStringNpos)
{
    BOOST_CHECK(string::npos == static_cast<string::size_type>(-1));
    BOOST_CHECK(NPOS == static_cast<SIZE_TYPE>(-1));
}

/////////////////////////////////////////////////////////////////////////////


CCheckMe<bool> chk1_True(void)
{
    return true;
}
CCheckMe<bool> chk1_False(void)
{
    return CCheckMe<bool>(false);
}
CCheckMe<bool> chk2_False(void)
{
    CCheckMe<bool> f(chk1_False());
    return f;
}
CCheckMe<bool> chk3_False(void)
{
    CCheckMe<bool> f(chk1_False());
    return CCheckMe<bool>(f);
}

class TwoInt
{
    public:
        TwoInt(int lo, int hi)
            : m_Lo(lo), m_Hi(hi)
        {
        }
        TwoInt(const TwoInt& t)
            : m_Lo(t.m_Lo), m_Hi(t.m_Hi)
        {
        }
        bool operator==(const int& lo) const
        {
            return m_Lo == lo && m_Hi == 0;
        }
    private:
        int m_Lo, m_Hi;
};

BOOST_AUTO_TEST_CASE(TestCheckMeType)
{
    {
        CCheckMe<bool> b(false);
        BOOST_CHECK(!b.IsChecked());
        bool v = b;
        BOOST_CHECK(!v);
        BOOST_CHECK(b.IsChecked());
    }
    {
        CCheckMe<bool> b(false);
        BOOST_CHECK(!b.IsChecked());
        bool v = !b;
        BOOST_CHECK(v);
        BOOST_CHECK(b.IsChecked());
    }
    {
        CCheckMe<bool> b(false);
        BOOST_CHECK(!b.IsChecked());
        bool v = (b == false);
        BOOST_CHECK(v);
        BOOST_CHECK(b.IsChecked());
    }
    {
        CCheckMe<bool> b(false);
        BOOST_CHECK(!b.IsChecked());
        bool v = (b != false);
        BOOST_CHECK(!v);
        BOOST_CHECK(b.IsChecked());
    }

    {
        CCheckMe<bool> b(false);
        BOOST_CHECK(!b.IsChecked());

        CCheckMe<bool> x = b;
        BOOST_CHECK(b.IsChecked());
        BOOST_CHECK(!x.IsChecked());

        CCheckMe<bool> y(false);
        BOOST_CHECK(!y.IsChecked());
        bool v = x == y;
        BOOST_CHECK(v);
        BOOST_CHECK(x.IsChecked());
        BOOST_CHECK(y.IsChecked());

        y = false;
        BOOST_CHECK(!y.IsChecked());
        y.SetChecked();
    }
    {
        CCheckMe<bool> x(true);
        BOOST_CHECK(!x.IsChecked());
        CCheckMe<bool> y(false);
        BOOST_CHECK(!y.IsChecked());
        bool v = x != y;
        BOOST_CHECK(v);
        BOOST_CHECK(x.IsChecked());
        BOOST_CHECK(y.IsChecked());
    }
    {
        // test that it does not crash here
        bool v = chk1_True();
        BOOST_CHECK(v);
        v = !chk1_True();
        BOOST_CHECK(!v);
        v = chk1_False();
        BOOST_CHECK(!v);
        v = !chk1_False();
        BOOST_CHECK(v);
        v = chk2_False();
        BOOST_CHECK(!v);
        v = !chk2_False();
        BOOST_CHECK(v);
        v = chk3_False();
        BOOST_CHECK(!v);
        v = !chk3_False();
        BOOST_CHECK(v);
    }
    {
        CCheckMe<bool> x = chk1_True();
        BOOST_CHECK(!x.IsChecked());
        x.SetChecked();

        x = chk1_False();
        BOOST_CHECK(!x.IsChecked());
        x.SetChecked();

        x = chk2_False();
        BOOST_CHECK(!x.IsChecked());
        x.SetChecked();

        x = chk3_False();
        BOOST_CHECK(!x.IsChecked());
        x.SetChecked();
    }
    {
        CCheckMe<bool> b;
        BOOST_CHECK(b.IsChecked());

        CCheckMe<bool> z;
        BOOST_CHECK(z.IsChecked());
        z = b;
        BOOST_CHECK(b.IsChecked());
        BOOST_CHECK(z.IsChecked());

        CCheckMe<bool> x = b;
        BOOST_CHECK(b.IsChecked());
        BOOST_CHECK(x.IsChecked());
        x = true;
        BOOST_CHECK(!x.IsChecked());

        CCheckMe<bool> y(false);
        BOOST_CHECK(!y.IsChecked());
        bool v = x == y;
        BOOST_CHECK(!v);
        BOOST_CHECK(x.IsChecked());
        BOOST_CHECK(y.IsChecked());
    }
    {
        CCheckMe<TwoInt> x(TwoInt(3,0));
        BOOST_CHECK(!x.IsChecked());
        bool v = x == 3;
        BOOST_CHECK(v);
        BOOST_CHECK(x.IsChecked());
    }
}


NCBITEST_AUTO_INIT()
{
    // Post error message
    ERR_POST("This message goes to the default diag.stream, CERR (2 times)");
}

NCBITEST_AUTO_FINI()
{
    SetDiagStream(0);
    assert( IsDiagStream(0) );
    assert( !IsDiagStream(&NcbiCout) );
}

#if 0
/////////////////////////////////////////////////////////////////////////////
// Test application
//
class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestStartup();
    TestDiag();
    TestDiag_ErrCodeInfo();
    TestException();
    TestException_AuxTrace();
    TestIostream();
    TestRegistry();
    TestThrowTrace();
    TestHeapStack();
    TestObjectSizes();
    TestBASE64Encoding();
    TestEraseIterate();
    TestStringNpos();

    NcbiCout << NcbiEndl << "CORETEST execution completed successfully!"
             << NcbiEndl << NcbiEndl << NcbiEndl;

    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
    assert( IsDiagStream(0) );
    assert( !IsDiagStream(&NcbiCout) );
}


  
/////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    // Post error message
    ERR_POST("This message goes to the default diag.stream, CERR (2 times)");
    // Execute main application function
    return CTestApplication().AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);
}
#endif
