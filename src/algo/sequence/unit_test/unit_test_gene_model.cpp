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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/validator/validator.hpp>
#include <objmgr/scope.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>

#include <algo/sequence/gene_model.hpp>

#include <boost/test/output_test_stream.hpp> 
using boost::test_tools::output_test_stream;

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("data-expected", "InputData",
                     "Expected Seq-annots produced from input alignments",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey("seqdata-expected", "InputData",
                     "Expected bioseqs produced from input alignments",
                     CArgDescriptions::eString);
    arg_desc->AddKey("combined-data-expected", "InputData",
                     "Expected single seq-annot produced from all input alignments",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey("combined-with-omission-expected", "InputData",
                     "Expected single seq-annot produced from all input alignments omitting the first RNA feature",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("seqdata-in", "InputData",
                     "FASTA of test sequences",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("data-out", "OutputData",
                     "Seq-annots produced from input alignments",
                     CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("seqdata-out", "OutputData",
                     "Bioseqss produced from input alignments",
                     CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("combined-data-out", "OutputData",
                     "Single seq-annot produced from all input alignments",
                     CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("combined-with-omission-out", "OutputData",
                     "Single seq-annot produced from all input alignments omitting the first RNA feature",
                     CArgDescriptions::eOutputFile);
}

/// Function to compare Cref<Cseq_feat>s by their referents
static bool s_CompareFeatRefs(const CRef<CSeq_feat>& ref1,
                              const CRef<CSeq_feat>& ref2)
{
    return *ref1 < *ref2;
}

void s_CompareFtables(const CSeq_annot::TData::TFtable &actual,
                      const CSeq_annot::TData::TFtable &expected,
                      const string& compared_features)
{
        CSeq_annot::TData::TFtable::const_iterator actual_iter =
            actual.begin();

        CSeq_annot::TData::TFtable::const_iterator expected_iter =
            expected.begin();

        for ( ;  actual_iter != actual.end()  &&  expected_iter != expected.end();
              ++actual_iter, ++expected_iter) {

            bool display = false;
            const CSeq_feat& f1 = **actual_iter;
            const CSeq_feat& f2 = **expected_iter;
            BOOST_CHECK_MESSAGE(f1.GetData().GetSubtype() == f2.GetData().GetSubtype(),
                                compared_features << ": f1.GetData().GetSubtype() == f2.GetData().GetSubtype() failed ["
                                << f1.GetData().GetSubtype() << " != " << f2.GetData().GetSubtype() << "]");
            BOOST_CHECK_MESSAGE(f1.GetLocation().Equals(f2.GetLocation()),
                                compared_features << ": f1.GetLocation().Equals(f2.GetLocation() failed");
            if ( !f1.GetLocation().Equals(f2.GetLocation()) ) {
                display = true;
            }

            BOOST_CHECK_MESSAGE((f1.IsSetPartial()  &&  f1.GetPartial()) ==
                                (f2.IsSetPartial()  &&  f2.GetPartial()),
                                compared_features << ": (f1.IsSetPartial()  &&  f1.GetPartial()) == (f2.IsSetPartial()  &&  f2.GetPartial()) failed");
            if ( (f1.IsSetPartial()  &&  f1.GetPartial()) !=
                 (f2.IsSetPartial()  &&  f2.GetPartial()) ) {
                display = true;
            }

            BOOST_CHECK_EQUAL(f1.IsSetPseudo()  &&  f1.GetPseudo(),
                              f2.IsSetPseudo()  &&  f2.GetPseudo());
            if ( (f1.IsSetPseudo()  &&  f1.GetPseudo()) !=
                 (f2.IsSetPseudo()  &&  f2.GetPseudo()) ) {
                display = true;
            }

            BOOST_CHECK_EQUAL(f1.IsSetDbxref(), f2.IsSetDbxref());
            if ( f1.IsSetDbxref() != f2.IsSetDbxref() ) {
                display = true;
            }

            BOOST_CHECK_EQUAL(f1.IsSetProduct(), f2.IsSetProduct());
            if (f1.IsSetProduct() && f2.IsSetProduct()) {
                BOOST_CHECK(f1.GetProduct().Equals(f2.GetProduct()));
            }

            BOOST_CHECK_EQUAL(f1.IsSetPseudo(), f2.IsSetPseudo());
            if ( f1.IsSetPseudo() != f2.IsSetPseudo() ) {
                display = true;
            }

            if (f1.GetData().IsCdregion()  &&
                f2.GetData().IsCdregion()) {
                BOOST_CHECK(f1.GetData().GetCdregion().IsSetCode_break() ==
                            f2.GetData().GetCdregion().IsSetCode_break());
                if (f1.GetData().GetCdregion().IsSetCode_break() !=
                    f2.GetData().GetCdregion().IsSetCode_break()) {
                    display = true;
                } else if (f1.GetData().GetCdregion().IsSetCode_break()) {
                    CNcbiOstrstream stream1;
                    ITERATE (CCdregion::TCode_break, cb, f1.GetData().GetCdregion().GetCode_break())
                        stream1 << MSerial_AsnText << **cb;
                    string code_break1 = CNcbiOstrstreamToString(stream1);

                    CNcbiOstrstream stream2;
                    ITERATE (CCdregion::TCode_break, cb, f2.GetData().GetCdregion().GetCode_break())
                        stream2 << MSerial_AsnText << **cb;
                    string code_break2 = CNcbiOstrstreamToString(stream2);

                    BOOST_CHECK_EQUAL(code_break1, code_break2);
                }
            }

            if(f1.GetData().IsGene() && f2.GetData().IsGene()){
                BOOST_CHECK(f1.GetData().GetGene().IsSetLocus() ==
                            f2.GetData().GetGene().IsSetLocus());
                if (f1.GetData().GetGene().IsSetLocus() !=
                    f2.GetData().GetGene().IsSetLocus()) {
                    display = true;
                }
                if(f1.GetData().GetGene().IsSetLocus() &&
                   f2.GetData().GetGene().IsSetLocus()){
                    BOOST_CHECK(f1.GetData().GetGene().GetLocus() ==
                                f2.GetData().GetGene().GetLocus());
                    if (f1.GetData().GetGene().GetLocus() !=
                        f2.GetData().GetGene().GetLocus()) {
                        display = true;
                    }
                }
                BOOST_CHECK(f1.GetData().GetGene().IsSetDesc() ==
                            f2.GetData().GetGene().IsSetDesc());
                if (f1.GetData().GetGene().IsSetDesc() !=
                    f2.GetData().GetGene().IsSetDesc()) {
                    display = true;
                }
                if(f1.GetData().GetGene().IsSetDesc() &&
                   f2.GetData().GetGene().IsSetDesc()){
                    BOOST_CHECK(f1.GetData().GetGene().GetDesc() ==
                                f2.GetData().GetGene().GetDesc());
                    if (f1.GetData().GetGene().GetDesc() !=
                        f2.GetData().GetGene().GetDesc()) {
                        display = true;
                    }
                }
                BOOST_CHECK(f1.GetData().GetGene().IsSetSyn() ==
                            f2.GetData().GetGene().IsSetSyn());
                if (f1.GetData().GetGene().IsSetSyn() !=
                    f2.GetData().GetGene().IsSetSyn()) {
                    display = true;
                }
                if(f1.GetData().GetGene().IsSetSyn() &&
                   f2.GetData().GetGene().IsSetSyn()){
                    BOOST_CHECK(f1.GetData().GetGene().GetSyn() ==
                                f2.GetData().GetGene().GetSyn());
                    if (f1.GetData().GetGene().GetSyn() !=
                        f2.GetData().GetGene().GetSyn()) {
                        display = true;
                    }
                }
            }

            bool f1_except = f1.IsSetExcept()  &&  f1.GetExcept();
            bool f2_except = f2.IsSetExcept()  &&  f2.GetExcept();

            BOOST_CHECK_EQUAL(f1_except, f2_except);

            string f1_except_text =
                f1.IsSetExcept_text() ? f1.GetExcept_text() : kEmptyStr;
            string f2_except_text =
                f2.IsSetExcept_text() ? f2.GetExcept_text() : kEmptyStr;
            BOOST_CHECK_EQUAL(f1_except_text, f2_except_text);

            BOOST_CHECK(f1.IsSetComment() == f2.IsSetComment());
            if (f1.IsSetComment() != f2.IsSetComment()) {
                display = true;
            }
            if(f1.IsSetComment() && f2.IsSetComment()){
                BOOST_CHECK(f1.GetComment() == f2.GetComment());
                if (f1.GetComment() != f2.GetComment()) {
                    display = true;
                }
            }

            if (display) {
                cerr << "expected: " << MSerial_AsnText << f2;
                cerr << "got: " << MSerial_AsnText << f1;
            }
        }
}

void AddFastaToScope(const string& fasta_file, CScope& scope)
{
        CFastaReader fastareader(fasta_file, CFastaReader::fParseRawID);
        do {
            CRef<CSeq_entry> seq_entry = fastareader.ReadOneSeq();
            scope.AddTopLevelSeqEntry(*seq_entry);
        } while(!fastareader.AtEOF());
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();
    validator::CValidator validator(*om);

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& align_istr = args["data-in"].AsInputFile();
    CNcbiIstream& annot_istr = args["data-expected"].AsInputFile();
    CNcbiIstream& combined_annot_istr = args["combined-data-expected"].AsInputFile();
    CNcbiIstream& combined_annot_with_omission_istr = args["combined-with-omission-expected"].AsInputFile();

    auto_ptr<CObjectIStream> align_is(CObjectIStream::Open(eSerial_AsnText,
                                                           align_istr));
    auto_ptr<CObjectIStream> annot_is(CObjectIStream::Open(eSerial_AsnText,
                                                           annot_istr));
    auto_ptr<CObjectIStream> combined_annot_is(CObjectIStream::Open(eSerial_AsnText,
                                                           combined_annot_istr));
    auto_ptr<CObjectIStream> combined_annot_with_omission_is(
                                   CObjectIStream::Open(eSerial_AsnText,
                                   combined_annot_with_omission_istr));
    auto_ptr<CObjectOStream> annot_os;
    if (args["data-out"]) {
        CNcbiOstream& annot_ostr = args["data-out"].AsOutputFile();
        annot_os.reset(CObjectOStream::Open(eSerial_AsnText,
                                            annot_ostr));
    }
    auto_ptr<CObjectOStream> combined_annot_os;
    if (args["combined-data-out"]) {
        CNcbiOstream& combined_annot_ostr = args["combined-data-out"].AsOutputFile();
        combined_annot_os.reset(CObjectOStream::Open(eSerial_AsnText,
                                            combined_annot_ostr));
    }
    auto_ptr<CObjectOStream> combined_annot_with_omission_os;
    if (args["combined-with-omission-out"]) {
        CNcbiOstream& combined_annot_with_omission_ostr = args["combined-with-omission-out"].AsOutputFile();
        combined_annot_with_omission_os.reset(CObjectOStream::Open(eSerial_AsnText,
                                            combined_annot_with_omission_ostr));
    }
    output_test_stream seqdata_test_stream( args["seqdata-expected"].AsString(), true );
    auto_ptr<CObjectOStream> seqdata_test_os(CObjectOStream::Open(eSerial_AsnText,
                                                                  seqdata_test_stream));
    auto_ptr<CObjectOStream> seqdata_os;
    if (args["seqdata-out"]) {
        CNcbiOstream& seqdata_ostr = args["seqdata-out"].AsOutputFile();
        seqdata_os.reset(CObjectOStream::Open(eSerial_AsnText,
                                            seqdata_ostr));
    }

    if (args["seqdata-in"]) {
        AddFastaToScope(args["seqdata-in"].AsString(), scope);
    }

    CSeq_annot actual_combined_annot;
    CSeq_annot::C_Data::TFtable &actual_combined_features = 
            actual_combined_annot.SetData().SetFtable();
    CSeq_annot expected_combined_annot;
    *combined_annot_is >> expected_combined_annot;
    CSeq_annot::C_Data::TFtable &expected_combined_features = 
            expected_combined_annot.SetData().SetFtable();
    CSeq_annot actual_combined_annot_with_omission;
    CSeq_annot::C_Data::TFtable &actual_combined_features_with_omission = 
            actual_combined_annot_with_omission.SetData().SetFtable();
    CSeq_annot expected_combined_annot_with_omission;
    *combined_annot_with_omission_is >> expected_combined_annot_with_omission;
    CSeq_annot::C_Data::TFtable &expected_combined_features_with_omission = 
            expected_combined_annot_with_omission.SetData().SetFtable();

    set< CSeq_id_Handle > unique_gene_ids;

    /// combined_aligns will contain all alignments read fro this specific gene
    list< CRef<CSeq_align> > combined_aligns;
    CSeq_id gene_for_combined_aligns;
    gene_for_combined_aligns.SetOther().SetAccession("NT_007933");
    gene_for_combined_aligns.SetOther().SetVersion(15);
    set<CSeq_id_Handle> genes_for_redo_partial;
    genes_for_redo_partial.insert(CSeq_id_Handle::GetHandle(gene_for_combined_aligns));
    genes_for_redo_partial.insert(CSeq_id_Handle::GetHandle("NT_011515.12"));
    genes_for_redo_partial.insert(CSeq_id_Handle::GetGiHandle(224514634));
    genes_for_redo_partial.insert(CSeq_id_Handle::GetGiHandle(258441149));

    const int default_flags =
        (CFeatureGenerator::fDefaults &
         ~CFeatureGenerator::fGenerateLocalIds) |
        CFeatureGenerator::fGenerateStableLocalIds |
        CFeatureGenerator::fForceTranslateCds | CFeatureGenerator::fForceTranscribeMrna;

    for (int alignment = 0; align_istr  &&  annot_istr; ++alignment) {

        CFeatureGenerator generator(scope);

        CRef<CSeq_align> align(new CSeq_align);
        CSeq_annot expected_annot;

        /// we wrap the first serialization in try/catch
        /// if this fails, we are at the end of the file, and we expect both to
        /// be at the end of the file.
        /// a failure in the second serialization is fatal
        try {
            *align_is >> *align;
        }
        catch (CEofException&) {
            try {
                *annot_is >> expected_annot;
            }
            catch (CEofException&) {
            }
            break;
        }

        cerr << "Alignment "<< alignment <<  endl;

        BOOST_CHECK_NO_THROW(align->Validate(true));


        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq_set& seqs = seq_entry->SetSet();
        seqs.SetSeq_set();
        CSeq_annot actual_annot;
        CSeq_annot::C_Data::TFtable &actual_features = 
            actual_annot.SetData().SetFtable();
        {
            generator.SetFlags(default_flags);
            TSeqRange adjust_range;
            CRef<CSeq_feat> feat;
            ITERATE (CSeq_align::TExt, ext_it, align->GetExt()) {
                if ((*ext_it)->GetType().IsStr() &&
                    (*ext_it)->GetType().GetStr() == "CFeatureGenerator") {
                    if ((*ext_it)->HasField("Flags")) {
                        int flags = (*ext_it)->GetField("Flags").GetData().GetInt();
                        generator.SetFlags(flags);
                    }
                    if ((*ext_it)->HasField("AdjustRange")) {
                        const vector<int>& range_vec = (*ext_it)->GetField("AdjustRange").GetData().GetInts();
                        adjust_range = TSeqRange(range_vec[0], range_vec[1]);
                        generator.SetAllowedUnaligned(0);
                    }
                    if ((*ext_it)->HasField("cdregion")) {
                        string cdregion = (*ext_it)->GetField("cdregion").GetData().GetStr();
                        CNcbiIstrstream istrs(cdregion.c_str());
                        CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);
                        feat.Reset(new CSeq_feat);
                        *istr >> *feat;
                    }
                }
            }

            CConstRef<CSeq_align> clean_align = generator.CleanAlignment(*align);

            if (adjust_range.NotEmpty()) {
                clean_align = generator.AdjustAlignment(*clean_align, adjust_range);
            }

            generator.ConvertAlignToAnnot(*clean_align, actual_annot, seqs, 0, feat.GetPointer());
//            BOOST_CHECK( validator.Validate(*seq_entry, &scope)->FatalSize()==0 );
            CSeq_id_Handle id = CSeq_id_Handle::GetHandle(*actual_features.front()->GetLocation().GetId());
            if(id == gene_for_combined_aligns)
                combined_aligns.push_back(align);

            NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, it, actual_features){
                /// Add to combined annot, unless this is a gene feature that
                /// was already added. Also, don't add the RNA feature from the
                /// very first alignment (to test recomputation of the partial flag
                /// for the gene)
                if (genes_for_redo_partial.count(id) &&
                   (!(*it)->GetData().IsGene() ||
                   unique_gene_ids.insert(id).second) &&
                   (!(*it)->GetData().IsRna() || alignment > 0))
                    actual_combined_features_with_omission.push_back(*it);
            }
        }

        if (annot_os.get() != NULL) {
            *annot_os << actual_annot;
        }
        if (seqdata_os.get() != NULL) {
            *seqdata_os << seqs;
        }

        *seqdata_test_os << seqs;
        BOOST_CHECK( seqdata_test_stream.match_pattern() );

        *annot_is >> expected_annot;
        const CSeq_annot::C_Data::TFtable &expected_features = 
            expected_annot.GetData().GetFtable();

        s_CompareFtables(actual_features, expected_features, "Main annotation");
    }

    CFeatureGenerator generator(scope);

    generator.SetFlags(default_flags);

    generator.RecomputePartialFlags(actual_combined_annot_with_omission);

    if (combined_annot_with_omission_os.get() != NULL) {
        *combined_annot_with_omission_os << actual_combined_annot_with_omission;
    }
    s_CompareFtables(actual_combined_features_with_omission, expected_combined_features_with_omission, "combined_features_with_omission");

    CBioseq_set seqs;
    generator.ConvertAlignToAnnot(combined_aligns, actual_combined_annot, seqs);

    // ConvertAlignToAnnot collates alignments by gene using unpredictable ordering over
    // SeqId handles; so order of features in result is unpredictable, we need to sort
    // them before comparison
    actual_combined_features.sort(s_CompareFeatRefs);
    expected_combined_features.sort(s_CompareFeatRefs);

    if(combined_annot_os.get() != NULL) {
        *combined_annot_os << actual_combined_annot;
    }
    s_CompareFtables(actual_combined_features, expected_combined_features, "combined_features");

    BOOST_CHECK(align_istr.eof());
    BOOST_CHECK(annot_istr.eof());
}

BOOST_AUTO_TEST_SUITE(TestSuiteTrimAlignment)

BOOST_AUTO_TEST_CASE(TestCaseTrimAlignmentCall)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);
    
    CSeq_align align;
    CConstRef<CSeq_align> trimmed_align;
    BOOST_CHECK_NO_THROW(
                         trimmed_align = feat_gen.CleanAlignment(align)
                         );
}

BOOST_AUTO_TEST_CASE(TestCaseStitch)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);
    
    CSeq_align align;
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;
    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetNucpos(0);
    exon->SetProduct_end().SetNucpos(100);
    exon->SetGenomic_start(0);
    exon->SetGenomic_end(100);
    exons.push_back(exon);
    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetNucpos(200);
    exon->SetProduct_end().SetNucpos(300);
    exon->SetGenomic_start(200);
    exon->SetGenomic_end(300);
    exons.push_back(exon);

    CConstRef<CSeq_align> trimmed_align;
    trimmed_align = feat_gen.CleanAlignment(align);

    BOOST_CHECK_EQUAL(trimmed_align->GetSegs().GetSpliced().GetExons().size(), size_t(1));

}

BOOST_AUTO_TEST_CASE(TestCaseTrim)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);
    
    CSeq_align align;
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("NM_018690.2"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("NT_010393.16"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetNucpos(10);
    exon->SetProduct_end().SetNucpos(11);
    exon->SetGenomic_start(20);
    exon->SetGenomic_end(21);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetNucpos(16);
    exon->SetProduct_end().SetNucpos(19);
    exon->SetGenomic_start(1031);
    exon->SetGenomic_end(1034);
    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(1);
    exon->SetParts().push_back(chunk);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetNucpos(200);
    exon->SetProduct_end().SetNucpos(300);
    exon->SetGenomic_start(2000);
    exon->SetGenomic_end(2100);
    exons.push_back(exon);

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CConstRef<CSeq_align> trimmed_align;
    trimmed_align = feat_gen.CleanAlignment(align);

    BOOST_CHECK_NO_THROW(trimmed_align->Validate(true));

    BOOST_CHECK_EQUAL(trimmed_align->GetSegs().GetSpliced().GetExons().size(), size_t(2));

    CSpliced_seg::TExons::const_iterator i = trimmed_align->GetSegs().GetSpliced().GetExons().begin();

    BOOST_CHECK_EQUAL((*i)->GetGenomic_start(), TSeqPos(1031) );
    BOOST_CHECK_EQUAL((*i)->GetGenomic_end(), TSeqPos(1032) );
    BOOST_CHECK_EQUAL((*++i)->GetGenomic_start(), TSeqPos(2002) );
}

BOOST_AUTO_TEST_CASE(TestCaseTrimProtein)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);
    
    CSeq_align align;
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("lcl|genomic"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(3);
    exon->SetProduct_start().SetProtpos().SetFrame(1);
    exon->SetProduct_end().SetProtpos().SetAmin(3);
    exon->SetProduct_end().SetProtpos().SetFrame(2);
    exon->SetGenomic_start(20);
    exon->SetGenomic_end(21);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(5);
    exon->SetProduct_start().SetProtpos().SetFrame(2);
    exon->SetProduct_end().SetProtpos().SetAmin(7);
    exon->SetProduct_end().SetProtpos().SetFrame(2);
    exon->SetGenomic_start(1031);
    exon->SetGenomic_end(1037);
    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(4);
    exon->SetParts().push_back(chunk);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(66);
    exon->SetProduct_start().SetProtpos().SetFrame(3);
    exon->SetProduct_end().SetProtpos().SetAmin(100);
    exon->SetProduct_end().SetProtpos().SetFrame(1);
    exon->SetGenomic_start(2000);
    exon->SetGenomic_end(2100);
    exons.push_back(exon);

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CConstRef<CSeq_align> trimmed_align;
    trimmed_align = feat_gen.CleanAlignment(align);

    BOOST_CHECK_NO_THROW(trimmed_align->Validate(true));

    BOOST_CHECK_EQUAL(trimmed_align->GetSegs().GetSpliced().GetExons().size(), size_t(2));

    CSpliced_seg::TExons::const_iterator i = trimmed_align->GetSegs().GetSpliced().GetExons().begin();

    BOOST_CHECK_EQUAL((*i)->GetGenomic_start(), TSeqPos(1032) );
    BOOST_CHECK_EQUAL((*i)->GetGenomic_end(), TSeqPos(1035) );
    BOOST_CHECK_EQUAL((*++i)->GetGenomic_start(), TSeqPos(2001) );
}

BOOST_AUTO_TEST_CASE(TestCaseStitchProtein)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);
    
    CSeq_align align;
    align.SetType(CSeq_align::eType_partial);
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("lcl|genomic"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(3);
    exon->SetProduct_start().SetProtpos().SetFrame(1);
    exon->SetProduct_end().SetProtpos().SetAmin(3);
    exon->SetProduct_end().SetProtpos().SetFrame(2);
    exon->SetGenomic_start(20);
    exon->SetGenomic_end(21);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(5);
    exon->SetProduct_start().SetProtpos().SetFrame(2);
    exon->SetProduct_end().SetProtpos().SetAmin(7);
    exon->SetProduct_end().SetProtpos().SetFrame(2);
    exon->SetGenomic_start(31);
    exon->SetGenomic_end(37);
    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetMatch(4);
    exon->SetParts().push_back(chunk);
    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(36);
    exon->SetProduct_start().SetProtpos().SetFrame(3);
    exon->SetProduct_end().SetProtpos().SetAmin(70);
    exon->SetProduct_end().SetProtpos().SetFrame(1);
    exon->SetGenomic_start(137);
    exon->SetGenomic_end(237);
    exons.push_back(exon);

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CConstRef<CSeq_align> modified_align;
    modified_align = feat_gen.CleanAlignment(align);

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));

    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(1));

    CSpliced_seg::TExons::const_iterator i = modified_align->GetSegs().GetSpliced().GetExons().begin();

    BOOST_CHECK_EQUAL((*i)->GetGenomic_start(), TSeqPos(20) );
    BOOST_CHECK_EQUAL((*i)->GetGenomic_end(), TSeqPos(237) );

    TSeqPos product_pos = 0;
    ITERATE(CSpliced_exon::TParts, p, (*i)->GetParts()) {
        const CSpliced_exon_chunk& chunk = **p;
        switch (chunk.Which()) {
        case CSpliced_exon_chunk::e_Match:
            product_pos += chunk.GetMatch();
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            product_pos += chunk.GetMismatch();
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            product_pos += chunk.GetProduct_ins();
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            if (chunk.GetGenomic_ins() > 1) {
                BOOST_CHECK_EQUAL(product_pos % 3, TSeqPos(0) );
            }
            break;
        default:
            break;
        }
    }


// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << *trimmed_align;
// delete ostr;

}

BOOST_AUTO_TEST_CASE(TestCaseExpandAlignment)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    for (int strand = -1 ; strand <= 1; strand +=2) {

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

    CSeq_align align;
    {
    align.SetType(CSeq_align::eType_partial);
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("lcl|genomic"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(3);
    exon->SetProduct_start().SetProtpos().SetFrame(3);
    exon->SetProduct_end().SetProtpos().SetAmin(75);
    exon->SetProduct_end().SetProtpos().SetFrame(1);
    exon->SetGenomic_start(22);
    exon->SetGenomic_end(237);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;

    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(7);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(204);
    exon->SetParts().push_back(chunk);

    exons.push_back(exon);
    }
    BOOST_CHECK_NO_THROW(align.Validate(true));

// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << align;

    CConstRef<CSeq_align> modified_align;
    TSeqRange range(8, 248);
    modified_align = feat_gen.AdjustAlignment(align, range);
// *ostr << *modified_align;

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(1));
    const CSpliced_exon& exon = **modified_align->GetSegs().GetSpliced().GetExons().begin();

    int cumulative_indel_len = 0;
    TSeqPos product_pos = 0;
    ITERATE(CSpliced_exon::TParts, p, exon.GetParts()) {
        const CSpliced_exon_chunk& chunk = **p;
        switch (chunk.Which()) {
        case CSpliced_exon_chunk::e_Match:
            product_pos += chunk.GetMatch();
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            product_pos += chunk.GetMismatch();
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            product_pos += chunk.GetProduct_ins();
            cumulative_indel_len += 1;
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            if (chunk.GetGenomic_ins() > 1) {
                BOOST_CHECK_EQUAL(product_pos % 3, TSeqPos(0) );
            }
            cumulative_indel_len -= 1;
            break;
        default:
            break;
        }
    }
    
    
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetProduct_length(), (range.GetLength()+cumulative_indel_len)/3 );

    BOOST_CHECK_EQUAL(exon.GetGenomic_start(), range.GetFrom() );
    BOOST_CHECK_EQUAL(exon.GetGenomic_end(), range.GetTo() );

    BOOST_CHECK_EQUAL(exon.GetProduct_start().GetProtpos().GetAmin(), unsigned(0) );
    BOOST_CHECK_EQUAL(exon.GetProduct_end().GetProtpos().GetAmin(), modified_align->GetSegs().GetSpliced().GetProduct_length() -1 );
    



// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << *trimmed_align;
//  delete ostr;
    }
}
BOOST_AUTO_TEST_CASE(TestCaseShrinkAlignment)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    for (int strand = -1 ; strand <= 1; strand +=2) {

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

    CSeq_align align;
    {
    align.SetType(CSeq_align::eType_partial);
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("lcl|genomic"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(3);
    exon->SetProduct_start().SetProtpos().SetFrame(3);
    exon->SetProduct_end().SetProtpos().SetAmin(75);
    exon->SetProduct_end().SetProtpos().SetFrame(1);
    exon->SetGenomic_start(22);
    exon->SetGenomic_end(237);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;

    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(6);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(204);
    exon->SetParts().push_back(chunk);

    exons.push_back(exon);
    }
    BOOST_CHECK_NO_THROW(align.Validate(true));

// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << align;

    CConstRef<CSeq_align> modified_align;
    TSeqRange range(23, 236);
    modified_align = feat_gen.AdjustAlignment(align, range);
// *ostr << *modified_align;

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(1));
    const CSpliced_exon& exon = **modified_align->GetSegs().GetSpliced().GetExons().begin();

    int cumulative_indel_len = 0;
    TSeqPos product_pos = 0;
    ITERATE(CSpliced_exon::TParts, p, exon.GetParts()) {
        const CSpliced_exon_chunk& chunk = **p;
        switch (chunk.Which()) {
        case CSpliced_exon_chunk::e_Match:
            product_pos += chunk.GetMatch();
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            product_pos += chunk.GetMismatch();
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            product_pos += chunk.GetProduct_ins();
            cumulative_indel_len += 1;
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            if (chunk.GetGenomic_ins() > 1) {
                BOOST_CHECK_EQUAL(product_pos % 3, TSeqPos(0) );
            }
            cumulative_indel_len -= 1;
            break;
        default:
            break;
        }
    }
    
    
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetProduct_length(), (range.GetLength()+cumulative_indel_len)/3 );

    BOOST_CHECK_EQUAL(exon.GetGenomic_start(), range.GetFrom() );
    BOOST_CHECK_EQUAL(exon.GetGenomic_end(), range.GetTo() );

    BOOST_CHECK_EQUAL(exon.GetProduct_start().GetProtpos().GetAmin(), unsigned(0) );
    BOOST_CHECK_EQUAL(exon.GetProduct_end().GetProtpos().GetAmin(), modified_align->GetSegs().GetSpliced().GetProduct_length() -1 );
    



// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << *trimmed_align;
//  delete ostr;
    }
}

BOOST_AUTO_TEST_CASE(TestCaseExpandAlignmentCrossOrigin)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    for (int strand = -1 ; strand <= 1; strand +=2) {

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

    CSeq_align align;
    {
    align.SetType(CSeq_align::eType_partial);
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("gi|17158061"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(3);
    exon->SetProduct_start().SetProtpos().SetFrame(3);
    exon->SetProduct_end().SetProtpos().SetAmin(75);
    exon->SetProduct_end().SetProtpos().SetFrame(1);
    exon->SetGenomic_start(10);
    exon->SetGenomic_end(225);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;

    CRef<CSpliced_exon_chunk> chunk;

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(7);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetProduct_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(2);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetGenomic_ins(1);
    exon->SetParts().push_back(chunk);

    chunk.Reset(new CSpliced_exon_chunk);
    chunk->SetDiag(204);
    exon->SetParts().push_back(chunk);

    exons.push_back(exon);
    }
//  CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                              cerr);
//  *ostr << align;

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CConstRef<CSeq_align> modified_align;
    TSeqRange range(5583, 248);
    modified_align = feat_gen.AdjustAlignment(align, range);
//  *ostr << *modified_align;

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(2));

    const CSpliced_exon* exon = modified_align->GetSegs().GetSpliced().GetExons().front().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? range.GetFrom() : 0 );
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? range.GetFrom() : range.GetTo() );

    exon = modified_align->GetSegs().GetSpliced().GetExons().back().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? 0 : range.GetFrom());
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? range.GetTo() : range.GetFrom());
    



// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << *trimmed_align;
//   delete ostr;
    }
}
BOOST_AUTO_TEST_CASE(TestCaseShrinkAlignmentCrossOrigin)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    for (int strand = -1 ; strand <= 1; strand +=2) {

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

    CSeq_align align;
    {
    align.SetType(CSeq_align::eType_partial);
    CSpliced_seg& seg = align.SetSegs().SetSpliced();
    seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    seg.SetProduct_length(101);
    CRef<CSeq_id> seq_id;
    seq_id.Reset(new CSeq_id("lcl|prot"));
    seg.SetProduct_id(*seq_id);
    seq_id.Reset(new CSeq_id("gi|17158061"));
    seg.SetGenomic_id(*seq_id);
    CSpliced_seg::TExons& exons = seg.SetExons();
    CRef<CSpliced_exon> exon;

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(strand > 0 ? 0 : 2);
    exon->SetProduct_start().SetProtpos().SetFrame(strand > 0 ? 1 : 3);
    exon->SetProduct_end().SetProtpos().SetAmin(strand > 0 ? 1 : 3);
    exon->SetProduct_end().SetProtpos().SetFrame(strand > 0 ? 1 : 3);
    exon->SetGenomic_start(5580);
    exon->SetGenomic_end(5583);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;

    exons.push_back(exon);

    exon.Reset(new CSpliced_exon);
    exon->SetProduct_start().SetProtpos().SetAmin(strand > 0 ? 1 : 0);
    exon->SetProduct_start().SetProtpos().SetFrame(strand > 0 ? 2 : 1);
    exon->SetProduct_end().SetProtpos().SetAmin(strand > 0 ? 3 : 2);
    exon->SetProduct_end().SetProtpos().SetFrame(strand > 0 ? 3 : 2);
    exon->SetGenomic_start(0);
    exon->SetGenomic_end(7);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;

    if (strand > 0)
        exons.push_back(exon);
    else 
        exons.insert(exons.begin(), exon);
    }
    BOOST_CHECK_NO_THROW(align.Validate(true));

//  CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                              cerr);
//  *ostr << align;

    CConstRef<CSeq_align> modified_align;
    TSeqRange range(5583, 4);
    modified_align = feat_gen.AdjustAlignment(align, range);
//  *ostr << *modified_align;

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(2));

    const CSpliced_exon* exon = modified_align->GetSegs().GetSpliced().GetExons().front().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? range.GetFrom() : 0 );
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? range.GetFrom() : range.GetTo() );

    exon = modified_align->GetSegs().GetSpliced().GetExons().back().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? 0 : range.GetFrom());
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? range.GetTo() : range.GetFrom());


// CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                             cerr);
// *ostr << *trimmed_align;
//   delete ostr;
    }
}

BOOST_AUTO_TEST_CASE(TestCaseExpandAlignmentNextToOrigin)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id gi 386076534, \
    genomic-id gi 386018361, \
    product-type protein, \
    exons { \
      { \
        product-start protpos { \
          amin 0, \
          frame 1 \
        }, \
        product-end protpos { \
          amin 0, \
          frame 3 \
        }, \
        genomic-start 321741, \
        genomic-end 321743, \
        partial TRUE \
      } \
    }, \
    product-length 2 \
  } \
}";

    TSeqPos genomic_size = 321744;

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);
    CSeq_align align;
    *istr >> align;

    for (int strand = -1 ; strand <= 1; strand +=2) {
    for (int side = -1 ; side <= 1; side +=2) {

        {
    CSpliced_exon* exon = align.SetSegs().SetSpliced().SetExons().front().GetPointer();
    exon->SetProduct_start().SetProtpos().SetAmin(side != strand ? 0 : 1);
    exon->SetProduct_end().SetProtpos().SetAmin(side != strand ? 0 : 1);
    exon->SetGenomic_start(side < 0 ? genomic_size-3 : 0);
    exon->SetGenomic_end(side < 0 ? genomic_size-1 : 2);
    exon->SetGenomic_strand() = strand > 0 ? eNa_strand_plus : eNa_strand_minus;
        }

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CConstRef<CSeq_align> modified_align;
    TSeqRange range(genomic_size-3, 2);
    modified_align = feat_gen.AdjustAlignment(align, range);

//  CObjectOStream* ostr = CObjectOStream::Open(eSerial_AsnText,
//                                              cerr);
//   *ostr << *modified_align;

    BOOST_CHECK_NO_THROW(modified_align->Validate(true));
    BOOST_CHECK_EQUAL(modified_align->GetSegs().GetSpliced().GetExons().size(), size_t(2));

    const CSpliced_exon* exon = modified_align->GetSegs().GetSpliced().GetExons().front().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? range.GetFrom() : TSeqPos(0));
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? genomic_size - 1 : range.GetTo());

    exon = modified_align->GetSegs().GetSpliced().GetExons().back().GetPointer();

    BOOST_CHECK_EQUAL(exon->GetGenomic_start(), strand > 0 ? TSeqPos(0) : range.GetFrom());
    BOOST_CHECK_EQUAL(exon->GetGenomic_end(), strand > 0 ? range.GetTo() : genomic_size - 1);

    }}
}

BOOST_AUTO_TEST_CASE(TestCasePartialCDS)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id local id 386076534, \
    genomic-id gi 183579259, \
    genomic-strand minus, \
    product-type transcript, \
    exons { \
      { \
        product-start nucpos 0, \
        product-end nucpos 150, \
        genomic-start 132443, \
        genomic-end 132593 \
      }, \
      { \
        product-start nucpos 151, \
        product-end nucpos 381, \
        genomic-start 132090, \
        genomic-end 132320, \
        partial TRUE \
      } \
    }, \
    product-length 382 \
  } \
} \
Seq-feat ::= { \
              data cdregion { \
                code { \
                  id 1 \
                } \
              }, \
              product whole local str \"PROT_10_36\", \
              location int { \
                from 59, \
                to 381, \
                id local id 386076534, \
                fuzz-to lim gt \
              } \
            } \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id local id 386076534, \
    genomic-id gi 183579259, \
    genomic-strand minus, \
    product-type transcript, \
    exons { \
      { \
        product-start nucpos 0, \
        product-end nucpos 132, \
        genomic-start 127519, \
        genomic-end 127651 \
      }, \
      { \
        product-start nucpos 133, \
        product-end nucpos 355, \
        genomic-start 127174, \
        genomic-end 127396 \
      }, \
      { \
        product-start nucpos 356, \
        product-end nucpos 359, \
        genomic-start 110589, \
        genomic-end 110592, \
        partial TRUE \
      } \
    }, \
    product-length 382 \
  } \
} \
Seq-feat ::= { \
              data cdregion { \
                code { \
                  id 1 \
                } \
              }, \
              product whole local str \"PROT_10_36\", \
              location int { \
                from 41, \
                to 359, \
                id local id 386076534, \
                fuzz-to lim gt \
              } \
            } \
";

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

    for (;;) {
        CSeq_align align;
        CSeq_feat feat;
        try {
            *istr >> align;
            *istr >> feat;
        }
        catch (CEofException&) {
            break;
        }

        BOOST_CHECK_NO_THROW(align.Validate(true));
        
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq_set& seqs = seq_entry->SetSet();
        seqs.SetSeq_set();
        CSeq_annot annot;
        annot.SetData().SetFtable();
        
        CRef<CScope> scope(new CScope(*om));
        scope->AddDefaults();
    
        CFeatureGenerator feat_gen(*scope);

        int flags = (CFeatureGenerator::fDefaults & ~CFeatureGenerator::fGenerateLocalIds) |
            CFeatureGenerator::fForceTranslateCds | CFeatureGenerator::fForceTranscribeMrna |
            CFeatureGenerator::fDeNovoProducts;
        feat_gen.SetFlags(flags);
        BOOST_CHECK_NO_THROW(feat_gen.ConvertAlignToAnnot(align, annot, seqs, 0, &feat));
    }
}

BOOST_AUTO_TEST_CASE(TestCaseGeneForPartialFeatureIsPartial)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    CFeatureGenerator feat_gen(*scope);

string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id gi 16762324, \
    genomic-id gi 188504888, \
    genomic-strand plus, \
    product-type protein, \
    exons { \
      { \
        product-start protpos { \
          amin 25, \
          frame 1 \
        }, \
        product-end protpos { \
          amin 576, \
          frame 3 \
        }, \
        genomic-start 0, \
        genomic-end 1655 \
      } \
    }, \
    product-length 577, \
    modifiers { \
      stop-codon-found TRUE \
    } \
  } \
} \
";

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

    CSeq_align align;
    *istr >> align;

    BOOST_CHECK_NO_THROW(align.Validate(true));
        
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    CBioseq_set& seqs = seq_entry->SetSet();
    seqs.SetSeq_set();
    CSeq_annot annot;
    annot.SetData().SetFtable();
        
    int flags =
        CFeatureGenerator::fCreateGene |
        CFeatureGenerator::fCreateCdregion |
        CFeatureGenerator::fForceTranslateCds;
    feat_gen.SetFlags(flags);

    TSeqRange range(0, 1655);
    CConstRef<CSeq_align> modified_align = feat_gen.AdjustAlignment(align, range);
    feat_gen.ConvertAlignToAnnot(*modified_align, annot, seqs);

    ITERATE(CSeq_annot::C_Data::TFtable, it, annot.GetData().GetFtable()){
        if ((*it)->GetData().IsGene()) {
            BOOST_CHECK( (*it)->GetLocation().IsPartialStart(eExtreme_Biological) );
        }
    }
}

BOOST_AUTO_TEST_CASE(TestCaseConvertLocToAnnotIncompleteCodon)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

string buf = " \
Seq-loc ::= packed-int { \
            { \
              from 632363, \
              to 633564, \
              strand plus, \
              id gi 255926317, \
              fuzz-to lim gt \
            } \
          } \
";
    CNcbiIstrstream istrs(buf.c_str());
    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);
    CSeq_loc loc;
    *istr >> loc;

    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    CBioseq_set& seqs = seq_entry->SetSet();
    seqs.SetSeq_set();
    CSeq_annot annot;
    annot.SetData().SetFtable();
    CFeatureGenerator feat_gen(*scope);
    int flags =
        CFeatureGenerator::fCreateGene |
        CFeatureGenerator::fCreateCdregion |
        CFeatureGenerator::fForceTranslateCds;
    feat_gen.SetFlags(flags);


    feat_gen.ConvertLocToAnnot(loc, annot, seqs);

    int protein_length = seqs.GetSeq_set().front()->GetSeq().GetLength();
    BOOST_CHECK_EQUAL(protein_length, 401);

}

BOOST_AUTO_TEST_CASE(TestCaseConvertLocToAnnotNoMismatches)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

string buf = " \
Seq-loc ::= packed-int { \
            { \
              from 0, \
              to 290, \
              strand minus, \
              id general { db \"PRJNA205468\" , \
                           tag str \"contig_484\" }, \
              fuzz-from lim lt, \
              fuzz-to lim gt \
            } \
          } \
";
string fasta_string = "\
>gnl|PRJNA205468|contig_484 [organism=Sphingobacterium sp. IITKGP-BTPF85] [moltype=Genomic] [strain=IITKGP-BTPF85] [gcode=11] [tech=wgs]\n\
GCTTCAACAAATAGGCATAGCCTTGATTCTGAAAAGCTTTTAAGGCGTAATCTTCAAACGCTGTAGTAAA\n\
AATAACAGGTGCCTGAACTTTGACCTGGTCAAATATTTCGAAACTCAATCCATCACCGAGCTGCACATCC\n\
ATAAAGATGAGATCGACTTCATTTTTAAGCAACCAATCGGTAGCTTCACGCACTGTTGTTATAATCGTAG\n\
ATTGAAATTTGGAAGCAATCAATTGGTCTAATTTCTCCAAAAGACTTTCCGAAGCCCAGTTTTCATCTTC\n\
TACGATCAATA\n\
";
    CNcbiIstrstream istrs(buf.c_str());
    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);
    CSeq_loc loc;
    *istr >> loc;

    CNcbiIstrstream fasta_stream(fasta_string.c_str());
    CFastaReader fasta_reader(fasta_stream, CFastaReader::fAddMods);
    scope->AddTopLevelSeqEntry(*fasta_reader.ReadOneSeq());

    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    CBioseq_set& seqs = seq_entry->SetSet();
    seqs.SetSeq_set();
    CSeq_annot annot;
    annot.SetData().SetFtable();
    CFeatureGenerator feat_gen(*scope);
    int flags =
        CFeatureGenerator::fCreateGene |
        CFeatureGenerator::fCreateCdregion |
        CFeatureGenerator::fForceTranslateCds;
    feat_gen.SetFlags(flags);


    feat_gen.ConvertLocToAnnot(loc, annot, seqs, CCdregion::eFrame_three);

    // there should not be any exceptions set
    bool no_exceptions_set = !annot.GetData().GetFtable().back()->IsSetExcept();
    BOOST_CHECK(no_exceptions_set);

}

BOOST_AUTO_TEST_SUITE_END();


