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
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const char* sc_TestEntry; //
extern const char* sc_TestEntry_code_break; //
extern const char* sc_TestEntry_alt_frame; //
extern const char* sc_TestEntry_internal_stop; //
extern const char* sc_TestEntry_5prime_partial;
extern const char* sc_TestEntry_3prime_partial;
extern const char* sc_TestEntry_5prime_partial_minus;
extern const char* sc_TestEntry_TerminalTranslExcept;
extern const char* sc_TestEntry_ShortCDS;
extern const char* sc_TestEntry_FirstCodon;
extern const char* sc_TestEntry_FirstCodon2;
extern const char* sc_TestEntry_GapInSeq1;
extern const char* sc_TestEntry_GapInSeq2;
extern const char* sc_TestEntry_GapInSeq3;
extern const char* sc_TestEntry_GapInSeq4;
extern const char* sc_TestEntry_GapInSeq5;
extern const char* sc_TestEntry_CodeBreakForStopCodon;

static string GetProteinString (CFeat_CI fi, CScope& scope)
{
    string real_prot_seq;
    CBioseq_Handle bsh =
       scope.GetBioseqHandle(*(fi->GetProduct().GetId()));
    CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
    vec.SetCoding(CSeq_data::e_Ncbieaa); // allow extensions
    vec.GetSeqData(0, bsh.GetBioseqLength(), real_prot_seq);
    return real_prot_seq;
}


#ifdef TEST_DEPRECATED
// removed, CCdregion_translate::TranslateCdregion is deprecated, so discontinue unit test
BOOST_AUTO_TEST_CASE(Test_TranslateCdregion)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString(feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CCdregion_translate
            tmp.clear();
            CCdregion_translate::TranslateCdregion
                (tmp, feat_iter->GetOriginalFeature(), scope, false);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CCdregion_translate, include the stop codon
            real_prot_seq += '*';
            tmp.clear();
            CCdregion_translate::TranslateCdregion
                (tmp, feat_iter->GetOriginalFeature(), scope, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}
#endif


BOOST_AUTO_TEST_CASE(Test_Translator_Raw)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CBioseq_Handle bsh = *bs_iter;
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);

        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            string nucleotide_sequence;
            vec.GetSeqData(feat_iter->GetTotalRange().GetFrom(),
                           feat_iter->GetTotalRange().GetTo() + 1,
                           nucleotide_sequence);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(nucleotide_sequence, tmp,
                                      CSeqTranslator::fNoStop |
                                      CSeqTranslator::fRemoveTrailingX);

            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate(), include the stop codon
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(nucleotide_sequence, tmp, CSeqTranslator::fRemoveTrailingX);

            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeqVector)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);


            CSeqVector vec(feat_iter->GetLocation(), scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(vec, tmp,
                                      CSeqTranslator::fNoStop |
                                      CSeqTranslator::fRemoveTrailingX);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(vec, tmp, CSeqTranslator::fRemoveTrailingX);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}

BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_loc_1)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), bs_iter->GetScope(), tmp,
                                      NULL, false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), bs_iter->GetScope(), tmp,
                                      NULL, true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_loc_2)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), scope, tmp,
                                      NULL, false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetLocation(), scope, tmp,
                                      NULL, true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_code_break)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_code_break);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_alt_frame)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_alt_frame);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_internal_stop)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_internal_stop);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);
            real_prot_seq[51] = '*';

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true /*include stops*/,
                                      true /*remove trailing X*/);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            tmp.clear();
            real_prot_seq.erase(real_prot_seq.find_first_of("*"));
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false /*include stops*/,
                                      true /*remove trailing X*/);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_5prime_partial)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_5prime_partial);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true /*include stops*/,
                                      true /*remove trailing X*/);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            /// use CSeqTranslator::Translate()
            real_prot_seq.erase(real_prot_seq.find_first_of("*"));
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false /*include stops*/,
                                      true /*remove trailing X*/);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
            for (size_t i = 0;  i < real_prot_seq.size()  &&  i < tmp.size();  ++i) {
                if (real_prot_seq[i] != tmp[i]) {
                    LOG_POST(Error << "char " << i << ": "
                             << real_prot_seq[i] << " != "
                             << tmp[i]);
                }
            }
        }
    }
}



BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_3prime_partial)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_3prime_partial);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);
            real_prot_seq[51] = '*';

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_5prime_partial_minus)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_5prime_partial_minus);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
            for (size_t i = 0;  i < real_prot_seq.size()  &&  i < tmp.size();  ++i) {
                if (real_prot_seq[i] != tmp[i]) {
                    LOG_POST(Error << "char " << i << ": "
                             << real_prot_seq[i] << " != "
                             << tmp[i]);
                }
            }

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_TerminalTranslExcept)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_TerminalTranslExcept);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
            for (size_t i = 0;  i < real_prot_seq.size()  &&  i < tmp.size();  ++i) {
                if (real_prot_seq[i] != tmp[i]) {
                    LOG_POST(Error << "char " << i << ": "
                             << real_prot_seq[i] << " != "
                             << tmp[i]);
                }
            }

            /// use CSeqTranslator::Translate()
            real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      true, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_ShortCDS)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_ShortCDS);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            /// use CSeqTranslator::Translate()
            tmp.clear();
            CSeqTranslator::Translate(feat_iter->GetOriginalFeature(),
                                      scope, tmp,
                                      false, true);
            BOOST_CHECK_EQUAL("-", tmp);
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_FirstCodon)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_FirstCodon);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetCdregion();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("FirstCodon");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(38);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry.SetSeq().SetAnnot().push_back(annot);

    string tmp;
    string complete_trans = "-MGMCFLRGWKGV";
    string partial_trans = "KMGMCFLRGWKGV";

    // translate with vector
    tmp.clear();
    CSeqVector vec(feat->GetLocation(), scope);
    // default value for 5' complete is true
    CSeqTranslator::Translate(vec, tmp, (CSeqTranslator::fNoStop|
                                         CSeqTranslator::fRemoveTrailingX));
    BOOST_CHECK_EQUAL(complete_trans, tmp);
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(vec, tmp, CSeqTranslator::fNoStop);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    // set 5' complete false
    tmp.clear();
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(vec, tmp,
                              NULL, false, true, 0, false);
    BOOST_CHECK_EQUAL(partial_trans, tmp);
#endif
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(vec, tmp, CSeqTranslator::fIs5PrimePartial | CSeqTranslator::fNoStop);
    BOOST_CHECK_EQUAL(partial_trans, tmp);

    // translate with string
    string seq_str;
    vec.GetSeqData(0, entry.GetSeq().GetLength(), seq_str);
    // default value for 5' complete is true
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(seq_str, tmp,
                              NULL, false, true);
    BOOST_CHECK_EQUAL(complete_trans, tmp);
#endif
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(seq_str, tmp, CSeqTranslator::fNoStop);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    // set 5' complete false
    tmp.clear();
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(seq_str, tmp,
                              NULL, false, true, 0, false);
    BOOST_CHECK_EQUAL(partial_trans, tmp);
#endif
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(seq_str, tmp, CSeqTranslator::fIs5PrimePartial | CSeqTranslator::fNoStop);
    BOOST_CHECK_EQUAL(partial_trans, tmp);


    ///
    /// translate the CDRegion directly
    ///

    /// use CSeqTranslator::Translate()
    tmp.clear();
    CSeqTranslator::Translate(*feat,
                              scope, tmp,
                              false, true);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    // if partial, should translate first codon
    feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    tmp.clear();
    CSeqTranslator::Translate(*feat,
                              scope, tmp,
                              false, true);
    BOOST_CHECK_EQUAL(partial_trans, tmp);



}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_FirstCodon2)
{
    // here, the first codon translates to M if complete, because it's an alternate start,
    // but L if partial
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_FirstCodon2);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetCdregion();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("FirstCodon2");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(26);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry.SetSeq().SetAnnot().push_back(annot);

    string tmp;
    string complete_trans = "MP*K*E*N*";
    string partial_trans = "LP*K*E*N*";

    // translate with vector
    tmp.clear();
    CSeqVector vec(feat->GetLocation(), scope);

    //
    // default value for 5' complete is true
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(vec, tmp,
                              NULL, true, true);
    BOOST_CHECK_EQUAL(complete_trans, tmp);
#endif

    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(vec, tmp, 0);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    //
    // set 5' complete false
    tmp.clear();
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(vec, tmp,
                              NULL, true, true, 0, false);
    BOOST_CHECK_EQUAL(partial_trans, tmp);
#endif

    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(vec, tmp, CSeqTranslator::fIs5PrimePartial);
    BOOST_CHECK_EQUAL(partial_trans, tmp);


    // translate with string
    string seq_str;
    vec.GetSeqData(0, entry.GetSeq().GetLength(), seq_str);
    // default value for 5' complete is true
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(seq_str, tmp,
                              NULL, true, true);
    BOOST_CHECK_EQUAL(complete_trans, tmp);
#endif
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(seq_str, tmp, 0);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    // set 5' complete false
    tmp.clear();
#ifdef TEST_DEPRECATED
    CSeqTranslator::Translate(seq_str, tmp,
                              NULL, true, true, 0, false);
    BOOST_CHECK_EQUAL(partial_trans, tmp);
#endif
    // try it with flag version
    tmp.clear();
    CSeqTranslator::Translate(seq_str, tmp, CSeqTranslator::fIs5PrimePartial);
    BOOST_CHECK_EQUAL(partial_trans, tmp);


    ///
    /// translate the CDRegion directly
    ///

    /// use CSeqTranslator::Translate()
    tmp.clear();
    CSeqTranslator::Translate(*feat,
                              scope, tmp,
                              true, true);
    BOOST_CHECK_EQUAL(complete_trans, tmp);

    // if partial, should translate first codon
    feat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    tmp.clear();
    CSeqTranslator::Translate(*feat,
                              scope, tmp,
                              true, true);
    BOOST_CHECK_EQUAL(partial_trans, tmp);

}


static void CheckTranslatedBioseq (CRef<CBioseq> bioseq, string seg1, bool mid_fuzz, string seg2)
{
    if (bioseq) {
        BOOST_CHECK_EQUAL(CSeq_inst::eRepr_delta, bioseq->GetInst().GetRepr());
        if (bioseq->GetInst().IsSetExt()
            && bioseq->GetInst().GetExt().IsDelta()) {
            CDelta_ext::Tdata::iterator seg_it = bioseq->SetInst().SetExt().SetDelta().Set().begin();
            CRef<CDelta_seq> seg = *seg_it;
            const CSeq_literal& lit1 = seg->GetLiteral();
            string p1 = lit1.GetSeq_data().GetIupacaa().Get();
            BOOST_CHECK_EQUAL(seg1, p1);

            ++seg_it;
            if (seg_it != bioseq->SetInst().SetExt().SetDelta().Set().end()) {
                seg = *seg_it;   
                
                BOOST_CHECK_EQUAL(true, seg->GetLiteral().GetSeq_data().IsGap());
                BOOST_CHECK_EQUAL(mid_fuzz, seg->GetLiteral().IsSetFuzz());
                ++seg_it;
            } else {
                BOOST_CHECK_EQUAL("Missing segment", "Missing segment in Bioseq");
            }

            if (seg_it != bioseq->SetInst().SetExt().SetDelta().Set().end()) {
                seg = *seg_it;       
                const CSeq_literal& lit2 = seg->GetLiteral();
                string p2 = lit2.GetSeq_data().GetIupacaa().Get();
                BOOST_CHECK_EQUAL(seg2, p2);
            } else {
                BOOST_CHECK_EQUAL("Missing segment", "Missing segment in Bioseq");
            }
        } else {
            BOOST_CHECK_EQUAL("Expected delta seq", "Result not delta seq");
        }
    } else {
        BOOST_CHECK_EQUAL("Expected Bioseq creation", "Bioseq creation failed");
    }
}


static void CheckTranslatedBioseq (CRef<CBioseq> bioseq, string seqdata)
{
    if (bioseq) {
        BOOST_CHECK_EQUAL(CSeq_inst::eRepr_raw, bioseq->GetInst().GetRepr());
        if (bioseq->GetInst().IsSetSeq_data()
            && bioseq->GetInst().GetSeq_data().IsIupacaa()) {
            BOOST_CHECK_EQUAL(seqdata, bioseq->GetInst().GetSeq_data().GetIupacaa().Get());
        } else {
            BOOST_CHECK_EQUAL("Expected raw seq", "Result not raw seq");
        }
    } else {
        BOOST_CHECK_EQUAL("Expected Bioseq creation", "Bioseq creation failed");
    }
}


static void SetLocationSkipGap (CRef<CSeq_feat> feat, const CBioseq& bioseq)
{
    string local_id = bioseq.GetId().front()->GetLocal().GetStr();

    feat->ResetLocation();
    CDelta_ext::Tdata::const_iterator nuc_it = bioseq.GetInst().GetExt().GetDelta().Get().begin();
    size_t pos = 0;
    while (nuc_it != bioseq.GetInst().GetExt().GetDelta().Get().end()) {
        size_t lit_len = (*nuc_it)->GetLiteral().GetLength();
        if ((*nuc_it)->GetLiteral().IsSetSeq_data() && (*nuc_it)->GetLiteral().GetSeq_data().IsIupacna()) {
            CRef<CSeq_id> id(new CSeq_id());
            id->SetLocal().SetStr(local_id);
            feat->SetLocation().SetMix().AddInterval(*id, pos, pos + lit_len - 1);
        }
        pos += lit_len;
        ++nuc_it;
    }
}


static void TestOneGapSeq(const char *asn, string seg1, string seg2)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(asn);
         istr >> MSerial_AsnText >> entry;
    }}

    string local_id = entry.GetSeq().GetId().front()->GetLocal().GetStr();

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetCdregion();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr(local_id);
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(entry.GetSeq().GetLength() - 1);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry.SetSeq().SetAnnot().push_back(annot);

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CRef<CBioseq> bioseq = CSeqTranslator::TranslateToProtein(*feat, scope);
    CheckTranslatedBioseq (bioseq, seg1, false, seg2);

    // take sequence out of scope, so that change in fuzz will be noted
    scope.RemoveTopLevelSeqEntry(seh);
    CDelta_ext::Tdata::iterator nuc_it = entry.SetSeq().SetInst().SetExt().SetDelta().Set().begin();
    ++nuc_it;
    CRef<CDelta_seq> nuc_mid = *nuc_it;
    nuc_mid->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    seh = scope.AddTopLevelSeqEntry(entry);

    bioseq = CSeqTranslator::TranslateToProtein(*feat, scope);
    CheckTranslatedBioseq (bioseq, seg1, true, seg2);
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_GapInSeq)
{
    TestOneGapSeq (sc_TestEntry_GapInSeq1, "MPK", "PK");
    // try with gap not on codon boundary
    TestOneGapSeq (sc_TestEntry_GapInSeq2, "MPX", "XPK");
    // try with 2 leftover nt, no stop codon
    TestOneGapSeq (sc_TestEntry_GapInSeq3, "MPK", "PKI");

    // try with coding region that has gap in intron
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_GapInSeq4);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetCdregion();
    SetLocationSkipGap (feat, entry.SetSeq());
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry.SetSeq().SetAnnot().push_back(annot);
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CRef<CBioseq> bioseq = CSeqTranslator::TranslateToProtein(*feat, scope);
    CheckTranslatedBioseq (bioseq, "MPKPK"); 
}


BOOST_AUTO_TEST_CASE(Test_Translator_CSeq_feat_ZeroGap)
{
    // try with coding region that has zero-length gap

    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_GapInSeq5);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetCdregion();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("GapInSeq5");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(17);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry.SetSeq().SetAnnot().push_back(annot);
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CRef<CBioseq> bioseq = CSeqTranslator::TranslateToProtein(*feat, scope);
    CheckTranslatedBioseq (bioseq, "MPK", true, "PK");
}



BOOST_AUTO_TEST_CASE(Test_Translate_CodeBreakForStopCodon)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry_CodeBreakForStopCodon);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CBioseq> prot(new CBioseq);
    prot->SetId().push_back(CRef<CSeq_id>(new CSeq_id("gnl|GNOMON|912063.p")));

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_cdregion));
        for ( ;  feat_iter;  ++feat_iter) {
            ///
            /// retrieve the actual protein sequence
            ///
            string real_prot_seq = GetProteinString (feat_iter, scope);

            ///
            /// translate the CDRegion directly
            ///
            string tmp;

            tmp.clear();
            CSeqTranslator::Translate
                (feat_iter->GetOriginalFeature(), scope, tmp, false);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);


            /// use CCdregion_translate, include the stop codon
            //NOTE: the test case lacks a trailing stop!
            //real_prot_seq += '*';
            tmp.clear();
            CSeqTranslator::Translate
                (feat_iter->GetOriginalFeature(), scope, tmp, true);
            BOOST_CHECK_EQUAL(real_prot_seq, tmp);

            prot->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            prot->SetInst().SetMol(CSeq_inst::eMol_aa);
            prot->SetInst().SetLength(tmp.size());
            prot->SetInst().SetSeq_data().SetNcbieaa().Set(tmp);
        }
    }

    /**
    CRef<CSeq_entry> nuc_se(new CSeq_entry);
    nuc_se->Assign(entry);

    CRef<CSeq_entry> prot_se(new CSeq_entry);
    prot_se->SetSeq(*prot);

    CRef<CSeq_entry> e(new CSeq_entry);
    e->SetSet().SetSeq_set().push_back(nuc_se);
    e->SetSet().SetSeq_set().push_back(prot_se);
    cerr << MSerial_AsnText << *e;
    **/
}


BOOST_AUTO_TEST_CASE(Test_FindBestFrame)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CSeq_feat> cds = entry.SetSet().SetAnnot().front()->SetData().SetFtable().front();

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    
    BOOST_CHECK_EQUAL(CSeqTranslator::FindBestFrame(*cds, scope), CCdregion::eFrame_one);
    cds->SetLocation().SetInt().SetFrom(15);
    BOOST_CHECK_EQUAL(CSeqTranslator::FindBestFrame(*cds, scope), CCdregion::eFrame_three);
    cds->SetLocation().SetInt().SetFrom(16);
    BOOST_CHECK_EQUAL(CSeqTranslator::FindBestFrame(*cds, scope), CCdregion::eFrame_two);
}



const char * sc_MinusOrigin = "\
Seq-entry ::= seq {\
  id { \
    local str \"test\" } , \
    inst { \
      repr raw , \
      mol dna , \
      length 20 , \
      topology circular , \
      seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } , \
    annot {\
      {\
        data ftable {\
          {\
            data cdregion {\
            },\
            location mix { \
              int {\
                from 0,\
                to 8,\
                strand minus,\
                id local str \"test\" } , \
              int { \
                from 17 , \
                to 19 , \
                strand minus,\
                id local str \"test\" } }  } , \
          {\
            data gene {\
            },\
            location mix { \
              int {\
                from 0,\
                to 8,\
                strand minus,\
                id local str \"test\" } , \
              int { \
                from 17 , \
                to 19 , \
                strand minus,\
                id local str \"test\" } \
            } \
          }\
        }\
      }\
    }\
  }\
}";


BOOST_AUTO_TEST_CASE(Test_FindOverlappingFeatureForMinusStrandCrossingOrigin)
{

    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_MinusOrigin);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CBioseq_Handle bsh = *bs_iter;
        CFeat_CI feat_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_gene));
        for ( ;  feat_iter;  ++feat_iter) {
            size_t num_cds = 0;
            sequence::TFeatScores cds;
            GetOverlappingFeatures (feat_iter->GetLocation(), CSeqFeatData::e_Cdregion,
                CSeqFeatData::eSubtype_cdregion, sequence::eOverlap_Contained, cds, scope);
            ITERATE (sequence::TFeatScores, s, cds) {
                num_cds++;
            }
            BOOST_CHECK_EQUAL(num_cds, 1u);
            num_cds = 0;
            cds.clear();
            GetOverlappingFeatures (feat_iter->GetLocation(), CSeqFeatData::e_Cdregion,
                CSeqFeatData::eSubtype_cdregion, sequence::eOverlap_Simple, cds, scope);
            ITERATE (sequence::TFeatScores, s, cds) {
                num_cds++;
            }
            BOOST_CHECK_EQUAL(num_cds, 1u);
        }
    }
}


const char * sc_TooManyOverlap = "\
Seq-entry ::= seq {\
  id { \
    local str \"test\" } , \
    inst { \
      repr raw , \
      mol dna , \
      length 20 , \
      topology circular , \
      seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } , \
    annot {\
      {\
        data ftable {\
          {\
            data rna {\
              type mRNA },\
            partial TRUE , \
            location mix { \
              int {\
                from 0,\
                to 19,\
                id local str \"test\" } , \
              null NULL , \
              int { \
                from 0 , \
                to 19 , \
                id gi 1213148 } }  } , \
          {\
            data gene {\
            },\
            location mix { \
              int {\
                from 0,\
                to 19,\
                id local str \"test\" } , \
              null NULL , \
              int { \
                from 0 , \
                to 19 , \
                id gi 1213148 } \
            } \
          }\
        }\
      }\
    }\
  }\
}";


BOOST_AUTO_TEST_CASE(Test_FindOverlappingFeaturesOnMultipleSeqs)
{

    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TooManyOverlap);
         istr >> MSerial_AsnText >> entry;
     }}

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    FOR_EACH_ANNOT_ON_BIOSEQ (annot, entry.GetSeq()) {
        if ((*annot)->IsFtable()) {
            FOR_EACH_FEATURE_ON_ANNOT (feat, **annot) {
                if ((*feat)->GetData().IsRna()) {
                    sequence::TFeatScores gene;
                    GetOverlappingFeatures ((*feat)->GetLocation(), CSeqFeatData::e_Gene,
                        CSeqFeatData::eSubtype_gene, sequence::eOverlap_Contained, gene, scope);
                    BOOST_CHECK_EQUAL(gene.size(), 1u);
                } else if ((*feat)->GetData().IsGene()) {
                    BOOST_CHECK_EQUAL((*feat)->IsSetPartial(), false);
                }
            }
        }
    }


    for (CBioseq_CI bs_iter(seh);  bs_iter;  ++bs_iter) {
        CBioseq_Handle bsh = *bs_iter;

        CFeat_CI mrna_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_mRNA));
        for ( ;  mrna_iter;  ++mrna_iter) {
            sequence::TFeatScores gene;
            GetOverlappingFeatures (mrna_iter->GetLocation(), CSeqFeatData::e_Gene,
                CSeqFeatData::eSubtype_gene, sequence::eOverlap_Contained, gene, scope);
            BOOST_CHECK_EQUAL(gene.size(), 1u);
        }

        CFeat_CI gene_iter(*bs_iter,
                           SAnnotSelector().IncludeFeatSubtype
                           (CSeqFeatData::eSubtype_gene));
        for ( ;  gene_iter;  ++gene_iter) {            
            BOOST_CHECK_EQUAL(gene_iter->IsSetPartial(), false);
        }

    }
}


//////////////////////////////////////////////////////////////////////////////

const char* sc_TestEntry ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_code_break ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MQFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            },\
            code-break {\
              {\
                loc int {\
                  from 17,\
                  to 19,\
                  strand plus,\
                  id gi 3002526\
                },\
                aa ncbieaa 81\
              }\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";


const char* sc_TestEntry_alt_frame ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame two,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 13,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_internal_stop ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILX\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_5prime_partial ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 374,\
        topology not-set,\
        seq-data ncbieaa \"-FSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLIL*\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 17,\
            to 1141,\
            strand plus,\
            id gi 3002526,\
            fuzz-from lim tr\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_3prime_partial ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTGATTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 374,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILX\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDL\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1135,\
            strand plus,\
            id gi 3002526,\
            fuzz-from lim tl\
          }\
        }\
      }\
    }\
  }\
}";

const char* sc_TestEntry_5prime_partial_minus ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        local str \"minus_5_prime_partial\" },\
      inst {\
        repr raw,\
        mol dna,\
        length 20,\
        seq-data iupacna \"AAATTTGGGCAAATTTGGGC\"\
      }\
    },\
    seq {\
      id {\
        local str \"minus_5_prime_partial_prot\" },\
      inst {\
        repr raw,\
        mol aa,\
        length 5,\
        seq-data ncbieaa \"-FAQI\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"test protein\"\
                }\
              },\
              location int {\
                from 0,\
                to 5,\
                strand plus,\
                id local str \"minus_5_prime_partial_prot\"\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame one,\
            code {\
              id 1\
            }\
          },\
          product whole local str \"minus_5_prime_partial_prot\",\
          location int {\
            from 0,\
            to 15,\
            strand minus,\
            id local str \"minus_5_prime_partial\",\
            fuzz-from lim tr\
          }\
        }\
      }\
    }\
  }\
}";

const char *sc_TestEntry_TerminalTranslExcept = "\
Seq-entry ::= set {\
      class nuc-prot ,\
      descr {\
        source {\
          genome mitochondrion ,\
          org {\
            taxname \"Takifugu fasciatus\" ,\
            common \"obscure pufferfish\" ,\
            db {\
              {\
                db \"taxon\" ,\
                tag\
                  id 301270 } } ,\
            orgname {\
              name\
                binomial {\
                  genus \"Takifugu\" ,\
                  species \"fasciatus\" } ,\
              lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Actinopterygii; Neopterygii; Teleostei; Euteleostei;\
 Neoteleostei; Acanthomorpha; Acanthopterygii; Percomorpha; Tetraodontiformes;\
 Tetradontoidea; Tetraodontidae; Takifugu\" ,\
              gcode 1 ,\
              mgcode 2 ,\
              div \"VRT\" } } } } ,\
      seq-set {\
        seq {\
          id {\
            local\
              str \"bankit1246641\" ,\
            general {\
              db \"BankIt\" ,\
              tag\
                id 1246641 } ,\
            general {\
              db \"TMSMART\" ,\
              tag\
                id 10764938 } ,\
            genbank {\
              accession \"GQ409967\" } } ,\
          descr {\
            title \"Takifugu fasciatus mitochondrion, complete genome.\" ,\
            molinfo {\
              biomol genomic ,\
              completeness complete } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 16444 ,\
            topology circular ,\
            strand ds ,\
            seq-data\
              ncbi2na '9C9B27C140922C78239C23A15E00B564A44027EB5E1FC7041DE3407\
C44E42CD64D52E00E5559556D680CA8BEB34A4440FEC953845C9FE519550A83D24B8C04F094C2E\
007E1F2D38DC08B6B0076E525166BCC622150BEF25069B02AEBC81CC04101E216045F429EF319F\
58241820430602C97470760546027284407A8F2315473971570118CE01C6C4CD65EBC71893C9F0\
0540A1FA6B9FC014DC8A25EFF0058C755BD05D1575FBFC165CCC516DB49715EE0A1032C8403E91\
25001B4AD8AEC983A2AA100EA713DDE5C8811808EE780E44560A28FC92C2420322ED39E0169CE0\
999111656D1DD54077C3F003070C25140200AA290B6C13AC2EC5A0AE47E8005A24C9F049F0245D\
5F1162F8656E40D89E55817049C95515454410147303155C231F07010040D3FC515CB3288C8028\
1CA2732302C590A819E0220380C152C0B00009223C45DB17FE4D38FC9CB30F2902247F2DC11560\
1E0E271D42125FCCA9135B77BA4022E8089FE2C8AE3017162F4BCC9EBE562078B309D25FFA7DF0\
753073F33C1587F0805008BC340AAB1255FE3108041FF04A28C28F3000F0A4799F0B29F2092514\
42026F0277244D4E510314300475C157D5716A7FDCE7D4C8203CE700E2C30AA5855750910BB134\
8181551603D068541408A80C0CF01D10420013F047FD6F15711EBB940CA021C020020A076901D0\
25D97BF14004D977E7D2E0C22B465E57B8733BF06966B3FE16E40AC990D1FB5FC0EE85ECE0E930\
62A7C9EDD7F742D0E07E3755B9209AA30053086208573A27F210004957B43015C0C0A030172E05\
EFF0EDFEBEA9859AAC1000554EE8380115FFC01422D147728C48137850C38D69C0963C18162F15\
CA8C126435DFF22D4CD8422AFC61763BE8D284D70EB9259CF02BDBFBD063C0B5C6E378BD2168B0\
D4AD2FDCDCE0B17FF52C602858008AA50ED4042551DD1F9E0549D0903088B10070B4081384EF2E\
E92256B3E4025F015F6048AF41D7757073871AC70F151F0F4557053CFB57BDF725B27FFC13C3E0\
600B7CAF1390F1802A54136CA17CE875D4153658E9BC07FD340895B595371759D535CFCF3E55C4\
F257C776530C338114397757C5535F87C0DE94F73FB5C9CDDC9FA4B71DCF729D68E25D41D00CE5\
7CCA3771892F91030DD3182C2DDA1C35FF34F8F3FF12907F15F404FC1B454A024FE1C34D506E15\
76490CE3137519C96010162753F85C120AA2D607ACDEAF41BE0CE4A2957F95CF7F72483394135D\
F0C04477510D73D72895C4CE5277D481ED77343F8CD001253FCD5D37D738958975C5463D633852\
70E45D138001F5C51F13C93D3CDE137E47541C4324A5F55740CC0281EE5E0102851FE38AE0DC8A\
BC0F575275F2020A87E054156223401DD2E7D471147F72C0B4AC0C27FCA54C57003A2BC03555FC\
40E057CCF1E5D5FF3FA5E7BCAC41CF107149111E1C3E4E0EA5C80F0457273CF57C325410451518\
90F8251C4031F7C1D09E592490575D764944141978304A50E2071242715152D50414C3C53E4F25\
F0036872559D445E1D520B0C4287217C41E87CD7A513840036553DD5F71C40F4150C140477C35D\
76535DD4C7EDA68E2AA5C0D0150B1B00D725C75D4D951729E0CB4F3DF43DD1415E9EC305724771\
34D304D7517D7E7F4E3010051043E817C10DD3894015634FD3570C55C35F774FA8AFE554707AFD\
3970387CD7D081F100421729304944F2495F2E571C2DFCFDCDC65CD719CC15C14DD540C1784A04\
715E1815010100013C50594D73ED77D4F75D757F05560B1F11DFC1130887CAF045214225F409DD\
2E82E00DFD2D5E3087E4253CDD137DE4E401211FC3C27025F072321297635E4077CBC127099500\
50627D4DC1FD565C900900A6A80956921B4379F7F23F90DC13B04552A78C808A1F0177B33AA710\
1459F057494F717BA4344678FFDD050D102136915CC5CBFFAE5E25A0CB2912470B7DF3DA96074B\
415A6477EA638523710EC36F1254E4F6C38FF7F32C31434E3E8A7FA81E3CBD57CC36895484E97D\
55830104C27DE1C7D5535F5D7DE764D77A2C825A26AC6A781AFC5455C9280DF9519289F7B21745\
37DDDF4DF92AB75DCFF2A90D07D3441CD3C138055490DD10C501177FDBB896FF0F1E7B1F75E775\
7D4B5F924A8D10C7DD1E1601703105F7F854928A2885535EC5247CF78F7FA515E0B713DF3DD5E9\
F6A30FD1336C971C76A4002053DAF13A93ADE2538E94FADF7E9FCFB3895144EFC4B693A1B21158\
971FC5D6D10C3CF94D584A2D02CFC9E1F905F93A28D0D0388055CC73895DA7D37D73F12EA69705\
A0FB5E941D3572136FF11845C71B2F953F51CED7753AB9ECFE43CEAE4F6C478F5473FD28C44751\
247E0700D47FA2C3BD3EBB41705F7D55047D7EB727A8C5D8631D61C55865C65738075B75D0FA74\
32DDDCBA4B0F3BD77F35DE2097D16509882D434BE07043860EC838711AB95D75F14C4F6089597D\
B501D017735880A8A0D8155B01EBF4252710051DED1FDF4C21DCB00E90F11C7FB42B20FBABC855\
46EDF3CF0E9135F441CABF42B927D14B0C82075F47D4614E770C36CF5F3C913CB1FC4F3EF9CCB7\
5140707042C4F721DD088F80F37814D7149CF3DF35D3E5757D5D60F7F170C8603438554DC10F02\
532B450E31E09CE0CC4873261725FE1D3130D5444217A556943D65FF201E14D83ADBD4BA1D553D\
835CB749208ED751D7896D57772BB003A396C5696DC050125FCD7B5617AEFF73A50E7780DE689C\
1449F4C50DBEF825B5572047F801E35D1C39D823974708270068449BC97FC27003AE5C50115F2E\
039741D055917E3D74F3ADF77E3BB3D70DF5D5D400D32747CF540E2575D50047E5502034057857\
8D384709F7D850FFC255165FE935578F95D9DD73C5FA15DF551141416F81410DB7FC11D40943F3\
C363F1D041E75D51C04CA2A440E257CCF65D3C32EF5C3C53C131CA1D7D4C44F455C51127F5B033\
25727B157BA76412C34DA0C60105412495CA517DF520AC55419DC35535C3CF3601ED25DF4F6177\
A6728BD87859C17449294DCF0F41C3671249FDB9D7557CE507B653DC13415EF3D75E11F720BE5B\
6430D425CEFF6C7DF709773710801B70E953424454C5132C854254E17704A24B6795F7DF135A5C\
94FE3D47F0741CF70C15CA1CB5C7FC74731F438E188CFB1882913D4294511575ED400A5F631A0C\
3DCFCF17582CF7DF5FA7DF7893DC51D097255475E0F28A79E15544A4F35570357D82F51D701124\
B5E7A4D28BC5B05E2745124F3A0AA2600424352D5D15C10D7DCAFDC7F13D79090C8B3CE0955F44\
3648EBBF1A7417DF6D9C5A7D4697D1B4D368D05F7C96CEDC71840D63D47D17581147FA7D824965\
E31E11FEC8EFB78F3DF31374371E38A74C37F72CC0B4B17B87D43454B7EBC0750A0230E07C7041\
0CF0F345127F3574FF0E16DD3DE1D5657C4521C5001CD14CE0EE9F6155CA34958757F74719F7DF\
2D90D7BD75FF61F2036575D755757E28850754D5515E11F33A17595F70F75D10FA5C971838750A\
297E0E25832B0F2FC3C013F8FDA74001CEBC0B530F05E38574D41DD3D174B7DF5CA1CFE97E4FF1\
62D45F73791DF3B7E024C33C95CF5D91FD04E09790335D4527FD25914F1D7DC93DD24EE09EBB2A\
7A7F0CB651256C5468D2345C40170175C439C00D70D5051CC7CD724138F04557038F38577D1D74\
32DD70F9DD1C971CE9C003177804A381FD741571F2516151CD0555FF0D774E7875F55C30D72525\
00511374E250F05B4183333447DF975790F7D78D7E5F7594480E34EFF1B0CFE094770F545F0D74\
F159EA80509206DF064AC4C7D7FDCC5724A7571575D36471C77C401D43A3573575F31C551F2941\
F81C134CE48C2378EA4A3B35E93D72F00C55FC68BD177871502746C82754F928DCCB1C967B1C70\
0728A71A4C3183FCB4572157C14081D27153D3ECF25778A6E3CC1EBD4DE4C6D0121700D3C3E5C7\
4D2D2513287ACBE8ACF74D407578A3D16991C35D30F951A3E136D67F3DE5C9431C9C6264444960\
531C7E762B39030C75770C925E38F4D9097650FC91C51575507C32881C30F3C5D5CF43E3557810\
FA5F1EA72911F3C7969C7477333D74C15062A56554145D7277E057444562245F7CF95D45D75470\
D71F3C401481C378AB81E7EB2132FC34013C8FB8F700122BC015DFB516022BD9043821E70DF4D5\
5DAF01D6A9D3D8549FF028C3270D6FADF28140077EB90750B009CE4751D770F304DC570F3CFF64\
71D3F152FF0417FD54014401501E2474D42E00492C03197D76D25F7957397517C18281E2DCD347\
07C07830115D17F84D04D25F03D875C743CDFC552C91CC6C178D4FF20FE4D78C4C474854F4C058\
F7F0B17DE37D70D90C3CF7ED16504133D41DF4DA7882ABEB3CCD3D7DD369E38C646268670C4965\
71092D373058B6884FA3C34D9CC9F8325151701DF889C41037DFF51C004C84C15D53C3E578DF25\
904A40D2743DA5F4D6E1F5F790C82B5C45ADDE571D47724532DBE4A0DF7C30D6CDD5577C801054\
12574477973972A95C14573F16517997C15206334000FB27FF41F52D072970E32C14DA5C0D2550\
7E5F5E44DE45467FFD02533CF7CE77AB73CD449F038E042135800EA2831147E157BC5D74E5C10F\
A49F25705A0557D725A7DFF502394D3E0DFC145D50F0197895CE5D15D7241F7F449CDC49718B6C\
F71B3532945767F0D57F450D0E0041535B0F05534061C938A493CF9E973C3451017DD5040045EC\
3343173AFBC07C79DF3ED10DFA875C3E5720F27D5C1750107C05C45117B5D5147DDC331FA7DF54\
10FB116E5DD700F0DD3FEA1010F9C540F3E1705E3C800BE854090FD373411D574DDC53CB04D410\
A343421317ED77D74711F99CD0576F7DD17079D8225558541559141DC31C4241BC30401509550C\
B0C755451C83330B8055353B45D80079374373775201574D0177D3428533FBB108C500100000CD\
40020C0145ED05ECA65F28D7CDE48499E183030110504C55533034F04114084202F554E401E095\
449020B90524503E007530028928FEC84110F3441413570C020010993004CBF794A3FC14A173A6\
E0014DBEF1D0710011C3A525C64015155C70036C06132C3E17D5157D013F5978E01FE9D5C768F3\
97CF11034D1287BD7E4311C44D61377165FF4D6C9533F9621B01C69E1C3D90DC46406B974F7FFC\
DE7CCF54CDA58ADDC73A77C5C2C0805E01B2AB2D77C7FCB0E9459FDB28C6D7D4E2840CD7FE2894\
7B0F10179DDE5B55C6CA0119FBD0E2CE28A7DD2C84994770463DFE5F51F5D754FCDBE496794FB1\
37CFFDF460129D410557287C3D0112300D53D454C7DDF102175EA7D10D39DD2576431D95CF7550\
1C5DA215E107D1149435DDB455654CF014838CFD73F93190D79ADCD54309CA2BBDE95F7E5D0DF0\
F7CCB2F5FDF11177010609704F5951CD10F5CF7815C3E786C94D705E0FA29315B608D7C4F3CDA1\
03E5D0C7F1F7577D7ADF0E58C96BE1C80100C701C1093CB2748F489B6B7EC0583B6AAF00D557CE\
740082A1F4155147A75409493DF0F01C77F8C31333B3CD54F4CCCF013C310E4C1C2132CF333D1F\
30C5F41130242C4801C02E720930F200F57003CF04078180FD21604301D34B423314A1D04D7300\
C50B3C3B2C2058534BE3F7C3933CF3E0AE2A10C12EAAFD1C0E073D7A4FEBD71F4A94F0F8F475D3\
DFD3619E130BEFAE8B534B88C3544E589BDDD44AAD2BCFFFDDFD7F43E13F48B9266D06BD342BE0\
4FFDFAFCEB0EF2F0E0F88CD33083C4F3E334284C0C30C63D0434C43F4557DF7FF008FC1B31555C\
555400328805FCA7E05095D47C3C0CF4D33CF34CCF30CF30C330F33'H } } ,\
        seq {\
          id {\
            local\
              str \"PROT_4_bankit1246641\" ,\
            general {\
              db \"TMSMART\" ,\
              tag\
                id 10764942 } } ,\
          descr {\
            title \"cytochrome c oxidase subunit II [Takifugu fasciatus]\" ,\
            molinfo {\
              biomol peptide ,\
              tech concept-trans-a } } ,\
          inst {\
            repr raw ,\
            mol aa ,\
            length 230 ,\
            topology not-set ,\
            seq-data\
              ncbieaa \"MAHPSQLGFQGAASPVMEELLHFHDHALMIVFLISTLVLYIIVAMVSTKLTNKYI\
LDSQEIEIIWTILPAIILILIALPSLRILYLMDEINDPHLTIKAMGHQWYWSYEYTDYSDLAFDSYMIPTQDLAPGQF\
RLLETDHRMVVPVDSPIRILVSAEDVLHSWAVPSLGVKMDAVPGRLNQTAFILSRPGVFYGQCSEICGANHSFMPIVV\
EAVPLEHFENWSSLMLEDA\" } ,\
          annot {\
            {\
              data\
                ftable {\
                  {\
                    data\
                      prot {\
                        name {\
                          \"cytochrome c oxidase subunit II\" } } ,\
                    location\
                      int {\
                        from 0 ,\
                        to 229 ,\
                        id\
                          local\
                            str \"PROT_4_bankit1246641\" } } } } } } } ,\
          annot {\
            {\
              data\
                ftable {\
              {\
                data\
                  cdregion {\
                    frame one ,\
                    code {\
                      id 2 } ,\
                    code-break {\
                      {\
                        loc\
                          int {\
                            from 7837 ,\
                            to 7837 ,\
                            strand plus ,\
                            id\
                              genbank {\
                                accession \"GQ409967\" } } ,\
                        aa\
                          ncbieaa 42 } } } ,\
                comment \"TAA stop codon is completed by the addition of 3' A\
 residues to the mRNA\" ,\
                product\
                  whole\
                    local\
                      str \"PROT_4_bankit1246641\" ,\
                location\
                  int {\
                    from 7147 ,\
                    to 7837 ,\
                    strand plus ,\
                    id\
                      genbank {\
                        accession \"GQ409967\" } } } } } } }\
";

const char *sc_TestEntry_ShortCDS = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"ShortCDS\" } ,\
          descr {\
            molinfo {\
              biomol mRNA } } ,\
          inst {\
            repr raw ,\
            mol rna ,\
            length 20 ,\
            seq-data\
              iupacna \"ATGTTTAAACATGTTTAAAC\" } ,\
          annot {\
            {\
              data\
                ftable {\
                  {\
                    data\
                      cdregion {\
                         } ,\
                    location\
                      int {\
                        from 12 ,\
                        to 13 ,\
                        strand plus ,\
                        id\
                          local\
                            str \"ShortCDS\" } } ,\
                  {\
                    data\
                      cdregion {\
                         } ,\
                    location\
                      int {\
                        from 12 ,\
                        to 13 ,\
                        strand minus ,\
                        id\
                          local\
                            str \"ShortCDS\" } } } } } }\
";

const char *sc_TestEntry_FirstCodon = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"FirstCodon\" } ,\
          descr {\
            molinfo {\
              biomol mRNA } } ,\
          inst {\
            repr raw ,\
            mol rna ,\
            length 39 ,\
            seq-data\
              iupacna \"AAAATGGGAATGTGCTTTTTGAGAGGATGGAAAGGTGTT\" } }\
";

const char *sc_TestEntry_FirstCodon2 = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"FirstCodon2\" } ,\
          descr {\
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } }\
";


const char *sc_TestEntry_GapInSeq1 = "\
Seq-entry ::= seq {\
      id {\
        local\
          str \"GapInSeq1\" } ,\
      descr {\
        molinfo {\
          biomol genomic } } ,\
      inst {\
        repr delta ,\
        mol dna ,\
        length 27 ,\
        ext \
          delta { \
            literal { \
            length 9 , \
              seq-data \
                iupacna \"ATGCCCAAA\" } , \
            literal { \
              length 9 } , \
            literal { \
              length 9 , \
              seq-data \
                iupacna \"CCCAAATAA\" } } } } \
";


const char *sc_TestEntry_GapInSeq2 = "\
Seq-entry ::= seq {\
      id {\
        local\
          str \"GapInSeq2\" } ,\
      descr {\
        molinfo {\
          biomol genomic } } ,\
      inst {\
        repr delta ,\
        mol dna ,\
        length 27 ,\
        ext \
          delta { \
            literal { \
            length 8 , \
              seq-data \
                iupacna \"ATGCCCAA\" } , \
            literal { \
              length 9 } , \
            literal { \
              length 10 , \
              seq-data \
                iupacna \"ACCCAAATAA\" } } } } \
";

const char *sc_TestEntry_GapInSeq3 = "\
Seq-entry ::= seq {\
      id {\
        local\
          str \"GapInSeq3\" } ,\
      descr {\
        molinfo {\
          biomol genomic } } ,\
      inst {\
        repr delta ,\
        mol dna ,\
        length 29 ,\
        ext \
          delta { \
            literal { \
            length 9 , \
              seq-data \
                iupacna \"ATGCCCAAA\" } , \
            literal { \
              length 9 } , \
            literal { \
              length 11 , \
              seq-data \
                iupacna \"CCCAAAATAAA\" } } } } \
";


const char *sc_TestEntry_GapInSeq4 = "\
Seq-entry ::= seq {\
      id {\
        local\
          str \"GapInSeq4\" } ,\
      descr {\
        molinfo {\
          biomol genomic } } ,\
      inst {\
        repr delta ,\
        mol dna ,\
        length 27 ,\
        ext \
          delta { \
            literal { \
            length 9 , \
              seq-data \
                iupacna \"ATGCCCAAA\" } , \
            literal { \
              length 9 } , \
            literal { \
              length 9 , \
              seq-data \
                iupacna \"CCCAAATAA\" } } } } \
";


const char *sc_TestEntry_GapInSeq5 = "\
Seq-entry ::= seq {\
      id {\
        local\
          str \"GapInSeq5\" } ,\
      descr {\
        molinfo {\
          biomol genomic } } ,\
      inst {\
        repr delta ,\
        mol dna ,\
        length 18 ,\
        ext \
          delta { \
            literal { \
            length 9 , \
              seq-data \
                iupacna \"ATGCCCAAA\" } , \
            literal { \
              length 0 } , \
            literal { \
              length 9 , \
              seq-data \
                iupacna \"CCCAAATAA\" } } } } \
";

const char* sc_TestEntry_CodeBreakForStopCodon = "\
Seq-entry ::= set {\
  seq-set {\
    seq {\
      id {\
        general {\
          db \"GNOMON\",\
          tag str \"912063.m\"\
        }\
      },\
      descr {\
        molinfo {\
          biomol mRNA,\
          completeness no-ends\
        }\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1674,\
        seq-data ncbi4na '2481822428821148121184414284124281824121844848888241\
141141141484144141411144128442828241148441842444141141114121411142142144412884\
284114118182828124121144121282418824821288821144188824821182484118844842828828\
241182824121124142488211882218482884844184488821142214412112441144882828824248\
428821141418214121142184444821114844282118844224414484824284181848284288122182\
12441411214824288184121121188484218282422214418812488441822112112182112F842882\
882211824411848824481282482882144484411144121484288214282148121828881282112848\
242214821282481212828822248282124184181184882842488882888184114111182142411884\
142214148448188482211881482211844882824821184414282114441824181214418442421241\
121822281188121182212484112844821141121821142482881824224424142242141114228814\
282218448121141128822424411141821882111824122242482418884218428184484114884284\
842884184121121221884141821224148842211821112148411282114188121114141144888844\
144418841844141428244124141141112242284881824844128122144148182121214181211284\
821118824824841142844842214411112882484118844142421142144182882182114842822142\
822118284821828421841428221844842888824184284224418482882421248221288242214144\
211881282114148828124181422422114141148122842141411144148122822824114882882144\
282141421142482821281122144282488821822824221141821121114141884818824141121141\
141144814221884824184188841821111844488884184284182214841141148441144144142111\
142821142882484118284414111844124114184182814281284824224824114184124114424888\
228284284241144224884111221142818214428124148122888824411841281822824124888244\
114114141281121142821821184118244144141141841214142882414842841141141141228442\
11418288844218414418284412112882288822414282214211141144284142'H\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data cdregion {\
                frame one,\
                code {\
                  id 1\
                },\
                code-break {\
                  {\
                    loc int {\
                      from 879,\
                      to 881,\
                      strand plus,\
                      id general {\
                        db \"GNOMON\",\
                        tag str \"912063.m\"\
                      }\
                    },\
                    aa ncbieaa 88\
                  }\
                }\
              },\
              partial TRUE,\
              product whole general {\
                db \"GNOMON\",\
                tag str \"912063.p\"\
              },\
              location int {\
                from 0,\
                to 1673,\
                strand plus,\
                id general {\
                  db \"GNOMON\",\
                  tag str \"912063.m\"\
                },\
                fuzz-from lim lt,\
                fuzz-to lim gt\
              }\
            }\
          }\
        }\
      }\
    },\
    seq {\
      id {\
        general {\
          db \"GNOMON\",\
          tag str \"912063.p\"\
        }\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 558,\
        seq-data ncbieaa \"RIRFKYNGADAIDMVFSKKKSEERKDWLSKWMREKKDRKQQGLAEEYLYDKD\
TRFVTFKDFVNRELVLFSNLDNERSIPCLVDGFKPGQRKVLFACFKRSDKHGVKVAQLAGGVADMSAYHHGEQSLMTT\
IVHLAQDYVGSNNINXLLPIGMFGTRLQGGKDSASAQYIFTQLSPVTRTLFPSHDDNVLRFLYEENQRIEPEWYCPIS\
PMVLVNGAQGIDTGWRTNIPNYNPRELVKNIKRLIAGEPQKALAPWYKNFRGKIIQIDPRRFACYGEVAVLDDNTIEI\
TELPIKQXTQDYKEKVLEGLMESSDEKKPPVIVDYQEYHTDTTVKFVVKLVPGKLRELERKQDLHQVLQLQSVICMSS\
MVLFDAAGCLRTSTSPEAITQEFYDSRQEKYLQRKEYLLEVLQAQSKRLTNQARFILAKINKEIVFENKKKVAIVDDL\
IKMGFDADPVKKWKEEQKLKLRESGEMDEDDLATVAVEDDEGVSSAAKAVETKLSGYEYLFGMTILDVSEEETNKLIN\
ESEEKMTELRVLKKKTWQDLWHEDLDNFLSELQQRRLS\"\
      }\
    }\
  }\
}\
";

