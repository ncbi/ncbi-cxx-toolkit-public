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
 * Author: Greg Boratyn
 *
 * File Description:
 *   Unit tests for CMultiAlnPrinter
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/scope.hpp>

#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objtools/align_format/aln_printer.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_format);


BOOST_AUTO_TEST_SUITE(aln_printer)


string PrintAlignment(CMultiAlnPrinter::EFormat format)
{
    blast::CBlastScopeSource scope_src(true);
    CRef<CScope> scope(scope_src.NewScope());

    CSeq_align seqalign;
    CNcbiIfstream istr("data/multialign.asn");
    istr >> MSerial_AsnText >> seqalign;

    CMultiAlnPrinter printer(seqalign, *scope);
    printer.SetWidth(80);
    printer.SetFormat(format);

    ostrstream output_stream;
    printer.Print(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);

    return output;
}

BOOST_AUTO_TEST_CASE(TestFastaPlusGaps)
{
    string output = PrintAlignment(CMultiAlnPrinter::eFastaPlusGaps);

    BOOST_REQUIRE(output.find(">gi|129295 RecName: Full=Ovalbumin-related pro"
                              "tein X; AltName: Full=Gene X protein") != NPOS);
    BOOST_REQUIRE(output.find("--------------------------------MPQWANPVPAIA--G"
                              "AAPVVITSARAAISAGVDEA---GALGTSAAVP") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("KTG------LLLAAGLIGDPLLAGE----") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestClustalW)
{
    string output = PrintAlignment(CMultiAlnPrinter::eClustal);

    BOOST_REQUIRE(output.find("gi|189500654                  M----------------"
                              "------------------------RII-IYNR---------------"
                              "----------------") != NPOS);

    BOOST_REQUIRE(output.find("gi|125972714                  VMHSYNILWGESSKQISE"
                              "GIS-EVVYRTELTGLEPNT---EYTYKI----YGQMPIRNKEGTPETI"
                              "TFKTLPKKLVYGEL") != NPOS);

    BOOST_REQUIRE(output.find("*") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestPhylipSequential)
{
    string output = PrintAlignment(CMultiAlnPrinter::ePhylipSequential);

    BOOST_REQUIRE(output.find("  100   749") != NPOS);

    BOOST_REQUIRE(output.find("RecName__ ---------------------------") != NPOS);
    BOOST_REQUIRE(output.find("-----------------------------------------------"
                              "----------QIKDLLVSSSTD-LDTTLVLVNA") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("ARFAFALRDTKTG------LLLAAGLIGDPLLAGE----")
                  != NPOS);
}


BOOST_AUTO_TEST_CASE(TestPhylipInterleaved)
{
    string output = PrintAlignment(CMultiAlnPrinter::ePhylipInterleaved);

    BOOST_REQUIRE(output.find("  100   749") != NPOS);

    BOOST_REQUIRE(output.find("RecName__ -------------------------------------"
                              "----------------------------------") != NPOS);


    BOOST_REQUIRE(output.find("secreted_ --------------------------------MPQWA"
                              "NPVPAIA--GAAPVVITSARAAISAGVDEA---G") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("DTKTG------LLLAAGLIGDPLLAGE----") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestNexus)
{
    string output = PrintAlignment(CMultiAlnPrinter::eNexus);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN DATA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=100 nchar=748;") != NPOS);
    BOOST_REQUIRE(output.find("MATRIX") != NPOS);


    BOOST_REQUIRE(output.find("129295     -----------------------------------"
                              "----------------------------------------------")
                  != NPOS);


    BOOST_REQUIRE(output.find("162452372  --------------------------------MPQW"
                              "ANPVPAIA--GAAPVVITSARAAISAGVDEA---GALGTSAAVPG")
                  != NPOS);

    // last line
    BOOST_REQUIRE(output.find("241667095  LLLAAGLIGDPLLAGE----") != NPOS);
    BOOST_REQUIRE(output.find("END;") != NPOS);
}



BOOST_AUTO_TEST_SUITE_END()



