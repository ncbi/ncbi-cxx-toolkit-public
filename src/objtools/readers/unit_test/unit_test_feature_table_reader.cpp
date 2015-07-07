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
#include <objects/misc/sequence_util_macros.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/table_filter.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/line_error.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    class CMessageListenerLenientIgnoreProgress:
        public CMessageListenerBase
    {
    public:
        CMessageListenerLenientIgnoreProgress() {};
        ~CMessageListenerLenientIgnoreProgress() {};

        bool
            PutError(
            const ILineError& err ) 
        {
            if( err.Severity() > eDiag_Info ) {
                StoreError(err);
            }
            return true;
        };
    };   
}

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
    if( line_error.Severity() <= eDiag_Info ) {
        return true;
    }
    return false;
}

static void s_ListLinesWithPattern(
    const CTempString & str,
    const CTempString & pattern,
    vector<size_t> & out_vecOfLinesThatMatch )
{
    out_vecOfLinesThatMatch.clear();

    vector<CTempString> vecOfLineContents;
    NStr::Tokenize(str, "\n", vecOfLineContents);
    ITERATE_0_IDX(ii, vecOfLineContents.size()) {
        if( NPOS != NStr::Find(vecOfLineContents[ii], pattern)) {
            out_vecOfLinesThatMatch.push_back(ii + 1); // line nums are 1-based
        }
    }
}

static size_t s_CountOccurrences(const CTempString & str, const CTempString & pattern )
{
    vector<CTempString> vecPiecesBetweenTheWordError;
    NStr::TokenizePattern(
        str, pattern, vecPiecesBetweenTheWordError );
    size_t iNumErrsExpected = ( vecPiecesBetweenTheWordError.size() - 1 );
    BOOST_REQUIRE(iNumErrsExpected > 0);
    return iNumErrsExpected;
}

typedef list<ILineError::EProblem> TErrList;

static void 
s_CheckErrorsVersusExpected( 
    ILineErrorListener * pMessageListener, 
    TErrList expected_errors // yes, *copy* the container
    )
{
    for (size_t i = 0; i < pMessageListener->Count(); i++) {
        const ILineError& line_error = pMessageListener->GetError(i);
        string error_text = line_error.Message();
        if( s_IgnoreError(line_error) ) {
            // certain error types may be ignored
        } else if( expected_errors.empty() ) {
            BOOST_ERROR("More errors occurred than expected at " << error_text);
        } else {
            BOOST_CHECK_EQUAL(
                line_error.ProblemStr() + "(" + error_text + ")", 
                ILineError::ProblemStr(expected_errors.front()) +
                    "(" + error_text + ")" );
            expected_errors.pop_front();
        }
    }

    BOOST_CHECK_MESSAGE( expected_errors.empty(),
        "There were " << expected_errors.size() 
        << " expected errors which did not occur." );
}

static CRef<CSeq_annot> s_ReadOneTableFromString (
    const char * str,
    // no errors expected by default
     const TErrList & expected_errors = TErrList(),
     CFeature_table_reader::TFlags additional_flags = 0,
     ILineErrorListener * pMessageListener = NULL );

static CRef<CSeq_annot> s_ReadOneTableFromString (
    const char * str,
    const TErrList & expected_errors,
    CFeature_table_reader::TFlags additional_flags,
    ILineErrorListener * pMessageListener )
{
    CNcbiIstrstream istr(str);
    CRef<ILineReader> reader = ILineReader::New(istr);

    CSimpleTableFilter tbl_filter(ITableFilter::eAction_Okay);
    tbl_filter.SetActionForFeat("source", ITableFilter::eAction_Disallowed );

    auto_ptr<ILineErrorListener> p_temp_err_container;
    if( ! pMessageListener ) {
        p_temp_err_container.reset( new CMessageListenerLenientIgnoreProgress );
        pMessageListener = p_temp_err_container.get();
    }
        
    CRef<CSeq_annot> annot = CFeature_table_reader::ReadSequinFeatureTable
                   (*reader, // of type ILineReader, which is like istream but line-oriented
                    additional_flags | CFeature_table_reader::fReportBadKey, // flags also available: fKeepBadKey and fTranslateBadKey (to /standard_name=…)
                    pMessageListener, // holds errors found during reading
                    &tbl_filter // used to make it act certain ways on certain feats.  In particular, in bankit we consider “source” and “REFERENCE” to be disallowed
                    );

    s_CheckErrorsVersusExpected( pMessageListener, expected_errors );
        
    BOOST_REQUIRE(annot != NULL);
    BOOST_REQUIRE(annot->IsFtable());

    return annot;
}

typedef list< CRef<CSeq_annot> > TAnnotRefList;
typedef auto_ptr<TAnnotRefList> TAnnotRefListPtr;

static TAnnotRefListPtr
s_ReadMultipleTablesFromString(
    const char * str,
    // no errors expected by default
    const TErrList & expected_errors = TErrList(),
    CFeature_table_reader::TFlags additional_flags = 0,
    ILineErrorListener * pMessageListener = NULL );

static TAnnotRefListPtr
s_ReadMultipleTablesFromString(
    const char * str,
    const TErrList & expected_errors,
    CFeature_table_reader::TFlags additional_flags,
    ILineErrorListener * pMessageListener )
{
    TAnnotRefListPtr pAnnotRefList( new TAnnotRefList );

    CNcbiIstrstream istr(str);
    CRef<ILineReader> reader = ILineReader::New(istr);

    CSimpleTableFilter tbl_filter(ITableFilter::eAction_Okay);
    tbl_filter.SetActionForFeat("source", ITableFilter::eAction_Disallowed );

    auto_ptr<ILineErrorListener> p_temp_err_container;
    if( ! pMessageListener ) {
        p_temp_err_container.reset( new CMessageListenerLenientIgnoreProgress );
        pMessageListener = p_temp_err_container.get();
    }

    CRef<CSeq_annot> annot;
    while ((annot = CFeature_table_reader::ReadSequinFeatureTable
        (*reader,
        additional_flags | CFeature_table_reader::fReportBadKey, 
        pMessageListener, 
        &tbl_filter 
        )) != NULL) 
    {
            BOOST_REQUIRE(annot->IsFtable());
            if( annot->GetData().GetFtable().empty() ) {
                break;
            }
            pAnnotRefList->push_back( CRef<CSeq_annot>(annot) );
    }

    s_CheckErrorsVersusExpected( pMessageListener, expected_errors );

    return pAnnotRefList;
}

static void CheckExpectedQuals (CConstRef<CSeq_feat> feat, 
    const set<string> & expected_quals)
{
    set<string> found_quals;
    ITERATE (CSeq_feat::TQual, it, feat->GetQual()) {
        found_quals.insert( (*it)->GetQual() );
    }

    // print unexpected qualifiers
    // (found, but not expected)
    {{
        set<string> unexpected_quals;
        set_difference(found_quals.begin(), found_quals.end(),
            expected_quals.begin(), expected_quals.end(),
            inserter(unexpected_quals, unexpected_quals.begin() ) );
        ITERATE(set<string>, unexpected_qual, unexpected_quals) {
            BOOST_CHECK_EQUAL("Unexpected qualifier", *unexpected_qual);
        }
    }}

    // print missing qualifiers
    // (expected, but not found)
    {{
        set<string> missing_quals;
        set_difference(expected_quals.begin(), expected_quals.end(),
            found_quals.begin(), found_quals.end(),
            inserter(missing_quals, missing_quals.begin() ) );
        ITERATE(set<string>, missing_qual, missing_quals) {
            BOOST_CHECK_EQUAL("Missing qualifier", *missing_qual);
        }
    }}
}

NCBITEST_INIT_CMDLINE(descrs)
{
    // Add calls like this for each command-line argument to be used.
    descrs->AddFlag("v", "Verbose: tests produce extra output");
}

// convenience functions for accessing the above cmd line opts
namespace {
    bool IsVerbose()
    {
        return CNcbiApplication::Instance()->GetArgs()["v"];
    }

    void SerializeOutIfVerbose(const string & note, const CSerialObject & obj)
    {
        if( ! IsVerbose() ) {
            return;
        }

        cerr << "Verbose output: " << note << ": "
             << MSerial_AsnText << obj << endl;
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
    // must be true AND not throw an exception
    NCBITEST_REQUIRE(
        ftable.back()->GetProduct().GetWhole().GetLocal().GetStr() == "seq_1_1" );
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
    TAnnotRefListPtr pAnnotRefList = s_ReadMultipleTablesFromString(sc_Table2);
    BOOST_REQUIRE_EQUAL( pAnnotRefList->size(), 2 );

    ITERATE(TAnnotRefList, annot_ref_it, *pAnnotRefList ) {
        const CSeq_annot::TData::TFtable& ftable = 
            (*annot_ref_it)->GetData().GetFtable();
        BOOST_REQUIRE_EQUAL( ftable.size(), 2 );

        BOOST_REQUIRE_EQUAL(ftable.size(), 2);
        BOOST_REQUIRE(ftable.front()->IsSetData());
        BOOST_REQUIRE(ftable.front()->GetData().IsGene());
        BOOST_REQUIRE(ftable.back()->IsSetData());
        BOOST_REQUIRE(ftable.back()->GetData().IsCdregion());
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
    BOOST_CHECK_EQUAL(cds->GetQual().size(), 2);
    NCBITEST_CHECK( cds->GetXref()[0]->GetData().IsGene() );
    set<string> expected_quals;
    expected_quals.insert("transcript_id");
    expected_quals.insert("transl_except");
    CheckExpectedQuals (cds, expected_quals);

    // check protein_ids
    CSeq_annot::TData::TFtable::const_iterator ftable_it = ftable.begin();
    NCBITEST_CHECK( ! (*ftable_it++)->IsSetProduct() );
    NCBITEST_CHECK_EQUAL( (*ftable_it++)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr(), "CG30334-PA" );
    NCBITEST_CHECK_EQUAL( (*ftable_it++)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr(), "CG30334-PA" );
    NCBITEST_CHECK_EQUAL( (*ftable_it++)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr(), "CG30334-PB" );
    NCBITEST_CHECK_EQUAL( (*ftable_it++)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr(), "CG30334-PB" );
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
            set<string> expected_quals;
            NCBITEST_CHECK( NStr::StartsWith( 
                (*feat)->GetProduct().GetWhole().GetOther().GetAccession(), "YP_0070247") );
            if (NStr::Equal(prot.GetName().front(), "cytochrome c oxidase subunit II")
                || NStr::Equal(prot.GetName().front(), "cytochrome c oxidase subunit III")) {
                expected_quals.insert("transl_except");
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
6465\t6514\n\
9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_183421.1\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tmRNA\n\
6465\t6514\n\
9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_183420.1\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tmRNA\n\
9194\t9286\n\
\t\t\tproduct\tF-box protein 25\n\
\t\t\ttranscript_id\tNM_012173.3\n\
\t\t\texception\tunclassified transcription discrepancy\n\
\t\t\tdb_xref\tGeneID:26260\n\
\t\t\tdb_xref\tMIM:609098\n\
<4920\t5023\tCDS\n\
6465\t6514\n\
9194\t9286\n\
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
6465\t6514\n\
9194\t9286\n\
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

            // since no FASTA tag, becomes a local ID even though it might be
            // parsed as an accession
            NCBITEST_CHECK( 
                NStr::StartsWith( 
                (*feat)->GetProduct().GetWhole().GetLocal().GetStr(), "NP_") );
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

static const char * sc_Table8 = "\
>Feature lcl|seq1\n\
1\t10\ttRNA\n\
20\t30\n\
\t\t\tanticodon\t(pos:21..23,aa:His)\n\
101\t110\ttRNA\n\
120\t130\n\
\t\t\tanticodon\t(pos:complement(121..123),aa:Pro)\n\
201\t210\ttRNA\n\
220\t230\n\
\t\t\tanticodon\t(pos:join(210,220..221),aa:Ala)\n\
301\t310\ttRNA\n\
320\t330\n\
\t\t\tanticodon\t(pos:complement(join(310,320..321)),aa:Cys)\n\
";

BOOST_AUTO_TEST_CASE(Test_tRNAAnticodonQualifiers)
{
    // test various conditions for anticodon qualifiers

    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table8);
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 4);

    // expect no quals
    set<string> expected_quals;

    // expected amino acids
    int expected_aas[] = {
        // implicit char to int casts
        'H', 'P', 'A', 'C'
    };

    const char *pchExpectedAnticodonLocations[] = {
        "Seq-loc ::= int { from 20, to 22, id local str \"seq1\" }",
        "Seq-loc ::= int { from 120, to 122, strand minus, id local str \"seq1\" }",
        "Seq-loc ::= mix { pnt { point 209, id local str \"seq1\" }, \
               int { from 219, to 220, id local str \"seq1\" } }",
        "Seq-loc ::= mix { int { from 319, to 320, strand minus, id local str \"seq1\" }, \
               pnt { point 309, strand minus, id local str \"seq1\" } }"
    };

    size_t pos = 0;
    ITERATE(CSeq_annot::TData::TFtable, feat, ftable) {
        const CRNA_ref & trna_ref = (*feat)->GetData().GetRna();
        BOOST_CHECK_EQUAL( trna_ref.GetType(), CRNA_ref::eType_tRNA );
        CheckExpectedQuals (*feat, expected_quals);

        const CTrna_ext & trna_ext = trna_ref.GetExt().GetTRNA();

        BOOST_CHECK_EQUAL(trna_ext.GetAa().GetNcbieaa(), expected_aas[pos] );

        CRef<CSeq_loc> pExpectedAnticodonLoc( new CSeq_loc );
        CNcbiIstrstream strmAnticodonLoc(pchExpectedAnticodonLocations[pos]);
        BOOST_REQUIRE_NO_THROW( strmAnticodonLoc >> MSerial_AsnText >> *pExpectedAnticodonLoc );

        if( ! trna_ext.GetAnticodon().Equals(*pExpectedAnticodonLoc) ) {
            BOOST_ERROR( "Anticodon mismatch: \n"
                << "Received: " 
                << MSerial_AsnText << trna_ext.GetAnticodon() << "\n"
                << "\n"
                << "Expected: " 
                << MSerial_AsnText << *pExpectedAnticodonLoc );
        }

        ++pos;
    }
}

static const char * sc_Table9 = "\
>Feature lcl|seq1\n\
1\t10\ttRNA\n\
20\t30\n\
\t\t\tanticodon\t(pos:join(10..10,complement(20..21)),aa:Pro)\n\
";

BOOST_AUTO_TEST_CASE(Test_ForbidMixedStrandAnticodonQualifier)
{
    TErrList expected_errors;
    expected_errors.push_back(ILineError::eProblem_QualifierBadValue);

    CRef<CSeq_annot> annot = s_ReadOneTableFromString (
        sc_Table9,
        expected_errors);
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 1);

    // expect no quals
    set<string> expected_quals;
    CheckExpectedQuals (ftable.front(), expected_quals);

    const CRNA_ref & trna_ref = ftable.front()->GetData().GetRna();
    BOOST_CHECK_EQUAL( trna_ref.GetType(), CRNA_ref::eType_tRNA );

    const CTrna_ext & trna_ext = trna_ref.GetExt().GetTRNA();
    BOOST_CHECK( ! trna_ext.IsSetAa() );
    BOOST_CHECK( ! trna_ext.IsSetAnticodon() );
}

// each CDS has a note on it indicating
// whether or not it should have an
// error if fCDSsMustBeInTheirGenes is set.
static const char * sc_Table10 = "\
>Feature lcl|seq1\n\
1\t100\tgene\n\
\t\t\tgene\tSOME_GENE\n\
50\t200\tgene\n\
\t\t\tgene\tANOTHER_GENE\n\
1\t100\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\tshould be okay\n\
20\t70\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\tshould be okay\n\
70\t150\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\tshould have error\n\
2\t100\tCDS\n\
\t\t\tgene\tANOTHER_GENE\n\
\t\t\tnote\tshould have error\n\
21\t70\tCDS\n\
\t\t\tgene\tANOTHER_GENE\n\
\t\t\tnote\tshould have error\n\
71\t150\tCDS\n\
\t\t\tgene\tANOTHER_GENE\n\
\t\t\tnote\tshould be okay\n\
60\t80\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\tshould be okay\n\
61\t80\tCDS\n\
\t\t\tgene\tANOTHER_GENE\n\
\t\t\tnote\tshould be okay\n\
";

BOOST_AUTO_TEST_CASE(TestCDSInGenesCheck)
{
    // count how many times the word "error" appears in sc_Table10
    // to determine how many errors we expect
    vector<size_t> linesWithError;
    s_ListLinesWithPattern(sc_Table10, "error", linesWithError);
    BOOST_REQUIRE( ! linesWithError.empty() );

    TErrList expected_errors;
    fill_n( back_inserter(expected_errors), 
        linesWithError.size(), ILineError::eProblem_FeatMustBeInXrefdGene);

    CMessageListenerLenientIgnoreProgress err_container;

    CRef<CSeq_annot> annot = s_ReadOneTableFromString (
        sc_Table10,
        expected_errors,
        CFeature_table_reader::fCDSsMustBeInTheirGenes,
        &err_container );
    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();
    BOOST_CHECK_EQUAL(ftable.size(), 
        s_CountOccurrences(sc_Table10, "gene\n") +
            s_CountOccurrences(sc_Table10, "CDS\n") );

    BOOST_REQUIRE_EQUAL(err_container.Count(), linesWithError.size());
    ITERATE_0_IDX(ii, err_container.Count()) {
        const ILineError& line_error = err_container.GetError(ii);
        // (The "2" is because the error line is 2 lines down from the CDS's start line)
        BOOST_CHECK_EQUAL( line_error.Line(), linesWithError[ii] - 2 );
        BOOST_CHECK( ! line_error.OtherLines().empty());
    }
}

static const char * sc_Table11 = "\
>Feature lcl|seq1\n\
1\t100\tgene\n\
\t\t\tgene\tSOME_GENE\n\
20\t70\tCDS\n\
\t\t\tnote\tokay\n\
150\t200\tCDS\n\
\t\t\tnote\tokay\n\
30\t80\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\tokay\n\
75\t400\tCDS\n\
\t\t\tgene\tSOME_GENE\n\
\t\t\tnote\terror_if_checking_bounds\n\
40\t90\tCDS\n\
\t\t\tgene\tCREATED_GENE_1\n\
\t\t\tnote\tokay\n\
80\t300\tCDS\n\
\t\t\tgene\tCREATED_GENE_2\n\
\t\t\tnote\tokay\n\
200\t250\tCDS\n\
\t\t\tgene\tCREATED_GENE_2\n\
\t\t\tnote\tokay\n\
50\t300\tCDS\n\
\t\t\tgene\tCREATED_GENE_2\n\
\t\t\tnote\terror_if_checking_bounds\n\
";

BOOST_AUTO_TEST_CASE(TestCreateGenesFromCDSs)
{
    set<string> geneNamesExpected;
    geneNamesExpected.insert("SOME_GENE");
    geneNamesExpected.insert("CREATED_GENE_1");
    geneNamesExpected.insert("CREATED_GENE_2");

    vector<string> geneXrefExpectedOnEachCDS; //empty str if no xref
    // the code in these braces just sets geneExpectedOnEachCDS
    {{
        vector<CTempString> cdsSplitPieces;
        NStr::TokenizePattern(sc_Table11, "CDS", cdsSplitPieces);

        CTempString kStartOfGene("gene\t");

        ITERATE_0_IDX(ii, cdsSplitPieces.size() ) {
            if( 0 == ii ) {
                continue; // the first part is not CDS info
            }
            CTempString sCDSInfo = cdsSplitPieces[ii];
            NStr::TruncateSpacesInPlace(sCDSInfo, NStr::eTrunc_Begin);

            // extract sGeneLocus (if any) from the
            // sCDSInfo
            CTempString sGeneLocus;
            if( NStr::StartsWith(sCDSInfo, kStartOfGene) ) {
                sGeneLocus = sCDSInfo.substr(
                    kStartOfGene.length());
                SIZE_TYPE sz1stEndlinePos = sGeneLocus.find_first_of("\r\n");
                if( sz1stEndlinePos != NPOS ) {
                    sGeneLocus = sGeneLocus.substr(0, sz1stEndlinePos);
                }
                NStr::TruncateSpacesInPlace(sGeneLocus);
                BOOST_CHECK( ! sGeneLocus.empty() );
                // make sure no spaces
                BOOST_CHECK( sGeneLocus.end() == 
                    find_if(sGeneLocus.begin(), sGeneLocus.end(), ::isspace ) );
            }
            geneXrefExpectedOnEachCDS.push_back(sGeneLocus);
        }
    }}

    ITERATE_BOTH_BOOL_VALUES(bCheckIfCDSInItsGene) {
        cout << "Testing with bCheckIfCDSInItsGene = " 
             << NStr::BoolToString(bCheckIfCDSInItsGene) << endl;

        TErrList expected_errors;
        vector<size_t> linesWithError;
        CFeature_table_reader::TFlags readfeat_flags = 
            CFeature_table_reader::fCreateGenesFromCDSs;

        if( bCheckIfCDSInItsGene ) {
            expected_errors.push_back(
                ILineError::eProblem_FeatMustBeInXrefdGene );
            linesWithError.push_back(11);

            readfeat_flags |= CFeature_table_reader::fCDSsMustBeInTheirGenes;
        }
        expected_errors.push_back(
            ILineError::eProblem_CreatedGeneFromMultipleFeats );
        linesWithError.push_back(20);
        expected_errors.push_back(
            ILineError::eProblem_CreatedGeneFromMultipleFeats );
        linesWithError.push_back(23);
        if( bCheckIfCDSInItsGene ) {
            expected_errors.push_back(
                ILineError::eProblem_FeatMustBeInXrefdGene);
            linesWithError.push_back(23);
        }

        CMessageListenerLenientIgnoreProgress err_container;

        CRef<CSeq_annot> annot = s_ReadOneTableFromString (
            sc_Table11,
            expected_errors,
            readfeat_flags,
            &err_container );
        typedef CSeq_annot::TData::TFtable TFtable;
        const TFtable& ftable = annot->GetData().GetFtable();
        BOOST_CHECK_EQUAL(ftable.size(), 
            geneNamesExpected.size() +
            s_CountOccurrences(sc_Table11, "CDS\n") );

        BOOST_REQUIRE_EQUAL(err_container.Count(), linesWithError.size());
        ITERATE_0_IDX(ii, err_container.Count()) {
            const ILineError& line_error = err_container.GetError(ii);
            // (The "2" is because the error line is 2 lines down from the CDS's start line)
            BOOST_CHECK_EQUAL( line_error.Line(), linesWithError[ii] );
            // Other lines expected only for "CDS not in xref'd gene" error
            const size_t iNumOtherLinesExpected = ( 
                line_error.Line() != 23 &&
                line_error.Problem() == 
                ILineError::eProblem_FeatMustBeInXrefdGene ? 1 : 0 );
            BOOST_CHECK_EQUAL( line_error.OtherLines().size(), 
                iNumOtherLinesExpected );
        }

        // check that all genes were created
        vector<string> vecOfGenesInResult; // use a vector so we err on dupes
        vector<string> vecOfCDSXrefsInResult;
        ITERATE(TFtable, feat_it, ftable) {
            const CSeq_feat & feat = **feat_it;
            if( FIELD_IS_SET_AND_IS(feat, Data, Gene) ) {
                BOOST_CHECK_NO_THROW( vecOfGenesInResult.push_back(
                    feat.GetData().GetGene().GetLocus() ) );
                NCBITEST_CHECK( 
                    RAW_FIELD_IS_EMPTY_OR_UNSET(
                        feat.GetData().GetGene(), Locus_tag));
            } else if( FIELD_IS_SET_AND_IS(feat, Data, Cdregion) ) {
                const CGene_ref * pCDSGeneXref = feat.GetGeneXref();
                if( pCDSGeneXref ) {
                    BOOST_CHECK_NO_THROW( vecOfCDSXrefsInResult.push_back(
                        pCDSGeneXref->GetLocus() ) );
                } else {
                    vecOfCDSXrefsInResult.push_back(kEmptyStr);
                }
            }
        }
        // sort, but don't remove dupes so we can detect them
        sort( vecOfGenesInResult.begin(), vecOfGenesInResult.end() );
        BOOST_CHECK_EQUAL_COLLECTIONS(
            vecOfGenesInResult.begin(), vecOfGenesInResult.end(),
            geneNamesExpected.begin(), geneNamesExpected.end() );

        // check that each CDS references the correct gene
        // (do NOT sort or unique, because order matters)
        BOOST_CHECK_EQUAL_COLLECTIONS(
            vecOfCDSXrefsInResult.begin(), vecOfCDSXrefsInResult.end(),
            geneXrefExpectedOnEachCDS.begin(),geneXrefExpectedOnEachCDS.end());
    }
}

static const char * sc_Table12 = "\
>Feature lcl|Seq1\n\
1\t20\tgene\n\
\t\t\tgene g0\n\
[offset=7]\n\
1\t20\tgene\n\
31\t41\n\
\t\t\tgene g1\n\
>Feature lcl|Seq2\n\
1\t20\tgene\n\
\t\t\tgene g2\n\
30\t40\tgene\n\
\t\t\tgene g3\n\
[offset=0]\n\
40\t50\tgene\n\
\t\t\tgene g4\n\
[offset=-30]\n\
40\t50\tgene\n\
\t\t\tgene g5\n\
[offset=abc]\n\
55\t45\tgene\n\
\t\t\tgene g6\n\
[nonsense=foo]\n\
55\t65\tgene\n\
\t\t\tgene g7\n\
";

BOOST_AUTO_TEST_CASE(TestOffsetCommand)
{
    TErrList expected_errors;
    fill_n( back_inserter(expected_errors),
        3, ILineError::eProblem_UnrecognizedSquareBracketCommand);

    TAnnotRefListPtr pAnnotRefList =
        s_ReadMultipleTablesFromString(
        sc_Table12,
        expected_errors );
    BOOST_REQUIRE_EQUAL(pAnnotRefList->size(), 2);

    // merge ftables to simplify logic below
    CSeq_annot::TData::TFtable merged_ftables;
    ITERATE( TAnnotRefList, annot_ref_it, *pAnnotRefList ) {
        const CSeq_annot::TData::TFtable& an_ftable =
            (*annot_ref_it)->GetData().GetFtable();
        copy( an_ftable.begin(), an_ftable.end(),
            back_inserter(merged_ftables) );
    }
    
    // check that gene offsets are correct
    typedef SStaticPair<TSeqPos, TSeqPos> TGeneExtremes;
    TGeneExtremes gene_extremes_arr[] = { // 1-based, biological extremes
        {1, 20},
        {8, 48}, // Note: multi-interval
        {1, 20},
        {30, 40},
        {40, 50},
        {40, 50},
        {55, 45}, // Note: complement
        {55, 65}
    };
    BOOST_REQUIRE_EQUAL( 
        ArraySize(gene_extremes_arr), merged_ftables.size() );

    CSeq_annot::TData::TFtable::const_iterator merged_ftables_it = 
        merged_ftables.begin();
    ITERATE_0_IDX(ii, merged_ftables.size() ) {
        BOOST_CHECK( FIELD_IS_SET_AND_IS(**merged_ftables_it, Data, Gene) );

        const CSeq_loc & gene_loc = (*merged_ftables_it)->GetLocation();
        BOOST_CHECK_EQUAL( 
            gene_loc.GetStart(eExtreme_Biological), 
            (gene_extremes_arr[ii].first - 1) );
        BOOST_CHECK_EQUAL( 
            gene_loc.GetStop(eExtreme_Biological), 
            (gene_extremes_arr[ii].second - 1) );

        ++merged_ftables_it;
    }
}

// note the "END" buried in the string,
// which should be replaced in code that uses sc_Table13
static const char * sc_Table13 = "\
>Feature lcl|Seq1\n\
1\t20\tgene\n\
\t\t\tgene g0\n\
17^\tEND\tvariation\n\
\t\t\treplace\tCCT\n\
";

BOOST_AUTO_TEST_CASE(TestBetweenBaseIntervals)
{
    ITERATE_BOTH_BOOL_VALUES(bGoodLoc) {
        cerr << "Testing with bGoodLoc = " << NStr::BoolToString(bGoodLoc) << endl;

        const char * pchEndVal = (bGoodLoc ? "18" : "19");

        TErrList errList;
        if( ! bGoodLoc ) {
            errList.push_back(ILineError::eProblem_BadFeatureInterval);
        }

        CRef<CSeq_annot> pSeqAnnot =
            s_ReadOneTableFromString(
                NStr::Replace(sc_Table13, "END", pchEndVal).c_str(),
                errList );
        const CSeq_annot::TData::TFtable& ftable = 
            pSeqAnnot->GetData().GetFtable();

        BOOST_REQUIRE(ftable.size() == 2);
        BOOST_REQUIRE(ftable.front()->IsSetData());
        BOOST_REQUIRE(ftable.front()->GetData().IsGene());
        BOOST_REQUIRE(ftable.back()->IsSetData());
        BOOST_REQUIRE_EQUAL(ftable.back()->GetData().GetImp().GetKey(), "variation");
    }
}

// For example, this should accept ">    Feature abc"
BOOST_AUTO_TEST_CASE(TestSpacesBeforeFeature)
{
    ITERATE_0_IDX(num_spaces, 3) {
        string sTable = 
            ">" + string(num_spaces, ' ') + "Feature lcl|Seq1\n"
            "1\t20\tgene\n"
            "\t\t\tgene g0\n";

        CRef<CSeq_annot> pSeqAnnot =
            s_ReadOneTableFromString(
            sTable.c_str(),
            TErrList() );
        const CSeq_annot::TData::TFtable& ftable =
            pSeqAnnot->GetData().GetFtable();
        BOOST_REQUIRE(ftable.size() == 1);
    }
}

static const char * sc_Table14 = "\
>Feature lcl|Seq1\n\
1\t1008\tCDS\n\
\t\t\tgene    THE_GENE_NAME\n\
50\t200\n\
\t\t\tproduct THE_GENE_PRODUCT\n\
\n\
";

BOOST_AUTO_TEST_CASE(TestErrorIfRangeAfterQuals)
{
    TErrList expected_errors;
    expected_errors.push_back(ILineError::eProblem_NoFeatureProvidedOnIntervals );

    s_ReadOneTableFromString(
        sc_Table14,
        expected_errors );
}

static const char * sc_Table15 = "\
>Features_blah_blah lcl|Seq1\n\
1\t1008\tCDS\n\
\t\t\tgene    THE_GENE_NAME\n\
\n\
";

BOOST_AUTO_TEST_CASE(TestIfHandlesWhenJunkAfterFeature)
{
    s_ReadOneTableFromString(
        sc_Table15 );
}


static const char * sc_Table16 = "\
>Feature lcl|seq1\n\
<1\t32\trRNA\n\
\t\tProduct\t18S ribosomal RNA\n\
33\t170\tmisc_RNA\n\
\t\tProduct\tinternal transcribed spacer 1\n\
\n\
";

BOOST_AUTO_TEST_CASE(TestCaseInsensitivity)
{
    CRef<CSeq_annot> annot = s_ReadOneTableFromString (sc_Table16);
    SerializeOutIfVerbose("TestCaseInsensitivity initial annot", *annot);

    const CSeq_annot::TData::TFtable& ftable = annot->GetData().GetFtable();

    BOOST_REQUIRE_EQUAL(ftable.size(), 2);
    BOOST_REQUIRE(ftable.front()->IsSetData());
    BOOST_REQUIRE(ftable.back()->IsSetData());

    // make sure rRNA products are processed right, just in case
    const CSeqFeatData & feat_data_0 = ftable.front()->GetData();
    BOOST_CHECK_EQUAL(
        feat_data_0.GetSubtype(), CSeqFeatData::eSubtype_rRNA);
    BOOST_CHECK_EQUAL(
        feat_data_0.GetRna().GetExt().GetName(), "18S ribosomal RNA");

    const CSeqFeatData & feat_data_1 = ftable.back()->GetData();
    // make sure it's a miscRNA
    BOOST_CHECK_EQUAL(
        feat_data_1.GetSubtype(),
        CSeqFeatData::eSubtype_otherRNA // subtype otherRNA is used for miscRNA
    );
    // The "Product" in the input table should have become "product" in the
    // output
    BOOST_CHECK_EQUAL(
        ftable.back()->GetNamedQual("Product"), "");
    BOOST_CHECK_EQUAL(
        ftable.back()->GetNamedQual("product"),
        "internal transcribed spacer 1");
}


static const char * sc_Table17 = "\
>Feature lcl|seq1\n\
<1\t32\tsnoRNA\n\
\t\t\tnote\tHello, this is a note.\n\
33\t170\tmobile_element\n\
\t\ttransposon\tSomeTransposon\n\
\n\
";


BOOST_AUTO_TEST_CASE(TestDiscouragedKeys)
{
    TErrList expected_errors;
    expected_errors.push_back(
        ILineError::eProblem_DiscouragedFeatureName);
    expected_errors.push_back(
        ILineError::eProblem_DiscouragedQualifierName);

    CRef<CSeq_annot> annot = s_ReadOneTableFromString (
        sc_Table17,
        expected_errors,
        CFeature_table_reader::fReportDiscouragedKey);
        // &err_container);
}

struct SRegulatoryFeatSubtypeCases {
    CSeqFeatData::ESubtype subtype;
    bool                   is_regulatory;
};
const SRegulatoryFeatSubtypeCases subtype_cases [] = {
    { CSeqFeatData::eSubtype_regulatory, true },
    // This is a special case which a special string translation
    { CSeqFeatData::eSubtype_10_signal, false },
    // Not a special case; just uses eSubtype_10_signal
    { CSeqFeatData::eSubtype_CAAT_signal, false },
};


BOOST_AUTO_TEST_CASE(TestRegulatoryFeat)
{
    // set what constitutes a valid vs. invalid regulatory_class
    // (note that at this time we're not testing the case of a
    // mismatch between the subtype and qual value)
    const string kInvalidRegulatoryClass("foo");
    const string & kValidRegulatoryClass =
        CSeqFeatData::GetRegulatoryClass(
            CSeqFeatData::eSubtype_10_signal);
    BOOST_REQUIRE( ! kValidRegulatoryClass.empty() );

    ITERATE_0_IDX(subtype_case_idx, ArraySize(subtype_cases)) {
        ITERATE_BOTH_BOOL_VALUES(bUseValidRegulatoryClass) {
            TErrList expected_errors;

            const SRegulatoryFeatSubtypeCases & subtype_case =
                subtype_cases[subtype_case_idx];

            const CSeqFeatData::ESubtype subtype = subtype_case.subtype;
            const string & subtype_name =
                CSeqFeatData::SubtypeValueToName(subtype);

            cout << "Test case: use subtype: "
                 << subtype_name
                 << ", use valid regulatory_class: "
                 << NStr::BoolToString(bUseValidRegulatoryClass) << endl;

            const bool bUseRegulatorySubtypeItself =
                subtype_case.is_regulatory;
            if( ! bUseRegulatorySubtypeItself ) {
                expected_errors.push_back(
                    ILineError::eProblem_DiscouragedFeatureName);
            }

            const string & qual_val = (
                bUseValidRegulatoryClass ?
                kValidRegulatoryClass :
                kInvalidRegulatoryClass );
            if( ! bUseValidRegulatoryClass ) {
                expected_errors.push_back(
                    ILineError::eProblem_QualifierBadValue);
            }

            // build the feat table
            string feat_table;
            feat_table += ">Feature lcl|seq1\n";
            feat_table += "<1\t32\t" + subtype_name + '\n';
            feat_table += "\t\t\tregulatory_class\t" + qual_val + '\n';

            CRef<CSeq_annot> annot = s_ReadOneTableFromString (
                feat_table.c_str(), expected_errors,
                CFeature_table_reader::fReportDiscouragedKey);
            // &err_container);

            BOOST_CHECK( annot );
        }
    }
}
