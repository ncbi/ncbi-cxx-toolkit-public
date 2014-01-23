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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Unit tests for method CSeq_entry::ReassignConflictingLocalIds().
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/iterator.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define TEST_DIR "test_cases/ReassignIds/"

static void sx_RunTest(const string& file_name)
{
    string asn_name = TEST_DIR+file_name+".asn";
    string ref_name = TEST_DIR+file_name+".ref";
    string out_name = TEST_DIR+file_name+".out";
    CSeq_entry entry, ref_entry;
    {
        ifstream in(asn_name.c_str());
        in >> MSerial_AsnText >> entry;
    }
    entry.ReassignConflictingIds();
    {
        ifstream in(ref_name.c_str());
        in >> MSerial_AsnText >> ref_entry;
    }
    if ( !entry.Equals(ref_entry) ) {
        // update delayed parsing buffers for correct ASN.1 text indentation.
        for ( CStdTypeConstIterator<string> it(ConstBegin(entry)); it; ++it ) {
        }
        ofstream out(out_name.c_str());
        out << MSerial_AsnText << entry;
        system(("diff "+out_name+" "+ref_name).c_str());
        BOOST_CHECK(entry.Equals(ref_entry));
    }
}


BOOST_AUTO_TEST_CASE(TestExample1)
{
    sx_RunTest("example1");
}


BOOST_AUTO_TEST_CASE(TestExample2)
{
    sx_RunTest("example2");
}


BOOST_AUTO_TEST_CASE(TestExample3)
{
    sx_RunTest("example3");
}
