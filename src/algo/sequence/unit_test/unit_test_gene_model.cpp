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
#include <objmgr/scope.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include <algo/sequence/gene_model.hpp>

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
    arg_desc->AddOptionalKey("data-out", "OutputData",
                     "Seq-annots produced from input alignments",
                     CArgDescriptions::eOutputFile);
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& align_istr = args["data-in"].AsInputFile();
    CNcbiIstream& annot_istr = args["data-expected"].AsInputFile();

    auto_ptr<CObjectIStream> align_is(CObjectIStream::Open(eSerial_AsnText,
                                                           align_istr));
    auto_ptr<CObjectIStream> annot_is(CObjectIStream::Open(eSerial_AsnText,
                                                           annot_istr));
    auto_ptr<CObjectOStream> annot_os;
    if (args["data-out"]) {
        CNcbiOstream& annot_ostr = args["data-out"].AsOutputFile();
        annot_os.reset(CObjectOStream::Open(eSerial_AsnText,
                                            annot_ostr));
    }

    int count = 0;

    while (align_istr  &&  annot_istr) {

        CSeq_align align;
        CSeq_annot expected_annot;

        /// we wrap the first serialization in try/catch
        /// if this fails, we are at the end of the file, and we expect both to
        /// be at the end of the file.
        /// a failure in the second serialization is fatal
        try {
            *align_is >> align;
        }
        catch (CEofException&) {
            try {
                *annot_is >> expected_annot;
            }
            catch (CEofException&) {
            }
            break;
        }

        cerr << "Alignment "<< ++count <<  endl;

        BOOST_CHECK_NO_THROW(align.Validate(true));

        CBioseq_set seqs;
        CSeq_annot actual_annot;
        BOOST_CHECK_NO_THROW(
                             CGeneModel::CreateGeneModelFromAlign(align, scope,
                                                                  actual_annot, seqs)
                             );

        //cerr << MSerial_AsnText << actual_annot;

        *annot_is >> expected_annot;
        if (annot_os.get() != NULL) {
            *annot_os << actual_annot;
        }

        CSeq_annot::TData::TFtable::const_iterator actual_iter =
            actual_annot.GetData().GetFtable().begin();
        CSeq_annot::TData::TFtable::const_iterator actual_end =
            actual_annot.GetData().GetFtable().end();

        CSeq_annot::TData::TFtable::const_iterator expected_iter =
            expected_annot.GetData().GetFtable().begin();
        CSeq_annot::TData::TFtable::const_iterator expected_end =
            expected_annot.GetData().GetFtable().end();

        for ( ;  actual_iter != actual_end  &&  expected_iter != expected_end;
              ++actual_iter, ++expected_iter) {

            bool display = false;
            const CSeq_feat& f1 = **actual_iter;
            const CSeq_feat& f2 = **expected_iter;
            BOOST_CHECK(f1.GetData().GetSubtype() == f2.GetData().GetSubtype());
            BOOST_CHECK(f1.GetLocation().Equals(f2.GetLocation()));
            if ( !f1.GetLocation().Equals(f2.GetLocation()) ) {
                display = true;
            }
            BOOST_CHECK_EQUAL(f1.IsSetProduct(), f2.IsSetProduct());
            if (f1.IsSetProduct()) {
                BOOST_CHECK(f1.GetProduct().Equals(f2.GetProduct()));
            }

            if (f1.GetData().IsCdregion()  &&
                f2.GetData().IsCdregion()) {
                BOOST_CHECK(f1.GetData().GetCdregion().IsSetCode_break() ==
                            f2.GetData().GetCdregion().IsSetCode_break());
                if (f1.GetData().GetCdregion().IsSetCode_break() !=
                    f2.GetData().GetCdregion().IsSetCode_break()) {
                    display = true;
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

            if (display) {
                cerr << "expected: " << MSerial_AsnText << f2;
                cerr << "got: " << MSerial_AsnText << f1;
            }
        }
    }

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
    
    CFeatureGenerator feat_gen(scope);
    
    CSeq_align align;
    CConstRef<CSeq_align> trimmed_align;
    BOOST_CHECK_NO_THROW(
                         trimmed_align = feat_gen.CleanAlignment(align)
                         );
}

BOOST_AUTO_TEST_SUITE_END();
