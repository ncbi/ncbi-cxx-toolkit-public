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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *   Test/demo for query parser
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/qparse/query_parse.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



/// @internal
class CTestQParse : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestQParse::Init(void)
{

    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_qparse",
                       "test query parse");
    SetupArgDescriptions(d.release());
}


int CTestQParse::Run(void)
{
    {{
    const char* q = "1!=2";
    CQueryParseTree qtree;
    qtree.Parse(q);
    NcbiCout << "---------------------------------------------------" << endl;
    qtree.Print(NcbiCout);
    NcbiCout << "---------------------------------------------------" << endl;
    }}
    return 0;
}


int main(int argc, const char* argv[])
{
    CTestQParse app; 
    return app.AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2007/01/11 14:49:52  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.1  2007/01/10 17:06:31  kuznets
 * initial revision
 *
 * ===========================================================================
 */
