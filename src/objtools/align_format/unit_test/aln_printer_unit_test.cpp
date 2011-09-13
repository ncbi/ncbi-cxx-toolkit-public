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
#include <serial/iterator.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/scope.hpp>

#include <objtools/align_format/aln_printer.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <corelib/test_boost.hpp>
#include "blast_test_util.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_format);
using namespace TestUtil;


// Read sequences from fasta file and add them to scope
static CRef<CScope> CreateScope(const string& filename);


BOOST_AUTO_TEST_SUITE(aln_printer)

// input file names
const string protein_seqalign = "data/multialign.asn";
const string nucleotide_seqalign = "data/multialign_nucleotide.asn";
const string nucleotide_seqs = "data/nucleotide.fa";

string PrintAlignment(CMultiAlnPrinter::EFormat format,
                      const string& seqalign_file,
                      const string& fasta_file = "",
                      CMultiAlnPrinter::EAlignType type
                      = CMultiAlnPrinter::eNotSet)
{
    CRef<CScope> scope = CreateScope(fasta_file);
    
    CSeq_align seqalign;
    CNcbiIfstream istr(seqalign_file.c_str());
    istr >> MSerial_AsnText >> seqalign;

    CMultiAlnPrinter printer(seqalign, *scope, type);
    printer.SetWidth(80);
    printer.SetFormat(format);

    ostrstream output_stream;
    printer.Print(output_stream);
    string output = CNcbiOstrstreamToString(output_stream);

    return output;
}

BOOST_AUTO_TEST_CASE(TestFastaPlusGaps)
{
    // Test protein
    string output = PrintAlignment(CMultiAlnPrinter::eFastaPlusGaps,
                                   protein_seqalign);

    BOOST_REQUIRE(output.find(">gi|129295 RecName: Full=Ovalbumin-related pro"
                              "tein X; AltName: Full=Gene X protein") != NPOS);
    BOOST_REQUIRE(output.find("--------------------------------MPQWANPVPAIA--G"
                              "AAPVVITSARAAISAGVDEA---GALGTSAAVP") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("KTG------LLLAAGLIGDPLLAGE----") != NPOS);


    // Test nucleotide
    output = PrintAlignment(CMultiAlnPrinter::eFastaPlusGaps,
                            nucleotide_seqalign, nucleotide_seqs);

    BOOST_REQUIRE(output.find(">lcl|1 gi|405832|gb|U00001.1|HSCDC27 Human "
                              "homologue of S. pombe nuc2+ and A. nidulans "
                              "bimA") != NPOS);

    BOOST_REQUIRE(output.find("CACTAATACACCTCCTGTAATTGATGTGCCATCCACCGGAGCCCCT"
                              "TC-------AA-A-------A-------------") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("GATGAATTTTAACTTCTGGAAATCAGACTTTTACAACTGGATGTGT"
                              "GACTAGTGCTGACATGTTTCT-------") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestClustalW)
{
    // Test protein
    string output = PrintAlignment(CMultiAlnPrinter::eClustal,
                                   protein_seqalign);

    BOOST_REQUIRE(output.find("gi|189500654                  M----------------"
                              "------------------------RII-IYNR---------------"
                              "----------------") != NPOS);

    BOOST_REQUIRE(output.find("gi|125972714                  VMHSYNILWGESSKQISE"
                              "GIS-EVVYRTELTGLEPNT---EYTYKI----YGQMPIRNKEGTPETI"
                              "TFKTLPKKLVYGEL") != NPOS);

    BOOST_REQUIRE(output.find("*") != NPOS);


    // Test nucleotide
    output = PrintAlignment(CMultiAlnPrinter::eClustal,
                            nucleotide_seqalign, nucleotide_seqs);

    BOOST_REQUIRE(output.find("lcl|2                         ------CCGCTACAGG"
                              "GGGGGCCTGAGGCACTGCAGAAAGTGGGCCTGAGCCTCGAGGATGA"
                              "CGGTG") != NPOS);

    BOOST_REQUIRE(output.find("                                              "
                              "                                      ********"
                              "*****") != NPOS);

    // in last alignment line
    BOOST_REQUIRE(output.find("lcl|10                        GATGAATTTTAACTTC"
                              "TGGAAATCAGACTTTTACAACTGGATGTG") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestPhylipSequential)
{
    // Test protein
    string output = PrintAlignment(CMultiAlnPrinter::ePhylipSequential,
                                   protein_seqalign);

    BOOST_REQUIRE(output.find("  100   749") != NPOS);

    BOOST_REQUIRE(output.find("RecName__ ---------------------------") != NPOS);
    BOOST_REQUIRE(output.find("-----------------------------------------------"
                              "----------QIKDLLVSSSTD-LDTTLVLVNA") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("ARFAFALRDTKTG------LLLAAGLIGDPLLAGE----")
                  != NPOS);


    // Test nucleotide
    output = PrintAlignment(CMultiAlnPrinter::ePhylipSequential,
                            nucleotide_seqalign, nucleotide_seqs);

    BOOST_REQUIRE(output.find("  10   2634") != NPOS);

    BOOST_REQUIRE(output.find("gi_167466 ------CCGCTACAGGGGGGGCCTGAGGCACTGCAG"
                              "AAAGTGGGCCTGAGCCTCGAGGATGACGGTGCTG") != NPOS);

    // one before last line
    BOOST_REQUIRE(output.find("AGCTGAAAGTGATGAATTTTAACTTCTGGAAATCAGACTTTTACAA"
                              "CTGGATGTGTGACTAGTGCTGACATGTTTCT---") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestPhylipInterleaved)
{
    // Test protein
    string output = PrintAlignment(CMultiAlnPrinter::ePhylipInterleaved,
                                   protein_seqalign);

    BOOST_REQUIRE(output.find("  100   749") != NPOS);

    BOOST_REQUIRE(output.find("RecName__ -------------------------------------"
                              "----------------------------------") != NPOS);


    BOOST_REQUIRE(output.find("serine__o --------------------------------MPQWA"
                              "NPVPAIA--GAAPVVITSARAAISAGVDEA---G") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("DTKTG------LLLAAGLIGDPLLAGE----") != NPOS);


    // Test nucleotide
    output = PrintAlignment(CMultiAlnPrinter::ePhylipInterleaved,
                            nucleotide_seqalign, nucleotide_seqs);

    BOOST_REQUIRE(output.find("  10   2634") != NPOS);

    BOOST_REQUIRE(output.find("gi_167466 ------CCGCTACAGGGGGGGCCTGAGGCACTGCAG"
                              "AAAGTGGGCCTGAGCCTCGAGGATGACGGTGCTGC") != NPOS);

    // last line
    BOOST_REQUIRE(output.find("ATCAGACTTTTACAACTGGATGTGTGACTAGTGCTGACATGTTTCT"
                              "-------") != NPOS);
}


BOOST_AUTO_TEST_CASE(TestNexus)
{
    // Test protein
    string output = PrintAlignment(CMultiAlnPrinter::eNexus,
                                   protein_seqalign, "",
                                   CMultiAlnPrinter::eProtein);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN DATA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=100 nchar=749;") != NPOS);
    BOOST_REQUIRE(output.find("FORMAT datatype=protein gap=- interleave;")
                  != NPOS);
    BOOST_REQUIRE(output.find("MATRIX") != NPOS);


    BOOST_REQUIRE(output.find("129295     -----------------------------------"
                              "----------------------------------------------")
                  != NPOS);


    BOOST_REQUIRE(output.find("162452372  --------------------------------MPQW"
                              "ANPVPAIA--GAAPVVITSARAAISAGVDEA---GALGTSAAVPG")
                  != NPOS);

    // last alignment line
    // verify that a ';' follows the alignment
    BOOST_REQUIRE(output.find("241667095  LLLAAGLIGDPLLAGE----\n\n;") != NPOS
                  || 
                  // for Windows end of line
                  output.find("241667095  LLLAAGLIGDPLLAGE----\r\n\r\n;")
                  != NPOS);
    BOOST_REQUIRE(output.find("END;") != NPOS);


    // Test nucleotide
    output = PrintAlignment(CMultiAlnPrinter::eNexus,
                            nucleotide_seqalign, nucleotide_seqs,
                            CMultiAlnPrinter::eNucleotide);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN DATA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=10 nchar=2634;") != NPOS);
    BOOST_REQUIRE(output.find("FORMAT datatype=dna gap=- interleave;") != NPOS);
    BOOST_REQUIRE(output.find("MATRIX") != NPOS);

    BOOST_REQUIRE(output.find("2   ------CCGCTACAGGGGGGGCCTGAGGCACTGCAGAAAGTG"
                      "GGCCTGAGCCTCGAGGATGACGGTGCTGCAGGAACCCGT") != NPOS);

    BOOST_REQUIRE(output.find("4   CCAGGCTGCTATATGGCAAGCACTAAACCACTATGCTTACCG"
                      "AGATGCGGTTTTCCTCGCAGAACGCCTTTATGCAGAAGT") != NPOS);

    // last line
    // verify that a ';' follows the alignment
    BOOST_REQUIRE(output.find("10  ACAACTGGATGTGTGACTAGTGCTGACATGTTTCT-------\n\n;")
                  ||
                  // for Windows end of line
                  output.find("10  ACAACTGGATGTGTGACTAGTGCTGACATGTTTCT-------\r\n\r\n;")
                  != NPOS);
    BOOST_REQUIRE(output.find("END;") != NPOS);
}

BOOST_AUTO_TEST_CASE(TestRejectNexusWithNoAlignType)
{
    // verify that formatting nexus alignment with m_AlignType == eNotSet
    // throws the exception
    BOOST_REQUIRE_THROW(PrintAlignment(CMultiAlnPrinter::eNexus,
                                       protein_seqalign, "",
                                       CMultiAlnPrinter::eNotSet),
                        CException);    
}

BOOST_AUTO_TEST_SUITE_END()


CRef<CScope> CreateScope(const string& filename)
{
	const string kDbName("prot_dbs");
	const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);
	TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
	CRef<CScope> scope = tmp_data_loader.NewScope();

    if (filename == "") {
        return scope;
    }

    CNcbiIfstream instream(filename.c_str());
    BOOST_REQUIRE(instream);

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    while (!line_reader.AtEOF()) {

        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
    }

    return scope;
}

