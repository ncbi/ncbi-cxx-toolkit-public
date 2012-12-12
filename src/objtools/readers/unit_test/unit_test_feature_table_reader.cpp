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
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
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
            BOOST_CHECK_EQUAL("Unexpected Error", line_error.FeatureName() + ":" + line_error.QualifierName());
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
    bool expected = false;
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
            const CCdregion& cdr = (*feat)->GetData().GetCdregion();
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
