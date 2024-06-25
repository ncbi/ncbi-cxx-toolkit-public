/* $Id$
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
 * Author: Anton Lavrentiev
 *
 * File Description:
 *   Test program for CMemory_Streambuf
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/memory_streambuf.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/// @internal
class CMemory_StreambufTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CMemory_StreambufTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    unique_ptr<CArgDescriptions> desc(new CArgDescriptions);

    desc->SetUsageContext("test_memory_streambuf",
                          "test_memory_streambuf");
    SetupArgDescriptions(desc.release());
}


int CMemory_StreambufTest::Run(void)
{
 //                           /*           1         2         3         4    */
 //                           /* 01234567890123456789012345678901234567890123 */
    static const char kTest[] = "The quick brown fox jumps over the lazy dog.";
    static const CT_POS_TYPE err = (CT_POS_TYPE)((CT_OFF_TYPE)(-1L));
    char test[sizeof(kTest)];

    memcpy(test, kTest, sizeof(test));
    CMemory_Streambuf rosb(kTest, sizeof(kTest) - 1);
    CMemory_Streambuf rwsb(test, sizeof(test) - 1);
    iostream ios(&rosb);
    string s;
    char c = '\0';
    ios >> s;
    assert(s == "The");
    ios >> c;
    assert(c == 'q');
    ios << "Bad";
    assert(!ios);
    ios.clear();
    CT_POS_TYPE pos = ios.tellg();
    assert(ios);
    assert(pos - (CT_POS_TYPE)((CT_OFF_TYPE) 0) == 5);
    pos = ios.tellp();
    assert(pos == err);
    assert(!ios.seekp(0, IOS_BASE::end));
    ios.clear();
    ios.seekg((CT_POS_TYPE)((CT_OFF_TYPE) 25));
    ios >> s;
    assert(s == "over");
    assert(rosb.pubseekoff(4, IOS_BASE::end, IOS_BASE::in) == err);
    rosb.pubseekoff(-4, IOS_BASE::end, IOS_BASE::in);
    ios >> s;
    assert(s == "dog.");
    ios >> c;
    assert(ios.fail()  &&  ios.eof());
    ios.clear();
    ios.rdbuf(&rwsb);
    ios >> s;
    assert(s == "The");
    pos = ios.tellp();
    assert(pos == (CT_POS_TYPE)((CT_OFF_TYPE) 0));
    assert(ios.seekp(10, IOS_BASE::cur));
    ios << "nasty";
    assert(memcmp(test + 9, " nasty ", 7) == 0);
    assert(ios.seekp(-9, IOS_BASE::end));
    ios << "tyrannosaurus";
    assert(ios.fail());
    assert(memcmp(&(test + sizeof(test))[-11], " tyrannosa\0", 11) == 0);
    ios.clear();
    assert(strlen(test) == ios.tellp() - (CT_POS_TYPE)((CT_OFF_TYPE) 0));
    cout << test;
    return 0;
}


int main(int argc, const char* argv[])
{
    CMemory_StreambufTest app; 
    return app.AppMain(argc, argv);
}
