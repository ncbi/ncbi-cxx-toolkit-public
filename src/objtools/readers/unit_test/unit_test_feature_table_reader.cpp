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
* Author:  Colleen Bollin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/table_filter.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/line_error.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


static const char * sc_Table1 = "\
>Feature lcl|seq_1\n\
1\t336\tgene\n\
\t\t\tgene\ta\n\
\t\t\tgene_desc\tb\n\
1\t336\tCDS\n\
\t\t\tproduct c\n\
\t\t\tprot_desc\td\n\
\t\t\tprotein_id\tlcl|seq_1_1\n\
\t\t\tnote\te\n\
";


static bool s_IgnoreError(const ILineError& line_error)
{
    return false;
}


static CRef<CSeq_annot> s_ReadOneTableFromString (const char * str)
{
    CNcbiIstrstream istr(str);
    CRef<ILineReader> reader = ILineReader::New(istr);

    CSimpleTableFilter tbl_filter;
    tbl_filter.AddDisallowedFeatureName("source", ITableFilter::eResult_Disallowed );

    CErrorContainerLenient err_container;
        
    CRef<CSeq_annot> annot = CFeature_table_reader::ReadSequinFeatureTable
                   (*reader, // of type ILineReader, which is like istream but line-oriented
                    CFeature_table_reader::fReportBadKey, // flags also available: fKeepBadKey and fTranslateBadKey (to /standard_name=…)
                    &err_container, // holds errors found during reading
                    &tbl_filter // used to make it act certain ways on certain feats.  In particular, in bankit we consider “source” and “REFERENCE” to be disallowed
                    );

    for (size_t i = 0; i < err_container.Count(); i++) {
        const ILineError& line_error = err_container.GetError(i);
        if (!s_IgnoreError (line_error)) {
            string error_text = line_error.FeatureName() + ":" + line_error.QualifierName();
            BOOST_CHECK_EQUAL(error_text, "Unexpected Error");
        }
    }        
        
    BOOST_REQUIRE(annot != NULL);
    BOOST_REQUIRE(annot->IsFtable());

    return annot;
}


static void CheckExpectedQuals (CConstRef<CSeq_feat> feat, vector<string> expected_quals)
{
    vector<bool> found;
    ITERATE (vector<string>, it, expected_quals) {
        found.push_back(false);
    }
    ITERATE (CSeq_feat::TQual, it, feat->GetQual()) {
        string qual = (*it)->GetQual();
        size_t pos = 0;
        bool expected = false;
        ITERATE (vector<string>, it, expected_quals) {
            if (NStr::Equal(*it, qual)) {
                found[pos] = true;
                expected = true;
            }
            pos++;
        }
        if (!expected) {
            BOOST_CHECK_EQUAL("Unexpected qualifier", qual);
        }
    }
    size_t pos = 0;
    ITERATE (vector<string>, it, expected_quals) {
        if (!found[pos]) {
            BOOST_CHECK_EQUAL("Missing Qualifier", *it);
        }
        pos++;
    }
}


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_FeatureTableWithGeneAndCodingRegion)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table1); 
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_REQUIRE(ftable.size() == 2);
    BOOST_REQUIRE(ftable.front()->IsSetData());
    BOOST_REQUIRE(ftable.front()->GetData().IsGene());
    BOOST_REQUIRE(ftable.back()->IsSetData());
    BOOST_REQUIRE(ftable.back()->GetData().IsCdregion());
}


static const char * sc_Table2 = "\
>Feature lcl|seq_1\n\
1\t336\tgene\n\
\t\t\tgene\ta\n\
\t\t\tgene_desc\tb\n\
1\t336\tCDS\n\
\t\t\tproduct c\n\
\t\t\tprot_desc\td\n\
\t\t\tprotein_id\tlcl|seq_1_1\n\
\t\t\tnote\te\n\
>Feature lcl|seq_2\n\
1\t336\tgene\n\
\t\t\tgene\ta\n\
\t\t\tgene_desc\tb\n\
1\t336\tCDS\n\
\t\t\tproduct c\n\
\t\t\tprot_desc\td\n\
\t\t\tprotein_id\tlcl|seq_2_1\n\
\t\t\tnote\te\n\
";


///
/// Test reading multiple tables
///
BOOST_AUTO_TEST_CASE(Test_MultipleFeatureTables)
{
    CNcbiIstrstream istr(sc_Table2);
    CRef<ILineReader> reader = ILineReader::New(istr);

    CSimpleTableFilter tbl_filter;
    tbl_filter.AddDisallowedFeatureName("source", ITableFilter::eResult_Disallowed );
        
    CErrorContainerLenient err_container;

    CRef<CSeq_annot> annot;
    while ((annot = CFeature_table_reader::ReadSequinFeatureTable
                   (*reader, // of type ILineReader, which is like istream but line-oriented
                    CFeature_table_reader::fReportBadKey, // flags also available: fKeepBadKey and fTranslateBadKey (to /standard_name=…)
                    &err_container, // holds errors found during reading
                    &tbl_filter // used to make it act certain ways on certain feats.  In particular, in bankit we consider “source” and “REFERENCE” to be disallowed
                    )) != NULL) {
        BOOST_REQUIRE(err_container.Count() == 0);
        BOOST_REQUIRE(annot->IsFtable());
        const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
        if (ftable.size() == 0) {
            break;
        }
        if (ftable.size() != 0) {
            BOOST_REQUIRE(ftable.size() == 2);
            BOOST_REQUIRE(ftable.front()->IsSetData());
            BOOST_REQUIRE(ftable.front()->GetData().IsGene());
            BOOST_REQUIRE(ftable.back()->IsSetData());
            BOOST_REQUIRE(ftable.back()->GetData().IsCdregion());
        }
    }
}


static const char * sc_Table3 = "\
>Feature gnl|FlyBase|2R|gb|AE013599\n\
<8328778\t8328138 gene\n\
\t\t\tgene\tCG30334\n\
\t\t\tlocus_tag\tDmel_CG30334\n\
\t\t\tcyt_map 49A6-49A6\n\
\t\t\tgene_syn\tNEST:bs09a11\n\
\t\t\tgene_syn\tDmel\\CG30334\n\
\t\t\tdb_xref FLYBASE:FBgn0050334\n\
8328778\t8328693 mRNA\n\
8328642\t8328138\n\
\t\t\tgene\tCG30334\n\
\t\t\tlocus_tag\tDmel_CG30334\n\
\t\t\tproduct CG30334, transcript variant A\n\
\t\t\tnote\tCG30334-RA; Dmel\\CG30334-RA\n\
\t\t\tprotein_id\tgnl|FlyBase|CG30334-PA|gb|AAM68686\n\
\t\t\ttranscript_id\tgnl|FlyBase|CG30334-RA\n\
8328633\t8328241 CDS\n\
\t\t\tgene\tCG30334\n\
\t\t\tlocus_tag\tDmel_CG30334\n\
\t\t\ttranscript_id\tgnl|FlyBase|CG30334-RA\n\
\t\t\tprot_desc\tCG30334 gene product from transcript CG30334-RA\n\
\t\t\tproduct CG30334, isoform A\n\
\t\t\tproduct CG30334-PA\n\
\t\t\tprotein_id\tgnl|FlyBase|CG30334-PA|gb|AAM68686\n\
8328778\t8328693 mRNA\n\
8328625\t8328138\n\
\t\t\tgene\tCG30334\n\
\t\t\tlocus_tag\tDmel_CG30334\n\
\t\t\tproduct CG30334, transcript variant B\n\
\t\t\tnote\tCG30334-RB; Dmel\\CG30334-RB\n\
\t\t\tprotein_id\tgnl|FlyBase|CG30334-PB\n\
\t\t\ttranscript_id\tgnl|FlyBase|CG30334-RB\n\
8328694\t8328693 CDS\n\
8328625\t8328241\n\
\t\t\tgene\tCG30334\n\
\t\t\tlocus_tag\tDmel_CG30334\n\
\t\t\ttranscript_id\tgnl|FlyBase|CG30334-RB\n\
\t\t\tprot_desc\tCG30334 gene product from transcript CG30334-RB\n\
\t\t\tproduct CG30334, isoform B\n\
\t\t\tproduct CG30334-PB\n\
\t\t\tprotein_id\tgnl|FlyBase|CG30334-PB\n\
\t\t\ttransl_except\t(pos:8328694..8328693,8328625,aa:Met) \n\
\t\t\tnote\tnon-AUG (AUC) translation initiation\n\
";


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_FlybaseFeatureTableWithMultiIntervalTranslExcept)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table3); 
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 5);
    CConstRef<CSeq_feat> cds = ftable.back();
    BOOST_REQUIRE(cds->IsSetData());
    BOOST_REQUIRE(cds->GetData().IsCdregion());
    BOOST_CHECK_EQUAL(cds->GetQual().size(), 3);
    vector<string> expected_quals;
    expected_quals.push_back("transcript_id");
    expected_quals.push_back("protein_id");
    expected_quals.push_back("transl_except");
    CheckExpectedQuals (cds, expected_quals);
}


static const char * sc_Table4 = "\
>Feature ref|NC_019571.1|\n\
1\t1578\tgene\n\
\t\t\tgene\tCOX1\n\
\t\t\tdb_xref\tGeneID:14048202\n\
1\t1578\tCDS\n\
\t\t\tproduct\tcytochrome c oxidase subunit I\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024788.1|\n\
1577\t1634\ttRNA\n\
\t\t\tproduct\ttRNA-Cys\n\
1635\t1694\ttRNA\n\
\t\t\tproduct\ttRNA-Met\n\
1697\t1751\ttRNA\n\
\t\t\tproduct\ttRNA-Asp\n\
1759\t1814\ttRNA\n\
\t\t\tproduct\ttRNA-Gly\n\
1815\t2507\tgene\n\
\t\t\tgene\tCOX2\n\
\t\t\tdb_xref\tGeneID:14048191\n\
1815\t2507\tCDS\n\
\t\t\tproduct\tcytochrome c oxidase subunit II\n\
\t\t\ttransl_except\t(pos:1815..1817,aa:Met)\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024789.1|\n\
2506\t2561\ttRNA\n\
\t\t\tproduct\ttRNA-His\n\
2560\t3521\trRNA\n\
\t\t\tproduct\t16S ribosomal RNA\n\
\t\t\tnote\tl-rRNA\n\
3517\t3855\tgene\n\
\t\t\tgene\tND3\n\
\t\t\tdb_xref\tGeneID:14048192\n\
3517\t3855\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 3\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024790.1|\n\
3858\t5459\tgene\n\
\t\t\tgene\tND5\n\
\t\t\tdb_xref\tGeneID:14048193\n\
3858\t5459\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 5\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024791.1|\n\
5843\t5897\ttRNA\n\
\t\t\tproduct\ttRNA-Ala\n\
6132\t6190\ttRNA\n\
\t\t\tproduct\ttRNA-Pro\n\
6200\t6253\ttRNA\n\
\t\t\tproduct\ttRNA-Val\n\
6257\t6682\tgene\n\
\t\t\tgene\tND6\n\
\t\t\tdb_xref\tGeneID:14048194\n\
6257\t6682\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 6\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024792.1|\n\
6744\t6980\tgene\n\
\t\t\tgene\tND4L\n\
\t\t\tdb_xref\tGeneID:14048195\n\
6744\t6980\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 4L\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024793.1|\n\
6986\t7041\ttRNA\n\
\t\t\tproduct\ttRNA-Trp\n\
7054\t7109\ttRNA\n\
\t\t\tproduct\ttRNA-Glu\n\
7084\t7780\trRNA\n\
\t\t\tproduct\t12S ribosomal RNA\n\
\t\t\tnote\ts-rRNA\n\
7779\t7834\ttRNA\n\
\t\t\tproduct\ttRNA-Ser\n\
\t\t\tcodon_recognized\tUCN\n\
7842\t7896\ttRNA\n\
\t\t\tproduct\ttRNA-Asn\n\
7907\t7962\ttRNA\n\
\t\t\tproduct\ttRNA-Tyr\n\
7962\t8828\tgene\n\
\t\t\tgene\tND1\n\
\t\t\tdb_xref\tGeneID:14048196\n\
7962\t8828\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 1\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024794.1|\n\
8840\t9439\tgene\n\
\t\t\tgene\tATP6\n\
\t\t\tdb_xref\tGeneID:14048197\n\
8840\t9439\tCDS\n\
\t\t\tproduct\tATP synthase F0 subunit 6\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024795.1|\n\
9461\t9522\ttRNA\n\
\t\t\tproduct\ttRNA-Lys\n\
9523\t9578\ttRNA\n\
\t\t\tproduct\ttRNA-Leu\n\
\t\t\tcodon_recognized\tUUR\n\
9577\t9629\ttRNA\n\
\t\t\tproduct\ttRNA-Ser\n\
\t\t\tcodon_recognized\tAGN\n\
9632\t10486\tgene\n\
\t\t\tgene\tND2\n\
\t\t\tdb_xref\tGeneID:14048198\n\
9632\t10486\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 2\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024796.1|\n\
10486\t10541\ttRNA\n\
\t\t\tproduct\ttRNA-Ile\n\
10536\t10589\ttRNA\n\
\t\t\tproduct\ttRNA-Arg\n\
10589\t10646\ttRNA\n\
\t\t\tproduct\ttRNA-Gln\n\
10661\t10714\ttRNA\n\
\t\t\tproduct\ttRNA-Phe\n\
10706\t11809\tgene\n\
\t\t\tgene\tCYTB\n\
\t\t\tdb_xref\tGeneID:14048199\n\
10706\t11809\tCDS\n\
\t\t\tproduct\tcytochrome b\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024797.1|\n\
11809\t11866\ttRNA\n\
\t\t\tproduct\ttRNA-Leu\n\
\t\t\tcodon_recognized\tCUN\n\
11859\t12630\tgene\n\
\t\t\tgene\tCOX3\n\
\t\t\tdb_xref\tGeneID:14048200\n\
11859\t12630\tCDS\n\
\t\t\tproduct\tcytochrome c oxidase subunit III\n\
\t\t\ttransl_except\t(pos:12630,aa:TERM)\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024798.1|\n\
\t\t\tnote\tTAA stop codon is completed by the addition of 3' A residues to the mRNA\n\
12631\t12684\ttRNA\n\
\t\t\tproduct\ttRNA-Thr\n\
12684\t13913\tgene\n\
\t\t\tgene\tND4\n\
\t\t\tdb_xref\tGeneID:14048201\n\
12684\t13913\tCDS\n\
\t\t\tproduct\tNADH dehydrogenase subunit 4\n\
\t\t\ttransl_table\t5\n\
\t\t\tprotein_id\tref|YP_007024799.1|\n\
";


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_NCTableWithtRNAs)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table4); 
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 48);
    ITERATE(CSeq_annot::TData::TFtable, feat, ftable) {
        if ((*feat)->GetData().IsRna()) {
            const CRNA_ref& rna = (*feat)->GetData().GetRna();
            BOOST_REQUIRE (rna.IsSetExt());
            if (rna.GetType() == CRNA_ref::eType_tRNA) {
                BOOST_REQUIRE (rna.GetExt().IsTRNA());
                BOOST_REQUIRE (rna.GetExt().GetTRNA().IsSetAa());
            } else if (rna.GetType() == CRNA_ref::eType_rRNA) {
                BOOST_REQUIRE (rna.GetExt().IsName());
                BOOST_REQUIRE (!NStr::IsBlank(rna.GetExt().GetName()));
            }
        } else if ((*feat)->GetData().IsCdregion()) {
            BOOST_REQUIRE ((*feat)->IsSetXref());
            BOOST_REQUIRE ((*feat)->GetXref().front()->IsSetData());
            BOOST_REQUIRE ((*feat)->GetXref().front()->GetData().IsProt());
            const CProt_ref& prot = (*feat)->GetXref().front()->GetData().GetProt();
            BOOST_REQUIRE (prot.IsSetName());
            BOOST_REQUIRE (prot.GetName().size() == 1);
            vector<string> expected_quals;
            expected_quals.push_back("protein_id");
            if (NStr::Equal(prot.GetName().front(), "cytochrome c oxidase subunit II")
                || NStr::Equal(prot.GetName().front(), "cytochrome c oxidase subunit III")) {
                expected_quals.push_back("transl_except");
            }
            CheckExpectedQuals (*feat, expected_quals);
        } else if ((*feat)->GetData().IsGene()) {
            const CGene_ref& gene = (*feat)->GetData().GetGene();
            BOOST_REQUIRE (gene.IsSetLocus());
            BOOST_REQUIRE ((*feat)->IsSetDbxref());
            BOOST_REQUIRE ((*feat)->GetDbxref().size() == 1);
            CConstRef<CDbtag> tag = (*feat)->GetDbxref().front();
            BOOST_REQUIRE (tag->IsSetDb());
            BOOST_CHECK_EQUAL(tag->GetDb(), "GeneID");
            BOOST_REQUIRE (tag->GetTag().IsId());
        }
    }
}


static const char * sc_Table5 = "\
>Feature gb|CP003382.1|\n\
1982606\t1982707\tgene\n\
\t\t\tlocus_tag\tDeipe_1981\n\
\t\t\tnote\tIMG reference gene:2509592968\n\
1982606\t1982707\tncRNA\n\
\t\t\tncRNA_class\tSRP_RNA\n\
\t\t\tproduct\tBacterial signal recognition particle RNA\n\
";


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_CPTableWithncRNAs)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table5); 
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 2);
    CConstRef<CSeq_feat> ncrna = ftable.back();
    BOOST_REQUIRE(ncrna->IsSetData());
    BOOST_REQUIRE(ncrna->GetData().IsRna());
    const CRNA_ref& rna = ncrna->GetData().GetRna();
    BOOST_CHECK_EQUAL(rna.GetType(), CRNA_ref::eType_ncRNA);
    BOOST_REQUIRE(rna.IsSetExt());
    BOOST_REQUIRE(rna.GetExt().IsGen());
    BOOST_REQUIRE(rna.GetExt().GetGen().IsSetProduct());
    BOOST_REQUIRE(rna.GetExt().GetGen().IsSetClass());
    BOOST_CHECK_EQUAL(rna.GetExt().GetGen().GetProduct(), "Bacterial signal recognition particle RNA");
    BOOST_CHECK_EQUAL(rna.GetExt().GetGen().GetClass(), "SRP_RNA");
}


static const char * sc_Table6 = "\
>Feature ref|NC_000008.9|NC_000008\n\
<1\t>13208\tgene\n\
\t\t\tgene\tFBXO25\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tHGNC:13596\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tmRNA\n\
<6465\t6514\n\
<9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_183421.1\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tmRNA\n\
<6465\t6514\n\
<9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_183420.1\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tmRNA\n\
<9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_012173.3\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tCDS\n\
<6465\t6514\n\
<9194\t9286\n\
\t\t\tproduct\tF-box only protein 25 isoform 1\n\
\t\t\tproduct\tF-box protein Fbx25\n\
\t\t\tproduct\tF-box only protein 25\n\
\t\t\tprotein_id\tNP_904357.1\n\
\t\t\tnote\tisoform 1 is encoded by transcript variant 1\n\
\t\t\tGO_function\tubiquitin-protein ligase activity|0004842|10531035|NAS\n\
\t\t\tGO_process\tprotein ubiquitination|0016567|10531035|NAS\n\
\t\t\tGO_component\tubiquitin ligase complex|0000151|10531035|NAS\n\
\t\t\tdb_xref\tCCDS:CCDS5953.1\n\
\t\t\tdb_xref\tGeneID:26260\n\
<4920\t5023\tCDS\n\
<6465\t6514\n\
<9194\t9286\n\
\t\t\tproduct\tF-box only protein 25 isoform 2\n\
\t\t\tproduct\tF-box protein Fbx25\n\
\t\t\tproduct\tF-box only protein 25\n\
\t\t\tprotein_id\tNP_904356.1\n\
\t\t\tnote\tisoform 2 is encoded by transcript variant 2\n\
\t\t\tGO_function\tubiquitin-protein ligase activity|0004842|10531035|NAS\n\
\t\t\tGO_process\tprotein ubiquitination|0016567|10531035|NAS\n\
\t\t\tGO_component\tubiquitin ligase complex|0000151|10531035|NAS\n\
\t\t\tdb_xref\tCCDS:CCDS5952.1\n\
\t\t\tdb_xref\tGeneID:26260\n\
1\t13208\tvariation\n\
150\t150\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:55727401\n\
150\t150\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:10793768\n\
257\t257\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:12138618\n\
266\t266\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:2427889\n\
269\t269\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:7831204\n\
299\t299\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:62483103\n\
302\t302\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2427890\n\
325\t325\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:2977629\n\
408\t408\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:62483104\n\
414\t414\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:2905047\n\
438\t438\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:11786745\n\
480\t480\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3115862\n\
496\t496\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:3094316\n\
501\t501\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:12547344\n\
503\t504\tvariation\n\
\t\t\treplace\tGAAAATAGGTTTCACATCTTTTTTTTAACTTATATAAAATTGACTGGACTTTCTCTTCTGTGTGTTGTGTTAGATATTTAGGAAGGAAT\n\
\t\t\tdb_xref\tdbSNP:71202620\n\
504\t504\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:12550258\n\
504\t504\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3115863\n\
537\t537\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:73525986\n\
561\t561\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:11996480\n\
594\t594\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:11985199\n\
620\t620\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:11997205\n\
637\t637\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3115864\n\
733\t733\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:61688116\n\
735\t735\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:57970854\n\
786\t786\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:12184338\n\
804\t804\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:55678681\n\
810\t810\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:12184332\n\
849\t849\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:12550792\n\
852\t852\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:11783529\n\
901\t901\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:58688196\n\
901\t901\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:56426218\n\
929\t929\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:11136669\n\
976\t976\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:55837473\n\
989\t989\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:7834538\n\
1037\t1037\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:7844307\n\
1135\t1135\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:56115318\n\
1203\t1203\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:10089646\n\
1226\t1226\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:7823777\n\
1228\t1228\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:56312035\n\
1425\t1425\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:7813883\n\
1511\t1511\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:73525988\n\
1569\t1569\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:11783748\n\
1667\t1667\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:35389027\n\
1721\t1721\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:4495405\n\
1988\t1988\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:73669377\n\
2471\t2471\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2954702\n\
2512\t2512\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:7010178\n\
2941\t2942\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tA\n\
\t\t\tdb_xref\tdbSNP:34708162\n\
3014\t3014\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:73669378\n\
3233\t3233\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tA\n\
\t\t\tdb_xref\tdbSNP:34374462\n\
3630\t3631\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:36025637\n\
4035\t4035\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:35310547\n\
4349\t4349\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:17812912\n\
4527\t4527\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:73669379\n\
4790\t4790\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:9644342\n\
4845\t4845\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:73525989\n\
4923\t4923\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:28438773\n\
4937\t5023\tCDS\n\
9194\t9286\n\
\t\t\tproduct\tF-box only protein 25 isoform 3\n\
\t\t\tproduct\tF-box protein Fbx25\n\
\t\t\tproduct\tF-box only protein 25\n\
\t\t\tprotein_id\tNP_036305.2\n\
\t\t\tnote\tisoform 3 is encoded by transcript variant 3\n\
\t\t\tGO_function\tubiquitin-protein ligase activity|0004842|10531035|NAS\n\
\t\t\tGO_process\tprotein ubiquitination|0016567|10531035|NAS\n\
\t\t\tGO_component\tubiquitin ligase complex|0000151|10531035|NAS\n\
\t\t\tdb_xref\tCCDS:CCDS5954.1\n\
\t\t\tdb_xref\tGeneID:26260\n\
5776\t5776\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:6981190\n\
5977\t5977\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:56259539\n\
6016\t6016\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:61012540\n\
6130\t6130\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:2722516\n\
6235\t6235\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:2722517\n\
6290\t6290\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:9644272\n\
6536\t6536\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:6998464\n\
6842\t6842\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:73173380\n\
7314\t7314\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2798496\n\
7316\t7316\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:2488924\n\
7421\t7421\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:4973692\n\
7424\t7424\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:4973650\n\
7431\t7431\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:2722519\n\
7447\t7447\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2488925\n\
7493\t7493\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:12550478\n\
7545\t7545\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:12680761\n\
7900\t7900\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:17064969\n\
8491\t8491\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514143\n\
8497\t8497\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514144\n\
8559\t8559\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514145\n\
8615\t8615\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514146\n\
8637\t8637\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514147\n\
8758\t8758\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:62483130\n\
8785\t8785\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514148\n\
8815\t8815\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514149\n\
8819\t8819\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:13267767\n\
8864\t8864\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514150\n\
8924\t8924\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514151\n\
9036\t9036\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514152\n\
9067\t9067\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514153\n\
9174\t9174\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:71219302\n\
9187\t9187\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514154\n\
9191\t9191\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:56016669\n\
9191\t9191\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:17665428\n\
9365\t9365\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514155\n\
9405\t9405\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:71514156\n\
9407\t9407\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:3965448\n\
9415\t9415\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:71514157\n\
9415\t9415\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3873815\n\
9512\t9512\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tC\n\
\t\t\tdb_xref\tdbSNP:7838909\n\
10524\t10524\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3936437\n\
10667\t10667\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:3931132\n\
10711\t10711\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:3936436\n\
10780\t10780\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2034353\n\
10783\t10783\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3936435\n\
10792\t10792\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:1992879\n\
11159\t11159\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:61708689\n\
11319\t11320\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tTT\n\
\t\t\tdb_xref\tdbSNP:34339077\n\
11332\t11332\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:11990180\n\
11333\t11334\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tTT\n\
\t\t\tdb_xref\tdbSNP:56674698\n\
11343\t11343\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2335278\n\
11406\t11406\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2878381\n\
11443\t11443\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:2335279\n\
11455\t11455\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2335280\n\
11491\t11491\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3965449\n\
11501\t11501\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:3965450\n\
11537\t11537\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:58291483\n\
11653\t11653\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:35051996\n\
11765\t11765\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:9644275\n\
11906\t11906\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:9644276\n\
12198\t12198\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:58937369\n\
12492\t12492\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:73173382\n\
12569\t12569\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:73173383\n\
12684\t12684\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:12546248\n\
12744\t12744\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:59983968\n\
12864\t12864\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:4045703\n\
12888\t12888\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:3857917\n\
12982\t12983\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:35231189\n\
13029\t13029\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:880926\n\
13041\t13041\tvariation\n\
\t\t\treplace\tA\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2335281\n\
13057\t13058\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tTTA\n\
\t\t\tdb_xref\tdbSNP:4045704\n\
13088\t13088\tvariation\n\
\t\t\treplace\tC\n\
\t\t\treplace\tG\n\
\t\t\tdb_xref\tdbSNP:2003213\n\
13208\t13208\tvariation\n\
\t\t\treplace\tG\n\
\t\t\treplace\tT\n\
\t\t\tdb_xref\tdbSNP:7833133\n\
\n\
\n\
";


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_TableWithVariationsAndGoTerms)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table6); 
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 138);

    int num_variations = 0;
    ITERATE(CSeq_annot::TData::TFtable, feat, ftable) {
        if ((*feat)->GetData().IsImp()) {
            if (NStr::Equal((*feat)->GetData().GetImp().GetKey(), "variation")) {
                num_variations++;
            }
        } else if ((*feat)->GetData().IsCdregion()) {
            BOOST_REQUIRE((*feat)->IsSetExt());
            BOOST_CHECK_EQUAL((*feat)->GetExt().GetType().GetStr(), "GeneOntology");
            ITERATE(CUser_object::TData, it, (*feat)->GetExt().GetData()) {
                BOOST_REQUIRE ((*it)->GetData().IsFields());
                BOOST_CHECK_EQUAL((*it)->GetData().GetFields().size(), 4);
            }
        }
    }
    BOOST_CHECK_EQUAL(num_variations, 131);
}


static const char * sc_Table7 = "\
>Feature gb|CP003382.1|\n\
1982606\t1982707\tgene\n\
\t\t\tlocus_tag\tDeipe_1981\n\
\t\t\tnote\tIMG reference gene:2509592968\n\
1982606\t1982707\tncRNA\n\
\t\t\tncRNA_class\tSRP_RNA\n\
\t\t\tproduct\tBacterial signal recognition particle RNA\n\
\t\t\tEC_number\t1.2.3.4\n\
\t\t\tPCR_conditions\tabc\n\
1\t20\tmisc_feature\n\
\t\t\tSTS\tabc\n\
";


///
/// Test a simple table
///
BOOST_AUTO_TEST_CASE(Test_CapitalizedQualifiers)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table7); 
}
