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
* --------------------------------------------------------------------------
* $Log$
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
* ==========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <algorithm>

// Workaround for non-Debug compilation
#ifndef _DEBUG
#undef _ASSERT
#define _ASSERT(expr) ((void)(expr))
#endif


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
    _ASSERT( is.good() );
    _ASSERT( str.compare("abc") == 0 );

    is >> str;
    _ASSERT( is.good() );
    _ASSERT( str.compare("x0123456789") == 0 );

    is >> str;
    _ASSERT( is.good() );
    _ASSERT( str.compare("y012345") == 0 );

    is >> str;
    _ASSERT( is.eof() );
    _ASSERT( str.compare("cba") == 0 );

    is >> str;
    _ASSERT( !is.good() );

    is.clear();
    is >> str;
    _ASSERT( !is.good() );

    str = "0 1 2 3 4 5\n6 7 8 9";
    NcbiCout << "String output: "  << str << NcbiEndl;
}



/////////////////////////////////
// Registry
//

static void TestRegistry(void)
{
    CNcbiRegistry reg;
    _ASSERT( reg.Empty() );

    list<string> sections;
    reg.EnumerateSections(&sections);
    _ASSERT( sections.empty() );

    list<string> entries;
    reg.EnumerateEntries(NcbiEmptyString, &entries);
    _ASSERT( entries.empty() );

    // Compose a test registry
    _ASSERT(  reg.Set("Section0", "Name01", "Val01_BAD!!!") );
    _ASSERT(  reg.Set("Section1 ", "\nName11", "Val11_t") );
    _ASSERT( !reg.Empty() );
    _ASSERT(  reg.Get(" Section1", "Name11\t") == "Val11_t" );
    _ASSERT(  reg.Get("Section1", "Name11",
                      CNcbiRegistry::ePersistent).empty() );
    _ASSERT(  reg.Set("Section1", "Name11", "Val11_t") );
    _ASSERT( !reg.Set("Section1", "Name11", "Val11_BAD!!!",
                      CNcbiRegistry::eNoOverride) );

    _ASSERT(  reg.Set("   Section2", "\nName21  ", "Val21",
                      CNcbiRegistry::ePersistent |
                      CNcbiRegistry::eNoOverride) );
    _ASSERT(  reg.Set("Section2", "Name21", "Val21_t") );
    _ASSERT( !reg.Empty() );
    _ASSERT(  reg.Set("Section3", "Name31", "Val31_t") );

    _ASSERT( reg.Get(" \nSection1", " Name11  ") == "Val11_t" );
    _ASSERT( reg.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
             "Val21" );
    _ASSERT( reg.Get(" Section2", " Name21\n") == "Val21_t" );
    _ASSERT( reg.Get("SectionX", "Name21").empty() );

    _ASSERT( reg.Set("Section4", "Name41", "Val410 Val411 Val413",
                     CNcbiRegistry::ePersistent) );
    _ASSERT(!reg.Set("Sect ion4", "Name41", "BAD1",
                     CNcbiRegistry::ePersistent) );
    _ASSERT(!reg.Set("Section4", "Na me41", "BAD2") );
    _ASSERT( reg.Set("SECTION4", "Name42", "V420 V421\nV422 V423 \"",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("Section4", "NAME43",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("\tSection4", "Name43T",
                     " \tV430 V431  \n V432 V433 ",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    _ASSERT( reg.Set("Section4", "Name44", "\n V440 V441 \r\n",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("\r Section4", "  \t\rName45", "\r\n V450 V451  \n  ",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("Section4 \n", "  Name46  ", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set(" Section4", "Name46T", "\n\nV460\" \n \t \n\t",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    _ASSERT( reg.Set("Section4", "Name47", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("Section4", "Name47T", "470\n471\\\n 472\\\n473\\",
                     CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate) );
    string xxx("\" V481\" \n\"V482 ");
    _ASSERT( reg.Set("Section4", "Name48", xxx, CNcbiRegistry::ePersistent) );

    _ASSERT( reg.Set("Section5", "Name51", "Section5/Name51",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("_Section_5", "Name51", "_Section_5/Name51",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("_Section_5_", "_Name52", "_Section_5_/_Name52",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("_Section_5_", "Name52", "_Section_5_/Name52",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("_Section_5_", "_Name53_", "_Section_5_/_Name53_",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("Section-5.6", "Name-5.6", "Section-5.6/Name-5.6",
                     CNcbiRegistry::ePersistent) );
    _ASSERT( reg.Set("-Section_5", ".Name.5-3", "-Section_5/.Name.5-3",
                     CNcbiRegistry::ePersistent) );


    // Dump
    CNcbiOstrstream os;
    _ASSERT ( reg.Write(os) );
    os << '\0';
    const char* os_str = os.str();  os.rdbuf()->freeze(false);
    NcbiCerr << "\nRegistry:\n" << os_str << NcbiEndl;

    // "Persistent" load
    CNcbiIstrstream is1(os_str);
    CNcbiRegistry  reg1(is1);
    _ASSERT(  reg1.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
              "Val21" );
    _ASSERT(  reg1.Get("Section2", "Name21") == "Val21" );
    _ASSERT( !reg1.Set("Section2", "Name21", NcbiEmptyString) );
    _ASSERT( !reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    _ASSERT( !reg1.Empty() );
    _ASSERT(  reg1.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );

    // "Transient" load
    CNcbiIstrstream is2(os_str);
    CNcbiRegistry  reg2(is2, CNcbiRegistry::eTransient);
    _ASSERT(  reg2.Get("Section2", "Name21",
                       CNcbiRegistry::ePersistent).empty() );
    _ASSERT(  reg2.Get("Section2", "Name21") == "Val21" );
    _ASSERT( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent) );
    _ASSERT( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                       CNcbiRegistry::ePersistent |
                       CNcbiRegistry::eNoOverride) );
    _ASSERT( !reg2.Empty() );
    _ASSERT(  reg2.Set("Section2", "Name21", NcbiEmptyString) );


    _ASSERT( reg.Get("Sect ion4 ", "Name41 ").empty() );
    _ASSERT( reg.Get("Section4 ", "Na me41 ").empty() );

    _ASSERT( reg.Get("Section4 ", "Name41 ") == "Val410 Val411 Val413" );
    _ASSERT( reg.Get("Section4",  " Name42") == "V420 V421\nV422 V423 \"" );
    _ASSERT( reg.Get("Section4",  "Name43")  == " \tV430 V431  \n V432 V433 ");
    _ASSERT( reg.Get("Section4",  "Name43T") == "V430 V431  \n V432 V433" );
    _ASSERT( reg.Get("Section4",  " Name44") == "\n V440 V441 \r\n" );
    _ASSERT( reg.Get(" SecTIon4", "Name45")  == "\r\n V450 V451  \n  " );
    _ASSERT( reg.Get("SecTion4 ", "Name46")  == "\n\nV460\" \n \t \n\t" );
    _ASSERT( reg.Get("Section4",  "NaMe46T") == "\n\nV460\" \n \t \n" );
    _ASSERT( reg.Get(" Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    _ASSERT( reg.Get("Section4 ", "NAme47T") == "470\n471\\\n 472\\\n473\\" );
    _ASSERT( reg.Get("Section4",  "Name48")  == xxx );

    _ASSERT( reg2.Get("Section4", "Name41")  == "Val410 Val411 Val413" );
    _ASSERT( reg2.Get("Section4", "Name42")  == "V420 V421\nV422 V423 \"" );
    _ASSERT( reg2.Get("Section4", "Name43")  == " \tV430 V431  \n V432 V433 ");
    _ASSERT( reg2.Get("Section4", "Name43T") == "V430 V431  \n V432 V433" );
    _ASSERT( reg2.Get("Section4", "Name44")  == "\n V440 V441 \r\n" );
    _ASSERT( reg2.Get("Section4", "NaMe45")  == "\r\n V450 V451  \n  " );
    _ASSERT( reg2.Get("SecTIOn4", "NAme46")  == "\n\nV460\" \n \t \n\t" );
    _ASSERT( reg2.Get("Section4", "Name46T") == "\n\nV460\" \n \t \n" );
    _ASSERT( reg2.Get("Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    _ASSERT( reg2.Get("Section4", "Name47T") == "470\n471\\\n 472\\\n473\\" );
    _ASSERT( reg2.Get("Section4", "Name48")  == xxx );

    _ASSERT( reg2.Get(" Section5",    "Name51 ")   == "Section5/Name51" );
    _ASSERT( reg2.Get("_Section_5",   " Name51")   == "_Section_5/Name51" );
    _ASSERT( reg2.Get(" _Section_5_", " _Name52")  == "_Section_5_/_Name52");
    _ASSERT( reg2.Get("_Section_5_ ", "Name52")    == "_Section_5_/Name52");
    _ASSERT( reg2.Get("_Section_5_",  "_Name53_ ") == "_Section_5_/_Name53_" );
    _ASSERT( reg2.Get(" Section-5.6", "Name-5.6 ") == "Section-5.6/Name-5.6");
    _ASSERT( reg2.Get("-Section_5",   ".Name.5-3") == "-Section_5/.Name.5-3");

    // Printout of the whole registry content
    _ASSERT( reg.Set("Section0", "Name01", "") );
    reg.EnumerateSections(&sections);
    _ASSERT( find(sections.begin(), sections.end(), "Section0")
             == sections.end() );
    _ASSERT( find(sections.begin(), sections.end(), "Section1")
             != sections.end() );
    _ASSERT( !sections.empty() );
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
    _ASSERT( reg.Empty() );
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


static void TestDiag(void)
{
    CNcbiDiag diag;
    double d = 123.45;

    diag << "[Unset Diag Stream]  Diagnostics double = " << d << Endm;
    ERR_POST( "[Unset Diag Stream]  ERR_POST double = " << d );
    _TRACE( "[Unset Diag Stream]  Trace double = " << d );

    NcbiCout << NcbiEndl
             << "FLUSHing memory-stored diagnostics (if any):" << NcbiEndl;
    SIZE_TYPE n_flush = CNcbiApplication::Instance()->FlushDiag(&NcbiCout);
    NcbiCout << "FLUSHed diag: " << n_flush << " bytes" << NcbiEndl <<NcbiEndl;

    SetDiagStream(&NcbiCerr);
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

    diag << Trace << "This message has severity \"Trace\"" << Endm;

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
    SetDiagPostPrefix(0);
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
        _VERIFY( !strtod("1e-999999", 0) );
        throw CErrnoException("Failed strtod(\"1e-999999\", 0)");
    }
    catch (CErrnoException& e) {
        NcbiCerr << "TEST CErrnoException ---> " << e.what() << NcbiEndl;
    }
    try {
        THROW_TRACE(CParseException, ("Failed parsing(at pos. 123)", 123));
    }
    STD_CATCH ("TEST CParseException ---> ");
}


static void TestException_AuxTrace(void)
{
    try {
        _VERIFY( !strtod("1e-999999", 0) );
        THROW1_TRACE(CErrnoException, "Failed strtod('1e-999999', 0)");
    }
    catch (CErrnoException& e) {
        NcbiCerr << "THROW1_TRACE CErrnoException ---> " << e.what()
                 << NcbiEndl;
    }

    try {
        try {
            THROW0_TRACE("Throw a string");
        } catch (const char* e) {
            _TRACE("THROW0_TRACE: " << e);
            RETHROW_TRACE;
        }
    } catch (const char* e) {
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
        THROW_TRACE(CParseException, ("Failed parsing(at pos. 123)", 123));
    }
    STD_CATCH ("THROW_TRACE CParseException ---> ");
}


static void TestException(void)
{
    SetDiagStream(&NcbiCout);

    TestException_Features();
    TestException_Std();
    TestException_Aux();
}



/////////////////////////////////
// Utilities
//

static void TestUtilities(void)
{
    static const string s_Strings[] = {
        "",
        "1.",
        "-2147483649",
        "-2147483648",
        "-1",
        "0",
        "2147483647",
        "2147483648",
        "4294967295",
        "4294967296",
        "zzz"
    };

    NcbiCout << "TestUtilities:" << NcbiEndl;

    const int count = sizeof(s_Strings) / sizeof(s_Strings[0]);

    for ( int i = 0; i < count; ++i ) {
        const string& str = s_Strings[i];
        NcbiCout << "Checking string: '" << str << "':" << NcbiEndl;

        try {
            int value = NStr::StringToInt(str);
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
        }
        STD_CATCH("TestUtilities");

        try {
            unsigned int value = NStr::StringToUInt(str);
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::UIntToString(value) << "'" << NcbiEndl;
        }
        STD_CATCH("TestUtilities");

        try {
            long value = NStr::StringToLong(str);
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
        }
        STD_CATCH("TestUtilities");

        try {
            unsigned long value = NStr::StringToULong(str);
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::UIntToString(value) << "'" << NcbiEndl;
        }
        STD_CATCH("TestUtilities");

        try {
            double value = NStr::StringToDouble(str);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::DoubleToString(value) << "'" << NcbiEndl;
        }
        STD_CATCH("TestUtilities");
    }

    NcbiCout << NcbiEndl << "NStr::Replace() tests...";

    string src("aaabbbaaccczzcccXX");
    string dst;

    string search("ccc");
    string replace("RrR");
    NStr::Replace(src, search, replace, dst);
    _ASSERT(dst == "aaabbbaaRrRzzRrRXX");

    search = "a";
    replace = "W";
    NStr::Replace(src, search, replace, dst, 6, 1);
    _ASSERT(dst == "aaabbbWaccczzcccXX");
    
    search = "bbb";
    replace = "BBB";
    NStr::Replace(src, search, replace, dst, 50);
    _ASSERT(dst == "aaabbbaaccczzcccXX");

    search = "ggg";
    replace = "no";
    dst = NStr::Replace(src, search, replace);
    _ASSERT(dst == "aaabbbaaccczzcccXX");

    search = "a";
    replace = "A";
    dst = NStr::Replace(src, search, replace);
    _ASSERT(dst == "AAAbbbAAccczzcccXX");

    search = "X";
    replace = "x";
    dst = NStr::Replace(src, search, replace, src.size() - 1);
    _ASSERT(dst == "aaabbbaaccczzcccXx");

    NcbiCout << " completed successfully!" << NcbiEndl << NcbiEndl;

    NcbiCout << "TestUtilities finished" << NcbiEndl;
}

static void TestThrowTrace(void)
{
    SetDiagTrace(eDT_Enable);
    NcbiCerr << "The following lines should be equal pairs:" << NcbiEndl;
    try {
        THROW1_TRACE(runtime_error, "Message");
    }
    catch (...) {
        CNcbiDiag(__FILE__, __LINE__ - 3, eDiag_Trace, eDPF_Trace) <<
            "runtime_error: Message";
    }
    string mess = "ERROR";
    try {
        THROW1_TRACE(runtime_error, mess);
    }
    catch (...) {
        CNcbiDiag(__FILE__, __LINE__ - 3, eDiag_Trace, eDPF_Trace) <<
            "runtime_error: ERROR";
    }
    int i = 123;
    try {
        THROW0p_TRACE(i);
    }
    catch (...) {
        CNcbiDiag(__FILE__, __LINE__ - 3, eDiag_Trace, eDPF_Trace) <<
            "i: 123";
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
    TestException();
    TestException_AuxTrace();
    TestIostream();
    TestRegistry();
    TestUtilities();
    TestThrowTrace();
    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
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


int main(int argc, const char* argv[], const char* envp[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv, envp, eDS_ToMemory);
}
