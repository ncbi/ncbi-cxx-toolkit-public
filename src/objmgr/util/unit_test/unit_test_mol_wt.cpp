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
* Author:  Pavel Ivanov, NCBI
*
* File Description:
*   Sample unit tests file for main stream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework based
* on Boost.Test framework. For more advanced techniques look into another
* sample - unit_test_alt_sample.cpp.
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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/weight.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const string sc_TestBioseq_1;

BOOST_AUTO_TEST_CASE(Test_RawString)
{
    string t1("ABCDEFGHIKLMNPQRSTVWYZUOJ");
    double wt = GetProteinWeight(t1);
    cerr << "weight = " << wt << endl;
    BOOST_CHECK_CLOSE(wt, 3140.46, 0.001);

    string t2("MABCDEFGHIKLMNPQRSTVWYZUOJ");
    wt = GetProteinWeight(t2);
    cerr << "weight = " << wt << endl;
    BOOST_CHECK_CLOSE(wt, 3271.65, 0.001);

}

BOOST_AUTO_TEST_CASE(Test_Bioseq)
{
    CNcbiIstrstream is(sc_TestBioseq_1);
    CRef<CBioseq> bs(new CBioseq);
    is >> MSerial_AsnText >> *bs;

    CScope scope(*CObjectManager::GetInstance());
    CBioseq_Handle bsh = scope.AddBioseq(*bs);
    double wt = GetProteinWeight(bsh);
    //BOOST_CHECK_CLOSE(wt, 66753.98925, 0.001);
    BOOST_CHECK_CLOSE(wt, 66622.7923, 0.001);

    ///
    /// check after setting completeness
    ///
    CRef<CSeqdesc> molinfo(new CSeqdesc());
    molinfo->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
    bs->SetDescr().Set().push_back(molinfo);

    wt = GetProteinWeight(bsh);
    cerr << "weight = " << wt << endl;
    BOOST_CHECK_CLOSE(wt, 66622.7923, 0.001);

}

const string sc_TestBioseq_1 = "\
Bioseq ::= {\
    id {\
        local str \"1\"\
    },\
    inst {\
        repr raw,\
        mol aa,\
        length 655,\
        seq-data ncbieaa \"MAGVAELDGGAPRPRSPSGGPALQAERRDEPEAAGPKAQREEAREGPAEAPG\
GEGAGAAATAAGPEGEGPGPAGGRAGGLEPGSGGGPGAETRGGSGAQGQAEAGEDAREGEGPTSGAEPEEEASPGPGA\
QGEEPGEVQAGPRDPAALNAKEEAGNGPGEPAGSAPGASGARVDAPESVGESVVAEGSMGALVDAPESVGNSVDSEGS\
VGARVDAEGPAGESVGAEESMADSTDVGGPTGASTDVGVPTAGAQGSEGESRDGGDSKSQSQAEAIEAPAPESAECTP\
GELARGPRGSAEETGDPEGEEAGEAAPGDARAEASEEADQDGPSGQEEEAEEERPEQGLQGPGEEATAGGEEESPDGT\
AAGEAIQPAEVGEPQADLSNHLADERSAERGGGIVPENGGQEGGEASEEGAPGQDHDITLFVKAGCDGESIGNCPFSQ\
RLFMILWLKGVIFNVTTVDLKRKPADLQNLAPGTNPPFMTFDGEVKTDVNKIEEFLEEKLAPPRYPKLGTQHPESNSA\
GNDVFAKFSAFIKNTKKDANEIYERNLLKALKKLDSYLNSPLPDEIDAYSTEEAAISGRKFLDGNELTLADCNLLPKL\
HIIKIVAKRYRDFEFPSEMTGIWRYLNNAYARDEFTNTCPADQEIEHAYSDVAKRMK\"\
    }\
}\
";
