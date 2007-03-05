/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *   Test program for test UTF-8 conversion functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/utf8.hpp>
#include <util/sgml_entity.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CTestUtf8 : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
    void TestUtf8(void);
    void TestSgml(void);
};


void CTestUtf8::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_utf8",
                       "test UTF-8 converter");
    SetupArgDescriptions(d.release());
}

// Run test

int CTestUtf8::Run(void)
{
    TestUtf8();
    TestSgml();
    NcbiCout << NcbiEndl << NcbiEndl;
    return 0;
}

void CTestUtf8::TestUtf8(void)
{
    const char* sTest[]={
          "Archiv für Gynäkologie",
          "Phillip Journal für restaurative Zahnmedizin.",
          "Revista odontológica.",
          "Veterinární medicína.",
          "Zhōnghuá zhŏngliú zázhì",
          "Biokhimii\357\270\240a\357\270\241"
    };
    const size_t MAX_TEST_NUM = 6;
    string sRes;
    string s;
    vector<long> v;
    utf8::EConversionStatus stat;
    char ch;
    size_t len;
    

    // Start passes

    //-----------------------------------
    NcbiCout << "\nUTF -> Ascii\n" << NcbiEndl;

    for (size_t i=0; i<MAX_TEST_NUM; i++)
    {
        sRes=utf8::StringToAscii(sTest[i]);
        NcbiCout << sTest[i] << "\t -> " << sRes << NcbiEndl;
    }

    //-----------------------------------
    NcbiCout << "\nUTF -> Chars\n" << NcbiEndl;

    for (size_t i=0; i<MAX_TEST_NUM; i++)
    {
        s=sTest[i]; 
        NcbiCout << s << "\n" << NcbiEndl;

        for (size_t j=0; j<s.size(); )
        {
            ch=utf8::StringToChar(s.data()+j, &len, false, &stat);
            NcbiCout << s[j] << " (len = "<<len<<")\t - result "
                     << ch << ", status " << stat << NcbiEndl;
            j+=len;
        }
        NcbiCout << "----" << NcbiEndl;
    }

    //-----------------------------------
    NcbiCout << "\nUTF -> Vector (last string)\n" << NcbiEndl;

    v=utf8::StringToVector(sTest[4]);    
    for (size_t i=0; i<v.size(); i++)
    {
        NcbiCout << v[i] << ' ';
    }

    //-----------------------------------
}


void CTestUtf8::TestSgml(void)
{
    const size_t MAX_TEST_NUM = 4;
    const char* sTest[MAX_TEST_NUM]={
        "&Agr;&Bgr;&Dgr;&EEgr;&Egr;&Ggr;&Igr;&KHgr;&Kgr;&zgr;",
        ";&Agr;;&Bgr;;&Dgr;;&EEgr;;&Egr;;&Ggr;;&Igr;;&KHgr;;&Kgr;;&zgr;;",
        ";&Agr;&&Bgr;&&Dgr;&&EEgr;&&Egr;&&Ggr;&&Igr;&&KHgr;&&Kgr;&&zgr;&",
        ";&Agr;&;&Bgr;&;&Dgr;&;&EEgr;&;&Egr;&;&Ggr;&;&Igr;&;&KHgr;&;&Kgr;&;&zgr;&;"
    };
    const char* sResult[MAX_TEST_NUM]={
        "<Alpha><Beta><Delta><Eta><Epsilon><Gamma><Iota><Chi><Kappa><zeta>",
        ";<Alpha>;<Beta>;<Delta>;<Eta>;<Epsilon>;<Gamma>;<Iota>;<Chi>;<Kappa>;<zeta>;",
        ";<Alpha>&<Beta>&<Delta>&<Eta>&<Epsilon>&<Gamma>&<Iota>&<Chi>&<Kappa>&<zeta>&",
        ";<Alpha>&;<Beta>&;<Delta>&;<Eta>&;<Epsilon>&;<Gamma>&;<Iota>&;<Chi>&;<Kappa>&;<zeta>&;"
    };

    //-----------------------------------
    NcbiCout << "\nSGML -> Ascii\n" << NcbiEndl;

    for (size_t i=0; i<MAX_TEST_NUM; i++)
    {
        string sRes = Sgml2Ascii(sTest[i]);
        NcbiCout << sTest[i] << " -> " << sRes << NcbiEndl;
        _ASSERT(sRes == sResult[i]);
    }
}


int main(int argc, const char* argv[])
{
    return CTestUtf8().AppMain(argc, argv);
}
