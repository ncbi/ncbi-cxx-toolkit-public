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
 *   Test program for NCBI table
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/ncbi_table.hpp>
#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/// @internal
class CTestTable : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestTable::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_table",
                       "test NCBI table");
    SetupArgDescriptions(d.release());
}

// Run test

int CTestTable::Run(void)
{
    CNcbiTable<int, string, string> table;

    table.AddColumn("first");
    table.AddColumn("second");
    table.AddColumn("third");

    table.AddRow("r1");
    table.AddRow("r2");
    table.AddRow("r3");

    assert(table.Cols() == 3);
    assert(table.Rows() == 3);

    for (unsigned int i = 0; i < table.Rows(); ++i) {
        for (unsigned int j = 0; j < table.Cols(); ++j) {
            table.GetCell(i, j) = 101;
        } // for j
    } // for i

    table("r2", "third") = 202;

    for (unsigned int i = 0; i < table.Rows(); ++i) {
        for (unsigned int j = 0; j < table.Cols(); ++j) {
            int v = table.GetCell(i, j);
            assert(v == 101 || v == 202);
        } // for j
    } // for i

    table.Resize(table.Rows() + 1, table.Cols(), 303);
    assert(table.Rows() == 4);

    {{
    for (unsigned int i = 0; i < table.Rows(); ++i) {
        CNcbiTable<int, string, string>::TRowType r = 
            table.GetRowVector(i);

        for(unsigned j = 0; j < r.size(); ++j) {
            int v = r[j];
            if (i < table.Rows()-1) {
                assert(v == 101 || v == 202);
            } else {
                assert(v == 303);
            }
        }

    } // for i
    }}

    {{
    table.AssociateRow("r4", 3);
    CNcbiTable<int, string, string>::TRowType r = 
        table.GetRow("r4");
    for(unsigned j = 0; j < r.size(); ++j) {
        int v = r[j];
        assert(v == 303);
    }
    }}


    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestTable().AppMain(argc, argv);
}
