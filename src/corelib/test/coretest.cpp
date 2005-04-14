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

#include <test/test_assert.h>  /* This header must go last */

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;



/////////////////////////////////
// I/O stream extensions
//

static void TestIostream(void)
{
    CNcbiIstrstream is("abc\nx0123456789\ny012345\r \tcba");
    string str;

    NcbiGetline(is, str, '\n');
    assert( is.good() );
    assert( str.compare("abc") == 0 );

    is >> str;
    assert( is.good() );
    assert( str.compare("x0123456789") == 0 );

    is >> str;
    assert( is.good() );
    assert( str.compare("y012345") == 0 );

    is >> str;
    assert( is.eof() );
    assert( str.compare("cba") == 0 );

    is >> str;
    assert( !is.good() );

    is.clear();
    is >> str;
    assert( !is.good() );

    str = "0 1 2 3 4 5\n6 7 8 9";
    NcbiCout << "String output: "  << str << NcbiEndl;
}



/////////////////////////////////
// Registry
//

static void TestRegistry(void)
{
    CNcbiRegistry reg;
    assert( reg.Empty() );

    list<string> sections;
    reg.EnumerateSections(&sections);
    assert( sections.empty() );

    list<string> entries;
    reg.EnumerateEntries(kEmptyStr, &entries);
    assert( entries.empty() );

    // Compose a test registry
    assert(  reg.Set("Section0", "Name01", "Val01_BAD!!!") );
    assert(  reg.Set("Section1 ", "\nName11", "Val11_t") );
    assert( !reg.Empty() );
    assert(  reg.Get(" Section1", "Name11\t") == "Val11_t" );
    assert(  reg.Get("Section1", "Name11",
                      CNcbiRegistry::ePersistent).empty() );
    assert(  reg.Set("Section1", "Name11", "Val11_t") );
    assert( !reg.Set("Section1", "Name11", "Val11_BAD!!!",
                      CNcbiRegistry::eNoOverride) );

    assert(  reg.Set("   Section2", "\nName21  ", "Val21",
                      CNcbiRegistry::ePersistent |
                      CNcbiRegistry::eNoOverride) );
    assert(  reg.Set("Section2", "Name21", "Val21_t") );
    assert( !reg.Empty() );
    assert(  reg.Set("Section3", "Name31", "Val31_t") );

    assert( reg.Get(" \nSection1", " Name11  ") == "Val11_t" );
    assert( reg.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
             "Val21" );
    assert( reg.Get(" Section2", " Name21\n") == "Val21_t" );
    assert( reg.Get("SectionX", "Name21").empty() );

    assert( reg.Set("Section4", "Name41", "Val410 Val411 Val413",
                     CNcbiRegistry::ePersistent) );
    assert(!reg.Set("Sect ion4", "Name41", "BAD1",
                     CNcbiRegistry::ePersistent) );
    assert(!reg.Set("Section4", "Na me41", "BAD2") );
    assert( reg.Set("SECTION4", "Name42", "V420 V421\nV422 V423 \"",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("Section4", "NAME43",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("\tSection4", "Name43T",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    assert( reg.Set("Section4", "Name44", "\n V440 V441 \r\n",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("\r Section4", "  \t\rName45", "\r\n V450 V451  \n  ",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("Section4 \n", "  Name46  ", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set(" Section4", "Name46T", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    assert( reg.Set("Section4", "Name47", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("Section4", "Name47T", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    string xxx("\" V481\" \n\"V482 ");
    assert( reg.Set("Section4", "Name48", xxx, CNcbiRegistry::ePersistent) );

    assert( reg.Set("Section5", "Name51", "Section5/Name51",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("_Section_5", "Name51", "_Section_5/Name51",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("_Section_5_", "_Name52", "_Section_5_/_Name52",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("_Section_5_", "Name52", "_Section_5_/Name52",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("_Section_5_", "_Name53_", "_Section_5_/_Name53_",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("Section-5.6", "Name-5.6", "Section-5.6/Name-5.6",
                     CNcbiRegistry::ePersistent) );
    assert( reg.Set("-Section_5", ".Name.5-3", "-Section_5/.Name.5-3",
                     CNcbiRegistry::ePersistent) );


    // Dump
    CNcbiOstrstream os;
    assert ( reg.Write(os) );
    os << '\0';
    const char* os_str = os.str();  os.rdbuf()->freeze(false);
    NcbiCerr << "\nRegistry:\n" << os_str << NcbiEndl;

    // "Persistent" load
    CNcbiIstrstream is1(os_str);
    CNcbiRegistry  reg1(is1);
    assert(  reg1.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
              "Val21" );
    assert(  reg1.Get("Section2", "Name21") == "Val21" );
    assert( !reg1.Set("Section2", "Name21", NcbiEmptyString) );
    assert( !reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    assert( !reg1.Empty() );
    assert(  reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );

    // "Transient" load
    CNcbiIstrstream is2(os_str);
    CNcbiRegistry  reg2(is2, CNcbiRegistry::eTransient);
    assert(  reg2.Get("Section2", "Name21",
                       CNcbiRegistry::ePersistent).empty() );
    assert(  reg2.Get("Section2", "Name21") == "Val21" );
    assert( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );
    assert( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    assert( !reg2.Empty() );
    assert(  reg2.Set("Section2", "Name21", NcbiEmptyString) );


    assert( reg.Get("Sect ion4 ", "Name41 ").empty() );
    assert( reg.Get("Section4 ", "Na me41 ").empty() );

    assert( reg.Get("Section4 ", "Name41 ") == "Val410 Val411 Val413" );
    assert( reg.Get("Section4",  " Name42") == "V420 V421\nV422 V423 \"" );
    assert( reg.Get("Section4",  "Name43")  == " \tV430 V431  \n V432 V433 ");
    assert( reg.Get("Section4",  "Name43T") == "V430 V431  \n V432 V433" );
    assert( reg.Get("Section4",  " Name44") == "\n V440 V441 \r\n" );
    assert( reg.Get(" SecTIon4", "Name45")  == "\r\n V450 V451  \n  " );
    assert( reg.Get("SecTion4 ", "Name46")  == "\n\nV460\" \n \t \n\t" );
    assert( reg.Get("Section4",  "NaMe46T") == "\n\nV460\" \n \t \n" );
    assert( reg.Get(" Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    assert( reg.Get("Section4 ", "NAme47T") == "470\n471\\\n 472\\\n473\\" );
    assert( reg.Get("Section4",  "Name48")  == xxx );

    assert( reg2.Get("Section4", "Name41")  == "Val410 Val411 Val413" );
    assert( reg2.Get("Section4", "Name42")  == "V420 V421\nV422 V423 \"" );
    assert( reg2.Get("Section4", "Name43")  == " \tV430 V431  \n V432 V433 ");
    assert( reg2.Get("Section4", "Name43T") == "V430 V431  \n V432 V433" );
    assert( reg2.Get("Section4", "Name44")  == "\n V440 V441 \r\n" );
    assert( reg2.Get("Section4", "NaMe45")  == "\r\n V450 V451  \n  " );
    assert( reg2.Get("SecTIOn4", "NAme46")  == "\n\nV460\" \n \t \n\t" );
    assert( reg2.Get("Section4", "Name46T") == "\n\nV460\" \n \t \n" );
    assert( reg2.Get("Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    assert( reg2.Get("Section4", "Name47T") == "470\n471\\\n 472\\\n473\\" );
    assert( reg2.Get("Section4", "Name48")  == xxx );

    assert( reg2.Get(" Section5",    "Name51 ")   == "Section5/Name51" );
    assert( reg2.Get("_Section_5",   " Name51")   == "_Section_5/Name51" );
    assert( reg2.Get(" _Section_5_", " _Name52")  == "_Section_5_/_Name52");
    assert( reg2.Get("_Section_5_ ", "Name52")    == "_Section_5_/Name52");
    assert( reg2.Get("_Section_5_",  "_Name53_ ") == "_Section_5_/_Name53_" );
    assert( reg2.Get(" Section-5.6", "Name-5.6 ") == "Section-5.6/Name-5.6");
    assert( reg2.Get("-Section_5",   ".Name.5-3") == "-Section_5/.Name.5-3");

    // Printout of the whole registry content
    assert( reg.Set("Section0", "Name01", "") );
    reg.EnumerateSections(&sections);
    assert( find(sections.begin(), sections.end(), "Section0")
             == sections.end() );
    assert( find(sections.begin(), sections.end(), "Section1")
             != sections.end() );
    assert( !sections.empty() );
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
    assert( reg.Empty() );
    
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



/////////////////////////////////
// Start-up (cmd.-line args, config file)
//

static void TestStartup(void)
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


/////////////////////////////////
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


static void TestDiag(void)
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
    assert(  IsDiagStream(&NcbiCerr) );
    assert( !IsDiagStream(&NcbiCout) );
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

    EDiagSev prev_sev;
    IgnoreDiagDieLevel(true, &prev_sev);
    {{
        CNcbiDiag x_diag;
        x_diag << Fatal << "Fatal message w/o exit()/abort()!" << Endm;
    }}
    IgnoreDiagDieLevel(false);
    SetDiagDieLevel(prev_sev);
}


static void TestDiag_ErrCodeInfo(void)
{
    CNcbiDiag diag;

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

    assert(info.GetDescription(ErrCode(1,1), &desc));
    assert(desc.m_Message     == "Short message for error code (1,1)");
    assert(desc.m_Explanation == "This is a long message for error code (1,1).");
    assert(desc.m_Severity    == 3);

    assert(info.GetDescription(ErrCode(9,0), &desc));
    assert(desc.m_Severity    == 2);
    assert(info.GetDescription(ErrCode(9,1), &desc));
    assert(desc.m_Severity    == eDiag_Info);
    assert(info.GetDescription(ErrCode(9,2), &desc));
    assert(desc.m_Severity    == 2);

    assert(info.GetDescription(ErrCode(2,4), &desc));
    assert(desc.m_Message     == "Err 2.4");
    assert(desc.m_Explanation == "Error 2.4");
    assert(desc.m_Severity    == eDiag_Error);

    assert(!info.GetDescription(ErrCode(1,3), &desc));

    SetDiagErrCodeInfo(&info);
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
}


/////////////////////////////////
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
    NcbiCerr << "XXX: " << long(this) << NcbiEndl;
}
XXX::XXX(const XXX&x) : mess(x.mess) {
    NcbiCerr << "XXX(" << long(&x) << "): " << long(this) << NcbiEndl;
}
XXX::~XXX(void) {
    NcbiCerr << "~XXX: " << long(this) << NcbiEndl;
}

static void s_ThrowXXX(void) {
    NcbiCerr << NcbiEndl << "Throw class XXX" << NcbiEndl;
    throw XXX("s_ThrowXXX message");
   
}

static void TestException_Features(void)
{
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
}


static void TestException_Std(void)
{
    try { TE_runtime(); }
    catch (runtime_error& e) {
        NcbiCerr << "CATCH TE_runtime::runtime_error : " << e.what()<<NcbiEndl;
    }

    try { TE_runtime(); }
    catch (exception& e) {
        NcbiCerr << "CATCH TE_runtime::exception " << e.what() << NcbiEndl;
    }

    try { TE_runtime(); }
    STD_CATCH ("STD_CATCH" << ' ' << "TE_runtime ");

    try { TE_runtime(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_runtime::logic_error " << e.what() << NcbiEndl;
        _TROUBLE; }
    STD_CATCH_ALL ("STD_CATCH_ALL" << " " << "TE_runtime");

    TE_none();

    try { TE_logic(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_logic " << e.what() << NcbiEndl; }
    STD_CATCH_ALL ("try { TE_logic(); }  SOMETHING IS WRONG!");
}


static void TestException_Aux(void)
{
    try {
        assert( !strtod("1e-999999", 0) );
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
}


static void TestException_AuxTrace(void)
{
    try {
        assert( !strtod("1e-999999", 0) );
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
        } catch (const char* _DEBUG_ARG(e)) {
            _TRACE("THROW0_TRACE: " << e);
            RETHROW_TRACE;
        }
    } catch (const char* _DEBUG_ARG(e)) {
        _TRACE("RETHROW_TRACE: " << e);
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


static void TestException(void)
{
    SetDiagStream(&NcbiCout);

    TestException_Features();
    TestException_Std();
    TestException_Aux();
}


static void TestThrowTrace(void)
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

static void TestHeapStack(void)
{
    SetDiagTrace(eDT_Enable);
    {
      NcbiCerr << "Test CObjectInt in stack:" << NcbiEndl;
      CObjectInt stackObj;
      assert(!stackObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObjectInt on heap:" << NcbiEndl;
      CObjectInt* heapObj = new CObjectInt();
      assert(heapObj->CanBeDeleted());
    }
    {
      NcbiCerr << "Test static CObjectInt:" << NcbiEndl;
      static CObjectInt staticObj;
      assert(!staticObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CRef on CObjectInt on heap:" << NcbiEndl;
      CRef<CObjectInt> objRef(new CObjectInt());
      assert(objRef->CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObject in stack:" << NcbiEndl;
      CObject stackObj;
      assert(!stackObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CObject on heap:" << NcbiEndl;
      CObject* heapObj = new CObject();
      assert(heapObj->CanBeDeleted());
    }
    {
      NcbiCerr << "Test static CObject:" << NcbiEndl;
      static CObject staticObj;
      assert(!staticObj.CanBeDeleted());
    }
    {
      NcbiCerr << "Test CRef on CObject on heap:" << NcbiEndl;
      CRef<CObject> objRef(new CObject());
      assert(objRef->CanBeDeleted());
    }
    SetDiagTrace(eDT_Default);
}


/////////////////////////////////
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
// APPLICATION OBJECT
//   and
// MAIN
//


// Note that if the application's object ("theTestApplication") was defined
// inside the scope of function "main()", then its destructor could be
// called *before* destructors of other statically allocated objects
// defined in other modules.
// It would cause a premature closure of diag. stream, and disallow the
// destructors of other projects to refer to this application object:
//  - the singleton method CNcbiApplication::Instance() would return NULL, and
//  - if there is a "raw"(direct) pointer to "theTestApplication" then it
//    might cause a real trouble.
static CTestApplication theTestApplication;


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    // Post error message
    ERR_POST("This message goes to the default diag.stream, CERR");
    // Execute main application function
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.94  2005/04/14 20:27:03  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.93  2005/02/16 15:04:35  ssikorsk
 * Tweaked kEmptyStr with Linux GCC
 *
 * Revision 1.92  2004/09/22 16:57:35  ucko
 * Tweak TestThrowTrace to avoid confusing older GCC versions.
 *
 * Revision 1.91  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.90  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.89  2003/10/06 16:59:37  ivanov
 * Use macro _TROUBLE instead assert(0)
 *
 * Revision 1.88  2003/04/25 20:54:40  lavr
 * Add test for IgnoreDiagDieLevel() operations
 *
 * Revision 1.87  2003/02/24 19:56:51  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.86  2002/08/28 17:06:18  vasilche
 * Added code for checking heap detection
 *
 * Revision 1.85  2002/08/16 15:02:32  ivanov
 * Added test for class CDiagAutoPrefix
 *
 * Revision 1.84  2002/08/01 18:52:27  ivanov
 * Added test for output verbose messages for error codes
 *
 * Revision 1.83  2002/07/15 18:53:36  gouriano
 * renamed CNcbiException to CException
 *
 * Revision 1.82  2002/07/15 18:17:25  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.81  2002/07/11 14:18:28  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.80  2002/04/16 18:49:06  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.79  2001/10/15 19:48:24  vakatov
 * Use two #if's instead of "#if ... && ..." as KAI cannot handle #if x == y
 *
 * Revision 1.78  2001/09/05 18:47:12  ucko
 * Work around WorkShop 5.3 type-equivalence bug.
 *
 * Revision 1.77  2001/07/30 14:42:27  lavr
 * eDiag_Trace and eDiag_Fatal always print as much as possible
 *
 * Revision 1.76  2001/07/25 19:14:14  lavr
 * Added test for date/time stamp in message logging
 *
 * Revision 1.75  2001/06/22 21:46:15  ivanov
 * Added test for read/write the registry file with comments
 *
 * Revision 1.74  2001/06/13 23:19:39  vakatov
 * Revamped previous revision (prefix and error codes)
 *
 * Revision 1.73  2001/06/13 20:50:00  ivanov
 * Added test for stack post prefix messages and ErrCode manipulator.
 *
 * Revision 1.72  2001/03/26 20:59:57  vakatov
 * String-related tests moved to "test_ncbistr.cpp" (by A.Grichenko)
 *
 * Revision 1.71  2001/01/30 00:40:28  vakatov
 * Do not use arg "envp[]" in main() -- as it is not working on MAC
 *
 * Revision 1.70  2000/12/11 20:42:52  vakatov
 * + NStr::PrintableString()
 *
 * Revision 1.69  2000/10/24 21:51:23  vakatov
 * [DEBUG] By default, do not print file name and line into the diagnostics
 *
 * Revision 1.68  2000/10/24 19:54:48  vakatov
 * Diagnostics to go to CERR by default (was -- disabled by default)
 *
 * Revision 1.67  2000/10/11 21:03:50  vakatov
 * Cleanup to avoid 64-bit to 32-bit values truncation, etc.
 * (reported by Forte6 Patch 109490-01)
 *
 * Revision 1.66  2000/08/03 20:21:35  golikov
 * Added predicate PCase for AStrEquiv
 * PNocase, PCase goes through NStr::Compare now
 *
 * Revision 1.65  2000/06/26 20:56:35  vakatov
 * TestDiag() -- using user-istalled message posting handler
 *
 * Revision 1.64  2000/06/23 19:57:19  vakatov
 * TestDiag() -- added tests for the switching diag.handlers
 *
 * Revision 1.63  2000/06/11 01:47:34  vakatov
 * IsDiagSet(0) to return TRUE if the diag stream is unset
 *
 * Revision 1.62  2000/06/09 21:22:49  vakatov
 * Added test for IsDiagStream()
 *
 * Revision 1.61  2000/05/24 20:57:13  vasilche
 * Use new macro _DEBUG_ARG to avoid warning about unused argument.
 *
 * Revision 1.60  2000/04/19 18:36:44  vakatov
 * Test NStr::Compare() for non-zero "pos"
 *
 * Revision 1.59  2000/04/17 04:16:25  vakatov
 * Added tests for NStr::Compare() and NStr::ToLower/ToUpper()
 *
 * Revision 1.58  2000/04/04 22:34:32  vakatov
 * Checks for the NStr:: for "long", and for the debug tracing
 *
 * Revision 1.57  2000/02/18 16:54:10  vakatov
 * + eDiag_Critical
 *
 * Revision 1.56  2000/01/20 17:55:48  vakatov
 * Fixes to follow the "CNcbiApplication" change.
 *
 * Revision 1.55  1999/11/29 17:49:12  golikov
 * NStr::Replace tests modified obedience to Denis :)
 *
 * Revision 1.54  1999/11/26 18:45:30  golikov
 * NStr::Replace tests added
 *
 * Revision 1.53  1999/11/12 17:33:24  vakatov
 * To be more careful with _DEBUG to suit some ugly MSVC++ features
 *
 * Revision 1.52  1999/10/04 16:21:06  vasilche
 * Added full set of macros THROW*_TRACE
 *
 * Revision 1.51  1999/09/29 18:21:58  vakatov
 * + TestException_Features() -- to test how the thrown object is handled
 *
 * Revision 1.50  1999/09/02 21:54:42  vakatov
 * Tests for CNcbiRegistry:: allowed '-' and '.' in the section/entry name
 *
 * Revision 1.49  1999/08/30 16:00:45  vakatov
 * CNcbiRegistry:: Get()/Set() -- force the "name" and "section" to
 * consist of alphanumeric and '_' only;  ignore leading and trailing
 * spaces
 *
 * Revision 1.48  1999/07/07 14:17:07  vakatov
 * CNcbiRegistry::  made the section and entry names be case-insensitive
 *
 * Revision 1.47  1999/07/06 15:26:37  vakatov
 * CNcbiRegistry::
 *   - allow multi-line values
 *   - allow values starting and ending with space symbols
 *   - introduced EFlags/TFlags for optional parameters in the class
 *     member functions -- rather than former numerous boolean parameters
 *
 * Revision 1.46  1999/05/27 15:22:16  vakatov
 * Extended and fixed tests for the StringToXXX() functions
 *
 * Revision 1.45  1999/05/11 14:47:38  vakatov
 * Added missing <algorithm> header (for MSVC++)
 *
 * Revision 1.44  1999/05/11 02:53:52  vakatov
 * Moved CGI API from "corelib/" to "cgi/"
 *
 * Revision 1.43  1999/05/10 14:26:13  vakatov
 * Fixes to compile and link with the "egcs" C++ compiler under Linux
 *
 * Revision 1.42  1999/05/04 16:14:50  vasilche
 * Fixed problems with program environment.
 * Added class CNcbiEnvironment for cached access to C environment.
 *
 * Revision 1.41  1999/05/04 00:03:16  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.40  1999/05/03 20:32:31  vakatov
 * Use the (newly introduced) macro from <corelib/ncbidbg.h>:
 *   RETHROW_TRACE,
 *   THROW0_TRACE(exception_class),
 *   THROW1_TRACE(exception_class, exception_arg),
 *   THROW_TRACE(exception_class, exception_args)
 * instead of the former (now obsolete) macro _TRACE_THROW.
 *
 * Revision 1.39  1999/04/30 19:21:06  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.38  1999/04/27 14:50:12  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.37  1999/04/14 20:12:52  vakatov
 * + <stdio.h>, <stdlib.h>
 *
 * Revision 1.36  1999/03/12 18:04:09  vakatov
 * Added ERR_POST macro to perform a plain "standard" error posting
 *
 * Revision 1.35  1999/01/21 16:18:04  sandomir
 * minor changes due to NStr namespace to contain string utility functions
 *
 * Revision 1.34  1999/01/12 17:10:16  sandomir
 * test restored
 *
 * Revision 1.33  1999/01/12 17:06:37  sandomir
 * GetLink changed
 *
 * Revision 1.32  1999/01/07 21:15:24  vakatov
 * Changed prototypes for URL_DecodeString() and URL_EncodeString()
 *
 * Revision 1.31  1999/01/07 20:06:06  vakatov
 * + URL_DecodeString()
 * + URL_EncodeString()
 *
 * Revision 1.30  1999/01/04 22:41:44  vakatov
 * Do not use so-called "hardware-exceptions" as these are not supported
 * (on the signal level) by UNIX
 * Do not "set_unexpected()" as it works differently on UNIX and MSVC++
 *
 * Revision 1.29  1998/12/28 17:56:43  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.28  1998/12/15 15:43:24  vasilche
 * Added utilities to convert string <> int.
 *
 * Revision 1.27  1998/12/11 18:00:56  vasilche
 * Added cookies and output stream
 *
 * Revision 1.26  1998/12/10 22:59:49  vakatov
 * CNcbiRegistry:: API is ready(and by-and-large tested)
 *
 * Revision 1.25  1998/12/10 18:05:40  vakatov
 * CNcbiReg::  Just passed a draft test.
 *
 * Revision 1.24  1998/12/10 17:36:56  sandomir
 * ncbires.cpp added
 * 
 * Revision 1.23  1998/12/09 19:38:53  vakatov
 * Started with TestRegistry().  Freeze in the "compilable" state.
 *
 * Revision 1.22  1998/12/07 23:48:03  vakatov
 * Changes in the usage of CCgiApplication class
 *
 * Revision 1.21  1998/12/03 21:24:23  sandomir
 * NcbiApplication and CgiApplication updated
 *
 * Revision 1.20  1998/12/03 16:40:15  vakatov
 * Initial revision
 * Aux. function "Getline()" to read from "istream" to a "string"
 * Adopted standard I/O "string" <--> "istream" for old-fashioned streams
 *
 * Revision 1.19  1998/12/01 00:27:21  vakatov
 * Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
 * Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
 *
 * Revision 1.18  1998/11/30 21:23:20  vakatov
 * CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
 * + CCgiRequest::ParseIndexesAsEntries()
 * Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
 *
 * Revision 1.17  1998/11/27 20:55:23  vakatov
 * CCgiRequest::  made the input stream arg. be optional(std.input by default)
 *
 * Revision 1.16  1998/11/27 19:46:06  vakatov
 * TestCgi() -- test the query string passed as a cmd.-line argument
 *
 * Revision 1.15  1998/11/27 15:55:07  vakatov
 * + TestCgi(USER STDIN)
 *
 * Revision 1.14  1998/11/26 00:29:55  vakatov
 * Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
 *
 * Revision 1.13  1998/11/24 23:07:31  vakatov
 * Draft(almost untested) version of CCgiRequest API
 *
 * Revision 1.12  1998/11/24 21:31:34  vakatov
 * Updated with the ISINDEX-related code for CCgiRequest::
 * TCgiEntries, ParseIndexes(), GetIndexes(), etc.
 *
 * Revision 1.11  1998/11/24 17:52:38  vakatov
 * + TestException_Aux() -- tests for CErrnoException:: and CErrnoException::
 * + TestCgi_Request()   -- tests for CCgiRequest::ParseEntries()
 *
 * Revision 1.10  1998/11/20 22:34:39  vakatov
 * Reset diag. stream to get rid of a mem.leak
 *
 * Revision 1.9  1998/11/19 23:41:14  vakatov
 * Tested version of "CCgiCookie::" and "CCgiCookies::"
 *
 * Revision 1.8  1998/11/13 00:18:08  vakatov
 * Added a test for the "unexpected" exception.
 * Turned off "hardware" exception tests for UNIX.
 *
 * Revision 1.7  1998/11/10 01:17:38  vakatov
 * Cleaned, adopted to the standard NCBI C++ framework and incorporated
 * the "hardware exceptions" code and tests(originally written by
 * V.Sandomirskiy).
 * Only tested for MSVC++ compiler yet -- to be continued for SunPro...
 *
 * Revision 1.6  1998/11/06 22:42:42  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * Revision 1.5  1998/11/04 23:48:15  vakatov
 * Replaced <ncbidiag> by <ncbistd>
 *
 * ==========================================================================
 */
