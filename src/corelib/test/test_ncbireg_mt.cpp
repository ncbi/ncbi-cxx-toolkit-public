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
 *   Test for "NCBIREG" in multithreaded environment
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/04/06 15:57:09  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include "test_mt.hpp"
#include <corelib/ncbireg.hpp>
#include <algorithm>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestRegApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    CNcbiRegistry m_Registry;
};


bool CTestRegApp::Thread_Run(int /*idx*/)
{
    list<string> sections;
    list<string> entries;

    // Compose a test registry
    s_Verify( m_Registry.Set("Section0", "Name01", "Val01_BAD!!!") );
    s_Verify( m_Registry.Set("Section1 ", "\nName11", "Val11_t") );
    s_Verify(!m_Registry.Empty() );
    s_Verify( m_Registry.Get(" Section1", "Name11\t") == "Val11_t" );
    s_Verify( m_Registry.Get("Section1", "Name11",
                             CNcbiRegistry::ePersistent).empty() );
    s_Verify( m_Registry.Set("Section1", "Name11", "Val11_t") );
    s_Verify(!m_Registry.Set("Section1", "Name11", "Val11_BAD!!!",
                             CNcbiRegistry::eNoOverride) );

    s_Verify( m_Registry.Set("   Section2", "\nName21  ", "Val21",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("Section2", "Name21", "Val21_t") );
    s_Verify(!m_Registry.Empty() );
    s_Verify( m_Registry.Set("Section3", "Name31", "Val31_t") );

    s_Verify( m_Registry.Get(" \nSection1", " Name11  ") == "Val11_t" );
    s_Verify( m_Registry.Get("Section2", "Name21",
                             CNcbiRegistry::ePersistent) == "Val21" );
    s_Verify( m_Registry.Get(" Section2", " Name21\n") == "Val21_t" );
    s_Verify( m_Registry.Get("SectionX", "Name21").empty() );

    s_Verify( m_Registry.Set("Section4", "Name41", "Val410 Val411 Val413",
                             CNcbiRegistry::ePersistent) );
    s_Verify(!m_Registry.Set("Sect ion4", "Name41", "BAD1",
                             CNcbiRegistry::ePersistent) );
    s_Verify(!m_Registry.Set("Section4", "Na me41", "BAD2") );
    s_Verify( m_Registry.Set("SECTION4", "Name42", "V420 V421\nV422 V423 \"",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("Section4", "NAME43",
                             " \tV430 V431  \n V432 V433 ",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("\tSection4", "Name43T",
                             " \tV430 V431  \n V432 V433 ",
                             CNcbiRegistry::ePersistent |
                             CNcbiRegistry::eTruncate) );
    s_Verify( m_Registry.Set("Section4", "Name44", "\n V440 V441 \r\n",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("\r Section4", "  \t\rName45",
                             "\r\n V450 V451  \n  ",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("Section4 \n", "  Name46  ",
                             "\n\nV460\" \n \t \n\t",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set(" Section4", "Name46T", "\n\nV460\" \n \t \n\t",
                             CNcbiRegistry::ePersistent |
                             CNcbiRegistry::eTruncate) );
    s_Verify( m_Registry.Set("Section4", "Name47",
                             "470\n471\\\n 472\\\n473\\",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("Section4", "Name47T",
                             "470\n471\\\n 472\\\n473\\",
                             CNcbiRegistry::ePersistent |
                             CNcbiRegistry::eTruncate) );
    string xxx("\" V481\" \n\"V482 ");
    s_Verify( m_Registry.Set("Section4", "Name48", xxx,
                             CNcbiRegistry::ePersistent) );

    s_Verify( m_Registry.Set("Section5", "Name51", "Section5/Name51",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("_Section_5", "Name51", "_Section_5/Name51",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("_Section_5_", "_Name52", "_Section_5_/_Name52",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("_Section_5_", "Name52", "_Section_5_/Name52",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("_Section_5_", "_Name53_", "_Section_5_/_Name53_",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("Section-5.6", "Name-5.6", "Section-5.6/Name-5.6",
                             CNcbiRegistry::ePersistent) );
    s_Verify( m_Registry.Set("-Section_5", ".Name.5-3", "-Section_5/.Name.5-3",
                             CNcbiRegistry::ePersistent) );


    // Dump
    CNcbiOstrstream os;
    s_Verify ( m_Registry.Write(os) );
    os << '\0';
    const char* os_str = os.str();  os.rdbuf()->freeze(false);

    // "Persistent" load
    CNcbiIstrstream is1(os_str);
    CNcbiRegistry  reg1(is1);
    s_Verify(  reg1.Get("Section2", "Name21", CNcbiRegistry::ePersistent) ==
               "Val21" );
    s_Verify(  reg1.Get("Section2", "Name21") == "Val21" );
    s_Verify( !reg1.Set("Section2", "Name21", NcbiEmptyString) );
    s_Verify( !reg1.Set("Section2", "Name21", NcbiEmptyString,
                        CNcbiRegistry::ePersistent |
                        CNcbiRegistry::eNoOverride) );
    s_Verify( !reg1.Empty() );
    s_Verify(  reg1.Set("Section2", "Name21", NcbiEmptyString,
                        CNcbiRegistry::ePersistent) );

    // "Transient" load
    CNcbiIstrstream is2(os_str);
    CNcbiRegistry  reg2(is2, CNcbiRegistry::eTransient);
    s_Verify(  reg2.Get("Section2", "Name21",
                        CNcbiRegistry::ePersistent).empty() );
    s_Verify(  reg2.Get("Section2", "Name21") == "Val21" );
    s_Verify( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                        CNcbiRegistry::ePersistent) );
    s_Verify( !reg2.Set("Section2", "Name21", NcbiEmptyString,
                        CNcbiRegistry::ePersistent |
                        CNcbiRegistry::eNoOverride) );
    s_Verify( !reg2.Empty() );
    s_Verify(  reg2.Set("Section2", "Name21", NcbiEmptyString) );


    s_Verify( m_Registry.Get("Sect ion4 ", "Name41 ").empty() );
    s_Verify( m_Registry.Get("Section4 ", "Na me41 ").empty() );

    s_Verify( m_Registry.Get("Section4 ", "Name41 ") ==
              "Val410 Val411 Val413" );
    s_Verify( m_Registry.Get("Section4",  " Name42") ==
              "V420 V421\nV422 V423 \"" );
    s_Verify( m_Registry.Get("Section4",  "Name43")  ==
              " \tV430 V431  \n V432 V433 ");
    s_Verify( m_Registry.Get("Section4",  "Name43T") ==
              "V430 V431  \n V432 V433" );
    s_Verify( m_Registry.Get("Section4",  " Name44") ==
              "\n V440 V441 \r\n" );
    s_Verify( m_Registry.Get(" SecTIon4", "Name45")  ==
              "\r\n V450 V451  \n  " );
    s_Verify( m_Registry.Get("SecTion4 ", "Name46")  ==
              "\n\nV460\" \n \t \n\t" );
    s_Verify( m_Registry.Get("Section4",  "NaMe46T") ==
              "\n\nV460\" \n \t \n" );
    s_Verify( m_Registry.Get(" Section4", "Name47")  ==
              "470\n471\\\n 472\\\n473\\" );
    s_Verify( m_Registry.Get("Section4 ", "NAme47T") ==
              "470\n471\\\n 472\\\n473\\" );
    s_Verify( m_Registry.Get("Section4",  "Name48")  == xxx );

    s_Verify( reg2.Get("Section4", "Name41")  == "Val410 Val411 Val413" );
    s_Verify( reg2.Get("Section4", "Name42")  == "V420 V421\nV422 V423 \"" );
    s_Verify( reg2.Get("Section4", "Name43")  ==
              " \tV430 V431  \n V432 V433 ");
    s_Verify( reg2.Get("Section4", "Name43T") == "V430 V431  \n V432 V433" );
    s_Verify( reg2.Get("Section4", "Name44")  == "\n V440 V441 \r\n" );
    s_Verify( reg2.Get("Section4", "NaMe45")  == "\r\n V450 V451  \n  " );
    s_Verify( reg2.Get("SecTIOn4", "NAme46")  == "\n\nV460\" \n \t \n\t" );
    s_Verify( reg2.Get("Section4", "Name46T") == "\n\nV460\" \n \t \n" );
    s_Verify( reg2.Get("Section4", "Name47")  == "470\n471\\\n 472\\\n473\\" );
    s_Verify( reg2.Get("Section4", "Name47T") == "470\n471\\\n 472\\\n473\\" );
    s_Verify( reg2.Get("Section4", "Name48")  == xxx );

    s_Verify( reg2.Get(" Section5",    "Name51 ")   == "Section5/Name51" );
    s_Verify( reg2.Get("_Section_5",   " Name51")   == "_Section_5/Name51" );
    s_Verify( reg2.Get(" _Section_5_", " _Name52")  == "_Section_5_/_Name52");
    s_Verify( reg2.Get("_Section_5_ ", "Name52")    == "_Section_5_/Name52");
    s_Verify( reg2.Get("_Section_5_",  "_Name53_ ") ==
              "_Section_5_/_Name53_" );
    s_Verify( reg2.Get(" Section-5.6", "Name-5.6 ") == "Section-5.6/Name-5.6");
    s_Verify( reg2.Get("-Section_5",   ".Name.5-3") == "-Section_5/.Name.5-3");

    m_Registry.EnumerateSections(&sections);
    s_Verify( find(sections.begin(), sections.end(), "Section1")
             != sections.end() );
    s_Verify( !sections.empty() );

    return true;
}

bool CTestRegApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing NCBIREG with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    s_Verify( m_Registry.Empty() );

    list<string> sections;
    m_Registry.EnumerateSections(&sections);
    s_Verify( sections.empty() );

    list<string> entries;
    m_Registry.EnumerateEntries(NcbiEmptyString, &entries);
    s_Verify( entries.empty() );

    return true;
}

bool CTestRegApp::TestApp_Exit(void)
{
    m_Registry.Clear();
    s_Verify( m_Registry.Empty() );

    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestRegApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}
