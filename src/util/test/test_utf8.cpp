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
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CTestUtf8 : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
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
    printf("\nUTF -> Ascii\n\n");

    for (size_t i=0; i<MAX_TEST_NUM; i++)
    {
        sRes=utf8::StringToAscii(sTest[i]);
        sRes+='\0';
        printf("%s\t -> %s\n",sTest[i],sRes.data());
    }

    //-----------------------------------
    printf("\nUTF -> Chars\n\n");

    for (size_t i=0; i<MAX_TEST_NUM; i++)
    {
        s=sTest[i]; 
        printf("%s\n\n",s.data());

        for (size_t j=0; j<s.size(); )
        {
            ch=utf8::StringToChar(s.data()+j, &len, false, &stat);
            printf("%c (len = %i)\t - result %c, status %i\n",\
                   s.data()[j],len,ch,stat);
            j+=len;
        }
        printf("----\n");
    }

    //-----------------------------------
    printf("\nUTF -> Vector (last string)\n\n");

    v=utf8::StringToVector(sTest[4]);    
    for (size_t i=0; i<v.size(); i++)
    {
        printf("%lu ",v[i]);
    }

    //-----------------------------------

    printf("\n\n");
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestUtf8().AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2002/06/30 03:24:08  vakatov
 * Get rid of warnings caused by constless char* initialization
 *
 * Revision 1.4  2002/04/16 18:52:16  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.3  2002/01/18 19:25:26  ivanov
 * Polish source code. Appended one more test string.
 *
 * Revision 1.2  2001/04/18 16:32:24  ivanov
 * Change types TUnicodeChar, TUnicodeString to simple types.
 * TUnicode char to long, TUnicodeString to vector<long>.
 *
 * Revision 1.1  2001/04/06 19:16:04  ivanov
 * Initial revision
 * ===========================================================================
 */
