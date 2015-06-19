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
* Author:  Tom Madden
*
* File Description:
*   Unit test module to test CBlastTabularInfo
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/tabular.hpp>

#include "blast_test_util.hpp"
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <objects/blast/Blast4_archive.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>
#include <objects/blast/Blast4_request.hpp>
#include <objects/blast/Blast4_request_body.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_queries.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::align_format;
using namespace TestUtil;

BOOST_AUTO_TEST_SUITE(tabularinfo)

BOOST_AUTO_TEST_CASE(StandardOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream);
    // CBlastTabularInfo ctab(cout);

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }
    ctab.PrintNumProcessed(1);

    string output = CNcbiOstrstreamToString(output_stream);

    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1786181|gb|AE000111.1|AE000111	100.000	10596	0	0	1	10596") != NPOS);
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1787084|gb|AE000188.1|AE000188	97.059	34	1	0	5567	5600	1088") != NPOS);
    BOOST_REQUIRE(output.find("# BLAST processed 1 queries") != NPOS);
}

BOOST_AUTO_TEST_CASE(QuerySubjectScoreOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qseqid sseqid bitscore");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);

    // First result should be "gi|1786181|gb|AE000111.1|AE000111        gi|1786181|gb|AE000111.1|AE000111       1.957e+04".
    // but some windows binaires print the bit score as 1.957e+004.  Hence, we drop the exponent.
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1786181|gb|AE000111.1|AE000111	19568") != NPOS);
    BOOST_REQUIRE(output.find("gi|1786181|gb|AE000111.1|AE000111	gi|1788899|gb|AE000341.1|AE000341	91.6") != NPOS);
}

BOOST_AUTO_TEST_CASE(QueryAccSubjectAccIdentScoreOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qacc sacc pident bitscore");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);

    // First result should be "gi|1786181|gb|AE000111.1|AE000111        gi|1786181|gb|AE000111.1|AE000111       1.957e+04".
    // but some windows binaires print the bit score as 1.957e+004.  Hence, we drop the exponent.
    BOOST_REQUIRE(output.find("AE000111	AE000111	100.000	19568") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000310	94.595	56.5") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000509	80.508	76.8") != NPOS);
}

BOOST_AUTO_TEST_CASE(QueryAccSubjectAccIdentBTOPOutput) {

    const string seqAlignFileName_in = "data/blastn.vs.ecoli.asn";
    CRef<CSeq_annot> san(new CSeq_annot);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *san;
    in.close();

    list<CRef<CSeq_align> > seqalign_list = san->GetData().GetAlign();

    const string kDbName("ecoli");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    
    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qacc sacc score pident btop");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);
    
    BOOST_REQUIRE(output.find("AE000111	AE000111	10596	100.000	10596") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000447	57	89.412	15T-TA2TA2-T6AG8CT1GT4CA10AC28") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000116	48	98.039	12GA38") != NPOS);
    BOOST_REQUIRE(output.find("AE000111	AE000183	36	82.022	14G-TATGAG2-A3-G4-C1A-4-AGA3C-6CATATC7C-1-C28") != NPOS);
}

BOOST_AUTO_TEST_CASE(TaxonomyOutput) {

    const string seqAlignFileName_in = "data/tabular_seqalignset_2.asn";
    CRef<CSeq_align_set> sa(new CSeq_align_set);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *sa;
    in.close();

    const list<CRef<CSeq_align> > & seqalign_list = sa->Get();

    const string kDbName("nr");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();

    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qacc sacc staxids sscinames scomnames sblastnames sskingdoms");

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);

    BOOST_REQUIRE(output.find("M10360	XP_003609077	3880	Medicago truncatula	barrel medic	eudicots	Eukaryota") != NPOS);
    BOOST_REQUIRE(output.find("M10360	ZP_05395304	536227	Clostridium carboxidivorans P7	Clostridium carboxidivorans P7	firmicutes	Bacteria") != NPOS);
    BOOST_REQUIRE(output.find("M10360	YP_001109553	3694	Populus trichocarpa	black cottonwood	eudicots	Eukaryota") != NPOS);
    BOOST_REQUIRE(output.find("M10360	ABZ09338	455551	uncultured marine microorganism HF4000_APKG7H23	uncultured marine microorganism HF4000_APKG7H23	N/A	N/A") != NPOS);
}

BOOST_AUTO_TEST_CASE(SubjectTitlesOutput) {

    const string seqAlignFileName_in = "data/tabular_seqalignset_2.asn";
    CRef<CSeq_align_set> sa(new CSeq_align_set);

    ifstream in(seqAlignFileName_in.c_str());
    in >> MSerial_AsnText >> *sa;
    in.close();

    const list<CRef<CSeq_align> > & seqalign_list = sa->Get();

    const string kDbName("nr");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();

    {
        CNcbiOstrstream output_stream;
    	CBlastTabularInfo ctab(output_stream, "qacc sacc stitle");

    	ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    	{
    		ctab.SetFields(**iter, *scope);
    		ctab.Print();
    	}

    	string output = CNcbiOstrstreamToString(output_stream);

    	vector<string> results;
    	NStr::Tokenize(output, "\n", results);
    	BOOST_REQUIRE(results[0].find("hypothetical protein MTR_4g111700 [Medicago truncatula]") != NPOS);
    	BOOST_REQUIRE(results[3].find("Glycerophosphodiesterase-like protein [Medicago truncatula]") != NPOS);
    	BOOST_REQUIRE(results[4].find("Cell wall-associated hydrolase [Medicago truncatula]") != NPOS);
    	BOOST_REQUIRE(results[6].find("hypothetical protein ALOHA_HF4000APKG7H23ctg3g3 [uncultured marine microorganism HF4000_APKG7H23]") != NPOS);
    }

    {
        CNcbiOstrstream output_stream;
    	CBlastTabularInfo ctab(output_stream, "qacc sacc salltitles");

    	ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    	{
    		ctab.SetFields(**iter, *scope);
    		ctab.Print();
    	}

    	string output = CNcbiOstrstreamToString(output_stream);
    	vector<string> results;
    	NStr::Tokenize(output, "\n", results);
    	string ref_0 = "hypothetical protein MTR_4g111700 [Medicago truncatula]<>" \
		       "hypothetical protein MTR_4g112050 [Medicago truncatula]<>" \
                       "hypothetical protein MTR_4g111700 [Medicago truncatula]";
    	string ref_2 = "hypothetical protein Poptr_cp075 [Populus trichocarpa]<>hypothetical protein " \
    			       "Poptr_cp088 [Populus trichocarpa]<>hypothetical protein Poptr_cp075 [Populus " \
    			       "trichocarpa]<>hypothetical protein Poptr_cp088 [Populus trichocarpa]";
    	string ref_6 = "hypothetical protein ALOHA_HF4000APKG7H23ctg3g3 [uncultured marine " \
    			       "microorganism HF4000_APKG7H23]";

    	BOOST_REQUIRE(results[0].find(ref_0) != NPOS);
    	BOOST_REQUIRE(results[2].find(ref_2) != NPOS);
    	BOOST_REQUIRE(results[6].find(ref_6) != NPOS);
    }
}

// SB-1177
BOOST_AUTO_TEST_CASE(ExtractCorrectSeqIdWhenThereIsSourceObjectInDescription) {

    const string blastArchiveName = "data/query_w_metadata.blast_archive.asn";
    CRef<CBlast4_archive> ba(new CBlast4_archive);

    ifstream in(blastArchiveName.c_str());
    in >> MSerial_AsnText >> *ba;
    in.close();
    BOOST_REQUIRE(ba.NotEmpty());

    const CSeq_align_set::Tdata& seqalign_list = ba->GetResults().GetAlignments().Get();

    const string kDbName("nt.14");
    const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eNucleotide);
    TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, CBlastOM::eLocal);
    CRef<CScope> scope = tmp_data_loader.NewScope();
    const CBioseq_set& bioseqs = ba->GetRequest().GetBody().GetQueue_search().GetQueries().GetBioseq_set();
    scope->AddTopLevelSeqEntry(*bioseqs.GetSeq_set().front());

    CNcbiOstrstream output_stream;
    CBlastTabularInfo ctab(output_stream, "qseqid");
    ctab.SetParseLocalIds(true);    // if this isn't set, the local ID is ignored

    ITERATE(list<CRef<CSeq_align> >, iter, seqalign_list)
    {
       ctab.SetFields(**iter, *scope);
       ctab.Print();
    }

    string output = CNcbiOstrstreamToString(output_stream);
    BOOST_REQUIRE(NStr::StartsWith(output, "Query_1"));
    BOOST_REQUIRE(output.find("Macaca") == NPOS);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================

* ===========================================================================
*/

