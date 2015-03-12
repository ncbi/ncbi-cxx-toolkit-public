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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    Stack-trace test
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_stack.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CTestStackTrace : public CNcbiApplication
{
public:

    virtual void Init(void);
    virtual int  Run (void);

    virtual void Func1();
    virtual void Func2(int i);
    virtual void Func3(double d);
    virtual void Func4(const string& str);
    virtual void Func5(const char* char_ptr);
};

void CTestStackTrace::Func1()
{
    Func2(9);
}

void CTestStackTrace::Func2(int i)
{
    Func3(i/2.0);
}

void CTestStackTrace::Func3(double d)
{
    Func4(NStr::DoubleToString(d));
}

void CTestStackTrace::Func4(const string& str)
{
    Func5(str.c_str());
}

void CTestStackTrace::Func5(const char* char_ptr)
{
    CStackTrace trace("\t");

    cout << "Func5(" << char_ptr << ") stacktrace:" << endl << trace;

    ERR_POST(Warning
        << "Func5(" << char_ptr << ") stacktrace:\n"
        << trace);
}

void CTestStackTrace::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    SetupArgDescriptions(arg_desc.release());
}


int CTestStackTrace::Run(void)
{
    Func1();
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTestStackTrace().AppMain(argc, argv);
}
