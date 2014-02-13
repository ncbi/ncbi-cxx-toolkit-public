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
*   Sample unit tests file for class CObjectsSniffer.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <serial/objistr.hpp>
#include <objmgr/util/obj_sniff.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(Test_Text_Object1)
{
    string file_name = "test_data/object1.asn";
    ifstream file_in(file_name.c_str());
    BOOST_REQUIRE(file_in);
    AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, file_in));
    BOOST_REQUIRE(!in->EndOfData());
    BOOST_REQUIRE_EQUAL(in->GetStreamPos(), NcbiInt8ToStreampos(0));
    objects::CObjectsSniffer sniff;
    sniff.AddCandidate(CObject_id::GetTypeInfo());
    sniff.AddCandidate(CSeq_id::GetTypeInfo());
    sniff.AddCandidate(CBioseq::GetTypeInfo());
    sniff.AddCandidate(CSeq_entry::GetTypeInfo());
    sniff.Probe(*in);
    NcbiCout << "Found " << sniff.GetTopLevelMap().size()
             << " object(s) in " << file_name << NcbiEndl;
    BOOST_REQUIRE(in->EndOfData());
    BOOST_REQUIRE(in->GetStreamPos() != NcbiInt8ToStreampos(0));
}


BOOST_AUTO_TEST_CASE(Test_Bin_Object1)
{
    string file_name = "test_data/object1.asb";
    ifstream file_in(file_name.c_str(), IOS_BASE::binary);
    BOOST_REQUIRE(file_in);
    AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnBinary, file_in));
    BOOST_REQUIRE(!in->EndOfData());
    BOOST_REQUIRE_EQUAL(in->GetStreamPos(), NcbiInt8ToStreampos(0));
    objects::CObjectsSniffer sniff;
    sniff.AddCandidate(CObject_id::GetTypeInfo());
    sniff.AddCandidate(CSeq_id::GetTypeInfo());
    sniff.AddCandidate(CBioseq::GetTypeInfo());
    sniff.AddCandidate(CSeq_entry::GetTypeInfo());
    sniff.Probe(*in);
    NcbiCout << "Found " << sniff.GetTopLevelMap().size()
             << " object(s) in " << file_name << NcbiEndl;
    BOOST_REQUIRE(in->EndOfData());
    BOOST_REQUIRE(in->GetStreamPos() != NcbiInt8ToStreampos(0));
}


BOOST_AUTO_TEST_CASE(Test_PipeText_Object1)
{
    string file_name = "test_data/object1.asn";
    ifstream file_in(file_name.c_str());
    BOOST_REQUIRE(file_in);
    AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, file_in));
    BOOST_REQUIRE(!in->EndOfData());
    BOOST_REQUIRE_EQUAL(in->GetStreamPos(), NcbiInt8ToStreampos(0));
    objects::CObjectsSniffer sniff;
    sniff.AddCandidate(CObject_id::GetTypeInfo());
    sniff.AddCandidate(CSeq_id::GetTypeInfo());
    sniff.AddCandidate(CBioseq::GetTypeInfo());
    sniff.AddCandidate(CSeq_entry::GetTypeInfo());
    sniff.Probe(*in);
    NcbiCout << "Found " << sniff.GetTopLevelMap().size()
             << " object(s) in " << file_name << NcbiEndl;
    BOOST_REQUIRE(in->EndOfData());
    BOOST_REQUIRE(in->GetStreamPos() != NcbiInt8ToStreampos(0));
}
